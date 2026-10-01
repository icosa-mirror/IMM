#include "piDX12_Renderer.h"
#include "piDX12_ShaderBindings.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <string>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_set>
#include <vector>

namespace ImmCore
{
namespace
{
bool SamplerDescription(piRenderer::TextureFilter filter, piRenderer::TextureWrap wrap, float anisotropy,
                        D3D12_SAMPLER_DESC& desc)
{
    if (!std::isfinite(anisotropy) || anisotropy < 1 || anisotropy > 16) return false;
    desc = {};
    switch (filter)
    {
    case piRenderer::TextureFilter::NONE: desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; break;
    case piRenderer::TextureFilter::LINEAR: desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT; break;
    case piRenderer::TextureFilter::MIPMAP: desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; desc.MaxLOD = D3D12_FLOAT32_MAX; break;
    case piRenderer::TextureFilter::NONE_MIPMAP: desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR; desc.MaxLOD = D3D12_FLOAT32_MAX; break;
    case piRenderer::TextureFilter::PCF: desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; break;
    default: return false;
    }
    D3D12_TEXTURE_ADDRESS_MODE address;
    switch (wrap)
    {
    case piRenderer::TextureWrap::CLAMP_TO_BORDER: address = D3D12_TEXTURE_ADDRESS_MODE_BORDER; break;
    case piRenderer::TextureWrap::CLAMP: address = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; break;
    case piRenderer::TextureWrap::REPEAT: address = D3D12_TEXTURE_ADDRESS_MODE_WRAP; break;
    case piRenderer::TextureWrap::MIRROR_CLAMP: address = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE; break;
    case piRenderer::TextureWrap::MIRROR_REPEAT: address = D3D12_TEXTURE_ADDRESS_MODE_MIRROR; break;
    default: return false;
    }
    desc.AddressU = desc.AddressV = desc.AddressW = address;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.MaxAnisotropy = static_cast<UINT>(anisotropy);
    if (anisotropy > 1) desc.Filter = filter == piRenderer::TextureFilter::PCF ?
        D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
    return true;
}
std::pair<DXGI_FORMAT, UINT> VertexFormat(piRenderer::Format format)
{
    static const std::pair<DXGI_FORMAT, UINT> formats[] = {
        {DXGI_FORMAT_R32G32B32A32_FLOAT,16},{DXGI_FORMAT_R32G32B32A32_UINT,16},{DXGI_FORMAT_R32G32B32A32_SINT,16},
        {DXGI_FORMAT_R16G16B16A16_FLOAT,8},{DXGI_FORMAT_R16G16B16A16_UNORM,8},{DXGI_FORMAT_R16G16B16A16_UINT,8},
        {DXGI_FORMAT_R16G16B16A16_SNORM,8},{DXGI_FORMAT_R16G16B16A16_SINT,8},
        {DXGI_FORMAT_R8G8B8A8_UNORM,4},{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,4},{DXGI_FORMAT_R8G8B8A8_UINT,4},
        {DXGI_FORMAT_R8G8B8A8_SNORM,4},{DXGI_FORMAT_R8G8B8A8_SINT,4},
        {DXGI_FORMAT_R32G32B32_FLOAT,12},{DXGI_FORMAT_R32G32B32_UINT,12},{DXGI_FORMAT_R32G32B32_SINT,12},
        {DXGI_FORMAT_R32G32_FLOAT,8},{DXGI_FORMAT_R32G32_UINT,8},{DXGI_FORMAT_R32G32_SINT,8},
        {DXGI_FORMAT_R16G16_FLOAT,4},{DXGI_FORMAT_R16G16_UNORM,4},{DXGI_FORMAT_R16G16_UINT,4},
        {DXGI_FORMAT_R16G16_SNORM,4},{DXGI_FORMAT_R16G16_SINT,4},
        {DXGI_FORMAT_R8G8_UNORM,2},{DXGI_FORMAT_R8G8_UINT,2},{DXGI_FORMAT_R8G8_SNORM,2},{DXGI_FORMAT_R8G8_SINT,2},
        {DXGI_FORMAT_R32_FLOAT,4},{DXGI_FORMAT_R32_UINT,4},{DXGI_FORMAT_R32_SINT,4},
        {DXGI_FORMAT_R16_FLOAT,2},{DXGI_FORMAT_R16_UNORM,2},{DXGI_FORMAT_R16_UINT,2},
        {DXGI_FORMAT_R16_SNORM,2},{DXGI_FORMAT_R16_SINT,2},
        {DXGI_FORMAT_R8_UNORM,1},{DXGI_FORMAT_R8_UINT,1},{DXGI_FORMAT_R8_SNORM,1},{DXGI_FORMAT_R8_SINT,1},
        {DXGI_FORMAT_R10G10B10A2_UNORM,4},{DXGI_FORMAT_R10G10B10A2_UINT,4},
        {DXGI_FORMAT_R11G11B10_FLOAT,4},{DXGI_FORMAT_R9G9B9E5_SHAREDEXP,4}
    };
    const auto index = static_cast<unsigned int>(format);
    return index < sizeof(formats) / sizeof(formats[0]) ? formats[index] : std::make_pair(DXGI_FORMAT_UNKNOWN, 0u);
}
}
struct piRendererDX12::State
{
    struct Buffer
    {
        std::vector<unsigned char> data;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        BufferUse use;
        UINT stride = 0;
        bool mapped = false;
    };
    struct VertexArray
    {
        uint64_t id = 0;
        UINT streams = 0;
        Buffer* vertices[2] = {};
        UINT strides[2] = {};
        ArrayLayout2 layouts[2] = {};
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
        Buffer* indices = nullptr;
        bool indexed = false;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_R16_UINT;
    };
    std::unordered_set<VertexArray*> arrays;
    VertexArray* array = nullptr;
    uint64_t nextArrayId = 1;
    struct Shader
    {
        std::vector<uint8_t> vs, hs, ds, gs, ps;
        using Key = std::tuple<DXGI_FORMAT, DXGI_FORMAT, UINT, UINT, D3D12_PRIMITIVE_TOPOLOGY_TYPE, bool, bool, uint64_t>;
        struct Pipeline { Key key; Microsoft::WRL::ComPtr<ID3D12PipelineState> object; };
        std::vector<Pipeline> pipelines;
    };
    std::unordered_set<Shader*> shaders;
    Shader* shader = nullptr;
    piDX12ShaderBindings bindings;
    ExternalTarget target;
    bool colorWrite = true, depthWrite = true;
    D3D12_VIEWPORT viewport = {};
    piDX12CommandContext context;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    ID3D12GraphicsCommandList* commands = nullptr;
    std::unordered_set<Buffer*> buffers;
    Buffer* constants[10] = {};
    Buffer* structured[16] = {};
    struct Texture
    {
        TextureInfo info;
        TextureFilter filter;
        TextureWrap wrap;
        D3D12_SAMPLER_DESC sampler;
        D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    };
    struct Sampler { D3D12_SAMPLER_DESC description; };
    std::unordered_set<Texture*> textures;
    std::unordered_set<Sampler*> samplers;
    Texture* sampled[16] = {};
    Sampler* selectedSamplers[16] = {};
    Texture* Get(piTexture handle) const
    {
        auto* texture = reinterpret_cast<Texture*>(handle);
        if (!texture || textures.find(texture) == textures.end()) throw std::invalid_argument("IMM_DX12: foreign texture");
        return texture;
    }

    Buffer* Get(piBuffer handle) const
    {
        auto* buffer = reinterpret_cast<Buffer*>(handle);
        if (!buffer || buffers.find(buffer) == buffers.end())
            throw std::invalid_argument("IMM_DX12: foreign or destroyed buffer");
        return buffer;
    }
    HRESULT Upload(Buffer& buffer)
    {
        const bool standaloneUpload = commands == nullptr;
        ID3D12GraphicsCommandList* uploadCommands = nullptr;
        HRESULT result = standaloneUpload ? context.Begin(&uploadCommands) : S_OK;
        if (FAILED(result)) return result;
        Microsoft::WRL::ComPtr<ID3D12Resource> replacement;
        if (buffer.use == BufferUse::Constant)
        {
            // CBV sizes are 256-byte aligned; the logical piBuffer retains its
            // original length so UpdateBuffer bounds do not include padding.
            auto padded = buffer.data;
            padded.resize((padded.size() + 255) & ~size_t(255), 0);
            result = context.UploadBuffer(padded.data(), padded.size(), &replacement);
        }
        else result = context.UploadBuffer(buffer.data.data(), buffer.data.size(), &replacement);
        if (standaloneUpload)
        {
            if (FAILED(result)) context.Cancel();
            else
            {
                uint64_t completion = 0;
                result = context.Submit(&completion);
            }
        }
        if (SUCCEEDED(result)) buffer.resource = std::move(replacement);
        return result;
    }
    piBuffer Create(const void* data, UINT amount, BufferUse use, UINT stride, bool mapped)
    {
        if (!device || amount == 0 ||
            (use != BufferUse::Vertex && use != BufferUse::Index && use != BufferUse::Constant &&
             use != BufferUse::ShaderResource && use != BufferUse::DrawCommands)) return nullptr;
        if (use == BufferUse::Constant && amount > D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16) return nullptr;
        auto buffer = std::make_unique<Buffer>();
        buffer->data.resize(amount, 0);
        if (data) std::memcpy(buffer->data.data(), data, amount);
        buffer->use = use;
        buffer->stride = stride;
        buffer->mapped = mapped;
        if (!mapped && FAILED(Upload(*buffer))) return nullptr;
        buffers.insert(buffer.get());
        return reinterpret_cast<piBuffer>(buffer.release());
    }
};

piRendererDX12::piRendererDX12() : m(new State) {}
piRendererDX12::~piRendererDX12() { Deinitialize(); }

bool piRendererDX12::InitializeExternal(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    if (m->device || FAILED(m->context.Initialize(device, queue))) return false;
    if (FAILED(m->bindings.Initialize(device))) { m->context.Shutdown(); return false; }
    m->device = device;
    return true;
}

bool piRendererDX12::Initialize(int id, const void** hwnd, int num, bool disableVSync,
    bool disableErrors, piReporter* reporter, bool createDevice, void* device)
{
    (void)id; (void)hwnd; (void)num; (void)disableVSync; (void)disableErrors;
    (void)createDevice; (void)device;
    if (reporter) reporter->Error("IMM_DX12: use typed device and queue initialization", 1);
    return false;
}

void piRendererDX12::Deinitialize()
{
    if (m->commands) { m->context.Cancel(); m->commands = nullptr; }
    // CommandContext retains submitted versions even if shutdown cannot drain.
    m->context.Shutdown();
    for (auto* buffer : m->buffers) delete buffer;
    m->buffers.clear();
    for (auto* array : m->arrays) delete array;
    m->arrays.clear();
    m->array = nullptr;
    for (auto* texture : m->textures) delete texture;
    m->textures.clear();
    for (auto* sampler : m->samplers) delete sampler;
    m->samplers.clear();
    for (auto& texture : m->sampled) texture = nullptr;
    for (auto& sampler : m->selectedSamplers) sampler = nullptr;
    for (auto* shader : m->shaders) delete shader;
    m->shaders.clear();
    m->shader = nullptr;
    m->bindings = piDX12ShaderBindings();
    m->target = {};
    for (auto& buffer : m->constants) buffer = nullptr;
    for (auto& buffer : m->structured) buffer = nullptr;
    m->device.Reset();
}

HRESULT piRendererDX12::BeginFrame()
{
    ID3D12GraphicsCommandList* commands = nullptr;
    const HRESULT result = m->context.Begin(&commands);
    if (SUCCEEDED(result)) { m->commands = commands; m->target = {}; }
    return result;
}
HRESULT piRendererDX12::EndFrame(uint64_t* completion)
{
    if (!completion) return E_POINTER;
    if (m->target.color)
    {
        HRESULT restored = m->context.Transition(m->target.color, D3D12_RESOURCE_STATE_RENDER_TARGET, m->target.colorState);
        if (SUCCEEDED(restored) && m->target.depth)
            restored = m->context.Transition(m->target.depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, m->target.depthState);
        if (FAILED(restored)) return restored;
    }
    const HRESULT result = m->context.Submit(completion);
    m->commands = nullptr;
    m->target = {};
    return result;
}
HRESULT piRendererDX12::WaitForFrame(uint64_t completion) { return m->context.Wait(completion); }
bool piRendererDX12::SupportsFeature(RendererFeature feature) { (void)feature; return false; }
piRenderer::API piRendererDX12::GetAPI() { return API::DX12; }
void* piRendererDX12::GetContext() { return m->commands; }
void piRendererDX12::Report() {}

piBuffer piRendererDX12::CreateBuffer(const void* data, unsigned int amount, BufferType mode, BufferUse use)
{
    (void)mode;
    return m->Create(data, amount, use, 0, false);
}
piBuffer piRendererDX12::CreateStructuredBuffer(const void* data, unsigned int numElements,
    unsigned int elementSize, BufferType mode, BufferUse use)
{
    (void)mode;
    if (use != BufferUse::ShaderResource || numElements == 0 || elementSize == 0 ||
        elementSize % 4 != 0 || elementSize > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES ||
        numElements > (std::numeric_limits<UINT>::max)() / elementSize) return nullptr;
    return m->Create(data, numElements * elementSize, use, elementSize, false);
}
piBuffer piRendererDX12::CreateBufferMapped_Start(void** ptr, unsigned int amount, BufferUse use)
{
    if (!ptr) return nullptr;
    *ptr = nullptr;
    const auto handle = m->Create(nullptr, amount, use, 0, true);
    if (handle) *ptr = m->Get(handle)->data.data();
    return handle;
}
void piRendererDX12::CreateBufferMapped_End(piBuffer handle)
{
    auto* buffer = m->Get(handle);
    if (!buffer->mapped) throw std::logic_error("IMM_DX12: buffer is not mapped");
    if (FAILED(m->Upload(*buffer))) throw std::runtime_error("IMM_DX12: mapped buffer upload failed");
    buffer->mapped = false;
}
void piRendererDX12::DestroyBuffer(piBuffer handle)
{
    if (!handle) return;
    auto* buffer = m->Get(handle);
    for (auto& bound : m->constants) if (bound == buffer) bound = nullptr;
    for (auto& bound : m->structured) if (bound == buffer) bound = nullptr;
    for (auto* array : m->arrays)
    {
        for (auto& vertex : array->vertices) if (vertex == buffer) vertex = nullptr;
        if (array->indices == buffer) array->indices = nullptr;
    }
    m->buffers.erase(buffer);
    delete buffer;
}
void piRendererDX12::UpdateBuffer(piBuffer handle, const void* data, int offset, int len, bool invalidate)
{
    (void)invalidate; // Every update creates a new GPU version; earlier draws keep their version.
    auto* buffer = m->Get(handle);
    if (buffer->mapped || !data || offset < 0 || len < 0 || size_t(offset) > buffer->data.size() ||
        size_t(len) > buffer->data.size() - size_t(offset)) throw std::invalid_argument("IMM_DX12: invalid buffer update");
    if (len == 0) return;
    std::memcpy(buffer->data.data() + offset, data, len);
    if (FAILED(m->Upload(*buffer))) throw std::runtime_error("IMM_DX12: buffer update upload failed");
}
ID3D12Resource* piRendererDX12::BufferResource(piBuffer handle) const
{
    auto* buffer = m->Get(handle);
    if (buffer->mapped) throw std::logic_error("IMM_DX12: mapped buffer cannot be bound");
    return buffer->resource.Get();
}
void piRendererDX12::AttachShaderConstants(piBuffer handle, int unit)
{
    if (unit < 0 || unit >= 10) throw std::out_of_range("IMM_DX12: constant register");
    auto* buffer = handle ? m->Get(handle) : nullptr;
    if (buffer && (buffer->use != BufferUse::Constant || buffer->mapped))
        throw std::invalid_argument("IMM_DX12: invalid constant buffer");
    m->constants[unit] = buffer;
}
void piRendererDX12::AttachShaderBuffer(piBuffer handle, int unit)
{
    if (unit < 0 || unit >= 16) throw std::out_of_range("IMM_DX12: resource register");
    auto* buffer = handle ? m->Get(handle) : nullptr;
    if (buffer && (buffer->use != BufferUse::ShaderResource || buffer->stride == 0 || buffer->mapped))
        throw std::invalid_argument("IMM_DX12: invalid structured buffer");
    m->structured[unit] = buffer;
    m->sampled[unit] = nullptr;
}
void piRendererDX12::DettachShaderBuffer(int unit) { AttachShaderBuffer(nullptr, unit); }

HRESULT piRendererDX12::SetExternalTarget(const ExternalTarget& target)
{
    if (!m->commands || m->target.color) return E_UNEXPECTED;
    if (!target.color || !target.rtv.ptr || target.colorFormat == DXGI_FORMAT_UNKNOWN ||
        (target.depth && (!target.dsv.ptr || target.depthFormat == DXGI_FORMAT_UNKNOWN))) return E_INVALIDARG;
    const auto color = target.color->GetDesc();
    if (color.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || color.Width > INT_MAX || color.Height > INT_MAX ||
        !(color.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
        return E_INVALIDARG;
    Microsoft::WRL::ComPtr<ID3D12Device> attachmentDevice;
    HRESULT result = target.color->GetDevice(IID_PPV_ARGS(&attachmentDevice));
    if (FAILED(result)) return result;
    if (attachmentDevice.Get() != m->device.Get()) return E_INVALIDARG;
    if (target.depth)
    {
        const auto depth = target.depth->GetDesc();
        if (depth.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            !(depth.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ||
            depth.Width != color.Width || depth.Height != color.Height ||
            depth.SampleDesc.Count != color.SampleDesc.Count || depth.SampleDesc.Quality != color.SampleDesc.Quality)
            return E_INVALIDARG;
        result = target.depth->GetDevice(IID_PPV_ARGS(&attachmentDevice));
        if (FAILED(result)) return result;
        if (attachmentDevice.Get() != m->device.Get()) return E_INVALIDARG;
    }
    result = m->context.Transition(target.color, target.colorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (SUCCEEDED(result) && target.depth)
        result = m->context.Transition(target.depth, target.depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    if (FAILED(result)) return result;
    m->target = target;
    m->commands->OMSetRenderTargets(1, &target.rtv, FALSE, target.depth ? &target.dsv : nullptr);
    const int viewport[] = {0, 0, static_cast<int>(color.Width), static_cast<int>(color.Height)};
    SetViewport(0, viewport);
    return S_OK;
}

piShader piRendererDX12::CreateShaderBinary(const piShaderOptions* options, const uint8_t* vs, const int vs_len,
    const uint8_t* cs, const int cs_len, const uint8_t* es, const int es_len, const uint8_t* gs, const int gs_len,
    const uint8_t* fs, const int fs_len, char* error)
{
    (void)options;
    if (error) error[0] = 0;
    if (!m->device || !vs || vs_len <= 0 || cs_len < 0 || es_len < 0 || gs_len < 0 || fs_len < 0 ||
        ((cs != nullptr) != (cs_len > 0)) || ((es != nullptr) != (es_len > 0)) ||
        ((gs != nullptr) != (gs_len > 0)) || ((fs != nullptr) != (fs_len > 0)) || ((cs != nullptr) != (es != nullptr)))
        return nullptr;
    auto shader = std::make_unique<State::Shader>();
    shader->vs.assign(vs, vs + vs_len);
    if (cs) shader->hs.assign(cs, cs + cs_len);
    if (es) shader->ds.assign(es, es + es_len);
    if (gs) shader->gs.assign(gs, gs + gs_len);
    if (fs) shader->ps.assign(fs, fs + fs_len);
    m->shaders.insert(shader.get());
    return reinterpret_cast<piShader>(shader.release());
}

piShader piRendererDX12::CreateShader(const piShaderOptions* options, const char* vs, const char* cs,
    const char* es, const char* gs, const char* fs, char* error)
{
    if (error) error[0] = 0;
    if (!vs || (options && (options->mNum < 0 || options->mNum > 64))) return nullptr;
    std::string values[64];
    D3D_SHADER_MACRO macros[65] = {};
    for (int i = 0; options && i < options->mNum; ++i)
    {
        if (!std::memchr(options->mOption[i].mName, 0, sizeof(options->mOption[i].mName))) return nullptr;
        values[i] = std::to_string(options->mOption[i].mValue);
        macros[i] = {options->mOption[i].mName, values[i].c_str()};
    }
    const char* sources[] = {vs, cs, es, gs, fs};
    const char* profiles[] = {"vs_5_0", "hs_5_0", "ds_5_0", "gs_5_0", "ps_5_0"};
    Microsoft::WRL::ComPtr<ID3DBlob> code[5];
    for (int i = 0; i < 5; ++i)
    {
        if (!sources[i]) continue;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        const HRESULT result = D3DCompile(sources[i], std::strlen(sources[i]), "IMM_DX12", macros, nullptr,
            "main", profiles[i], D3DCOMPILE_ENABLE_STRICTNESS, 0, &code[i], &errors);
        if (FAILED(result))
        {
            // The existing piRenderer error-buffer contract has no size argument.
            // Leave it empty and emit a bounded compiler diagnostic instead.
            if (errors) std::fprintf(stderr, "IMM_DX12: %.1024s\n", static_cast<const char*>(errors->GetBufferPointer()));
            return nullptr;
        }
    }
    auto bytes = [&](int i) { return code[i] ? static_cast<const uint8_t*>(code[i]->GetBufferPointer()) : nullptr; };
    auto size = [&](int i) { return code[i] ? static_cast<int>(code[i]->GetBufferSize()) : 0; };
    return CreateShaderBinary(options, bytes(0), size(0), bytes(1), size(1), bytes(2), size(2),
        bytes(3), size(3), bytes(4), size(4), error);
}

void piRendererDX12::AttachShader(piShader handle)
{
    auto* shader = reinterpret_cast<State::Shader*>(handle);
    if (shader && m->shaders.find(shader) == m->shaders.end()) throw std::invalid_argument("IMM_DX12: foreign shader");
    m->shader = shader;
}
void piRendererDX12::DettachShader() { m->shader = nullptr; }
void piRendererDX12::DestroyShader(piShader handle)
{
    if (!handle) return;
    auto* shader = reinterpret_cast<State::Shader*>(handle);
    if (m->shaders.erase(shader) == 0) throw std::invalid_argument("IMM_DX12: foreign shader");
    if (m->shader == shader) m->shader = nullptr;
    delete shader;
}

void piRendererDX12::SetWriteMask(bool c0, bool c1, bool c2, bool c3, bool z)
{
    (void)c1; (void)c2; (void)c3; // No secondary color attachments are bound.
    m->colorWrite = c0;
    m->depthWrite = z;
}
void piRendererDX12::SetViewport(int id, const int* viewport)
{
    if (id != 0 || !viewport) throw std::invalid_argument("IMM_DX12: invalid viewport");
    const float values[] = {float(viewport[0]), float(viewport[1]), float(viewport[2]), float(viewport[3]), 0, 1};
    SetViewports(1, values);
}
void piRendererDX12::SetViewports(int num, const float* viewport)
{
    if (!m->commands || num != 1 || !viewport || viewport[2] <= 0 || viewport[3] <= 0)
        throw std::invalid_argument("IMM_DX12: invalid flat viewport");
    for (int i = 0; i < 6; ++i)
        if (!std::isfinite(viewport[i])) throw std::invalid_argument("IMM_DX12: non-finite viewport");
    if (viewport[0] < D3D12_VIEWPORT_BOUNDS_MIN || viewport[1] < D3D12_VIEWPORT_BOUNDS_MIN ||
        viewport[0] + viewport[2] > D3D12_VIEWPORT_BOUNDS_MAX ||
        viewport[1] + viewport[3] > D3D12_VIEWPORT_BOUNDS_MAX ||
        viewport[4] < 0 || viewport[5] > 1 || viewport[4] > viewport[5])
        throw std::invalid_argument("IMM_DX12: viewport exceeds device bounds");
    m->viewport = {viewport[0], viewport[1], viewport[2], viewport[3], viewport[4], viewport[5]};
    m->commands->RSSetViewports(1, &m->viewport);
    const D3D12_RECT scissor = {LONG(viewport[0]), LONG(viewport[1]), LONG(viewport[0] + viewport[2]), LONG(viewport[1] + viewport[3])};
    m->commands->RSSetScissorRects(1, &scissor);
}
void piRendererDX12::GetViewports(int* num, float* viewport)
{
    if (!num || !viewport) throw std::invalid_argument("IMM_DX12: null viewport output");
    *num = 1;
    const float values[] = {m->viewport.TopLeftX, m->viewport.TopLeftY, m->viewport.Width,
        m->viewport.Height, m->viewport.MinDepth, m->viewport.MaxDepth};
    std::memcpy(viewport, values, sizeof(values));
}
void piRendererDX12::Clear(const float* color0, const float* color1, const float* color2, const float* color3, const bool depth0)
{
    if (!m->commands || !m->target.color || color1 || color2 || color3)
        throw std::invalid_argument("IMM_DX12: invalid clear target");
    if (color0) m->commands->ClearRenderTargetView(m->target.rtv, color0, 0, nullptr);
    if (depth0 && m->target.depth) m->commands->ClearDepthStencilView(m->target.dsv, D3D12_CLEAR_FLAG_DEPTH, 0, 0, 0, nullptr);
}

void piRendererDX12::PrepareDraw(PrimitiveType primitive)
{
    if (!m->commands || !m->target.color || !m->shader)
        throw std::invalid_argument("IMM_DX12: incomplete draw state");
    D3D_PRIMITIVE_TOPOLOGY topology;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType;
    switch (primitive)
    {
    case PrimitiveType::Triangle: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
    case PrimitiveType::TriangleStrip: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
    case PrimitiveType::Lines: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
    case PrimitiveType::LineStrip: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
    case PrimitiveType::Point: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
    default: Unsupported("IMM_DX12: primitive topology not implemented");
    }
    auto& shader = *m->shader;
    const auto samples = m->target.color->GetDesc().SampleDesc;
    const auto depthFormat = m->target.depth ? m->target.depthFormat : DXGI_FORMAT_UNKNOWN;
    const State::Shader::Key key = {m->target.colorFormat, depthFormat, samples.Count, samples.Quality,
        topologyType, m->colorWrite, m->depthWrite, m->array ? m->array->id : 0};
    ID3D12PipelineState* pipeline = nullptr;
    for (const auto& entry : shader.pipelines) if (entry.key == key) { pipeline = entry.object.Get(); break; }
    if (!pipeline)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m->bindings.RootSignature();
        auto bytecode = [](const std::vector<uint8_t>& code) { return D3D12_SHADER_BYTECODE{code.empty() ? nullptr : code.data(), code.size()}; };
        desc.VS = bytecode(shader.vs); desc.HS = bytecode(shader.hs); desc.DS = bytecode(shader.ds);
        desc.GS = bytecode(shader.gs); desc.PS = bytecode(shader.ps);
        if (m->array) desc.InputLayout = {m->array->elements.data(), static_cast<UINT>(m->array->elements.size())};
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = m->colorWrite ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = m->target.depth != nullptr;
        desc.DepthStencilState.DepthWriteMask = m->depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        desc.PrimitiveTopologyType = topologyType;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = m->target.colorFormat;
        desc.DSVFormat = depthFormat;
        desc.SampleDesc = samples;
        State::Shader::Pipeline entry;
        entry.key = key;
        if (FAILED(m->device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&entry.object))))
            throw std::runtime_error("IMM_DX12: pipeline creation failed");
        pipeline = entry.object.Get();
        shader.pipelines.push_back(std::move(entry));
    }
    piDX12ShaderBindings::Resources resources;
    for (UINT i = 0; i < 10; ++i) if (m->constants[i]) resources.constants[i] = m->constants[i]->resource.Get();
    for (UINT i = 0; i < 16; ++i)
    {
        if (m->sampled[i])
        {
            resources.resources[i] = m->sampled[i]->resource.Get();
            resources.views[i] = m->sampled[i]->view;
            resources.samplers[i] = m->sampled[i]->sampler;
        }
        if (m->selectedSamplers[i]) resources.samplers[i] = m->selectedSamplers[i]->description;
        const auto* buffer = m->structured[i];
        if (!buffer) continue;
        resources.resources[i] = buffer->resource.Get();
        auto& view = resources.views[i];
        view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Buffer.NumElements = static_cast<UINT>(buffer->data.size() / buffer->stride);
        view.Buffer.StructureByteStride = buffer->stride;
    }
    if (FAILED(m->context.Retain(pipeline)) || FAILED(m->bindings.Bind(m->context, m->commands, resources)))
        throw std::runtime_error("IMM_DX12: resource binding failed");
    m->commands->SetPipelineState(pipeline);
    m->commands->IASetPrimitiveTopology(topology);
    if (m->array)
    {
        D3D12_VERTEX_BUFFER_VIEW views[2] = {};
        for (UINT i = 0; i < m->array->streams; ++i)
        {
            auto* buffer = m->array->vertices[i];
            if (!buffer || buffer->mapped) throw std::logic_error("IMM_DX12: missing or mapped vertex buffer");
            if (FAILED(m->context.Retain(buffer->resource.Get()))) throw std::runtime_error("IMM_DX12: retain vertex buffer failed");
            views[i] = {buffer->resource->GetGPUVirtualAddress(), static_cast<UINT>(buffer->data.size()), m->array->strides[i]};
        }
        m->commands->IASetVertexBuffers(0, m->array->streams, views);
    }
}

void piRendererDX12::DrawPrimitiveNotIndexed(PrimitiveType primitive, int first, int num, int instances)
{
    if (first < 0 || num < 0 || instances < 0) throw std::invalid_argument("IMM_DX12: invalid draw range");
    if (num == 0 || instances == 0) return;
    PrepareDraw(primitive);
    m->commands->DrawInstanced(num, instances, first, 0);
}

piVertexArray piRendererDX12::CreateVertexArray2(int numStreams, piBuffer vb0, const ArrayLayout2* layout0,
    piBuffer vb1, const ArrayLayout2* layout1, const void* shaderBinary, size_t shaderBinarySize,
    piBuffer ib, const IndexArrayFormat indexFormat)
{
    (void)shaderBinary; (void)shaderBinarySize; // D3D12 validates input layouts when creating the PSO.
    if (!m->device || numStreams < 0 || numStreams > 2 ||
        (indexFormat != IndexArrayFormat::UINT_16 && indexFormat != IndexArrayFormat::UINT_32)) return nullptr;
    auto array = std::make_unique<State::VertexArray>();
    array->id = m->nextArrayId++;
    array->streams = numStreams;
    array->indexed = ib != nullptr;
    array->indices = ib ? m->Get(ib) : nullptr;
    if (array->indices && array->indices->use != BufferUse::Index) return nullptr;
    array->indexFormat = indexFormat == IndexArrayFormat::UINT_16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    const ArrayLayout2* layouts[] = {layout0, layout1};
    const piBuffer buffers[] = {vb0, vb1};
    for (int stream = 0; stream < numStreams; ++stream)
    {
        if (!layouts[stream] || !buffers[stream] || layouts[stream]->mNumElements <= 0 || layouts[stream]->mNumElements > 12)
            return nullptr;
        array->vertices[stream] = m->Get(buffers[stream]);
        if (array->vertices[stream]->use != BufferUse::Vertex) return nullptr;
        array->layouts[stream] = *layouts[stream];
        const auto& layout = array->layouts[stream];
        for (int i = 0; i < layout.mNumElements; ++i)
        {
            const auto& entry = layout.mEntry[i];
            if (!entry.mName[0] || !std::memchr(entry.mName, 0, sizeof(entry.mName))) return nullptr;
            const auto format = VertexFormat(entry.mFormat);
            if (format.first == DXGI_FORMAT_UNKNOWN) return nullptr;
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {format.first, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE};
            if (FAILED(m->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) ||
                !(support.Support1 & D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER)) return nullptr;
            D3D12_INPUT_ELEMENT_DESC input = {};
            input.SemanticName = entry.mName;
            input.Format = format.first;
            input.InputSlot = stream;
            input.AlignedByteOffset = array->strides[stream];
            input.InputSlotClass = entry.mPerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            input.InstanceDataStepRate = entry.mPerInstance ? 1 : 0;
            array->strides[stream] += format.second;
            array->elements.push_back(input);
        }
    }
    m->arrays.insert(array.get());
    return reinterpret_cast<piVertexArray>(array.release());
}
void piRendererDX12::AttachVertexArray2(piVertexArray handle)
{
    auto* array = reinterpret_cast<State::VertexArray*>(handle);
    if (array && m->arrays.find(array) == m->arrays.end()) throw std::invalid_argument("IMM_DX12: foreign vertex array");
    m->array = array;
}
void piRendererDX12::AttachVertexArray(piVertexArray handle) { AttachVertexArray2(handle); }
void piRendererDX12::DettachVertexArray() { m->array = nullptr; }
void piRendererDX12::DestroyVertexArray2(piVertexArray handle)
{
    if (!handle) return;
    auto* array = reinterpret_cast<State::VertexArray*>(handle);
    if (m->arrays.erase(array) == 0) throw std::invalid_argument("IMM_DX12: foreign vertex array");
    if (m->array == array) m->array = nullptr;
    delete array;
}
void piRendererDX12::DestroyVertexArray(piVertexArray handle) { DestroyVertexArray2(handle); }

piTexture piRendererDX12::CreateTexture(const wchar_t* key, const TextureInfo* info, bool compress,
    TextureFilter filter, TextureWrap wrap, float anisotropy, const void* data)
{
    (void)key;
    if (!m->device || !info || compress || (info->mType != TextureType::T2D && info->mType != TextureType::T2D_ARRAY) ||
        info->mXres <= 0 || info->mYres <= 0 || info->mXres > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        info->mYres > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || info->mMultisample != 1) return nullptr;
    const auto format = VertexFormat(info->mFormat);
    if (format.first == DXGI_FORMAT_UNKNOWN) return nullptr;
    const UINT layers = info->mType == TextureType::T2D ? 1 : static_cast<UINT>(info->mZres);
    if (layers == 0 || layers > D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) return nullptr;
    auto texture = std::make_unique<State::Texture>();
    if (!SamplerDescription(filter, wrap, anisotropy, texture->sampler)) return nullptr;
    UINT maxMips = 1;
    for (UINT size = static_cast<UINT>((std::max)(info->mXres, info->mYres)); size > 1; size >>= 1) ++maxMips;
    const bool useMips = filter == TextureFilter::MIPMAP || filter == TextureFilter::NONE_MIPMAP;
    const UINT mips = info->mNumMips ? info->mNumMips : (useMips ? maxMips : 1);
    const bool srgb = info->mFormat == Format::C4_8_UNORM_SRGB;
    const bool byteChannels = info->mFormat == Format::C1_8_UNORM || info->mFormat == Format::C4_8_UNORM || srgb;
    if (mips > maxMips || (mips > 1 && !byteChannels)) return nullptr;
    texture->info = *info;
    texture->info.mNumMips = mips;
    texture->filter = filter;
    texture->wrap = wrap;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = info->mXres;
    description.Height = info->mYres;
    description.DepthOrArraySize = static_cast<UINT16>(layers);
    description.MipLevels = static_cast<UINT16>(mips);
    description.Format = format.first;
    description.SampleDesc.Count = 1;
    std::vector<std::vector<unsigned char>> storage;
    std::vector<piDX12CommandContext::TextureData> sources;
    storage.reserve(layers * mips);
    sources.reserve(layers * mips);
    const size_t baseBytes = size_t(info->mXres) * info->mYres * format.second;
    for (UINT layer = 0; layer < layers; ++layer)
    {
        UINT width = info->mXres, height = info->mYres;
        storage.emplace_back(baseBytes, static_cast<unsigned char>(0));
        if (data) std::memcpy(storage.back().data(), static_cast<const unsigned char*>(data) + layer * baseBytes, baseBytes);
        sources.push_back({storage.back().data(), storage.back().size(), size_t(width) * format.second});
        for (UINT mip = 1; mip < mips; ++mip)
        {
            const UINT nextWidth = (std::max)(1u, width / 2), nextHeight = (std::max)(1u, height / 2);
            const auto& previous = storage.back();
            std::vector<unsigned char> next(size_t(nextWidth) * nextHeight * format.second);
            // Generate once at image upload. Include all source texels for odd dimensions.
            for (UINT y = 0; y < nextHeight; ++y)
                for (UINT x = 0; x < nextWidth; ++x)
                    for (UINT channel = 0; channel < format.second; ++channel)
                    {
                        double sum = 0;
                        UINT count = 0;
                        for (UINT sy = y * height / nextHeight; sy < (y + 1) * height / nextHeight; ++sy)
                            for (UINT sx = x * width / nextWidth; sx < (x + 1) * width / nextWidth; ++sx)
                            {
                                double value = previous[(size_t(sy) * width + sx) * format.second + channel] / 255.0;
                                if (srgb && channel < 3) value = value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
                                sum += value; ++count;
                            }
                        double value = sum / count;
                        if (srgb && channel < 3) value = value <= 0.0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
                        next[(size_t(y) * nextWidth + x) * format.second + channel] =
                            static_cast<unsigned char>((std::min)(255.0, (std::max)(0.0, std::round(value * 255.0))));
                    }
            storage.push_back(std::move(next));
            width = nextWidth; height = nextHeight;
            sources.push_back({storage.back().data(), storage.back().size(), size_t(width) * format.second});
        }
    }
    const bool standaloneUpload = m->commands == nullptr;
    ID3D12GraphicsCommandList* uploadCommands = nullptr;
    HRESULT result = standaloneUpload ? m->context.Begin(&uploadCommands) : S_OK;
    if (FAILED(result)) return nullptr;
    result = m->context.UploadTexture2D(description, sources.data(), static_cast<UINT>(sources.size()), &texture->resource);
    if (standaloneUpload)
    {
        if (FAILED(result)) m->context.Cancel();
        else { uint64_t completion = 0; result = m->context.Submit(&completion); }
    }
    if (FAILED(result)) return nullptr;
    texture->view.Format = format.first;
    texture->view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (info->mType == TextureType::T2D)
    {
        texture->view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        texture->view.Texture2D.MipLevels = mips;
    }
    else
    {
        texture->view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        texture->view.Texture2DArray.MipLevels = mips;
        texture->view.Texture2DArray.ArraySize = layers;
    }
    m->textures.insert(texture.get());
    return reinterpret_cast<piTexture>(texture.release());
}
void piRendererDX12::DestroyTexture(piTexture handle)
{
    if (!handle) return;
    auto* texture = m->Get(handle);
    for (auto& bound : m->sampled) if (bound == texture) bound = nullptr;
    m->textures.erase(texture);
    delete texture;
}
void piRendererDX12::GetTextureInfo(piTexture handle, TextureInfo* info)
{
    if (!info) throw std::invalid_argument("IMM_DX12: null texture info output");
    *info = m->Get(handle)->info;
}
void piRendererDX12::GetTextureFormat(piTexture handle, Format* format)
{
    if (!format) throw std::invalid_argument("IMM_DX12: null texture format output");
    *format = m->Get(handle)->info.mFormat;
}
void piRendererDX12::GetTextureRes(piTexture handle, int* resolution)
{
    if (!resolution) throw std::invalid_argument("IMM_DX12: null texture resolution output");
    const auto& info = m->Get(handle)->info;
    resolution[0] = info.mXres; resolution[1] = info.mYres; resolution[2] = info.mZres;
}
void piRendererDX12::GetTextureSampling(piTexture handle, TextureFilter* filter, TextureWrap* wrap)
{
    const auto* texture = m->Get(handle);
    if (filter) *filter = texture->filter;
    if (wrap) *wrap = texture->wrap;
}
void piRendererDX12::AttachTextures(int num, piTexture* handles, int offset)
{
    if (num < 0 || offset < 0 || offset > 16 || num > 16 - offset || (num > 0 && !handles))
        throw std::invalid_argument("IMM_DX12: texture register range");
    for (int i = 0; i < num; ++i)
    {
        m->sampled[offset + i] = handles[i] ? m->Get(handles[i]) : nullptr;
        m->structured[offset + i] = nullptr;
    }
}
void piRendererDX12::AttachTextures(int num, piTexture t0, piTexture t1, piTexture t2, piTexture t3,
    piTexture t4, piTexture t5, piTexture t6, piTexture t7, piTexture t8, piTexture t9, piTexture t10,
    piTexture t11, piTexture t12, piTexture t13, piTexture t14, piTexture t15)
{
    piTexture handles[] = {t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15};
    AttachTextures(num, handles, 0);
}
void piRendererDX12::DettachTextures() { for (auto& texture : m->sampled) texture = nullptr; }
piSampler piRendererDX12::CreateSampler(TextureFilter filter, TextureWrap wrap, float anisotropy)
{
    if (!m->device) return nullptr;
    auto sampler = std::make_unique<State::Sampler>();
    if (!SamplerDescription(filter, wrap, anisotropy, sampler->description)) return nullptr;
    m->samplers.insert(sampler.get());
    return reinterpret_cast<piSampler>(sampler.release());
}
void piRendererDX12::DestroySampler(piSampler handle)
{
    if (!handle) return;
    auto* sampler = reinterpret_cast<State::Sampler*>(handle);
    if (m->samplers.erase(sampler) == 0) throw std::invalid_argument("IMM_DX12: foreign sampler");
    for (auto& bound : m->selectedSamplers) if (bound == sampler) bound = nullptr;
    delete sampler;
}
void piRendererDX12::AttachSamplers(int num, piSampler s0, piSampler s1, piSampler s2, piSampler s3,
    piSampler s4, piSampler s5, piSampler s6, piSampler s7)
{
    if (num < 0 || num > 8) throw std::invalid_argument("IMM_DX12: sampler register range");
    const piSampler handles[] = {s0,s1,s2,s3,s4,s5,s6,s7};
    for (int i = 0; i < num; ++i)
    {
        auto* sampler = reinterpret_cast<State::Sampler*>(handles[i]);
        if (sampler && m->samplers.find(sampler) == m->samplers.end()) throw std::invalid_argument("IMM_DX12: foreign sampler");
        m->selectedSamplers[i] = sampler;
    }
}
void piRendererDX12::DettachSamplers() { for (auto& sampler : m->selectedSamplers) sampler = nullptr; }

void piRendererDX12::DrawPrimitiveIndexed(PrimitiveType primitive, uint32_t num, uint32_t instances,
    uint32_t baseVertex, uint32_t baseInstance, uint32_t baseIndex)
{
    if (num == 0 || instances == 0) return;
    if (!m->array || !m->array->indexed || !m->array->indices || m->array->indices->mapped || baseVertex > INT_MAX)
        throw std::invalid_argument("IMM_DX12: invalid indexed draw state");
    auto* buffer = m->array->indices;
    const UINT stride = m->array->indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
    const size_t indices = buffer->data.size() / stride;
    if (baseIndex > indices || num > indices - baseIndex) throw std::out_of_range("IMM_DX12: index range");
    PrepareDraw(primitive);
    if (FAILED(m->context.Retain(buffer->resource.Get()))) throw std::runtime_error("IMM_DX12: retain index buffer failed");
    const D3D12_INDEX_BUFFER_VIEW view = {buffer->resource->GetGPUVirtualAddress(),
        static_cast<UINT>(buffer->data.size()), m->array->indexFormat};
    m->commands->IASetIndexBuffer(&view);
    m->commands->DrawIndexedInstanced(num, instances, baseIndex, static_cast<INT>(baseVertex), baseInstance);
}
}
