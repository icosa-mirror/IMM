#include "appImmShared/src/imm_engine_bridge.h"
#include "imm_godot_plugin.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#if defined(__APPLE__)
#include "libImmCore/src/libRender/metal/piMetal_Renderer.h"
#endif
#if defined(_WIN32) || defined(ANDROID)
#include "libImmCore/src/libRender/vulkan/piVulkan_Renderer.h"
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>

using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

namespace
{
    ImmShared::ImmEngineBridge gBridge;
    bool gDebugLogging = false;
    ImmGodotRenderAdapter gRenderAdapter = {};

    bool iEnvFlagEnabled(const char *name)
    {
        const char *value = std::getenv(name);
        return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
    }

    bool iDebugLoggingEnabled()
    {
        return gDebugLogging || iEnvFlagEnabled("IMM_GODOT_DEBUG");
    }

    bool iIsReasonableBound3(const bound3 &b)
    {
        const float limit = 1.0e6f;
        if (b.mMinX > b.mMaxX || b.mMinY > b.mMaxY || b.mMinZ > b.mMaxZ)
            return false;
        if (b.mMinX < -limit || b.mMinX > limit)
            return false;
        if (b.mMaxX < -limit || b.mMaxX > limit)
            return false;
        if (b.mMinY < -limit || b.mMinY > limit)
            return false;
        if (b.mMaxY < -limit || b.mMaxY > limit)
            return false;
        if (b.mMinZ < -limit || b.mMinZ > limit)
            return false;
        if (b.mMaxZ < -limit || b.mMaxZ > limit)
            return false;
        return true;
    }

    void iCopyBound3(ImmGodotBounds3 *dst, const bound3 &src)
    {
        dst->minX = src.mMinX;
        dst->maxX = src.mMaxX;
        dst->minY = src.mMinY;
        dst->maxY = src.mMaxY;
        dst->minZ = src.mMinZ;
        dst->maxZ = src.mMaxZ;
    }

    void iCopyWideToUtf8Buffer(char *dst, size_t dstCount, const wchar_t *src)
    {
        if (dst == nullptr || dstCount == 0)
            return;

        dst[0] = '\0';
        char *utf8 = piws2str(src);
        if (utf8 == nullptr)
            return;

        std::snprintf(dst, dstCount, "%s", utf8);
        std::free(utf8);
    }

    mat4x4 iHostToPilibs(const float *m)
    {
        return mat4x4(m[0], m[4], m[8], m[12],
                      m[1], m[5], m[9], m[13],
                      m[2], m[6], m[10], m[14],
                      m[3], m[7], m[11], m[15]);
    }

    Player &iPlayer()
    {
        return *gBridge.GetPlayer();
    }

    piRenderer::API iResolveRendererApi(int requestedApi)
    {
        switch (requestedApi)
        {
        case ImmGodotRendererApi_OpenGL:
            return piRenderer::API::GL;
        case ImmGodotRendererApi_Direct3D:
            return piRenderer::API::DX;
        case ImmGodotRendererApi_GLES:
            return piRenderer::API::GLES;
        case ImmGodotRendererApi_Metal:
            return piRenderer::API::Metal;
        case ImmGodotRendererApi_Vulkan:
            return piRenderer::API::Vulkan;
        case ImmGodotRendererApi_Auto:
        default:
#if defined(__APPLE__)
            return piRenderer::API::Metal;
#elif defined(_WIN32) || defined(ANDROID)
            return piRenderer::API::Vulkan;
#else
            return piRenderer::API::GL;
#endif
        }
    }

}

extern "C" IMMGODOT_EXPORT int ImmGodot_Init(int colorSpace,
                                             int antialiasing,
                                             char *logFileName,
                                             char *tmpFolderName)
{
    return ImmGodot_InitEx(colorSpace,
                           antialiasing,
                           logFileName,
                           tmpFolderName,
                           ImmGodotRendererApi_OpenGL);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_InitEx(int colorSpace,
                                               int antialiasing,
                                               char *logFileName,
                                               char *tmpFolderName,
                                               int rendererApi)
{
    gDebugLogging = gDebugLogging || iEnvFlagEnabled("IMM_GODOT_DEBUG");
    if (gBridge.IsInitialized())
        return 0;

    ImmShared::ImmEngineBridge::InitConfig config = {};
    config.colorSpace = colorSpace;
    config.antialiasing = antialiasing;
    config.logFileName = logFileName;
    config.tmpFolderName = tmpFolderName;
    config.rendererApi = iResolveRendererApi(rendererApi);
    config.initializeRendererOnInit = true;
    config.initializeFullscreen = true;
    if (config.rendererApi == piRenderer::API::Vulkan)
    {
        config.initializeRendererOnInit = false;
        config.initializeFullscreen = false;
    }
    const int result = gBridge.Init(config) ? 0 : -1;
    if (result == 0 && iDebugLoggingEnabled())
    {
        gBridge.GetLog()->Printf(LT_MESSAGE,
                                  L"ImmGodot_InitEx colorSpace=%d antialiasing=%d requestedRenderer=%d resolvedRenderer=%d",
                                  colorSpace,
                                  antialiasing,
                                  rendererApi,
                                  static_cast<int>(config.rendererApi));
    }
    if (result == 0 && gRenderAdapter.onGraphicsInitialized != nullptr)
    {
        gRenderAdapter.onGraphicsInitialized(gRenderAdapter.userData);
    }
    return result;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Shutdown()
{
    if (gBridge.IsInitialized() && gRenderAdapter.onGraphicsShutdown != nullptr)
    {
        gRenderAdapter.onGraphicsShutdown(gRenderAdapter.userData);
    }
    if (gBridge.IsInitialized() && iDebugLoggingEnabled())
    {
        gBridge.GetLog()->Printf(LT_MESSAGE, L"ImmGodot_Shutdown");
    }
    gBridge.Shutdown();
}

extern "C" IMMGODOT_EXPORT int ImmGodot_IsInitialized()
{
    return gBridge.IsInitialized() ? 1 : 0;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetDebugLogging(int enabled)
{
    gDebugLogging = enabled != 0;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetRenderAdapter(const ImmGodotRenderAdapter *adapter)
{
    if (adapter == nullptr || adapter->version != 1)
    {
        gRenderAdapter = {};
        return;
    }

    gRenderAdapter = *adapter;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_BeginMetalFrame(const ImmGodotMetalFrame *frame)
{
    if (frame == nullptr || frame->version != 1 || frame->width <= 0 || frame->height <= 0)
        return -1;
    if (!gBridge.IsInitialized() || gBridge.GetRenderer() == nullptr || gBridge.GetRenderer()->GetAPI() != piRenderer::API::Metal)
        return -1;

#if defined(__APPLE__)
    piRendererMetal *renderer = static_cast<piRendererMetal *>(gBridge.GetRenderer());
    bool began = false;
    switch (frame->mode)
    {
    case ImmGodotMetalFrameMode_CommandEncoder:
        began = renderer->BeginExternalCommandEncoderFrame(frame->commandBuffer,
                                                           frame->commandEncoder,
                                                           frame->renderPassDescriptor,
                                                           frame->width,
                                                           frame->height);
        break;
    case ImmGodotMetalFrameMode_CommandBufferRenderPass:
        began = renderer->BeginExternalRenderPassFrame(frame->commandBuffer,
                                                       frame->renderPassDescriptor,
                                                       frame->width,
                                                       frame->height);
        break;
    case ImmGodotMetalFrameMode_CommandQueueRenderPass:
        began = renderer->BeginExternalCommandQueueRenderPassFrame(frame->commandQueue,
                                                                   frame->renderPassDescriptor,
                                                                   frame->width,
                                                                   frame->height);
        break;
    default:
        return -1;
    }
    return began ? 0 : -1;
#else
    return -1;
#endif
}

extern "C" IMMGODOT_EXPORT void ImmGodot_EndMetalFrame()
{
    if (!gBridge.IsInitialized() || gBridge.GetRenderer() == nullptr || gBridge.GetRenderer()->GetAPI() != piRenderer::API::Metal)
        return;

#if defined(__APPLE__)
    piRendererMetal *renderer = static_cast<piRendererMetal *>(gBridge.GetRenderer());
    renderer->EndNativeFrame();
#endif
}

extern "C" IMMGODOT_EXPORT int ImmGodot_BeginVulkanFrame(const ImmGodotVulkanFrame *frame)
{
    if (frame == nullptr || (frame->version != 1 && frame->version != 2) || frame->width <= 0 || frame->height <= 0)
        return -1;
    if (!gBridge.IsInitialized() || gBridge.GetRenderer() == nullptr || gBridge.GetRenderer()->GetAPI() != piRenderer::API::Vulkan)
        return -1;

#if defined(_WIN32) || defined(ANDROID)
    piVulkanExternalDevice externalDevice = {};
    externalDevice.instance = frame->instance;
    externalDevice.physicalDevice = frame->physicalDevice;
    externalDevice.device = frame->device;
    externalDevice.graphicsQueue = frame->graphicsQueue;
    externalDevice.graphicsQueueFamilyIndex = frame->graphicsQueueFamilyIndex;
    if (externalDevice.instance == nullptr ||
        externalDevice.physicalDevice == nullptr ||
        externalDevice.device == nullptr ||
        externalDevice.graphicsQueue == nullptr)
    {
        return -1;
    }

    if (!gBridge.CompleteGraphicsInitialization(&externalDevice))
    {
        return -1;
    }
    piRendererVulkan *renderer = static_cast<piRendererVulkan *>(gBridge.GetRenderer());
    const bool clearExternalDepth = frame->version >= 2 && (frame->flags & ImmGodotVulkanFrameFlag_ClearExternalDepth) != 0;
    bool began = false;
    if (frame->depthImage != nullptr && frame->depthImageView != nullptr && frame->depthFormat != 0)
    {
        began = renderer->BeginExternalImageFrame(frame->colorImage,
                                                  frame->colorImageView,
                                                  frame->colorFormat,
                                                  frame->depthImage,
                                                  frame->depthImageView,
                                                  frame->depthFormat,
                                                  frame->width,
                                                  frame->height,
                                                  clearExternalDepth);
    }
    else
    {
        began = renderer->BeginExternalImageFrame(frame->colorImage,
                                                 frame->colorImageView,
                                                 frame->colorFormat,
                                                 frame->width,
                                                 frame->height);
    }

    return began ? 0 : -1;
#else
    return -1;
#endif
}

extern "C" IMMGODOT_EXPORT void ImmGodot_EndVulkanFrame()
{
#if defined(_WIN32) || defined(ANDROID)
    if (gBridge.IsInitialized() && gBridge.GetRenderer() != nullptr && gBridge.GetRenderer()->GetAPI() == piRenderer::API::Vulkan)
    {
        static_cast<piRendererVulkan *>(gBridge.GetRenderer())->EndExternalImageFrame();
    }
#endif
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GlobalWork(int enabled)
{
    gBridge.GlobalWork(enabled == 1, 9000);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetCameraMatrices(int cameraID,
                                                           int stereoType,
                                                           float *world2head,
                                                           float *prjHead,
                                                           float *world2leftEye,
                                                           float *prjLeft,
                                                           float *world2rightEye,
                                                           float *prjRight)
{
    if (iDebugLoggingEnabled())
    {
        if (gBridge.IsInitialized() && gBridge.GetLog() != nullptr)
        {
            gBridge.GetLog()->Printf(LT_MESSAGE, L"ImmGodot_SetCameraMatrices camera=%d stereo=%d hasHead=%d hasProjection=%d",
                                     cameraID,
                                     stereoType,
                                     world2head != nullptr ? 1 : 0,
                                     prjHead != nullptr ? 1 : 0);
        }
    }

    const mat4x4 head = (world2head != nullptr) ? iHostToPilibs(world2head) : mat4x4();
    const mat4x4 left = (world2leftEye != nullptr) ? iHostToPilibs(world2leftEye) : mat4x4();
    const mat4x4 right = (world2rightEye != nullptr) ? iHostToPilibs(world2rightEye) : mat4x4();
    const mat4x4 headPrj = (prjHead != nullptr) ? iHostToPilibs(prjHead) : mat4x4();
    const mat4x4 leftPrj = (prjLeft != nullptr) ? iHostToPilibs(prjLeft) : mat4x4();
    const mat4x4 rightPrj = (prjRight != nullptr) ? iHostToPilibs(prjRight) : mat4x4();

    gBridge.SetCameraMatrices(cameraID,
                              stereoType,
                              (world2head != nullptr) ? &head : nullptr,
                              (prjHead != nullptr) ? &headPrj : nullptr,
                              (world2leftEye != nullptr) ? &left : nullptr,
                              (prjLeft != nullptr) ? &leftPrj : nullptr,
                              (world2rightEye != nullptr) ? &right : nullptr,
                              (prjRight != nullptr) ? &rightPrj : nullptr);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_RenderCamera(int cameraID,
                                                     int eyeID,
                                                     float viewportX,
                                                     float viewportY,
                                                     float viewportWidth,
                                                     float viewportHeight,
                                                     float minDepth,
                                                     float maxDepth)
{
    const ImmGodotViewport hostViewport = {
        viewportX,
        viewportY,
        viewportWidth,
        viewportHeight,
        minDepth,
        maxDepth
    };

    if (iDebugLoggingEnabled() && gBridge.IsInitialized() && gBridge.GetLog() != nullptr)
    {
        gBridge.GetLog()->Printf(LT_MESSAGE,
                                 L"ImmGodot_RenderCamera camera=%d eye=%d viewport=%dx%d",
                                 cameraID,
                                 eyeID,
                                 static_cast<int>(viewportWidth),
                                 static_cast<int>(viewportHeight));
    }

    if (gRenderAdapter.beforeRenderCamera != nullptr &&
        gRenderAdapter.beforeRenderCamera(gRenderAdapter.userData, cameraID, eyeID, &hostViewport) == 0)
    {
        if (gRenderAdapter.afterRenderCamera != nullptr)
        {
            gRenderAdapter.afterRenderCamera(gRenderAdapter.userData, cameraID, eyeID, &hostViewport, -1);
        }
        return -1;
    }

    const ImmShared::ImmEngineBridge::ViewportInfo viewport = {
        viewportX, viewportY, viewportWidth, viewportHeight, minDepth, maxDepth, true
    };
    const int result = gBridge.RenderCamera(cameraID, viewport, eyeID, true) ? 0 : -1;
    if (gRenderAdapter.afterRenderCamera != nullptr)
    {
        gRenderAdapter.afterRenderCamera(gRenderAdapter.userData, cameraID, eyeID, &hostViewport, result);
    }
    return result;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetRenderPerformanceInfo(ImmGodotRenderPerformanceInfo *info)
{
    if (info == nullptr || !gBridge.IsInitialized() || gBridge.GetPlayer() == nullptr)
        return -1;

    const Player::PerformanceInfo &nativeInfo = iPlayer().GetPerformanceInfoForFrame();
    info->numDrawCalls = nativeInfo.numDrawCalls;
    info->numDrawCallsCulled = nativeInfo.numDrawCallsCulled;
    info->numPaintDrawCalls = nativeInfo.numPaintDrawCalls;
    info->numPictureDrawCalls = nativeInfo.numPictureDrawCalls;
    info->numPicture2DDrawCalls = nativeInfo.numPicture2DDrawCalls;
    info->numPicture360DrawCalls = nativeInfo.numPicture360DrawCalls;
    info->numPicture360EquirectDrawCalls = nativeInfo.numPicture360EquirectDrawCalls;
    info->numPicture360CubemapDrawCalls = nativeInfo.numPicture360CubemapDrawCalls;
    info->numModelDrawCalls = nativeInfo.numModelDrawCalls;
    info->numTriangles = nativeInfo.numTriangles;
    info->numTrianglesCulled = nativeInfo.numTrianglesCulled;
    info->validationTimeFrame = nativeInfo.validationTimeFrame;
    return 0;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_LoadFromFile(char *fileName)
{
    if (fileName == nullptr || fileName[0] == '\0')
        return -1;
    if (!gBridge.IsInitialized() || !gBridge.IsGraphicsInitialized() || gBridge.GetPlayer() == nullptr)
        return -1;
    return iPlayer().Load(pistr2ws(fileName));
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Unload(int id)
{
    iPlayer().Unload(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetDocumentToWorld(int id, float *doc2world)
{
    iPlayer().SetDocumentToWorld(id, fromMatrix(f2d(iHostToPilibs(doc2world)) * mat4x4d::flipZ()));
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetPlayerInfo(ImmGodotPlayerInfo *info)
{
    if (info == nullptr)
        return -1;

    Player::PlayerInfo nativeInfo;
    iPlayer().GetPlayerInfo(nativeInfo);
    info->backgroundRed = nativeInfo.mBackgrundColor.mRed;
    info->backgroundGreen = nativeInfo.mBackgrundColor.mGreen;
    info->backgroundBlue = nativeInfo.mBackgrundColor.mBlue;
    return 0;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetDocumentState(int id, ImmGodotDocumentState *state)
{
    if (state == nullptr)
        return -1;

    Player::DocumentState nativeState;
    iPlayer().GetDocumentState(nativeState, id);
    state->loadingState = static_cast<int>(nativeState.mLoadingState);
    state->playbackState = static_cast<int>(nativeState.mPlaybackState);
    return 0;
}

extern "C" IMMGODOT_EXPORT uint32_t ImmGodot_GetDocumentInfoEx(int id)
{
    return iPlayer().GetDocumentInfoEx(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_IsSequenceReady(int id)
{
    return iPlayer().IsSequenceReady(id) ? 1 : 0;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetBoundingBox(int id, ImmGodotBounds3 *bounds)
{
    if (bounds == nullptr || !iPlayer().IsSequenceReady(id))
        return -1;

    bound3 nativeBounds = d2f(iPlayer().GetDocumentBBox(id));
    const int layerCount = iPlayer().GetLayerCount(id);
    bound3 filtered = bound3(1.0e30f);
    for (int i = 0; i < layerCount; ++i)
    {
        Player::LayerInfo layerInfo;
        if (!iPlayer().GetLayerInfoByIndex(id, i, layerInfo))
            continue;
        if (layerInfo.hasBBox == 0)
            continue;
        if (!iIsReasonableBound3(layerInfo.bbox))
            continue;
        filtered = include(filtered, layerInfo.bbox);
    }
    if (filtered.mMinX <= filtered.mMaxX)
    {
        nativeBounds = filtered;
    }

    iCopyBound3(bounds, nativeBounds);
    return iIsReasonableBound3(nativeBounds) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetLayerCount(int id)
{
    return iPlayer().GetLayerCount(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetLayerInfoByIndex(int id, int index, ImmGodotLayerInfo *info)
{
    if (info == nullptr)
        return -1;

    *info = {};
    Player::LayerInfo nativeInfo;
    if (!iPlayer().GetLayerInfoByIndex(id, index, nativeInfo))
        return -1;

    info->id = nativeInfo.id;
    info->type = nativeInfo.type;
    info->parentId = nativeInfo.parentId;
    info->isTimeline = nativeInfo.isTimeline;
    info->isLoaded = nativeInfo.isLoaded;
    info->isVisible = nativeInfo.isVisible;
    info->opacity = nativeInfo.opacity;
    info->hasBounds = nativeInfo.hasBBox;
    iCopyBound3(&info->bounds, nativeInfo.bbox);
    info->numChildren = nativeInfo.numChildren;
    info->assetId = nativeInfo.assetId;
    info->paintNumDrawings = nativeInfo.paintNumDrawings;
    info->paintNumFrames = nativeInfo.paintNumFrames;
    info->paintNumStrokes = nativeInfo.paintNumStrokes;
    iCopyWideToUtf8Buffer(info->name, sizeof(info->name), nativeInfo.name);
    iCopyWideToUtf8Buffer(info->fullName, sizeof(info->fullName), nativeInfo.fullName);
    return 0;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_SetLayerVisible(int docId, int layerId, int visible)
{
    return iPlayer().SetLayerVisible(docId, layerId, visible != 0) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_ClearLayerVisibilityOverride(int docId, int layerId)
{
    return iPlayer().ClearLayerVisibilityOverride(docId, layerId) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_SetLayerOpacity(int docId, int layerId, float opacity)
{
    return iPlayer().SetLayerOpacity(docId, layerId, opacity) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_SetLayerTransform(int docId, int layerId, float *layerToWorld)
{
    if (layerToWorld == nullptr)
        return -1;

    const trans3d transform = fromMatrix(f2d(iHostToPilibs(layerToWorld)) * mat4x4d::flipZ());
    return iPlayer().SetLayerTransform(docId, layerId, transform) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_ClearLayerTransformOverride(int docId, int layerId)
{
    return iPlayer().ClearLayerTransformOverride(docId, layerId) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetLayerDiagnostics(int docId, int layerId, ImmGodotLayerDiagnostics *diagnostics)
{
    if (diagnostics == nullptr)
        return -1;

    *diagnostics = {};
    Player::LayerDiagnostics nativeDiagnostics;
    if (!iPlayer().GetLayerDiagnostics(docId, layerId, nativeDiagnostics))
        return -1;

    diagnostics->hasVisibilityKeys = nativeDiagnostics.hasVisibilityKeys;
    diagnostics->hasOpacityKeys = nativeDiagnostics.hasOpacityKeys;
    diagnostics->isVisible = nativeDiagnostics.isVisible;
    diagnostics->opacity = nativeDiagnostics.opacity;
    diagnostics->isWorldVisible = nativeDiagnostics.isWorldVisible;
    diagnostics->worldOpacity = nativeDiagnostics.worldOpacity;
    diagnostics->parentId = nativeDiagnostics.parentId;
    diagnostics->visibilityOverrideEnabled = nativeDiagnostics.visibilityOverrideEnabled;
    diagnostics->visibilityOverrideValue = nativeDiagnostics.visibilityOverrideValue;
    diagnostics->hasTransformKeys = nativeDiagnostics.hasTransformKeys;
    diagnostics->transformOverrideEnabled = nativeDiagnostics.transformOverrideEnabled;
    return 0;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Pause(int id)
{
    iPlayer().Pause(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Resume(int id)
{
    iPlayer().Resume(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Restart(int id)
{
    iPlayer().Restart(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Show(int id)
{
    iPlayer().Show(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Hide(int id)
{
    iPlayer().Hide(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Continue(int id)
{
    iPlayer().Continue(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SkipForward(int id)
{
    iPlayer().SkipForward(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SkipBack(int id)
{
    iPlayer().SkipBack(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetChapterCount(int id)
{
    return iPlayer().GetChapterCount(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetCurrentChapter(int id)
{
    return iPlayer().GetCurrentChapter(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetChapter(int id, int chapterIndex)
{
    iPlayer().SetChapter(id, chapterIndex);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetTime(int id, int64_t timeSinceStart, int64_t timeSinceStop)
{
    iPlayer().SetTime(id, piTick(timeSinceStart), piTick(timeSinceStop));
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetTime(int id, ImmGodotTimeInfo *timeInfo)
{
    if (timeInfo == nullptr)
        return -1;

    piTick startTime;
    piTick stopTime;
    iPlayer().GetTime(id, &startTime, &stopTime);
    timeInfo->timeSinceStart = piTick::CastInt(startTime);
    timeInfo->timeSinceStop = piTick::CastInt(stopTime);
    timeInfo->playTime = timeInfo->timeSinceStart;
    return 0;
}

extern "C" IMMGODOT_EXPORT int64_t ImmGodot_GetPlayTime(int id)
{
    ImmGodotTimeInfo timeInfo = {};
    if (ImmGodot_GetTime(id, &timeInfo) != 0)
        return 0;

    return timeInfo.playTime;
}

extern "C" IMMGODOT_EXPORT int64_t ImmGodot_SecondsToTicks(double seconds)
{
    return piTick::CastInt(piTick::FromSeconds(seconds));
}

extern "C" IMMGODOT_EXPORT double ImmGodot_TicksToSeconds(int64_t ticks)
{
    return piTick::ToSeconds(piTick(ticks));
}

extern "C" IMMGODOT_EXPORT int64_t ImmGodot_GetTicksPerSecond()
{
    return piTick::FromOneSecond();
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetVolume(int id, float volume)
{
    iPlayer().SetDocumentVolume(id, volume);
}

extern "C" IMMGODOT_EXPORT float ImmGodot_GetVolume(int id)
{
    return iPlayer().GetDocumentVolume(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaCount(int docId)
{
    return iPlayer().GetSpawnAreaCount(docId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaList(int docId, int spawnAreaIdsSize, int *spawnAreaIds)
{
    const int num = iPlayer().GetSpawnAreaCount(docId);
    piAssert(num <= spawnAreaIdsSize);
    for (int i = 0; i < num; ++i)
    {
        spawnAreaIds[i] = i;
    }
    return num;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetActiveSpawnAreaId(int docId)
{
    return iPlayer().GetSpawnArea(docId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetInitialSpawnAreaId(int docId)
{
    return iPlayer().GetInitialSpawnArea(docId);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetActiveSpawnAreaId(int docId, int spawnAreaId)
{
    iPlayer().SetSpawnArea(docId, spawnAreaId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaInfo(int docId, int spawnAreaId, ImmGodotSpawnArea *serializedSpawnArea)
{
    if (serializedSpawnArea == nullptr)
        return -1;

    *serializedSpawnArea = {};

    Document::SpawnAreaInfo spawnAreaInfo;
    if (!iPlayer().GetSpawnAreaInfo(spawnAreaInfo, docId, spawnAreaId))
        return -1;

    char *name = piws2str(spawnAreaInfo.mName);
    if (name != nullptr)
    {
        std::snprintf(serializedSpawnArea->name, sizeof(serializedSpawnArea->name), "%s", name);
        std::free(name);
    }
    serializedSpawnArea->version = spawnAreaInfo.mVersion;
    serializedSpawnArea->type = spawnAreaInfo.mIsFloorLevel ? ImmGodotSpawnArea::FloorLevel : ImmGodotSpawnArea::EyeLevel;
    serializedSpawnArea->animated = spawnAreaInfo.mAnimated;

    const trans3d transform = spawnAreaInfo.mSpawnAreaToWorld;
    serializedSpawnArea->transform.posx = static_cast<float>(transform.mTranslation.x);
    serializedSpawnArea->transform.posy = static_cast<float>(transform.mTranslation.y);
    serializedSpawnArea->transform.posz = static_cast<float>(transform.mTranslation.z);
    serializedSpawnArea->transform.rotx = static_cast<float>(transform.mRotation.x);
    serializedSpawnArea->transform.roty = static_cast<float>(transform.mRotation.y);
    serializedSpawnArea->transform.rotz = static_cast<float>(transform.mRotation.z);
    serializedSpawnArea->transform.rotw = static_cast<float>(transform.mRotation.w);
    serializedSpawnArea->transform.sca = static_cast<float>(transform.mScale);

    switch (spawnAreaInfo.mVolume.mType)
    {
    case ImmImporter::LayerSpawnArea::Volume::Type::Sphere:
    {
        serializedSpawnArea->volume.type = ImmGodotSpawnArea::Volume::Sphere;
        const vec4 sphere = spawnAreaInfo.mVolume.mShape.mSphere;
        serializedSpawnArea->volume.offset.x = sphere.x;
        serializedSpawnArea->volume.offset.y = sphere.y;
        serializedSpawnArea->volume.offset.z = sphere.z;
        serializedSpawnArea->volume.sphereExtent.r = sphere.w;
    } break;
    case ImmImporter::LayerSpawnArea::Volume::Type::Box:
    {
        serializedSpawnArea->volume.type = ImmGodotSpawnArea::Volume::Box;
        const vec3 center = getcenter(spawnAreaInfo.mVolume.mShape.mBox);
        const vec3 extent = getradiius(spawnAreaInfo.mVolume.mShape.mBox);
        serializedSpawnArea->volume.offset.x = center.x;
        serializedSpawnArea->volume.offset.y = center.y;
        serializedSpawnArea->volume.offset.z = center.z;
        serializedSpawnArea->volume.boxExtent.x = extent.x;
        serializedSpawnArea->volume.boxExtent.y = extent.y;
        serializedSpawnArea->volume.boxExtent.z = extent.z;
    } break;
    default:
        return -1;
    }

    serializedSpawnArea->locomotion =
        (((spawnAreaInfo.mVolume.mAllowTranslationX ? 1 : 0) << 2) |
         ((spawnAreaInfo.mVolume.mAllowTranslationY ? 1 : 0) << 1) |
         ((spawnAreaInfo.mVolume.mAllowTranslationZ ? 1 : 0) << 0));
    serializedSpawnArea->volume.allowTranslation.x = spawnAreaInfo.mVolume.mAllowTranslationX ? 1 : 0;
    serializedSpawnArea->volume.allowTranslation.y = spawnAreaInfo.mVolume.mAllowTranslationY ? 1 : 0;
    serializedSpawnArea->volume.allowTranslation.z = spawnAreaInfo.mVolume.mAllowTranslationZ ? 1 : 0;

    return 0;
}
