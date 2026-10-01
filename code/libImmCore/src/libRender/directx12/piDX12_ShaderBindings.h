#pragma once

#include "piDX12_CommandContext.h"

namespace ImmCore
{
// IMM's existing HLSL register layout: constants b0..b9, resources t0..t15,
// samplers s0..s15. A Bind creates an immutable descriptor snapshot so later
// layers/cameras cannot overwrite descriptors still being consumed by the GPU.
class piDX12ShaderBindings final
{
public:
    static constexpr UINT ConstantCount = 10;
    static constexpr UINT ResourceCount = 16;
    struct Resources
    {
        ID3D12Resource* constants[ConstantCount] = {};
        ID3D12Resource* resources[ResourceCount] = {};
        D3D12_SHADER_RESOURCE_VIEW_DESC views[ResourceCount] = {};
        D3D12_SAMPLER_DESC samplers[ResourceCount] = {};
    };
    HRESULT Initialize(ID3D12Device* device);
    ID3D12RootSignature* RootSignature() const { return mSignature.Get(); }
    // Commands must be the current list returned by context.Begin(). Resource
    // states and view formats belong to the renderer; Bind retains all resources.
    HRESULT Bind(piDX12CommandContext& context, ID3D12GraphicsCommandList* commands,
                 const Resources& resources);
private:
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mSignature;
};
}
