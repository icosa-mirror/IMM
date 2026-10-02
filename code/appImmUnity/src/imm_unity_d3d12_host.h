#pragma once
#include "IUnityGraphicsD3D12.h"
#include "libImmCore/src/libRender/directx12/piDX12_Renderer.h"

// D3D12 has one Unity submission route: a queue-access plugin event. Unity must
// execute this event after binding the RenderGraph attachments. Its preconditions
// flush preceding Unity work and exclude concurrent worker access to the queue.
// Configure on device initialization; all renderer access, initialization, and
// shutdown must occur inside that configured rendering event. Not yet wired to
// the public plugin entry points: their D3D12 guard stays until integration passes.
class ImmUnityD3D12Host final
{
public:
    bool Configure(IUnityGraphicsD3D12v7* unity, int eventId)
    {
        if (mUnity || !unity || !unity->ConfigureEvent || !unity->GetDevice || !unity->GetCommandQueue)
            return false;
        UnityD3D12PluginEventConfig config = {};
        config.graphicsQueueAccess = kUnityD3D12GraphicsQueueAccess_Allow;
        config.flags = kUnityD3D12EventConfigFlag_FlushCommandBuffers | kUnityD3D12EventConfigFlag_SyncWorkerThreads;
        config.ensureActiveRenderTextureIsBound = false;
        unity->ConfigureEvent(eventId, &config);
        mUnity = unity;
        return true;
    }

    bool InitializeInRenderEvent()
    {
        if (!mUnity || mInitialized) return false;
        mInitialized = mRenderer.InitializeExternal(mUnity->GetDevice(), mUnity->GetCommandQueue());
        return mInitialized;
    }

    // Unity's preceding SetRenderTarget command must have transitioned these
    // attachments to RENDER_TARGET/DEPTH_WRITE before the configured event.
    // Array binding is explicit: mono callers cannot accidentally bind XR attachments.
    HRESULT BindTargetsInRenderEvent(UnityRenderBuffer colorBuffer, UnityRenderBuffer depthBuffer,
        DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat, UINT slices = 1)
    {
        if (!mInitialized || !mRenderer.GetContext() || !mUnity->TextureFromRenderBuffer) return E_UNEXPECTED;
        if (!colorBuffer || !depthBuffer || (slices != 1 && slices != 2)) return E_INVALIDARG;
        auto* color = mUnity->TextureFromRenderBuffer(colorBuffer);
        auto* depth = mUnity->TextureFromRenderBuffer(depthBuffer);
        if (!color || !depth) return E_INVALIDARG;
        const auto colorDesc = color->GetDesc(), depthDesc = depth->GetDesc();
        if (colorDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            depthDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            colorDesc.DepthOrArraySize != slices || depthDesc.DepthOrArraySize != slices ||
            colorDesc.Width != depthDesc.Width || colorDesc.Height != depthDesc.Height ||
            colorDesc.SampleDesc.Count != depthDesc.SampleDesc.Count ||
            colorDesc.SampleDesc.Quality != depthDesc.SampleDesc.Quality ||
            !(colorDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ||
            !(depthDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ||
            !CompatibleFormat(colorDesc.Format, colorFormat) || !CompatibleFormat(depthDesc.Format, depthFormat))
            return E_INVALIDARG;
        auto* device = mUnity->GetDevice();
        for (auto* resource : {color, depth})
        {
            Microsoft::WRL::ComPtr<ID3D12Device> owner;
            if (FAILED(resource->GetDevice(IID_PPV_ARGS(&owner))) || owner.Get() != device) return E_INVALIDARG;
        }
        D3D12_FEATURE_DATA_FORMAT_SUPPORT colorSupport = {colorFormat}, depthSupport = {depthFormat};
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &colorSupport, sizeof(colorSupport))) ||
            FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &depthSupport, sizeof(depthSupport))) ||
            !(colorSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) ||
            !(depthSupport.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) return E_INVALIDARG;
        if (!mRtv)
        {
            D3D12_DESCRIPTOR_HEAP_DESC heap = {}; heap.NumDescriptors = 1;
            heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            HRESULT result = device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&mRtv));
            if (FAILED(result)) return result;
        }
        if (!mDsv)
        {
            D3D12_DESCRIPTOR_HEAP_DESC heap = {}; heap.NumDescriptors = 1;
            heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            HRESULT result = device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&mDsv));
            if (FAILED(result)) return result;
        }
        D3D12_RENDER_TARGET_VIEW_DESC rtv = {}; rtv.Format = colorFormat;
        rtv.ViewDimension = colorDesc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {}; dsv.Format = depthFormat;
        dsv.ViewDimension = depthDesc.SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
        if (slices == 2)
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))) ||
                !options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation)
                return E_NOTIMPL;
            if (colorDesc.SampleDesc.Count > 1)
            {
                rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                rtv.Texture2DMSArray.ArraySize = slices;
                dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                dsv.Texture2DMSArray.ArraySize = slices;
            }
            else
            {
                rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtv.Texture2DArray.ArraySize = slices;
                dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dsv.Texture2DArray.ArraySize = slices;
            }
        }
        device->CreateRenderTargetView(color, &rtv, mRtv->GetCPUDescriptorHandleForHeapStart());
        device->CreateDepthStencilView(depth, &dsv, mDsv->GetCPUDescriptorHandleForHeapStart());
        const ImmCore::piRendererDX12::ExternalTarget target = {color, depth,
            mRtv->GetCPUDescriptorHandleForHeapStart(), mDsv->GetCPUDescriptorHandleForHeapStart(), colorFormat, depthFormat,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_DEPTH_WRITE};
        return mRenderer.SetExternalTarget(target);
    }

    ImmCore::piRendererDX12& RendererInRenderEvent() { return mRenderer; }

    void ShutdownInRenderEvent()
    {
        mRenderer.Deinitialize();
        mRtv.Reset(); mDsv.Reset();
        mInitialized = false;
        mUnity = nullptr;
    }

private:
    static bool CompatibleFormat(DXGI_FORMAT resource, DXGI_FORMAT view)
    {
        if (view == DXGI_FORMAT_UNKNOWN) return false;
        switch (resource)
        {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return view == DXGI_FORMAT_R8G8B8A8_UNORM || view == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return view == DXGI_FORMAT_B8G8R8A8_UNORM || view == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: return view == DXGI_FORMAT_R16G16B16A16_FLOAT || view == DXGI_FORMAT_R16G16B16A16_UNORM;
        case DXGI_FORMAT_R16_TYPELESS: return view == DXGI_FORMAT_D16_UNORM || view == DXGI_FORMAT_R16_FLOAT || view == DXGI_FORMAT_R16_UNORM;
        case DXGI_FORMAT_R32_TYPELESS: return view == DXGI_FORMAT_D32_FLOAT || view == DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R24G8_TYPELESS: return view == DXGI_FORMAT_D24_UNORM_S8_UINT;
        case DXGI_FORMAT_R32G8X24_TYPELESS: return view == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        default: return resource == view;
        }
    }
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtv, mDsv;
    IUnityGraphicsD3D12v7* mUnity = nullptr; // Unity-owned; valid until device shutdown.
    bool mInitialized = false;
    ImmCore::piRendererDX12 mRenderer;
};
