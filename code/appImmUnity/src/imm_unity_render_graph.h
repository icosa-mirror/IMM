#pragma once
#include "imm_unity_render_graph_packet.h"
#include <cmath>
#include "imm_unity_d3d12_host.h"
#include "appImmShared/src/imm_engine_bridge.h"

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
    if (packet.version != 2 || packet.size != sizeof(packet) || (packet.viewCount != 1 && packet.viewCount != 2) || packet.completed || packet.operation > 3)
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
    if (packet.viewCount == 2)
        for (int i = 0; i < 16; ++i)
            if (!std::isfinite(packet.leftView[i]) || !std::isfinite(packet.leftProjection[i]) ||
                !std::isfinite(packet.rightView[i]) || !std::isfinite(packet.rightProjection[i])) return E_INVALIDARG;
    auto& renderer = state.host.RendererInRenderEvent();
    HRESULT result = renderer.BeginFrame();
    if (FAILED(result)) return result;
    result = state.host.BindTargetsInRenderEvent(reinterpret_cast<UnityRenderBuffer>(packet.colorBuffer),
        reinterpret_cast<UnityRenderBuffer>(packet.depthBuffer), static_cast<DXGI_FORMAT>(packet.colorFormat),
        static_cast<DXGI_FORMAT>(packet.depthFormat), packet.viewCount);
    if (FAILED(result)) { renderer.CancelFrame(); return result; }
    const ImmCore::mat4x4 view(packet.worldToView), projection(packet.projection);
    if (packet.viewCount == 2)
    {
        const ImmCore::mat4x4 leftView(packet.leftView), leftProjection(packet.leftProjection);
        const ImmCore::mat4x4 rightView(packet.rightView), rightProjection(packet.rightProjection);
        bridge.SetCameraMatrices(packet.camera, 2, &view, &projection, &leftView, &leftProjection, &rightView, &rightProjection);
    }
    else bridge.SetCameraMatrices(packet.camera, 0, &view, &projection, nullptr, nullptr, nullptr, nullptr);
    ImmShared::ImmEngineBridge::ViewportInfo viewport = {};
    viewport.x = static_cast<float>(packet.x); viewport.y = static_cast<float>(packet.y);
    viewport.width = static_cast<float>(packet.width); viewport.height = static_cast<float>(packet.height);
    viewport.forceViewport = true;
    if (!bridge.RenderCamera(packet.camera, viewport, 0, true)) { renderer.CancelFrame(); return E_FAIL; }
    return renderer.EndFrame(&packet.gpuCompletion);
}
