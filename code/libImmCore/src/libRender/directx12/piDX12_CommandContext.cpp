#include "piDX12_CommandContext.h"

#include <limits>
#include <cstdio>
#include <cstring>

namespace ImmCore
{
piDX12CommandContext::~piDX12CommandContext()
{
    if (FAILED(Shutdown()) && mDevice)
    {
        // A live device that refuses a drain fence may still be executing.
        // Shutdown permits retry; during destruction the only safe fallback is
        // to leave these references (and the registered event) until process exit.
        std::fprintf(stderr, "IMM_DX12_PHASE1: queue drain failed on a live device; retaining GPU objects until process exit.\n");
        for (auto& frame : mFrames)
        {
            for (auto& object : frame.retained) object.Detach();
            frame.commands.Detach();
            frame.allocator.Detach();
        }
        mFence.Detach();
        mQueue.Detach();
        mDevice.Detach();
    }
}

HRESULT piDX12CommandContext::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    if (mDevice) return E_UNEXPECTED;
    if (!device || !queue || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
        return E_INVALIDARG;

    Microsoft::WRL::ComPtr<ID3D12Device> queueDevice;
    HRESULT result = queue->GetDevice(IID_PPV_ARGS(&queueDevice));
    if (FAILED(result)) return result;
    if (queueDevice.Get() != device) return E_INVALIDARG;

    mDevice = device;
    mQueue = queue;
    result = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
    if (SUCCEEDED(result))
    {
        mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!mFenceEvent) result = HRESULT_FROM_WIN32(GetLastError());
    }
    for (auto& frame : mFrames)
    {
        if (FAILED(result)) break;
        result = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 IID_PPV_ARGS(&frame.allocator));
        if (SUCCEEDED(result))
            result = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                frame.allocator.Get(), nullptr, IID_PPV_ARGS(&frame.commands));
        if (SUCCEEDED(result)) result = frame.commands->Close();
    }
    if (FAILED(result)) Shutdown();
    return result;
}

HRESULT piDX12CommandContext::Wait(uint64_t completionValue, DWORD timeoutMilliseconds)
{
    if (!mFence || completionValue > mLastSubmitted) return E_INVALIDARG;
    if (completionValue == 0) return S_OK;
    const uint64_t removed = (std::numeric_limits<uint64_t>::max)();
    uint64_t completed = mFence->GetCompletedValue();
    if (completed == removed) return DXGI_ERROR_DEVICE_REMOVED;
    if (completed >= completionValue) return S_OK;

    // An earlier timed-out wait can leave a later event signal pending. Always
    // re-check the fence rather than treating a signaled event as completion.
    HRESULT result = mFence->SetEventOnCompletion(completionValue, mFenceEvent);
    if (FAILED(result)) return result;
    const ULONGLONG start = GetTickCount64();
    for (;;)
    {
        const ULONGLONG elapsed = GetTickCount64() - start;
        const DWORD remaining = timeoutMilliseconds == INFINITE ? INFINITE :
            (elapsed >= timeoutMilliseconds ? 0 : timeoutMilliseconds - static_cast<DWORD>(elapsed));
        const DWORD wait = WaitForSingleObject(mFenceEvent, remaining);
        if (wait == WAIT_FAILED) return HRESULT_FROM_WIN32(GetLastError());
        completed = mFence->GetCompletedValue();
        if (completed == removed) return DXGI_ERROR_DEVICE_REMOVED;
        if (completed >= completionValue) return S_OK;
        if (wait == WAIT_TIMEOUT) return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    }
}

HRESULT piDX12CommandContext::Begin(ID3D12GraphicsCommandList** commands, DWORD timeoutMilliseconds)
{
    if (!commands) return E_POINTER;
    *commands = nullptr;
    if (!mDevice || mRecording || mFailed) return E_UNEXPECTED;
    auto& frame = mFrames[mFrameIndex];
    HRESULT result = Wait(frame.completionValue, timeoutMilliseconds);
    if (FAILED(result)) return result;
    frame.retained.clear();
    frame.completionValue = 0;
    result = frame.allocator->Reset();
    if (SUCCEEDED(result)) result = frame.commands->Reset(frame.allocator.Get(), nullptr);
    if (FAILED(result))
    {
        mFailed = true;
        return result;
    }
    mRecording = true;
    *commands = frame.commands.Get();
    return S_OK;
}

HRESULT piDX12CommandContext::Retain(IUnknown* object)
{
    if (!mRecording || mFailed) return E_UNEXPECTED;
    if (!object) return E_POINTER;
    mFrames[mFrameIndex].retained.emplace_back(object);
    return S_OK;
}

HRESULT piDX12CommandContext::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                        D3D12_RESOURCE_STATES after, UINT subresource)
{
    HRESULT result = Retain(resource);
    if (FAILED(result) || before == after) return result;
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = subresource;
    mFrames[mFrameIndex].commands->ResourceBarrier(1, &barrier);
    return S_OK;
}

HRESULT piDX12CommandContext::UploadBuffer(const void* data, size_t bytes, ID3D12Resource** buffer)
{
    if (!buffer) return E_POINTER;
    *buffer = nullptr;
    if (!mRecording || mFailed) return E_UNEXPECTED;
    if (!data || bytes == 0) return E_INVALIDARG;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    Microsoft::WRL::ComPtr<ID3D12Resource> staging;
    HRESULT result = mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging));
    if (FAILED(result)) return result;
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    Microsoft::WRL::ComPtr<ID3D12Resource> destination;
    result = mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&destination));
    if (FAILED(result)) return result;

    void* mapped = nullptr;
    const D3D12_RANGE noReads = {0, 0};
    result = staging->Map(0, &noReads, &mapped);
    if (FAILED(result)) return result;
    std::memcpy(mapped, data, bytes);
    const D3D12_RANGE written = {0, bytes};
    staging->Unmap(0, &written);

    // Retain before recording any GPU reference. No CPU pointer escapes this call.
    result = Retain(staging.Get());
    if (FAILED(result)) return result;
    result = Retain(destination.Get());
    if (FAILED(result)) return result;
    mFrames[mFrameIndex].commands->CopyBufferRegion(destination.Get(), 0, staging.Get(), 0, bytes);
    result = Transition(destination.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (FAILED(result)) return result;
    *buffer = destination.Detach();
    return S_OK;
}

HRESULT piDX12CommandContext::UploadTexture2D(const D3D12_RESOURCE_DESC& description,
    const TextureData* sources, UINT sourceCount, ID3D12Resource** texture)
{
    if (!texture) return E_POINTER;
    *texture = nullptr;
    if (!mRecording || mFailed) return E_UNEXPECTED;
    if (!sources || description.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        description.Width == 0 || description.Height == 0 || description.MipLevels == 0 ||
        description.DepthOrArraySize == 0 || description.SampleDesc.Count != 1 ||
        description.SampleDesc.Quality != 0 || description.Flags != D3D12_RESOURCE_FLAG_NONE ||
        description.Layout != D3D12_TEXTURE_LAYOUT_UNKNOWN ||
        sourceCount != UINT(description.MipLevels) * description.DepthOrArraySize)
        return E_INVALIDARG;
    D3D12_FEATURE_DATA_FORMAT_INFO format = {description.Format, 0};
    HRESULT result = mDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &format, sizeof(format));
    if (FAILED(result)) return result;
    if (format.PlaneCount != 1) return E_INVALIDARG;

    // Let the device validate the resource description before allocating footprint arrays.
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    Microsoft::WRL::ComPtr<ID3D12Resource> destination;
    result = mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&destination));
    if (FAILED(result)) return result;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(sourceCount);
    std::vector<UINT> rows(sourceCount);
    std::vector<UINT64> rowBytes(sourceCount);
    UINT64 totalBytes = 0;
    mDevice->GetCopyableFootprints(&description, 0, sourceCount, 0,
        footprints.data(), rows.data(), rowBytes.data(), &totalBytes);
    if (totalBytes == 0 || totalBytes == (std::numeric_limits<UINT64>::max)() ||
        totalBytes > (std::numeric_limits<size_t>::max)()) return E_INVALIDARG;
    for (UINT i = 0; i < sourceCount; ++i)
    {
        const auto& source = sources[i];
        // Division avoids overflow when rejecting malformed source pitches/sizes.
        if (!source.data || rows[i] == 0 || source.rowPitch < rowBytes[i] ||
            source.bytes < rowBytes[i] || source.rowPitch == 0 ||
            rows[i] - 1 > (source.bytes - rowBytes[i]) / source.rowPitch)
            return E_INVALIDARG;
    }
    D3D12_RESOURCE_DESC stagingDesc = {};
    stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    stagingDesc.Width = totalBytes;
    stagingDesc.Height = 1;
    stagingDesc.DepthOrArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    Microsoft::WRL::ComPtr<ID3D12Resource> staging;
    result = mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &stagingDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging));
    if (FAILED(result)) return result;
    unsigned char* mapped = nullptr;
    const D3D12_RANGE noReads = {0, 0};
    result = staging->Map(0, &noReads, reinterpret_cast<void**>(&mapped));
    if (FAILED(result)) return result;
    for (UINT i = 0; i < sourceCount; ++i)
        for (UINT row = 0; row < rows[i]; ++row)
            std::memcpy(mapped + footprints[i].Offset + size_t(row) * footprints[i].Footprint.RowPitch,
                static_cast<const unsigned char*>(sources[i].data) + size_t(row) * sources[i].rowPitch,
                static_cast<size_t>(rowBytes[i]));
    const D3D12_RANGE written = {0, static_cast<size_t>(totalBytes)};
    staging->Unmap(0, &written);
    result = Retain(staging.Get());
    if (FAILED(result)) return result;
    result = Retain(destination.Get());
    if (FAILED(result)) return result;
    for (UINT i = 0; i < sourceCount; ++i)
    {
        D3D12_TEXTURE_COPY_LOCATION source = {};
        source.pResource = staging.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprints[i];
        D3D12_TEXTURE_COPY_LOCATION target = {};
        target.pResource = destination.Get();
        target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        target.SubresourceIndex = i;
        mFrames[mFrameIndex].commands->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
    }
    result = Transition(destination.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (FAILED(result)) return result;
    *texture = destination.Detach();
    return S_OK;
}

HRESULT piDX12CommandContext::Submit(uint64_t* completionValue)
{
    if (!completionValue) return E_POINTER;
    *completionValue = 0;
    if (!mRecording || mFailed) return E_UNEXPECTED;
    auto& frame = mFrames[mFrameIndex];
    HRESULT result = frame.commands->Close();
    mRecording = false;
    if (FAILED(result))
    {
        mFailed = true;
        return result;
    }
    ID3D12CommandList* lists[] = {frame.commands.Get()};
    mQueue->ExecuteCommandLists(1, lists);
    const uint64_t next = mLastSubmitted + 1;
    result = mQueue->Signal(mFence.Get(), next);
    if (FAILED(result))
    {
        // Work may already be executing. Keep every reference until Shutdown
        // drains the queue (or observes device removal), and forbid reuse.
        mFailed = true;
        return result;
    }
    frame.completionValue = next;
    mLastSubmitted = next;
    *completionValue = next;
    mFrameIndex = (mFrameIndex + 1) % FrameCount;
    return S_OK;
}

HRESULT piDX12CommandContext::Cancel()
{
    if (!mRecording) return E_UNEXPECTED;
    auto& frame = mFrames[mFrameIndex];
    const HRESULT result = frame.commands->Close();
    mRecording = false;
    frame.retained.clear();
    if (FAILED(result)) mFailed = true;
    return result;
}

HRESULT piDX12CommandContext::Shutdown()
{
    if (mRecording) Cancel();
    HRESULT result = S_OK;
    if (mQueue && mFence && (mLastSubmitted != 0 || mFailed))
    {
        // Signal even after a failed submission so already-enqueued work is
        // covered. Device removal terminates fence waits in D3D12.
        const uint64_t drain = mLastSubmitted + 1;
        result = mQueue->Signal(mFence.Get(), drain);
        if (SUCCEEDED(result))
        {
            mLastSubmitted = drain;
            result = Wait(drain, INFINITE);
        }
        if (FAILED(result) && SUCCEEDED(mDevice->GetDeviceRemovedReason()))
        {
            mFailed = true;
            return result; // No completion proof: retain everything for retry.
        }
    }
    for (auto& frame : mFrames)
    {
        frame.retained.clear();
        frame.commands.Reset();
        frame.allocator.Reset();
        frame.completionValue = 0;
    }
    if (mFenceEvent) CloseHandle(mFenceEvent);
    mFenceEvent = nullptr;
    mFence.Reset();
    mQueue.Reset();
    mDevice.Reset();
    mLastSubmitted = 0;
    mFrameIndex = 0;
    mRecording = false;
    mFailed = false;
    return result;
}
}
