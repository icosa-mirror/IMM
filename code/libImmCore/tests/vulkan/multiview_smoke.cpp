#include <vulkan/vulkan.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <limits>
#include "libImmCore/src/libRender/vulkan/piVulkan_Renderer.h"
#include "libImmPlayer/src/player.h"
#include "libImmPlayer/src/layerRenderers/layerRendererModel/layerRendererModel.h"
#include "appImmUnity/src/imm_unity_render_graph_packet.h"
#include "appImmUnity/src/imm_unity_vulkan_render_graph.h"

namespace {
constexpr uint32_t Size = 64;
void Check(VkResult result, const char* operation)
{
    if (result != VK_SUCCESS) throw std::runtime_error(operation);
}
struct Host
{
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    std::atomic<int> validationErrors{0};
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkImage color = VK_NULL_HANDLE, depth = VK_NULL_HANDLE;
    VkImageView colorView = VK_NULL_HANDLE, depthView = VK_NULL_HANDLE;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
    std::vector<VkDeviceMemory> allocations;
    ~Host()
    {
        if (device) {
            vkDeviceWaitIdle(device);
            vkDestroyFramebuffer(device, framebuffer, nullptr);
            vkDestroyRenderPass(device, pass, nullptr);
            vkDestroyImageView(device, colorView, nullptr);
            vkDestroyImageView(device, depthView, nullptr);
            vkDestroyImage(device, color, nullptr); vkDestroyImage(device, depth, nullptr);
            vkDestroyBuffer(device, readback, nullptr);
            for (auto memory : allocations) vkFreeMemory(device, memory, nullptr);
            vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (messenger) reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"))(instance, messenger, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL Validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void* user)
    {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            ++static_cast<Host*>(user)->validationErrors;
            std::fprintf(stderr, "IMM_VULKAN_MULTIVIEW validation: %s\n", message->pMessage);
        }
        return VK_FALSE;
    }
    VkDeviceMemory Allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags flags)
    {
        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        uint32_t type = 0;
        for (; type < properties.memoryTypeCount; ++type)
            if ((requirements.memoryTypeBits & (1u << type)) &&
                (properties.memoryTypes[type].propertyFlags & flags) == flags) break;
        if (type == properties.memoryTypeCount) throw std::runtime_error("Multiview memory type unavailable");
        VkMemoryAllocateInfo info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        info.allocationSize = requirements.size; info.memoryTypeIndex = type;
        VkDeviceMemory memory;
        Check(vkAllocateMemory(device, &info, nullptr, &memory), "Allocate multiview memory");
        allocations.push_back(memory);
        return memory;
    }
    void Image(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImage& image, VkImageView& view)
    {
        VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D; info.format = format;
        info.extent = {Size, Size, 1}; info.mipLevels = 1; info.arrayLayers = 2;
        info.samples = VK_SAMPLE_COUNT_1_BIT; info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(vkCreateImage(device, &info, nullptr, &image), "Create multiview image");
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, image, &requirements);
        Check(vkBindImageMemory(device, image, Allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), 0), "Bind multiview image");
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; vi.format = format;
        vi.subresourceRange = {aspect, 0, 1, 0, 2};
        Check(vkCreateImageView(device, &vi, nullptr, &view), "Create multiview image view");
    }
    void Init()
    {
        VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_1;
        uint32_t layerCount = 0;
        Check(vkEnumerateInstanceLayerProperties(&layerCount, nullptr), "Enumerate Vulkan validation layers");
        std::vector<VkLayerProperties> layers(layerCount);
        Check(vkEnumerateInstanceLayerProperties(&layerCount, layers.data()), "Read Vulkan validation layers");
        const char* validation = "VK_LAYER_KHRONOS_validation";
        bool validate = false;
        for (const auto& layer : layers) if (std::strcmp(layer.layerName, validation) == 0) validate = true;
        std::printf("IMM_VULKAN_MULTIVIEW validation=%s\n", validate ? "enabled" : "unavailable");
        const char* instanceExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
        VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ii.pApplicationInfo = &app; ii.enabledExtensionCount = validate ? 2 : 1; ii.ppEnabledExtensionNames = instanceExtensions;
        ii.enabledLayerCount = validate ? 1 : 0; ii.ppEnabledLayerNames = &validation;
        Check(vkCreateInstance(&ii, nullptr, &instance), "Create multiview instance");
        if (validate) {
            VkDebugUtilsMessengerCreateInfoEXT debug = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug.pfnUserCallback = Validation; debug.pUserData = this;
            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            if (!create) throw std::runtime_error("Vulkan validation debug entry point unavailable");
            Check(create(instance, &debug, nullptr, &messenger), "Create Vulkan validation callback");
        }
        uint32_t count = 0;
        Check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "Enumerate multiview devices");
        std::vector<VkPhysicalDevice> devices(count);
        Check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "Read multiview devices");
        for (auto candidate : devices) {
            VkPhysicalDeviceMultiviewFeatures multiview = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
            VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &multiview;
            vkGetPhysicalDeviceFeatures2(candidate, &features);
            if (!multiview.multiview) continue;
            uint32_t families = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, nullptr);
            std::vector<VkQueueFamilyProperties> queues(families);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, queues.data());
            for (uint32_t i = 0; i < families; ++i)
                if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { physical = candidate; family = i; break; }
            if (physical) break;
        }
        if (!physical) throw std::runtime_error("Required Vulkan multiview feature unavailable");
        float priority = 1;
        VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex = family; qi.queueCount = 1; qi.pQueuePriorities = &priority;
        VkPhysicalDeviceMultiviewFeatures enabled = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES}; enabled.multiview = VK_TRUE;
        const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        di.pNext = &enabled; di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi;
        di.enabledExtensionCount = 1; di.ppEnabledExtensionNames = extensions;
        Check(vkCreateDevice(physical, &di, nullptr, &device), "Create enabled multiview device");
        vkGetDeviceQueue(device, family, 0, &queue);
        VkCommandPoolCreateInfo pi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pi.queueFamilyIndex = family;
        pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        Check(vkCreateCommandPool(device, &pi, nullptr, &pool), "Create host command pool");
        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        Check(vkAllocateCommandBuffers(device, &ai, &commands), "Allocate host command buffer");
        Image(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, color, colorView);
        Image(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, depth, depthView);
        VkAttachmentDescription attachments[2] = {};
        attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT; attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        attachments[1].format = VK_FORMAT_D32_SFLOAT; attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass = {}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef; subpass.pDepthStencilAttachment = &depthRef;
        uint32_t mask = 3;
        VkRenderPassMultiviewCreateInfo mv = {VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
        mv.subpassCount = 1; mv.pViewMasks = &mask; mv.correlationMaskCount = 1; mv.pCorrelationMasks = &mask;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo ri = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; ri.pNext = &mv;
        ri.attachmentCount = 2; ri.pAttachments = attachments; ri.subpassCount = 1; ri.pSubpasses = &subpass;
        ri.dependencyCount = 1; ri.pDependencies = &dependency;
        Check(vkCreateRenderPass(device, &ri, nullptr, &pass), "Create multiview render pass");
        VkImageView views[] = {colorView, depthView};
        VkFramebufferCreateInfo fi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass = pass; fi.attachmentCount = 2; fi.pAttachments = views; fi.width = Size; fi.height = Size; fi.layers = 1;
        Check(vkCreateFramebuffer(device, &fi, nullptr, &framebuffer), "Create multiview framebuffer");
        VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = Size * Size * 4 * 2; bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(vkCreateBuffer(device, &bi, nullptr, &readback), "Create multiview readback");
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, readback, &requirements);
        readbackMemory = Allocate(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkBindBufferMemory(device, readback, readbackMemory, 0), "Bind multiview readback");
    }
};

Host* adapterHost = nullptr;
bool adapterInside = false;
int adapterLayers = 2;
UnityVulkanInstance UNITY_INTERFACE_API AdapterInstance()
{
    UnityVulkanInstance instance = {};
    instance.instance = adapterHost->instance; instance.physicalDevice = adapterHost->physical;
    instance.device = adapterHost->device; instance.graphicsQueue = adapterHost->queue;
    instance.queueFamilyIndex = adapterHost->family; instance.getInstanceProcAddr = vkGetInstanceProcAddr;
    return instance;
}
bool UNITY_INTERFACE_API AdapterRecording(UnityVulkanRecordingState* recording, UnityVulkanGraphicsQueueAccess access)
{
    if (!adapterInside || access != kUnityVulkanGraphicsQueueAccess_DontCare) return false;
    *recording = {};
    recording->commandBuffer = adapterHost->commands; recording->commandBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    recording->renderPass = adapterHost->pass; recording->framebuffer = adapterHost->framebuffer;
    recording->currentFrameNumber = 1; recording->safeFrameNumber = 0;
    return true;
}
bool UNITY_INTERFACE_API AdapterAttachment(UnityRenderBuffer buffer, const VkImageSubresource* subresource,
    VkImageLayout, VkPipelineStageFlags stages, VkAccessFlags access, UnityVulkanResourceAccessMode mode, UnityVulkanImage* image)
{
    if (subresource || stages || access || mode != kUnityVulkanResourceAccess_ObserveOnly) return false;
    const bool color = reinterpret_cast<uintptr_t>(buffer) == 1;
    if (!color && reinterpret_cast<uintptr_t>(buffer) != 2) return false;
    *image = {};
    image->image = color ? adapterHost->color : adapterHost->depth;
    image->format = color ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_D32_SFLOAT;
    image->extent = {Size, Size, 1}; image->layers = adapterLayers; image->mipCount = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT; image->type = VK_IMAGE_TYPE_2D;
    image->layout = color ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return true;
}
void RunUnityAdapterProbe(Host& host)
{
    using namespace ImmCore;
    std::fprintf(stderr, "IMM_VULKAN_UNITY_ADAPTER begin\n");
    adapterHost = &host;
    IUnityGraphicsVulkan unity = {};
    unity.Instance = AdapterInstance; unity.CommandRecordingState = AdapterRecording;
    unity.AccessRenderBufferTexture = AdapterAttachment;
    ImmVulkanRenderGraphState state; state.unity = &unity;
    state.logFileName = "vulkan-unity-adapter-smoke.log"; state.tmpFolderName = ".";
    // The fixture really enabled multiview. Supply the observer's captured result;
    // its device-creation interception is covered separately by the CPU contract.
    auto& capture = ImmUnityVulkanFeatures::State();
    capture.multiview.store(true); capture.device.store(host.device);
    ImmShared::ImmEngineBridge bridge;
    std::fprintf(stderr, "IMM_VULKAN_UNITY_ADAPTER initialize\n");
    ImmRenderGraphPacket init; init.enableSound = 0;
    bool acknowledge = false;
    if (ProcessImmVulkanRenderGraph(state, bridge, init, ImmRenderGraphPreparationEventId, acknowledge) != 0 ||
        !state.ready || !state.device.multiviewEnabled || !acknowledge) throw std::runtime_error("Initialize Unity Vulkan adapter");
    auto* player = bridge.GetPlayer();
    const int document = player->Load(IMM_VULKAN_SAMPLE_FILE);
    if (document < 0) throw std::runtime_error("Queue adapter sample load");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    for (;;) {
        bridge.GlobalWork(true, 10000);
        ImmRenderGraphPacket maintenance; maintenance.operation = 3;
        if (ProcessImmVulkanRenderGraph(state, bridge, maintenance, ImmRenderGraphPreparationEventId, acknowledge) != 0)
            throw std::runtime_error("Adapter GPU maintenance failed");
        ImmPlayer::Player::DocumentState loading;
        player->GetDocumentState(loading, document);
        if (loading.mLoadingState == ImmPlayer::Player::LoadingState::Loaded) break;
        if (loading.mLoadingState == ImmPlayer::Player::LoadingState::Failed || std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Adapter sample failed or timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    player->SetTime(document, piTick::FromSeconds(3), piTick(0)); bridge.GlobalWork(true, 10000);
    const auto bounds = player->GetDocumentBBox(document);
    const double radius = std::max({bounds.mMaxX - bounds.mMinX, bounds.mMaxY - bounds.mMinY, bounds.mMaxZ - bounds.mMinZ, 1.0}) * .6;
    const auto view = trans3d::translate(-(bounds.mMinX + bounds.mMaxX) * .5,
        -(bounds.mMinY + bounds.mMaxY) * .5, -(bounds.mMinZ + bounds.mMaxZ) * .5 - radius * 2);
    const float nearPlane = float(radius) * .01f, farPlane = float(radius) * 4;
    const mat4x4 projection(1,0,0,0, 0,1,0,0, 0,0,nearPlane/(farPlane-nearPlane),nearPlane*farPlane/(farPlane-nearPlane), 0,0,-1,0);
    const auto head = d2f(toMatrix(view)); auto left = head, right = head;
    left[3] -= float(radius) * .1f; right[3] += float(radius) * .1f;
    ImmRenderGraphPacket packet; packet.operation = 1; packet.viewCount = 2;
    packet.width = packet.height = Size; packet.colorBuffer = 1; packet.depthBuffer = 2;
    packet.colorFormat = 28; packet.depthFormat = 40;
    for (int i = 0; i < 16; ++i) {
        packet.worldToView[i] = head[i]; packet.leftView[i] = left[i]; packet.rightView[i] = right[i];
        packet.projection[i] = packet.leftProjection[i] = packet.rightProjection[i] = projection[i];
    }
    auto prepare = [&]() {
        return ProcessImmVulkanRenderGraph(state, bridge, packet, ImmRenderGraphPreparationEventId, acknowledge);
    };
    const float saved = packet.rightProjection[0]; packet.rightProjection[0] = std::numeric_limits<float>::quiet_NaN();
    if (prepare() >= 0) throw std::runtime_error("Adapter accepted invalid right-eye projection");
    packet.rightProjection[0] = saved;
    if (prepare() != 0 || acknowledge || bridge.GetCameraState(0)->stereoType != 2)
        throw std::runtime_error("Adapter did not prepare preferred stereo");
    // Array mismatch must reject before requesting recording access.
    adapterLayers = 1;
    if (ProcessImmVulkanRenderGraph(state, bridge, packet, ImmRenderGraphEventId, acknowledge) >= 0 || !acknowledge)
        throw std::runtime_error("Adapter accepted mono attachments for stereo packet");
    adapterLayers = 2;
    if (prepare() != 0) throw std::runtime_error("Prepare valid adapter packet");
    Check(vkResetCommandBuffer(host.commands, 0), "Reset adapter host commands");
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    Check(vkBeginCommandBuffer(host.commands, &begin), "Begin adapter host commands");
    VkClearValue clear[2] = {}; // Unity's reversed-Z depth clear is zero.
    VkRenderPassBeginInfo pass = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = host.pass; pass.framebuffer = host.framebuffer; pass.renderArea.extent = {Size, Size};
    pass.clearValueCount = 2; pass.pClearValues = clear;
    vkCmdBeginRenderPass(host.commands, &pass, VK_SUBPASS_CONTENTS_INLINE); adapterInside = true;
    if (ProcessImmVulkanRenderGraph(state, bridge, packet, ImmRenderGraphEventId, acknowledge) != 0 || !acknowledge)
        throw std::runtime_error("Draw preferred stereo through Unity Vulkan adapter");
    adapterInside = false; vkCmdEndRenderPass(host.commands);
    VkBufferImageCopy copy = {}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 2}; copy.imageExtent = {Size, Size, 1};
    vkCmdCopyImageToBuffer(host.commands, host.color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, host.readback, 1, &copy);
    Check(vkEndCommandBuffer(host.commands), "End adapter host commands");
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &host.commands;
    Check(vkQueueSubmit(host.queue, 1, &submit, VK_NULL_HANDLE), "Submit adapter host commands");
    Check(vkQueueWaitIdle(host.queue), "Complete adapter GPU readback");
    void* mapped = nullptr;
    Check(vkMapMemory(host.device, host.readbackMemory, 0, VK_WHOLE_SIZE, 0, &mapped), "Map adapter readback");
    const auto* pixels = static_cast<const unsigned char*>(mapped);
    int coverage[2] = {}, different = 0;
    double centroids[2] = {};
    for (uint32_t i = 0; i < Size * Size; ++i) {
        for (int eye = 0; eye < 2; ++eye) {
            const auto* pixel = pixels + (eye * Size * Size + i) * 4;
            if (std::max({pixel[0], pixel[1], pixel[2]}) > 8) { ++coverage[eye]; centroids[eye] += i % Size; }
        }
        for (int c = 0; c < 3; ++c) if (pixels[i * 4 + c] != pixels[(Size * Size + i) * 4 + c]) { ++different; break; }
    }
    vkUnmapMemory(host.device, host.readbackMemory);
    for (int eye = 0; eye < 2; ++eye) if (coverage[eye]) centroids[eye] /= coverage[eye];
    ImmRenderGraphPacket shutdown; shutdown.operation = 2;
    if (ProcessImmVulkanRenderGraph(state, bridge, shutdown, ImmRenderGraphShutdownEventId, acknowledge) != 0 || state.ready)
        throw std::runtime_error("Shutdown Unity Vulkan adapter");
    capture.device.store(nullptr); capture.multiview.store(false); adapterHost = nullptr;
    std::printf("IMM_VULKAN_UNITY_ADAPTER stereo=preferred coverage=%d,%d different=%d centroids=%.2f,%.2f\n", coverage[0], coverage[1], different, centroids[0], centroids[1]);
    if (coverage[0] < 20 || coverage[1] < 20 || different < 50 || centroids[1] - centroids[0] < 1 ||
        centroids[0] < 16 || centroids[1] > 48 || host.validationErrors.load() != 0)
        throw std::runtime_error("Unity adapter stereo readback or Vulkan validation failed");
    std::puts("IMM_VULKAN_UNITY_ADAPTER PASS prepared two-view packet and scene GPU readback");
}
}

void RunVulkanMultiviewProbe(ImmCore::piRenderer::piReporter& reporter, ImmCore::piLog& log)
{
    using namespace ImmCore;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Host host; host.Init();
    piRendererVulkan renderer;
    piVulkanExternalDevice external = {host.instance, host.physical, host.device, host.queue, host.family, false, false};
    // No capability is inferred from physical support or the presence of array attachments.
    if (!renderer.Initialize(0, nullptr, 0, true, false, &reporter, false, &external))
        throw std::runtime_error("Initialize mono-only borrowed Vulkan renderer");
    if (renderer.SupportsFeature(piRenderer::RendererFeature::MULTIVIEW) ||
        renderer.BeginHostRenderPassFrame(host.commands, reinterpret_cast<void*>(host.pass), reinterpret_cast<void*>(host.framebuffer), VK_FORMAT_R8G8B8A8_UNORM, 1, true, true, 0, Size, Size, 1, 0, false, 2))
        throw std::runtime_error("Borrowed renderer inferred unconfirmed multiview capability");
    renderer.Deinitialize();
    external.multiviewEnabled = true;
    if (!renderer.Initialize(0, nullptr, 0, true, false, &reporter, false, &external) ||
        !renderer.SupportsFeature(piRenderer::RendererFeature::MULTIVIEW)) throw std::runtime_error("Initialize multiview borrowed renderer");
    ImmPlayer::LayerRendererModel models;
    if (!models.Init(&renderer, &log, ImmImporter::Drawing::ColorSpace::Linear, true)) throw std::runtime_error("Initialize production multiview model shaders");
    ImmImporter::LayerModel model;
    if (!model.Init(false, ImmImporter::LayerModel::ShadingModel::Unlit)) throw std::runtime_error("Initialize multiview model");
    auto* mesh = model.GetMesh();
    piMesh::VertexFormat format = {}; format.mStride = 7 * sizeof(float); format.mNumElems = 2;
    format.mElems[0] = {3, piMesh::VertexElemDataType::Float, false, 0};
    format.mElems[1] = {4, piMesh::VertexElemDataType::Float, false, 3 * sizeof(float)};
    if (!mesh->Init(1, 4, &format, piMesh::Type::Polys, 1, 2)) throw std::runtime_error("Allocate multiview mesh");
    float vertices[4][7] = {{-.25f,-.5f,.5f,.5f,.5f,.5f,1}, {.25f,-.5f,.5f,.5f,.5f,.5f,1}, {.25f,.5f,.5f,.5f,.5f,.5f,1}, {-.25f,.5f,.5f,.5f,.5f,.5f,1}};
    for (uint32_t i = 0; i < 4; ++i) mesh->SetVertex(0, i, vertices[i]);
    mesh->SetTriangle(0, 0, 0, 1, 2); mesh->SetTriangle(0, 1, 0, 2, 3); mesh->CalcBBox(0, 0);
    ImmImporter::Layer layer(nullptr, nullptr, 0); layer.SetImplementation(&model);
    if (!models.LoadInCPU(&log, &layer) || !models.LoadInGPU(&renderer, nullptr, &log, &layer)) throw std::runtime_error("Load multiview model");
    float frame[4] = {}, display[36] = {};
    for (int eye = 0; eye < 2; ++eye) for (int axis = 0; axis < 4; ++axis) display[eye * 16 + axis * 5] = 1;
    display[3] = -.4f; display[19] = .4f; display[32] = display[33] = Size;
    auto frameBuffer = renderer.CreateBuffer(frame, sizeof(frame), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto displayBuffer = renderer.CreateBuffer(display, sizeof(display), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    auto layerBuffer = renderer.CreateBuffer(nullptr, sizeof(ImmPlayer::LayersState), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
    if (!frameBuffer || !displayBuffer || !layerBuffer) throw std::runtime_error("Create multiview constants");
    renderer.AttachShaderConstants(frameBuffer, 0); renderer.AttachShaderConstants(layerBuffer, 3); renderer.AttachShaderConstants(displayBuffer, 4);
    const int viewport[] = {0, 0, Size, Size}; renderer.SetViewport(0, viewport);
    // Prime lazy mesh uploads before entering the foreign render pass.
    piRenderer::TextureInfo warmInfo = {piRenderer::TextureType::T2D, piRenderer::Format::C3_11_11_10_FLOAT, Size, Size, 1, 1, 1, 0};
    auto warmColor = renderer.CreateTexture(nullptr, &warmInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1, nullptr);
    auto warmTarget = renderer.CreateRenderTarget(warmColor, nullptr, nullptr, nullptr, nullptr);
    if (!warmColor || !warmTarget) throw std::runtime_error("Create warmup target");
    renderer.SetRenderTarget(warmTarget);
    const float black[4] = {0, 0, 0, 1};
    renderer.Clear(black, nullptr, nullptr, nullptr, false);
    models.PrepareForDisplay(ImmPlayer::StereoMode::None);
    models.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()), trans3d::identity(), 1);
    models.DisplayRender(&renderer, &log, layerBuffer, 0);
    std::vector<unsigned char> warmPixels(Size * Size * 4);
    renderer.GetTextureContent(warmColor, warmPixels.data(), piRenderer::Format::C4_8_UNORM);
    renderer.SetRenderTarget(nullptr); renderer.DestroyRenderTarget(warmTarget); renderer.DestroyTexture(warmColor);
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    Check(vkBeginCommandBuffer(host.commands, &begin), "Begin multiview host commands");
    VkClearValue clear[2] = {}; clear[1].depthStencil.depth = 1;
    VkRenderPassBeginInfo pass = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; pass.renderPass = host.pass; pass.framebuffer = host.framebuffer;
    pass.renderArea.extent = {Size, Size}; pass.clearValueCount = 2; pass.pClearValues = clear;
    vkCmdBeginRenderPass(host.commands, &pass, VK_SUBPASS_CONTENTS_INLINE);
    if (!renderer.BeginHostRenderPassFrame(host.commands, reinterpret_cast<void*>(host.pass), reinterpret_cast<void*>(host.framebuffer), VK_FORMAT_R8G8B8A8_UNORM, 1, true, true, 0, Size, Size, 1, 0, false, 2)) throw std::runtime_error("Begin two-view IMM host frame");
    // Reject invalid requests without tearing down the valid borrowed frame.
    if (renderer.BeginHostRenderPassFrame(host.commands, reinterpret_cast<void*>(host.pass), reinterpret_cast<void*>(host.framebuffer), VK_FORMAT_R8G8B8A8_UNORM, 1, true, true, 0, Size, Size, 1, 0, false, 3) || !renderer.IsExternalHostFrame()) throw std::runtime_error("Invalid view count changed active host frame");
    renderer.SetViewport(0, viewport);
    models.PrepareForDisplay(ImmPlayer::StereoMode::Preferred);
    models.DisplayPreRender(&renderer, nullptr, &log, &layer, frustum3(mat4x4::identity()), trans3d::identity(), 1);
    models.DisplayRender(&renderer, &log, layerBuffer, 0);
    if (!renderer.HostFrameResourcesValid() || models.GetDrawCallInfo().numDrawCalls != 1 || models.GetDrawCallInfo().numTriangles != 2) throw std::runtime_error("Multiview did not submit one model instance");
    renderer.EndExternalImageFrame(); vkCmdEndRenderPass(host.commands);
    VkBufferImageCopy copy = {}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 2}; copy.imageExtent = {Size, Size, 1};
    vkCmdCopyImageToBuffer(host.commands, host.color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, host.readback, 1, &copy);
    Check(vkEndCommandBuffer(host.commands), "End multiview host commands");
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &host.commands;
    Check(vkQueueSubmit(host.queue, 1, &submit, VK_NULL_HANDLE), "Submit multiview host frame");
    Check(vkQueueWaitIdle(host.queue), "Complete multiview readback");
    void* mapped = nullptr;
    Check(vkMapMemory(host.device, host.readbackMemory, 0, VK_WHOLE_SIZE, 0, &mapped), "Map two-layer GPU readback");
    double centroids[2] = {}; int coverage[2] = {};
    auto* pixels = static_cast<unsigned char*>(mapped);
    for (int eye = 0; eye < 2; ++eye) {
        for (uint32_t y = 0; y < Size; ++y) for (uint32_t x = 0; x < Size; ++x)
            if (pixels[((eye * Size + y) * Size + x) * 4] > 30) { centroids[eye] += x; ++coverage[eye]; }
        if (coverage[eye]) centroids[eye] /= coverage[eye];
    }
    vkUnmapMemory(host.device, host.readbackMemory);
    std::printf("IMM_VULKAN_MULTIVIEW layers=2 draws=1 instances=1 coverage=%d,%d centroids=%.2f,%.2f\n", coverage[0], coverage[1], centroids[0], centroids[1]);
    if (coverage[0] < 400 || coverage[1] < 400 || centroids[0] < 15 || centroids[0] > 23 || centroids[1] < 40 || centroids[1] > 48) throw std::runtime_error("Multiview eye matrices or layer broadcast failed");
    models.UnloadInGPU(&renderer, nullptr, &log, &layer); models.UnloadInCPU(&log, &layer);
    models.Deinit(&renderer, &log); mesh->DeInit(); model.Deinit();
    renderer.DestroyBuffer(frameBuffer); renderer.DestroyBuffer(displayBuffer); renderer.DestroyBuffer(layerBuffer);
    renderer.Deinitialize();
    if (host.validationErrors.load() != 0) throw std::runtime_error("Borrowed multiview Vulkan validation errors");
    std::puts("IMM_VULKAN_MULTIVIEW PASS borrowed two-layer production model GPU readback");
    RunUnityAdapterProbe(host);
}
