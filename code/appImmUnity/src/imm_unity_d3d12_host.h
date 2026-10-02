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

    // The caller supplies actual bound colour/depth resources and their views.
    // The renderer restores their incoming states when EndFrame submits the list.
    ImmCore::piRendererDX12& RendererInRenderEvent() { return mRenderer; }

    void ShutdownInRenderEvent()
    {
        mRenderer.Deinitialize();
        mInitialized = false;
        mUnity = nullptr;
    }

private:
    IUnityGraphicsD3D12v7* mUnity = nullptr; // Unity-owned; valid until device shutdown.
    bool mInitialized = false;
    ImmCore::piRendererDX12 mRenderer;
};
