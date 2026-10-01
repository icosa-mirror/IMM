#include "piDX12_Renderer.h"
#include <cstring>
#include <limits>
#include <unordered_set>
#include <vector>

namespace ImmCore
{
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
    piDX12CommandContext context;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    ID3D12GraphicsCommandList* commands = nullptr;
    std::unordered_set<Buffer*> buffers;
    Buffer* constants[10] = {};
    Buffer* structured[16] = {};

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
    for (auto& buffer : m->constants) buffer = nullptr;
    for (auto& buffer : m->structured) buffer = nullptr;
    m->device.Reset();
}

HRESULT piRendererDX12::BeginFrame()
{
    ID3D12GraphicsCommandList* commands = nullptr;
    const HRESULT result = m->context.Begin(&commands);
    if (SUCCEEDED(result)) m->commands = commands;
    return result;
}
HRESULT piRendererDX12::EndFrame(uint64_t* completion)
{
    if (!completion) return E_POINTER;
    const HRESULT result = m->context.Submit(completion);
    m->commands = nullptr;
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
}
void piRendererDX12::DettachShaderBuffer(int unit) { AttachShaderBuffer(nullptr, unit); }
}
