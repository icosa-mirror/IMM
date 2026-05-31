//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015.
// See THIRD_PARTY_LICENSES.txt
//
#include "piVulkan_Renderer.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(ANDROID)
#include <dlfcn.h>
#endif

namespace ImmCore {

typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef int32_t VkResult;
typedef uint32_t VkStructureType;
typedef uint32_t VkQueueFlags;
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkDevice_T *VkDevice;
typedef struct VkQueue_T *VkQueue;
typedef uint64_t VkSurfaceKHR;
typedef uint64_t VkSwapchainKHR;
typedef uint64_t VkImage;
typedef struct VkCommandPool_T *VkCommandPool;
typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef struct VkSemaphore_T *VkSemaphore;
typedef struct VkFence_T *VkFence;
typedef struct VkBuffer_T *VkBuffer;
typedef struct VkDeviceMemory_T *VkDeviceMemory;
typedef uint64_t VkImageView;
typedef uint64_t VkRenderPass;
typedef uint64_t VkFramebuffer;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkFormat;
typedef uint32_t VkColorSpaceKHR;
typedef uint32_t VkPresentModeKHR;
typedef uint32_t VkImageUsageFlags;
typedef uint32_t VkSharingMode;
typedef uint32_t VkSurfaceTransformFlagBitsKHR;
typedef uint32_t VkCompositeAlphaFlagBitsKHR;
typedef uint32_t VkImageType;
typedef uint32_t VkImageTiling;
typedef uint32_t VkSampleCountFlagBits;
typedef uint32_t VkImageCreateFlags;
typedef uint32_t VkImageViewType;
typedef uint32_t VkComponentSwizzle;
typedef uint32_t VkAttachmentDescriptionFlags;
typedef uint32_t VkAttachmentLoadOp;
typedef uint32_t VkAttachmentStoreOp;
typedef uint32_t VkPipelineBindPoint;
typedef uint32_t VkSubpassDescriptionFlags;
typedef uint32_t VkRenderPassCreateFlags;
typedef uint32_t VkFramebufferCreateFlags;
typedef uint32_t VkSubpassContents;
typedef uint32_t VkCommandPoolCreateFlags;
typedef uint32_t VkCommandBufferLevel;
typedef uint32_t VkCommandBufferUsageFlags;
typedef uint32_t VkPipelineStageFlags;
typedef uint32_t VkAccessFlags;
typedef uint32_t VkImageLayout;
typedef uint32_t VkImageAspectFlags;
typedef uint32_t VkDependencyFlags;
typedef uint32_t VkFenceCreateFlags;
typedef uint32_t VkBufferUsageFlags;
typedef uint32_t VkMemoryPropertyFlags;

static constexpr VkInstance VK_NULL_INSTANCE = nullptr;
static constexpr VkPhysicalDevice VK_NULL_PHYSICAL_DEVICE = nullptr;
static constexpr VkDevice VK_NULL_DEVICE = nullptr;
static constexpr VkQueue VK_NULL_QUEUE = nullptr;
static constexpr VkCommandPool VK_NULL_COMMAND_POOL = nullptr;
static constexpr VkCommandBuffer VK_NULL_COMMAND_BUFFER = nullptr;
static constexpr VkSemaphore VK_NULL_SEMAPHORE = nullptr;
static constexpr VkFence VK_NULL_FENCE = nullptr;
static constexpr VkBuffer VK_NULL_BUFFER = nullptr;
static constexpr VkDeviceMemory VK_NULL_DEVICE_MEMORY = nullptr;
static constexpr VkImageView VK_NULL_IMAGE_VIEW = 0;
static constexpr VkRenderPass VK_NULL_RENDER_PASS = 0;
static constexpr VkFramebuffer VK_NULL_FRAMEBUFFER = 0;
static constexpr VkSurfaceKHR VK_NULL_SURFACE_KHR = 0;
static constexpr VkSwapchainKHR VK_NULL_SWAPCHAIN_KHR = 0;
static constexpr VkResult VK_SUCCESS = 0;
static constexpr VkResult VK_NOT_READY = 1;
static constexpr VkResult VK_TIMEOUT = 2;
static constexpr VkStructureType VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;
static constexpr VkStructureType VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3;
static constexpr VkStructureType VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;
static constexpr VkStructureType VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8;
static constexpr VkStructureType VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9;
static constexpr VkStructureType VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12;
static constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14;
static constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;
static constexpr VkStructureType VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5;
static constexpr VkStructureType VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO = 37;
static constexpr VkStructureType VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO = 38;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;
static constexpr VkStructureType VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO = 43;
static constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 44;
static constexpr VkStructureType VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000;
static constexpr VkStructureType VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR = 1000001000;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001;
static constexpr VkQueueFlags VK_QUEUE_GRAPHICS_BIT = 0x00000001;
static constexpr VkFormat VK_FORMAT_B8G8R8A8_UNORM = 44;
static constexpr VkFormat VK_FORMAT_R8G8B8A8_UNORM = 37;
static constexpr VkFormat VK_FORMAT_R8G8B8A8_SRGB = 43;
static constexpr VkFormat VK_FORMAT_R8_UNORM = 9;
static constexpr VkFormat VK_FORMAT_R16G16B16A16_SFLOAT = 97;
static constexpr VkFormat VK_FORMAT_R32G32B32A32_SFLOAT = 109;
static constexpr VkFormat VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122;
static constexpr VkFormat VK_FORMAT_D32_SFLOAT = 126;
static constexpr VkFormat VK_FORMAT_D16_UNORM = 124;
static constexpr VkFormat VK_FORMAT_D24_UNORM_S8_UINT = 129;
static constexpr VkFormat VK_FORMAT_D32_SFLOAT_S8_UINT = 130;
static constexpr VkColorSpaceKHR VK_COLOR_SPACE_SRGB_NONLINEAR_KHR = 0;
static constexpr VkPresentModeKHR VK_PRESENT_MODE_IMMEDIATE_KHR = 0;
static constexpr VkPresentModeKHR VK_PRESENT_MODE_MAILBOX_KHR = 1;
static constexpr VkPresentModeKHR VK_PRESENT_MODE_FIFO_KHR = 2;
static constexpr VkImageUsageFlags VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002;
static constexpr VkImageUsageFlags VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001;
static constexpr VkImageUsageFlags VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004;
static constexpr VkImageUsageFlags VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010;
static constexpr VkImageUsageFlags VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080;
static constexpr VkBufferUsageFlags VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT = 0x00000100;
static constexpr VkSharingMode VK_SHARING_MODE_EXCLUSIVE = 0;
static constexpr VkCompositeAlphaFlagBitsKHR VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR = 0x00000001;
static constexpr VkCommandPoolCreateFlags VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002;
static constexpr VkCommandBufferLevel VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0;
static constexpr VkCommandBufferUsageFlags VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000;
static constexpr VkAccessFlags VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000;
static constexpr VkAccessFlags VK_ACCESS_MEMORY_READ_BIT = 0x00008000;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_UNDEFINED = 0;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002;
static constexpr VkImageAspectFlags VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001;
static constexpr VkImageAspectFlags VK_IMAGE_ASPECT_DEPTH_BIT = 0x00000002;
static constexpr VkImageAspectFlags VK_IMAGE_ASPECT_STENCIL_BIT = 0x00000004;
static constexpr VkFenceCreateFlags VK_FENCE_CREATE_SIGNALED_BIT = 0x00000001;
static constexpr uint32_t VK_QUEUE_FAMILY_IGNORED = 0xffffffffu;
static constexpr VkMemoryPropertyFlags VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001;
static constexpr VkMemoryPropertyFlags VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002;
static constexpr VkMemoryPropertyFlags VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004;
static constexpr VkImageType VK_IMAGE_TYPE_2D = 1;
static constexpr VkImageTiling VK_IMAGE_TILING_OPTIMAL = 0;
static constexpr VkSampleCountFlagBits VK_SAMPLE_COUNT_1_BIT = 0x00000001;
static constexpr VkSampleCountFlagBits VK_SAMPLE_COUNT_2_BIT = 0x00000002;
static constexpr VkSampleCountFlagBits VK_SAMPLE_COUNT_4_BIT = 0x00000004;
static constexpr VkSampleCountFlagBits VK_SAMPLE_COUNT_8_BIT = 0x00000008;
static constexpr VkSampleCountFlagBits VK_SAMPLE_COUNT_16_BIT = 0x00000010;
static constexpr VkImageViewType VK_IMAGE_VIEW_TYPE_2D = 1;
static constexpr VkComponentSwizzle VK_COMPONENT_SWIZZLE_IDENTITY = 0;
static constexpr VkAttachmentLoadOp VK_ATTACHMENT_LOAD_OP_LOAD = 0;
static constexpr VkAttachmentLoadOp VK_ATTACHMENT_LOAD_OP_CLEAR = 1;
static constexpr VkAttachmentLoadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE = 2;
static constexpr VkAttachmentStoreOp VK_ATTACHMENT_STORE_OP_STORE = 0;
static constexpr VkAttachmentStoreOp VK_ATTACHMENT_STORE_OP_DONT_CARE = 1;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3;
static constexpr VkPipelineBindPoint VK_PIPELINE_BIND_POINT_GRAPHICS = 0;
static constexpr VkSubpassContents VK_SUBPASS_CONTENTS_INLINE = 0;

#define IMM_VK_MAKE_VERSION(major, minor, patch) ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))
#define IMM_VK_API_VERSION_1_0 IMM_VK_MAKE_VERSION(1, 0, 0)

struct VkApplicationInfo
{
    VkStructureType sType;
    const void *pNext;
    const char *pApplicationName;
    uint32_t applicationVersion;
    const char *pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
};

struct VkInstanceCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    const VkApplicationInfo *pApplicationInfo;
    uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
};

struct VkDeviceQueueCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    const float *pQueuePriorities;
};

struct VkDeviceCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo *pQueueCreateInfos;
    uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
    const void *pEnabledFeatures;
};

struct VkQueueFamilyProperties
{
    VkQueueFlags queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    struct
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    } minImageTransferGranularity;
};

struct VkExtent2D
{
    uint32_t width;
    uint32_t height;
};

struct VkOffset2D
{
    int32_t x;
    int32_t y;
};

struct VkRect2D
{
    VkOffset2D offset;
    VkExtent2D extent;
};

struct VkSurfaceCapabilitiesKHR
{
    uint32_t minImageCount;
    uint32_t maxImageCount;
    VkExtent2D currentExtent;
    VkExtent2D minImageExtent;
    VkExtent2D maxImageExtent;
    uint32_t maxImageArrayLayers;
    VkFlags supportedTransforms;
    VkSurfaceTransformFlagBitsKHR currentTransform;
    VkFlags supportedCompositeAlpha;
    VkImageUsageFlags supportedUsageFlags;
};

struct VkSurfaceFormatKHR
{
    VkFormat format;
    VkColorSpaceKHR colorSpace;
};

struct VkSwapchainCreateInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    VkSurfaceKHR surface;
    uint32_t minImageCount;
    VkFormat imageFormat;
    VkColorSpaceKHR imageColorSpace;
    VkExtent2D imageExtent;
    uint32_t imageArrayLayers;
    VkImageUsageFlags imageUsage;
    VkSharingMode imageSharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t *pQueueFamilyIndices;
    VkSurfaceTransformFlagBitsKHR preTransform;
    VkCompositeAlphaFlagBitsKHR compositeAlpha;
    VkPresentModeKHR presentMode;
    VkBool32 clipped;
    VkSwapchainKHR oldSwapchain;
};

struct VkMemoryType
{
    VkMemoryPropertyFlags propertyFlags;
    uint32_t heapIndex;
};

struct VkMemoryHeap
{
    VkDeviceSize size;
    VkFlags flags;
};

struct VkPhysicalDeviceMemoryProperties
{
    uint32_t memoryTypeCount;
    VkMemoryType memoryTypes[32];
    uint32_t memoryHeapCount;
    VkMemoryHeap memoryHeaps[16];
};

struct VkBufferCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkSharingMode sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t *pQueueFamilyIndices;
};

struct VkExtent3D
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct VkImageCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkImageCreateFlags flags;
    VkImageType imageType;
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkSampleCountFlagBits samples;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkSharingMode sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t *pQueueFamilyIndices;
    VkImageLayout initialLayout;
};

struct VkComponentMapping
{
    VkComponentSwizzle r;
    VkComponentSwizzle g;
    VkComponentSwizzle b;
    VkComponentSwizzle a;
};

struct VkImageSubresourceRange
{
    VkImageAspectFlags aspectMask;
    uint32_t baseMipLevel;
    uint32_t levelCount;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
};

struct VkImageViewCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    VkImage image;
    VkImageViewType viewType;
    VkFormat format;
    VkComponentMapping components;
    VkImageSubresourceRange subresourceRange;
};

struct VkAttachmentDescription
{
    VkAttachmentDescriptionFlags flags;
    VkFormat format;
    VkSampleCountFlagBits samples;
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp;
    VkAttachmentLoadOp stencilLoadOp;
    VkAttachmentStoreOp stencilStoreOp;
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
};

struct VkAttachmentReference
{
    uint32_t attachment;
    VkImageLayout layout;
};

struct VkSubpassDescription
{
    VkSubpassDescriptionFlags flags;
    VkPipelineBindPoint pipelineBindPoint;
    uint32_t inputAttachmentCount;
    const VkAttachmentReference *pInputAttachments;
    uint32_t colorAttachmentCount;
    const VkAttachmentReference *pColorAttachments;
    const VkAttachmentReference *pResolveAttachments;
    const VkAttachmentReference *pDepthStencilAttachment;
    uint32_t preserveAttachmentCount;
    const uint32_t *pPreserveAttachments;
};

struct VkRenderPassCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkRenderPassCreateFlags flags;
    uint32_t attachmentCount;
    const VkAttachmentDescription *pAttachments;
    uint32_t subpassCount;
    const VkSubpassDescription *pSubpasses;
    uint32_t dependencyCount;
    const void *pDependencies;
};

struct VkFramebufferCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFramebufferCreateFlags flags;
    VkRenderPass renderPass;
    uint32_t attachmentCount;
    const VkImageView *pAttachments;
    uint32_t width;
    uint32_t height;
    uint32_t layers;
};

union VkClearColorValue
{
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
};

union VkClearValue
{
    VkClearColorValue color;
    float depthStencil[2];
};

struct VkRenderPassBeginInfo
{
    VkStructureType sType;
    const void *pNext;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    VkRect2D renderArea;
    uint32_t clearValueCount;
    const VkClearValue *pClearValues;
};

struct VkMemoryRequirements
{
    VkDeviceSize size;
    VkDeviceSize alignment;
    uint32_t memoryTypeBits;
};

struct VkMemoryAllocateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkDeviceSize allocationSize;
    uint32_t memoryTypeIndex;
};

struct VkCommandPoolCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkCommandPoolCreateFlags flags;
    uint32_t queueFamilyIndex;
};

struct VkCommandBufferAllocateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkCommandPool commandPool;
    VkCommandBufferLevel level;
    uint32_t commandBufferCount;
};

struct VkCommandBufferBeginInfo
{
    VkStructureType sType;
    const void *pNext;
    VkCommandBufferUsageFlags flags;
    const void *pInheritanceInfo;
};

struct VkSemaphoreCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
};

struct VkFenceCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFenceCreateFlags flags;
};

struct VkImageSubresourceLayers
{
    VkImageAspectFlags aspectMask;
    uint32_t mipLevel;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
};

struct VkOffset3D
{
    int32_t x;
    int32_t y;
    int32_t z;
};

struct VkBufferImageCopy
{
    VkDeviceSize bufferOffset;
    uint32_t bufferRowLength;
    uint32_t bufferImageHeight;
    VkImageSubresourceLayers imageSubresource;
    VkOffset3D imageOffset;
    VkExtent3D imageExtent;
};

struct VkImageMemoryBarrier
{
    VkStructureType sType;
    const void *pNext;
    VkAccessFlags srcAccessMask;
    VkAccessFlags dstAccessMask;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    VkImage image;
    VkImageSubresourceRange subresourceRange;
};

struct VkSubmitInfo
{
    VkStructureType sType;
    const void *pNext;
    uint32_t waitSemaphoreCount;
    const VkSemaphore *pWaitSemaphores;
    const VkPipelineStageFlags *pWaitDstStageMask;
    uint32_t commandBufferCount;
    const VkCommandBuffer *pCommandBuffers;
    uint32_t signalSemaphoreCount;
    const VkSemaphore *pSignalSemaphores;
};

struct VkPresentInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    uint32_t waitSemaphoreCount;
    const VkSemaphore *pWaitSemaphores;
    uint32_t swapchainCount;
    const VkSwapchainKHR *pSwapchains;
    const uint32_t *pImageIndices;
    VkResult *pResults;
};

#if defined(WINDOWS)
struct VkWin32SurfaceCreateInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    HINSTANCE hinstance;
    HWND hwnd;
};
#endif

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *name);
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo *createInfo, const void *allocator, VkInstance *instance);
typedef void (*PFN_vkDestroyInstance)(VkInstance instance, const void *allocator);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance instance, uint32_t *physicalDeviceCount, VkPhysicalDevice *physicalDevices);
typedef void (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice physicalDevice, uint32_t *queueFamilyPropertyCount, VkQueueFamilyProperties *queueFamilyProperties);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *createInfo, const void *allocator, VkDevice *device);
typedef void (*PFN_vkDestroyDevice)(VkDevice device, const void *allocator);
typedef void (*PFN_vkGetDeviceQueue)(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *queue);
typedef VkResult (*PFN_vkDeviceWaitIdle)(VkDevice device);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32 *supported);
typedef void (*PFN_vkDestroySurfaceKHR)(VkInstance instance, VkSurfaceKHR surface, const void *allocator);
typedef PFN_vkVoidFunction (*PFN_vkGetDeviceProcAddr)(VkDevice device, const char *name);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *surfaceCapabilities);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *surfaceFormatCount, VkSurfaceFormatKHR *surfaceFormats);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *presentModeCount, VkPresentModeKHR *presentModes);
typedef void (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties *memoryProperties);
typedef VkResult (*PFN_vkCreateSwapchainKHR)(VkDevice device, const VkSwapchainCreateInfoKHR *createInfo, const void *allocator, VkSwapchainKHR *swapchain);
typedef void (*PFN_vkDestroySwapchainKHR)(VkDevice device, VkSwapchainKHR swapchain, const void *allocator);
typedef VkResult (*PFN_vkGetSwapchainImagesKHR)(VkDevice device, VkSwapchainKHR swapchain, uint32_t *swapchainImageCount, VkImage *swapchainImages);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice device, const VkCommandPoolCreateInfo *createInfo, const void *allocator, VkCommandPool *commandPool);
typedef void (*PFN_vkDestroyCommandPool)(VkDevice device, VkCommandPool commandPool, const void *allocator);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice device, const VkCommandBufferAllocateInfo *allocateInfo, VkCommandBuffer *commandBuffers);
typedef VkResult (*PFN_vkResetCommandBuffer)(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *beginInfo);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer commandBuffer);
typedef void (*PFN_vkCmdPipelineBarrier)(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const void *memoryBarriers, uint32_t bufferMemoryBarrierCount, const void *bufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *imageMemoryBarriers);
typedef void (*PFN_vkCmdClearColorImage)(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue *color, uint32_t rangeCount, const VkImageSubresourceRange *ranges);
typedef void (*PFN_vkCmdCopyBufferToImage)(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy *regions);
typedef void (*PFN_vkCmdBeginRenderPass)(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *renderPassBegin, VkSubpassContents contents);
typedef void (*PFN_vkCmdEndRenderPass)(VkCommandBuffer commandBuffer);
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice device, const VkBufferCreateInfo *createInfo, const void *allocator, VkBuffer *buffer);
typedef void (*PFN_vkDestroyBuffer)(VkDevice device, VkBuffer buffer, const void *allocator);
typedef void (*PFN_vkGetBufferMemoryRequirements)(VkDevice device, VkBuffer buffer, VkMemoryRequirements *memoryRequirements);
typedef VkResult (*PFN_vkCreateImage)(VkDevice device, const VkImageCreateInfo *createInfo, const void *allocator, VkImage *image);
typedef void (*PFN_vkDestroyImage)(VkDevice device, VkImage image, const void *allocator);
typedef void (*PFN_vkGetImageMemoryRequirements)(VkDevice device, VkImage image, VkMemoryRequirements *memoryRequirements);
typedef VkResult (*PFN_vkCreateImageView)(VkDevice device, const VkImageViewCreateInfo *createInfo, const void *allocator, VkImageView *view);
typedef void (*PFN_vkDestroyImageView)(VkDevice device, VkImageView imageView, const void *allocator);
typedef VkResult (*PFN_vkCreateRenderPass)(VkDevice device, const VkRenderPassCreateInfo *createInfo, const void *allocator, VkRenderPass *renderPass);
typedef void (*PFN_vkDestroyRenderPass)(VkDevice device, VkRenderPass renderPass, const void *allocator);
typedef VkResult (*PFN_vkCreateFramebuffer)(VkDevice device, const VkFramebufferCreateInfo *createInfo, const void *allocator, VkFramebuffer *framebuffer);
typedef void (*PFN_vkDestroyFramebuffer)(VkDevice device, VkFramebuffer framebuffer, const void *allocator);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice device, const VkMemoryAllocateInfo *allocateInfo, const void *allocator, VkDeviceMemory *memory);
typedef void (*PFN_vkFreeMemory)(VkDevice device, VkDeviceMemory memory, const void *allocator);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset);
typedef VkResult (*PFN_vkBindImageMemory)(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset);
typedef VkResult (*PFN_vkMapMemory)(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkFlags flags, void **data);
typedef void (*PFN_vkUnmapMemory)(VkDevice device, VkDeviceMemory memory);
typedef VkResult (*PFN_vkCreateSemaphore)(VkDevice device, const VkSemaphoreCreateInfo *createInfo, const void *allocator, VkSemaphore *semaphore);
typedef void (*PFN_vkDestroySemaphore)(VkDevice device, VkSemaphore semaphore, const void *allocator);
typedef VkResult (*PFN_vkCreateFence)(VkDevice device, const VkFenceCreateInfo *createInfo, const void *allocator, VkFence *fence);
typedef void (*PFN_vkDestroyFence)(VkDevice device, VkFence fence, const void *allocator);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice device, uint32_t fenceCount, const VkFence *fences, VkBool32 waitAll, uint64_t timeout);
typedef VkResult (*PFN_vkResetFences)(VkDevice device, uint32_t fenceCount, const VkFence *fences);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *submits, VkFence fence);
typedef VkResult (*PFN_vkAcquireNextImageKHR)(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *imageIndex);
typedef VkResult (*PFN_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR *presentInfo);
#if defined(WINDOWS)
typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *createInfo, const void *allocator, VkSurfaceKHR *surface);
#endif

struct piShaderS
{
    const uint8_t *vs = nullptr;
    int vsLen = 0;
    const uint8_t *fs = nullptr;
    int fsLen = 0;
    piShaderOptions options = {};
    bool hasOptions = false;
};

struct piTextureS
{
    piRenderer::TextureInfo info = {};
    piRenderer::TextureFilter filter = piRenderer::TextureFilter::NONE;
    piRenderer::TextureWrap wrap = piRenderer::TextureWrap::CLAMP;
    uint8_t *data = nullptr;
    size_t dataSize = 0;
    uint64_t externalHandle = 0;
    VkImage image = 0;
    VkImageView imageView = VK_NULL_IMAGE_VIEW;
    VkDeviceMemory memory = VK_NULL_DEVICE_MEMORY;
    VkFormat vkFormat = 0;
    VkImageUsageFlags imageUsage = 0;
};

struct piBufferS
{
    uint8_t *data = nullptr;
    unsigned int size = 0;
    piRenderer::BufferType type = piRenderer::BufferType::Static;
    piRenderer::BufferUse use = piRenderer::BufferUse::Vertex;
    VkBuffer buffer = VK_NULL_BUFFER;
    VkDeviceMemory memory = VK_NULL_DEVICE_MEMORY;
    VkBufferUsageFlags usage = 0;
};

struct piVertexArrayS
{
    piBuffer vertexBuffer[2] = { nullptr, nullptr };
    piBuffer indexBuffer = nullptr;
    piRenderer::IndexArrayFormat indexFormat = piRenderer::IndexArrayFormat::UINT_32;
};

struct piRTargetS
{
    piTexture color[4] = { nullptr, nullptr, nullptr, nullptr };
    piTexture depth = nullptr;
    VkRenderPass renderPass = VK_NULL_RENDER_PASS;
    VkFramebuffer framebuffer = VK_NULL_FRAMEBUFFER;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t colorAttachmentCount = 0;
    bool hasDepth = false;
};

struct piSamplerS
{
    piRenderer::TextureFilter filter = piRenderer::TextureFilter::NONE;
    piRenderer::TextureWrap wrap = piRenderer::TextureWrap::CLAMP;
    float anisotropy = 1.0f;
};

struct piRasterStateS
{
    bool wireframe = false;
    bool frontIsCounterClockWise = true;
    piRenderer::CullMode cullMode = piRenderer::CullMode::NONE;
    bool depthClamp = false;
    bool multiSample = false;
};

struct piBlendStateS
{
    bool alphaToCoverage = false;
    bool enabled0 = false;
};

struct piDepthStateS
{
    bool alphaToCoverage = false;
    bool lessEqual = true;
};

struct piQueryS
{
    piRenderer::QueryType type = piRenderer::QueryType::TimeElapsed;
    uint64_t startNanoseconds = 0;
    uint64_t resultNanoseconds = 0;
    bool active = false;
};

enum class piVulkanUnsupportedFeature : int
{
    DrawSubmission = 0,
    SourceShaderCompilation,
    RenderTargetOperations,
    ImageLoadStore,
    Compute,
    Atomics,
    PixelPackBuffer,
    TextureReadback,
    ExternalTexture,
    Count
};

struct piVulkanState
{
    VkInstance instance = VK_NULL_INSTANCE;
    VkPhysicalDevice physicalDevice = VK_NULL_PHYSICAL_DEVICE;
    VkDevice device = VK_NULL_DEVICE;
    VkQueue graphicsQueue = VK_NULL_QUEUE;
    VkSurfaceKHR surface = VK_NULL_SURFACE_KHR;
    VkSwapchainKHR swapchain = VK_NULL_SWAPCHAIN_KHR;
    VkImage swapchainImages[8] = {};
    VkImageView swapchainImageViews[8] = {};
    VkFramebuffer swapchainFramebuffers[8] = {};
    uint32_t swapchainImageCount = 0;
    VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapchainExtent = {};
    VkRenderPass swapchainRenderPass = VK_NULL_RENDER_PASS;
    VkCommandPool commandPool = VK_NULL_COMMAND_POOL;
    VkCommandBuffer commandBuffer = VK_NULL_COMMAND_BUFFER;
    VkSemaphore imageAvailableSemaphore = VK_NULL_SEMAPHORE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_SEMAPHORE;
    VkFence frameFence = VK_NULL_FENCE;
    VkBuffer stagingBuffer = VK_NULL_BUFFER;
    VkDeviceMemory stagingMemory = VK_NULL_DEVICE_MEMORY;
    VkDeviceSize stagingSize = 0;
    piTexture pendingPresentTexture = nullptr;
    uint32_t presentFrameIndex = 0;
    bool realPresentReported = false;
    bool texturePresentReported = false;
    bool textureImageReported = false;
    bool bufferReported = false;
#if defined(WINDOWS)
    HMODULE vulkanLibrary = nullptr;
    HWND window = nullptr;
    int windowWidth = 1;
    int windowHeight = 1;
    bool captureWritten = false;
#elif defined(ANDROID)
    void *vulkanLibrary = nullptr;
#endif
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdClearColorImage vkCmdClearColorImage = nullptr;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;
    PFN_vkWaitForFences vkWaitForFences = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
#if defined(WINDOWS)
    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
#endif
    uint32_t graphicsQueueFamilyIndex = 0;
    bool ownsInstance = false;
    bool ownsDevice = false;
    bool initialized = false;
    int activeWindow = -1;
    int numViewports = 1;
    float viewports[6 * 16] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
    piRTarget currentRenderTarget = nullptr;
    piShader currentShader = nullptr;
    piVertexArray currentVertexArray = nullptr;
    piRasterState currentRasterState = nullptr;
    piBlendState currentBlendState = nullptr;
    piDepthState currentDepthState = nullptr;
    piTexture textures[16] = {};
    piSampler samplers[8] = {};
    piBuffer constantBuffers[16] = {};
    piBuffer shaderBuffers[16] = {};
    piQuery perfQueries[2] = { nullptr, nullptr };
    int currentPerformanceQuery = 0;
    bool unsupportedReported[(int)piVulkanUnsupportedFeature::Count] = {};
    bool cpuDrawDiagnosticReported = false;
    bool cpuPaintDiagnosticReported = false;
    bool cpuPresentDiagnosticReported = false;
    uint32_t cpuPaintDrawCount = 0;
    uint64_t liveRenderTargets = 0;
    uint64_t liveRasterStates = 0;
    uint64_t liveBlendStates = 0;
    uint64_t liveDepthStates = 0;
    uint64_t liveTextures = 0;
    uint64_t liveSamplers = 0;
    uint64_t liveShaders = 0;
    uint64_t liveBuffers = 0;
    uint64_t liveVertexArrays = 0;
    uint64_t liveQueries = 0;
};

struct iCpuStaticVertex
{
    float pos[3];
    uint32_t wid;
    uint8_t col[3];
    uint8_t alp;
    uint8_t dir[3];
    uint8_t info;
    uint32_t axu;
    uint32_t axv;
    float tim;
};

struct iCpuStaticVertexPacked
{
    float pos[3];
    uint32_t widInfo;
    uint8_t col[3];
    uint8_t alp;
    uint32_t axu;
    uint32_t axv;
};

struct iCpuLayerState
{
    float layerToViewer[16];
    float scale;
    float opacity;
    float flipSign;
    float drawInTime;
    float animParam[4];
    float keepAlive[8];
    uint32_t id;
};

struct iCpuDisplayState
{
    float eyeToClip[16];
    float viewerToEye[16];
    float viewerToClip[16];
};

struct iCpuChunkData
{
    uint32_t vertexOffset;
    float biggestStroke;
};

static uint64_t iNowNanoseconds()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

static void iReport(piRenderer::piReporter *reporter, const char *message)
{
    if (reporter)
    {
        reporter->Info(message);
    }
}

static void iError(piRenderer::piReporter *reporter, const char *message)
{
    if (reporter)
    {
        reporter->Error(message, 0);
    }
}

static void iUnsupported(piVulkanState *state, piRenderer::piReporter *reporter, piVulkanUnsupportedFeature feature, const char *message)
{
    const int index = (int)feature;
    if (!state || index < 0 || index >= (int)piVulkanUnsupportedFeature::Count || state->unsupportedReported[index])
    {
        return;
    }
    state->unsupportedReported[index] = true;
    iError(reporter, message);
}

static size_t iBytesPerPixel(piRenderer::Format format)
{
    switch (format)
    {
        case piRenderer::Format::C1_8_UNORM: return 1;
        case piRenderer::Format::C4_8_UNORM:
        case piRenderer::Format::C4_8_UNORM_SRGB:
        case piRenderer::Format::C3_11_11_10_FLOAT: return 4;
        case piRenderer::Format::C4_16_FLOAT: return 8;
        case piRenderer::Format::C4_32_FLOAT: return 16;
        default: return 0;
    }
}

static size_t iTextureDataSize(const piRenderer::TextureInfo *info)
{
    if (!info)
    {
        return 0;
    }
    const size_t bytesPerPixel = iBytesPerPixel(info->mFormat);
    if (bytesPerPixel == 0 || info->mXres <= 0 || info->mYres <= 0 || info->mZres <= 0)
    {
        return 0;
    }
    return (size_t)info->mXres * (size_t)info->mYres * (size_t)info->mZres * bytesPerPixel;
}

static int iShaderOption(const piShader shader, const char *name, int fallback)
{
    if (!shader || !shader->hasOptions || !name)
    {
        return fallback;
    }
    for (int i = 0; i < shader->options.mNum; ++i)
    {
        if (std::strcmp(shader->options.mOption[i].mName, name) == 0)
        {
            return shader->options.mOption[i].mValue;
        }
    }
    return fallback;
}

static void iTextureWritePixel(piTexture texture, int x, int y, float r, float g, float b, float a)
{
    if (!texture || !texture->data || x < 0 || y < 0 || x >= texture->info.mXres || y >= texture->info.mYres)
    {
        return;
    }
    uint8_t *dst = texture->data + ((size_t)y * (size_t)texture->info.mXres + (size_t)x) * 4u;
    const float invA = 1.0f - a;
    dst[0] = (uint8_t)(r * 255.0f * a + (float)dst[0] * invA);
    dst[1] = (uint8_t)(g * 255.0f * a + (float)dst[1] * invA);
    dst[2] = (uint8_t)(b * 255.0f * a + (float)dst[2] * invA);
    dst[3] = 255;
}

static void iMulPoint(const float *m, const float *p, float *out)
{
    // piLib matrices are consumed here in the same memory order as the HLSL row-major constants.
    out[0] = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
    out[1] = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
    out[2] = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
    out[3] = m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15];
}

static void iDrawCpuLine(piTexture texture, float x0, float y0, float x1, float y1, float width, float r, float g, float b, float a)
{
    if (!texture || !texture->data)
    {
        return;
    }
    int steps = (int)(std::abs(x1 - x0) + std::abs(y1 - y0)) + 1;
    if (steps > 64)
    {
        steps = 64;
    }
    const int radius = (int)(width < 1.0f ? 1.0f : (width > 2.0f ? 2.0f : width));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = steps > 0 ? (float)i / (float)steps : 0.0f;
        const int cx = (int)(x0 + (x1 - x0) * t);
        const int cy = (int)(y0 + (y1 - y0) * t);
        for (int yy = -radius; yy <= radius; ++yy)
        {
            for (int xx = -radius; xx <= radius; ++xx)
            {
                if (xx * xx + yy * yy <= radius * radius)
                {
                    iTextureWritePixel(texture, cx + xx, cy + yy, r, g, b, a);
                }
            }
        }
    }
}

#if defined(WINDOWS)
static void iWritePpmCapture(piVulkanState *state, piTexture texture)
{
    if (!state || !texture || !texture->data)
    {
        return;
    }
    const char *path = std::getenv("IMM_VULKAN_CPU_CAPTURE_PATH");
    if (!path || path[0] == 0)
    {
        return;
    }
    const bool overwriteCapture = std::getenv("IMM_VULKAN_CPU_CAPTURE_OVERWRITE") != nullptr;
    if (state->captureWritten && !overwriteCapture)
    {
        return;
    }
    bool hasColor = false;
    const size_t pixelCount = (size_t)texture->info.mXres * (size_t)texture->info.mYres;
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const uint8_t *src = texture->data + i * 4u;
        if (src[0] != 0 || src[1] != 0 || src[2] != 0)
        {
            hasColor = true;
            break;
        }
    }
    if (!hasColor)
    {
        return;
    }
    FILE *file = std::fopen(path, "wb");
    if (!file)
    {
        return;
    }
    std::fprintf(file, "P6\n%d %d\n255\n", texture->info.mXres, texture->info.mYres);
    for (int y = 0; y < texture->info.mYres; ++y)
    {
        for (int x = 0; x < texture->info.mXres; ++x)
        {
            const uint8_t *src = texture->data + ((size_t)y * (size_t)texture->info.mXres + (size_t)x) * 4u;
            const uint8_t rgb[3] = { src[0], src[1], src[2] };
            std::fwrite(rgb, 1, sizeof(rgb), file);
        }
    }
    std::fclose(file);
    state->captureWritten = true;
}
#endif

static bool iLoadVulkanEntryPoints(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state)
    {
        return false;
    }

#if defined(WINDOWS)
    state->vulkanLibrary = LoadLibraryW(L"vulkan-1.dll");
    if (!state->vulkanLibrary)
    {
        iError(reporter, "Vulkan renderer could not load vulkan-1.dll");
        return false;
    }
    state->vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(state->vulkanLibrary, "vkGetInstanceProcAddr");
#elif defined(ANDROID)
    state->vulkanLibrary = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!state->vulkanLibrary)
    {
        iError(reporter, "Vulkan renderer could not load libvulkan.so");
        return false;
    }
    state->vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(state->vulkanLibrary, "vkGetInstanceProcAddr");
#else
    iError(reporter, "Vulkan dynamic loading is not implemented for this platform yet");
    return false;
#endif

    if (!state->vkGetInstanceProcAddr)
    {
        iError(reporter, "Vulkan renderer could not load vkGetInstanceProcAddr");
        return false;
    }

    state->vkCreateInstance = (PFN_vkCreateInstance)state->vkGetInstanceProcAddr(nullptr, "vkCreateInstance");
    if (!state->vkCreateInstance)
    {
        iError(reporter, "Vulkan renderer could not load vkCreateInstance");
        return false;
    }
    return true;
}

static bool iLoadVulkanInstanceEntryPoints(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || !state->vkGetInstanceProcAddr || !state->instance)
    {
        return false;
    }
    state->vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)state->vkGetInstanceProcAddr(state->instance, "vkEnumeratePhysicalDevices");
    state->vkDestroyInstance = (PFN_vkDestroyInstance)state->vkGetInstanceProcAddr(state->instance, "vkDestroyInstance");
    state->vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)state->vkGetInstanceProcAddr(state->instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    state->vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)state->vkGetInstanceProcAddr(state->instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    state->vkCreateDevice = (PFN_vkCreateDevice)state->vkGetInstanceProcAddr(state->instance, "vkCreateDevice");
    state->vkDestroyDevice = (PFN_vkDestroyDevice)state->vkGetInstanceProcAddr(state->instance, "vkDestroyDevice");
    state->vkGetDeviceQueue = (PFN_vkGetDeviceQueue)state->vkGetInstanceProcAddr(state->instance, "vkGetDeviceQueue");
    state->vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)state->vkGetInstanceProcAddr(state->instance, "vkDeviceWaitIdle");
    state->vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)state->vkGetInstanceProcAddr(state->instance, "vkDestroySurfaceKHR");
    state->vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)state->vkGetInstanceProcAddr(state->instance, "vkGetDeviceProcAddr");
    state->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)state->vkGetInstanceProcAddr(state->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    state->vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)state->vkGetInstanceProcAddr(state->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    state->vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)state->vkGetInstanceProcAddr(state->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    state->vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)state->vkGetInstanceProcAddr(state->instance, "vkGetPhysicalDeviceMemoryProperties");
#if defined(WINDOWS)
    state->vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)state->vkGetInstanceProcAddr(state->instance, "vkCreateWin32SurfaceKHR");
#endif
    if (!state->vkDestroyInstance || !state->vkEnumeratePhysicalDevices || !state->vkGetPhysicalDeviceQueueFamilyProperties ||
        !state->vkGetPhysicalDeviceMemoryProperties || !state->vkCreateDevice || !state->vkDestroyDevice ||
        !state->vkGetDeviceQueue || !state->vkDeviceWaitIdle)
    {
        iError(reporter, "Vulkan renderer could not load required Vulkan entry points");
        return false;
    }
    return true;
}

static bool iLoadVulkanSwapchainEntryPoints(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || !state->device || !state->vkGetDeviceProcAddr)
    {
        iError(reporter, "Vulkan renderer cannot load swapchain entry points without vkGetDeviceProcAddr");
        return false;
    }
    state->vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)state->vkGetDeviceProcAddr(state->device, "vkCreateSwapchainKHR");
    state->vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)state->vkGetDeviceProcAddr(state->device, "vkDestroySwapchainKHR");
    state->vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)state->vkGetDeviceProcAddr(state->device, "vkGetSwapchainImagesKHR");
    state->vkCreateCommandPool = (PFN_vkCreateCommandPool)state->vkGetDeviceProcAddr(state->device, "vkCreateCommandPool");
    state->vkDestroyCommandPool = (PFN_vkDestroyCommandPool)state->vkGetDeviceProcAddr(state->device, "vkDestroyCommandPool");
    state->vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)state->vkGetDeviceProcAddr(state->device, "vkAllocateCommandBuffers");
    state->vkResetCommandBuffer = (PFN_vkResetCommandBuffer)state->vkGetDeviceProcAddr(state->device, "vkResetCommandBuffer");
    state->vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)state->vkGetDeviceProcAddr(state->device, "vkBeginCommandBuffer");
    state->vkEndCommandBuffer = (PFN_vkEndCommandBuffer)state->vkGetDeviceProcAddr(state->device, "vkEndCommandBuffer");
    state->vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)state->vkGetDeviceProcAddr(state->device, "vkCmdPipelineBarrier");
    state->vkCmdClearColorImage = (PFN_vkCmdClearColorImage)state->vkGetDeviceProcAddr(state->device, "vkCmdClearColorImage");
    state->vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)state->vkGetDeviceProcAddr(state->device, "vkCmdCopyBufferToImage");
    state->vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)state->vkGetDeviceProcAddr(state->device, "vkCmdBeginRenderPass");
    state->vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)state->vkGetDeviceProcAddr(state->device, "vkCmdEndRenderPass");
    state->vkCreateBuffer = (PFN_vkCreateBuffer)state->vkGetDeviceProcAddr(state->device, "vkCreateBuffer");
    state->vkDestroyBuffer = (PFN_vkDestroyBuffer)state->vkGetDeviceProcAddr(state->device, "vkDestroyBuffer");
    state->vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)state->vkGetDeviceProcAddr(state->device, "vkGetBufferMemoryRequirements");
    state->vkCreateImage = (PFN_vkCreateImage)state->vkGetDeviceProcAddr(state->device, "vkCreateImage");
    state->vkDestroyImage = (PFN_vkDestroyImage)state->vkGetDeviceProcAddr(state->device, "vkDestroyImage");
    state->vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)state->vkGetDeviceProcAddr(state->device, "vkGetImageMemoryRequirements");
    state->vkCreateImageView = (PFN_vkCreateImageView)state->vkGetDeviceProcAddr(state->device, "vkCreateImageView");
    state->vkDestroyImageView = (PFN_vkDestroyImageView)state->vkGetDeviceProcAddr(state->device, "vkDestroyImageView");
    state->vkCreateRenderPass = (PFN_vkCreateRenderPass)state->vkGetDeviceProcAddr(state->device, "vkCreateRenderPass");
    state->vkDestroyRenderPass = (PFN_vkDestroyRenderPass)state->vkGetDeviceProcAddr(state->device, "vkDestroyRenderPass");
    state->vkCreateFramebuffer = (PFN_vkCreateFramebuffer)state->vkGetDeviceProcAddr(state->device, "vkCreateFramebuffer");
    state->vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)state->vkGetDeviceProcAddr(state->device, "vkDestroyFramebuffer");
    state->vkAllocateMemory = (PFN_vkAllocateMemory)state->vkGetDeviceProcAddr(state->device, "vkAllocateMemory");
    state->vkFreeMemory = (PFN_vkFreeMemory)state->vkGetDeviceProcAddr(state->device, "vkFreeMemory");
    state->vkBindBufferMemory = (PFN_vkBindBufferMemory)state->vkGetDeviceProcAddr(state->device, "vkBindBufferMemory");
    state->vkBindImageMemory = (PFN_vkBindImageMemory)state->vkGetDeviceProcAddr(state->device, "vkBindImageMemory");
    state->vkMapMemory = (PFN_vkMapMemory)state->vkGetDeviceProcAddr(state->device, "vkMapMemory");
    state->vkUnmapMemory = (PFN_vkUnmapMemory)state->vkGetDeviceProcAddr(state->device, "vkUnmapMemory");
    state->vkCreateSemaphore = (PFN_vkCreateSemaphore)state->vkGetDeviceProcAddr(state->device, "vkCreateSemaphore");
    state->vkDestroySemaphore = (PFN_vkDestroySemaphore)state->vkGetDeviceProcAddr(state->device, "vkDestroySemaphore");
    state->vkCreateFence = (PFN_vkCreateFence)state->vkGetDeviceProcAddr(state->device, "vkCreateFence");
    state->vkDestroyFence = (PFN_vkDestroyFence)state->vkGetDeviceProcAddr(state->device, "vkDestroyFence");
    state->vkWaitForFences = (PFN_vkWaitForFences)state->vkGetDeviceProcAddr(state->device, "vkWaitForFences");
    state->vkResetFences = (PFN_vkResetFences)state->vkGetDeviceProcAddr(state->device, "vkResetFences");
    state->vkQueueSubmit = (PFN_vkQueueSubmit)state->vkGetDeviceProcAddr(state->device, "vkQueueSubmit");
    state->vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)state->vkGetDeviceProcAddr(state->device, "vkAcquireNextImageKHR");
    state->vkQueuePresentKHR = (PFN_vkQueuePresentKHR)state->vkGetDeviceProcAddr(state->device, "vkQueuePresentKHR");
    if (!state->vkCreateSwapchainKHR || !state->vkDestroySwapchainKHR || !state->vkGetSwapchainImagesKHR ||
        !state->vkCreateCommandPool || !state->vkDestroyCommandPool || !state->vkAllocateCommandBuffers ||
        !state->vkResetCommandBuffer || !state->vkBeginCommandBuffer || !state->vkEndCommandBuffer ||
        !state->vkCmdPipelineBarrier || !state->vkCmdClearColorImage || !state->vkCmdCopyBufferToImage ||
        !state->vkCmdBeginRenderPass || !state->vkCmdEndRenderPass ||
        !state->vkCreateBuffer || !state->vkDestroyBuffer || !state->vkGetBufferMemoryRequirements ||
        !state->vkCreateImage || !state->vkDestroyImage || !state->vkGetImageMemoryRequirements ||
        !state->vkCreateImageView || !state->vkDestroyImageView ||
        !state->vkCreateRenderPass || !state->vkDestroyRenderPass || !state->vkCreateFramebuffer || !state->vkDestroyFramebuffer ||
        !state->vkAllocateMemory || !state->vkFreeMemory || !state->vkBindBufferMemory || !state->vkBindImageMemory ||
        !state->vkMapMemory || !state->vkUnmapMemory || !state->vkCreateSemaphore ||
        !state->vkDestroySemaphore || !state->vkCreateFence || !state->vkDestroyFence || !state->vkWaitForFences ||
        !state->vkResetFences || !state->vkQueueSubmit || !state->vkAcquireNextImageKHR || !state->vkQueuePresentKHR)
    {
        iError(reporter, "Vulkan renderer could not load required swapchain frame entry points");
        return false;
    }
    return true;
}

static bool iCreateVulkanSurface(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state)
    {
        return false;
    }
#if defined(WINDOWS)
    if (!state->window)
    {
        return true;
    }
    if (!state->vkCreateWin32SurfaceKHR)
    {
        iError(reporter, "Vulkan renderer could not load vkCreateWin32SurfaceKHR");
        return false;
    }
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandleW(nullptr);
    surfaceInfo.hwnd = state->window;
    const VkResult result = state->vkCreateWin32SurfaceKHR(state->instance, &surfaceInfo, nullptr, &state->surface);
    if (result != VK_SUCCESS || state->surface == VK_NULL_SURFACE_KHR)
    {
        iError(reporter, "Vulkan renderer failed to create Win32 VkSurfaceKHR");
        return false;
    }
    iReport(reporter, "Vulkan renderer created Win32 surface");
    return true;
#else
    return true;
#endif
}

static uint32_t iClampUint32(uint32_t value, uint32_t lo, uint32_t hi)
{
    if (value < lo) return lo;
    if (hi != 0 && value > hi) return hi;
    return value;
}

static bool iCreateSwapchainRenderTargets(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->swapchainImageCount == 0)
    {
        return true;
    }

    for (uint32_t i = 0; i < state->swapchainImageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = state->swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = state->swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkResult result = state->vkCreateImageView(state->device, &viewInfo, nullptr, &state->swapchainImageViews[i]);
        if (result != VK_SUCCESS || state->swapchainImageViews[i] == VK_NULL_IMAGE_VIEW)
        {
            iError(reporter, "Vulkan renderer failed to create swapchain image view");
            return false;
        }
    }

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = state->swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    VkResult result = state->vkCreateRenderPass(state->device, &renderPassInfo, nullptr, &state->swapchainRenderPass);
    if (result != VK_SUCCESS || state->swapchainRenderPass == VK_NULL_RENDER_PASS)
    {
        iError(reporter, "Vulkan renderer failed to create swapchain render pass");
        return false;
    }

    for (uint32_t i = 0; i < state->swapchainImageCount; ++i)
    {
        VkImageView attachments[] = { state->swapchainImageViews[i] };
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = state->swapchainRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = state->swapchainExtent.width;
        framebufferInfo.height = state->swapchainExtent.height;
        framebufferInfo.layers = 1;
        result = state->vkCreateFramebuffer(state->device, &framebufferInfo, nullptr, &state->swapchainFramebuffers[i]);
        if (result != VK_SUCCESS || state->swapchainFramebuffers[i] == VK_NULL_FRAMEBUFFER)
        {
            iError(reporter, "Vulkan renderer failed to create swapchain framebuffer");
            return false;
        }
    }

    iReport(reporter, "Vulkan renderer created swapchain render pass and framebuffers");
    return true;
}

static bool iCreateVulkanSwapchain(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->surface == VK_NULL_SURFACE_KHR)
    {
        return true;
    }
    if (!state->vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !state->vkGetPhysicalDeviceSurfaceFormatsKHR ||
        !state->vkGetPhysicalDeviceSurfacePresentModesKHR)
    {
        iError(reporter, "Vulkan renderer could not load required surface query entry points");
        return false;
    }
    if (!iLoadVulkanSwapchainEntryPoints(state, reporter))
    {
        return false;
    }

    VkSurfaceCapabilitiesKHR capabilities = {};
    VkResult result = state->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &capabilities);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to query surface capabilities");
        return false;
    }

    uint32_t formatCount = 0;
    result = state->vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0)
    {
        iError(reporter, "Vulkan renderer found no surface formats");
        return false;
    }
    VkSurfaceFormatKHR formats[16] = {};
    if (formatCount > 16)
    {
        formatCount = 16;
    }
    result = state->vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, formats);
    if (result != VK_SUCCESS || formatCount == 0)
    {
        iError(reporter, "Vulkan renderer failed to query surface formats");
        return false;
    }

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (uint32_t i = 0; i < formatCount; ++i)
    {
        if ((formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            selectedFormat = formats[i];
            break;
        }
    }

    uint32_t presentModeCount = 0;
    result = state->vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0)
    {
        iError(reporter, "Vulkan renderer found no present modes");
        return false;
    }
    VkPresentModeKHR presentModes[16] = {};
    if (presentModeCount > 16)
    {
        presentModeCount = 16;
    }
    result = state->vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount, presentModes);
    if (result != VK_SUCCESS || presentModeCount == 0)
    {
        iError(reporter, "Vulkan renderer failed to query present modes");
        return false;
    }
    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < presentModeCount; ++i)
    {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            selectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            selectedPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == 0xffffffffu || extent.height == 0xffffffffu)
    {
        extent.width = iClampUint32((uint32_t)state->windowWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = iClampUint32((uint32_t)state->windowHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount != 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = state->surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = selectedFormat.format;
    swapchainInfo.imageColorSpace = selectedFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = selectedPresentMode;
    swapchainInfo.clipped = 1;
    swapchainInfo.oldSwapchain = VK_NULL_SWAPCHAIN_KHR;

    result = state->vkCreateSwapchainKHR(state->device, &swapchainInfo, nullptr, &state->swapchain);
    if (result != VK_SUCCESS || state->swapchain == VK_NULL_SWAPCHAIN_KHR)
    {
        iError(reporter, "Vulkan renderer failed to create swapchain");
        return false;
    }

    uint32_t swapchainImageCount = 0;
    result = state->vkGetSwapchainImagesKHR(state->device, state->swapchain, &swapchainImageCount, nullptr);
    if (result != VK_SUCCESS || swapchainImageCount == 0)
    {
        iError(reporter, "Vulkan renderer failed to query swapchain image count");
        return false;
    }
    if (swapchainImageCount > 8)
    {
        swapchainImageCount = 8;
    }
    result = state->vkGetSwapchainImagesKHR(state->device, state->swapchain, &swapchainImageCount, state->swapchainImages);
    if (result != VK_SUCCESS || swapchainImageCount == 0)
    {
        iError(reporter, "Vulkan renderer failed to query swapchain images");
        return false;
    }

    state->swapchainImageCount = swapchainImageCount;
    state->swapchainFormat = selectedFormat.format;
    state->swapchainExtent = extent;
    if (!iCreateSwapchainRenderTargets(state, reporter))
    {
        return false;
    }
    char message[256];
    std::snprintf(message,
                  sizeof(message),
                  "Vulkan renderer created swapchain %ux%u images=%u format=%u",
                  extent.width,
                  extent.height,
                  swapchainImageCount,
                  selectedFormat.format);
    iReport(reporter, message);
    return true;
}

static bool iCreateVulkanFrameResources(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->swapchain == VK_NULL_SWAPCHAIN_KHR)
    {
        return true;
    }

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = state->graphicsQueueFamilyIndex;
    VkResult result = state->vkCreateCommandPool(state->device, &poolInfo, nullptr, &state->commandPool);
    if (result != VK_SUCCESS || state->commandPool == VK_NULL_COMMAND_POOL)
    {
        iError(reporter, "Vulkan renderer failed to create command pool");
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = state->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    result = state->vkAllocateCommandBuffers(state->device, &allocInfo, &state->commandBuffer);
    if (result != VK_SUCCESS || state->commandBuffer == VK_NULL_COMMAND_BUFFER)
    {
        iError(reporter, "Vulkan renderer failed to allocate command buffer");
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = state->vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->imageAvailableSemaphore);
    if (result != VK_SUCCESS || state->imageAvailableSemaphore == VK_NULL_SEMAPHORE)
    {
        iError(reporter, "Vulkan renderer failed to create image-available semaphore");
        return false;
    }
    result = state->vkCreateSemaphore(state->device, &semaphoreInfo, nullptr, &state->renderFinishedSemaphore);
    if (result != VK_SUCCESS || state->renderFinishedSemaphore == VK_NULL_SEMAPHORE)
    {
        iError(reporter, "Vulkan renderer failed to create render-finished semaphore");
        return false;
    }

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = state->vkCreateFence(state->device, &fenceInfo, nullptr, &state->frameFence);
    if (result != VK_SUCCESS || state->frameFence == VK_NULL_FENCE)
    {
        iError(reporter, "Vulkan renderer failed to create frame fence");
        return false;
    }
    iReport(reporter, "Vulkan renderer created frame resources");
    return true;
}

static bool iFindMemoryType(piVulkanState *state, uint32_t typeBits, VkMemoryPropertyFlags requiredFlags, uint32_t *outTypeIndex)
{
    if (!state || !state->vkGetPhysicalDeviceMemoryProperties || !outTypeIndex)
    {
        return false;
    }
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    state->vkGetPhysicalDeviceMemoryProperties(state->physicalDevice, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags)
        {
            *outTypeIndex = i;
            return true;
        }
    }
    return false;
}

static VkBufferUsageFlags iBufferUsageFlags(piRenderer::BufferUse use)
{
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    switch (use)
    {
        case piRenderer::BufferUse::Vertex:
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case piRenderer::BufferUse::Index:
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case piRenderer::BufferUse::Constant:
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case piRenderer::BufferUse::ShaderResource:
        case piRenderer::BufferUse::Atomics:
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case piRenderer::BufferUse::DrawCommands:
            flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            break;
        case piRenderer::BufferUse::Pixel:
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        default:
            break;
    }
    return flags;
}

static bool iUploadBufferData(piVulkanState *state, piBuffer buffer, const void *data, unsigned int offset, unsigned int len, piRenderer::piReporter *reporter)
{
    if (!state || !buffer || !data || buffer->memory == VK_NULL_DEVICE_MEMORY || len == 0)
    {
        return true;
    }
    void *mapped = nullptr;
    VkResult result = state->vkMapMemory(state->device, buffer->memory, offset, len, 0, &mapped);
    if (result != VK_SUCCESS || !mapped)
    {
        iError(reporter, "Vulkan renderer failed to map buffer memory");
        return false;
    }
    std::memcpy(mapped, data, len);
    state->vkUnmapMemory(state->device, buffer->memory);
    return true;
}

static bool iCreateBufferObject(piVulkanState *state, piBuffer buffer, const void *data, piRenderer::piReporter *reporter)
{
    if (!state || !buffer || state->device == VK_NULL_DEVICE || buffer->size == 0)
    {
        return true;
    }
    if (!state->vkCreateBuffer || !state->vkGetBufferMemoryRequirements || !state->vkAllocateMemory ||
        !state->vkBindBufferMemory || !state->vkMapMemory || !state->vkUnmapMemory)
    {
        return true;
    }

    buffer->usage = iBufferUsageFlags(buffer->use);
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buffer->size;
    bufferInfo.usage = buffer->usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = state->vkCreateBuffer(state->device, &bufferInfo, nullptr, &buffer->buffer);
    if (result != VK_SUCCESS || buffer->buffer == VK_NULL_BUFFER)
    {
        iError(reporter, "Vulkan renderer failed to create buffer");
        buffer->buffer = VK_NULL_BUFFER;
        return false;
    }

    VkMemoryRequirements requirements = {};
    state->vkGetBufferMemoryRequirements(state->device, buffer->buffer, &requirements);
    uint32_t memoryTypeIndex = 0;
    if (!iFindMemoryType(state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memoryTypeIndex))
    {
        iError(reporter, "Vulkan renderer failed to find host-visible buffer memory");
        state->vkDestroyBuffer(state->device, buffer->buffer, nullptr);
        buffer->buffer = VK_NULL_BUFFER;
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, &buffer->memory);
    if (result != VK_SUCCESS || buffer->memory == VK_NULL_DEVICE_MEMORY)
    {
        iError(reporter, "Vulkan renderer failed to allocate buffer memory");
        state->vkDestroyBuffer(state->device, buffer->buffer, nullptr);
        buffer->buffer = VK_NULL_BUFFER;
        return false;
    }

    result = state->vkBindBufferMemory(state->device, buffer->buffer, buffer->memory, 0);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to bind buffer memory");
        state->vkFreeMemory(state->device, buffer->memory, nullptr);
        state->vkDestroyBuffer(state->device, buffer->buffer, nullptr);
        buffer->memory = VK_NULL_DEVICE_MEMORY;
        buffer->buffer = VK_NULL_BUFFER;
        return false;
    }

    if (data && !iUploadBufferData(state, buffer, data, 0, buffer->size, reporter))
    {
        state->vkFreeMemory(state->device, buffer->memory, nullptr);
        state->vkDestroyBuffer(state->device, buffer->buffer, nullptr);
        buffer->memory = VK_NULL_DEVICE_MEMORY;
        buffer->buffer = VK_NULL_BUFFER;
        return false;
    }

    if (!state->bufferReported)
    {
        iReport(reporter, "Vulkan renderer created VkBuffer-backed renderer buffer");
        state->bufferReported = true;
    }
    return true;
}

static bool iToVulkanTextureFormat(piRenderer::Format format, VkFormat *outFormat, bool *outDepth)
{
    if (!outFormat || !outDepth)
    {
        return false;
    }
    *outDepth = false;
    switch (format)
    {
        case piRenderer::Format::C1_8_UNORM:
            *outFormat = VK_FORMAT_R8_UNORM;
            return true;
        case piRenderer::Format::C4_8_UNORM:
            *outFormat = VK_FORMAT_R8G8B8A8_UNORM;
            return true;
        case piRenderer::Format::C4_8_UNORM_SRGB:
            *outFormat = VK_FORMAT_R8G8B8A8_SRGB;
            return true;
        case piRenderer::Format::C4_16_FLOAT:
            *outFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
            return true;
        case piRenderer::Format::C4_32_FLOAT:
            *outFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            return true;
        case piRenderer::Format::C3_11_11_10_FLOAT:
            *outFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            return true;
        case piRenderer::Format::D1_32_FLOAT:
            *outFormat = VK_FORMAT_D32_SFLOAT;
            *outDepth = true;
            return true;
        case piRenderer::Format::D1_16_UNORM:
            *outFormat = VK_FORMAT_D16_UNORM;
            *outDepth = true;
            return true;
        case piRenderer::Format::DS_24_8_UINT:
            *outFormat = VK_FORMAT_D24_UNORM_S8_UINT;
            *outDepth = true;
            return true;
        case piRenderer::Format::DS_32_8_UINT:
            *outFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
            *outDepth = true;
            return true;
        default:
            return false;
    }
}

static VkImageAspectFlags iTextureAspectMask(piRenderer::Format format)
{
    switch (format)
    {
        case piRenderer::Format::D1_32_FLOAT:
        case piRenderer::Format::D1_16_UNORM:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case piRenderer::Format::DS_24_8_UINT:
        case piRenderer::Format::DS_32_8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

static VkSampleCountFlagBits iTextureSampleCount(const piRenderer::TextureInfo &info)
{
    switch (info.mMultisample)
    {
        case 2: return VK_SAMPLE_COUNT_2_BIT;
        case 4: return VK_SAMPLE_COUNT_4_BIT;
        case 8: return VK_SAMPLE_COUNT_8_BIT;
        case 16: return VK_SAMPLE_COUNT_16_BIT;
        default: return VK_SAMPLE_COUNT_1_BIT;
    }
}

static bool iCreateTextureImage(piVulkanState *state, piTexture texture, int bindUsage, piRenderer::piReporter *reporter)
{
    (void)bindUsage;
    if (!state || !texture || state->device == VK_NULL_DEVICE || texture->info.mType != piRenderer::TextureType::T2D ||
        texture->info.mXres <= 0 || texture->info.mYres <= 0)
    {
        return true;
    }
    if (!state->vkCreateImage || !state->vkGetImageMemoryRequirements || !state->vkAllocateMemory || !state->vkBindImageMemory ||
        !state->vkCreateImageView)
    {
        return false;
    }

    bool depth = false;
    VkFormat vkFormat = 0;
    if (!iToVulkanTextureFormat(texture->info.mFormat, &vkFormat, &depth))
    {
        return true;
    }

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    usage |= depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent.width = (uint32_t)texture->info.mXres;
    imageInfo.extent.height = (uint32_t)texture->info.mYres;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = texture->info.mMultisample > 1 ? 1 : (texture->info.mNumMips > 0 ? texture->info.mNumMips : 1);
    imageInfo.arrayLayers = 1;
    imageInfo.samples = iTextureSampleCount(texture->info);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = state->vkCreateImage(state->device, &imageInfo, nullptr, &texture->image);
    if (result != VK_SUCCESS || texture->image == 0)
    {
        iError(reporter, "Vulkan renderer failed to create VkImage-backed texture");
        texture->image = 0;
        return false;
    }

    VkMemoryRequirements requirements = {};
    state->vkGetImageMemoryRequirements(state->device, texture->image, &requirements);
    uint32_t memoryTypeIndex = 0;
    if (!iFindMemoryType(state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryTypeIndex))
    {
        iError(reporter, "Vulkan renderer failed to find device-local texture memory");
        state->vkDestroyImage(state->device, texture->image, nullptr);
        texture->image = 0;
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, &texture->memory);
    if (result != VK_SUCCESS || texture->memory == VK_NULL_DEVICE_MEMORY)
    {
        iError(reporter, "Vulkan renderer failed to allocate texture memory");
        state->vkDestroyImage(state->device, texture->image, nullptr);
        texture->image = 0;
        return false;
    }

    result = state->vkBindImageMemory(state->device, texture->image, texture->memory, 0);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to bind texture memory");
        state->vkFreeMemory(state->device, texture->memory, nullptr);
        state->vkDestroyImage(state->device, texture->image, nullptr);
        texture->memory = VK_NULL_DEVICE_MEMORY;
        texture->image = 0;
        return false;
    }

    texture->vkFormat = vkFormat;
    texture->imageUsage = usage;

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = iTextureAspectMask(texture->info.mFormat);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    result = state->vkCreateImageView(state->device, &viewInfo, nullptr, &texture->imageView);
    if (result != VK_SUCCESS || texture->imageView == VK_NULL_IMAGE_VIEW)
    {
        iError(reporter, "Vulkan renderer failed to create texture image view");
        state->vkFreeMemory(state->device, texture->memory, nullptr);
        state->vkDestroyImage(state->device, texture->image, nullptr);
        texture->memory = VK_NULL_DEVICE_MEMORY;
        texture->image = 0;
        texture->vkFormat = 0;
        texture->imageUsage = 0;
        return false;
    }

    if (!state->textureImageReported)
    {
        iReport(reporter, "Vulkan renderer created VkImage-backed texture view");
        state->textureImageReported = true;
    }
    return true;
}

static bool iCreateRenderTargetObjects(piVulkanState *state, piRTarget target, piRenderer::piReporter *reporter)
{
    if (!state || !target || state->device == VK_NULL_DEVICE)
    {
        return true;
    }
    if (!state->vkCreateRenderPass || !state->vkCreateFramebuffer)
    {
        return false;
    }

    VkAttachmentDescription attachments[5] = {};
    VkAttachmentReference colorReferences[4] = {};
    VkImageView attachmentViews[5] = {};
    uint32_t attachmentCount = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    for (uint32_t i = 0; i < 4; ++i)
    {
        piTexture color = target->color[i];
        if (!color)
        {
            break;
        }
        if (color->imageView == VK_NULL_IMAGE_VIEW || color->info.mXres <= 0 || color->info.mYres <= 0)
        {
            iError(reporter, "Vulkan render target color texture is missing an image view");
            return false;
        }
        if (width == 0)
        {
            width = (uint32_t)color->info.mXres;
            height = (uint32_t)color->info.mYres;
            sampleCount = iTextureSampleCount(color->info);
        }
        else if (width != (uint32_t)color->info.mXres || height != (uint32_t)color->info.mYres)
        {
            iError(reporter, "Vulkan render target color attachment sizes do not match");
            return false;
        }
        if (sampleCount != iTextureSampleCount(color->info))
        {
            iError(reporter, "Vulkan render target color attachment sample counts do not match");
            return false;
        }

        VkAttachmentDescription &attachment = attachments[attachmentCount];
        attachment.format = color->vkFormat;
        attachment.samples = sampleCount;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        colorReferences[target->colorAttachmentCount].attachment = attachmentCount;
        colorReferences[target->colorAttachmentCount].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentViews[attachmentCount] = color->imageView;
        ++attachmentCount;
        ++target->colorAttachmentCount;
    }

    VkAttachmentReference depthReference = {};
    if (target->depth)
    {
        piTexture depth = target->depth;
        if (depth->imageView == VK_NULL_IMAGE_VIEW || depth->info.mXres <= 0 || depth->info.mYres <= 0)
        {
            iError(reporter, "Vulkan render target depth texture is missing an image view");
            return false;
        }
        if (width == 0)
        {
            width = (uint32_t)depth->info.mXres;
            height = (uint32_t)depth->info.mYres;
            sampleCount = iTextureSampleCount(depth->info);
        }
        else if (width != (uint32_t)depth->info.mXres || height != (uint32_t)depth->info.mYres)
        {
            iError(reporter, "Vulkan render target depth attachment size does not match");
            return false;
        }
        if (sampleCount != iTextureSampleCount(depth->info))
        {
            iError(reporter, "Vulkan render target depth attachment sample count does not match");
            return false;
        }

        VkAttachmentDescription &attachment = attachments[attachmentCount];
        attachment.format = depth->vkFormat;
        attachment.samples = sampleCount;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthReference.attachment = attachmentCount;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentViews[attachmentCount] = depth->imageView;
        ++attachmentCount;
        target->hasDepth = true;
    }

    if (attachmentCount == 0 || width == 0 || height == 0)
    {
        iError(reporter, "Vulkan render target has no usable attachments");
        return false;
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = target->colorAttachmentCount;
    subpass.pColorAttachments = target->colorAttachmentCount > 0 ? colorReferences : nullptr;
    subpass.pDepthStencilAttachment = target->hasDepth ? &depthReference : nullptr;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    VkResult result = state->vkCreateRenderPass(state->device, &renderPassInfo, nullptr, &target->renderPass);
    if (result != VK_SUCCESS || target->renderPass == VK_NULL_RENDER_PASS)
    {
        iError(reporter, "Vulkan renderer failed to create render target render pass");
        return false;
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = target->renderPass;
    framebufferInfo.attachmentCount = attachmentCount;
    framebufferInfo.pAttachments = attachmentViews;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;
    result = state->vkCreateFramebuffer(state->device, &framebufferInfo, nullptr, &target->framebuffer);
    if (result != VK_SUCCESS || target->framebuffer == VK_NULL_FRAMEBUFFER)
    {
        iError(reporter, "Vulkan renderer failed to create render target framebuffer");
        state->vkDestroyRenderPass(state->device, target->renderPass, nullptr);
        target->renderPass = VK_NULL_RENDER_PASS;
        return false;
    }

    target->width = width;
    target->height = height;
    iReport(reporter, "Vulkan renderer created render target framebuffer");
    return true;
}

static bool iEnsureStagingBuffer(piVulkanState *state, VkDeviceSize size, piRenderer::piReporter *reporter)
{
    if (!state || size == 0)
    {
        return false;
    }
    if (state->stagingBuffer != VK_NULL_BUFFER && state->stagingMemory != VK_NULL_DEVICE_MEMORY && state->stagingSize >= size)
    {
        return true;
    }
    if (state->stagingBuffer != VK_NULL_BUFFER && state->vkDestroyBuffer)
    {
        state->vkDestroyBuffer(state->device, state->stagingBuffer, nullptr);
        state->stagingBuffer = VK_NULL_BUFFER;
    }
    if (state->stagingMemory != VK_NULL_DEVICE_MEMORY && state->vkFreeMemory)
    {
        state->vkFreeMemory(state->device, state->stagingMemory, nullptr);
        state->stagingMemory = VK_NULL_DEVICE_MEMORY;
    }
    state->stagingSize = 0;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = state->vkCreateBuffer(state->device, &bufferInfo, nullptr, &state->stagingBuffer);
    if (result != VK_SUCCESS || state->stagingBuffer == VK_NULL_BUFFER)
    {
        iError(reporter, "Vulkan renderer failed to create staging buffer");
        return false;
    }

    VkMemoryRequirements requirements = {};
    state->vkGetBufferMemoryRequirements(state->device, state->stagingBuffer, &requirements);
    uint32_t memoryTypeIndex = 0;
    if (!iFindMemoryType(state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memoryTypeIndex))
    {
        iError(reporter, "Vulkan renderer failed to find host-visible staging memory");
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, &state->stagingMemory);
    if (result != VK_SUCCESS || state->stagingMemory == VK_NULL_DEVICE_MEMORY)
    {
        iError(reporter, "Vulkan renderer failed to allocate staging memory");
        return false;
    }
    result = state->vkBindBufferMemory(state->device, state->stagingBuffer, state->stagingMemory, 0);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to bind staging memory");
        return false;
    }
    state->stagingSize = requirements.size;
    return true;
}

static bool iUploadTextureToStaging(piVulkanState *state, piTexture texture, piRenderer::piReporter *reporter)
{
    if (!state || !texture || !texture->data || texture->info.mXres <= 0 || texture->info.mYres <= 0)
    {
        return false;
    }
    const VkDeviceSize size = (VkDeviceSize)texture->info.mXres * (VkDeviceSize)texture->info.mYres * 4ull;
    if (!iEnsureStagingBuffer(state, size, reporter))
    {
        return false;
    }
    void *mapped = nullptr;
    VkResult result = state->vkMapMemory(state->device, state->stagingMemory, 0, size, 0, &mapped);
    if (result != VK_SUCCESS || !mapped)
    {
        iError(reporter, "Vulkan renderer failed to map staging memory");
        return false;
    }
    uint8_t *dstBytes = (uint8_t *)mapped;
    const size_t pixelCount = (size_t)texture->info.mXres * (size_t)texture->info.mYres;
    const bool swapRB = state->swapchainFormat == VK_FORMAT_B8G8R8A8_UNORM;
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const uint8_t *src = texture->data + i * 4u;
        uint8_t *dst = dstBytes + i * 4u;
        if (swapRB)
        {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 255;
        }
        else
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
        }
    }
    state->vkUnmapMemory(state->device, state->stagingMemory);
    return true;
}

static bool iCreateOwnedVulkanDevice(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!iLoadVulkanEntryPoints(state, reporter))
    {
        return false;
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "IMM Vulkan Renderer";
    appInfo.applicationVersion = IMM_VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "IMM";
    appInfo.engineVersion = IMM_VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = IMM_VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
#if defined(WINDOWS)
    const char *instanceExtensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    if (state->window)
    {
        instanceInfo.enabledExtensionCount = 2;
        instanceInfo.ppEnabledExtensionNames = instanceExtensions;
    }
#endif
    VkResult result = state->vkCreateInstance(&instanceInfo, nullptr, &state->instance);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to create VkInstance");
        return false;
    }
    state->ownsInstance = true;
    if (!iLoadVulkanInstanceEntryPoints(state, reporter))
    {
        return false;
    }
    if (!iCreateVulkanSurface(state, reporter))
    {
        return false;
    }

    uint32_t physicalDeviceCount = 0;
    result = state->vkEnumeratePhysicalDevices(state->instance, &physicalDeviceCount, nullptr);
    if (result != VK_SUCCESS || physicalDeviceCount == 0)
    {
        iError(reporter, "Vulkan renderer found no physical devices");
        return false;
    }

    VkPhysicalDevice physicalDevices[8] = {};
    if (physicalDeviceCount > 8)
    {
        physicalDeviceCount = 8;
    }
    result = state->vkEnumeratePhysicalDevices(state->instance, &physicalDeviceCount, physicalDevices);
    if (result != VK_SUCCESS || physicalDeviceCount == 0)
    {
        iError(reporter, "Vulkan renderer failed to enumerate physical devices");
        return false;
    }
    state->physicalDevice = physicalDevices[0];

    uint32_t queueFamilyCount = 0;
    state->vkGetPhysicalDeviceQueueFamilyProperties(state->physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
    {
        iError(reporter, "Vulkan renderer found no queue families");
        return false;
    }

    VkQueueFamilyProperties queueFamilies[32] = {};
    if (queueFamilyCount > 32)
    {
        queueFamilyCount = 32;
    }
    state->vkGetPhysicalDeviceQueueFamilyProperties(state->physicalDevice, &queueFamilyCount, queueFamilies);
    bool foundGraphicsQueue = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
        {
            continue;
        }
        VkBool32 presentSupported = 1;
        if (state->surface != VK_NULL_SURFACE_KHR)
        {
            if (!state->vkGetPhysicalDeviceSurfaceSupportKHR)
            {
                iError(reporter, "Vulkan renderer could not load vkGetPhysicalDeviceSurfaceSupportKHR");
                return false;
            }
            result = state->vkGetPhysicalDeviceSurfaceSupportKHR(state->physicalDevice, i, state->surface, &presentSupported);
            if (result != VK_SUCCESS)
            {
                presentSupported = 0;
            }
        }
        if (presentSupported)
        {
            state->graphicsQueueFamilyIndex = i;
            foundGraphicsQueue = true;
            break;
        }
    }
    if (!foundGraphicsQueue)
    {
        iError(reporter, "Vulkan renderer found no graphics queue family");
        return false;
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = state->graphicsQueueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    const char *deviceExtensions[] = { "VK_KHR_swapchain" };
    if (state->surface != VK_NULL_SURFACE_KHR)
    {
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = deviceExtensions;
    }
    result = state->vkCreateDevice(state->physicalDevice, &deviceInfo, nullptr, &state->device);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to create VkDevice");
        return false;
    }
    state->ownsDevice = true;
    state->vkGetDeviceQueue(state->device, state->graphicsQueueFamilyIndex, 0, &state->graphicsQueue);
    if (state->graphicsQueue == VK_NULL_QUEUE)
    {
        return false;
    }
    if (!iCreateVulkanSwapchain(state, reporter))
    {
        return false;
    }
    return iCreateVulkanFrameResources(state, reporter);
}

piRendererVulkan::piRendererVulkan()
    : mState(new piVulkanState()), mReporter(nullptr)
{
}

piRendererVulkan::~piRendererVulkan()
{
    Deinitialize();
    delete mState;
}

bool piRendererVulkan::Initialize(int id, const void **hwnd, int num, bool disableVSync, bool disableErrors, piReporter *reporter, bool createDevice, void *device)
{
    (void)hwnd;
    (void)num;
    (void)disableVSync;
    (void)disableErrors;
    (void)createDevice;
    Deinitialize();
    mState = new piVulkanState();
    mState->activeWindow = id;
    mReporter = reporter;
#if defined(WINDOWS)
    if (hwnd && hwnd[0])
    {
        mState->window = (HWND)hwnd[0];
        RECT rect = {};
        if (GetClientRect(mState->window, &rect))
        {
            mState->windowWidth = rect.right - rect.left;
            mState->windowHeight = rect.bottom - rect.top;
        }
    }
#endif

    const piVulkanExternalDevice *externalDevice = static_cast<const piVulkanExternalDevice *>(device);
    if (externalDevice && externalDevice->instance && externalDevice->physicalDevice && externalDevice->device && externalDevice->graphicsQueue)
    {
        mState->instance = static_cast<VkInstance>(externalDevice->instance);
        mState->physicalDevice = static_cast<VkPhysicalDevice>(externalDevice->physicalDevice);
        mState->device = static_cast<VkDevice>(externalDevice->device);
        mState->graphicsQueue = static_cast<VkQueue>(externalDevice->graphicsQueue);
        mState->graphicsQueueFamilyIndex = externalDevice->graphicsQueueFamilyIndex;
        mState->initialized = true;
        iReport(mReporter, "Vulkan renderer initialized with external device");
        return true;
    }

    if (!iCreateOwnedVulkanDevice(mState, mReporter))
    {
        Deinitialize();
        return false;
    }

    mState->initialized = true;
    iReport(mReporter, "Vulkan renderer initialized with owned device");
    return true;
}

void piRendererVulkan::Deinitialize(void)
{
    if (!mState)
    {
        return;
    }
    if (mState->ownsDevice && mState->device != VK_NULL_DEVICE)
    {
        if (mState->vkDeviceWaitIdle)
        {
            mState->vkDeviceWaitIdle(mState->device);
        }
        if (mState->imageAvailableSemaphore != VK_NULL_SEMAPHORE && mState->vkDestroySemaphore)
        {
            mState->vkDestroySemaphore(mState->device, mState->imageAvailableSemaphore, nullptr);
            mState->imageAvailableSemaphore = VK_NULL_SEMAPHORE;
        }
        if (mState->renderFinishedSemaphore != VK_NULL_SEMAPHORE && mState->vkDestroySemaphore)
        {
            mState->vkDestroySemaphore(mState->device, mState->renderFinishedSemaphore, nullptr);
            mState->renderFinishedSemaphore = VK_NULL_SEMAPHORE;
        }
        if (mState->frameFence != VK_NULL_FENCE && mState->vkDestroyFence)
        {
            mState->vkDestroyFence(mState->device, mState->frameFence, nullptr);
            mState->frameFence = VK_NULL_FENCE;
        }
        if (mState->commandPool != VK_NULL_COMMAND_POOL && mState->vkDestroyCommandPool)
        {
            mState->vkDestroyCommandPool(mState->device, mState->commandPool, nullptr);
            mState->commandPool = VK_NULL_COMMAND_POOL;
            mState->commandBuffer = VK_NULL_COMMAND_BUFFER;
        }
        if (mState->stagingBuffer != VK_NULL_BUFFER && mState->vkDestroyBuffer)
        {
            mState->vkDestroyBuffer(mState->device, mState->stagingBuffer, nullptr);
            mState->stagingBuffer = VK_NULL_BUFFER;
        }
        if (mState->stagingMemory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
        {
            mState->vkFreeMemory(mState->device, mState->stagingMemory, nullptr);
            mState->stagingMemory = VK_NULL_DEVICE_MEMORY;
            mState->stagingSize = 0;
        }
        for (uint32_t i = 0; i < mState->swapchainImageCount && i < 8; ++i)
        {
            if (mState->swapchainFramebuffers[i] != VK_NULL_FRAMEBUFFER && mState->vkDestroyFramebuffer)
            {
                mState->vkDestroyFramebuffer(mState->device, mState->swapchainFramebuffers[i], nullptr);
                mState->swapchainFramebuffers[i] = VK_NULL_FRAMEBUFFER;
            }
        }
        if (mState->swapchainRenderPass != VK_NULL_RENDER_PASS && mState->vkDestroyRenderPass)
        {
            mState->vkDestroyRenderPass(mState->device, mState->swapchainRenderPass, nullptr);
            mState->swapchainRenderPass = VK_NULL_RENDER_PASS;
        }
        for (uint32_t i = 0; i < mState->swapchainImageCount && i < 8; ++i)
        {
            if (mState->swapchainImageViews[i] != VK_NULL_IMAGE_VIEW && mState->vkDestroyImageView)
            {
                mState->vkDestroyImageView(mState->device, mState->swapchainImageViews[i], nullptr);
                mState->swapchainImageViews[i] = VK_NULL_IMAGE_VIEW;
            }
        }
        if (mState->swapchain != VK_NULL_SWAPCHAIN_KHR && mState->vkDestroySwapchainKHR)
        {
            mState->vkDestroySwapchainKHR(mState->device, mState->swapchain, nullptr);
            mState->swapchain = VK_NULL_SWAPCHAIN_KHR;
        }
        if (mState->vkDestroyDevice)
        {
            mState->vkDestroyDevice(mState->device, nullptr);
        }
    }
    if (mState->surface != VK_NULL_SURFACE_KHR && mState->vkDestroySurfaceKHR && mState->instance != VK_NULL_INSTANCE)
    {
        mState->vkDestroySurfaceKHR(mState->instance, mState->surface, nullptr);
        mState->surface = VK_NULL_SURFACE_KHR;
    }
    if (mState->ownsInstance && mState->instance != VK_NULL_INSTANCE)
    {
        if (mState->vkDestroyInstance)
        {
            mState->vkDestroyInstance(mState->instance, nullptr);
        }
    }
#if defined(WINDOWS)
    if (mState->vulkanLibrary)
    {
        FreeLibrary(mState->vulkanLibrary);
    }
#elif defined(ANDROID)
    if (mState->vulkanLibrary)
    {
        dlclose(mState->vulkanLibrary);
    }
#endif
    delete mState;
    mState = nullptr;
}

bool piRendererVulkan::SupportsFeature(RendererFeature feature)
{
    (void)feature;
    return false;
}

piRenderer::API piRendererVulkan::GetAPI(void) { return API::Vulkan; }

void piRendererVulkan::Report(void)
{
    if (!mState || !mReporter)
    {
        return;
    }
    mReporter->Begin(mState->liveBuffers, mState->liveBuffers, (int)mState->liveTextures, (int)mState->liveTextures);
    mReporter->End();
}

void piRendererVulkan::SetActiveWindow(int id) { if (mState) mState->activeWindow = id; }
void piRendererVulkan::Enable(void) {}
void piRendererVulkan::Disable(void) {}
void piRendererVulkan::SwapBuffers(void)
{
    if (!mState || mState->swapchain == VK_NULL_SWAPCHAIN_KHR || mState->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        mState->imageAvailableSemaphore == VK_NULL_SEMAPHORE || mState->renderFinishedSemaphore == VK_NULL_SEMAPHORE ||
        mState->frameFence == VK_NULL_FENCE)
    {
        return;
    }

    const uint64_t timeout = 1000000000ull;
    VkResult result = mState->vkWaitForFences(mState->device, 1, &mState->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return;
    }
    mState->vkResetFences(mState->device, 1, &mState->frameFence);

    uint32_t imageIndex = 0;
    result = mState->vkAcquireNextImageKHR(mState->device, mState->swapchain, timeout, mState->imageAvailableSemaphore, VK_NULL_FENCE, &imageIndex);
    if (result != VK_SUCCESS)
    {
        return;
    }
    if (imageIndex >= mState->swapchainImageCount)
    {
        return;
    }
    piTexture presentTexture = mState->pendingPresentTexture;
    bool presentTextureHasColor = false;
    if (presentTexture && presentTexture->data)
    {
        const size_t pixelCount = (size_t)presentTexture->info.mXres * (size_t)presentTexture->info.mYres;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const uint8_t *src = presentTexture->data + i * 4u;
            if (src[0] != 0 || src[1] != 0 || src[2] != 0)
            {
                presentTextureHasColor = true;
                break;
            }
        }
    }
    const bool copyTexture =
        presentTexture && presentTexture->data &&
        presentTexture->info.mXres == (int)mState->swapchainExtent.width &&
        presentTexture->info.mYres == (int)mState->swapchainExtent.height &&
        iUploadTextureToStaging(mState, presentTexture, mReporter);

    mState->vkResetCommandBuffer(mState->commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = mState->vkBeginCommandBuffer(mState->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return;
    }

    if (copyTexture)
    {
        VkImageSubresourceRange colorRange = {};
        colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorRange.baseMipLevel = 0;
        colorRange.levelCount = 1;
        colorRange.baseArrayLayer = 0;
        colorRange.layerCount = 1;

        VkImageMemoryBarrier toTransfer = {};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = mState->swapchainImages[imageIndex];
        toTransfer.subresourceRange = colorRange;
        mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &toTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset.x = 0;
        copyRegion.imageOffset.y = 0;
        copyRegion.imageOffset.z = 0;
        copyRegion.imageExtent.width = mState->swapchainExtent.width;
        copyRegion.imageExtent.height = mState->swapchainExtent.height;
        copyRegion.imageExtent.depth = 1;
        mState->vkCmdCopyBufferToImage(mState->commandBuffer,
                                       mState->stagingBuffer,
                                       mState->swapchainImages[imageIndex],
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       1,
                                       &copyRegion);

        VkImageMemoryBarrier toPresent = {};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = mState->swapchainImages[imageIndex];
        toPresent.subresourceRange = colorRange;
        mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &toPresent);
    }
    else if (mState->swapchainRenderPass != VK_NULL_RENDER_PASS && mState->swapchainFramebuffers[imageIndex] != VK_NULL_FRAMEBUFFER)
    {
        const float phase = (float)((mState->presentFrameIndex % 120u) / 119.0f);
        VkClearValue clearValue = {};
        clearValue.color.float32[0] = 0.02f;
        clearValue.color.float32[1] = 0.12f + 0.25f * phase;
        clearValue.color.float32[2] = 0.08f;
        clearValue.color.float32[3] = 1.0f;
        VkRenderPassBeginInfo renderPassBegin = {};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = mState->swapchainRenderPass;
        renderPassBegin.framebuffer = mState->swapchainFramebuffers[imageIndex];
        renderPassBegin.renderArea.offset.x = 0;
        renderPassBegin.renderArea.offset.y = 0;
        renderPassBegin.renderArea.extent = mState->swapchainExtent;
        renderPassBegin.clearValueCount = 1;
        renderPassBegin.pClearValues = &clearValue;
        mState->vkCmdBeginRenderPass(mState->commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
        mState->vkCmdEndRenderPass(mState->commandBuffer);
    }

    result = mState->vkEndCommandBuffer(mState->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &mState->imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mState->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mState->renderFinishedSemaphore;
    result = mState->vkQueueSubmit(mState->graphicsQueue, 1, &submitInfo, mState->frameFence);
    if (result != VK_SUCCESS)
    {
        return;
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mState->renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mState->swapchain;
    presentInfo.pImageIndices = &imageIndex;
    result = mState->vkQueuePresentKHR(mState->graphicsQueue, &presentInfo);
    if (result == VK_SUCCESS)
    {
        ++mState->presentFrameIndex;
        if (copyTexture && presentTextureHasColor && !mState->texturePresentReported)
        {
            mState->texturePresentReported = true;
            iReport(mReporter, "Vulkan renderer presented swapchain nonblack texture frame");
        }
        else if (!copyTexture && !mState->realPresentReported)
        {
            mState->realPresentReported = true;
            iReport(mReporter, "Vulkan renderer presented swapchain clear frame");
        }
    }
}
void *piRendererVulkan::GetContext(void) { return mState ? (void *)mState->device : nullptr; }

void piRendererVulkan::StartPerformanceMeasure(void)
{
    if (!mState)
    {
        return;
    }
    if (!mState->perfQueries[0]) mState->perfQueries[0] = CreateQuery(QueryType::TimeElapsed);
    if (!mState->perfQueries[1]) mState->perfQueries[1] = CreateQuery(QueryType::TimeElapsed);
    BeginQuery(mState->perfQueries[mState->currentPerformanceQuery]);
}

void piRendererVulkan::EndPerformanceMeasure(void)
{
    if (!mState || !mState->perfQueries[mState->currentPerformanceQuery])
    {
        return;
    }
    EndQuery(mState->perfQueries[mState->currentPerformanceQuery]);
    mState->currentPerformanceQuery = 1 - mState->currentPerformanceQuery;
}

uint64_t piRendererVulkan::GetPerformanceMeasure(void)
{
    if (!mState)
    {
        return 0;
    }
    return GetQueryResult(mState->perfQueries[1 - mState->currentPerformanceQuery]);
}

piRTarget piRendererVulkan::CreateRenderTarget(piTexture vtex0, piTexture vtex1, piTexture vtex2, piTexture vtex3, piTexture zbuf)
{
    piRTargetS *target = new piRTargetS();
    target->color[0] = vtex0;
    target->color[1] = vtex1;
    target->color[2] = vtex2;
    target->color[3] = vtex3;
    target->depth = zbuf;
    if (mState && !iCreateRenderTargetObjects(mState, target, mReporter))
    {
        delete target;
        return nullptr;
    }
    if (mState) ++mState->liveRenderTargets;
    return target;
}

void piRendererVulkan::DestroyRenderTarget(piRTarget obj)
{
    if (!obj) return;
    if (mState && mState->device != VK_NULL_DEVICE)
    {
        if (obj->framebuffer != VK_NULL_FRAMEBUFFER && mState->vkDestroyFramebuffer)
        {
            mState->vkDestroyFramebuffer(mState->device, obj->framebuffer, nullptr);
            obj->framebuffer = VK_NULL_FRAMEBUFFER;
        }
        if (obj->renderPass != VK_NULL_RENDER_PASS && mState->vkDestroyRenderPass)
        {
            mState->vkDestroyRenderPass(mState->device, obj->renderPass, nullptr);
            obj->renderPass = VK_NULL_RENDER_PASS;
        }
    }
    if (mState && mState->liveRenderTargets > 0) --mState->liveRenderTargets;
    delete obj;
}

bool piRendererVulkan::SetRenderTarget(piRTarget obj)
{
    if (mState) mState->currentRenderTarget = obj;
    return obj == nullptr || obj->framebuffer != VK_NULL_FRAMEBUFFER;
}

void piRendererVulkan::RenderTargetSampleLocations(piRTarget vdst, const float *locations) { (void)vdst; (void)locations; }
void piRendererVulkan::BlitRenderTarget(piRTarget dst, piRTarget src, bool color, bool depth) { (void)dst; (void)src; (void)color; (void)depth; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::RenderTargetOperations, "Vulkan render target blit is not implemented yet"); }
void piRendererVulkan::SetWriteMask(bool c0, bool c1, bool c2, bool c3, bool z) { (void)c0; (void)c1; (void)c2; (void)c3; (void)z; }
void piRendererVulkan::SetShadingSamples(int shadingSamples) { (void)shadingSamples; }
void piRendererVulkan::RenderTargetGetDefaultSampleLocation(piRTarget vdst, const int id, float *location) { (void)vdst; (void)id; if (location) { location[0] = 0.5f; location[1] = 0.5f; } }
void piRendererVulkan::Clear(const float *color0, const float *color1, const float *color2, const float *color3, const bool depth0)
{
    (void)color1;
    (void)color2;
    (void)color3;
    (void)depth0;
    if (!mState || !mState->currentRenderTarget || !mState->currentRenderTarget->color[0] || !mState->currentRenderTarget->color[0]->data)
    {
        return;
    }
    piTexture texture = mState->currentRenderTarget->color[0];
    const uint8_t r = color0 ? (uint8_t)(color0[0] * 255.0f) : 0;
    const uint8_t g = color0 ? (uint8_t)(color0[1] * 255.0f) : 0;
    const uint8_t b = color0 ? (uint8_t)(color0[2] * 255.0f) : 0;
    for (int y = 0; y < texture->info.mYres; ++y)
    {
        for (int x = 0; x < texture->info.mXres; ++x)
        {
            uint8_t *dst = texture->data + ((size_t)y * (size_t)texture->info.mXres + (size_t)x) * 4u;
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst[3] = 255;
        }
    }
}
void piRendererVulkan::SetState(piState state, bool value) { (void)state; (void)value; }
void piRendererVulkan::SetBlending(int buf, BlendEquation equRGB, BlendOperations srcRGB, BlendOperations dstRGB, BlendEquation equALP, BlendOperations srcALP, BlendOperations dstALP) { (void)buf; (void)equRGB; (void)srcRGB; (void)dstRGB; (void)equALP; (void)srcALP; (void)dstALP; }

void piRendererVulkan::SetViewport(int id, const int *vp)
{
    if (!mState || !vp || id < 0 || id >= 16) return;
    float viewport[6] = { (float)vp[0], (float)vp[1], (float)vp[2], (float)vp[3], 0.0f, 1.0f };
    std::memcpy(&mState->viewports[id * 6], viewport, sizeof(viewport));
    if (id >= mState->numViewports) mState->numViewports = id + 1;
}

void piRendererVulkan::SetViewports(int num, const float *viewports)
{
    if (!mState || !viewports || num <= 0) return;
    if (num > 16) num = 16;
    mState->numViewports = num;
    std::memcpy(mState->viewports, viewports, sizeof(float) * 6u * (size_t)num);
}

void piRendererVulkan::GetViewports(int *num, float *viewports)
{
    if (!mState) return;
    if (num) *num = mState->numViewports;
    if (viewports) std::memcpy(viewports, mState->viewports, sizeof(float) * 6u * (size_t)mState->numViewports);
}

piRasterState piRendererVulkan::CreateRasterState(bool wireframe, bool frontIsCounterClockWise, CullMode cullMode, bool depthClamp, bool multiSample)
{
    piRasterStateS *state = new piRasterStateS();
    state->wireframe = wireframe;
    state->frontIsCounterClockWise = frontIsCounterClockWise;
    state->cullMode = cullMode;
    state->depthClamp = depthClamp;
    state->multiSample = multiSample;
    if (mState) ++mState->liveRasterStates;
    return state;
}

void piRendererVulkan::SetRasterState(const piRasterState vme) { if (mState) mState->currentRasterState = vme; }
void piRendererVulkan::DestroyRasterState(piRasterState vme) { if (!vme) return; if (mState && mState->liveRasterStates > 0) --mState->liveRasterStates; delete vme; }
piBlendState piRendererVulkan::CreateBlendState(bool alphaToCoverage, bool enabled0) { piBlendStateS *state = new piBlendStateS(); state->alphaToCoverage = alphaToCoverage; state->enabled0 = enabled0; if (mState) ++mState->liveBlendStates; return state; }
void piRendererVulkan::SetBlendState(const piBlendState vme) { if (mState) mState->currentBlendState = vme; }
void piRendererVulkan::DestroyBlendState(piBlendState vme) { if (!vme) return; if (mState && mState->liveBlendStates > 0) --mState->liveBlendStates; delete vme; }
piDepthState piRendererVulkan::CreateDepthState(bool alphaToCoverage, bool lessEqual) { piDepthStateS *state = new piDepthStateS(); state->alphaToCoverage = alphaToCoverage; state->lessEqual = lessEqual; if (mState) ++mState->liveDepthStates; return state; }
void piRendererVulkan::SetDepthState(const piDepthState vme) { if (mState) mState->currentDepthState = vme; }
void piRendererVulkan::DestroyDepthState(piDepthState vme) { if (!vme) return; if (mState && mState->liveDepthStates > 0) --mState->liveDepthStates; delete vme; }

piTexture piRendererVulkan::CreateTexture(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap, float aniso, const void *buffer)
{
    return CreateTexture2(key, info, compress, filter, wrap, aniso, buffer, 0);
}

piTexture piRendererVulkan::CreateTexture2(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap1, float aniso, const void *buffer, int bindUsage)
{
    (void)key; (void)compress; (void)aniso;
    if (!info) return nullptr;
    piTextureS *texture = new piTextureS();
    texture->info = *info;
    texture->filter = filter;
    texture->wrap = wrap1;
    texture->dataSize = iTextureDataSize(info);
    if (texture->dataSize > 0)
    {
        texture->data = (uint8_t *)std::malloc(texture->dataSize);
        if (texture->data)
        {
            if (buffer) std::memcpy(texture->data, buffer, texture->dataSize);
            else std::memset(texture->data, 0, texture->dataSize);
        }
    }
    if (mState && !iCreateTextureImage(mState, texture, bindUsage, mReporter))
    {
        std::free(texture->data);
        delete texture;
        return nullptr;
    }
    if (mState) ++mState->liveTextures;
    return texture;
}

void piRendererVulkan::DestroyTexture(piTexture obj)
{
    if (!obj) return;
    if (mState && mState->device != VK_NULL_DEVICE)
    {
        if (obj->imageView != VK_NULL_IMAGE_VIEW && mState->vkDestroyImageView)
        {
            mState->vkDestroyImageView(mState->device, obj->imageView, nullptr);
            obj->imageView = VK_NULL_IMAGE_VIEW;
        }
        if (obj->image != 0 && mState->vkDestroyImage)
        {
            mState->vkDestroyImage(mState->device, obj->image, nullptr);
            obj->image = 0;
        }
        if (obj->memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
        {
            mState->vkFreeMemory(mState->device, obj->memory, nullptr);
            obj->memory = VK_NULL_DEVICE_MEMORY;
        }
    }
    std::free(obj->data);
    if (mState && mState->liveTextures > 0) --mState->liveTextures;
    delete obj;
}
void piRendererVulkan::ClearTexture(piTexture vme, int level, const void *data) { (void)level; if (vme && vme->data && data) std::memcpy(vme->data, data, vme->dataSize); }
void piRendererVulkan::UpdateTexture(piTexture me, int x0, int y0, int z0, int xres, int yres, int zres, const void *buffer) { (void)x0; (void)y0; (void)z0; (void)xres; (void)yres; (void)zres; if (me && me->data && buffer) std::memcpy(me->data, buffer, me->dataSize); }
void piRendererVulkan::GetTextureRes(piTexture me, int *res) { if (me && res) { res[0] = me->info.mXres; res[1] = me->info.mYres; res[2] = me->info.mZres; } }
void piRendererVulkan::GetTextureFormat(piTexture me, Format *format) { if (me && format) *format = me->info.mFormat; }
void piRendererVulkan::GetTextureContent(piTexture me, void *data, const Format fmt) { (void)fmt; if (me && data && me->data) std::memcpy(data, me->data, me->dataSize); else iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::TextureReadback, "Vulkan texture GPU readback is not implemented yet"); }
void piRendererVulkan::GetTextureContent(piTexture vme, void *data, int x, int y, int z, int xres, int yres, int zres) { (void)x; (void)y; (void)z; (void)xres; (void)yres; (void)zres; GetTextureContent(vme, data, vme ? vme->info.mFormat : Format::UNKOWN); }
void piRendererVulkan::GetTextureInfo(piTexture me, TextureInfo *info) { if (me && info) *info = me->info; }
void piRendererVulkan::GetTextureSampling(piTexture vme, TextureFilter *rfilter, TextureWrap *rwrap) { if (vme && rfilter) *rfilter = vme->filter; if (vme && rwrap) *rwrap = vme->wrap; }
void piRendererVulkan::ComputeMipmaps(piTexture me) { (void)me; }
void piRendererVulkan::AttachTextures(int num, piTexture vt0, piTexture vt1, piTexture vt2, piTexture vt3, piTexture vt4, piTexture vt5, piTexture vt6, piTexture vt7, piTexture vt8, piTexture vt9, piTexture vt10, piTexture vt11, piTexture vt12, piTexture vt13, piTexture vt14, piTexture vt15) { piTexture textures[16] = { vt0, vt1, vt2, vt3, vt4, vt5, vt6, vt7, vt8, vt9, vt10, vt11, vt12, vt13, vt14, vt15 }; AttachTextures(num, textures, 0); }
void piRendererVulkan::AttachTextures(int num, piTexture *vt, int offset) { if (!mState || !vt || offset < 0) return; for (int i = 0; i < num && (i + offset) < 16; ++i) mState->textures[i + offset] = vt[i]; }
void piRendererVulkan::DettachTextures(void) { if (mState) std::memset(mState->textures, 0, sizeof(mState->textures)); }
piTexture piRendererVulkan::CreateTextureFromID(unsigned int id, TextureFilter filter) { piTextureS *texture = new piTextureS(); texture->externalHandle = id; texture->filter = filter; if (mState) ++mState->liveTextures; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::ExternalTexture, "Vulkan external texture wrapping is not implemented yet"); return texture; }
void piRendererVulkan::MakeResident(piTexture vme) { (void)vme; }
void piRendererVulkan::MakeNonResident(piTexture vme) { (void)vme; }
uint64_t piRendererVulkan::GetTextureHandle(piTexture vme) { return vme ? vme->externalHandle : 0; }

piSampler piRendererVulkan::CreateSampler(TextureFilter filter, TextureWrap wrap, float anisotropy) { piSamplerS *sampler = new piSamplerS(); sampler->filter = filter; sampler->wrap = wrap; sampler->anisotropy = anisotropy; if (mState) ++mState->liveSamplers; return sampler; }
void piRendererVulkan::DestroySampler(piSampler obj) { if (!obj) return; if (mState && mState->liveSamplers > 0) --mState->liveSamplers; delete obj; }
void piRendererVulkan::AttachSamplers(int num, piSampler vt0, piSampler vt1, piSampler vt2, piSampler vt3, piSampler vt4, piSampler vt5, piSampler vt6, piSampler vt7) { if (!mState) return; piSampler samplers[8] = { vt0, vt1, vt2, vt3, vt4, vt5, vt6, vt7 }; for (int i = 0; i < num && i < 8; ++i) mState->samplers[i] = samplers[i]; }
void piRendererVulkan::DettachSamplers(void) { if (mState) std::memset(mState->samplers, 0, sizeof(mState->samplers)); }
void piRendererVulkan::AttachImage(int unit, piTexture texture, int level, bool layered, int layer, Format format) { (void)unit; (void)texture; (void)level; (void)layered; (void)layer; (void)format; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::ImageLoadStore, "Vulkan image load/store bindings are not implemented yet"); }

piShader piRendererVulkan::CreateShader(const piShaderOptions *options, const char *vs, const char *cs, const char *es, const char *gs, const char *fs, char *error)
{
    (void)vs;
    (void)cs;
    (void)es;
    (void)gs;
    (void)fs;
    piShaderS *shader = new piShaderS();
    if (options)
    {
        shader->options = *options;
        shader->hasOptions = true;
    }
    if (error) error[0] = 0;
    if (mState) ++mState->liveShaders;
    return shader;
}
piShader piRendererVulkan::CreateShaderBinary(const piShaderOptions *options, const uint8_t *vs, const int vs_len, const uint8_t *cs, const int cs_len, const uint8_t *es, const int es_len, const uint8_t *gs, const int gs_len, const uint8_t *fs, const int fs_len, char *error) { (void)options; (void)cs; (void)cs_len; (void)es; (void)es_len; (void)gs; (void)gs_len; piShaderS *shader = new piShaderS(); shader->vs = vs; shader->vsLen = vs_len; shader->fs = fs; shader->fsLen = fs_len; if (error) error[0] = 0; if (mState) ++mState->liveShaders; return shader; }
void piRendererVulkan::DestroyShader(piShader obj) { if (!obj) return; if (mState && mState->liveShaders > 0) --mState->liveShaders; delete obj; }
void piRendererVulkan::AttachShader(piShader obj) { if (mState) mState->currentShader = obj; }
void piRendererVulkan::DettachShader(void) { if (mState) mState->currentShader = nullptr; }
piShader piRendererVulkan::CreateCompute(const piShaderOptions *options, const char *cs, char *error) { (void)options; (void)cs; const char *message = "Vulkan compute is not implemented yet"; if (error) std::strcpy(error, message); iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::Compute, message); return nullptr; }

void piRendererVulkan::SetShaderConstant4F(const unsigned int pos, const float *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant3F(const unsigned int pos, const float *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant2F(const unsigned int pos, const float *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant1F(const unsigned int pos, const float *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant1I(const unsigned int pos, const int *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant1UI(const unsigned int pos, const unsigned int *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant2UI(const unsigned int pos, const unsigned int *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant3UI(const unsigned int pos, const unsigned int *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstant4UI(const unsigned int pos, const unsigned int *value, int num) { (void)pos; (void)value; (void)num; }
void piRendererVulkan::SetShaderConstantMat4F(const unsigned int pos, const float *value, int num, bool transpose) { (void)pos; (void)value; (void)num; (void)transpose; }
void piRendererVulkan::SetShaderConstantSampler(const unsigned int pos, int unit) { (void)pos; (void)unit; }
void piRendererVulkan::AttachShaderConstants(piBuffer obj, int unit) { if (mState && unit >= 0 && unit < 16) mState->constantBuffers[unit] = obj; }
void piRendererVulkan::AttachShaderBuffer(piBuffer obj, int unit) { if (mState && unit >= 0 && unit < 16) mState->shaderBuffers[unit] = obj; }
void piRendererVulkan::DettachShaderBuffer(int unit) { if (mState && unit >= 0 && unit < 16) mState->shaderBuffers[unit] = nullptr; }
void piRendererVulkan::AttachAtomicsBuffer(piBuffer obj, int unit) { (void)obj; (void)unit; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::Atomics, "Vulkan atomic buffers are not implemented yet"); }
void piRendererVulkan::DettachAtomicsBuffer(int unit) { (void)unit; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::Atomics, "Vulkan atomic buffers are not implemented yet"); }

piBuffer piRendererVulkan::CreateBuffer(const void *data, unsigned int amount, BufferType mode, BufferUse use)
{
    if (amount == 0) return nullptr;
    piBufferS *buffer = new piBufferS();
    buffer->size = amount;
    buffer->type = mode;
    buffer->use = use;
    buffer->data = (uint8_t *)std::malloc(amount);
    if (!buffer->data)
    {
        delete buffer;
        return nullptr;
    }
    if (data) std::memcpy(buffer->data, data, amount);
    else std::memset(buffer->data, 0, amount);
    if (mState && !iCreateBufferObject(mState, buffer, data ? data : buffer->data, mReporter))
    {
        std::free(buffer->data);
        delete buffer;
        return nullptr;
    }
    if (mState) ++mState->liveBuffers;
    return buffer;
}
piBuffer piRendererVulkan::CreateStructuredBuffer(const void *data, unsigned int numElements, unsigned int elementSize, BufferType mode, BufferUse use) { return CreateBuffer(data, numElements * elementSize, mode, use); }
piBuffer piRendererVulkan::CreateBufferMapped_Start(void **ptr, unsigned int amount, BufferUse use) { piBuffer buffer = CreateBuffer(nullptr, amount, BufferType::Dynamic, use); if (ptr) *ptr = buffer ? buffer->data : nullptr; return buffer; }
void piRendererVulkan::CreateBufferMapped_End(piBuffer vme)
{
    if (!vme || !mState) return;
    iUploadBufferData(mState, vme, vme->data, 0, vme->size, mReporter);
}
void piRendererVulkan::DestroyBuffer(piBuffer obj)
{
    if (!obj) return;
    if (mState && mState->device != VK_NULL_DEVICE)
    {
        if (obj->buffer != VK_NULL_BUFFER && mState->vkDestroyBuffer)
        {
            mState->vkDestroyBuffer(mState->device, obj->buffer, nullptr);
            obj->buffer = VK_NULL_BUFFER;
        }
        if (obj->memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
        {
            mState->vkFreeMemory(mState->device, obj->memory, nullptr);
            obj->memory = VK_NULL_DEVICE_MEMORY;
        }
    }
    std::free(obj->data);
    if (mState && mState->liveBuffers > 0) --mState->liveBuffers;
    delete obj;
}
void piRendererVulkan::UpdateBuffer(piBuffer obj, const void *data, int offset, int len, bool invalidate)
{
    (void)invalidate;
    if (!obj || !data || offset < 0 || len < 0 || (unsigned int)(offset + len) > obj->size) return;
    std::memcpy(obj->data + offset, data, (size_t)len);
    if (mState)
    {
        iUploadBufferData(mState, obj, data, (unsigned int)offset, (unsigned int)len, mReporter);
    }
}
void piRendererVulkan::AttachPixelPackBuffer(piBuffer obj) { (void)obj; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::PixelPackBuffer, "Vulkan pixel pack buffers are not implemented yet"); }
void piRendererVulkan::DettachPixelPackBuffer(void) { iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::PixelPackBuffer, "Vulkan pixel pack buffers are not implemented yet"); }

piVertexArray piRendererVulkan::CreateVertexArray(int numStreams, piBuffer vb0, const piRArrayLayout *streamLayout0, piBuffer vb1, const piRArrayLayout *streamLayout1, piBuffer eb, const IndexArrayFormat ebFormat) { (void)numStreams; (void)streamLayout0; (void)streamLayout1; piVertexArrayS *vertexArray = new piVertexArrayS(); vertexArray->vertexBuffer[0] = vb0; vertexArray->vertexBuffer[1] = vb1; vertexArray->indexBuffer = eb; vertexArray->indexFormat = ebFormat; if (mState) ++mState->liveVertexArrays; return vertexArray; }
void piRendererVulkan::DestroyVertexArray(piVertexArray obj) { if (!obj) return; if (mState && mState->liveVertexArrays > 0) --mState->liveVertexArrays; delete obj; }
void piRendererVulkan::AttachVertexArray(piVertexArray obj) { if (mState) mState->currentVertexArray = obj; }
void piRendererVulkan::DettachVertexArray(void) { if (mState) mState->currentVertexArray = nullptr; }
piVertexArray piRendererVulkan::CreateVertexArray2(int numStreams, piBuffer vb0, const ArrayLayout2 *streamLayout0, piBuffer vb1, const ArrayLayout2 *streamLayout1, const void *shaderBinary, size_t shaderBinarySize, piBuffer ib, const IndexArrayFormat ebFormat) { (void)shaderBinary; (void)shaderBinarySize; (void)streamLayout0; (void)streamLayout1; return CreateVertexArray(numStreams, vb0, nullptr, vb1, nullptr, ib, ebFormat); }
void piRendererVulkan::AttachVertexArray2(piVertexArray vme) { AttachVertexArray(vme); }
void piRendererVulkan::DestroyVertexArray2(piVertexArray vme) { DestroyVertexArray(vme); }

piQuery piRendererVulkan::CreateQuery(piRenderer::QueryType type) { piQueryS *query = new piQueryS(); query->type = type; if (mState) ++mState->liveQueries; return query; }
void piRendererVulkan::DestroyQuery(piQuery vme) { if (!vme) return; if (mState && mState->liveQueries > 0) --mState->liveQueries; delete vme; }
void piRendererVulkan::BeginQuery(piQuery vme) { if (!vme) return; vme->startNanoseconds = iNowNanoseconds(); vme->active = true; }
void piRendererVulkan::EndQuery(piQuery vme) { if (!vme || !vme->active) return; const uint64_t now = iNowNanoseconds(); vme->resultNanoseconds = now >= vme->startNanoseconds ? now - vme->startNanoseconds : 0; vme->active = false; }
uint64_t piRendererVulkan::GetQueryResult(piQuery vme) { return vme ? vme->resultNanoseconds : 0; }

void piRendererVulkan::DrawPrimitiveIndexed(PrimitiveType pt, uint32_t num, uint32_t numInstances, uint32_t baseVertex, uint32_t baseInstance, uint32_t baseIndex)
{
    (void)numInstances;
    (void)baseVertex;
    (void)baseInstance;
    if (!mState)
    {
        return;
    }
    if (pt != PrimitiveType::TriangleStrip)
    {
        return;
    }
    if (!mState->currentRenderTarget || !mState->currentRenderTarget->color[0] ||
        !mState->currentVertexArray || !mState->currentVertexArray->indexBuffer || !mState->shaderBuffers[8] ||
        !mState->constantBuffers[3] || !mState->constantBuffers[4] || !mState->constantBuffers[9] || !mState->currentShader)
    {
        if (!mState->cpuDrawDiagnosticReported)
        {
            mState->cpuDrawDiagnosticReported = true;
            char message[512];
            std::snprintf(message,
                          sizeof(message),
                          "IMM_VK_CPU: draw guard failed pt=%d target=%d color=%d va=%d ib=%d vb8=%d cb3=%d cb4=%d cb9=%d shader=%d",
                          (int)pt,
                          mState->currentRenderTarget ? 1 : 0,
                          (mState->currentRenderTarget && mState->currentRenderTarget->color[0]) ? 1 : 0,
                          mState->currentVertexArray ? 1 : 0,
                          (mState->currentVertexArray && mState->currentVertexArray->indexBuffer) ? 1 : 0,
                          mState->shaderBuffers[8] ? 1 : 0,
                          mState->constantBuffers[3] ? 1 : 0,
                          mState->constantBuffers[4] ? 1 : 0,
                          mState->constantBuffers[9] ? 1 : 0,
                          mState->currentShader ? 1 : 0);
            iReport(mReporter, message);
        }
        iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet");
        return;
    }

    piTexture target = mState->currentRenderTarget->color[0];
    const uint16_t *indices = (const uint16_t *)(mState->currentVertexArray->indexBuffer->data + baseIndex * sizeof(uint16_t));
    const iCpuStaticVertex *vertices = (const iCpuStaticVertex *)mState->shaderBuffers[8]->data;
    const iCpuLayerState *layer = (const iCpuLayerState *)mState->constantBuffers[3]->data;
    const iCpuDisplayState *display = (const iCpuDisplayState *)mState->constantBuffers[4]->data;
    const iCpuChunkData *chunk = (const iCpuChunkData *)mState->constantBuffers[9]->data;
    const int brushType = iShaderOption(mState->currentShader, "BRUSHTYPE", 0);
    const int vertexFormat = iShaderOption(mState->currentShader, "VERTEX_FORMAT", 0);

    float previous[2] = {};
    bool hasPrevious = false;
    uint32_t lastBid = 0;
    bool hasLastBid = false;
    uint32_t projected = 0;
    float minX = 1000000.0f;
    float minY = 1000000.0f;
    float maxX = -1000000.0f;
    float maxY = -1000000.0f;
    float maxA = 0.0f;
    uint32_t visibleSegments = 0;
    uint32_t insidePoints = 0;
    uint8_t maxColor[3] = {};
    for (uint32_t i = 0; i < num; ++i)
    {
        const uint32_t realVertexID = (uint32_t)indices[i] + chunk[0].vertexOffset;
        uint32_t bid = realVertexID;
        if (brushType == 1) bid = realVertexID >> 1u;
        else if (brushType == 2 || brushType == 3) bid = realVertexID / 7u;
        else if (brushType == 4) bid = realVertexID >> 2u;
        if (hasLastBid && bid == lastBid)
        {
            continue;
        }
        lastBid = bid;
        hasLastBid = true;

        float p[3] = {};
        uint8_t color[3] = {};
        uint8_t alpha = 255;
        uint32_t widthBits = 0;
        if (vertexFormat == 1)
        {
            const iCpuStaticVertexPacked *packedVertices = (const iCpuStaticVertexPacked *)mState->shaderBuffers[8]->data;
            const iCpuStaticVertexPacked &v = packedVertices[bid];
            p[0] = v.pos[0];
            p[1] = v.pos[1];
            p[2] = v.pos[2];
            color[0] = v.col[0];
            color[1] = v.col[1];
            color[2] = v.col[2];
            alpha = v.alp;
            widthBits = v.widInfo >> 8u;
        }
        else
        {
            const iCpuStaticVertex &v = vertices[bid];
            p[0] = v.pos[0];
            p[1] = v.pos[1];
            p[2] = v.pos[2];
            color[0] = v.col[0];
            color[1] = v.col[1];
            color[2] = v.col[2];
            alpha = v.alp;
            widthBits = v.wid;
        }
        float viewer[4] = {};
        float clip[4] = {};
        iMulPoint(layer->layerToViewer, p, viewer);
        iMulPoint(display->eyeToClip, viewer, clip);
        if (std::abs(clip[3]) < 1.0e-5f)
        {
            continue;
        }
        const float ndcX = clip[0] / clip[3];
        const float ndcY = clip[1] / clip[3];
        const float sx = (ndcX * 0.5f + 0.5f) * (float)target->info.mXres;
        const float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * (float)target->info.mYres;
        if (sx < minX) minX = sx;
        if (sy < minY) minY = sy;
        if (sx > maxX) maxX = sx;
        if (sy > maxY) maxY = sy;
        ++projected;
        const float r = (float)color[0] / 255.0f;
        const float g = (float)color[1] / 255.0f;
        const float b = (float)color[2] / 255.0f;
        const float a = ((float)alpha / 255.0f) * layer->opacity;
        if (a > maxA) maxA = a;
        if (color[0] > maxColor[0]) maxColor[0] = color[0];
        if (color[1] > maxColor[1]) maxColor[1] = color[1];
        if (color[2] > maxColor[2]) maxColor[2] = color[2];
        if (sx >= 0.0f && sy >= 0.0f && sx < (float)target->info.mXres && sy < (float)target->info.mYres)
        {
            ++insidePoints;
        }
        const float width = 1.0f + 6.0f * ((float)(widthBits & 0x7fffu) / 32767.0f);
        if (hasPrevious)
        {
            iDrawCpuLine(target, previous[0], previous[1], sx, sy, width, r, g, b, a);
            if (a > 0.0f)
            {
                ++visibleSegments;
            }
        }
        previous[0] = sx;
        previous[1] = sy;
        hasPrevious = true;
    }
    if (!mState->cpuPaintDiagnosticReported)
    {
        uint32_t targetNonBlack = 0;
        const size_t pixelCount = (size_t)target->info.mXres * (size_t)target->info.mYres;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const uint8_t *src = target->data + i * 4u;
            if (src[0] != 0 || src[1] != 0 || src[2] != 0)
            {
                ++targetNonBlack;
            }
        }
        mState->cpuPaintDiagnosticReported = true;
        char message[512];
        std::snprintf(message,
                      sizeof(message),
                      "IMM_VK_CPU: paint draw num=%u projected=%u inside=%u segments=%u nonblack=%u brush=%d maxColor=%u,%u,%u maxA=%.3f screen=(%.1f,%.1f)-(%.1f,%.1f) target=%dx%d",
                      num,
                      projected,
                      insidePoints,
                      visibleSegments,
                      targetNonBlack,
                      brushType,
                      (unsigned int)maxColor[0],
                      (unsigned int)maxColor[1],
                      (unsigned int)maxColor[2],
                      maxA,
                      minX,
                      minY,
                      maxX,
                      maxY,
                      target->info.mXres,
                      target->info.mYres);
        iReport(mReporter, message);
    }
#if defined(WINDOWS)
    ++mState->cpuPaintDrawCount;
    if (mState->cpuPaintDrawCount == 1u || (mState->cpuPaintDrawCount & 31u) == 0u)
    {
        mState->pendingPresentTexture = target;
        iWritePpmCapture(mState, target);
        SwapBuffers();
    }
#endif
}
void piRendererVulkan::DrawPrimitiveIndirect(PrimitiveType pt, piBuffer cmds, uint32_t offset, uint32_t num) { (void)pt; (void)cmds; (void)offset; (void)num; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet"); }
void piRendererVulkan::DrawPrimitiveNotIndexed(PrimitiveType pt, int first, int num, int numInstances) { (void)pt; (void)first; (void)num; (void)numInstances; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet"); }
void piRendererVulkan::DrawPrimitiveNotIndexedMultiple(PrimitiveType pt, const int *firsts, const int *counts, int num) { (void)pt; (void)firsts; (void)counts; (void)num; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet"); }
void piRendererVulkan::DrawPrimitiveNotIndexedIndirect(PrimitiveType pt, piBuffer cmds, int num) { (void)pt; (void)cmds; (void)num; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet"); }
void piRendererVulkan::DettachIndirectBuffer(void) {}
void piRendererVulkan::DrawUnitCube_XYZ_NOR(int numInstanced) { (void)numInstanced; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet"); }
void piRendererVulkan::DrawUnitCube_XYZ(int numInstanced) { (void)numInstanced; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet"); }
void piRendererVulkan::DrawUnitQuad_XY(int numInstanced)
{
    (void)numInstanced;
    if (!mState || !mState->textures[0])
    {
        iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet");
        return;
    }
    if (!mState->currentRenderTarget)
    {
        mState->pendingPresentTexture = mState->textures[0];
    }

    if (!mState->cpuPresentDiagnosticReported && mState->textures[0]->data)
    {
        uint32_t textureNonBlack = 0;
        const size_t pixelCount = (size_t)mState->textures[0]->info.mXres * (size_t)mState->textures[0]->info.mYres;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const uint8_t *src = mState->textures[0]->data + i * 4u;
            if (src[0] != 0 || src[1] != 0 || src[2] != 0)
            {
                ++textureNonBlack;
            }
        }
        mState->cpuPresentDiagnosticReported = true;
        char message[256];
        std::snprintf(message,
                      sizeof(message),
                      "IMM_VK_CPU: present texture nonblack=%u size=%dx%d target=%d",
                      textureNonBlack,
                      mState->textures[0]->info.mXres,
                      mState->textures[0]->info.mYres,
                      mState->currentRenderTarget ? 1 : 0);
        iReport(mReporter, message);
    }

    if (mState->currentRenderTarget && mState->currentRenderTarget->color[0] && mState->currentRenderTarget->color[0]->data &&
        mState->textures[0]->data && mState->currentRenderTarget->color[0]->dataSize == mState->textures[0]->dataSize)
    {
        std::memcpy(mState->currentRenderTarget->color[0]->data, mState->textures[0]->data, mState->textures[0]->dataSize);
        return;
    }

#if defined(WINDOWS)
    iWritePpmCapture(mState, mState->textures[0]);
    if (!mState->currentRenderTarget)
    {
        SwapBuffers();
    }
#endif
}
void piRendererVulkan::ExecuteCompute(int ngx, int ngy, int ngz, int gsx, int gsy, int gsz) { (void)ngx; (void)ngy; (void)ngz; (void)gsx; (void)gsy; (void)gsz; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::Compute, "Vulkan compute is not implemented yet"); }
void piRendererVulkan::CreateSyncObject(piBuffer &buffer) { buffer = nullptr; }
bool piRendererVulkan::CheckSyncObject(piBuffer &buffer) { (void)buffer; return true; }
void piRendererVulkan::SetPointSize(bool mode, float size) { (void)mode; (void)size; }
void piRendererVulkan::SetLineWidth(float size) { (void)size; }
void piRendererVulkan::PolygonOffset(bool mode, bool wireframe, float a, float b) { (void)mode; (void)wireframe; (void)a; (void)b; }
void piRendererVulkan::RenderMemoryBarrier(BarrierType type) { (void)type; }

} // namespace ImmCore
