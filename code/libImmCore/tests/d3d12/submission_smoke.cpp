#include "../../src/libRender/directx12/piDX12_CommandContext.h"
#include "../../src/libRender/directx12/piDX12_ShaderBindings.h"
#include "../../src/libRender/directx12/piDX12_Renderer.h"

#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

using Microsoft::WRL::ComPtr;
using ImmCore::piDX12CommandContext;

namespace
{
constexpr UINT Width = 64;
constexpr UINT Height = 64;
constexpr UINT Frames = 9; // Three complete allocator-ring rotations.

void Check(HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::fprintf(stderr, "IMM_DX12_PHASE1 %s failed: 0x%08lx\n", operation,
                     static_cast<unsigned long>(result));
        throw std::runtime_error(operation);
    }
}

void Require(bool condition, const char* description)
{
    if (!condition) throw std::runtime_error(description);
}

D3D12_HEAP_PROPERTIES Heap(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = type;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    return heap;
}

ComPtr<ID3D12Resource> Texture(ID3D12Device* device, DXGI_FORMAT format,
                              D3D12_RESOURCE_FLAGS flags)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = Width;
    desc.Height = Height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Flags = flags;
    const auto heap = Heap(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> texture;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture)), "Create texture");
    return texture;
}

ComPtr<ID3D12DescriptorHeap> Descriptors(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = Frames;
    ComPtr<ID3D12DescriptorHeap> heap;
    Check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "Create descriptors");
    return heap;
}

ComPtr<ID3DBlob> Shader(const char* source, const char* entry, const char* target)
{
    ComPtr<ID3DBlob> binary;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(source, std::strlen(source), "submission_smoke", nullptr,
        nullptr, entry, target, D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &binary, &errors);
    if (FAILED(result) && errors)
        std::fprintf(stderr, "IMM_DX12_PHASE1 shader: %.1024s\n",
                     static_cast<const char*>(errors->GetBufferPointer()));
    Check(result, "Compile smoke shader");
    return binary;
}

void CheckDebugMessages(ID3D12InfoQueue* messages)
{
    if (!messages) return;
    for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i)
    {
        SIZE_T length = 0;
        Check(messages->GetMessage(i, nullptr, &length), "Get debug message length");
        std::vector<unsigned char> storage(length);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        Check(messages->GetMessage(i, message, &length), "Get debug message");
        if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
        {
            std::fprintf(stderr, "IMM_DX12_PHASE1 validation: %.1024s\n", message->pDescription);
            throw std::runtime_error("D3D12 debug layer reported an error");
        }
    }
}

void CheckTextureUploads(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    piDX12CommandContext context;
    Check(context.Initialize(device, queue), "Initialize texture upload context");
    for (const auto format : {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_BC1_UNORM})
    {
        D3D12_RESOURCE_DESC description = {};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = format == DXGI_FORMAT_BC1_UNORM ? 12 : 7;
        description.Height = format == DXGI_FORMAT_BC1_UNORM ? 12 : 5;
        description.DepthOrArraySize = 2;
        description.MipLevels = 2;
        description.Format = format;
        description.SampleDesc.Count = 1;
        constexpr UINT count = 4;
        std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, count> footprints;
        std::array<UINT, count> rows;
        std::array<UINT64, count> rowBytes;
        UINT64 totalBytes = 0;
        device->GetCopyableFootprints(&description, 0, count, 0,
            footprints.data(), rows.data(), rowBytes.data(), &totalBytes);
        std::array<std::vector<unsigned char>, count> pixels;
        std::array<piDX12CommandContext::TextureData, count> sources;
        for (UINT i = 0; i < count; ++i)
        {
            // Deliberately padded CPU rows differ from D3D12's 256-byte pitch.
            const size_t pitch = static_cast<size_t>(rowBytes[i]) + 13;
            pixels[i].resize(pitch * rows[i], 0xcd);
            for (UINT row = 0; row < rows[i]; ++row)
                for (size_t column = 0; column < rowBytes[i]; ++column)
                    pixels[i][row * pitch + column] = static_cast<unsigned char>(i * 41 + row * 7 + column);
            sources[i] = {pixels[i].data(), pixels[i].size(), pitch};
        }
        ID3D12GraphicsCommandList* commands = nullptr;
        Check(context.Begin(&commands), "Begin texture upload");
        ComPtr<ID3D12Resource> texture;
        auto invalid = sources;
        invalid[1].bytes = 1;
        Require(context.UploadTexture2D(description, invalid.data(), count, &texture) == E_INVALIDARG && !texture,
                "Undersized mip data accepted");
        Require(context.UploadTexture2D(description, sources.data(), count - 1, &texture) == E_INVALIDARG,
                "Missing array/mip data accepted");
        Check(context.UploadTexture2D(description, sources.data(), count, &texture), "Upload texture array mips");
        // Overwrite the sources before submission to prove Upload copied CPU data.
        for (auto& data : pixels) std::memset(data.data(), 0xee, data.size());
        D3D12_RESOURCE_DESC readbackDescription = {};
        readbackDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        readbackDescription.Width = totalBytes;
        readbackDescription.Height = 1;
        readbackDescription.DepthOrArraySize = 1;
        readbackDescription.MipLevels = 1;
        readbackDescription.SampleDesc.Count = 1;
        readbackDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto readbackHeap = Heap(D3D12_HEAP_TYPE_READBACK);
        ComPtr<ID3D12Resource> readback;
        Check(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDescription,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)), "Create texture readback");
        Check(context.Retain(readback.Get()), "Retain texture readback");
        Check(context.Transition(texture.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_COPY_SOURCE), "Transition texture for readback");
        for (UINT i = 0; i < count; ++i)
        {
            D3D12_TEXTURE_COPY_LOCATION source = {};
            source.pResource = texture.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source.SubresourceIndex = i;
            D3D12_TEXTURE_COPY_LOCATION target = {};
            target.pResource = readback.Get();
            target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            target.PlacedFootprint = footprints[i];
            commands->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
        }
        texture.Reset();
        uint64_t completion = 0;
        Check(context.Submit(&completion), "Submit texture copies");
        Check(context.Wait(completion), "Wait for texture readback");
        const D3D12_RANGE range = {0, static_cast<size_t>(totalBytes)};
        unsigned char* mapped = nullptr;
        Check(readback->Map(0, &range, reinterpret_cast<void**>(&mapped)), "Map texture readback");
        for (UINT i = 0; i < count; ++i)
            for (UINT row = 0; row < rows[i]; ++row)
                for (size_t column = 0; column < rowBytes[i]; ++column)
                    Require(mapped[footprints[i].Offset + size_t(row) * footprints[i].Footprint.RowPitch + column] ==
                        static_cast<unsigned char>(i * 41 + row * 7 + column), "Texture mip/array/row data mismatch");
        const D3D12_RANGE noWrites = {0, 0};
        readback->Unmap(0, &noWrites);
    }
    Check(context.Shutdown(), "Drain texture uploads");
}

void CheckRendererBuffers(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    ImmCore::piRendererDX12 renderer;
    Check(renderer.InitializeExternal(device, queue) ? S_OK : E_FAIL, "Initialize renderer adapter");
    ImmCore::piRenderer& api = renderer;
    const char* rendererVS = R"(
        cbuffer Draw : register(b0) { float4 color; float depth; };
        float4 main(uint id : SV_VertexID) : SV_Position {
            const float2 p[3] = {float2(-0.75,-0.75),float2(0,0.75),float2(0.75,-0.75)};
            return float4(p[id],depth,1);
        })";
    const char* rendererPS = R"(
        cbuffer Draw : register(b0) { float4 color; float depth; };
        StructuredBuffer<float4> tint : register(t8);
        Texture2D image : register(t0);
        SamplerState imageSampler : register(s0);
        float4 main() : SV_Target { return color * tint[0] * image.SampleLevel(imageSampler,float2(0.25,0.25),1); })";
    auto shader = api.CreateShader(nullptr, rendererVS, nullptr, nullptr, nullptr, rendererPS, nullptr);
    Require(shader != nullptr, "piRenderer shader creation failed");
    const char* indexedVS = R"(
        cbuffer Draw : register(b0) { float4 color; float depth; };
        float4 main(float2 position : POSITION, float2 offset : OFFSET) : SV_Position {
            return float4(position + offset,depth,1);
        })";
    auto indexedShader = api.CreateShader(nullptr, indexedVS, nullptr, nullptr, nullptr, rendererPS, nullptr);
    Require(indexedShader != nullptr, "Indexed shader creation failed");
    const float vertices[][2] = {{200,200},{-0.75f,-0.75f},{0,0.75f},{0.75f,-0.75f}};
    const float offsets[][2] = {{200,200},{0,0}};
    const uint16_t indices16[] = {0,0,0,2,1,0};
    const uint32_t indices32[] = {0,0,0,2,1,0};
    using Renderer = ImmCore::piRenderer;
    const char* quadVS = R"(
        cbuffer Draw : register(b0) { float4 color; float depth; };
        float4 main(float2 position : POSITION) : SV_Position { return float4(position,depth,1); })";
    auto quadShader = api.CreateShader(nullptr, quadVS, nullptr, nullptr, nullptr, rendererPS, nullptr);
    const char* cubePS = R"(
        cbuffer Draw : register(b0) { float4 color; float depth; };
        TextureCube image : register(t1);
        SamplerState imageSampler : register(s1);
        float4 main() : SV_Target {
            const float3 directions[6] = {float3(1,0,0),float3(-1,0,0),float3(0,1,0),
                float3(0,-1,0),float3(0,0,1),float3(0,0,-1)};
            return image.SampleLevel(imageSampler,directions[int(color.x)],0);
        })";
    auto cubeShader = api.CreateShader(nullptr, quadVS, nullptr, nullptr, nullptr, cubePS, nullptr);
    const unsigned char cubeColors[][4] = {{255,0,0,255},{0,255,0,255},{0,0,255,255},
        {255,255,0,255},{255,0,255,255},{0,255,255,255}};
    Renderer::TextureInfo cubeInfo = {Renderer::TextureType::TCUBE, Renderer::Format::C4_8_UNORM, 1, 1, 1, 1, 1, 0};
    auto cubeTexture = api.CreateTexture(nullptr, &cubeInfo, false, Renderer::TextureFilter::NONE,
        Renderer::TextureWrap::CLAMP, 1, cubeColors);
    Require(cubeShader && cubeTexture, "Renderer cube creation failed");
    api.AttachTextures(1, &cubeTexture, 1);
    auto rasterState = api.CreateRasterState(false, true, Renderer::CullMode::NONE, false, false);
    auto depthState = api.CreateDepthState(true, false);
    auto noDepthState = api.CreateDepthState(false, false);
    auto opaqueState = api.CreateBlendState(false, false);
    auto blendState = api.CreateBlendState(false, true);
    Require(quadShader && rasterState && depthState && noDepthState && opaqueState && blendState,
            "Renderer draw state creation failed");
    const unsigned char imagePixels[] = {0,0,0,255, 255,255,255,255, 255,255,255,255, 0,0,0,255};
    Renderer::TextureInfo imageInfo = {Renderer::TextureType::T2D, Renderer::Format::C4_8_UNORM, 2, 2, 1, 1, 0, 0};
    auto sampledImage = api.CreateTexture(nullptr, &imageInfo, false, Renderer::TextureFilter::MIPMAP,
        Renderer::TextureWrap::CLAMP, 1, imagePixels);
    Require(sampledImage != nullptr, "Renderer sampled texture creation failed");
    Renderer::TextureInfo actualInfo = {};
    api.GetTextureInfo(sampledImage, &actualInfo);
    Require(actualInfo.mNumMips == 2, "Renderer did not generate the requested mip chain");
    api.AttachTextures(1, &sampledImage, 0);
    auto imageSampler = api.CreateSampler(Renderer::TextureFilter::MIPMAP, Renderer::TextureWrap::CLAMP, 1);
    Require(imageSampler != nullptr, "Renderer sampler creation failed");
    api.AttachSamplers(1, imageSampler, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    auto vertexBuffer = api.CreateBuffer(vertices, sizeof(vertices), Renderer::BufferType::Static, Renderer::BufferUse::Vertex);
    auto instanceBuffer = api.CreateBuffer(offsets, sizeof(offsets), Renderer::BufferType::Static, Renderer::BufferUse::Vertex);
    ImmCore::piBuffer indexBuffers[] = {
        api.CreateBuffer(indices16, sizeof(indices16), Renderer::BufferType::Static, Renderer::BufferUse::Index),
        api.CreateBuffer(indices32, sizeof(indices32), Renderer::BufferType::Static, Renderer::BufferUse::Index)};
    Renderer::ArrayLayout2 vertexLayout = {}, instanceLayout = {};
    vertexLayout.mNumElements = instanceLayout.mNumElements = 1;
    std::memcpy(vertexLayout.mEntry[0].mName, "POSITION", sizeof("POSITION"));
    std::memcpy(instanceLayout.mEntry[0].mName, "OFFSET", sizeof("OFFSET"));
    vertexLayout.mEntry[0].mFormat = instanceLayout.mEntry[0].mFormat = Renderer::Format::C2_32_FLOAT;
    instanceLayout.mEntry[0].mPerInstance = true;
    ImmCore::piVertexArray arrays[2];
    for (UINT i = 0; i < 2; ++i)
    {
        arrays[i] = api.CreateVertexArray2(2, vertexBuffer, &vertexLayout, instanceBuffer, &instanceLayout,
            nullptr, 0, indexBuffers[i], i == 0 ? Renderer::IndexArrayFormat::UINT_16 : Renderer::IndexArrayFormat::UINT_32);
        Require(arrays[i] != nullptr, "Indexed vertex layout creation failed");
    }
    api.AttachShader(shader);
    auto drawConstants = api.CreateBuffer(nullptr, 20, ImmCore::piRenderer::BufferType::Dynamic,
        ImmCore::piRenderer::BufferUse::Constant);
    const float white[] = {1, 1, 1, 1};
    auto tint = api.CreateStructuredBuffer(white, 1, sizeof(white), ImmCore::piRenderer::BufferType::Static,
        ImmCore::piRenderer::BufferUse::ShaderResource);
    Require(drawConstants && tint, "piRenderer draw resource creation failed");
    api.AttachShaderConstants(drawConstants, 0);
    api.AttachShaderBuffer(tint, 8);
    const auto rtvHeap = Descriptors(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const auto dsvHeap = Descriptors(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    std::array<ComPtr<ID3D12Resource>, Frames> images;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, Frames> imageFootprints;
    const uint32_t original[] = {101, 102, 103, 104};
    auto buffer = api.CreateBuffer(original, sizeof(original), ImmCore::piRenderer::BufferType::Dynamic,
        ImmCore::piRenderer::BufferUse::Constant);
    Require(buffer != nullptr && renderer.BufferResource(buffer)->GetDesc().Width == 256,
            "Renderer constant buffer alignment failed");
    std::array<ComPtr<ID3D12Resource>, Frames> readbacks;
    uint64_t lastCompletion = 0;
    for (UINT frame = 0; frame < Frames; ++frame)
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeof(original) * 2;
        desc.Height = 1;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto heap = Heap(D3D12_HEAP_TYPE_READBACK);
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbacks[frame])), "Create version readback");
        auto color = Texture(device, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        auto depth = Texture(device, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        auto dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += frame * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsv.ptr += frame * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        device->CreateRenderTargetView(color.Get(), nullptr, rtv);
        device->CreateDepthStencilView(depth.Get(), nullptr, dsv);
        const auto colorDescription = color->GetDesc();
        device->GetCopyableFootprints(&colorDescription, 0, 1, 0, &imageFootprints[frame], nullptr, nullptr, &desc.Width);
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&images[frame])), "Create renderer image readback");
        Check(renderer.BeginFrame(), "Begin renderer frame");
        Require(renderer.BeginFrame() == E_UNEXPECTED, "Renderer accepted nested frame");
        auto* commands = static_cast<ID3D12GraphicsCommandList*>(api.GetContext());
        Require(commands != nullptr, "Nested frame failure lost active command list");
        ImmCore::piRendererDX12::ExternalTarget target;
        target.color = color.Get(); target.depth = depth.Get(); target.rtv = rtv; target.dsv = dsv;
        target.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM; target.depthFormat = DXGI_FORMAT_D32_FLOAT;
        Check(renderer.SetExternalTarget(target), "Set renderer host attachments");
        const float blue[] = {0, 0, 1, 1};
        api.Clear(blue, nullptr, nullptr, nullptr, true);
        api.SetRasterState(rasterState);
        api.SetDepthState(depthState);
        api.SetBlendState(opaqueState);
        const float nearData[] = {float(frame % 2), 1, 0, 1, 0.8f};
        api.UpdateBuffer(drawConstants, nearData, 0, sizeof(nearData), false);
        api.AttachShader(indexedShader);
        api.AttachVertexArray2(arrays[frame % 2]);
        api.DrawPrimitiveIndexed(Renderer::PrimitiveType::Triangle, 3, 1, 1, 1, 3);
        const float farData[] = {1, 0, 0, 1, 0.2f};
        api.UpdateBuffer(drawConstants, farData, 0, sizeof(farData), false);
        api.AttachShader(shader);
        api.DettachVertexArray();
        api.DrawPrimitiveNotIndexed(ImmCore::piRenderer::PrimitiveType::Triangle, 0, 3, 1);
        const int cornerViewport[] = {48, 0, 16, 16};
        const D3D12_RECT cornerRect = {48, 0, 64, 16};
        commands->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 1, &cornerRect);
        api.SetViewport(0, cornerViewport);
        api.SetDepthState(noDepthState);
        api.SetBlendState(blendState);
        api.SetBlending(0, Renderer::BlendEquation::piBLEND_ADD, Renderer::BlendOperations::piBLEND_ONE,
            Renderer::BlendOperations::piBLEND_ONE, Renderer::BlendEquation::piBLEND_ADD,
            Renderer::BlendOperations::piBLEND_ONE, Renderer::BlendOperations::piBLEND_ONE);
        api.AttachShader(quadShader);
        api.DrawUnitQuad_XY(1);
        const int cubeViewport[] = {0, 48, 16, 16};
        api.SetViewport(0, cubeViewport);
        api.SetBlendState(opaqueState);
        api.AttachShader(cubeShader);
        const float cubeConstants[] = {float(frame % 6), 0, 0, 1, 0.2f};
        api.UpdateBuffer(drawConstants, cubeConstants, 0, sizeof(cubeConstants), false);
        api.DrawUnitQuad_XY(1);
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = color.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        commands->ResourceBarrier(1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION imageSource = {};
        imageSource.pResource = color.Get();
        imageSource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION imageTarget = {};
        imageTarget.pResource = images[frame].Get();
        imageTarget.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        imageTarget.PlacedFootprint = imageFootprints[frame];
        commands->CopyTextureRegion(&imageTarget, 0, 0, 0, &imageSource, nullptr);
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commands->ResourceBarrier(1, &barrier);
        for (UINT version = 0; version < 2; ++version)
        {
            const uint32_t value = frame + version * 1000;
            api.UpdateBuffer(buffer, &value, sizeof(uint32_t), sizeof(value), false);
            api.AttachShaderConstants(buffer, 3);
            commands->CopyBufferRegion(readbacks[frame].Get(), version * sizeof(original),
                renderer.BufferResource(buffer), 0, sizeof(original));
        }
        if (frame + 1 == Frames)
        {
            api.DestroyBuffer(buffer);
            api.DestroyBuffer(drawConstants);
            api.DestroyBuffer(tint);
            api.DestroyShader(shader);
            api.DestroyShader(indexedShader);
            for (auto array : arrays) api.DestroyVertexArray2(array);
            api.DestroyBuffer(vertexBuffer);
            api.DestroyBuffer(instanceBuffer);
            for (auto index : indexBuffers) api.DestroyBuffer(index);
            api.DestroyTexture(sampledImage);
            api.DestroySampler(imageSampler);
            api.DestroyShader(quadShader);
            api.DestroyShader(cubeShader);
            api.DestroyTexture(cubeTexture);
            api.DestroyRasterState(rasterState);
            api.DestroyDepthState(depthState);
            api.DestroyDepthState(noDepthState);
            api.DestroyBlendState(opaqueState);
            api.DestroyBlendState(blendState);
        }
        Check(renderer.EndFrame(&lastCompletion), "Submit renderer buffer versions");
    }
    Check(renderer.WaitForFrame(lastCompletion), "Wait for renderer buffer versions");
    renderer.Deinitialize();
    for (UINT frame = 0; frame < Frames; ++frame)
    {
        uint32_t* words = nullptr;
        const D3D12_RANGE readRange = {0, sizeof(original) * 2};
        Check(readbacks[frame]->Map(0, &readRange, reinterpret_cast<void**>(&words)), "Map buffer versions");
        for (UINT version = 0; version < 2; ++version)
        {
            const auto* data = words + version * 4;
            Require(data[0] == 101 && data[1] == frame + version * 1000 && data[2] == 103 && data[3] == 104,
                    "piRenderer buffer update changed an earlier GPU version or untouched bytes");
        }
        const D3D12_RANGE noWrites = {0, 0};
        readbacks[frame]->Unmap(0, &noWrites);
        const UINT pitch = imageFootprints[frame].Footprint.RowPitch;
        const D3D12_RANGE imageRange = {0, size_t(pitch) * Height};
        unsigned char* pixels = nullptr;
        Check(images[frame]->Map(0, &imageRange, reinterpret_cast<void**>(&pixels)), "Map piRenderer image");
        const auto* center = pixels + (Height / 2) * pitch + (Width / 2) * 4;
        Require(center[0] == (frame % 2 ? 128 : 0) && center[1] == 128 && center[2] == 0,
                "piRenderer draw constants/depth failed");
        Require(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 255, "piRenderer clear failed");
        const auto* corner = pixels + 8 * pitch + 56 * 4;
        Require(corner[0] == 128 && corner[1] == 0 && corner[2] == 255,
                "piRenderer depth-disable/additive blend/unit quad failed");
        const auto* cubePixel = pixels + 56 * pitch + 8 * 4;
        Require(std::memcmp(cubePixel, cubeColors[frame % 6], 4) == 0, "piRenderer cube face selection failed");
        images[frame]->Unmap(0, &noWrites);
    }
}

int Run()
{
    ComPtr<ID3D12Debug> debug;
    const bool debugEnabled = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    if (debugEnabled) debug->EnableDebugLayer();

    ComPtr<IDXGIFactory4> factory;
    Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "Create DXGI factory");
    ComPtr<IDXGIAdapter> adapter;
    Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "Select WARP adapter");
    ComPtr<ID3D12Device> device;
    Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
          "Create D3D12 device (no fallback)");
    ComPtr<ID3D12InfoQueue> messages;
    if (debugEnabled) Check(device.As(&messages), "Get D3D12 info queue");

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "Create direct queue");
    piDX12CommandContext context;
    Require(context.Initialize(nullptr, queue.Get()) == E_INVALIDARG, "Null device accepted");
    D3D12_COMMAND_QUEUE_DESC copyDesc = {};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    ComPtr<ID3D12CommandQueue> copyQueue;
    Check(device->CreateCommandQueue(&copyDesc, IID_PPV_ARGS(&copyQueue)), "Create copy queue");
    Require(context.Initialize(device.Get(), copyQueue.Get()) == E_INVALIDARG,
            "Non-graphics queue accepted");
    Check(context.Initialize(device.Get(), queue.Get()), "Initialize context");
    Require(context.Initialize(device.Get(), queue.Get()) == E_UNEXPECTED, "Double initialization accepted");
    uint64_t completion = 0;
    Require(context.Submit(&completion) == E_UNEXPECTED, "Submit without Begin accepted");
    ComPtr<ID3D12Resource> invalidUpload;
    const uint32_t uploadValue = 42;
    Require(context.UploadBuffer(&uploadValue, sizeof(uploadValue), &invalidUpload) == E_UNEXPECTED,
            "Upload outside recording accepted");
    ID3D12GraphicsCommandList* commands = nullptr;
    Check(context.Begin(&commands), "Begin cancelled frame");
    ID3D12GraphicsCommandList* duplicate = nullptr;
    Require(context.Begin(&duplicate) == E_UNEXPECTED && duplicate == nullptr, "Nested Begin accepted");
    Require(context.UploadBuffer(nullptr, sizeof(uploadValue), &invalidUpload) == E_INVALIDARG && !invalidUpload,
            "Null upload data accepted");
    Require(context.UploadBuffer(&uploadValue, 0, &invalidUpload) == E_INVALIDARG && !invalidUpload,
            "Empty upload accepted");
    Check(context.UploadBuffer(&uploadValue, sizeof(uploadValue), &invalidUpload), "Upload before cancellation");
    Check(context.Cancel(), "Cancel frame");
    invalidUpload.Reset();
    Require(context.Wait(1, 0) == E_INVALIDARG, "Unsubmitted fence accepted");

    D3D12_ROOT_PARAMETER parameters[2] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[1].Descriptor.ShaderRegister = 8; // IMM paint's structured-buffer slot.
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signatureDesc = {};
    signatureDesc.NumParameters = 2;
    signatureDesc.pParameters = parameters;
    signatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    Check(D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &errors), "Serialize root signature");
    ComPtr<ID3D12RootSignature> signature;
    Check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&signature)), "Create root signature");
    const char* source = R"(
        cbuffer Draw : register(b0) { float4 color; float depth; };
        StructuredBuffer<float4> tint : register(t8);
        float4 VS(float2 position : POSITION) : SV_Position {
            return float4(position, depth, 1);
        }
        float4 PS() : SV_Target { return color * tint[0]; }
    )";
    const auto vs = Shader(source, "VS", "vs_5_0");
    const auto ps = Shader(source, "PS", "ps_5_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = signature.Get();
    pipelineDesc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pipelineDesc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    const D3D12_INPUT_ELEMENT_DESC position = {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,
        0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
    pipelineDesc.InputLayout = {&position, 1};
    pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineDesc.SampleMask = UINT_MAX;
    pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
    pipelineDesc.DepthStencilState.DepthEnable = TRUE;
    pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineDesc.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> pipeline;
    Check(device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline)), "Create pipeline");

    ImmCore::piDX12ShaderBindings pictureBindings;
    Check(pictureBindings.Initialize(device.Get()), "Initialize IMM shader bindings");
    const D3D_SHADER_MACRO pictureOptions[] = {{"STEREOMODE", "0"}, {"FORMAT_IS_STEREO", "0"},
        {"COLOR_SPACE", "1"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> pictureVS, picturePS, pictureErrors;
    auto compilePicture = [&](const wchar_t* path, const char* target, ID3DBlob** binary) {
        const HRESULT result = D3DCompileFromFile(path, pictureOptions, nullptr, "main", target,
            D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, binary, &pictureErrors);
        if (FAILED(result) && pictureErrors)
            std::fprintf(stderr, "IMM_DX12_PHASE1 picture shader: %.1024s\n",
                static_cast<const char*>(pictureErrors->GetBufferPointer()));
        Check(result, "Compile production picture shader");
    };
    compilePicture(IMM_PICTURE_SHADER_DIRECTORY L"shader_pi2D_vs.hlsl", "vs_5_0", &pictureVS);
    compilePicture(IMM_PICTURE_SHADER_DIRECTORY L"shader_pi2D_fs.hlsl", "ps_5_0", &picturePS);
    auto picturePipelineDesc = pipelineDesc;
    picturePipelineDesc.pRootSignature = pictureBindings.RootSignature();
    picturePipelineDesc.VS = {pictureVS->GetBufferPointer(), pictureVS->GetBufferSize()};
    picturePipelineDesc.PS = {picturePS->GetBufferPointer(), picturePS->GetBufferSize()};
    ComPtr<ID3D12PipelineState> picturePipeline;
    Check(device->CreateGraphicsPipelineState(&picturePipelineDesc, IID_PPV_ARGS(&picturePipeline)),
        "Create production picture pipeline");

    const auto rtvHeap = Descriptors(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const auto dsvHeap = Descriptors(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    std::array<ComPtr<ID3D12Resource>, Frames> readbacks;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, Frames> footprints = {};
    for (UINT frame = 0; frame < Frames; ++frame)
    {
        auto color = Texture(device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        auto depth = Texture(device.Get(), DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        auto dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += frame * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsv.ptr += frame * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        device->CreateRenderTargetView(color.Get(), nullptr, rtv);
        device->CreateDepthStencilView(depth.Get(), nullptr, dsv);
        const auto colorDesc = color->GetDesc();
        UINT64 readbackSize = 0;
        device->GetCopyableFootprints(&colorDesc, 0, 1, 0, &footprints[frame], nullptr, nullptr, &readbackSize);
        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = readbackSize;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto readbackHeap = Heap(D3D12_HEAP_TYPE_READBACK);
        Check(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbacks[frame])), "Create readback");

        Check(context.Begin(&commands), "Begin frame");
        // Deliberately destroy the CPU data and caller's COM references before
        // Submit. Only the context keeps these copied buffers alive for the GPU.
        D3D12_GPU_VIRTUAL_ADDRESS nearAddress = 0, farAddress = 0, tintAddress = 0;
        D3D12_VERTEX_BUFFER_VIEW vertexView = {};
        D3D12_INDEX_BUFFER_VIEW indexView = {};
        {
            const float vertices[][2] = {{-0.75f, -0.75f}, {0, 0.75f}, {0.75f, -0.75f}};
            const uint16_t indices[] = {2, 1, 0};
            struct DrawData
            {
                float color[4];
                float depth;
                float padding[59];
            };
            static_assert(sizeof(DrawData) == D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            const DrawData nearData = {{1, 1, 1, 1}, 0.8f, {}};
            const DrawData farData = {{1, 0, 0, 1}, 0.2f, {}};
            const float tint[] = {static_cast<float>(frame % 2), 1, 0, 1};
            ComPtr<ID3D12Resource> vertexBuffer, indexBuffer, nearBuffer, farBuffer, tintBuffer;
            Check(context.UploadBuffer(vertices, sizeof(vertices), &vertexBuffer), "Upload vertices");
            Check(context.UploadBuffer(indices, sizeof(indices), &indexBuffer), "Upload indices");
            Check(context.UploadBuffer(&nearData, sizeof(nearData), &nearBuffer), "Upload near constants");
            Check(context.UploadBuffer(&farData, sizeof(farData), &farBuffer), "Upload far constants");
            Check(context.UploadBuffer(tint, sizeof(tint), &tintBuffer), "Upload structured data");
            vertexView = {vertexBuffer->GetGPUVirtualAddress(), sizeof(vertices), sizeof(vertices[0])};
            indexView = {indexBuffer->GetGPUVirtualAddress(), sizeof(indices), DXGI_FORMAT_R16_UINT};
            nearAddress = nearBuffer->GetGPUVirtualAddress();
            farAddress = farBuffer->GetGPUVirtualAddress();
            tintAddress = tintBuffer->GetGPUVirtualAddress();
        }
        Check(context.Retain(pipeline.Get()), "Retain pipeline");
        Check(context.Retain(signature.Get()), "Retain signature");
        Check(context.Retain(rtvHeap.Get()), "Retain RTV heap");
        Check(context.Retain(dsvHeap.Get()), "Retain DSV heap");
        Check(context.Retain(readbacks[frame].Get()), "Retain readback");
        Check(context.Transition(color.Get(), D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_RENDER_TARGET), "Transition color");
        Check(context.Transition(depth.Get(), D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_DEPTH_WRITE), "Transition depth");
        const float blue[] = {0, 0, 1, 1};
        commands->ClearRenderTargetView(rtv, blue, 0, nullptr);
        commands->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
        commands->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(Width), static_cast<float>(Height), 0, 1};
        D3D12_RECT scissor = {0, 0, Width, Height};
        commands->RSSetViewports(1, &viewport);
        commands->RSSetScissorRects(1, &scissor);
        commands->SetGraphicsRootSignature(signature.Get());
        commands->SetPipelineState(pipeline.Get());
        commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // Different near colours prove per-submission data survives ring reuse.
        commands->IASetVertexBuffers(0, 1, &vertexView);
        commands->IASetIndexBuffer(&indexView);
        commands->SetGraphicsRootShaderResourceView(1, tintAddress);
        commands->SetGraphicsRootConstantBufferView(0, nearAddress);
        commands->DrawIndexedInstanced(3, 1, 0, 0, 0);
        // Draw production IMM picture shaders in a separate viewport, preserving
        // the centre pixel's independent geometry/structured-buffer/depth check.
        {
            ImmCore::piDX12ShaderBindings::Resources bindings;
            std::array<ComPtr<ID3D12Resource>, 10> constants;
            for (UINT slot : {0u, 3u, 4u, 9u})
            {
                float data[64] = {};
                if (slot == 3 || slot == 4)
                    for (UINT diagonal : {0u, 5u, 10u, 15u}) data[diagonal] = 1;
                if (slot == 3) { data[11] = 0.9f; data[17] = 1; } // layer depth, opacity
                if (slot == 9) { data[0] = 1; data[1] = 1; } // picture size
                Check(context.UploadBuffer(data, sizeof(data), &constants[slot]), "Upload picture constants");
                bindings.constants[slot] = constants[slot].Get();
            }
            D3D12_RESOURCE_DESC imageDescription = {};
            imageDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            imageDescription.Width = imageDescription.Height = 1;
            imageDescription.DepthOrArraySize = imageDescription.MipLevels = 1;
            imageDescription.SampleDesc.Count = 1;
            imageDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            const unsigned char texel[] = {255, static_cast<unsigned char>(frame % 2 ? 255 : 0), 255, 255};
            const piDX12CommandContext::TextureData pictureSource = {texel, sizeof(texel), sizeof(texel)};
            ComPtr<ID3D12Resource> picture;
            Check(context.UploadTexture2D(imageDescription, &pictureSource, 1, &picture), "Upload picture texture");
            bindings.resources[0] = picture.Get();
            bindings.views[0].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bindings.views[0].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            bindings.views[0].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            bindings.views[0].Texture2D.MipLevels = 1;
            Check(context.Retain(picturePipeline.Get()), "Retain picture pipeline");
            Check(pictureBindings.Bind(context, commands, bindings), "Bind production picture resources");
            commands->SetPipelineState(picturePipeline.Get());
            const D3D12_VIEWPORT pictureViewport = {48, 0, 16, 16, 0, 1};
            commands->RSSetViewports(1, &pictureViewport);
            commands->DrawIndexedInstanced(3, 1, 0, 0, 0);
            commands->RSSetViewports(1, &viewport);
            commands->SetGraphicsRootSignature(signature.Get());
            commands->SetPipelineState(pipeline.Get());
            commands->SetGraphicsRootShaderResourceView(1, tintAddress);
        }
        // Submitted later, but must fail reversed-Z depth testing.
        commands->SetGraphicsRootConstantBufferView(0, farAddress);
        commands->DrawIndexedInstanced(3, 1, 0, 0, 0);
        Check(context.Transition(color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE), "Transition readback source");
        D3D12_TEXTURE_COPY_LOCATION destination = {};
        destination.pResource = readbacks[frame].Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprints[frame];
        D3D12_TEXTURE_COPY_LOCATION sourceLocation = {};
        sourceLocation.pResource = color.Get();
        sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        commands->CopyTextureRegion(&destination, 0, 0, 0, &sourceLocation, nullptr);
        Check(context.Transition(color.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_COMMON), "Return color to host state");
        Check(context.Transition(depth.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_COMMON), "Return depth to host state");
        Check(context.Submit(&completion), "Submit frame");
        Require(completion == frame + 1, "Fence values are not sequential");
        // color/depth are deliberately released here. Context owns their GPU lifetime.
    }
    Check(context.Wait(completion), "Wait for GPU readback");
    for (UINT frame = 0; frame < Frames; ++frame)
    {
        const UINT pitch = footprints[frame].Footprint.RowPitch;
        D3D12_RANGE readRange = {0, static_cast<SIZE_T>(pitch) * Height};
        unsigned char* pixels = nullptr;
        Check(readbacks[frame]->Map(0, &readRange, reinterpret_cast<void**>(&pixels)), "Map readback");
        const auto* center = pixels + (Height / 2) * pitch + (Width / 2) * 4;
        Require(center[0] == (frame % 2 ? 255 : 0) && center[1] == 255 && center[2] == 0,
                "Triangle colour/depth or submission isolation failed");
        Require(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 255, "Clear colour failed");
        const auto* picturePixel = pixels + 8 * pitch + 56 * 4;
        Require(picturePixel[0] == 255 && picturePixel[1] == (frame % 2 ? 255 : 0) && picturePixel[2] == 255,
                "Production picture shader/texture binding failed");
        if (frame + 1 == Frames)
        {
            std::ofstream capture("d3d12-submission.ppm", std::ios::binary);
            capture << "P6\n" << Width << " " << Height << "\n255\n";
            for (UINT y = 0; y < Height; ++y)
                for (UINT x = 0; x < Width; ++x)
                    capture.write(reinterpret_cast<const char*>(pixels + y * pitch + x * 4), 3);
            Require(capture.good(), "Could not write capture");
        }
        const D3D12_RANGE noWrites = {0, 0};
        readbacks[frame]->Unmap(0, &noWrites);
    }
    // Exercise timeout recovery with a deterministic queue gate. Its destructor
    // unblocks the queue even if an assertion throws, so teardown cannot deadlock.
    {
        struct QueueGate
        {
            ComPtr<ID3D12Fence> fence;
            ~QueueGate() { if (fence) fence->Signal(1); }
        } gate;
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate.fence)), "Create queue gate");
        Check(context.Begin(&commands), "Begin gated submission");
        Check(queue->Wait(gate.fence.Get(), 1), "Gate queue");
        Check(context.Submit(&completion), "Submit gated work");
        Require(context.Wait(completion, 1) == HRESULT_FROM_WIN32(WAIT_TIMEOUT),
                "Pending fence did not time out");
        Check(gate.fence->Signal(1), "Release queue gate");
        Check(context.Wait(completion), "Recover from timed-out fence wait");
    }
    Check(context.Shutdown(), "Drain context");
    Check(context.Initialize(device.Get(), queue.Get()), "Reinitialize context");
    Check(context.Begin(&commands), "Begin after reinitialize");
    Check(context.Submit(&completion), "Submit after reinitialize");
    Check(context.Shutdown(), "Drain final submission");
    CheckTextureUploads(device.Get(), queue.Get());
    CheckRendererBuffers(device.Get(), queue.Get());
    CheckDebugMessages(messages.Get());
    std::ofstream report("d3d12-submission-result.json");
    report << "{\"status\":\"pass\",\"api\":\"D3D12\",\"adapter\":\"WARP\","
           << "\"scope\":\"submission-context\",\"imm_scene_renderer\":false,"
           << "\"buffer_uploads\":[\"vertex\",\"index\",\"constant\",\"structured\"],"
           << "\"texture_uploads\":[\"rgba8\",\"bc1\"],\"texture_subresources_verified\":8,"
           << "\"production_picture_shader\":true,"
           << "\"renderer_buffer_versions_verified\":18,"
           << "\"renderer_draw_frames_verified\":9,"
           << "\"renderer_indexed_frames_verified\":9,"
           << "\"renderer_sampled_mip_frames_verified\":9,"
           << "\"renderer_state_frames_verified\":9,"
           << "\"renderer_cube_faces_verified\":6,"
           << "\"frames_verified\":" << Frames << ",\"reversed_z\":true,\"debug_layer\":"
           << (debugEnabled ? "true" : "false") << "}\n";
    Require(report.good(), "Could not write result");
    std::printf("IMM_DX12_PHASE1 PASS api=D3D12 adapter=WARP frames=%u depth=reversed-Z debug=%s scope=submission-context\n",
                Frames, debugEnabled ? "enabled" : "unavailable");
    return 0;
}
}

int main()
{
    try { return Run(); }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "IMM_DX12_PHASE1 FAIL %s\n", error.what());
        return 1;
    }
}
