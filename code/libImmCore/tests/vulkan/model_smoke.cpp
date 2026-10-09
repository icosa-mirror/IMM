#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <memory>
#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmImporter/src/document/layerPaintStatic.h"
#include "libImmPlayer/src/layerRenderers/layerRendererPaint/static/layerRendererPaintStatic.h"
#include "libImmPlayer/src/layerRenderers/layerRendererPicture/layerRendererPicture.h"
#include "libImmImporter/src/document/layerPicture.h"
#include "libImmCore/src/libBasics/piImage.h"
#include "libImmImporter/src/document/layerPaintPretessellated.h"
#include "libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated/layerRendererPaintPretessellated.h"
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

void RunVulkanMultiviewProbe(ImmCore::piRenderer::piReporter& reporter, ImmCore::piLog& log);

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
    if (!layer.Init(ImmImporter::Layer::Type::Model, L"mono model", true,
        trans3d::identity(), trans3d::identity(), 1, false, piTick(1), 1, 0, &log, false))
        throw std::runtime_error("Initialize mono model wrapper");
    layer.SetImplementation(&model); layer.SetLoaded(true);
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
    layer.SetLoaded(false); layer.Deinit(&log);
}

static void VerifyModelMsaaCoverage(ImmCore::piRendererVulkan& renderer, ImmCore::piLog& log)
{
    using namespace ImmCore;
    constexpr int size = 64;
    for (int samples : {4, 8}) {
    const piRenderer::TextureInfo colorInfo = { piRenderer::TextureType::T2D,
        piRenderer::Format::C3_11_11_10_FLOAT, size, size, 1, samples, 1, 0 };
    const piRenderer::TextureInfo depthInfo = { piRenderer::TextureType::T2D,
        piRenderer::Format::D1_32_FLOAT, size, size, 1, samples, 1, 0 };
    auto color = renderer.CreateTexture(L"coverage-color", &colorInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1, nullptr);
    auto depth = renderer.CreateTexture(L"coverage-depth", &depthInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1, nullptr);
    if (!color || !depth) throw std::runtime_error("Create multisample coverage attachments");
    auto target = renderer.CreateRenderTarget(color, nullptr, nullptr, nullptr, depth);
    if (!target) throw std::runtime_error("Create multisample coverage target");
    int totalMismatches = 0;
    for (int colorSpace = 0; colorSpace < 2; ++colorSpace) {
        ImmPlayer::LayerRendererModel models;
        if (!models.Init(&renderer, &log, static_cast<ImmImporter::Drawing::ColorSpace>(colorSpace), true))
            throw std::runtime_error("Initialize coverage model renderer");
        std::vector<unsigned char> opaque(size * size * 4), half(size * size * 4);
        const float black[4] = {0, 0, 0, 1};
        renderer.SetRenderTarget(target); renderer.Clear(black, nullptr, nullptr, nullptr, true);
        DrawModelProbe(renderer, log, models, false, 1, size, color, opaque.data());
        renderer.SetRenderTarget(target); renderer.Clear(black, nullptr, nullptr, nullptr, true);
        DrawModelProbe(renderer, log, models, false, 0.5f, size, color, half.data());
        int opaqueMismatches = 0, coverageMismatches = 0;
        for (int y = 16; y < size - 16; ++y) for (int x = 16; x < size - 16; ++x) {
            const int pixel = 4 * (y * size + x);
            if (std::abs(int(opaque[pixel]) - (colorSpace == 0 ? 55 : 128)) > 3) ++opaqueMismatches;
            if (std::abs(2 * int(half[pixel]) - int(opaque[pixel])) > 3) ++coverageMismatches;
        }
        std::printf("IMM_VULKAN_MSAA_COVERAGE samples=%d colorSpace=%d opaqueMismatches=%d coverageMismatches=%d\n",
            samples, colorSpace, opaqueMismatches, coverageMismatches);
        totalMismatches += opaqueMismatches + coverageMismatches;
        models.Deinit(&renderer, &log);
    }
    renderer.SetRenderTarget(nullptr); renderer.DestroyRenderTarget(target);
    renderer.DestroyTexture(color); renderer.DestroyTexture(depth);
    if (totalMismatches) throw std::runtime_error("Half opacity does not cover exactly half the attachment samples");
    }
    std::puts("IMM_VULKAN_MSAA_COVERAGE PASS 4x 8x half opacity in both color spaces");
}

template<typename LayerType, typename DrawingType, typename RendererType>
static void DrawPaintProbe(ImmCore::piRendererVulkan& renderer, ImmCore::piLog& log, int colorSpace, float opacity, int viewportSize, ImmCore::piTexture color, unsigned char* pixels, int directional = 0)
{
    using namespace ImmCore;
    LayerType paint;
    if (!paint.Init(1, 1, 0, 30, 1)) throw std::runtime_error("Initialize packed paint layer");
    paint.GetFrameBuffer()[0] = 0;
    auto* drawing = static_cast<DrawingType*>(paint.GetDrawing(0));
    if (!drawing->Init(1) || !drawing->StartAdding(1.0f)) throw std::runtime_error("Initialize packed drawing");
    auto element = std::make_unique<ImmImporter::Element>();
    ImmImporter::Element::PointSource points[2] = {};
    for (int i = 0; i < 2; ++i)
    {
        points[i].mPos = vec3(i ? 0.6f : -0.6f, 0, 0.5f);
        points[i].mNor = vec3(0, 0, 1);
        points[i].mDir = vec3(directional < 0 ? -1.0f : 1.0f, 0, 0);
        points[i].mCol = vec3(0.25f);
        points[i].mAlpha = 1.0f; points[i].mWidth = 0.5f;
    }
    const auto visibility = directional ? ImmImporter::Element::VisibilityType::FadePow2 : ImmImporter::Element::VisibilityType::Always;
    if (!element->Set(points, 2, ImmImporter::Element::BrushSectionType::Segment, visibility, 1.0f) ||
        !drawing->Add(element.get(), static_cast<ImmImporter::Drawing::ColorSpace>(colorSpace), false))
        throw std::runtime_error("Generate packed paint geometry");
    drawing->StopAdding(); drawing->SetLoaded(true);
    ImmImporter::Layer layer(nullptr, nullptr, 0);
    layer.SetImplementation(&paint); layer.SetLoaded(true);
    RendererType paintRenderer;
    if (!paintRenderer.Init(&renderer, &log, static_cast<ImmImporter::Drawing::ColorSpace>(colorSpace), true) ||
        !paintRenderer.LoadInCPU(&log, &layer) || !paintRenderer.LoadInGPU(&renderer, nullptr, &log, &layer))
        throw std::runtime_error("Load packed paint renderer probe");
    float frame[4] = {};
    float display[36] = {};
    for (int eye = 0; eye < 2; ++eye)
        for (int axis = 0; axis < 4; ++axis) display[eye * 16 + axis * 5] = 1;
    display[32] = display[33] = static_cast<float>(viewportSize);
    auto frameBuffer = renderer.CreateBuffer(frame, sizeof(frame), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto displayBuffer = renderer.CreateBuffer(display, sizeof(display), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto layerBuffer = renderer.CreateBuffer(nullptr, sizeof(ImmPlayer::LayersState), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    if (!frameBuffer || !displayBuffer || !layerBuffer) throw std::runtime_error("Create model probe constants");
    const int pass[4] = {};
    auto passBuffer = renderer.CreateBuffer(pass, sizeof(pass), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    renderer.AttachShaderConstants(passBuffer, 5);
    renderer.AttachShaderConstants(frameBuffer, 0);
    renderer.AttachShaderConstants(layerBuffer, 3);
    renderer.AttachShaderConstants(displayBuffer, 4);
    const int viewport[] = {0, 0, viewportSize, viewportSize}; renderer.SetViewport(0, viewport);
    paintRenderer.PrepareForDisplay(ImmPlayer::StereoMode::None);
    paintRenderer.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()), trans3d::identity(), opacity);
    paintRenderer.DisplayRender(&renderer, &log, layerBuffer, 0);
    if (paintRenderer.GetDrawCallInfo().numDrawCalls != 1 || paintRenderer.GetDrawCallInfo().numTriangles != 2)
        throw std::runtime_error("Model probe did not submit its two triangles");
    if (color && pixels) renderer.GetTextureContent(color, pixels, piRenderer::Format::C4_8_UNORM);
    paintRenderer.UnloadInGPU(&renderer, nullptr, &log, &layer);
    paintRenderer.UnloadInCPU(&log, &layer);
    renderer.DestroyBuffer(frameBuffer); renderer.DestroyBuffer(displayBuffer); renderer.DestroyBuffer(layerBuffer);
    paintRenderer.Deinit(&renderer, &log);
    renderer.DestroyBuffer(passBuffer);
    paint.Deinit();
}


static void DrawPictureProbe(ImmCore::piRendererVulkan& renderer, ImmCore::piLog& log, int colorSpace, float offset, int viewportSize, ImmCore::piTexture color, unsigned char* output, bool fallback, int pictureFormat = 4, int cubeFace = 4)
{
    using namespace ImmCore;
    constexpr float opacity = 1.0f;
    constexpr bool layered = false;
    ImmImporter::LayerPicture picture;
    const auto type = pictureFormat == 1 ? ImmImporter::LayerPicture::Image360EquirectStereo :
        pictureFormat == 2 ? ImmImporter::LayerPicture::Image360CubemapCrossMono :
        pictureFormat == 3 ? ImmImporter::LayerPicture::Image360CubemapVstripMono :
        pictureFormat == 4 ? ImmImporter::LayerPicture::Image2D : ImmImporter::LayerPicture::Image360EquirectMono;
    picture.Init(type, false, &log);
    const int width = pictureFormat == 3 ? 16 : 64;
    const int height = pictureFormat == 2 ? 48 : pictureFormat == 3 ? 96 : 64;
    piImage source;
    const piImage::Format format = piImage::FORMAT_I_RGBA;
    if (!source.Init(piImage::TYPE_2D, width, height, 1, 1, &format)) throw std::runtime_error("Allocate panorama image");
    auto* pixels = static_cast<unsigned char*>(source.GetData(0));
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
    {
        const int offset = 4 * (y * width + x);
        pixels[offset] = (pictureFormat != 1 || y < 32) ? 128 : 0;
        pixels[offset + 1] = (pictureFormat != 1 || y < 32) ? 0 : 255;
        pixels[offset + 2] = 0; pixels[offset + 3] = 255;
    }
    if (pictureFormat == 2 || pictureFormat == 3)
    {
        // Input cross layout: +X at (2,1), -X at (0,1), +Y at (1,0),
        // -Y at (1,2), +Z at (3,1), -Z at (1,1). A strip stores that face order.
        const int crossX[] = {2, 0, 1, 1, 3, 1};
        const int crossY[] = {1, 1, 0, 2, 1, 1};
        for (int face = 0; face < 6; ++face)
            for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
            {
                const int px = pictureFormat == 2 ? crossX[face] * 16 + x : x;
                const int py = pictureFormat == 2 ? crossY[face] * 16 + y : face * 16 + y;
                pixels[4 * (py * width + px)] = static_cast<unsigned char>((face + 1) * 32);
            }
    }
    piTArray<uint8_t> encoded;
    if (!encoded.Init(0, false) || !source.WriteToMemory(&encoded, 0, L"png") ||
        !picture.LoadAssetMemory(encoded, &log, L"png")) throw std::runtime_error("Load panorama image");
    encoded.End(); source.Free();
    ImmImporter::Layer layer(nullptr, nullptr, 0);
    layer.SetImplementation(&picture); layer.SetLoaded(true);
    ImmPlayer::LayerRendererPicture pictureRenderer;
    if (!pictureRenderer.Init(&renderer, &log, static_cast<ImmImporter::Drawing::ColorSpace>(colorSpace), true) ||
        !pictureRenderer.LoadInCPU(&log, &layer) || !pictureRenderer.LoadInGPU(&renderer, nullptr, &log, &layer))
        throw std::runtime_error("Load panorama renderer probe");
    float frame[4] = {};
    float display[36] = {};
    for (int eye = 0; eye < 2; ++eye)
        for (int axis = 0; axis < 4; ++axis) display[eye * 16 + axis * 5] = 1;
    if (pictureFormat == 2 || pictureFormat == 3)
    {
        // Symmetric orthonormal rotations map the requested cube axis onto +Z.
        const float rotations[6][9] = {
            {0,0,1, 0,-1,0, 1,0,0}, {0,0,-1, 0,-1,0, -1,0,0},
            {-1,0,0, 0,0,1, 0,1,0}, {-1,0,0, 0,0,-1, 0,-1,0},
            {1,0,0, 0,1,0, 0,0,1}, {-1,0,0, 0,1,0, 0,0,-1}};
        for (int eye = 0; eye < 2; ++eye)
            for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col)
                display[eye * 16 + row * 4 + col] = rotations[cubeFace][row * 3 + col];
    }
    if (fallback) display[19] += 0.25f;
    display[32] = display[33] = static_cast<float>(viewportSize);
    auto frameBuffer = renderer.CreateBuffer(frame, sizeof(frame), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto displayBuffer = renderer.CreateBuffer(display, sizeof(display), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto layerBuffer = renderer.CreateBuffer(nullptr, sizeof(ImmPlayer::LayersState), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    if (!frameBuffer || !displayBuffer || !layerBuffer) throw std::runtime_error("Create picture probe constants");
    const int pass[4] = {fallback ? 1 : 0, 0, 0, 0};
    auto passBuffer = renderer.CreateBuffer(pass, sizeof(pass), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    renderer.AttachShaderConstants(passBuffer, 5);
    renderer.AttachShaderConstants(frameBuffer, 0);
    renderer.AttachShaderConstants(layerBuffer, 3);
    renderer.AttachShaderConstants(displayBuffer, 4);
    const int viewport[] = {0, 0, viewportSize, viewportSize}; renderer.SetViewport(0, viewport);
    pictureRenderer.PrepareForDisplay(fallback ? ImmPlayer::StereoMode::Fallback : ImmPlayer::StereoMode::None);
    pictureRenderer.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()),
        pictureFormat == 4 ? trans3d::translate(offset, 0.0, 0.5) * trans3d::scale(0.25) : trans3d::identity(), opacity);
    pictureRenderer.DisplayRender(&renderer, &log, layerBuffer, 0);
    if (pictureRenderer.GetDrawCallInfo().numDrawCalls != 1)
        throw std::runtime_error("Picture probe did not submit a draw");
    renderer.GetTextureContent(color, output, piRenderer::Format::C4_8_UNORM);
    pictureRenderer.UnloadInGPU(&renderer, nullptr, &log, &layer);
    pictureRenderer.UnloadInCPU(&log, &layer);
    pictureRenderer.Deinit(&renderer, &log);
    renderer.DestroyBuffer(frameBuffer); renderer.DestroyBuffer(displayBuffer); renderer.DestroyBuffer(layerBuffer);
    renderer.DestroyBuffer(passBuffer);
    picture.Deinit();
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
        // Run pictures first so their readback cannot depend on paint/model counters.
        for (int colorSpace = 0; colorSpace < 2; ++colorSpace)
        for (float offset : {-0.5f, 0.5f})
        for (bool fallback : {false, true})
        {
            renderer.SetRenderTarget(target);
            const float black[4] = {0,0,0,1};
            renderer.Clear(black, nullptr, nullptr, nullptr, true);
            std::vector<unsigned char> pixels(size * size * 4);
            DrawPictureProbe(renderer, log, colorSpace, offset, size, color, pixels.data(), fallback);
            int visible = 0, sumX = 0;
            for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const auto* pixel = &pixels[(y * size + x) * 4];
                if (pixel[0] < 8) continue;
                const int expected = colorSpace == 0 ? 64 : 128;
                if (std::abs(static_cast<int>(pixel[0]) - expected) > 3)
                    { std::fprintf(stderr, "IMM_VULKAN_PICTURE colourSpace=%d red=%u expected=%d\n", colorSpace, pixel[0], expected); throw std::runtime_error("Picture colour-space readback mismatch"); }
                ++visible; sumX += x;
            }
            const float center = visible ? static_cast<float>(sumX) / visible : -1;
            std::fprintf(stderr, "IMM_VULKAN_PICTURE offset=%.1f visible=%d center=%.1f\n", offset, visible, center);
            const float expectedCenter = 31.5f + offset * 32 + (fallback ? 8 : 0);
            if (visible < 20 || visible > 1000 || std::abs(center - expectedCenter) > 1.5f)
                throw std::runtime_error("Picture layer/camera transform readback mismatch");
        }
        for (int colorSpace = 0; colorSpace < 2; ++colorSpace)
        for (bool rightEye : {false, true})
        {
            renderer.SetRenderTarget(target);
            const float black[4] = {0,0,0,1};
            renderer.Clear(black, nullptr, nullptr, nullptr, true);
            std::vector<unsigned char> pixels(size * size * 4);
            DrawPictureProbe(renderer, log, colorSpace, 0, size, color, pixels.data(), rightEye, 1);
            const auto* center = &pixels[(size / 2 * size + size / 2) * 4];
            const int expectedRed = rightEye ? 0 : colorSpace == 0 ? 55 : 128;
            const int expectedGreen = rightEye ? 255 : 0;
            if (std::abs(static_cast<int>(center[0]) - expectedRed) > 3 ||
                std::abs(static_cast<int>(center[1]) - expectedGreen) > 3)
            {
                std::fprintf(stderr, "IMM_VULKAN_PANORAMA right=%d colourSpace=%d rgb=%u/%u/%u\n", rightEye, colorSpace, center[0], center[1], center[2]);
                throw std::runtime_error("Stereo panorama atlas eye selection mismatch");
            }
        }
        for (int colorSpace = 0; colorSpace < 2; ++colorSpace)
        for (int pictureFormat : {2, 3})
        for (int face = 0; face < 6; ++face)
        {
            renderer.SetRenderTarget(target);
            const float black[4] = {0,0,0,1};
            renderer.Clear(black, nullptr, nullptr, nullptr, true);
            std::vector<unsigned char> pixels(size * size * 4);
            DrawPictureProbe(renderer, log, colorSpace, 0, size, color, pixels.data(), false, pictureFormat, face);
            const int actual = pixels[(size / 2 * size + size / 2) * 4];
            const float authored = static_cast<float>((face + 1) * 32) / 255;
            const int expected = colorSpace == 0 ? static_cast<int>(std::pow(authored, 2.2f) * 255) : (face + 1) * 32;
            if (std::abs(actual - expected) > 4)
            {
                std::fprintf(stderr, "IMM_VULKAN_CUBEMAP layout=%d colourSpace=%d face=%d red=%d expected=%d\n", pictureFormat, colorSpace, face, actual, expected);
                throw std::runtime_error("Production cubemap sampler or face upload mismatch");
            }
        }
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
            modelRenderer.Deinit(&renderer, &log);
        }
        for (int colorSpace = 0; colorSpace < 2; ++colorSpace)
        for (float opacity : {1.0f, 0.5f, 0.0f})
        for (bool staticPaint : {false, true})
        {
            if (!renderer.SetRenderTarget(target)) throw std::runtime_error("Bind packed paint target");
            const float black[4] = {0,0,0,1};
            renderer.Clear(black, nullptr, nullptr, nullptr, true);
            std::vector<unsigned char> pixels(size * size * 4);
            if (staticPaint) DrawPaintProbe<ImmImporter::LayerPaintStatic, ImmImporter::DrawingStatic, ImmPlayer::LayerRendererPaintStatic>(renderer, log, colorSpace, opacity, size, color, pixels.data());
            else DrawPaintProbe<ImmImporter::LayerPaintPretessellated, ImmImporter::DrawingPretessellated, ImmPlayer::LayerRendererPaintPretessellated>(renderer, log, colorSpace, opacity, size, color, pixels.data());
            int visible = 0;
            for (int pixel = 0; pixel < size * size; ++pixel)
                if (pixels[pixel * 4] > 8)
                {
                    const int expected = colorSpace == 0 ? 64 : static_cast<int>(std::pow(0.25f, 1.0f / 2.2f) * 255);
                    for (int channel = 0; channel < 3; ++channel)
                        if (std::abs(static_cast<int>(pixels[pixel * 4 + channel]) - expected) > 4)
                            throw std::runtime_error("Packed paint colour-space readback mismatch");
                    ++visible;
                }
            std::fprintf(stderr, "IMM_VULKAN_PRETESSELLATED colourSpace=%d opacity=%.1f visible=%d\n", colorSpace, opacity, visible);
            if ((opacity == 1 && visible < 500) || (opacity == 0 && visible != 0) ||
                (opacity == 0.5f && (visible < 100 || visible > 1100)))
                throw std::runtime_error("Production packed paint readback mismatch");
        }
        for (int direction : {-1, 1})
        {
        renderer.SetRenderTarget(target);
        const float black[4] = {0,0,0,1};
        renderer.Clear(black, nullptr, nullptr, nullptr, true);
        std::vector<unsigned char> directionalPixels(size * size * 4);
        DrawPaintProbe<ImmImporter::LayerPaintPretessellated, ImmImporter::DrawingPretessellated, ImmPlayer::LayerRendererPaintPretessellated>(renderer, log, 0, 1.0f, size, color, directionalPixels.data(), direction);
        int left = 0, right = 0;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (directionalPixels[(y * size + x) * 4] > 8)
                {
                    if (x < size / 2) ++left;
                    else ++right;
                }
        std::fprintf(stderr, "IMM_VULKAN_PRETESSELLATED direction=%d left=%d right=%d\n", direction, left, right);
        if ((direction > 0 && (right < 50 || right < 3 * left)) ||
            (direction < 0 && (left < 50 || left < 3 * right)))
            throw std::runtime_error("Packed direction decoding or facing readback mismatch");
        }
        renderer.SetRenderTarget(nullptr);
        renderer.DestroyRenderTarget(target);
        renderer.DestroyTexture(color); renderer.DestroyTexture(depth);
        VerifyModelMsaaCoverage(renderer, log);
        renderer.Deinitialize();
        RunVulkanMultiviewProbe(reporter, log);
        log.End();
        std::puts("IMM_VULKAN_MODEL PASS production model layouts and packed paint, linear/gamma colour and opacity readback");
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
