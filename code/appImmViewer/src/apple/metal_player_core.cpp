#include "metal_player_core.h"

#include "libImmCore/src/libBasics/piImage.h"
#include "libImmCore/src/libBasics/piString.h"

#include <math.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ExePlayer
{

namespace
{
float DecodeUnsignedFloat(uint32_t bits, int mantissaBits)
{
    const uint32_t exponent = bits >> mantissaBits;
    const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const uint32_t mantissa = bits & mantissaMask;
    if (exponent == 0)
        return ldexpf((float)mantissa, -14 - mantissaBits);
    if (exponent == 31)
        return mantissa ? NAN : INFINITY;
    return ldexpf(1.0f + ((float)mantissa / (float)(1u << mantissaBits)), (int)exponent - 15);
}

uint8_t LinearToSrgbByte(float value)
{
    if (!isfinite(value) || value <= 0.0f)
        return 0;
    if (value > 1.0f)
        value = 1.0f;
    const float srgb = value <= 0.0031308f ? 12.92f * value : 1.055f * powf(value, 1.0f / 2.4f) - 0.055f;
    return (uint8_t)(srgb * 255.0f + 0.5f);
}
}

void MetalPlayerCore::Reporter::Info(const char *text)
{
    fprintf(stdout, "IMM Apple Metal: %s\n", text ? text : "");
}

void MetalPlayerCore::Reporter::Error(const char *text, int level)
{
    fprintf(stderr, "IMM Apple Metal error[%d]: %s\n", level, text ? text : "");
}

MetalPlayerCore::MetalPlayerCore() = default;

MetalPlayerCore::~MetalPlayerCore()
{
    Shutdown();
}

bool MetalPlayerCore::SetContentPath(const wchar_t *contentPath)
{
    if (!contentPath || !contentPath[0])
        return false;
    for (ImmCore::piString &load : mSettings.mFiles.mLoad)
        load.End();
    mSettings.mFiles.mLoad.SetLength(0);
    ImmCore::piString *entry = mSettings.mFiles.mLoad.GetAddress(0);
    new (entry) ImmCore::piString();
    mSettings.mFiles.mLoad.SetLength(1);
    return entry->InitCopyW(contentPath);
}

bool MetalPlayerCore::Initialize(void *metalDevice,
                                 const wchar_t *settingsPath,
                                 const wchar_t *contentPath,
                                 const wchar_t *logPath,
                                 const char *temporaryDirectory)
{
    Shutdown();
    if (!metalDevice || !settingsPath || !contentPath || !logPath || !temporaryDirectory)
        return false;
    snprintf(mTemporaryDirectory, sizeof(mTemporaryDirectory), "%s", temporaryDirectory);

    mRenderer = static_cast<ImmCore::piRendererMetal *>(ImmCore::piRenderer::Create(ImmCore::piRenderer::API::Metal));
    if (!mRenderer || !mRenderer->Initialize(0, nullptr, 0, true, false, &mReporter, false, metalDevice))
        return false;
    if (!mLog.Init(logPath, PILOG_TXT + PILOG_CNS))
        return false;
    mLogReady = true;
    if (!mTimer.Init())
        return false;
    mTimerReady = true;
    if (!mSettings.Init(settingsPath, &mLog))
        return false;
    mSettingsReady = true;
    mSettings.mRendering.mRenderingAPI = Settings::Rendering::API::Metal;
    mSettings.mRendering.mEnableVR = false;
    if (!SetContentPath(contentPath))
        return false;
    if (!mResolve.Init(mRenderer, 1, 1, Resolve::OutputEncoding::DisplaySrgb))
        return false;
    mResolveReady = true;
    return InitializeViewer(temporaryDirectory);
}

bool MetalPlayerCore::InitializeViewer(const char *temporaryDirectory)
{
    mSoundBackend = ImmCore::piCreateSoundEngineBackend(ImmCore::piSoundEngineBackend::API::AVFoundation, &mLog);
    ImmCore::piSoundEngineBackend::Configuration soundConfig;
    soundConfig.mTempPath = temporaryDirectory;
    if (!mSoundBackend || !mSoundBackend->Init(nullptr, -1, &soundConfig))
        return false;
    if (!mViewer.Init(nullptr, mRenderer, mSoundBackend->GetEngine(), &mLog, &mTimer, ImmPlayer::StereoMode::None, &mSettings))
        return false;
    mReady = true;
    mFirstFrame = true;
    mTimeBase = mTimer.GetTime();
    mLastTime = 0.0;
    mFrameIndex = 0;
    memset(&mEvents, 0, sizeof(mEvents));
    return true;
}

void MetalPlayerCore::DestroyRenderTarget()
{
    if (!mRenderer)
        return;
    if (mRenderTarget)
        mRenderer->DestroyRenderTarget(mRenderTarget);
    if (mDepthTexture)
        mRenderer->DestroyTexture(mDepthTexture);
    if (mColorTexture)
        mRenderer->DestroyTexture(mColorTexture);
    mRenderTarget = nullptr;
    mDepthTexture = nullptr;
    mColorTexture = nullptr;
    mRenderSize = ImmCore::ivec2(0, 0);
}

bool MetalPlayerCore::Resize(int width, int height)
{
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    if (mRenderTarget && mRenderSize.x == width && mRenderSize.y == height)
        return true;
    DestroyRenderTarget();
    if (!mRenderer)
        return false;
    mRenderSize = ImmCore::ivec2(width, height);
    const ImmCore::piRenderer::TextureInfo colorInfo = {
        ImmCore::piRenderer::TextureType::T2D, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT,
        width, height, 1, 1, 1, 0
    };
    const ImmCore::piRenderer::TextureInfo depthInfo = {
        ImmCore::piRenderer::TextureType::T2D, ImmCore::piRenderer::Format::D1_32_FLOAT,
        width, height, 1, 1, 1, 0
    };
    mColorTexture = mRenderer->CreateTexture(nullptr, &colorInfo, false, ImmCore::piRenderer::TextureFilter::NONE,
                                              ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f, nullptr);
    mDepthTexture = mRenderer->CreateTexture(nullptr, &depthInfo, false, ImmCore::piRenderer::TextureFilter::NONE,
                                              ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f, nullptr);
    mRenderTarget = mRenderer->CreateRenderTarget(mColorTexture, nullptr, nullptr, nullptr, mDepthTexture);
    return mColorTexture && mDepthTexture && mRenderTarget;
}

bool MetalPlayerCore::Draw(void *renderPassDescriptor, void *drawable)
{
    if (!mReady || mSuspended || !mRenderTarget || !renderPassDescriptor || !drawable)
        return false;
    if (!mRenderer->BeginNativeFrame(renderPassDescriptor, drawable))
        return false;

    const double now = mTimer.GetTime() - mTimeBase;
    const float dt = (float)(now - mLastTime);
    mLastTime = now;
    const ImmCore::trans3d head = ImmCore::trans3d::identity();
    mViewer.GlobalWork(&mEvents, false, head, nullptr, nullptr, &mLog, dt, mRenderSize, true, 9000, mFirstFrame);
    memset(&mEvents, 0, sizeof(mEvents));

    const int viewport[4] = {0, 0, mRenderSize.x, mRenderSize.y};
    const float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    mRenderer->SetRenderTarget(mRenderTarget);
    mRenderer->SetViewport(0, viewport);
    mRenderer->SetWriteMask(true, false, false, false, true);
    mRenderer->Clear(clear, nullptr, nullptr, nullptr, true);
    mViewer.GlobalRender(head, ImmCore::vec4(0.0f));
    mViewer.RenderMono(mRenderSize, head, 0);
    mResolve.Do(mRenderer, nullptr, viewport, 0, 1.0f, mColorTexture);
    mSoundBackend->Tick();
    mRenderer->EndNativeFrame();
    ++mFrameIndex;
    return true;
}

bool MetalPlayerCore::LoadDocument(const wchar_t *contentPath)
{
    if (!mReady || !SetContentPath(contentPath))
        return false;
    mViewer.Deinit();
    mReady = false;
    mSoundBackend->Deinit();
    ImmCore::piDestroySoundEngineBackend(mSoundBackend);
    mSoundBackend = nullptr;
    return InitializeViewer(mTemporaryDirectory);
}

bool MetalPlayerCore::WriteCapture(const wchar_t *path)
{
    if (!mReady || !mColorTexture || !path)
        return false;
    const size_t pixelCount = (size_t)mRenderSize.x * (size_t)mRenderSize.y;
    uint32_t *packed = (uint32_t *)malloc(pixelCount * sizeof(uint32_t));
    uint8_t *rgb = (uint8_t *)malloc(pixelCount * 3u);
    if (!packed || !rgb)
    {
        free(packed);
        free(rgb);
        return false;
    }
    mRenderer->GetTextureContent(mColorTexture, packed, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        rgb[3u * i + 0u] = LinearToSrgbByte(DecodeUnsignedFloat(packed[i] & 0x7ffu, 6));
        rgb[3u * i + 1u] = LinearToSrgbByte(DecodeUnsignedFloat((packed[i] >> 11u) & 0x7ffu, 6));
        rgb[3u * i + 2u] = LinearToSrgbByte(DecodeUnsignedFloat((packed[i] >> 22u) & 0x3ffu, 5));
    }
    ImmCore::piImage image;
    image.InitWrap(ImmCore::piImage::TYPE_2D, mRenderSize.x, mRenderSize.y, 1, ImmCore::piImage::FORMAT_I_RGB, rgb);
    const bool result = image.WriteToDisk(path, 0, L"png");
    image.Free();
    free(packed);
    free(rgb);
    return result;
}

void MetalPlayerCore::Suspend()
{
    if (mReady && !mSuspended && mViewer.IsDocumentLoaded(0))
        mViewer.Pause(0);
    mSuspended = true;
}

void MetalPlayerCore::Resume()
{
    if (mReady && mSuspended && mViewer.IsDocumentLoaded(0))
        mViewer.Resume(0);
    mTimeBase = mTimer.GetTime() - mLastTime;
    mSuspended = false;
    mTemporaryDirectory[0] = 0;
}

void MetalPlayerCore::Rotate(float deltaYaw, float deltaPitch)
{
    if (mReady)
        mViewer.RotateCamera(deltaYaw, deltaPitch);
}

void MetalPlayerCore::MoveForward(float distance)
{
    if (mReady)
        mViewer.MoveCameraForward(distance);
}

void MetalPlayerCore::TogglePlayback()
{
    if (mReady && mViewer.IsDocumentLoaded(0))
        mViewer.TogglePlaybackState(0);
}

bool MetalPlayerCore::IsDocumentLoaded() const
{
    return mReady && mViewer.IsDocumentLoaded(0);
}

void MetalPlayerCore::Shutdown()
{
    if (mReady)
    {
        mViewer.Deinit();
        mReady = false;
    }
    if (mSoundBackend)
    {
        mSoundBackend->Deinit();
        ImmCore::piDestroySoundEngineBackend(mSoundBackend);
        mSoundBackend = nullptr;
    }
    if (mRenderer)
    {
        if (mResolveReady)
            mResolve.DeInit(mRenderer);
        mResolveReady = false;
        DestroyRenderTarget();
        mRenderer->Deinitialize();
        delete mRenderer;
        mRenderer = nullptr;
    }
    if (mSettingsReady)
        mSettings.End();
    if (mTimerReady)
        mTimer.End();
    if (mLogReady)
        mLog.End();
    mSettingsReady = false;
    mTimerReady = false;
    mLogReady = false;
    mSuspended = false;
}

} // namespace ExePlayer
