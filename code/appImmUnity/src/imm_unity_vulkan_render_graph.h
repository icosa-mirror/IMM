#pragma once
#include <cmath>
#include <cstdio>
#include <cstdarg>
#if defined(__ANDROID__)
#include <android/log.h>
#endif
#include <string>
#include "imm_unity_vulkan_render_graph_events.h"
#include "appImmShared/src/imm_engine_bridge.h"
#include "libImmCore/src/libRender/vulkan/piVulkan_Renderer.h"

inline void ImmVulkanRenderGraphDiagnostic(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
#if defined(__ANDROID__)
    __android_log_vprint(ANDROID_LOG_ERROR, "IMM_VULKAN_RENDER_GRAPH", format, arguments);
#else
    std::fputs("[IMM_VULKAN_RENDER_GRAPH] ", stderr);
    std::vfprintf(stderr, format, arguments);
    std::fputc('\n', stderr);
#endif
    va_end(arguments);
}

struct ImmVulkanRenderGraphState
{
    IUnityGraphicsVulkan* unity = nullptr;
    ImmCore::piVulkanExternalDevice device = {};
    bool ready = false;
    bool eventsConfigured = false;
    bool ownsBridge = false;
    std::string logFileName, tmpFolderName;
    ImmRenderGraphPacket* prepared = nullptr;
    int32_t preparationResult = 0;
};

inline uint32_t ImmVulkanPacketFormat(uint32_t format)
{
    switch (format)
    {
        case 28: return 37; case 29: return 43; // RGBA8
        case 87: return 44; case 91: return 50; // BGRA8
        case 10: return 97; case 26: return 122;
        case 55: return 124; case 40: return 126;
        case 45: return 129; case 20: return 130;
        default: return 0;
    }
}
inline bool ImmVulkanColorFormatCompatible(uint32_t image, uint32_t view)
{
    // Unity can use an UNORM base image with an sRGB attachment view.
    return image == view || ((image == 37 || image == 43) && (view == 37 || view == 43)) ||
        ((image == 44 || image == 50) && (view == 44 || view == 50));
}
inline void ShutdownImmVulkanRenderGraph(ImmVulkanRenderGraphState& state,
    ImmShared::ImmEngineBridge& bridge)
{
    // Called only after the dedicated shutdown event flushes Unity recording,
    // or during graphics-device teardown. Deinitialize then waits for GPU work.
    if (state.ownsBridge)
    {
        if (auto* renderer = bridge.GetRenderer())
            static_cast<ImmCore::piRendererVulkan*>(renderer)->EndExternalImageFrame();
        bridge.Shutdown();
    }
    state.ready = false;
    state.ownsBridge = false;
    if (state.prepared)
    {
        state.prepared->result = -2147467259;
        state.prepared->completed.store(1, std::memory_order_release);
        state.prepared = nullptr;
    }
}

inline int32_t ProcessImmVulkanRenderGraph(ImmVulkanRenderGraphState& state,
    ImmShared::ImmEngineBridge& bridge, ImmRenderGraphPacket& packet, int eventId, bool& acknowledge)
{
    constexpr int32_t invalid = -2147024809;
    constexpr int32_t failed = -2147467259;
    auto reject = [&](const char* reason, int32_t result) {
        ImmVulkanRenderGraphDiagnostic("Rejected %s: operation=%u event=%d camera=%d views=%u viewport=%d,%d %dx%d",
            reason, packet.operation, eventId, packet.camera, packet.viewCount,
            packet.x, packet.y, packet.width, packet.height);
        return result;
    };
    const bool preparation = eventId == ImmRenderGraphPreparationEventId && packet.operation == 1;
    const bool prepared = state.prepared == &packet;
    const int32_t preparationResult = state.preparationResult;
    if (eventId == ImmRenderGraphEventId && packet.operation == 1 && prepared)
        state.prepared = nullptr;
    // A render request stays owned until its draw event, even if preparation fails.
    acknowledge = !preparation;
    if (preparation) { state.prepared = &packet; state.preparationResult = failed; }
    auto finishPreparation = [&](int32_t result) { state.preparationResult = result; return result; };
    if (packet.version != 2 || packet.size != sizeof(packet) || packet.completed ||
        packet.operation > 3 || packet.viewCount != 1) return preparation ? finishPreparation(reject("packet header", invalid)) : reject("packet header", invalid);
    if (packet.operation == 2)
    {
        if (eventId != ImmRenderGraphShutdownEventId) return invalid;
        ShutdownImmVulkanRenderGraph(state, bridge);
        return 0;
    }
    auto* unity = state.unity;
    if (!unity || !unity->Instance || !unity->CommandRecordingState ||
        !unity->AccessRenderBufferTexture || !unity->EnsureInsideRenderPass) return failed;
    if (packet.operation == 0)
    {
        if (eventId != ImmRenderGraphPreparationEventId || state.ready || bridge.IsInitialized() ||
            packet.colorSpace < 0 || packet.colorSpace > 1 || packet.samples != 8 ||
            packet.enableSound < 0 || packet.enableSound > 1 ||
            state.logFileName.empty() || state.tmpFolderName.empty()) return invalid;
        const auto instance = unity->Instance();
        state.device.instance = instance.instance;
        state.device.physicalDevice = instance.physicalDevice;
        state.device.device = instance.device;
        state.device.graphicsQueue = instance.graphicsQueue;
        state.device.graphicsQueueFamilyIndex = instance.queueFamilyIndex;
        state.device.allowDedicatedQueue = false;
        state.device.externalDepthReverseZ = true;
        ImmShared::ImmEngineBridge::InitConfig config = {};
        config.rendererApi = ImmCore::piRenderer::API::Vulkan;
        config.graphicsDevice = &state.device;
        config.colorSpace = packet.colorSpace;
        config.antialiasing = packet.samples;
        config.enableSound = packet.enableSound != 0;
        config.reverseDepthBuffer = true;
        config.overrideFrontIsCCW = true;
        config.frontIsCCW = false;
        config.logFileName = state.logFileName.c_str();
        config.tmpFolderName = state.tmpFolderName.c_str();
        if (!instance.device) return failed;
        state.ownsBridge = true;
        if (!bridge.Init(config)) { ShutdownImmVulkanRenderGraph(state, bridge); return failed; }
        state.ready = true;
        return 0;
    }
    if (!state.ready) return failed;
    if (packet.operation == 3)
    {
        if (eventId != ImmRenderGraphPreparationEventId) return invalid;
        bridge.GetPlayer()->MaintainGPU();
        return 0;
    }
    if (packet.camera < 0 || packet.camera >= ImmShared::ImmEngineBridge::kMaxCameras ||
        packet.width <= 0 || packet.height <= 0 || packet.x != 0 || packet.y != 0 ||
        !packet.colorBuffer || !packet.depthBuffer) return preparation ? finishPreparation(reject("camera or viewport", invalid)) : reject("camera or viewport", invalid);
    if (preparation)
    {
        for (int i = 0; i < 16; ++i)
            if (!std::isfinite(packet.worldToView[i]) || !std::isfinite(packet.projection[i])) return finishPreparation(invalid);
        const ImmCore::mat4x4 view(packet.worldToView), projection(packet.projection);
        bridge.SetCameraMatrices(packet.camera, 0, &view, &projection, nullptr, nullptr, nullptr, nullptr);
        return finishPreparation(bridge.PrepareCamera(packet.camera) ? 0 : failed);
    }
    if (eventId != ImmRenderGraphEventId || !prepared) return reject("unmatched preparation", invalid);
    if (preparationResult < 0) return preparationResult;
    UnityVulkanImage color = {}, depth = {};
    if (!unity->AccessRenderBufferTexture(reinterpret_cast<UnityRenderBuffer>(packet.colorBuffer), nullptr,
            0, 0, 0, kUnityVulkanResourceAccess_ObserveOnly, &color) ||
        !unity->AccessRenderBufferTexture(reinterpret_cast<UnityRenderBuffer>(packet.depthBuffer), nullptr,
            0, 0, 0, kUnityVulkanResourceAccess_ObserveOnly, &depth)) return reject("attachment access", failed);
    const uint32_t colorFormat = ImmVulkanPacketFormat(packet.colorFormat);
    const uint32_t depthFormat = ImmVulkanPacketFormat(packet.depthFormat);
    if (!color.image || !depth.image || !colorFormat || !depthFormat ||
        !ImmVulkanColorFormatCompatible(color.format, colorFormat) || depth.format != depthFormat ||
        color.extent.width != depth.extent.width || color.extent.height != depth.extent.height ||
        color.extent.width < static_cast<uint32_t>(packet.width) || color.extent.height < static_cast<uint32_t>(packet.height) ||
        color.layers != 1 || depth.layers != 1 || color.samples != depth.samples ||
        color.samples != static_cast<uint32_t>(packet.samples))
    {
        ImmVulkanRenderGraphDiagnostic("Invalid attachments: colour=%ux%u format=%u samples=%u layers=%d depth=%ux%u format=%u samples=%u layers=%d request=%dx%d formats=%u/%u samples=%d",
            color.extent.width, color.extent.height, color.format, color.samples, color.layers,
            depth.extent.width, depth.extent.height, depth.format, depth.samples, depth.layers,
            packet.width, packet.height, colorFormat, depthFormat, packet.samples);
        return invalid;
    }
    UnityVulkanRecordingState recording = {};
    if (!unity->CommandRecordingState(&recording, kUnityVulkanGraphicsQueueAccess_DontCare)) return failed;
    if (!recording.renderPass || recording.subPassIndex < 0)
    {
        unity->EnsureInsideRenderPass();
        if (!unity->CommandRecordingState(&recording, kUnityVulkanGraphicsQueueAccess_DontCare)) return failed;
    }
    auto* renderer = static_cast<ImmCore::piRendererVulkan*>(bridge.GetRenderer());
    if (recording.subPassIndex < 0 || !renderer->BeginHostRenderPassFrame(recording.commandBuffer,
        reinterpret_cast<void*>(recording.renderPass), reinterpret_cast<void*>(recording.framebuffer),
        colorFormat, color.samples, true, true,
        static_cast<uint32_t>(recording.subPassIndex), packet.width, packet.height,
        recording.currentFrameNumber, recording.safeFrameNumber, true)) return failed;
    ImmShared::ImmEngineBridge::ViewportInfo viewport = {};
    viewport.width = static_cast<float>(packet.width);
    viewport.height = static_cast<float>(packet.height);
    viewport.forceViewport = true;
    bool rendered = false;
    try { rendered = bridge.RenderPreparedCamera(packet.camera, viewport, 0, true); }
    catch (...) { renderer->EndExternalImageFrame(); throw; }
    const bool valid = renderer->HostFrameResourcesValid();
    renderer->EndExternalImageFrame();
    return rendered && valid ? 0 : failed;
}
