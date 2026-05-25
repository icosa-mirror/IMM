#include "imm_engine_bridge.h"

#include "libImmCore/src/libBasics/piStr.h"

using namespace ImmCore;
using namespace ImmPlayer;

namespace ImmShared
{
    struct ImmEngineBridge::MainRenderReporter : public piRenderer::piReporter
    {
        piLog *mLog;

        explicit MainRenderReporter(piLog *log) : mLog(log) {}

        void Info(const char *str) override
        {
            piString wstr;
            wstr.InitCopyS(str);
            mLog->Printf(LT_MESSAGE, L"%s", wstr.GetS());
            wstr.End();
        }

        void Error(const char *str, int) override
        {
            piString wstr;
            wstr.InitCopyS(str);
            mLog->Printf(LT_ERROR, L"%s", wstr.GetS());
            wstr.End();
        }

        void Begin(uint64_t memCurrent, uint64_t memPeak, int texCurrent, int texPeak) override
        {
            mLog->Printf(LT_MESSAGE, L"---- Renderer Report ---- ");
            mLog->Printf(LT_MESSAGE, L"Peak: %d MB in %d textures", memPeak >> 20, texPeak);
            mLog->Printf(LT_MESSAGE, L"Curr: %d MB in %d textures", memCurrent >> 20, texCurrent);
        }

        void End(void) override
        {
            mLog->Printf(LT_MESSAGE, L"---- Renderer Report ---- ");
        }

        void Texture(const wchar_t *key, uint64_t kb, piRenderer::Format format, bool, int xres, int yres, int zres) override
        {
            mLog->Printf(LT_MESSAGE, L"* Texture: %5d kb, %4d x %4d x %4d %2d (%s)",
                         static_cast<int>(kb),
                         xres,
                         yres,
                         zres,
                         format,
                         (key == nullptr) ? L"null" : key);
        }
    };

    ImmEngineBridge::ImmEngineBridge()
        : mRenderer(nullptr),
          mRenderReporter(nullptr),
          mSoundBackend(nullptr),
          mLogInitialized(false),
          mTimerInitialized(false),
          mSoundInitialized(false),
          mGraphicsInitialized(false),
          mPlayerInitialized(false),
          mBridgeInitialized(false)
    {
    }

    ImmEngineBridge::~ImmEngineBridge()
    {
        Shutdown();
    }

    bool ImmEngineBridge::Init(const InitConfig &config)
    {
        Shutdown();
        mConfig = config;

        if (!InitializeLog())
            return false;

        if (!InitializeTimer())
        {
            ShutdownRuntime();
            return false;
        }

        if (!InitializeSound())
        {
            ShutdownRuntime();
            return false;
        }

        if (!CreateRenderer())
        {
            ShutdownRuntime();
            return false;
        }

        if (mConfig.initializeRendererOnInit && !CompleteGraphicsInitialization())
        {
            ShutdownRuntime();
            return false;
        }

        mBridgeInitialized = true;
        return true;
    }

    bool ImmEngineBridge::CompleteGraphicsInitialization()
    {
        if (mGraphicsInitialized && mPlayerInitialized)
            return true;

        if (mRenderer == nullptr)
            return false;

        if (!InitializeRenderer())
            return false;

        if (!InitializePlayer())
            return false;

        mGraphicsInitialized = true;
        mBridgeInitialized = true;
        return true;
    }

    void ImmEngineBridge::Shutdown()
    {
        ShutdownRuntime();
        ResetConfig();
    }

    void ImmEngineBridge::GlobalWork(bool enabled, int budgetMicroseconds)
    {
        if (!mPlayerInitialized)
            return;

        mPlayer.GlobalWork(enabled, budgetMicroseconds);
    }

    void ImmEngineBridge::SetCameraMatrices(int cameraID,
                                            int stereoType,
                                            const mat4x4 *world2Head,
                                            const mat4x4 *headProjection,
                                            const mat4x4 *world2LeftEye,
                                            const mat4x4 *leftEyeProjection,
                                            const mat4x4 *world2RightEye,
                                            const mat4x4 *rightEyeProjection)
    {
        if (cameraID < 0 || cameraID >= kMaxCameras)
            return;

        CameraState &camera = mCamera[cameraID];
        camera.stereoType = stereoType;
        camera.currentEye = 0;

        if (world2Head != nullptr) camera.world2Head = *world2Head;
        if (headProjection != nullptr) camera.headProjection = *headProjection;
        if (world2LeftEye != nullptr) camera.world2LeftEye = *world2LeftEye;
        if (leftEyeProjection != nullptr) camera.leftEyeProjection = *leftEyeProjection;
        if (world2RightEye != nullptr) camera.world2RightEye = *world2RightEye;
        if (rightEyeProjection != nullptr) camera.rightEyeProjection = *rightEyeProjection;
    }

    bool ImmEngineBridge::RenderCamera(int cameraID, const ViewportInfo &viewport, int eyeID, bool tickSound)
    {
        if (!mPlayerInitialized || !mGraphicsInitialized || mRenderer == nullptr)
            return false;

        if (cameraID < 0 || cameraID >= kMaxCameras)
        {
            if (mLogInitialized)
                mLog.Printf(LT_ERROR, L"Invalid cameraID: %d", cameraID);
            return false;
        }

        const ivec2 res(static_cast<int>(viewport.width), static_cast<int>(viewport.height));
        if (res.x <= 0 || res.y <= 0)
            return false;

        if (viewport.forceViewport)
        {
            const int vp[4] = {
                static_cast<int>(viewport.x),
                static_cast<int>(viewport.y),
                res.x,
                res.y
            };
            mRenderer->SetViewport(0, vp);
        }

        const CameraState &camera = mCamera[cameraID];
        if (camera.stereoType == 0)
        {
            mPlayer.GlobalRender(fromMatrix(f2d(camera.world2Head)),
                                 fromMatrix(f2d(camera.world2Head)),
                                 camera.headProjection,
                                 StereoMode::None);
            mPlayer.RenderMono(res, 0);
        }
        else if (camera.stereoType == 1)
        {
            if (eyeID < 0 || eyeID > 1)
            {
                if (mLogInitialized)
                    mLog.Printf(LT_ERROR, L"Invalid eyeID: %d", eyeID);
                return false;
            }

            mPlayer.GlobalRender(fromMatrix(f2d(camera.world2Head)),
                                 fromMatrix(f2d(camera.world2Head)),
                                 camera.headProjection,
                                 StereoMode::Fallback);

            if (eyeID == 0)
            {
                const mat4x4d headToLeftEye = f2d(camera.world2LeftEye) * invert(f2d(camera.world2Head));
                mPlayer.RenderStereoMultiPass(res, 0, headToLeftEye, camera.leftEyeProjection);
            }
            else
            {
                const mat4x4d headToRightEye = f2d(camera.world2RightEye) * invert(f2d(camera.world2Head));
                mPlayer.RenderStereoMultiPass(res, 1, headToRightEye, camera.rightEyeProjection);
            }
        }
        else if (camera.stereoType == 2)
        {
            const float oldVp[6] = { viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth };
            const float newVp[6] = { viewport.x, viewport.y, viewport.width * 2.0f, viewport.height, viewport.minDepth, viewport.maxDepth };
            mRenderer->SetViewports(1, newVp);

            mPlayer.GlobalRender(fromMatrix(f2d(camera.world2Head)),
                                 fromMatrix(f2d(camera.world2Head)),
                                 camera.headProjection,
                                 StereoMode::Preferred);

            const mat4x4d headToLeftEye = f2d(camera.world2LeftEye) * invert(f2d(camera.world2Head));
            const mat4x4d headToRightEye = f2d(camera.world2RightEye) * invert(f2d(camera.world2Head));
            mPlayer.RenderStereoSinglePass(res,
                                           headToLeftEye,
                                           camera.leftEyeProjection,
                                           headToRightEye,
                                           camera.rightEyeProjection);
            mRenderer->SetViewports(1, oldVp);
        }

        if (tickSound && mSoundBackend != nullptr)
            mSoundBackend->Tick();

        return true;
    }

    Player *ImmEngineBridge::GetPlayer()
    {
        return &mPlayer;
    }

    const Player *ImmEngineBridge::GetPlayer() const
    {
        return &mPlayer;
    }

    piLog *ImmEngineBridge::GetLog()
    {
        return &mLog;
    }

    const piLog *ImmEngineBridge::GetLog() const
    {
        return &mLog;
    }

    piRenderer *ImmEngineBridge::GetRenderer()
    {
        return mRenderer;
    }

    const piRenderer *ImmEngineBridge::GetRenderer() const
    {
        return mRenderer;
    }

    const ImmEngineBridge::CameraState *ImmEngineBridge::GetCameraState(int cameraID) const
    {
        if (cameraID < 0 || cameraID >= kMaxCameras)
            return nullptr;
        return &mCamera[cameraID];
    }

    bool ImmEngineBridge::IsInitialized() const
    {
        return mBridgeInitialized;
    }

    bool ImmEngineBridge::IsGraphicsInitialized() const
    {
        return mGraphicsInitialized && mPlayerInitialized;
    }

    bool ImmEngineBridge::InitializeLog()
    {
        const wchar_t *logFileName = (mConfig.logFileName == nullptr) ? L"imm_player_log.txt" : pistr2ws(mConfig.logFileName);

#ifdef _DEBUG
        const int mode = PILOG_TXT + PILOG_CNS;
#else
        const int mode = PILOG_TXT;
#endif
        if (!mLog.Init(logFileName, mode))
            return false;

        mLogInitialized = true;
        mLog.Printf(LT_DEBUG, mConfig.colorSpace == 0 ? L"Linear" : L"Gamma");
        mLog.Printf(LT_DEBUG, L"Antialiasing: %d", mConfig.antialiasing);
        mLog.Printf(LT_DEBUG, L"Log File: %s", logFileName);
        return true;
    }

    bool ImmEngineBridge::InitializeTimer()
    {
        if (!mTimer.Init())
            return false;

        mTimerInitialized = true;
        return true;
    }

    bool ImmEngineBridge::InitializeSound()
    {
#if defined(__ANDROID__) || defined(ANDROID)
        const piSoundEngineBackend::API soundApi = piSoundEngineBackend::API::Android;
#elif defined(WINDOWS)
        const piSoundEngineBackend::API soundApi = piSoundEngineBackend::API::DirectSoundOVR;
#else
        const piSoundEngineBackend::API soundApi = piSoundEngineBackend::API::Null;
#endif
        mSoundBackend = piCreateSoundEngineBackend(soundApi, &mLog);
        if (mSoundBackend == nullptr)
        {
            mLog.Printf(LT_ERROR, L"Failed to create SoundBackend.");
            return false;
        }

        int deviceID = -1;
        const int numDevices = mSoundBackend->GetNumDevices();
        for (int i = 0; i < numDevices; ++i)
        {
            const wchar_t *deviceName = mSoundBackend->GetDeviceName(i);
            if (piwstrcontains(deviceName, L"Rift"))
            {
                deviceID = i;
                break;
            }
        }

        piSoundEngineBackend::Configuration config = {};
        config.mTempPath = mConfig.tmpFolderName;

        if (!mSoundBackend->Init(nullptr, deviceID, &config))
        {
            mLog.Printf(LT_ERROR, L"Failed to initialize SoundBackend.");
            return false;
        }

        mSoundInitialized = true;
        mLog.Printf(LT_DEBUG, L"SoundBackend initialized successfully.");
        return true;
    }

    bool ImmEngineBridge::CreateRenderer()
    {
        mRenderReporter = new MainRenderReporter(&mLog);
        mRenderer = piRenderer::Create(mConfig.rendererApi);
        if (mRenderer == nullptr)
        {
            mLog.Printf(LT_ERROR, L"Failed to create Renderer.");
            return false;
        }

        const char *apiName[] = { "GL", "DX", "GLES", "Metal" };
        mLog.Printf(LT_DEBUG, L"API: %s", pistr2ws(apiName[static_cast<int>(mConfig.rendererApi)]));
        return true;
    }

    bool ImmEngineBridge::InitializeRenderer()
    {
        if (mGraphicsInitialized)
            return true;

        if (!mRenderer->Initialize(mConfig.initializeWindow,
                                   nullptr,
                                   mConfig.initializeDisplay,
                                   mConfig.initializeFullscreen,
                                   mConfig.initializeVsync,
                                   mRenderReporter,
                                   false,
                                   mConfig.graphicsDevice))
        {
            mLog.Printf(LT_ERROR, L"Failed to initialize Renderer.");
            return false;
        }

        mGraphicsInitialized = true;
        return true;
    }

    bool ImmEngineBridge::InitializePlayer()
    {
        if (mPlayerInitialized)
            return true;

        Player::Configuration conf = {};
        conf.colorSpace = static_cast<Drawing::ColorSpace>(mConfig.colorSpace);
        conf.multisamplingLevel = mConfig.antialiasing;
        const bool usesZeroToOneDepth = (mConfig.rendererApi == piRenderer::API::DX || mConfig.rendererApi == piRenderer::API::Metal);
        conf.depthBuffer = usesZeroToOneDepth ? DepthBuffer::Linear10 : DepthBuffer::Linear01;
        conf.clipDepth = usesZeroToOneDepth ? ClipSpaceDepth::FromZeroToOne : ClipSpaceDepth::FromNegativeOneToOne;
        conf.projectionMatrix = usesZeroToOneDepth ? ClipSpaceDepth::FromZeroToOne : ClipSpaceDepth::FromNegativeOneToOne;
        conf.frontIsCCW = (mConfig.rendererApi == piRenderer::API::DX) ? false : true;
        conf.paintRenderingTechnique = Drawing::PaintRenderingTechnique::Static;

        if (!mPlayer.Init(mRenderer, mSoundBackend->GetEngine(), &mLog, &mTimer, &conf))
        {
            mLog.Printf(LT_ERROR, L"Failed to initialize ImmPlayer.");
            return false;
        }

        mPlayerInitialized = true;
        return true;
    }

    void ImmEngineBridge::ShutdownRuntime()
    {
        if (mPlayerInitialized)
        {
            mPlayer.UnloadAllSync();
            mPlayer.Deinit();
            mPlayerInitialized = false;
        }

        if (mGraphicsInitialized && mRenderer != nullptr)
        {
            mRenderer->Deinitialize();
            mGraphicsInitialized = false;
        }

        if (mRenderer != nullptr)
        {
            delete mRenderer;
            mRenderer = nullptr;
        }

        if (mRenderReporter != nullptr)
        {
            delete mRenderReporter;
            mRenderReporter = nullptr;
        }

        if (mSoundBackend != nullptr)
        {
            if (mSoundInitialized)
            {
                mSoundBackend->Deinit();
                mSoundInitialized = false;
            }

            piDestroySoundEngineBackend(mSoundBackend);
            mSoundBackend = nullptr;
        }

        if (mTimerInitialized)
        {
            mTimer.End();
            mTimerInitialized = false;
        }

        if (mLogInitialized)
        {
            mLog.End();
            mLogInitialized = false;
        }

        mBridgeInitialized = false;
    }

    void ImmEngineBridge::ResetConfig()
    {
        mConfig = InitConfig();
    }
}
