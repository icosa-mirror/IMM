#if defined(__APPLE__)
#define AA 1
#else
#define AA 8
#endif

// Set this to 1, ONLY if you need to build the viewer without the Oculus SDF installed
// This flag is NOT meant to force mono rendering. Mono rendering can always be forced
// from the config file even if the viewer is built to do VR
#ifndef DISABLE_VR
#define DISABLE_VR 0
#endif

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WINDOWS)
#include <windows.h>
#endif
#include "libImmCore/src/libBasics/piTimer.h"
#include "libImmCore/src/libBasics/piWindow.h"
#include "libImmCore/src/libBasics/piImage.h"
#include "libImmCore/src/libBasics/piFile.h"
#if DISABLE_VR==0
#include "libImmCore/src/libVR/piVR.h"
#endif
#include "viewer/viewer.h"
#include "settings.h"
#include "resolve.h"
using namespace ImmCore;
using namespace ImmImporter;
using namespace ExePlayer;

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

//-----

static const char* vsMirror = ""

"layout(location = 0) in vec2 inVertex;"

"out V2FData"
"{"
"vec2 uv;"
"}vf;"

"void main()"
"{"
"vf.uv = 0.5 + 0.5*inVertex;"
"gl_Position = vec4(inVertex, 0.0, 1.0);"
"}";

static const char* fsMirror = ""

"layout(binding = 0) uniform sampler2D unTex0;"
"layout(location = 0, index = 0) out vec4 outColor;"
"layout(location = 1) uniform vec2 unRes;"

"in V2FData"
"{"
"vec2 uv;"
"}vf;"

"void main()"
"{"
"vec3 col = texture( unTex0, vec2( vf.uv.x, 0.5 + (vf.uv.y-0.5)/(unRes.x/unRes.y) ) ).xyz;"
"col = pow(col, vec3(0.4545));"
"outColor = vec4(col, 1.0);"
"}";


class MainRenderReporter : public piRenderer::piReporter
{
private:
    piLog* mLog;
    wchar_t tmp[2048];

public:
    MainRenderReporter(piLog* log) : piRenderer::piReporter() { mLog = log; }
    virtual ~MainRenderReporter() {}
    void Info(const char* str)
    {
        pistr2ws(tmp, 2048, str);
#if defined(WINDOWS)
        mLog->Printf(LT_MESSAGE, L"%s", tmp);
#else
        mLog->Printf(LT_MESSAGE, L"%ls", tmp);
#endif
    }
    void Error(const char* str, int level)
    {
        pistr2ws(tmp, 2048, str);
#if defined(WINDOWS)
        mLog->Printf(LT_ERROR, L"%s", tmp);
#else
        mLog->Printf(LT_ERROR, L"%ls", tmp);
#endif
    }
    void Begin(uint64_t memCurrent, uint64_t memPeak, int texCurrent, int texPeak)
    {
        mLog->Printf(LT_MESSAGE, L"---- Renderer Report ---- ");
        mLog->Printf(LT_MESSAGE, L"Max Used : %d MB in %d textures and buffers", memPeak >> 20, texPeak);
        mLog->Printf(LT_MESSAGE, L"Leaked   : %d MB in %d textures and buffer", memCurrent >> 20, texCurrent);
    }
    void End(void)
    {
        mLog->Printf(LT_MESSAGE, L"---- Renderer Report ---- ");
    }
    void Texture(const wchar_t* key, uint64_t kb, piRenderer::Format format, bool compressed, int xres, int yres, int zres)
    {
        mLog->Printf(LT_MESSAGE, L"* Texture: %5d kb, %4d x %4d x %4d %2d (%s)", (int)kb, xres, yres, zres, format, (key == nullptr) ? L"null" : key);
    }
};

//----------------------------------------------------------------------------------

static float iDecodeUnsignedFloat(uint32_t bits, int mantissaBits)
{
    const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const uint32_t mantissa = bits & mantissaMask;
    const uint32_t exponent = (bits >> mantissaBits) & 0x1fu;
    if (exponent == 0)
    {
        return ldexpf((float)mantissa / (float)(1u << mantissaBits), -14);
    }
    if (exponent == 31)
    {
        return 1.0f;
    }
    return ldexpf(1.0f + (float)mantissa / (float)(1u << mantissaBits), (int)exponent - 15);
}

static uint8_t iFloatToByte(float value)
{
    if (value <= 0.0f)
    {
        return 0;
    }
    if (value >= 1.0f)
    {
        return 255;
    }
    return (uint8_t)(value * 255.0f + 0.5f);
}

static bool iPathHasExtension(const char *path, const char *extension)
{
    if (!path || !extension)
    {
        return false;
    }
    const size_t pathLength = strlen(path);
    const size_t extensionLength = strlen(extension);
    if (pathLength < extensionLength)
    {
        return false;
    }
    const char *tail = path + pathLength - extensionLength;
    for (size_t i = 0; i < extensionLength; ++i)
    {
        char a = tail[i];
        char b = extension[i];
        if (a >= 'A' && a <= 'Z')
        {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}

static void iDecodeRG11B10Pixels(uint8_t *dstRGB, const uint32_t *pixels, int width, int height)
{
    for (int y = 0; y < height; ++y)
    {
        const uint32_t *srcRow = pixels + (size_t)y * (size_t)width;
        uint8_t *dstRow = dstRGB + (size_t)y * (size_t)width * 3u;
        for (int x = 0; x < width; ++x)
        {
            const uint32_t pixel = srcRow[x];
            dstRow[3 * x + 0] = iFloatToByte(iDecodeUnsignedFloat(pixel & 0x7ffu, 6));
            dstRow[3 * x + 1] = iFloatToByte(iDecodeUnsignedFloat((pixel >> 11u) & 0x7ffu, 6));
            dstRow[3 * x + 2] = iFloatToByte(iDecodeUnsignedFloat((pixel >> 22u) & 0x3ffu, 5));
        }
    }
}

static bool iWriteRG11B10PPM(const char *path, const uint32_t *pixels, int width, int height)
{
    if (!path || !path[0] || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file)
    {
        return false;
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);
    uint8_t *rgb = (uint8_t*)malloc((size_t)width * (size_t)height * 3u);
    if (!rgb)
    {
        fclose(file);
        return false;
    }
    iDecodeRG11B10Pixels(rgb, pixels, width, height);
    const bool ok = fwrite(rgb, 3u, (size_t)width * (size_t)height, file) == (size_t)width * (size_t)height;
    free(rgb);

    fclose(file);
    return ok;
}

static bool iWriteRG11B10PNG(const char *path, const uint32_t *pixels, int width, int height)
{
    if (!path || !path[0] || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    uint8_t *rgb = (uint8_t*)malloc((size_t)width * (size_t)height * 3u);
    if (!rgb)
    {
        return false;
    }
    iDecodeRG11B10Pixels(rgb, pixels, width, height);

    wchar_t *widePath = pistr2ws(path);
    if (!widePath)
    {
        free(rgb);
        return false;
    }

    piImage image;
    image.InitWrap(piImage::TYPE_2D, width, height, 1, piImage::FORMAT_I_RGB, rgb);
    const bool ok = image.WriteToDisk(widePath, 0, L"png");
    image.Free();
    free(widePath);
    free(rgb);
    return ok;
}

static bool iWriteRG11B10Capture(const char *path, const uint32_t *pixels, int width, int height)
{
    if (iPathHasExtension(path, ".png"))
    {
        return iWriteRG11B10PNG(path, pixels, width, height);
    }
    return iWriteRG11B10PPM(path, pixels, width, height);
}

static bool iFileExistsUtf8(const char *path)
{
    if (!path || !path[0])
    {
        return false;
    }

    wchar_t *widePath = pistr2ws(path);
    if (!widePath)
    {
        return false;
    }

    const bool exists = piFile::Exists(widePath);
    free(widePath);
    return exists;
}

static const char *iGetValidationEnv(const char *genericName, const char *legacyName)
{
    const char *value = getenv(genericName);
    if (value && value[0])
    {
        return value;
    }
    return getenv(legacyName);
}

static bool iHasImmExtension(const wchar_t *path)
{
    if (!path)
        return false;
    const wchar_t *dot = wcsrchr(path, L'.');
#if defined(WINDOWS)
    return dot && _wcsicmp(dot, L".imm") == 0;
#else
    return dot && wcscasecmp(dot, L".imm") == 0;
#endif
}

static bool iSetSingleLoadedFile(ExePlayer::Settings *settings, const wchar_t *path)
{
    if (!settings || !path || !path[0])
        return false;

    for (ImmCore::piString &load : settings->mFiles.mLoad)
    {
        load.End();
    }
    settings->mFiles.mLoad.SetLength(0);

    ImmCore::piString *fileToLoad = settings->mFiles.mLoad.GetAddress(0);
    new (fileToLoad) ImmCore::piString();
    settings->mFiles.mLoad.SetLength(1);
    return fileToLoad && fileToLoad->InitCopyW(path);
}

static bool iFindFirstImmBesideExecutable(const wchar_t *executablePath, wchar_t *dst, size_t dstLen)
{
    if (!executablePath || !dst || dstLen == 0)
        return false;

    wchar_t directory[PATH_MAX] = {};
    wcsncpy(directory, executablePath, PATH_MAX - 1);
    wchar_t *slash = wcsrchr(directory, L'\\');
    wchar_t *forwardSlash = wcsrchr(directory, L'/');
    if (!slash || (forwardSlash && forwardSlash > slash))
        slash = forwardSlash;
    if (!slash)
        return false;
    *slash = 0;

#if defined(WINDOWS)
    wchar_t pattern[PATH_MAX] = {};
    swprintf(pattern, PATH_MAX, L"%s\\*.imm", directory);
    WIN32_FIND_DATAW findData = {};
    HANDLE findHandle = FindFirstFileW(pattern, &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            swprintf(dst, dstLen, L"%s\\%s", directory, findData.cFileName);
            found = true;
            break;
        }
    } while (FindNextFileW(findHandle, &findData));
    FindClose(findHandle);
    return found;
#else
    (void)iHasImmExtension;
    return false;
#endif
}

static bool iApplyDefaultImmWhenNoLoadConfigured(ExePlayer::Settings *settings, const wchar_t *executablePath, ImmCore::piLog *log)
{
    if (!settings || settings->mFiles.mLoad.GetLength() > 0)
        return true;

    wchar_t defaultImmPath[PATH_MAX] = {};
    if (!iFindFirstImmBesideExecutable(executablePath, defaultImmPath, PATH_MAX))
        return true;
    if (!iSetSingleLoadedFile(settings, defaultImmPath))
        return false;

#if defined(WINDOWS)
    if (log)
        log->Printf(LT_MESSAGE, L"Using default IMM beside executable: %s", defaultImmPath);
#else
    if (log)
        log->Printf(LT_MESSAGE, L"Using default IMM beside executable: %ls", defaultImmPath);
#endif
    return true;
}

#if !defined(ANDROID)
#if defined(WINDOWS)
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif
#endif


int piMainFunc(const wchar_t* path, const wchar_t** args, int numArgs, void* instance)
{
    const char *validationFrameEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_FRAME", "IMM_GL_VALIDATE_FRAME");
    const bool validationRequested = validationFrameEnv && validationFrameEnv[0];
    const int validationStartupExitCode = validationRequested ? 2 : 0;

    ExePlayer::Viewer mViewer;
    piLog mLog;
    int  mSuperSample;
    piWindowMgr mWinMgr;
    piWindow mWindow;
    piRenderer* mRenderer;
    piSoundEngineBackend* mSoundEngineBackend;
    piRenderer::piReporter* mRenderReporter;
    #if DISABLE_VR==0
    piVRHMD* mHMD;
    piShader mMirrorShader;
    #endif
    ivec2 mWindowSize;
    Settings mSettings;
    piTimer       mTimer;
    ImmPlayer::StereoMode mStereoMode;
    piTexture mColorTextureM;
    piTexture mDepthTextureM;
    piRTarget mRenderTargetM;
    ivec2 mRenderSize;

    Resolve mResolve;
    #if DISABLE_VR==0
    struct TextureChain
    {
        int mRenderNumTextures;
        piTexture mRenderTexture[32];
        piRTarget mRenderTarget[32];
    } mTextureChain[2];
    #endif

#ifdef DEBUG
    //_controlfp(_EM_UNDERFLOW | _EM_INEXACT, _MCW_EM);
#endif

    if (!mLog.Init(L"debug.txt", PILOG_TXT + PILOG_CNS))
        return false;

    if (!mTimer.Init())
        return false;

    //------------------------

    const wchar_t* settingsFileName = L"settings.json";
    if (numArgs > 1)
    {
        settingsFileName = args[1];
    }
#if defined(WINDOWS)
    mLog.Printf(LT_MESSAGE, L"Reading config file \"%s\"...", settingsFileName);
#else
    mLog.Printf(LT_MESSAGE, L"Reading config file \"%ls\"...", settingsFileName);
#endif

    if (!mSettings.Init(settingsFileName, &mLog))
    {
        mLog.Printf(LT_ERROR, L"Failed to load settings");
        return false;
    }
    mLog.Printf(LT_MESSAGE, L"Settings loaded");
    if (!iApplyDefaultImmWhenNoLoadConfigured(&mSettings, path, &mLog))
    {
        mLog.Printf(LT_ERROR, L"Failed to apply default IMM file");
        return false;
    }

    mSuperSample = mSettings.mRendering.mSupersampling;
    if (mSuperSample < 1) { mSuperSample = 1; mLog.Printf(LT_WARNING, L"Supersampling factor must be between 1 and 3 (1 sample per pixel and 3x3 samples per pixel"); }
    if (mSuperSample > 3) { mSuperSample = 3; mLog.Printf(LT_WARNING, L"Supersampling factor must be between 1 and 3 (1 sample per pixel and 3x3 samples per pixel"); }


    mWinMgr = piWindowMgr_Init();
    if (!mWinMgr)
    {
        mLog.Printf(LT_ERROR, L"Failed to init window manager");
        mSettings.End();
        return false;
    }
    mWindow = piWindow_init(mWinMgr, L"rendering", mSettings.mWindow.mPositionX, mSettings.mWindow.mPositionY, mSettings.mWindow.mWidth, mSettings.mWindow.mHeight, mSettings.mWindow.mFullScreen, !mSettings.mWindow.mFullScreen, false, mSettings.mWindow.mFullScreen);
    if (!mWindow)
    {
        mLog.Printf(LT_ERROR, L"Failed to create window");
        mSettings.End();
        return false;
    }
    piWindow_show(mWindow);

    const wchar_t *renderingBackend =
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::DX) ? L"DirectX" :
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Metal) ? L"Metal" :
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Vulkan) ? L"Vulkan" :
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::GLES) ? L"OpenGLES" :
        L"OpenGL";
    mLog.Printf(LT_MESSAGE, L"Rendering Backened: %s", renderingBackend);
    mLog.Printf(LT_MESSAGE, L"Rendering Technique: %s", (mSettings.mRendering.mRenderingTechnique==Settings::Rendering::Technique::Static)?L"Static":L"Pretessellated" );
    #if DISABLE_VR==0
    mLog.Printf(LT_MESSAGE, L"Rendering in VR: %s", (mSettings.mRendering.mEnableVR) ? L"yes" : L"no");
    #else
    mLog.Printf(LT_MESSAGE, L"Rendering in VR: no" );;
    #endif

    // renderer
    mRenderReporter = new MainRenderReporter(&mLog);

    piRenderer::API rendererAPI = piRenderer::API::GL;
    if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::DX)
    {
        rendererAPI = piRenderer::API::DX;
    }
    else if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::GLES)
    {
        rendererAPI = piRenderer::API::GLES;
    }
    else if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Metal)
    {
        rendererAPI = piRenderer::API::Metal;
    }
    else if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Vulkan)
    {
        rendererAPI = piRenderer::API::Vulkan;
    }

    mRenderer = piRenderer::Create(rendererAPI);
    if (!mRenderer)
    {
        mSettings.End();
        return false;
    }

    // renderer
    const void* hwnds[1] = { piWindow_getHandle(mWindow) };
    bool disableRendererErrors = false; // can set this to true
    if (!mRenderer->Initialize(0, hwnds, 1, true, disableRendererErrors, mRenderReporter, true, nullptr))
    {
        mLog.Printf(LT_ERROR, L"Can't create renderer");
        {
            mSettings.End();
            return false;
        }
    }
    mRenderer->SetActiveWindow(0);
    mLog.Printf(LT_MESSAGE, L"Renderer initialized");

    //=============
    mWindowSize = ivec2(mSettings.mWindow.mWidth, mSettings.mWindow.mHeight);

    #if DISABLE_VR==0
    if (mSettings.mRendering.mEnableVR)
    {
        mHMD = nullptr;

        float pd = mSettings.mRendering.mPixelDensity;
        if (pd < 0.1f) { pd = 0.1f; mLog.Printf(LT_WARNING, L"Pixel Density too small"); }
        if (pd > 3.0f) { pd = 3.0f; mLog.Printf(LT_WARNING, L"Pixel Density too big"); }

        mHMD = piVRHMD::Create(piVRHMD::ANY_AVAILABLE, nullptr, 0, pd, &mLog, &mTimer);
        if (mHMD == nullptr)
        {
            mLog.Printf(LT_ERROR, L"Cannot do VR");
            mSettings.End();
            return false;
        }
        mStereoMode = ImmPlayer::StereoMode::Preferred;
        mRenderSize = ivec2(mHMD->mInfo.mVRXres, mHMD->mInfo.mVRYres);

        //-----------------------
        if (mHMD->mType == piVRHMD::Oculus_Rift || mHMD->mType == piVRHMD::Oculus_RiftS || mHMD->mType == piVRHMD::Oculus_Quest)
        {
            if (!mHMD->AttachToWindow(true, mWindowSize.x, mWindowSize.y))
            {
                mSettings.End();
                return false;
            }
            for (int j = 0; j < 2; j++)
            {
                TextureChain* tc = mTextureChain + j;
                tc->mRenderNumTextures = mHMD->mInfo.mTexture[j].mNum;
                for (int i = 0; i < tc->mRenderNumTextures; i++)
                {
                    tc->mRenderTexture[i] = mRenderer->CreateTextureFromID(mHMD->mInfo.mTexture[j].mTexIDColor[i], piRenderer::TextureFilter::MIPMAP);
                    tc->mRenderTarget[i] = mRenderer->CreateRenderTarget(tc->mRenderTexture[i], 0, 0, 0, 0);
                    if (!tc->mRenderTarget[i])
                    {
                        mSettings.End();
                        return false;
                    }
                }
            }
        }
        else if (mHMD->mType == piVRHMD::HTC_Vive)
        {
            for (int j = 0; j < 2; j++)
            {
                TextureChain* tc = mTextureChain + j;
                tc->mRenderNumTextures = mHMD->mInfo.mTexture[j].mNum;
                for (int i = 0; i < tc->mRenderNumTextures; i++)
                {
                    const piRenderer::TextureInfo infocm = { piRenderer::TextureType::T2D, piRenderer::Format::C4_8_UNORM_SRGB, mRenderSize.x, mRenderSize.y, 1, 1 };
                    tc->mRenderTexture[i] = mRenderer->CreateTexture(0, &infocm, false, piRenderer::TextureFilter::LINEAR, piRenderer::TextureWrap::CLAMP, 1.0f, 0);
                    if (!tc->mRenderTexture[i])
                    {
                        mSettings.End();
                        return false;
                    }
                    tc->mRenderTarget[i] = mRenderer->CreateRenderTarget(tc->mRenderTexture[i], 0, 0, 0, 0);
                    if (!tc->mRenderTarget[i])
                    {
                        mSettings.End();
                        return false;
                    }
                }
            }

            piRenderer::TextureInfo info[2];
            mRenderer->GetTextureInfo(mTextureChain[0].mRenderTexture[0], info + 0);
            mRenderer->GetTextureInfo(mTextureChain[1].mRenderTexture[0], info + 1);
            if (!mHMD->AttachToWindow2(reinterpret_cast<void*>(static_cast<uint64_t>(info[0].mDeleteMe)), reinterpret_cast<void*>(static_cast<uint64_t>(info[1].mDeleteMe))))
            {
                mSettings.End();
                return false;
            }
        }
        else
        {
            return false;
        }

        //-----------------------

        char error[2048];
        mMirrorShader = mRenderer->CreateShader(nullptr, vsMirror, nullptr, nullptr, nullptr, fsMirror, error);
        if (!mMirrorShader)
        {
            mLog.Printf(LT_ERROR, L"Can't create mirror shader");
            mSettings.End();
            return false;
        }


        if (!mRenderer->SupportsFeature(piRenderer::RendererFeature::VERTEX_VIEWPORT) || !mRenderer->SupportsFeature(piRenderer::RendererFeature::VIEWPORT_ARRAY))
        {
            mLog.Printf(LT_WARNING, L"Fast stereo is not available, falling back to slow stereo");
            mStereoMode = ImmPlayer::StereoMode::Fallback;
        }
        else
        {
            mLog.Printf(LT_MESSAGE, L"Fast stereo enabled");
            mStereoMode = ImmPlayer::StereoMode::Preferred;
        }

        mHMD->SetTrackingOriginType(piVRHMD::TrackingOrigin::FloorLevel);
    }
    else
    #endif
    {
        #if DISABLE_VR==0
        mHMD = nullptr;
        #endif
        mStereoMode = ImmPlayer::StereoMode::None;
        mRenderSize = mWindowSize;
    }

    //------------------------------

    const char *disableAudioForValidation = getenv("IMM_VIEWER_VALIDATE_DISABLE_AUDIO");
    const bool useNullSoundBackend = disableAudioForValidation && disableAudioForValidation[0];
    mSoundEngineBackend = piCreateSoundEngineBackend(useNullSoundBackend ? piSoundEngineBackend::API::Null : piSoundEngineBackend::API::DirectSoundOVR, &mLog);
    if (!mSoundEngineBackend)
    {
        mLog.Printf(LT_WARNING, L"Sound backend unavailable; continuing without audio");
        mSoundEngineBackend = piCreateSoundEngineBackend(piSoundEngineBackend::API::Null, &mLog);
        if (!mSoundEngineBackend)
        {
            mSettings.End();
            return false;
        }
    }

    const int num = mSoundEngineBackend->GetNumDevices();
    mLog.Printf(LT_MESSAGE, L"%d sound devices", num);
    for (int i = 0; i < num; ++i)
    {
        const wchar_t * deviceName = mSoundEngineBackend->GetDeviceName(i);
#if defined(WINDOWS)
        mLog.Printf(LT_MESSAGE, L"    %d: %s", i, deviceName);
#else
        mLog.Printf(LT_MESSAGE, L"    %d: %ls", i, deviceName);
#endif
    }

    int soundDevice = -1;
    if (mSettings.mSound.mDevice.EqualW(L"Default"))
    {
        #if DISABLE_VR==0
        if (mHMD)
        {
            void* deviceGUID = mHMD->GetSoundOutputGUID();
            if (deviceGUID == nullptr)
            {
                soundDevice = -1;
            }
            else
            {
                soundDevice = mSoundEngineBackend->GetDeviceFromGUID(deviceGUID);
                if (soundDevice == -1)
                {
                    mLog.Printf(LT_WARNING, L"Headset headphones are off. Switching to default sound device");
                    soundDevice = -1;
                }
            }
        }
        else
        #endif
        {
            soundDevice = -1;
        }
    }
    else
    {
        soundDevice = mSoundEngineBackend->GetDeviceFromName(mSettings.mSound.mDevice.GetS());
        if (soundDevice == -1)
        {
            mLog.Printf(LT_ERROR, L"Couldn't find specified sound device");
            mSettings.End();
            return false;
        }
    }

    const wchar_t *deviceName = (soundDevice == -1) ? L"Default" : mSoundEngineBackend->GetDeviceName(soundDevice);
#if defined(WINDOWS)
    mLog.Printf(LT_MESSAGE, L"Sound device selected: \"%s\", requested \"%s\"", deviceName, mSettings.mSound.mDevice.GetS());
#else
    mLog.Printf(LT_MESSAGE, L"Sound device selected: \"%ls\", requested \"%ls\"", deviceName, mSettings.mSound.mDevice.GetS());
#endif

    piSoundEngineBackend::Configuration config;

    if (!mSoundEngineBackend->Init(piWindow_getHandle(mWindow), soundDevice, &config)) // TODO: copy max sounds setting from app
    {
        mLog.Printf(LT_WARNING, L"Sound backend init failed; continuing without audio");
    }



    const int vpmult = (mStereoMode == ImmPlayer::StereoMode::Preferred) ? 2 : 1;

    if (mRenderer->GetAPI() != piRenderer::API::DX)
    {
        const piRenderer::TextureInfo infocm = { piRenderer::TextureType::T2D, piRenderer::Format::C3_11_11_10_FLOAT, mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        const piRenderer::TextureInfo infozm = { piRenderer::TextureType::T2D, piRenderer::Format::DS_24_8_UINT, mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        mLog.Printf(LT_MESSAGE, L"Creating render textures (%d x %d)", mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample);
        mColorTextureM = mRenderer->CreateTexture(0, &infocm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0);
        if (!mColorTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create color render texture");
            return false;
        }
        mDepthTextureM = mRenderer->CreateTexture(0, &infozm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0);
        if (!mDepthTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create depth render texture");
            return false;
        }
    }
    else
    {
        const piRenderer::TextureInfo infocm = { piRenderer::TextureType::T2D, piRenderer::Format::C3_11_11_10_FLOAT, mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        const piRenderer::TextureInfo infozm = { piRenderer::TextureType::T2D, piRenderer::Format::DS_24_8_UINT,            mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        //const piRenderer::TextureInfo2 infozm = { piRenderer::TextureType::T2D, piRenderer::Format::D1_32_FLOAT,            mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        mColorTextureM = mRenderer->CreateTexture2(0, &infocm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0, 1 + 2);
        if (!mColorTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create color render texture (DX path)");
            return false;
        }
        mDepthTextureM = mRenderer->CreateTexture2(0, &infozm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0, 2);
        if (!mDepthTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create depth render texture (DX path)");
            return false;
        }
    }
    if (!mColorTextureM || !mDepthTextureM)
    {
        mLog.Printf(LT_ERROR, L"Render textures missing");
        mSettings.End();
        return false;
    }
    mRenderTargetM = mRenderer->CreateRenderTarget(mColorTextureM, 0, 0, 0, mDepthTextureM);
    if (!mRenderTargetM)
    {
        mLog.Printf(LT_ERROR, L"Failed to create render target");
        return false;
    }


    mLog.Printf(LT_MESSAGE, L"Initializing resolve");
    if (!mResolve.Init(mRenderer, mSuperSample, AA, Resolve::OutputEncoding::DisplaySrgb))
    {
        mLog.Printf(LT_ERROR, L"Resolve init failed");
        mSettings.End();
        return false;
    }
    mLog.Printf(LT_MESSAGE, L"Resolve initialized");

    piSoundEngine* soundEngine = mSoundEngineBackend->GetEngine();

    if (!mViewer.Init(nullptr, mRenderer, soundEngine, &mLog, &mTimer, mStereoMode, &mSettings))
    {
        mLog.Printf(LT_ERROR, L"Viewer init failed");
        mSettings.End();
        return validationStartupExitCode;
    }
    mLog.Printf(LT_MESSAGE, L"Viewer initialized");

    // enter render loop

    double to = mTimer.GetTime();
    double renderFpsTo = 0.0;
    int renderFrame = 0;
    float renderFps = 0.0;
    int totalFrames = 0;
    int done = 0;
    bool doSave = true;
    double oldTime;
    bool enabled = true;

    oldTime = to;


#if defined(WINDOWS)
    static const uint32_t kRenderBudgetMicroseconds = 9000;
#elif defined(ANDROID)
    static const uint32_t kRenderBudgetMicroseconds = 5000;
#else
    static const uint32_t kRenderBudgetMicroseconds = 9000;
#endif


    mLog.Printf(LT_MESSAGE, L"X = next,  Z = prev,  C = restart,   v = replay,   P = pause/resume");
    int frameid = 0;
    bool isFirstFrame = true;
    const bool validationEnabled = validationRequested;
    const uint64_t validationFrame = validationEnabled ? strtoull(validationFrameEnv, nullptr, 10) : 0;
    double validationFixedDt = -1.0;
    uint64_t validationMaxFrame = validationFrame + 300;
    uint64_t validationMinNonZeroPixels = 16;
    uint64_t validationMinDrawCalls = 1;
    uint64_t validationMinPictureDrawCalls = 1;
    uint64_t validationMinPicture360DrawCalls = 1;
    uint64_t validationMinPicture360EquirectDrawCalls = 0;
    uint64_t validationMinPicture360CubemapDrawCalls = 0;
    uint64_t validationMinTriangles = 1;
    uint64_t validationPlayerFrame = 0;
    bool validationPlayerFrameEnabled = false;
    bool validationDone = false;
    int validationExitCode = 0;
    char validationCapturePath[PATH_MAX] = {};

    const char *validationMaxFrameEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MAX_FRAME", "IMM_GL_VALIDATE_MAX_FRAME");
    if (validationMaxFrameEnv && validationMaxFrameEnv[0])
    {
        validationMaxFrame = strtoull(validationMaxFrameEnv, nullptr, 10);
    }
    const char *validationFixedDtEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_FIXED_DT", "IMM_GL_VALIDATE_FIXED_DT");
    if (validationFixedDtEnv && validationFixedDtEnv[0])
    {
        validationFixedDt = atof(validationFixedDtEnv);
    }
    const char *validationMinNonZeroEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_NONZERO", "IMM_GL_VALIDATE_MIN_NONZERO");
    if (validationMinNonZeroEnv && validationMinNonZeroEnv[0])
    {
        validationMinNonZeroPixels = strtoull(validationMinNonZeroEnv, nullptr, 10);
    }
    const char *validationMinDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_DRAWCALLS", "IMM_GL_VALIDATE_MIN_DRAWCALLS");
    if (validationMinDrawCallsEnv && validationMinDrawCallsEnv[0])
    {
        validationMinDrawCalls = strtoull(validationMinDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPictureDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE_DRAWCALLS");
    if (validationMinPictureDrawCallsEnv && validationMinPictureDrawCallsEnv[0])
    {
        validationMinPictureDrawCalls = strtoull(validationMinPictureDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPicture360DrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE360_DRAWCALLS");
    if (validationMinPicture360DrawCallsEnv && validationMinPicture360DrawCallsEnv[0])
    {
        validationMinPicture360DrawCalls = strtoull(validationMinPicture360DrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPicture360EquirectDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS");
    if (validationMinPicture360EquirectDrawCallsEnv && validationMinPicture360EquirectDrawCallsEnv[0])
    {
        validationMinPicture360EquirectDrawCalls = strtoull(validationMinPicture360EquirectDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPicture360CubemapDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS");
    if (validationMinPicture360CubemapDrawCallsEnv && validationMinPicture360CubemapDrawCallsEnv[0])
    {
        validationMinPicture360CubemapDrawCalls = strtoull(validationMinPicture360CubemapDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinTrianglesEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_TRIANGLES", "IMM_GL_VALIDATE_MIN_TRIANGLES");
    if (validationMinTrianglesEnv && validationMinTrianglesEnv[0])
    {
        validationMinTriangles = strtoull(validationMinTrianglesEnv, nullptr, 10);
    }
    const char *validationPlayerFrameEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_PLAYER_FRAME", "IMM_GL_VALIDATE_PLAYER_FRAME");
    if (validationPlayerFrameEnv && validationPlayerFrameEnv[0])
    {
        validationPlayerFrame = strtoull(validationPlayerFrameEnv, nullptr, 10);
        validationPlayerFrameEnabled = true;
    }
    const char *validationCapturePathEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_CAPTURE_PATH", "IMM_GL_VALIDATE_CAPTURE_PATH");
    if (validationCapturePathEnv && validationCapturePathEnv[0])
    {
        strncpy(validationCapturePath, validationCapturePathEnv, sizeof(validationCapturePath) - 1);
        validationCapturePath[sizeof(validationCapturePath) - 1] = 0;
    }

    while (!done)
    {
        frameid++;
        const double time = mTimer.GetTime() - to;
        float dtime = float(time - oldTime);
        if (validationEnabled && validationFixedDt >= 0.0)
        {
            dtime = (float)validationFixedDt;
        }
        oldTime = time;

        float mQuitFade = 1.0f;

        // events
        piWindowEvents_Erase(mWindow);
        piWindowMgr_MessageLoop(mWinMgr);
        done |= piWindow_getExitReq(mWindow);
        piWindowEvents* evt = piWindow_getEvents(mWindow);
        piWindowEvents_GetMouse_D(&evt->mouse);

        #if DISABLE_VR==0
        if (mStereoMode != ImmPlayer::StereoMode::None)
        {
            int tid[2];
            bool needMipMapping;
            mHMD->BeginFrame(tid + 0, tid + 1, &needMipMapping);

            const trans3d vr_to_head = fromMatrix(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));

            mViewer.GlobalWork(evt, mStereoMode != ImmPlayer::StereoMode::None, vr_to_head, mHMD->mInfo.mController, &mHMD->mInfo.mRemote, &mLog, dtime, mWindowSize, enabled ? 1 : 0, kRenderBudgetMicroseconds, isFirstFrame);


            mViewer.GlobalRender(vr_to_head, vec4(mHMD->mInfo.mHead.mProjection));

            if (mStereoMode == ImmPlayer::StereoMode::Fallback)
            {
                const int vpS[4] = { 0, 0, mRenderSize.x, mRenderSize.y };
                const int vpM[4] = { 0, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
                for (int i = 0; i < 2; i++)
                {
                    mRenderer->SetRenderTarget(mRenderTargetM);
                    mRenderer->SetViewport(0, vpM);
                    mRenderer->SetWriteMask(true, false, false, false, true);
                    const mat4x4d headToEye = f2d(mat4x4(mHMD->mInfo.mEye[i].mCamera)) * invert(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));
                    mViewer.RenderStereoMultiPass(mRenderSize*mSuperSample, i, headToEye, vec4(mHMD->mInfo.mEye[i].mProjection), vr_to_head);
                    // resolve multisampling and postpro
                    mResolve.Do(mRenderer, mTextureChain[i].mRenderTarget[tid[i]], vpS, 0, mQuitFade, mColorTextureM);
                }
            }
            else
            {
                mRenderer->SetRenderTarget(mRenderTargetM);
                #if 0
                const int vpL[4] = {                            0, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
                const int vpR[4] = { mRenderSize.x * mSuperSample, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
                mRenderer->SetViewport(0, vpL);
                mRenderer->SetViewport(1, vpR);
                #else
                const float data[12] = {
                    0.0f,                              0.0f, float(mRenderSize.x*mSuperSample), float(mRenderSize.y*mSuperSample), 0.0f, 0.0f,
                    float(mRenderSize.x*mSuperSample), 0.0f, float(mRenderSize.x*mSuperSample), float(mRenderSize.y*mSuperSample), 0.0f, 0.0f };
                mRenderer->SetViewports(2, data);
                #endif
                const mat4x4d headToLEye = f2d(mat4x4(mHMD->mInfo.mEye[0].mCamera)) * invert(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));
                const mat4x4d headToREye = f2d(mat4x4(mHMD->mInfo.mEye[1].mCamera)) * invert(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));

                mRenderer->SetWriteMask(true, false, false, false, true);
                mViewer.RenderStereoSinglePass(mRenderSize*mSuperSample, vr_to_head, headToLEye, vec4(mHMD->mInfo.mEye[0].mProjection), headToREye, vec4(mHMD->mInfo.mEye[1].mProjection), mHMD);
                // resolve multisampling and postpro
                for (int i = 0; i < 2; i++)
                {
                    const int unXOffset = i * mRenderSize.x;
                    mResolve.Do(mRenderer, mTextureChain[i].mRenderTarget[tid[i]], mHMD->mInfo.mEye[i].mVP, unXOffset, mQuitFade, mColorTextureM);
                }
            }

            // compute mipmaps before distortion occurs
            mRenderer->ComputeMipmaps(mTextureChain[0].mRenderTexture[tid[0]]);
            if (needMipMapping)
            {
                mRenderer->ComputeMipmaps(mTextureChain[1].mRenderTexture[tid[1]]);
            }

            // mirror
            const int wvp[4] = { 0, 0, mWindowSize.x, mWindowSize.y };
            mRenderer->SetRenderTarget(nullptr);
            mRenderer->SetViewport(0, wvp);
            mRenderer->SetWriteMask(true, false, false, false, false);
            mRenderer->SetState(piSTATE_CULL_FACE, false);
            mRenderer->SetState(piSTATE_DEPTH_TEST, false);
            // mRenderer->AttachSamplers(1, mMirrorSampler );
            mRenderer->AttachTextures(1, mTextureChain[0].mRenderTexture[tid[0]]);
            // mRenderer->AttachTextures(1, mMirrorRenderTexture);
            mRenderer->AttachShader(mMirrorShader);
            const float data[2] = { float(mWindowSize.x), float(mWindowSize.y) };
            mRenderer->SetShaderConstant2F(1, data, 1);
            //mRenderer->SetShaderConstantSampler(0, 0);
            mRenderer->DrawUnitQuad_XY(1);
            mRenderer->DettachTextures();
            mRenderer->DettachSamplers();
            mRenderer->DettachShader();

            mHMD->EndFrame();

            mRenderer->SwapBuffers();
        }
        else
        #endif
        {
            const int vpM[4] = { 0, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
            const trans3d vr_to_head = trans3d::identity();

            mRenderer->SetRenderTarget(mRenderTargetM);
            mRenderer->SetViewport(0, vpM);
			mRenderer->SetWriteMask(true, false, false, false, true);
            #if DISABLE_VR==0
            mViewer.GlobalWork(evt, mStereoMode != ImmPlayer::StereoMode::None, vr_to_head, mHMD->mInfo.mController, &mHMD->mInfo.mRemote, &mLog, dtime, mWindowSize, enabled ? 1 : 0, kRenderBudgetMicroseconds, isFirstFrame);
            #else
            mViewer.GlobalWork(evt, mStereoMode != ImmPlayer::StereoMode::None, vr_to_head, nullptr, nullptr, &mLog, dtime, mWindowSize, enabled ? 1 : 0, kRenderBudgetMicroseconds, isFirstFrame);
            #endif

            // render
            mViewer.GlobalRender(vr_to_head, vec4(0.0f));
            // render
            mViewer.RenderMono(mRenderSize*mSuperSample, vr_to_head, 0);

            // Resolve before validation so capture/readback observes the frame just rendered.
            mResolve.Do(mRenderer, nullptr, vpM, 0, mQuitFade, mColorTextureM);

            if (validationEnabled && !validationDone && (uint64_t)frameid >= validationFrame)
            {
                const size_t pixelCount = (size_t)mRenderSize.x * (size_t)mRenderSize.y;
                uint32_t *pixels = (uint32_t*)malloc(pixelCount * sizeof(uint32_t));
                if (!pixels)
                {
                    mLog.Printf(LT_ERROR, L"IMM GL validation failed: could not allocate readback buffer");
                    validationDone = true;
                    validationExitCode = 2;
                    done = 1;
                }
                else
                {
                    mRenderer->GetTextureContent(mColorTextureM, pixels, piRenderer::Format::C3_11_11_10_FLOAT);

                    uint64_t nonZeroPixels = 0;
                    uint64_t hash = 1469598103934665603ull;
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        if (pixels[i] != 0)
                        {
                            ++nonZeroPixels;
                        }
                        hash ^= pixels[i];
                        hash *= 1099511628211ull;
                    }

                    const ImmPlayer::Player::PerformanceInfo &perf = mViewer.GetPerformanceInfoForFrame();
                    const bool passed =
                        nonZeroPixels >= validationMinNonZeroPixels &&
                        (uint64_t)perf.numDrawCalls >= validationMinDrawCalls &&
                        (uint64_t)perf.numPictureDrawCalls >= validationMinPictureDrawCalls &&
                        (uint64_t)perf.numPicture360DrawCalls >= validationMinPicture360DrawCalls &&
                        (uint64_t)perf.numPicture360EquirectDrawCalls >= validationMinPicture360EquirectDrawCalls &&
                        (uint64_t)perf.numPicture360CubemapDrawCalls >= validationMinPicture360CubemapDrawCalls &&
                        (uint64_t)perf.numTriangles >= validationMinTriangles &&
                        (!validationPlayerFrameEnabled || perf.validationTimeFrame >= validationPlayerFrame);

                    if (!passed && (uint64_t)frameid < validationMaxFrame)
                    {
                        free(pixels);
                    }
                    else
                    {
                        validationDone = true;
                        if (!passed)
                        {
                            mLog.Printf(LT_ERROR,
                                        L"IMM GL validation failed: frame=%d pixels=%llu nonZero=%llu minNonZero=%llu hash=%llu drawCalls=%d minDrawCalls=%llu paintDrawCalls=%d pictureDrawCalls=%d minPictureDrawCalls=%llu picture2DDrawCalls=%d picture360DrawCalls=%d minPicture360DrawCalls=%llu picture360EquirectDrawCalls=%d minPicture360EquirectDrawCalls=%llu picture360CubemapDrawCalls=%d minPicture360CubemapDrawCalls=%llu modelDrawCalls=%d triangles=%d minTriangles=%llu playerFrame=%llu culledCalls=%d",
                                        frameid,
                                        (unsigned long long)pixelCount,
                                        (unsigned long long)nonZeroPixels,
                                        (unsigned long long)validationMinNonZeroPixels,
                                        (unsigned long long)hash,
                                        perf.numDrawCalls,
                                        (unsigned long long)validationMinDrawCalls,
                                        perf.numPaintDrawCalls,
                                        perf.numPictureDrawCalls,
                                        (unsigned long long)validationMinPictureDrawCalls,
                                        perf.numPicture2DDrawCalls,
                                        perf.numPicture360DrawCalls,
                                        (unsigned long long)validationMinPicture360DrawCalls,
                                        perf.numPicture360EquirectDrawCalls,
                                        (unsigned long long)validationMinPicture360EquirectDrawCalls,
                                        perf.numPicture360CubemapDrawCalls,
                                        (unsigned long long)validationMinPicture360CubemapDrawCalls,
                                        perf.numModelDrawCalls,
                                        perf.numTriangles,
                                        (unsigned long long)validationMinTriangles,
                                        (unsigned long long)perf.validationTimeFrame,
                                        perf.numDrawCallsCulled);
                            validationExitCode = 2;
                        }
                        else
                        {
                            mLog.Printf(LT_MESSAGE,
                                        L"IMM GL validation: frame=%d pixels=%llu nonZero=%llu hash=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture2DDrawCalls=%d picture360DrawCalls=%d picture360EquirectDrawCalls=%d picture360CubemapDrawCalls=%d modelDrawCalls=%d triangles=%d playerFrame=%llu culledCalls=%d",
                                        frameid,
                                        (unsigned long long)pixelCount,
                                        (unsigned long long)nonZeroPixels,
                                        (unsigned long long)hash,
                                        perf.numDrawCalls,
                                        perf.numPaintDrawCalls,
                                        perf.numPictureDrawCalls,
                                        perf.numPicture2DDrawCalls,
                                        perf.numPicture360DrawCalls,
                                        perf.numPicture360EquirectDrawCalls,
                                        perf.numPicture360CubemapDrawCalls,
                                        perf.numModelDrawCalls,
                                        perf.numTriangles,
                                        (unsigned long long)perf.validationTimeFrame,
                                        perf.numDrawCallsCulled);
                            if (validationCapturePath[0])
                            {
                                wchar_t *wideCapturePath = pistr2ws(validationCapturePath);
                                if (wideCapturePath)
                                {
#if defined(WINDOWS)
                                    mLog.Printf(LT_MESSAGE, L"IMM GL validation capture path: %s", wideCapturePath);
#else
                                    mLog.Printf(LT_MESSAGE, L"IMM GL validation capture path: %ls", wideCapturePath);
#endif
                                    free(wideCapturePath);
                                }
                                if (iWriteRG11B10Capture(validationCapturePath, pixels, mRenderSize.x, mRenderSize.y))
                                {
                                    if (iFileExistsUtf8(validationCapturePath))
                                    {
                                        mLog.Printf(LT_MESSAGE, L"IMM GL validation capture written");
                                    }
                                    else
                                    {
                                        mLog.Printf(LT_ERROR, L"IMM GL validation capture write reported success but file is missing");
                                        validationExitCode = 2;
                                    }
                                }
                                else
                                {
                                    mLog.Printf(LT_ERROR, L"IMM GL validation capture failed");
                                    validationExitCode = 2;
                                }
                            }
                        }
                        free(pixels);
                        done = 1;
                    }
                }
            }

            if (!done)
            {
                mRenderer->SwapBuffers();
            }
        }

        mSoundEngineBackend->Tick();

        totalFrames++;

        // update fps counter
        renderFrame++;
        const double dt = time - renderFpsTo;
        if (dt > 1.0)
        {
            renderFps = (float)renderFrame / (float)dt;
            renderFrame = 0;
            renderFpsTo = time;
        }
        if ((totalFrames & 63) == 0)
        {
            wchar_t str[64];
            piwsprintf(str, 63, L"%.1f fps :: %.2f", renderFps, time);
            piWindow_setText(mWindow, str);
        }
    }

    mViewer.Deinit();

    mSoundEngineBackend->Deinit();

    piDestroySoundEngineBackend(mSoundEngineBackend);

    mResolve.DeInit(mRenderer);

    mRenderer->DestroyRenderTarget(mRenderTargetM);
    mRenderer->DestroyTexture(mDepthTextureM);
    mRenderer->DestroyTexture(mColorTextureM);
    #if DISABLE_VR==0
    if (mStereoMode != ImmPlayer::StereoMode::None)
    {
        mRenderer->DestroyShader(mMirrorShader);
        for (int j = 0; j < 2; j++)
        {
            for (int i = 0; i < mTextureChain[j].mRenderNumTextures; i++)
            {
                mRenderer->DestroyTexture(mTextureChain[j].mRenderTexture[i]);
                mRenderer->DestroyRenderTarget(mTextureChain[j].mRenderTarget[i]);
            }
        }
    }

    if (mStereoMode != ImmPlayer::StereoMode::None)
    {
        piVRHMD::Destroy(mHMD);
    }
    #endif

    mRenderer->Deinitialize();
    mRenderer->Report();

    delete mRenderReporter;
    delete mRenderer;
    piWindow_end(mWindow);
    piWindowMgr_End(mWinMgr);

    mSettings.End();

    mLog.End();

    return validationEnabled ? validationExitCode : 1;
}
