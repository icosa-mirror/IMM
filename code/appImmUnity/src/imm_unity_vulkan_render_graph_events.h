#pragma once
#include "IUnityGraphicsVulkanMinimal.h"
#include "imm_unity_render_graph_packet.h"

// Draws borrow Unity's recording context. Shutdown instead requires all prior
// recording/submission to finish before the renderer waits for the device.
inline bool ConfigureImmVulkanRenderGraphEvents(IUnityGraphicsVulkan* unity)
{
    if (!unity || !unity->ConfigureEvent) return false;
    UnityVulkanPluginEventConfig draw = {};
    draw.renderPassPrecondition = kUnityVulkanRenderPass_DontCare;
    draw.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
    draw.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission |
        kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
    unity->ConfigureEvent(ImmRenderGraphEventId, &draw);

    UnityVulkanPluginEventConfig shutdown = {};
    shutdown.renderPassPrecondition = kUnityVulkanRenderPass_DontCare;
    shutdown.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_Allow;
    shutdown.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission |
        kUnityVulkanEventConfigFlag_FlushCommandBuffers |
        kUnityVulkanEventConfigFlag_SyncWorkerThreads;
    unity->ConfigureEvent(ImmRenderGraphShutdownEventId, &shutdown);
    UnityVulkanPluginEventConfig preparation = shutdown;
    preparation.flags &= ~kUnityVulkanEventConfigFlag_FlushCommandBuffers;
    unity->ConfigureEvent(ImmRenderGraphPreparationEventId, &preparation);
    return true;
}
