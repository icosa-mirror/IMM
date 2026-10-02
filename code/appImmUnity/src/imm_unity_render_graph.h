#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "imm_unity_d3d12_host.h"
#include "appImmShared/src/imm_engine_bridge.h"

// ABI v1, Windows x64. Caller owns this immutable request until completed becomes
// 1 (read with acquire semantics). Only result/completed are written by native code.
// Targets must be bound before the event; render requests are flat-camera only.
// Matrices use the native row-major array convention. GPU completion is a renderer
// fence token, not permission for the caller to signal Unity or renderer fences.
struct alignas(8) ImmRenderGraphPacket
{
    uint32_t version = 1, size = sizeof(ImmRenderGraphPacket);
    uint32_t operation = 0, reserved = 0; // 0 initialize, 1 render, 2 shutdown
    uint64_t sequence = 0;
    int32_t camera = 0, colorSpace = 0, samples = 8, enableSound = 1;
    uint64_t colorBuffer = 0, depthBuffer = 0;
    uint32_t colorFormat = 0, depthFormat = 0;
    int32_t x = 0, y = 0, width = 0, height = 0;
    float worldToView[16] = {}, projection[16] = {};
    uint64_t gpuCompletion = 0;
    int32_t result = 0, completed = 0;
};
static_assert(sizeof(ImmRenderGraphPacket) == 224, "RenderGraph packet ABI size");
static_assert(offsetof(ImmRenderGraphPacket, worldToView) == 80, "RenderGraph matrix ABI offset");
static_assert(offsetof(ImmRenderGraphPacket, completed) == 220, "RenderGraph completion ABI offset");
constexpr int ImmRenderGraphEventId = 0x494d4d;

// Declare before the bridge so a borrowed renderer outlives bridge destruction.
struct ImmRenderGraphState
{
    ImmUnityD3D12Host host;
    IUnityGraphicsD3D12v7* unity = nullptr;
    bool configured = false, ready = false;
};
inline bool ConfigureImmRenderGraph(ImmRenderGraphState& state, IUnityGraphicsD3D12v7* unity)
{
    if (state.configured) return state.unity == unity;
    state.configured = state.host.Configure(unity, ImmRenderGraphEventId);
    if (state.configured) state.unity = unity;
    return state.configured;
}
inline void ShutdownImmRenderGraph(ImmRenderGraphState& state, ImmShared::ImmEngineBridge& bridge)
{
    if (state.ready) bridge.Shutdown();
    state.host.ShutdownInRenderEvent();
    state.ready = state.configured = false;
}
inline HRESULT ProcessImmRenderGraph(ImmRenderGraphState& state, ImmShared::ImmEngineBridge& bridge, ImmRenderGraphPacket& packet)
{
    if (packet.version != 1 || packet.size != sizeof(packet) || packet.reserved || packet.completed || packet.operation > 3)
        return E_INVALIDARG;
    if (packet.operation == 2) { ShutdownImmRenderGraph(state, bridge); return S_OK; }
    if (packet.operation == 0)
    {
        if (state.ready || bridge.IsInitialized() || packet.colorSpace < 0 || packet.colorSpace > 1 ||
            packet.samples != 8 || packet.enableSound < 0 || packet.enableSound > 1) return E_INVALIDARG;
        if (!ConfigureImmRenderGraph(state, state.unity) || !state.host.InitializeInRenderEvent()) return E_FAIL;
        ImmShared::ImmEngineBridge::InitConfig config = {};
        config.externalRenderer = &state.host.RendererInRenderEvent();
        config.rendererApi = ImmCore::piRenderer::API::DX12;
        config.colorSpace = packet.colorSpace; config.antialiasing = packet.samples;
        config.enableSound = packet.enableSound != 0;
        config.logFileName = "imm-render-graph.log";
        if (!bridge.Init(config)) { bridge.Shutdown(); ShutdownImmRenderGraph(state, bridge); return E_FAIL; }
        state.ready = true;
        return S_OK;
    }
    if (!state.ready) return E_UNEXPECTED;
    if (packet.operation == 3)
    {
        auto& renderer = state.host.RendererInRenderEvent();
        HRESULT result = renderer.BeginFrame();
        if (FAILED(result)) return result;
        bridge.GetPlayer()->MaintainGPU();
        return renderer.EndFrame(&packet.gpuCompletion);
    }
    if (packet.camera < 0 || packet.camera >= ImmShared::ImmEngineBridge::kMaxCameras ||
        packet.width <= 0 || packet.height <= 0 || packet.x < 0 || packet.y < 0 ||
        !packet.colorBuffer || !packet.depthBuffer) return E_INVALIDARG;
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(packet.worldToView[i]) || !std::isfinite(packet.projection[i])) return E_INVALIDARG;
    auto& renderer = state.host.RendererInRenderEvent();
    HRESULT result = renderer.BeginFrame();
    if (FAILED(result)) return result;
    result = state.host.BindTargetsInRenderEvent(reinterpret_cast<UnityRenderBuffer>(packet.colorBuffer),
        reinterpret_cast<UnityRenderBuffer>(packet.depthBuffer), static_cast<DXGI_FORMAT>(packet.colorFormat),
        static_cast<DXGI_FORMAT>(packet.depthFormat));
    if (FAILED(result)) { renderer.CancelFrame(); return result; }
    const ImmCore::mat4x4 view(packet.worldToView), projection(packet.projection);
    bridge.SetCameraMatrices(packet.camera, 0, &view, &projection, nullptr, nullptr, nullptr, nullptr);
    ImmShared::ImmEngineBridge::ViewportInfo viewport = {};
    viewport.x = static_cast<float>(packet.x); viewport.y = static_cast<float>(packet.y);
    viewport.width = static_cast<float>(packet.width); viewport.height = static_cast<float>(packet.height);
    viewport.forceViewport = true;
    if (!bridge.RenderCamera(packet.camera, viewport, 0, true)) { renderer.CancelFrame(); return E_FAIL; }
    return renderer.EndFrame(&packet.gpuCompletion);
}
