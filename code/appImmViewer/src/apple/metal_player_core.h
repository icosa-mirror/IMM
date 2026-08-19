#pragma once

#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piTimer.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piWindow.h"
#include "libImmCore/src/libRender/metal/piMetal_Renderer.h"
#include "libImmCore/src/libSound/piSound.h"
#include "appImmViewer/src/resolve.h"
#include "appImmViewer/src/settings.h"
#include "appImmViewer/src/viewer/viewer.h"

#include <stdint.h>

namespace ExePlayer
{

// Shared native-Metal runtime used by Apple application shells. Presentation,
// lifecycle notifications, file pickers, and gestures remain platform-owned.
class MetalPlayerCore final
{
public:
    MetalPlayerCore();
    ~MetalPlayerCore();

    MetalPlayerCore(const MetalPlayerCore &) = delete;
    MetalPlayerCore &operator=(const MetalPlayerCore &) = delete;

    bool Initialize(void *metalDevice,
                    const wchar_t *settingsPath,
                    const wchar_t *contentPath,
                    const wchar_t *logPath,
                    const char *temporaryDirectory);
    void Shutdown();

    bool Resize(int width, int height);
    bool Draw(void *renderPassDescriptor, void *drawable);
    bool LoadDocument(const wchar_t *contentPath);
    bool WriteCapture(const wchar_t *path);

    void Suspend();
    void Resume();
    void Rotate(float deltaYaw, float deltaPitch);
    void MoveForward(float distance);
    void TogglePlayback();

    bool IsReady() const { return mReady; }
    bool IsDocumentLoaded() const;
    uint64_t FrameIndex() const { return mFrameIndex; }
    ImmCore::ivec2 RenderSize() const { return mRenderSize; }

private:
    class Reporter final : public ImmCore::piRenderer::piReporter
    {
    public:
        void Info(const char *text) override;
        void Error(const char *text, int level) override;
        void Begin(uint64_t, uint64_t, int, int) override {}
        void Texture(const wchar_t *, uint64_t, ImmCore::piRenderer::Format, bool, int, int, int) override {}
        void End() override {}
    };

    bool SetContentPath(const wchar_t *contentPath);
    bool InitializeViewer(const char *temporaryDirectory);
    void DestroyRenderTarget();

    Reporter mReporter;
    ImmCore::piLog mLog;
    ImmCore::piTimer mTimer;
    Settings mSettings;
    Viewer mViewer;
    Resolve mResolve;
    ImmCore::piRendererMetal *mRenderer = nullptr;
    ImmCore::piSoundEngineBackend *mSoundBackend = nullptr;
    ImmCore::piTexture mColorTexture = nullptr;
    ImmCore::piTexture mDepthTexture = nullptr;
    ImmCore::piRTarget mRenderTarget = nullptr;
    ImmCore::ivec2 mRenderSize = ImmCore::ivec2(0, 0);
    ImmCore::piWindowEvents mEvents = {};
    double mTimeBase = 0.0;
    double mLastTime = 0.0;
    uint64_t mFrameIndex = 0;
    bool mFirstFrame = true;
    bool mReady = false;
    bool mSuspended = false;
    bool mLogReady = false;
    bool mTimerReady = false;
    bool mSettingsReady = false;
    bool mResolveReady = false;
    char mTemporaryDirectory[1024] = {};
};

} // namespace ExePlayer
