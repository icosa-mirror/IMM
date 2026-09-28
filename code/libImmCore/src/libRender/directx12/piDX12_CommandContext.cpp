#include "piDX12_CommandContext.h"

#include <limits>
#include <cstdio>

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
