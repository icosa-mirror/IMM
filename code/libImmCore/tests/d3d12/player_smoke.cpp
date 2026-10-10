#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>
#include <limits>
#include "../../src/libRender/directx12/piDX12_Renderer.h"
#include "libImmPlayer/src/player.h"
#include "libImmPlayer/src/layerRenderers/layerRendererModel/layerRendererModel.h"
#include "libImmPlayer/src/layerRenderers/layerRendererPicture/layerRendererPicture.h"
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
static void DrawModelProbe(ImmCore::piRendererDX12& renderer, ImmCore::piLog& log, int colorSpace, float opacity = 1.0f, bool layered = false, int viewportSize = 256)
{
    using namespace ImmCore;
    ImmImporter::LayerModel model;
    if (!model.Init(false, ImmImporter::LayerModel::ShadingModel::Unlit)) throw std::runtime_error("Initialize model probe");
    auto* mesh = model.GetMesh();
    piMesh::VertexFormat format = {};
    format.mStride = 7 * sizeof(float); format.mNumElems = 2;
    format.mElems[0] = {3, piMesh::VertexElemDataType::Float, false, 0};
    format.mElems[1] = {4, piMesh::VertexElemDataType::Float, false, 3 * sizeof(float)};
    if (!mesh->Init(1, 4, &format, piMesh::Type::Polys, 1, 2)) throw std::runtime_error("Allocate model probe mesh");
    float vertices[4][7] = {
        {-0.75f,-0.75f,0.5f, 0.5f,0.5f,0.5f,1}, {0.75f,-0.75f,0.5f, 0.5f,0.5f,0.5f,1},
        {0.75f,0.75f,0.5f, 0.5f,0.5f,0.5f,1}, {-0.75f,0.75f,0.5f, 0.5f,0.5f,0.5f,1}};
    for (uint32_t i = 0; i < 4; ++i) mesh->SetVertex(0, i, vertices[i]);
    mesh->SetTriangle(0, 0, 0, 1, 2); mesh->SetTriangle(0, 1, 0, 2, 3);
    mesh->CalcBBox(0, 0);
    ImmImporter::Layer layer(nullptr, nullptr, 0);
    layer.SetImplementation(&model);
    ImmPlayer::LayerRendererModel modelRenderer;
    if (!modelRenderer.Init(&renderer, &log, static_cast<ImmImporter::Drawing::ColorSpace>(colorSpace), true) ||
        !modelRenderer.LoadInCPU(&log, &layer) || !modelRenderer.LoadInGPU(&renderer, nullptr, &log, &layer))
        throw std::runtime_error("Load model renderer probe");
    float frame[4] = {};
    float display[36] = {};
    for (int eye = 0; eye < 2; ++eye)
        for (int axis = 0; axis < 4; ++axis) display[eye * 16 + axis * 5] = 1;
    if (layered) { display[3] = -0.25f; display[19] = 0.25f; }
    display[32] = display[33] = static_cast<float>(viewportSize);
    auto frameBuffer = renderer.CreateBuffer(frame, sizeof(frame), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto displayBuffer = renderer.CreateBuffer(display, sizeof(display), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto layerBuffer = renderer.CreateBuffer(nullptr, sizeof(ImmPlayer::LayersState), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    if (!frameBuffer || !displayBuffer || !layerBuffer) throw std::runtime_error("Create model probe constants");
    renderer.AttachShaderConstants(frameBuffer, 0);
    renderer.AttachShaderConstants(layerBuffer, 3);
    renderer.AttachShaderConstants(displayBuffer, 4);
    const int viewport[] = {0, 0, viewportSize, viewportSize}; renderer.SetViewport(0, viewport);
    modelRenderer.PrepareForDisplay(layered ? ImmPlayer::StereoMode::Preferred : ImmPlayer::StereoMode::None);
    modelRenderer.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()), trans3d::identity(), opacity);
    modelRenderer.DisplayRender(&renderer, &log, layerBuffer, 0);
    if (modelRenderer.GetDrawCallInfo().numDrawCalls != 1 || modelRenderer.GetDrawCallInfo().numTriangles != (layered ? 4 : 2))
        throw std::runtime_error("Model probe did not submit its two triangles");
    modelRenderer.UnloadInGPU(&renderer, nullptr, &log, &layer);
    modelRenderer.UnloadInCPU(&log, &layer);
    modelRenderer.Deinit(&renderer, &log);
    renderer.DestroyBuffer(frameBuffer); renderer.DestroyBuffer(displayBuffer); renderer.DestroyBuffer(layerBuffer);
    model.Deinit();
}

static void DrawPictureProbe(ImmCore::piRendererDX12& renderer, ImmCore::piLog& log, int colorSpace, int pictureFormat, int cubeFace, float opacity, bool layered = false, int viewportSize = 256)
{
    using namespace ImmCore;
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
    display[32] = display[33] = static_cast<float>(viewportSize);
    auto frameBuffer = renderer.CreateBuffer(frame, sizeof(frame), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto displayBuffer = renderer.CreateBuffer(display, sizeof(display), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto layerBuffer = renderer.CreateBuffer(nullptr, sizeof(ImmPlayer::LayersState), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    if (!frameBuffer || !displayBuffer || !layerBuffer) throw std::runtime_error("Create picture probe constants");
    renderer.AttachShaderConstants(frameBuffer, 0);
    renderer.AttachShaderConstants(layerBuffer, 3);
    renderer.AttachShaderConstants(displayBuffer, 4);
    const int viewport[] = {0, 0, viewportSize, viewportSize}; renderer.SetViewport(0, viewport);
    pictureRenderer.PrepareForDisplay(layered ? ImmPlayer::StereoMode::Preferred : ImmPlayer::StereoMode::None);
    pictureRenderer.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()),
        pictureFormat == 4 ? trans3d::translate(0.0, 0.0, 0.5) : trans3d::identity(), opacity);
    pictureRenderer.DisplayRender(&renderer, &log, layerBuffer, 0);
    if (pictureRenderer.GetDrawCallInfo().numDrawCalls != 1)
        throw std::runtime_error("Picture probe did not submit a draw");
    pictureRenderer.UnloadInGPU(&renderer, nullptr, &log, &layer);
    pictureRenderer.UnloadInCPU(&log, &layer);
    pictureRenderer.Deinit(&renderer, &log);
    renderer.DestroyBuffer(frameBuffer); renderer.DestroyBuffer(displayBuffer); renderer.DestroyBuffer(layerBuffer);
    picture.Deinit();
}

static void RenderScene(ImmUnityD3D12Host& host, ID3D12Device* device, ImmPlayer::Player& player, int document, const char* capture, DepthProbe probe = DepthProbe::None, int modelColorSpace = -1, ImmCore::piLog* log = nullptr, int panorama = 0, int cubeFace = 4, UINT samples = 8, float opacity = 1.0f, bool orthographic = false)
{
    auto& renderer = host.RendererInRenderEvent();
    constexpr UINT size = 256;
    const bool hostDepth = probe == DepthProbe::HostOcclusion;
    D3D12_HEAP_PROPERTIES gpu = {}; gpu.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = desc.Height = size;
    desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.SampleDesc.Count = samples;
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
    hostDepthView.Format = DXGI_FORMAT_D32_FLOAT; hostDepthView.ViewDimension = samples > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depth.Get(), &hostDepthView, dsv->GetCPUDescriptorHandleForHeapStart());
    const auto bounds = player.GetDocumentBBox(document);
    const double radius = std::max({bounds.mMaxX-bounds.mMinX, bounds.mMaxY-bounds.mMinY, bounds.mMaxZ-bounds.mMinZ, 1.0}) * 0.6;
    const auto view = ImmCore::trans3d::translate(-(bounds.mMinX+bounds.mMaxX)*0.5,
        -(bounds.mMinY+bounds.mMaxY)*0.5, -(bounds.mMinZ+bounds.mMaxZ)*0.5-radius*(orthographic ? 1000.0 : 2.0));
    const float r = static_cast<float>(radius);
    const float nearPlane = r * 0.01f, farPlane = r * (orthographic ? 2000.0f : 4.0f);
    const ImmCore::mat4x4 projection = orthographic ?
        ImmCore::mat4x4(1/r,0,0,0, 0,1/r,0,0, 0,0,1/(farPlane-nearPlane),farPlane/(farPlane-nearPlane), 0,0,0,1) :
        ImmCore::mat4x4(1,0,0,0, 0,1,0,0, 0,0,nearPlane/(farPlane-nearPlane),nearPlane*farPlane/(farPlane-nearPlane), 0,0,-1,0);
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
    if (panorama) DrawPictureProbe(renderer, *log, modelColorSpace, panorama, cubeFace, opacity);
    else if (modelColorSpace >= 0) DrawModelProbe(renderer, *log, modelColorSpace, opacity);
    else
    {
        player.SetTime(document, ImmCore::piTick::FromSeconds(3.0), ImmCore::piTick(0));
        player.GlobalWork(true, 10000);
        player.GlobalRender(ImmCore::trans3d::identity(), view, projection, ImmPlayer::StereoMode::None);
        player.RenderMono(ImmCore::ivec2(size, size), 0);
        const auto& perf = player.GetPerformanceInfoForFrame();
        std::printf("IMM_DX12_PLAYER draws=%d paint=%d triangles=%d\n", perf.numDrawCalls, perf.numPaintDrawCalls, perf.numTriangles);
        if (perf.numPaintDrawCalls <= 0 || perf.numTriangles <= 0) throw std::runtime_error("Scene submitted no paint geometry");
    }
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
    if (samples > 1)
    {
        transition(color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        commands->ResolveSubresource(resolved.Get(), 0, color.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        transition(color.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        transition(resolved.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    else
    {
        transition(color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(resolved.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_DEST);
        commands->CopyResource(resolved.Get(), color.Get());
        transition(color.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        transition(resolved.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    D3D12_TEXTURE_COPY_LOCATION source = {}; source.pResource = resolved.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION destination = {}; destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = footprint;
    commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    uint64_t completion = 0; Check(renderer.EndFrame(&completion), "Submit scene frame");
    Check(renderer.WaitForFrame(completion), "Wait for scene frame");
    void* data = nullptr; D3D12_RANGE range = {0, static_cast<SIZE_T>(bytes)};
    Check(readback->Map(0, &range, &data), "Map scene pixels");
    UINT coveragePixels = 0, coveredPixels = 0;
    const bool singleSampleCoverage = samples == 1 && modelColorSpace >= 0 && opacity == 0.5f;
    UINT visible = 0, occludedPixels = 0, retainedScenePixels = 0, backgroundPixels = 0;
    std::ofstream image(capture, std::ios::binary);
    image << "P6\n256 256\n255\n";
    for (UINT y = 0; y < size; ++y) for (UINT x = 0; x < size; ++x)
    {
        const auto* pixel = static_cast<const unsigned char*>(data) + footprint.Offset + y*footprint.Footprint.RowPitch + x*4;
        if (modelColorSpace >= 0 && !singleSampleCoverage && x == size/2 && y == size/2)
        {
            const int encodedValue = (panorama == 2 || panorama == 3) ? (cubeFace + 1) * 32 : 128;
            const int expected = int(std::round((modelColorSpace == 0 ? std::pow(encodedValue / 255.0, 2.2) * 255.0 : encodedValue) * opacity));
            for (int channel = 0; channel < 3; ++channel)
                if (std::abs(int(pixel[channel]) - (panorama && channel > 0 ? 0 : expected)) > 2)
                    throw std::runtime_error(panorama ? "Panorama image-half/colour-space readback mismatch" : "Model pixel colour-space readback mismatch");
        }
        if (singleSampleCoverage && x >= 64 && x < 192 && y >= 64 && y < 192)
        {
            ++coveragePixels;
            const int encodedValue = (panorama == 2 || panorama == 3) ? (cubeFace + 1) * 32 : 128;
            const int opaque = modelColorSpace == 0 ? int(std::round(std::pow(encodedValue / 255.0, 2.2) * 255.0)) : encodedValue;
            const bool covered = pixel[0] != 0;
            for (int channel = 0; channel < 3; ++channel)
                if (std::abs(int(pixel[channel]) - (covered && (!panorama || channel == 0) ? opaque : 0)) > 2)
                    throw std::runtime_error("Single-sample coverage produced a partially blended pixel");
            if (covered) ++coveredPixels;
        }
        if (pixel[0] || pixel[1] || pixel[2])
        {
            ++visible;
            if (x < size/2) ++occludedPixels;
        }
        if (pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255) ++backgroundPixels;
        else if (pixel[0] || pixel[1] || pixel[2]) ++retainedScenePixels;
        image.write(reinterpret_cast<const char*>(pixel), 3);
    }
    if (singleSampleCoverage && (coveragePixels != 128 * 128 ||
        std::abs(double(coveredPixels) / coveragePixels - 0.5) > 0.02))
        throw std::runtime_error("Single-sample half-opacity spatial coverage mismatch");
    D3D12_RANGE written = {}; readback->Unmap(0, &written);
    image.close();
    if (!image || visible < 100) throw std::runtime_error("Scene readback is empty or capture failed");
    if (hostDepth && occludedPixels != 0) throw std::runtime_error("IMM overwrote host-occluded pixels");
    if (probe == DepthProbe::ImmWrites && (retainedScenePixels < 100 || backgroundPixels < 100))
        throw std::runtime_error("IMM depth failed to preserve scene in front of later host geometry");
    std::printf("IMM_DX12_PLAYER scene pixels=%u depth_probe=%d\n", visible, int(probe));
}

static void VerifyLayeredTargets(ImmUnityD3D12Host& host, ID3D12Device* device, UINT samples, ImmCore::piLog* modelLog = nullptr, int pictureFormat = 0, ImmPlayer::Player* scene = nullptr, int document = -1, int configuration = 0, ImmRenderGraphState* graph = nullptr, ImmShared::ImmEngineBridge* bridge = nullptr)
{
    auto& renderer = host.RendererInRenderEvent();
    const UINT size = scene ? 128 : 32;
    D3D12_HEAP_PROPERTIES gpu = {}; gpu.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = desc.Height = size; desc.DepthOrArraySize = 2; desc.MipLevels = 1;
    desc.SampleDesc.Count = samples; desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> color, depth, resolved, readback;
    Check(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
        nullptr, IID_PPV_ARGS(&color)), "Create layered color");
    desc.Format = DXGI_FORMAT_R32_TYPELESS; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    Check(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        nullptr, IID_PPV_ARGS(&depth)), "Create layered depth");
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.Flags = D3D12_RESOURCE_FLAG_NONE; desc.SampleDesc.Count = 1;
    const auto outputState = samples > 1 ? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;
    Check(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, outputState,
        nullptr, IID_PPV_ARGS(&resolved)), "Create layered readback texture");
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[2] = {}; UINT64 bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 2, 0, footprints, nullptr, nullptr, &bytes);
    D3D12_RESOURCE_DESC buffer = {}; buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = bytes; buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES cpu = {}; cpu.Type = D3D12_HEAP_TYPE_READBACK;
    Check(device->CreateCommittedResource(&cpu, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&readback)), "Create layered readback buffer");
    Check(renderer.BeginFrame(), "Begin layered frame");
    auto colorBuffer = reinterpret_cast<UnityRenderBuffer>(color.Get());
    auto depthBuffer = reinterpret_cast<UnityRenderBuffer>(depth.Get());
    if (host.BindTargetsInRenderEvent(colorBuffer, depthBuffer, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT) != E_INVALIDARG)
        throw std::runtime_error("Mono binding accepted layered attachments");
    Check(host.BindTargetsInRenderEvent(colorBuffer, depthBuffer, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 2),
        "Bind layered attachments");
    const float black[] = {0,0,0,0}; renderer.Clear(black, nullptr, nullptr, nullptr, true);
    const char* vs = "void main(float2 p:POSITION, uint eye:SV_InstanceID, out float4 pos:SV_Position, out uint slice:SV_RenderTargetArrayIndex, out float4 color:COLOR0) { pos=float4(p,0.5,1); slice=eye; color=eye==0?float4(1,0,0,1):float4(0,1,0,1); }";
    const char* ps = "float4 main(float4 pos:SV_Position, uint slice:SV_RenderTargetArrayIndex, float4 color:COLOR0):SV_Target { return color; }";
    auto shader = renderer.CreateShader(nullptr, vs, nullptr, nullptr, nullptr, ps, nullptr);
    if (!shader) throw std::runtime_error("Create layered shader");
    renderer.AttachShader(shader);
    renderer.SetState(ImmCore::piSTATE_CULL_FACE, false);
    renderer.SetState(ImmCore::piSTATE_BLEND, false);
    const int viewport[] = {0,0,size,size}; renderer.SetViewport(0, viewport);
    try
    {
        if (scene)
        {
            const auto bounds = scene->GetDocumentBBox(document);
            const double radius = std::max({bounds.mMaxX-bounds.mMinX, bounds.mMaxY-bounds.mMinY, bounds.mMaxZ-bounds.mMinZ, 1.0}) * 0.6;
            const auto view = ImmCore::trans3d::translate(-(bounds.mMinX+bounds.mMaxX)*0.5,
                -(bounds.mMinY+bounds.mMaxY)*0.5, -(bounds.mMinZ+bounds.mMaxZ)*0.5-radius*2.0);
            const float nearPlane = float(radius)*0.01f, farPlane = float(radius)*4.0f;
            const ImmCore::mat4x4 projection(1,0,0,0, 0,1,0,0, 0,0,nearPlane/(farPlane-nearPlane),nearPlane*farPlane/(farPlane-nearPlane), 0,0,-1,0);
            if (graph)
            {
                uint64_t clearCompletion = 0;
                Check(renderer.EndFrame(&clearCompletion), "Submit pre-packet clear");
                ImmRenderGraphPacket request;
                request.operation = 1; request.viewCount = 2;
                request.width = request.height = size;
                request.colorBuffer = reinterpret_cast<uint64_t>(colorBuffer);
                request.depthBuffer = reinterpret_cast<uint64_t>(depthBuffer);
                request.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                request.depthFormat = DXGI_FORMAT_D32_FLOAT;
                const auto head = ImmCore::d2f(ImmCore::toMatrix(view));
                auto left = head, right = head;
                left[3] -= float(radius)*0.1f; right[3] += float(radius)*0.1f;
                for (int i = 0; i < 16; ++i)
                {
                    request.worldToView[i] = head[i];
                    request.leftView[i] = left[i]; request.rightView[i] = right[i];
                    request.projection[i] = request.leftProjection[i] = request.rightProjection[i] = projection[i];
                }
                Check(ProcessImmRenderGraph(*graph, *bridge, request), "Submit stereo graph packet");
                if (!request.gpuCompletion) throw std::runtime_error("Stereo packet returned no GPU completion");
                Check(renderer.BeginFrame(), "Begin post-packet depth probe");
                Check(host.BindTargetsInRenderEvent(colorBuffer, depthBuffer, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 2),
                    "Rebind packet targets for depth probe");
                renderer.SetViewport(0, viewport);
                renderer.SetState(ImmCore::piSTATE_CULL_FACE, false);
                renderer.SetState(ImmCore::piSTATE_BLEND, false);
            }
            else
            {
                scene->GlobalRender(ImmCore::trans3d::identity(), view, projection, ImmPlayer::StereoMode::Preferred);
                scene->RenderStereoSinglePass(ImmCore::ivec2(size, size),
                    ImmCore::toMatrix(ImmCore::trans3d::translate(-radius*0.1, 0.0, 0.0)), projection,
                    ImmCore::toMatrix(ImmCore::trans3d::translate(radius*0.1, 0.0, 0.0)), projection);
            }
            if (scene->GetPerformanceInfoForFrame().numDrawCalls <= 0) throw std::runtime_error("Layered scene submitted no paint");
        }
        else if (pictureFormat) DrawPictureProbe(renderer, *modelLog, 0, pictureFormat, 4, 1.0f, true, size);
        else if (modelLog) DrawModelProbe(renderer, *modelLog, 0, 1.0f, true, size);
        else renderer.DrawUnitQuad_XY(2);
    }
    catch (...)
    {
        ComPtr<ID3D12InfoQueue> diagnostics;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&diagnostics))))
            for (UINT64 i = 0; i < diagnostics->GetNumStoredMessages(); ++i)
            {
                SIZE_T length = 0; diagnostics->GetMessage(i, nullptr, &length);
                std::vector<unsigned char> storage(length);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                if (SUCCEEDED(diagnostics->GetMessage(i, message, &length)) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                    std::fprintf(stderr, "IMM_DX12_LAYERED %.1200s\n", message->pDescription);
            }
        throw;
    }
    renderer.DestroyShader(shader);
    // Later geometry behind each eye must lose against that slice's depth.
    const char* behindVS = "void main(float2 p:POSITION, uint eye:SV_InstanceID, out float4 pos:SV_Position, out uint slice:SV_RenderTargetArrayIndex, out float4 color:COLOR0) { pos=float4(p,0.0001,1); slice=eye; color=float4(0,0,1,1); }";
    auto behind = renderer.CreateShader(nullptr, behindVS, nullptr, nullptr, nullptr, ps, nullptr);
    if (!behind) throw std::runtime_error("Create layered depth probe shader");
    renderer.AttachShader(behind);
    renderer.DrawUnitQuad_XY(2);
    renderer.DestroyShader(behind);
    auto* commands = static_cast<ID3D12GraphicsCommandList*>(renderer.GetContext());
    auto transition = [&](ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier = {}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
        commands->ResourceBarrier(1, &barrier);
    };
    const auto sourceState = samples > 1 ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
    transition(color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
    if (samples > 1)
        for (UINT eye = 0; eye < 2; ++eye)
            commands->ResolveSubresource(resolved.Get(), eye, color.Get(), eye, DXGI_FORMAT_R8G8B8A8_UNORM);
    else commands->CopyResource(resolved.Get(), color.Get());
    transition(color.Get(), sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    transition(resolved.Get(), outputState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    for (UINT eye = 0; eye < 2; ++eye)
    {
        D3D12_TEXTURE_COPY_LOCATION source = {}; source.pResource = resolved.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; source.SubresourceIndex = eye;
        D3D12_TEXTURE_COPY_LOCATION destination = {}; destination.pResource = readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = footprints[eye];
        commands->CopyTextureRegion(&destination, 0,0,0, &source, nullptr);
    }
    uint64_t completion = 0; Check(renderer.EndFrame(&completion), "Submit layered frame");
    Check(renderer.WaitForFrame(completion), "Wait for layered frame");
    void* data = nullptr; D3D12_RANGE range = {0, static_cast<SIZE_T>(bytes)};
    Check(readback->Map(0, &range, &data), "Map layered pixels");
    char capture[80];
    if (graph) std::snprintf(capture, sizeof(capture), "d3d12-layered-packet-%ux.ppm", samples);
    else if (scene) std::snprintf(capture, sizeof(capture), "d3d12-layered-scene-config%d-%ux.ppm", configuration, samples);
    else if (pictureFormat) std::snprintf(capture, sizeof(capture), "d3d12-layered-picture%d-%ux.ppm", pictureFormat, samples);
    else std::snprintf(capture, sizeof(capture), modelLog ? "d3d12-layered-model-%ux.ppm" : "d3d12-layered-%ux.ppm", samples);
    std::ofstream image(capture, std::ios::binary); image << "P6\n" << size << " " << size*2 << "\n255\n";
    UINT scenePixels[2] = {}, differentPixels = 0;
    for (UINT eye = 0; eye < 2; ++eye)
        for (UINT y = 0; y < size; ++y) for (UINT x = 0; x < size; ++x)
        {
            const auto* pixel = static_cast<const unsigned char*>(data) + footprints[eye].Offset + y*footprints[eye].Footprint.RowPitch + x*4;
            if (scene)
            {
                if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 255) ++scenePixels[eye];
                if (eye == 1)
                {
                    const auto* left = static_cast<const unsigned char*>(data) + footprints[0].Offset + y*footprints[0].Footprint.RowPitch + x*4;
                    if (pixel[0] != left[0] || pixel[1] != left[1] || pixel[2] != left[2]) ++differentPixels;
                }
                image.write(reinterpret_cast<const char*>(pixel), 3);
                continue;
            }
            const bool modelCovered = y >= 4 && y < 28 && (eye == 0 ? x < 24 : x >= 8);
            for (UINT channel = 0; channel < 3; ++channel)
            {
                const int expected = modelLog ? (modelCovered ? 55 : (channel == 2 ? 255 : 0)) : (channel == eye ? 255 : 0);
                if (pictureFormat)
                {
                    // Interior samples avoid sphere silhouettes and texture seams.
                    if (x >= 12 && x < 20 && y >= 12 && y < 20)
                    {
                        const int source = pictureFormat == 2 || pictureFormat == 3 ? 160 : 128;
                        const int red = int(std::round(std::pow(source / 255.0, 2.2) * 255.0));
                        const int pictureExpected = pictureFormat == 1 && eye == 1 ? (channel == 1 ? 255 : 0) : (channel == 0 ? red : 0);
                        if (std::abs(int(pixel[channel]) - pictureExpected) > 2)
                            throw std::runtime_error("Layered picture eye selection or depth mismatch");
                    }
                }
                else if (std::abs(int(pixel[channel]) - expected) > (modelLog ? 2 : 0))
                    throw std::runtime_error("Layered eye colour or projection mismatch");
            }
            image.write(reinterpret_cast<const char*>(pixel), 3);
        }
    if (scene && (scenePixels[0] < 100 || scenePixels[1] < 100 || differentPixels < 100))
        throw std::runtime_error("Layered scene missing an eye, per-eye transform or depth writes");
    D3D12_RANGE written = {}; readback->Unmap(0, &written);
    image.close(); if (!image) throw std::runtime_error("Write layered capture");
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
            const char* fixturePath = std::getenv("IMM_D3D12_DOCUMENT");
            const auto documentPath = fixturePath ? std::filesystem::path(fixturePath).wstring() : std::wstring(IMM_D3D12_SAMPLE_FILE);
            const int document = player.Load(documentPath.c_str());
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
            if (int(technique) == 0)
                RenderScene(host, device.Get(), player, document,
                    int(colorSpace) == 0 ? "d3d12-model-linear.ppm" : "d3d12-model-gamma.ppm",
                    DepthProbe::None, int(colorSpace), &log);
            if (int(technique) == 0)
                RenderScene(host, device.Get(), player, document,
                    int(colorSpace) == 0 ? "d3d12-panorama-linear.ppm" : "d3d12-panorama-gamma.ppm",
                    DepthProbe::None, int(colorSpace), &log, 1);
            if (int(technique) == 0)
                for (int layout = 2; layout <= 3; ++layout)
                    for (int face = 0; face < 6; ++face)
                    {
                        char capture[96];
                        std::snprintf(capture, sizeof(capture), "d3d12-cubemap-%s-%s-face%d.ppm",
                            layout == 2 ? "cross" : "strip", int(colorSpace) == 0 ? "linear" : "gamma", face);
                        RenderScene(host, device.Get(), player, document, capture,
                            DepthProbe::None, int(colorSpace), &log, layout, face);
                    }
            if (int(technique) == 0 && int(colorSpace) == 0)
                for (UINT samples : {1u, 2u, 4u, 8u})
                {
                    char capture[80];
                    std::snprintf(capture, sizeof(capture), "d3d12-model-half-opacity-%ux.ppm", samples);
                    RenderScene(host, device.Get(), player, document, capture,
                        DepthProbe::None, int(colorSpace), &log, 0, 4, samples, 0.5f);
                }
            if (int(technique) == 0 && int(colorSpace) == 0)
                for (int pictureFormat = 1; pictureFormat <= 5; ++pictureFormat)
                    for (UINT samples : {1u, 2u, 4u, 8u})
                    {
                        char capture[96];
                        std::snprintf(capture, sizeof(capture), "d3d12-picture-format%d-half-opacity-%ux.ppm", pictureFormat, samples);
                        RenderScene(host, device.Get(), player, document, capture,
                            DepthProbe::None, int(colorSpace), &log, pictureFormat, 4, samples, 0.5f);
                    }
            for (DepthProbe depthProbe : {DepthProbe::None, DepthProbe::HostOcclusion, DepthProbe::ImmWrites})
            {
                char capture[96];
                std::snprintf(capture, sizeof(capture), "d3d12-orthographic-technique%d-color%d-depth%d.ppm",
                    int(technique), int(colorSpace), int(depthProbe));
                RenderScene(host, device.Get(), player, document, capture, depthProbe, -1, nullptr, 0, 4, 8, 1.0f, true);
            }
            for (UINT samples : {1u, 2u, 4u, 8u})
                VerifyLayeredTargets(host, device.Get(), samples, nullptr, 0, &player, document, int(technique)*2+int(colorSpace));
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
        for (UINT samples : {1u, 2u, 4u, 8u}) VerifyLayeredTargets(host, device.Get(), samples);
        for (UINT samples : {1u, 2u, 4u, 8u}) VerifyLayeredTargets(host, device.Get(), samples, &log);
        for (int pictureFormat = 1; pictureFormat <= 5; ++pictureFormat)
            for (UINT samples : {1u, 2u, 4u, 8u})
                VerifyLayeredTargets(host, device.Get(), samples, &log, pictureFormat);
        host.ShutdownInRenderEvent();
        ImmRenderGraphState graph;
        if (!ConfigureImmRenderGraph(graph, &unity)) throw std::runtime_error("Configure graph event");
        ImmRenderGraphPacket packet;
        packet.enableSound = 0;
        packet.version = 99;
        if (ProcessImmRenderGraph(graph, bridge, packet) != E_INVALIDARG || graph.ready)
            throw std::runtime_error("Graph accepted unknown packet ABI");
        packet.version = ImmRenderGraphPacketVersion;
        packet.viewCount = 3;
        if (ProcessImmRenderGraph(graph, bridge, packet) != E_INVALIDARG || graph.ready)
            throw std::runtime_error("Graph accepted unsupported view count");
        packet.viewCount = 1;
        Check(ProcessImmRenderGraph(graph, bridge, packet), "Initialize graph session");
        if (!graph.ready || !bridge.IsGraphicsInitialized()) throw std::runtime_error("Graph session not ready");
        packet.operation = 1;
        if (ProcessImmRenderGraph(graph, bridge, packet) != E_INVALIDARG)
            throw std::runtime_error("Graph accepted missing targets");
        packet.viewCount = 2;
        packet.width = packet.height = 32;
        packet.colorBuffer = packet.depthBuffer = 1; // Must never reach target binding.
        for (float* matrix : {packet.leftView, packet.leftProjection, packet.rightView, packet.rightProjection})
        {
            matrix[0] = std::numeric_limits<float>::quiet_NaN();
            if (ProcessImmRenderGraph(graph, bridge, packet) != E_INVALIDARG)
                throw std::runtime_error("Graph accepted non-finite stereo matrix");
            matrix[0] = 0;
        }
        packet.viewCount = 1;
        packet.colorBuffer = packet.depthBuffer = 0;
        const int maintenanceDocument = bridge.GetPlayer()->Load(IMM_D3D12_SAMPLE_FILE);
        if (maintenanceDocument < 0) throw std::runtime_error("Queue target-free document load");
        for (int unloading = 0; unloading < 2; ++unloading)
        {
            if (unloading) bridge.GetPlayer()->Unload(maintenanceDocument);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            for (;;)
            {
                bridge.GlobalWork(true, 10000);
                packet.operation = 3;
                Check(ProcessImmRenderGraph(graph, bridge, packet), "Target-free GPU maintenance");
                ImmPlayer::Player::DocumentState state;
                bridge.GetPlayer()->GetDocumentState(state, maintenanceDocument);
                if (unloading ? !bridge.GetPlayer()->IsDocumentActive(maintenanceDocument) :
                    state.mLoadingState == ImmPlayer::Player::LoadingState::Loaded) break;
                if (state.mLoadingState == ImmPlayer::Player::LoadingState::Failed || std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("Target-free loading/unloading failed or timed out");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!unloading)
            {
                bridge.GetPlayer()->SetTime(maintenanceDocument, ImmCore::piTick::FromSeconds(3.0), ImmCore::piTick(0));
                bridge.GlobalWork(true, 10000);
                for (UINT samples : {1u, 2u, 4u, 8u})
                    VerifyLayeredTargets(graph.host, device.Get(), samples, nullptr, 0,
                        bridge.GetPlayer(), maintenanceDocument, 0, &graph, &bridge);
            }
        }
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
        result << R"({"status":"pass","api":"D3D12","adapter":"WARP","scope":"player-scene-smoke","configurations_verified":4,"documents_loaded":4,"scene_frames_verified":12,"orthographic_frames_verified":12,"layered_target_frames_verified":4,"layered_model_frames_verified":4,"layered_picture_frames_verified":20,"layered_scene_frames_verified":16,"layered_packet_frames_verified":4,"model_frames_verified":2,"model_half_opacity_sample_counts_verified":4,"picture_half_opacity_frames_verified":20,"panorama_frames_verified":2,"cubemap_frames_verified":24,"host_depth_frames_verified":4,"imm_depth_write_frames_verified":4,"msaa_samples":8,"unity_queue_event_contract_mocked":true,"unity_target_binding_mocked":true,"borrowed_renderer_lifecycle_verified":true,"render_graph_packet_lifecycle_verified":true,"target_free_maintenance_verified":true,"debug_layer_enabled":true,"imm_scene_renderer":false})";
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
