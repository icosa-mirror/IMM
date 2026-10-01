#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <fstream>
#include "../../src/libRender/directx12/piDX12_Renderer.h"
#include "libImmPlayer/src/player.h"

using Microsoft::WRL::ComPtr;
static void Check(HRESULT result, const char* label)
{
    if (FAILED(result)) throw std::runtime_error(label);
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
        ImmCore::piRendererDX12 renderer;
        Check(renderer.InitializeExternal(device.Get(), queue.Get()), "Initialize renderer");
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
            player.Deinit();
        }
        renderer.Deinitialize();
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
        result << R"({"status":"pass","api":"D3D12","adapter":"WARP","scope":"player-initialization","configurations_verified":4,"debug_layer_enabled":true,"imm_scene_renderer":false})";
        result.close();
        if (!result) throw std::runtime_error("Write player initialization evidence");
        std::puts("IMM_DX12_PLAYER PASS initialization and cleanup; scene rendering not tested");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "IMM_DX12_PLAYER FAIL %s\n", error.what());
        return 1;
    }
}
