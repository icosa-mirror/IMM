#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <vector>

namespace ImmCore
{
// One caller/recording thread. The device and direct queue belong to the host.
// This is submission infrastructure, not a piRenderer implementation. A Unity
// adapter must supply Unity-approved submission before this is used in Unity.
class piDX12CommandContext final
{
public:
    static constexpr size_t FrameCount = 3;

    piDX12CommandContext() = default;
    ~piDX12CommandContext();
    piDX12CommandContext(const piDX12CommandContext&) = delete;
    piDX12CommandContext& operator=(const piDX12CommandContext&) = delete;

    HRESULT Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);
    // Drains submitted work before releasing allocators and retained objects.
    // On a live-device drain failure, keeps resources intact so the host can retry.
    HRESULT Shutdown();
    HRESULT Begin(ID3D12GraphicsCommandList** commands, DWORD timeoutMilliseconds = 10000);
    HRESULT Retain(IUnknown* object);
    // Copies CPU data immediately, then records an upload into a new DEFAULT-heap
    // buffer. Valid only while recording. The returned buffer is GENERIC_READ
    // after execution; Cancel discards its initialization. Both GPU resources
    // survive until completion, even if the caller releases the returned reference.
    // Callers supply any layout/padding required by their view (e.g. CBV alignment).
    // Intended for immutable geometry/data, not a per-frame streaming allocator.
    HRESULT UploadBuffer(const void* data, size_t bytes, ID3D12Resource** buffer);
    struct TextureData
    {
        const void* data;
        size_t bytes;
        size_t rowPitch;
    };
    // Immutable, single-plane 2D textures, including mip chains, arrays and BC
    // formats. Supply one source per D3D12 subresource (mip varies fastest).
    // Uses the same recording/cancellation/lifetime contract as UploadBuffer.
    HRESULT UploadTexture2D(const D3D12_RESOURCE_DESC& description,
                            const TextureData* sources, UINT sourceCount,
                            ID3D12Resource** texture);
    HRESULT Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                       D3D12_RESOURCE_STATES after,
                       UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    HRESULT Submit(uint64_t* completionValue);
    HRESULT Cancel();
    HRESULT Wait(uint64_t completionValue, DWORD timeoutMilliseconds = 10000);

private:
    struct Frame
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
        std::vector<Microsoft::WRL::ComPtr<IUnknown>> retained;
        uint64_t completionValue = 0;
    };

    std::array<Frame, FrameCount> mFrames;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    HANDLE mFenceEvent = nullptr;
    uint64_t mLastSubmitted = 0;
    size_t mFrameIndex = 0;
    bool mRecording = false;
    bool mFailed = false;
};
}
