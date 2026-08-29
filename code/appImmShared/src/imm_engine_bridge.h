#pragma once

#include "libImmPlayer/src/player.h"

namespace ImmShared
{
    class ImmEngineBridge
    {
    public:
        static constexpr int kMaxCameras = 256;

        struct InitConfig
        {
            int colorSpace = 0;
            int antialiasing = 0;
            const char *logFileName = nullptr;
            const char *tmpFolderName = nullptr;
            ImmCore::piRenderer::API rendererApi = ImmCore::piRenderer::API::GL;
            void *graphicsDevice = nullptr;
            bool metalUnityProjectionAdjusted = false;
            bool reverseDepthBuffer = false;
            bool overrideFrontIsCCW = false;
            bool frontIsCCW = true;
            bool initializeRendererOnInit = true;
            int initializeWindow = 0;
            int initializeDisplay = 0;
            bool initializeFullscreen = false;
            bool initializeVsync = false;
        };

        struct CameraState
        {
            int stereoType = 0;
            int currentEye = 0;
            ImmCore::mat4x4 world2Head;
            ImmCore::mat4x4 world2LeftEye;
            ImmCore::mat4x4 world2RightEye;
            ImmCore::mat4x4 headProjection;
            ImmCore::mat4x4 leftEyeProjection;
            ImmCore::mat4x4 rightEyeProjection;
        };

        struct ViewportInfo
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            float minDepth = 0.0f;
            float maxDepth = 1.0f;
            bool forceViewport = false;
        };

        ImmEngineBridge();
        ~ImmEngineBridge();

        bool Init(const InitConfig &config);
        bool CompleteGraphicsInitialization();
        bool CompleteGraphicsInitialization(void *graphicsDevice);
        void Shutdown();

        void GlobalWork(bool enabled, int budgetMicroseconds = 9000);
        void SetCameraMatrices(int cameraID,
                               int stereoType,
                               const ImmCore::mat4x4 *world2Head,
                               const ImmCore::mat4x4 *headProjection,
                               const ImmCore::mat4x4 *world2LeftEye,
                               const ImmCore::mat4x4 *leftEyeProjection,
                               const ImmCore::mat4x4 *world2RightEye,
                               const ImmCore::mat4x4 *rightEyeProjection);
        bool PrepareCamera(int cameraID);
        bool RenderPreparedCamera(int cameraID, const ViewportInfo &viewport, int eyeID = 0, bool tickSound = true);
        bool RenderCamera(int cameraID, const ViewportInfo &viewport, int eyeID = 0, bool tickSound = true);

        ImmPlayer::Player *GetPlayer();
        const ImmPlayer::Player *GetPlayer() const;
        ImmCore::piLog *GetLog();
        const ImmCore::piLog *GetLog() const;
        ImmCore::piRenderer *GetRenderer();
        const ImmCore::piRenderer *GetRenderer() const;
        const CameraState *GetCameraState(int cameraID) const;
        bool IsInitialized() const;
        bool IsGraphicsInitialized() const;

    private:
        struct MainRenderReporter;

        bool InitializeLog();
        bool InitializeTimer();
        bool InitializeSound();
        bool CreateRenderer();
        bool InitializeRenderer();
        bool InitializePlayer();
        void ShutdownRuntime();
        void ResetConfig();

    private:
        InitConfig mConfig;
        CameraState mCamera[kMaxCameras];
        ImmCore::piRenderer *mRenderer;
        MainRenderReporter *mRenderReporter;
        ImmCore::piLog mLog;
        ImmCore::piSoundEngineBackend *mSoundBackend;
        ImmCore::piTimer mTimer;
        ImmPlayer::Player mPlayer;
        bool mLogInitialized;
        bool mTimerInitialized;
        bool mSoundInitialized;
        bool mGraphicsInitialized;
        bool mPlayerInitialized;
        bool mBridgeInitialized;
    };
}
