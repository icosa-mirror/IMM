#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <cmath>
#include "libImmCore/src/libRender/vulkan/piVulkan_Renderer.h"
#include "libImmPlayer/src/player.h"
#include "libImmPlayer/src/layerRenderers/layerRendererModel/layerRendererModel.h"

struct ProbeReporter : ImmCore::piRenderer::piReporter
{
    void Info(const char*) override {}
    void Error(const char* message, int) override { std::fprintf(stderr, "IMM_VULKAN_MODEL error: %s\n", message); }
    void Begin(uint64_t, uint64_t, int, int) override {}
    void Texture(const wchar_t*, uint64_t, ImmCore::piRenderer::Format, bool, int, int, int) override {}
    void End() override {}
};

static void DrawModelProbe(ImmCore::piRendererVulkan& renderer, ImmCore::piLog& log, ImmPlayer::LayerRendererModel& modelRenderer, bool padded, float opacity, int viewportSize, ImmCore::piTexture color, unsigned char* pixels)
{
    using namespace ImmCore;
    ImmImporter::LayerModel model;
    if (!model.Init(false, ImmImporter::LayerModel::ShadingModel::Unlit)) throw std::runtime_error("Initialize model probe");
    auto* mesh = model.GetMesh();
    piMesh::VertexFormat format = {};
    format.mStride = (padded ? 11 : 7) * sizeof(float); format.mNumElems = 2;
    format.mElems[0] = {3, piMesh::VertexElemDataType::Float, false, 0};
    format.mElems[1] = {4, piMesh::VertexElemDataType::Float, false, 3 * sizeof(float)};
    if (!mesh->Init(1, 4, &format, piMesh::Type::Polys, 1, 2)) throw std::runtime_error("Allocate model probe mesh");
    float vertices[4][11] = {
        {-0.75f,-0.75f,0.5f, 0.5f,0.5f,0.5f,1}, {0.75f,-0.75f,0.5f, 0.5f,0.5f,0.5f,1},
        {0.75f,0.75f,0.5f, 0.5f,0.5f,0.5f,1}, {-0.75f,0.75f,0.5f, 0.5f,0.5f,0.5f,1}};
    for (uint32_t i = 0; i < 4; ++i) mesh->SetVertex(0, i, vertices[i]);
    mesh->SetTriangle(0, 0, 0, 1, 2); mesh->SetTriangle(0, 1, 0, 2, 3);
    mesh->CalcBBox(0, 0);
    ImmImporter::Layer layer(nullptr, nullptr, 0);
    layer.SetImplementation(&model);
    if (!modelRenderer.LoadInCPU(&log, &layer) || !modelRenderer.LoadInGPU(&renderer, nullptr, &log, &layer))
        throw std::runtime_error("Load model renderer probe");
    float frame[4] = {};
    float display[36] = {};
    for (int eye = 0; eye < 2; ++eye)
        for (int axis = 0; axis < 4; ++axis) display[eye * 16 + axis * 5] = 1;
    display[32] = display[33] = static_cast<float>(viewportSize);
    auto frameBuffer = renderer.CreateBuffer(frame, sizeof(frame), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto displayBuffer = renderer.CreateBuffer(display, sizeof(display), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto layerBuffer = renderer.CreateBuffer(nullptr, sizeof(ImmPlayer::LayersState), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    if (!frameBuffer || !displayBuffer || !layerBuffer) throw std::runtime_error("Create model probe constants");
    renderer.AttachShaderConstants(frameBuffer, 0);
    renderer.AttachShaderConstants(layerBuffer, 3);
    renderer.AttachShaderConstants(displayBuffer, 4);
    const int viewport[] = {0, 0, viewportSize, viewportSize}; renderer.SetViewport(0, viewport);
    modelRenderer.PrepareForDisplay(ImmPlayer::StereoMode::None);
    modelRenderer.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()), trans3d::identity(), opacity);
    modelRenderer.DisplayRender(&renderer, &log, layerBuffer, 0);
    if (modelRenderer.GetDrawCallInfo().numDrawCalls != 1 || modelRenderer.GetDrawCallInfo().numTriangles != 2)
        throw std::runtime_error("Model probe did not submit its two triangles");
    if (color && pixels) renderer.GetTextureContent(color, pixels, piRenderer::Format::C4_8_UNORM);
    modelRenderer.UnloadInGPU(&renderer, nullptr, &log, &layer);
    modelRenderer.UnloadInCPU(&log, &layer);
    renderer.DestroyBuffer(frameBuffer); renderer.DestroyBuffer(displayBuffer); renderer.DestroyBuffer(layerBuffer);
    mesh->DeInit(); model.Deinit();
}


int main()
{
    using namespace ImmCore;
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"IMM_VULKAN_MODEL_PROBE";
    if (!RegisterClassW(&windowClass)) return 1;
    HWND window = CreateWindowW(windowClass.lpszClassName, L"IMM Vulkan model probe",
        WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr, windowClass.hInstance, nullptr);
    if (!window) return 1;
    const void* windows[] = { window };
    ProbeReporter reporter;
    piRendererVulkan renderer;
    piLog log;
    try
    {
        if (!log.Init(L"vulkan-model-smoke.log", 0)) throw std::runtime_error("Initialize probe log");
        if (!renderer.Initialize(0, windows, 1, true, false, &reporter, true, nullptr))
            throw std::runtime_error("Initialize Vulkan device");
        constexpr int size = 64;
        const piRenderer::TextureInfo colorInfo = { piRenderer::TextureType::T2D, piRenderer::Format::C3_11_11_10_FLOAT, size, size, 1, 1, 1, 0 };
        const piRenderer::TextureInfo depthInfo = { piRenderer::TextureType::T2D, piRenderer::Format::D1_32_FLOAT, size, size, 1, 1, 1, 0 };
        auto color = renderer.CreateTexture(L"model-color", &colorInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1, nullptr);
        auto depth = renderer.CreateTexture(L"model-depth", &depthInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1, nullptr);
        if (!color || !depth) throw std::runtime_error("Create model probe attachments");
        auto target = renderer.CreateRenderTarget(color, nullptr, nullptr, nullptr, depth);
        if (!target) throw std::runtime_error("Create model probe target");
        for (int colorSpace = 0; colorSpace < 2; ++colorSpace)
        {
            ImmPlayer::LayerRendererModel modelRenderer;
            if (!modelRenderer.Init(&renderer, &log, static_cast<ImmImporter::Drawing::ColorSpace>(colorSpace), true))
                throw std::runtime_error("Initialize model shaders");
            // Return to the first layout to exercise both pipeline creation and reuse.
            for (bool padded : {false, true, false})
            for (float opacity : {1.0f, 0.5f, 0.0f})
            {
                if (!renderer.SetRenderTarget(target)) throw std::runtime_error("Bind model probe target");
                const float black[4] = {0,0,0,1};
                renderer.Clear(black, nullptr, nullptr, nullptr, true);
                std::vector<unsigned char> pixels(size * size * 4);
                DrawModelProbe(renderer, log, modelRenderer, padded, opacity, size, color, pixels.data());

                int visible = 0;
                const int expected = colorSpace == 0 ? 55 : 128;
                for (int y = 8; y < size-8; ++y)
                    for (int x = 8; x < size-8; ++x)
                    {
                        const auto* pixel = &pixels[(y*size+x)*4];
                        if (pixel[0] < 8 && pixel[1] < 8 && pixel[2] < 8) continue;
                        if (std::abs(static_cast<int>(pixel[0])-expected) > 3 ||
                            std::abs(static_cast<int>(pixel[1])-expected) > 3 ||
                            std::abs(static_cast<int>(pixel[2])-expected) > 3)
                            throw std::runtime_error("Model colour-space readback mismatch");
                        ++visible;
                    }
                if ((opacity == 1 && visible < 2200) || (opacity == 0 && visible != 0) ||
                    (opacity == 0.5f && (visible < 650 || visible > 1650)))
                {
                    std::fprintf(stderr, "Model readback failed: colourSpace=%d opacity=%.1f visible=%d\n", colorSpace, opacity, visible);
                    throw std::runtime_error("Model opacity or geometry readback mismatch");
                }
            }
        }
        renderer.SetRenderTarget(nullptr);
        renderer.DestroyRenderTarget(target);
        renderer.DestroyTexture(color); renderer.DestroyTexture(depth);
        renderer.Deinitialize(); log.End();
        std::puts("IMM_VULKAN_MODEL PASS production mesh layouts, linear/gamma colour and opacity readback");
        DestroyWindow(window);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "IMM_VULKAN_MODEL FAIL %s\n", error.what());
        renderer.Deinitialize(); log.End();
        DestroyWindow(window);
        return 1;
    }
}
