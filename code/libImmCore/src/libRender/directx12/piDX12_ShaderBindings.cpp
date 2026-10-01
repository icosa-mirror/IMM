#include "piDX12_ShaderBindings.h"

namespace ImmCore
{
HRESULT piDX12ShaderBindings::Initialize(ID3D12Device* device)
{
    if (!device) return E_POINTER;
    if (mDevice) return E_UNEXPECTED;
    D3D12_DESCRIPTOR_RANGE ranges[3] = {};
    ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_CBV, ConstantCount, 0, 0, 0};
    ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ResourceCount, 0, 0, 0};
    ranges[2] = {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ResourceCount, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[3] = {};
    for (UINT i = 0; i < 3; ++i)
    {
        parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[i].DescriptorTable = {1, &ranges[i]};
        parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    D3D12_ROOT_SIGNATURE_DESC description = {};
    description.NumParameters = 3;
    description.pParameters = parameters;
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    HRESULT result = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
    if (FAILED(result)) return result;
    result = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mSignature));
    if (SUCCEEDED(result)) mDevice = device;
    return result;
}

HRESULT piDX12ShaderBindings::Bind(piDX12CommandContext& context, ID3D12GraphicsCommandList* commands,
                                  const Resources& resources)
{
    if (!mDevice || !commands) return E_UNEXPECTED;
    for (auto* constant : resources.constants)
    {
        if (!constant) continue;
        const auto desc = constant->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || desc.Width == 0 ||
            desc.Width > D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16 ||
            desc.Width % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0) return E_INVALIDARG;
    }
    HRESULT result = context.Retain(mSignature.Get());
    if (FAILED(result)) return result;
    D3D12_DESCRIPTOR_HEAP_DESC description = {};
    description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    description.NumDescriptors = ConstantCount + ResourceCount;
    description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> views, samplers;
    result = mDevice->CreateDescriptorHeap(&description, IID_PPV_ARGS(&views));
    if (FAILED(result)) return result;
    description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    description.NumDescriptors = ResourceCount;
    result = mDevice->CreateDescriptorHeap(&description, IID_PPV_ARGS(&samplers));
    if (FAILED(result)) return result;
    auto view = views->GetCPUDescriptorHandleForHeapStart();
    const UINT viewStride = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for (auto* constant : resources.constants)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
        if (constant)
        {
            cbv = {constant->GetGPUVirtualAddress(), static_cast<UINT>(constant->GetDesc().Width)};
            result = context.Retain(constant);
            if (FAILED(result)) return result;
        }
        mDevice->CreateConstantBufferView(constant ? &cbv : nullptr, view);
        view.ptr += viewStride;
    }
    auto sampler = samplers->GetCPUDescriptorHandleForHeapStart();
    const UINT samplerStride = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    for (UINT i = 0; i < ResourceCount; ++i)
    {
        auto srv = resources.views[i];
        if (!resources.resources[i])
        {
            srv = {};
            srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv.Texture2D.MipLevels = 1;
        }
        else
        {
            result = context.Retain(resources.resources[i]);
            if (FAILED(result)) return result;
        }
        mDevice->CreateShaderResourceView(resources.resources[i], &srv, view);
        auto desc = resources.samplers[i];
        if (desc.AddressU == 0)
        {
            desc = {};
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.MaxLOD = D3D12_FLOAT32_MAX;
        }
        mDevice->CreateSampler(&desc, sampler);
        view.ptr += viewStride;
        sampler.ptr += samplerStride;
    }
    result = context.Retain(views.Get());
    if (FAILED(result)) return result;
    result = context.Retain(samplers.Get());
    if (FAILED(result)) return result;
    ID3D12DescriptorHeap* heaps[] = {views.Get(), samplers.Get()};
    commands->SetDescriptorHeaps(2, heaps);
    commands->SetGraphicsRootSignature(mSignature.Get());
    auto gpu = views->GetGPUDescriptorHandleForHeapStart();
    commands->SetGraphicsRootDescriptorTable(0, gpu);
    gpu.ptr += ConstantCount * viewStride;
    commands->SetGraphicsRootDescriptorTable(1, gpu);
    commands->SetGraphicsRootDescriptorTable(2, samplers->GetGPUDescriptorHandleForHeapStart());
    return S_OK;
}
}
