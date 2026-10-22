#include "../../src/libRender/directx11/piDX11_Renderer.h"

#include <d3d11.h>
#include <cstdio>
#include <stdexcept>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

static void Require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try
    {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL level;
        Require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context)), "create headless WARP device");
        ImmCore::piRendererDX11 renderer;
        Require(renderer.Initialize(0, nullptr, 0, true, true, nullptr, false, device.Get()),
            "initialize renderer with borrowed WARP device");

        const float points[] = { 1, 2, 3, 4 };
        auto ordinary = renderer.CreateBuffer(points, sizeof(points),
            ImmCore::piRenderer::BufferType::Static, ImmCore::piRenderer::BufferUse::Vertex);
        Require(ordinary != nullptr, "create ordinary buffer without SRV");
        renderer.DestroyBuffer(ordinary);
        for (int iteration = 0; iteration < 8; ++iteration)
        {
            const auto mode = iteration % 2 == 0 ? ImmCore::piRenderer::BufferType::Static :
                ImmCore::piRenderer::BufferType::Dynamic;
            auto buffer = renderer.CreateStructuredBuffer(points, 4, sizeof(float),
                mode, ImmCore::piRenderer::BufferUse::ShaderResource);
            Require(buffer != nullptr, "create structured buffer");
            renderer.AttachShaderBuffer(buffer, 0);
            ID3D11ShaderResourceView * view = nullptr;
            context->VSGetShaderResources(0, 1, &view); // Adds one observer reference.
            Require(view != nullptr, "retrieve actual structured-buffer SRV");
            ID3D11Resource * resource = nullptr;
            view->GetResource(&resource); // Keeps the real buffer alive for inspection.
            Require(resource != nullptr, "retrieve actual backing buffer");
            // Remove both VS and PS bindings. No commands reference this buffer:
            // this checks renderer ownership, not in-flight GPU completion.
            context->ClearState();
            context->Flush();
            renderer.DestroyBuffer(buffer);
            const ULONG remainingViewReferences = view->Release();
            if (remainingViewReferences != 0)
            {
                // Clean the known leaked renderer reference in this isolated
                // no-draw test before reporting the regression.
                view->Release();
                resource->Release();
                std::fprintf(stderr, "[IMM_D3D11_BUFFER_OWNERSHIP] leakedViewReferences=%lu iteration=%d\n",
                    static_cast<unsigned long>(remainingViewReferences), iteration);
                renderer.Deinitialize();
                throw std::runtime_error("DestroyBuffer retained its shader-resource view");
            }
            const ULONG remainingResourceReferences = resource->Release();
            std::printf("[IMM_D3D11_BUFFER_OWNERSHIP_SAMPLE] iteration=%d viewReferences=%lu resourceReferences=%lu\n",
                iteration, static_cast<unsigned long>(remainingViewReferences),
                static_cast<unsigned long>(remainingResourceReferences));
            Require(remainingResourceReferences == 0, "DestroyBuffer retained its backing resource");
        }
        renderer.Deinitialize();
        std::puts("[IMM_D3D11_BUFFER_OWNERSHIP] passed iterations=8 backend=WARP viewAndBufferReleased=1 noInFlightClaim=1");
        return 0;
    }
    catch (const std::exception & error)
    {
        std::fprintf(stderr, "[IMM_D3D11_BUFFER_OWNERSHIP] failed: %s\n", error.what());
        return 1;
    }
}
