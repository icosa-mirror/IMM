#include "../../../appImmUnity/src/imm_unity_vulkan_render_graph_events.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

struct ConfiguredEvent { int id; UnityVulkanPluginEventConfig config; };
static std::vector<ConfiguredEvent> events;
static void UNITY_INTERFACE_API Capture(int id, const UnityVulkanPluginEventConfig* config)
{
    events.push_back({ id, *config });
}
static void Require(bool valid, const char* message)
{
    if (!valid) throw std::runtime_error(message);
}
int main()
{
    IUnityGraphicsVulkan unity = {};
    Require(!ConfigureImmVulkanRenderGraphEvents(nullptr), "Accepted absent Vulkan interface");
    Require(!ConfigureImmVulkanRenderGraphEvents(&unity), "Accepted absent event configuration API");
    unity.ConfigureEvent = Capture;
    Require(ConfigureImmVulkanRenderGraphEvents(&unity) && events.size() == 2,
        "Failed to configure distinct draw and shutdown events");
    const auto& draw = events[0];
    const auto& shutdown = events[1];
    Require(draw.id == ImmRenderGraphEventId && shutdown.id == ImmRenderGraphShutdownEventId &&
        shutdown.id != draw.id, "Shutdown must have its own event ID");
    Require(draw.config.graphicsQueueAccess == kUnityVulkanGraphicsQueueAccess_DontCare &&
        !(draw.config.flags & kUnityVulkanEventConfigFlag_FlushCommandBuffers),
        "Draws must retain recording access without forced queue flushing");
    Require(draw.config.renderPassPrecondition == kUnityVulkanRenderPass_DontCare,
        "Draw configuration must not force a pass transition under SRP");
    const uint32_t required = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission |
        kUnityVulkanEventConfigFlag_FlushCommandBuffers | kUnityVulkanEventConfigFlag_SyncWorkerThreads;
    Require(shutdown.config.graphicsQueueAccess == kUnityVulkanGraphicsQueueAccess_Allow &&
        (shutdown.config.flags & required) == required,
        "Shutdown must flush recording, synchronize workers and own queue access");
    std::puts("IMM_VULKAN_SHUTDOWN_CONTRACT PASS separate recording and flushed shutdown events");
}
