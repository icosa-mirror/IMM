#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>
#include "../../src/libRender/directx12/piDX12_Renderer.h"
#include "libImmPlayer/src/player.h"
#include "appImmShared/src/imm_engine_bridge.h"
#include "appImmUnity/src/imm_unity_d3d12_host.h"
#include "appImmUnity/src/imm_unity_render_graph.h"

using Microsoft::WRL::ComPtr;
static void Check(HRESULT result, const char* label)
{
    if (FAILED(result)) throw std::runtime_error(label);
}
namespace
{
ID3D12Device* hostDevice = nullptr;
ID3D12CommandQueue* hostQueue = nullptr;
bool configured = false;
ID3D12Resource* UNITY_INTERFACE_API ResolveHostBuffer(UnityRenderBuffer buffer) { return reinterpret_cast<ID3D12Resource*>(buffer); }
ID3D12Device* UNITY_INTERFACE_API GetHostDevice() { return hostDevice; }
ID3D12CommandQueue* UNITY_INTERFACE_API GetHostQueue() { return hostQueue; }
void UNITY_INTERFACE_API ConfigureHostEvent(int eventId, const UnityD3D12PluginEventConfig* config)
{
    configured = (eventId == 731 || eventId == ImmRenderGraphEventId) && config &&
        config->graphicsQueueAccess == kUnityD3D12GraphicsQueueAccess_Allow &&
        config->flags == (kUnityD3D12EventConfigFlag_FlushCommandBuffers | kUnityD3D12EventConfigFlag_SyncWorkerThreads) &&
        !config->ensureActiveRenderTextureIsBound;
}
}

enum class DepthProbe { None, HostOcclusion, ImmWrites };
static void RenderScene(ImmUnityD3D12Host& host, ID3D12Device* device, ImmPlayer::Player& player, int document, const char* capture, DepthProbe probe = DepthProbe::None)
{
    auto& renderer = host.RendererInRenderEvent();
    constexpr UINT size = 256;
    const bool hostDepth = probe == DepthProbe::HostOcclusion;
    D3D12_HEAP_PROPERTIES gpu = {}; gpu.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = desc.Height = size;
    desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.SampleDesc.Count = 8;
    desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> color, depth, resolved, readback;
    Check(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
        nullptr, IID_PPV_ARGS(&color)), "Create scene color");
    desc.Format = DXGI_FORMAT_R32_TYPELESS; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    Check(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        nullptr, IID_PPV_ARGS(&depth)), "Create scene depth");
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.Flags = D3D12_RESOURCE_FLAG_NONE; desc.SampleDesc.Count = 1;
    Check(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RESOLVE_DEST,
        nullptr, IID_PPV_ARGS(&resolved)), "Create scene resolve");
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {}; UINT64 bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    D3D12_RESOURCE_DESC buffer = {};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width = bytes;
    buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES cpu = {}; cpu.Type = D3D12_HEAP_TYPE_READBACK;
    Check(device->CreateCommittedResource(&cpu, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&readback)), "Create scene readback");
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {}; heapDesc.NumDescriptors = 1;
    ComPtr<ID3D12DescriptorHeap> dsv;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    Check(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsv)), "Create host DSV heap");
    D3D12_DEPTH_STENCIL_VIEW_DESC hostDepthView = {};
    hostDepthView.Format = DXGI_FORMAT_D32_FLOAT; hostDepthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    device->CreateDepthStencilView(depth.Get(), &hostDepthView, dsv->GetCPUDescriptorHandleForHeapStart());
    const auto bounds = player.GetDocumentBBox(document);
    const double radius = std::max({bounds.mMaxX-bounds.mMinX, bounds.mMaxY-bounds.mMinY, bounds.mMaxZ-bounds.mMinZ, 1.0}) * 0.6;
    const auto view = ImmCore::trans3d::translate(-(bounds.mMinX+bounds.mMaxX)*0.5,
        -(bounds.mMinY+bounds.mMaxY)*0.5, -(bounds.mMinZ+bounds.mMaxZ)*0.5-radius*2.0);
    const float r = static_cast<float>(radius);
    const float nearPlane = r * 0.01f, farPlane = r * 4.0f;
    const ImmCore::mat4x4 projection(1,0,0,0, 0,1,0,0, 0,0,nearPlane/(farPlane-nearPlane),nearPlane*farPlane/(farPlane-nearPlane), 0,0,-1,0);
    Check(renderer.BeginFrame(), "Begin scene frame");
    const auto colorBuffer = reinterpret_cast<UnityRenderBuffer>(color.Get());
    const auto depthBuffer = reinterpret_cast<UnityRenderBuffer>(depth.Get());
    if (host.BindTargetsInRenderEvent(colorBuffer, depthBuffer, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT) != E_INVALIDARG)
        throw std::runtime_error("Host adapter accepted invalid target format");
    if (host.BindTargetsInRenderEvent(colorBuffer, depthBuffer, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_D32_FLOAT) != E_INVALIDARG)
        throw std::runtime_error("Host adapter accepted a typeless RTV format");
    Check(host.BindTargetsInRenderEvent(colorBuffer, depthBuffer, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT), "Bind Unity scene buffers");
    const float black[] = {0,0,0,0}; renderer.Clear(black, nullptr, nullptr, nullptr, true);
    auto* commands = static_cast<ID3D12GraphicsCommandList*>(renderer.GetContext());
    if (hostDepth)
    {
        // Simulate opaque host content in front of IMM on the left half. With
        // reversed Z, 1 is the nearest depth. The right half remains at far depth 0.
        const D3D12_RECT left = {0, 0, size/2, size};
        commands->ClearDepthStencilView(dsv->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &left);
    }
    player.SetTime(document, ImmCore::piTick::FromSeconds(3.0), ImmCore::piTick(0));
    player.GlobalWork(true, 10000);
    player.GlobalRender(ImmCore::trans3d::identity(), view, projection, ImmPlayer::StereoMode::None);
    player.RenderMono(ImmCore::ivec2(size, size), 0);
    const auto& perf = player.GetPerformanceInfoForFrame();
    std::printf("IMM_DX12_PLAYER draws=%d paint=%d triangles=%d\n", perf.numDrawCalls, perf.numPaintDrawCalls, perf.numTriangles);
    if (perf.numPaintDrawCalls <= 0 || perf.numTriangles <= 0) throw std::runtime_error("Scene submitted no paint geometry");
    if (probe == DepthProbe::ImmWrites)
    {
        const char* vs = "float4 main(float2 p : POSITION) : SV_Position { return float4(p,0,1); }";
        const char* ps = "float4 main() : SV_Target { return float4(1,1,1,1); }";
        auto shader = renderer.CreateShader(nullptr, vs, nullptr, nullptr, nullptr, ps, nullptr);
        if (!shader) throw std::runtime_error("Create host background shader");
        renderer.AttachShader(shader);
        renderer.SetState(ImmCore::piSTATE_CULL_FACE, false);
        renderer.SetState(ImmCore::piSTATE_BLEND, false);
        renderer.DrawUnitQuad_XY(1);
        renderer.DestroyShader(shader);
    }
    auto transition = [&](ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier = {}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
        commands->ResourceBarrier(1, &barrier);
    };
    transition(color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    commands->ResolveSubresource(resolved.Get(), 0, color.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    transition(color.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    transition(resolved.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION source = {}; source.pResource = resolved.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION destination = {}; destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = footprint;
    commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    uint64_t completion = 0; Check(renderer.EndFrame(&completion), "Submit scene frame");
    Check(renderer.WaitForFrame(completion), "Wait for scene frame");
    void* data = nullptr; D3D12_RANGE range = {0, static_cast<SIZE_T>(bytes)};
    Check(readback->Map(0, &range, &data), "Map scene pixels");
    UINT visible = 0, occludedPixels = 0, retainedScenePixels = 0, backgroundPixels = 0;
    std::ofstream image(capture, std::ios::binary);
    image << "P6\n256 256\n255\n";
    for (UINT y = 0; y < size; ++y) for (UINT x = 0; x < size; ++x)
    {
        const auto* pixel = static_cast<const unsigned char*>(data) + footprint.Offset + y*footprint.Footprint.RowPitch + x*4;
        if (pixel[0] || pixel[1] || pixel[2])
        {
            ++visible;
            if (x < size/2) ++occludedPixels;
        }
        if (pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255) ++backgroundPixels;
        else if (pixel[0] || pixel[1] || pixel[2]) ++retainedScenePixels;
        image.write(reinterpret_cast<const char*>(pixel), 3);
    }
    D3D12_RANGE written = {}; readback->Unmap(0, &written);
    image.close();
    if (!image || visible < 100) throw std::runtime_error("Scene readback is empty or capture failed");
    if (hostDepth && occludedPixels != 0) throw std::runtime_error("IMM overwrote host-occluded pixels");
    if (probe == DepthProbe::ImmWrites && (retainedScenePixels < 100 || backgroundPixels < 100))
        throw std::runtime_error("IMM depth failed to preserve scene in front of later host geometry");
    std::printf("IMM_DX12_PLAYER scene pixels=%u depth_probe=%d\n", visible, int(probe));
}

int main()
{
    try
    {
        ComPtr<ID3D12Debug> debug;
        Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "Get debug layer");
        debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Create factory");
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "Get WARP");
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "Create device");
        D3D12_COMMAND_QUEUE_DESC description = {};
        Check(device->CreateCommandQueue(&description, IID_PPV_ARGS(&queue)), "Create queue");
        hostDevice = device.Get(); hostQueue = queue.Get();
        IUnityGraphicsD3D12v7 unity = {};
        unity.TextureFromRenderBuffer = ResolveHostBuffer;
        unity.GetDevice = GetHostDevice; unity.GetCommandQueue = GetHostQueue; unity.ConfigureEvent = ConfigureHostEvent;
        ImmUnityD3D12Host host;
        if (host.Configure(nullptr, 731) || !host.Configure(&unity, 731) || !configured ||
            host.Configure(&unity, 731) || !host.InitializeInRenderEvent())
            throw std::runtime_error("Configure Unity D3D12 host adapter");
        auto& renderer = host.RendererInRenderEvent();
        ImmCore::piLog log;
        ImmCore::piTimer timer;
        if (!log.Init(L"d3d12-player.log", PILOG_TXT) || !timer.Init()) throw std::runtime_error("Initialize log/timer");
        for (const auto technique : {ImmImporter::Drawing::Static, ImmImporter::Drawing::Pretessellated})
        for (const auto colorSpace : {ImmImporter::Drawing::ColorSpace::Linear, ImmImporter::Drawing::ColorSpace::Gamma})
        {
            ImmPlayer::Player player;
            ImmPlayer::Player::Configuration configuration = {};
            configuration.paintRenderingTechnique = technique;
            configuration.colorSpace = colorSpace;
            configuration.multisamplingLevel = 8;
            configuration.depthBuffer = ImmPlayer::DepthBuffer::Linear10;
            configuration.clipDepth = configuration.projectionMatrix = ImmPlayer::ClipSpaceDepth::FromZeroToOne;
            configuration.frontIsCCW = true;
            if (!player.Init(&renderer, nullptr, &log, &timer, &configuration))
                throw std::runtime_error("Player initialization failed; see d3d12-player.log");
            const int document = player.Load(IMM_D3D12_SAMPLE_FILE);
            if (document < 0) throw std::runtime_error("Queue sample document load");
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            for (;;)
            {
                player.GlobalRender(ImmCore::trans3d::identity(), ImmCore::trans3d::identity(),
                    ImmCore::mat4x4::identity(), ImmPlayer::StereoMode::None);
                player.GlobalWork(true, 10000);
                ImmPlayer::Player::DocumentState state;
                player.GetDocumentState(state, document);
                if (state.mLoadingState == ImmPlayer::Player::LoadingState::Loaded) break;
                if (state.mLoadingState == ImmPlayer::Player::LoadingState::Failed ||
                    std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("Load sample document failed or timed out");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (player.GetLayerCount(document) <= 0) throw std::runtime_error("Loaded sample has no layers");
            const char* captures[2][2] = {{"d3d12-player-scene-static-linear.ppm", "d3d12-player-scene-static-gamma.ppm"},
                {"d3d12-player-scene-pretessellated-linear.ppm", "d3d12-player-scene-pretessellated-gamma.ppm"}};
            RenderScene(host, device.Get(), player, document, captures[int(technique)][int(colorSpace)]);
            const char* depthCaptures[2][2] = {{"d3d12-player-depth-static-linear.ppm", "d3d12-player-depth-static-gamma.ppm"},
                {"d3d12-player-depth-pretessellated-linear.ppm", "d3d12-player-depth-pretessellated-gamma.ppm"}};
            RenderScene(host, device.Get(), player, document, depthCaptures[int(technique)][int(colorSpace)], DepthProbe::HostOcclusion);
            const char* writeCaptures[2][2] = {{"d3d12-player-depth-write-static-linear.ppm", "d3d12-player-depth-write-static-gamma.ppm"},
                {"d3d12-player-depth-write-pretessellated-linear.ppm", "d3d12-player-depth-write-pretessellated-gamma.ppm"}};
            RenderScene(host, device.Get(), player, document, writeCaptures[int(technique)][int(colorSpace)], DepthProbe::ImmWrites);
            player.UnloadAllSync();
            player.Deinit();
        }
        ImmShared::ImmEngineBridge bridge;
        ImmShared::ImmEngineBridge::InitConfig bridgeConfig = {};
        bridgeConfig.externalRenderer = &renderer;
        bridgeConfig.rendererApi = ImmCore::piRenderer::API::DX12;
        bridgeConfig.antialiasing = 8;
        bridgeConfig.enableSound = false;
        bridgeConfig.initializeRendererOnInit = false;
        bridgeConfig.logFileName = "d3d12-bridge.log";
        if (!bridge.Init(bridgeConfig) || bridge.IsGraphicsInitialized() ||
            bridge.GetRenderer() != &renderer || !bridge.CompleteGraphicsInitialization())
            throw std::runtime_error("Initialize bridge with borrowed Unity renderer");
        bridge.Shutdown();
        // Bridge shutdown must leave the host renderer initialized and usable.
        Check(renderer.BeginFrame(), "Borrowed renderer survived bridge shutdown");
        uint64_t bridgeCompletion = 0;
        Check(renderer.EndFrame(&bridgeCompletion), "Submit after bridge shutdown");
        Check(renderer.WaitForFrame(bridgeCompletion), "Wait after bridge shutdown");
        host.ShutdownInRenderEvent();
        ImmRenderGraphState graph;
        if (!ConfigureImmRenderGraph(graph, &unity)) throw std::runtime_error("Configure graph event");
        ImmRenderGraphPacket packet;
        packet.enableSound = 0;
        packet.version = 99;
        if (ProcessImmRenderGraph(graph, bridge, packet) != E_INVALIDARG || graph.ready)
            throw std::runtime_error("Graph accepted unknown packet ABI");
        packet.version = 1;
        Check(ProcessImmRenderGraph(graph, bridge, packet), "Initialize graph session");
        if (!graph.ready || !bridge.IsGraphicsInitialized()) throw std::runtime_error("Graph session not ready");
        packet.operation = 1;
        if (ProcessImmRenderGraph(graph, bridge, packet) != E_INVALIDARG)
            throw std::runtime_error("Graph accepted missing targets");
        packet.operation = 2;
        Check(ProcessImmRenderGraph(graph, bridge, packet), "Shutdown graph session");
        if (graph.ready || bridge.IsInitialized()) throw std::runtime_error("Graph session did not shut down");
        packet.operation = 0;
        Check(ProcessImmRenderGraph(graph, bridge, packet), "Reinitialize graph session");
        packet.operation = 2;
        Check(ProcessImmRenderGraph(graph, bridge, packet), "Shutdown recreated graph session");
        ComPtr<ID3D12InfoQueue> messages;
        Check(device.As(&messages), "Get debug messages");
        for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i)
        {
            SIZE_T length = 0;
            Check(messages->GetMessage(i, nullptr, &length), "Get debug message size");
            std::vector<unsigned char> storage(length);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            Check(messages->GetMessage(i, message, &length), "Get debug message");
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
            {
                std::fprintf(stderr, "IMM_DX12_PLAYER validation: %.1024s\n", message->pDescription);
                throw std::runtime_error("D3D12 debug layer reported an error");
            }
        }
        timer.End();
        log.End();
        std::ofstream result("d3d12-player-result.json");
        result << R"({"status":"pass","api":"D3D12","adapter":"WARP","scope":"player-scene-smoke","configurations_verified":4,"documents_loaded":4,"scene_frames_verified":12,"host_depth_frames_verified":4,"imm_depth_write_frames_verified":4,"msaa_samples":8,"unity_queue_event_contract_mocked":true,"unity_target_binding_mocked":true,"borrowed_renderer_lifecycle_verified":true,"render_graph_packet_lifecycle_verified":true,"debug_layer_enabled":true,"imm_scene_renderer":false})";
        result.close();
        if (!result) throw std::runtime_error("Write player initialization evidence");
        std::puts("IMM_DX12_PLAYER PASS twelve sample scene readbacks including bidirectional depth and cleanup; complete layer coverage not tested");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "IMM_DX12_PLAYER FAIL %s\n", error.what());
        return 1;
    }
}
