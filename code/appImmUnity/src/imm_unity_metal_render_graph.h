#pragma once
#include <cmath>
#include <TargetConditionals.h>
#include "imm_unity_render_graph_packet.h"
#include "IUnityGraphicsMetal.h"
#include "appImmShared/src/imm_engine_bridge.h"
#include "libImmCore/src/libRender/metal/piMetal_Renderer.h"

// Objective-C++ adapter. Unity owns the device, attachments and command buffer.
struct ImmMetalRenderGraphState
{
    IUnityGraphicsMetalV2* unity = nullptr;
    bool ready = false;
};

inline void ShutdownImmMetalRenderGraph(ImmMetalRenderGraphState& state, ImmShared::ImmEngineBridge& bridge)
{
    if (state.ready)
    {
        static_cast<ImmCore::piRendererMetal*>(bridge.GetRenderer())->EndNativeFrame();
        bridge.Shutdown();
    }
    state.ready = false;
}

// Packet format identifiers retain ABI v2's DXGI numeric values; each backend
// translates them to its own API rather than exposing an MTL enum to managed code.
inline MTLPixelFormat ImmMetalPacketFormat(uint32_t format)
{
    switch (format)
    {
        case 28: return MTLPixelFormatRGBA8Unorm;
        case 29: return MTLPixelFormatRGBA8Unorm_sRGB;
        case 87: return MTLPixelFormatBGRA8Unorm;
        case 91: return MTLPixelFormatBGRA8Unorm_sRGB;
        case 10: return MTLPixelFormatRGBA16Float;
        case 26: return MTLPixelFormatRG11B10Float;
        case 55: return MTLPixelFormatDepth16Unorm;
        case 40: return MTLPixelFormatDepth32Float;
        case 20: return MTLPixelFormatDepth32Float_Stencil8;
#if TARGET_OS_OSX
        case 45: return MTLPixelFormatDepth24Unorm_Stencil8;
#endif
        default: return MTLPixelFormatInvalid;
    }
}

inline int32_t ProcessImmMetalRenderGraph(ImmMetalRenderGraphState& state,
    ImmShared::ImmEngineBridge& bridge, ImmRenderGraphPacket& packet)
{
    constexpr int32_t invalid = -2147024809; // E_INVALIDARG, shared managed error convention.
    constexpr int32_t failed = -2147467259; // E_FAIL.
    if (packet.version != 2 || packet.size != sizeof(packet) || packet.completed ||
        packet.operation > 3 || packet.viewCount != 1) return invalid;
    if (packet.operation == 2) { ShutdownImmMetalRenderGraph(state, bridge); return 0; }
    auto* unity = state.unity;
    if (!unity || !unity->MetalDevice || !unity->CurrentCommandBuffer ||
        !unity->CurrentRenderPassDescriptor || !unity->EndCurrentCommandEncoder ||
        !unity->TextureFromRenderBuffer) return failed;
    if (packet.operation == 0)
    {
        if (state.ready || bridge.IsInitialized() || packet.colorSpace < 0 || packet.colorSpace > 1 ||
            packet.samples != 8 || packet.enableSound < 0 || packet.enableSound > 1) return invalid;
        ImmShared::ImmEngineBridge::InitConfig config = {};
        config.rendererApi = ImmCore::piRenderer::API::Metal;
        config.graphicsDevice = unity->MetalDevice();
        config.colorSpace = packet.colorSpace; config.antialiasing = packet.samples;
        config.enableSound = packet.enableSound != 0;
        config.metalUnityProjectionAdjusted = true;
        config.reverseDepthBuffer = true;
        config.logFileName = "imm-render-graph.log";
        if (!config.graphicsDevice || !bridge.Init(config)) { bridge.Shutdown(); return failed; }
        state.ready = true;
        return 0;
    }
    if (!state.ready) return failed;
    if (packet.operation == 3) { bridge.GetPlayer()->MaintainGPU(); return 0; }
    if (packet.camera < 0 || packet.camera >= ImmShared::ImmEngineBridge::kMaxCameras ||
        packet.width <= 0 || packet.height <= 0 || packet.x != 0 || packet.y != 0 ||
        !packet.colorBuffer || !packet.depthBuffer) return invalid;
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(packet.worldToView[i]) || !std::isfinite(packet.projection[i])) return invalid;

    @autoreleasepool
    {
        auto colorBuffer = reinterpret_cast<UnityRenderBuffer>(packet.colorBuffer);
        auto depthBuffer = reinterpret_cast<UnityRenderBuffer>(packet.depthBuffer);
        id<MTLTexture> color = (__bridge id<MTLTexture>)unity->TextureFromRenderBuffer(colorBuffer);
        id<MTLTexture> depth = (__bridge id<MTLTexture>)unity->TextureFromRenderBuffer(depthBuffer);
        MTLRenderPassDescriptor* active = (__bridge MTLRenderPassDescriptor*)unity->CurrentRenderPassDescriptor();
        if (!color || !depth || !active || active.colorAttachments[0].texture != color ||
            active.depthAttachment.texture != depth || active.colorAttachments[1].texture ||
            active.colorAttachments[0].level || active.depthAttachment.level ||
            color.width != depth.width || color.height != depth.height ||
            color.width != static_cast<NSUInteger>(packet.width) || color.height != static_cast<NSUInteger>(packet.height) ||
            color.sampleCount != depth.sampleCount || color.arrayLength != 1 || depth.arrayLength != 1 ||
            color.pixelFormat != ImmMetalPacketFormat(packet.colorFormat) ||
            depth.pixelFormat != ImmMetalPacketFormat(packet.depthFormat) ||
            color.storageMode == MTLStorageModeMemoryless || depth.storageMode == MTLStorageModeMemoryless)
            return invalid;
        if (color.sampleCount != 1 && color.sampleCount != 2 && color.sampleCount != 4 && color.sampleCount != 8)
            return invalid;
        const auto expectedType = color.sampleCount > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
        if (color.textureType != expectedType || depth.textureType != expectedType) return invalid;

        auto* renderer = static_cast<ImmCore::piRendererMetal*>(bridge.GetRenderer());
        const ImmCore::mat4x4 view(packet.worldToView), projection(packet.projection);
        bridge.SetCameraMatrices(packet.camera, 0, &view, &projection, nullptr, nullptr, nullptr, nullptr);
        if (!bridge.PrepareCamera(packet.camera)) return failed;
        // A fresh descriptor avoids replaying Unity's original clear/discard actions.
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = color;
        pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.depthAttachment.texture = depth;
        pass.depthAttachment.loadAction = MTLLoadActionLoad;
        pass.depthAttachment.storeAction = MTLStoreActionStore;
        if (active.stencilAttachment.texture)
        {
            pass.stencilAttachment.texture = active.stencilAttachment.texture;
            pass.stencilAttachment.loadAction = MTLLoadActionLoad;
            pass.stencilAttachment.storeAction = MTLStoreActionStore;
        }
        // Unity resumes its own encoder after this event and retains responsibility
        // for MSAA resolve and command-buffer commit. We preserve multisample storage.
        unity->EndCurrentCommandEncoder();
        void* commands = unity->CurrentCommandBuffer();
        if (!commands || !renderer->BeginExternalRenderPassFrame(commands, (__bridge void*)pass, packet.width, packet.height))
            return failed;
        ImmShared::ImmEngineBridge::ViewportInfo viewport = {};
        viewport.width = static_cast<float>(packet.width); viewport.height = static_cast<float>(packet.height);
        viewport.forceViewport = true;
        const bool rendered = bridge.RenderPreparedCamera(packet.camera, viewport, 0, true);
        renderer->EndNativeFrame();
        return rendered ? 0 : failed;
    }
}
