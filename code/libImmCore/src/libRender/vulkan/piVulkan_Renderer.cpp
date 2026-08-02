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
#include <vector>

#if defined(WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(ANDROID)
#include <android/native_window.h>
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
typedef uint64_t VkShaderModule;
typedef uint64_t VkSampler;
typedef uint64_t VkDescriptorSetLayout;
typedef uint64_t VkDescriptorPool;
typedef uint64_t VkDescriptorSet;
typedef uint64_t VkPipelineLayout;
typedef uint64_t VkPipeline;
typedef uint64_t VkPipelineCache;
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
typedef uint32_t VkFilter;
typedef uint32_t VkSamplerMipmapMode;
typedef uint32_t VkSamplerAddressMode;
typedef uint32_t VkCompareOp;
typedef uint32_t VkBorderColor;
typedef uint32_t VkDescriptorType;
typedef uint32_t VkShaderStageFlags;
typedef uint32_t VkDescriptorSetLayoutCreateFlags;
typedef uint32_t VkDescriptorPoolCreateFlags;
typedef uint32_t VkPipelineLayoutCreateFlags;
typedef uint32_t VkPipelineCreateFlags;
typedef uint32_t VkPipelineShaderStageCreateFlags;
typedef uint32_t VkPipelineVertexInputStateCreateFlags;
typedef uint32_t VkPipelineInputAssemblyStateCreateFlags;
typedef uint32_t VkPipelineViewportStateCreateFlags;
typedef uint32_t VkPipelineRasterizationStateCreateFlags;
typedef uint32_t VkPipelineMultisampleStateCreateFlags;
typedef uint32_t VkPipelineDepthStencilStateCreateFlags;
typedef uint32_t VkPipelineColorBlendStateCreateFlags;
typedef uint32_t VkPipelineDynamicStateCreateFlags;
typedef uint32_t VkShaderStageFlagBits;
typedef uint32_t VkPrimitiveTopology;
typedef uint32_t VkPolygonMode;
typedef uint32_t VkCullModeFlags;
typedef uint32_t VkFrontFace;
typedef uint32_t VkColorComponentFlags;
typedef uint32_t VkBlendFactor;
typedef uint32_t VkBlendOp;
typedef uint32_t VkDynamicState;
typedef uint32_t VkLogicOp;
typedef uint32_t VkIndexType;
typedef uint32_t VkVertexInputRate;
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
static constexpr VkShaderModule VK_NULL_SHADER_MODULE = 0;
static constexpr VkSampler VK_NULL_SAMPLER = 0;
static constexpr VkDescriptorSetLayout VK_NULL_DESCRIPTOR_SET_LAYOUT = 0;
static constexpr VkDescriptorPool VK_NULL_DESCRIPTOR_POOL = 0;
static constexpr VkDescriptorSet VK_NULL_DESCRIPTOR_SET = 0;
static constexpr VkPipelineLayout VK_NULL_PIPELINE_LAYOUT = 0;
static constexpr VkPipeline VK_NULL_PIPELINE = 0;
static constexpr VkPipelineCache VK_NULL_PIPELINE_CACHE = 0;
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
static constexpr VkStructureType VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16;
static constexpr VkStructureType VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO = 31;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34;
static constexpr VkStructureType VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO = 19;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO = 20;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO = 22;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO = 23;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO = 24;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO = 25;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO = 26;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO = 27;
static constexpr VkStructureType VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO = 28;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;
static constexpr VkStructureType VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO = 43;
static constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 44;
static constexpr VkStructureType VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000;
static constexpr VkStructureType VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR = 1000008000;
static constexpr VkStructureType VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR = 1000001000;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001;
static constexpr VkQueueFlags VK_QUEUE_GRAPHICS_BIT = 0x00000001;
static constexpr VkFormat VK_FORMAT_B8G8R8A8_UNORM = 44;
static constexpr VkFormat VK_FORMAT_R8G8B8A8_UNORM = 37;
static constexpr VkFormat VK_FORMAT_R8G8B8A8_SRGB = 43;
static constexpr VkFormat VK_FORMAT_R8_UNORM = 9;
static constexpr VkFormat VK_FORMAT_R32_SFLOAT = 100;
static constexpr VkFormat VK_FORMAT_R32G32_SFLOAT = 103;
static constexpr VkFormat VK_FORMAT_R32G32B32_SFLOAT = 106;
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
static constexpr VkSurfaceTransformFlagBitsKHR VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR = 0x00000001;
static constexpr VkCompositeAlphaFlagBitsKHR VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR = 0x00000001;
static constexpr VkCommandPoolCreateFlags VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002;
static constexpr VkCommandBufferLevel VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0;
static constexpr VkCommandBufferUsageFlags VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400;
static constexpr VkPipelineStageFlags VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000;
static constexpr VkAccessFlags VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000;
static constexpr VkAccessFlags VK_ACCESS_TRANSFER_READ_BIT = 0x00000800;
static constexpr VkAccessFlags VK_ACCESS_SHADER_READ_BIT = 0x00000020;
static constexpr VkAccessFlags VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100;
static constexpr VkAccessFlags VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x00000400;
static constexpr VkAccessFlags VK_ACCESS_MEMORY_READ_BIT = 0x00008000;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_UNDEFINED = 0;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5;
static constexpr VkImageLayout VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6;
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
static constexpr VkImageViewType VK_IMAGE_VIEW_TYPE_2D_ARRAY = 5;
static constexpr VkComponentSwizzle VK_COMPONENT_SWIZZLE_IDENTITY = 0;
static constexpr VkFilter VK_FILTER_NEAREST = 0;
static constexpr VkFilter VK_FILTER_LINEAR = 1;
static constexpr VkSamplerMipmapMode VK_SAMPLER_MIPMAP_MODE_NEAREST = 0;
static constexpr VkSamplerMipmapMode VK_SAMPLER_MIPMAP_MODE_LINEAR = 1;
static constexpr VkSamplerAddressMode VK_SAMPLER_ADDRESS_MODE_REPEAT = 0;
static constexpr VkSamplerAddressMode VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT = 1;
static constexpr VkSamplerAddressMode VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE = 2;
static constexpr VkSamplerAddressMode VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE = 3;
static constexpr VkSamplerAddressMode VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER = 4;
static constexpr VkCompareOp VK_COMPARE_OP_ALWAYS = 7;
static constexpr VkCompareOp VK_COMPARE_OP_LESS_OR_EQUAL = 3;
static constexpr VkCompareOp VK_COMPARE_OP_GREATER_OR_EQUAL = 6;
static constexpr VkBorderColor VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK = 0;
static constexpr VkDescriptorType VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1;
static constexpr VkDescriptorType VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6;
static constexpr VkDescriptorType VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7;
static constexpr VkShaderStageFlags VK_SHADER_STAGE_VERTEX_BIT = 0x00000001;
static constexpr VkShaderStageFlags VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010;
static constexpr VkPrimitiveTopology VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3;
static constexpr VkPrimitiveTopology VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4;
static constexpr VkVertexInputRate VK_VERTEX_INPUT_RATE_VERTEX = 0;
static constexpr VkPolygonMode VK_POLYGON_MODE_FILL = 0;
static constexpr VkPolygonMode VK_POLYGON_MODE_LINE = 1;
static constexpr VkCullModeFlags VK_CULL_MODE_NONE = 0;
static constexpr VkCullModeFlags VK_CULL_MODE_FRONT_BIT = 0x00000001;
static constexpr VkCullModeFlags VK_CULL_MODE_BACK_BIT = 0x00000002;
static constexpr VkFrontFace VK_FRONT_FACE_CLOCKWISE = 0;
static constexpr VkFrontFace VK_FRONT_FACE_COUNTER_CLOCKWISE = 1;
static constexpr VkColorComponentFlags VK_COLOR_COMPONENT_R_BIT = 0x00000001;
static constexpr VkColorComponentFlags VK_COLOR_COMPONENT_G_BIT = 0x00000002;
static constexpr VkColorComponentFlags VK_COLOR_COMPONENT_B_BIT = 0x00000004;
static constexpr VkColorComponentFlags VK_COLOR_COMPONENT_A_BIT = 0x00000008;
static constexpr VkBlendFactor VK_BLEND_FACTOR_ZERO = 0;
static constexpr VkBlendFactor VK_BLEND_FACTOR_ONE = 1;
static constexpr VkBlendFactor VK_BLEND_FACTOR_SRC_ALPHA = 6;
static constexpr VkBlendFactor VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7;
static constexpr VkBlendOp VK_BLEND_OP_ADD = 0;
static constexpr VkDynamicState VK_DYNAMIC_STATE_VIEWPORT = 0;
static constexpr VkDynamicState VK_DYNAMIC_STATE_SCISSOR = 1;
static constexpr VkLogicOp VK_LOGIC_OP_COPY = 3;
static constexpr VkIndexType VK_INDEX_TYPE_UINT16 = 0;
static constexpr VkIndexType VK_INDEX_TYPE_UINT32 = 1;
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

struct VkViewport
{
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
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

struct VkClearDepthStencilValue
{
    float depth;
    uint32_t stencil;
};

union VkClearValue
{
    VkClearColorValue color;
    float depthStencil[2];
};

struct VkClearAttachment
{
    VkImageAspectFlags aspectMask;
    uint32_t colorAttachment;
    VkClearValue clearValue;
};

struct VkClearRect
{
    VkRect2D rect;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
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

struct VkShaderModuleCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    size_t codeSize;
    const uint32_t *pCode;
};

struct VkSamplerCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    VkFilter magFilter;
    VkFilter minFilter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode addressModeU;
    VkSamplerAddressMode addressModeV;
    VkSamplerAddressMode addressModeW;
    float mipLodBias;
    VkBool32 anisotropyEnable;
    float maxAnisotropy;
    VkBool32 compareEnable;
    VkCompareOp compareOp;
    float minLod;
    float maxLod;
    VkBorderColor borderColor;
    VkBool32 unnormalizedCoordinates;
};

struct VkDescriptorSetLayoutBinding
{
    uint32_t binding;
    VkDescriptorType descriptorType;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    const VkSampler *pImmutableSamplers;
};

struct VkDescriptorSetLayoutCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkDescriptorSetLayoutCreateFlags flags;
    uint32_t bindingCount;
    const VkDescriptorSetLayoutBinding *pBindings;
};

struct VkPipelineLayoutCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineLayoutCreateFlags flags;
    uint32_t setLayoutCount;
    const VkDescriptorSetLayout *pSetLayouts;
    uint32_t pushConstantRangeCount;
    const void *pPushConstantRanges;
};

struct VkDescriptorPoolSize
{
    VkDescriptorType type;
    uint32_t descriptorCount;
};

struct VkDescriptorPoolCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkDescriptorPoolCreateFlags flags;
    uint32_t maxSets;
    uint32_t poolSizeCount;
    const VkDescriptorPoolSize *pPoolSizes;
};

struct VkDescriptorSetAllocateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkDescriptorPool descriptorPool;
    uint32_t descriptorSetCount;
    const VkDescriptorSetLayout *pSetLayouts;
};

struct VkDescriptorBufferInfo
{
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize range;
};

struct VkDescriptorImageInfo
{
    VkSampler sampler;
    VkImageView imageView;
    VkImageLayout imageLayout;
};

struct VkWriteDescriptorSet
{
    VkStructureType sType;
    const void *pNext;
    VkDescriptorSet dstSet;
    uint32_t dstBinding;
    uint32_t dstArrayElement;
    uint32_t descriptorCount;
    VkDescriptorType descriptorType;
    const VkDescriptorImageInfo *pImageInfo;
    const VkDescriptorBufferInfo *pBufferInfo;
    const void *pTexelBufferView;
};

struct VkSpecializationInfo
{
    uint32_t mapEntryCount;
    const void *pMapEntries;
    size_t dataSize;
    const void *pData;
};

struct VkSpecializationMapEntry
{
    uint32_t constantID;
    uint32_t offset;
    size_t size;
};

struct VkPipelineShaderStageCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineShaderStageCreateFlags flags;
    VkShaderStageFlagBits stage;
    VkShaderModule module;
    const char *pName;
    const VkSpecializationInfo *pSpecializationInfo;
};

struct VkPipelineVertexInputStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineVertexInputStateCreateFlags flags;
    uint32_t vertexBindingDescriptionCount;
    const void *pVertexBindingDescriptions;
    uint32_t vertexAttributeDescriptionCount;
    const void *pVertexAttributeDescriptions;
};

struct VkVertexInputBindingDescription
{
    uint32_t binding;
    uint32_t stride;
    VkVertexInputRate inputRate;
};

struct VkVertexInputAttributeDescription
{
    uint32_t location;
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
};

struct VkPipelineInputAssemblyStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineInputAssemblyStateCreateFlags flags;
    VkPrimitiveTopology topology;
    VkBool32 primitiveRestartEnable;
};

struct VkPipelineViewportStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineViewportStateCreateFlags flags;
    uint32_t viewportCount;
    const void *pViewports;
    uint32_t scissorCount;
    const void *pScissors;
};

struct VkPipelineRasterizationStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineRasterizationStateCreateFlags flags;
    VkBool32 depthClampEnable;
    VkBool32 rasterizerDiscardEnable;
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    VkBool32 depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth;
};

struct VkPipelineMultisampleStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineMultisampleStateCreateFlags flags;
    VkSampleCountFlagBits rasterizationSamples;
    VkBool32 sampleShadingEnable;
    float minSampleShading;
    const uint32_t *pSampleMask;
    VkBool32 alphaToCoverageEnable;
    VkBool32 alphaToOneEnable;
};

struct VkStencilOpState
{
    uint32_t failOp;
    uint32_t passOp;
    uint32_t depthFailOp;
    VkCompareOp compareOp;
    uint32_t compareMask;
    uint32_t writeMask;
    uint32_t reference;
};

struct VkPipelineDepthStencilStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineDepthStencilStateCreateFlags flags;
    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;
    VkCompareOp depthCompareOp;
    VkBool32 depthBoundsTestEnable;
    VkBool32 stencilTestEnable;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds;
    float maxDepthBounds;
};

struct VkPipelineColorBlendAttachmentState
{
    VkBool32 blendEnable;
    VkBlendFactor srcColorBlendFactor;
    VkBlendFactor dstColorBlendFactor;
    VkBlendOp colorBlendOp;
    VkBlendFactor srcAlphaBlendFactor;
    VkBlendFactor dstAlphaBlendFactor;
    VkBlendOp alphaBlendOp;
    VkColorComponentFlags colorWriteMask;
};

struct VkPipelineColorBlendStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineColorBlendStateCreateFlags flags;
    VkBool32 logicOpEnable;
    VkLogicOp logicOp;
    uint32_t attachmentCount;
    const VkPipelineColorBlendAttachmentState *pAttachments;
    float blendConstants[4];
};

struct VkPipelineDynamicStateCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineDynamicStateCreateFlags flags;
    uint32_t dynamicStateCount;
    const VkDynamicState *pDynamicStates;
};

struct VkGraphicsPipelineCreateInfo
{
    VkStructureType sType;
    const void *pNext;
    VkPipelineCreateFlags flags;
    uint32_t stageCount;
    const VkPipelineShaderStageCreateInfo *pStages;
    const VkPipelineVertexInputStateCreateInfo *pVertexInputState;
    const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyState;
    const void *pTessellationState;
    const VkPipelineViewportStateCreateInfo *pViewportState;
    const VkPipelineRasterizationStateCreateInfo *pRasterizationState;
    const VkPipelineMultisampleStateCreateInfo *pMultisampleState;
    const VkPipelineDepthStencilStateCreateInfo *pDepthStencilState;
    const VkPipelineColorBlendStateCreateInfo *pColorBlendState;
    const VkPipelineDynamicStateCreateInfo *pDynamicState;
    VkPipelineLayout layout;
    VkRenderPass renderPass;
    uint32_t subpass;
    VkPipeline basePipelineHandle;
    int32_t basePipelineIndex;
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

struct VkImageResolve
{
    VkImageSubresourceLayers srcSubresource;
    VkOffset3D srcOffset;
    VkImageSubresourceLayers dstSubresource;
    VkOffset3D dstOffset;
    VkExtent3D extent;
};

struct VkImageBlit
{
    VkImageSubresourceLayers srcSubresource;
    VkOffset3D srcOffsets[2];
    VkImageSubresourceLayers dstSubresource;
    VkOffset3D dstOffsets[2];
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
#elif defined(ANDROID)
struct VkAndroidSurfaceCreateInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    VkFlags flags;
    ANativeWindow *window;
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
typedef void (*PFN_vkCmdClearDepthStencilImage)(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue *depthStencil, uint32_t rangeCount, const VkImageSubresourceRange *ranges);
typedef void (*PFN_vkCmdClearAttachments)(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment *attachments, uint32_t rectCount, const VkClearRect *rects);
typedef void (*PFN_vkCmdCopyBufferToImage)(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy *regions);
typedef void (*PFN_vkCmdCopyImageToBuffer)(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy *regions);
typedef void (*PFN_vkCmdResolveImage)(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve *regions);
typedef void (*PFN_vkCmdBlitImage)(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit *regions, VkFilter filter);
typedef void (*PFN_vkCmdBeginRenderPass)(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *renderPassBegin, VkSubpassContents contents);
typedef void (*PFN_vkCmdEndRenderPass)(VkCommandBuffer commandBuffer);
typedef void (*PFN_vkCmdSetViewport)(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport *viewports);
typedef void (*PFN_vkCmdSetScissor)(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D *scissors);
typedef void (*PFN_vkCmdBindPipeline)(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
typedef void (*PFN_vkCmdBindDescriptorSets)(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *descriptorSets, uint32_t dynamicOffsetCount, const uint32_t *dynamicOffsets);
typedef void (*PFN_vkCmdBindVertexBuffers)(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *buffers, const VkDeviceSize *offsets);
typedef void (*PFN_vkCmdBindIndexBuffer)(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
typedef void (*PFN_vkCmdDraw)(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
typedef void (*PFN_vkCmdDrawIndexed)(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
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
typedef VkResult (*PFN_vkCreateShaderModule)(VkDevice device, const VkShaderModuleCreateInfo *createInfo, const void *allocator, VkShaderModule *shaderModule);
typedef void (*PFN_vkDestroyShaderModule)(VkDevice device, VkShaderModule shaderModule, const void *allocator);
typedef VkResult (*PFN_vkCreateSampler)(VkDevice device, const VkSamplerCreateInfo *createInfo, const void *allocator, VkSampler *sampler);
typedef void (*PFN_vkDestroySampler)(VkDevice device, VkSampler sampler, const void *allocator);
typedef VkResult (*PFN_vkCreateDescriptorSetLayout)(VkDevice device, const VkDescriptorSetLayoutCreateInfo *createInfo, const void *allocator, VkDescriptorSetLayout *setLayout);
typedef void (*PFN_vkDestroyDescriptorSetLayout)(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const void *allocator);
typedef VkResult (*PFN_vkCreateDescriptorPool)(VkDevice device, const VkDescriptorPoolCreateInfo *createInfo, const void *allocator, VkDescriptorPool *descriptorPool);
typedef void (*PFN_vkDestroyDescriptorPool)(VkDevice device, VkDescriptorPool descriptorPool, const void *allocator);
typedef VkResult (*PFN_vkAllocateDescriptorSets)(VkDevice device, const VkDescriptorSetAllocateInfo *allocateInfo, VkDescriptorSet *descriptorSets);
typedef void (*PFN_vkUpdateDescriptorSets)(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *descriptorWrites, uint32_t descriptorCopyCount, const void *descriptorCopies);
typedef VkResult (*PFN_vkCreatePipelineLayout)(VkDevice device, const VkPipelineLayoutCreateInfo *createInfo, const void *allocator, VkPipelineLayout *pipelineLayout);
typedef void (*PFN_vkDestroyPipelineLayout)(VkDevice device, VkPipelineLayout pipelineLayout, const void *allocator);
typedef VkResult (*PFN_vkCreateGraphicsPipelines)(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *createInfos, const void *allocator, VkPipeline *pipelines);
typedef void (*PFN_vkDestroyPipeline)(VkDevice device, VkPipeline pipeline, const void *allocator);
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
#elif defined(ANDROID)
typedef VkResult (*PFN_vkCreateAndroidSurfaceKHR)(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR *createInfo, const void *allocator, VkSurfaceKHR *surface);
#endif

struct piShaderS
{
    const uint8_t *vs = nullptr;
    int vsLen = 0;
    const uint8_t *fs = nullptr;
    int fsLen = 0;
    piShaderOptions options = {};
    bool hasOptions = false;
    VkShaderModule vertexModule = VK_NULL_SHADER_MODULE;
    VkShaderModule fragmentModule = VK_NULL_SHADER_MODULE;
    VkPipelineLayout pipelineLayout = VK_NULL_PIPELINE_LAYOUT;
    VkPipeline pipeline = VK_NULL_PIPELINE;
    VkRenderPass pipelineRenderPass = VK_NULL_RENDER_PASS;
    VkCullModeFlags pipelineCullMode = VK_CULL_MODE_NONE;
    VkFrontFace pipelineFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkSampleCountFlagBits pipelineSampleCount = VK_SAMPLE_COUNT_1_BIT;
    bool pipelineWireframe = false;
    bool pipelineDepthClamp = false;
    bool pipelineDepthTest = false;
    bool pipelineDepthWrite = false;
    VkCompareOp pipelineDepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    bool pipelineAlphaToCoverage = false;
    bool pipelineBlendEnabled = false;
    uint32_t pipelineHostDepthBackdropMode = 0;
    bool isPicture = false;
    bool isPicture2D = false;
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
    bool ownsImageView = true;
    VkDeviceMemory memory = VK_NULL_DEVICE_MEMORY;
    VkFormat vkFormat = 0;
    VkImageUsageFlags imageUsage = 0;
    VkSampler sampler = VK_NULL_SAMPLER;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
};

struct piBufferS
{
    uint8_t *data = nullptr;
    unsigned int size = 0;
    piRenderer::BufferType type = piRenderer::BufferType::Static;
    piRenderer::BufferUse use = piRenderer::BufferUse::Vertex;
    VkBuffer buffer = VK_NULL_BUFFER;
    VkBuffer descriptorBuffer = VK_NULL_BUFFER;
    VkDeviceSize descriptorOffset = 0;
    VkDeviceMemory memory = VK_NULL_DEVICE_MEMORY;
    VkBufferUsageFlags usage = 0;
};

struct piVulkanBorrowedUpload
{
    VkBuffer buffer = VK_NULL_BUFFER;
    VkDeviceMemory memory = VK_NULL_DEVICE_MEMORY;
    uint64_t frameNumber = 0;
};

struct piVulkanBorrowedPipeline
{
    VkPipeline pipeline = VK_NULL_PIPELINE;
    uint64_t frameNumber = 0;
};

struct piVertexArrayS
{
    piBuffer vertexBuffer[2] = { nullptr, nullptr };
    piBuffer indexBuffer = nullptr;
    piRenderer::IndexArrayFormat indexFormat = piRenderer::IndexArrayFormat::UINT_32;
    uint32_t stride[2] = {};
    uint32_t attributeCount = 0;
    VkVertexInputAttributeDescription attributes[12] = {};
};

struct piRTargetS
{
    piTexture color[4] = { nullptr, nullptr, nullptr, nullptr };
    piTexture depth = nullptr;
    VkRenderPass renderPass = VK_NULL_RENDER_PASS;
    VkFramebuffer framebuffer = VK_NULL_FRAMEBUFFER;
    uint32_t subpass = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t colorAttachmentCount = 0;
    bool hasDepth = false;
    bool ownsRenderPassObjects = true;
};

struct piSamplerS
{
    piRenderer::TextureFilter filter = piRenderer::TextureFilter::NONE;
    piRenderer::TextureWrap wrap = piRenderer::TextureWrap::CLAMP;
    float anisotropy = 1.0f;
    VkSampler sampler = VK_NULL_SAMPLER;
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
    bool depthEnable = false;
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

static constexpr uint32_t kBorrowedFrameSlotCount = 8;
static constexpr uint32_t kBorrowedStaticPaintSetsPerFrame = 128;
static constexpr uint32_t kBorrowedPictureSetsPerFrame = 16;
static constexpr uint32_t kBorrowedStaticPaintSetCount =
    kBorrowedFrameSlotCount * kBorrowedStaticPaintSetsPerFrame;
static constexpr uint32_t kBorrowedPictureSetCount =
    kBorrowedFrameSlotCount * kBorrowedPictureSetsPerFrame;
static constexpr VkDeviceSize kBorrowedUniformBytesPerFrame = 1ull * 1024ull * 1024ull;
static constexpr VkDeviceSize kBorrowedUniformTotalBytes =
    kBorrowedFrameSlotCount * kBorrowedUniformBytesPerFrame;

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
    VkCommandBuffer hostPreviousCommandBuffer = VK_NULL_COMMAND_BUFFER;
    piRTarget hostPreviousRenderTarget = nullptr;
    VkSemaphore imageAvailableSemaphore = VK_NULL_SEMAPHORE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_SEMAPHORE;
    VkFence frameFence = VK_NULL_FENCE;
    VkBuffer stagingBuffer = VK_NULL_BUFFER;
    VkDeviceMemory stagingMemory = VK_NULL_DEVICE_MEMORY;
    VkDeviceSize stagingSize = 0;
    piTexture pendingPresentTexture = nullptr;
    piTexture externalFrameColorTexture = nullptr;
    piTexture externalFrameDepthTexture = nullptr;
    piRTarget externalFrameRenderTarget = nullptr;
    piTexture borrowedExternalColorTexture = nullptr;
    piTexture borrowedExternalDepthTexture = nullptr;
    piRTarget borrowedExternalRenderTarget = nullptr;
    bool externalFrameUsesHostDepth = false;
    bool externalFrameHostDepthReverseZ = false;
    bool externalFramePreservesHostColor = false;
    bool externalCommandBufferFrameActive = false;
    bool borrowedFrameResourcesActive = false;
    bool hostRenderPassFrameActive = false;
    bool hostRenderPassFrameReported = false;
    bool hostTextureUploadRejectedReported = false;
    bool hostDrawBisectionEnabled = false;
    uint32_t hostDrawBisectionStage = 0;
    uint32_t hostDrawBisectionMask = 0;
    uint32_t hostDrawBisectionAttempted = 0;
    uint32_t hostDrawBisectionAdmitted = 0;
    VkBuffer hostTransientUniformBuffer = VK_NULL_BUFFER;
    VkDeviceMemory hostTransientUniformMemory = VK_NULL_DEVICE_MEMORY;
    uint8_t *hostTransientUniformMapped = nullptr;
    VkDeviceSize hostTransientUniformSize = 0;
    VkDeviceSize hostTransientUniformOffset = 0;
    VkDeviceSize hostTransientUniformLimit = 0;
    uint64_t borrowedFrameNumbers[kBorrowedFrameSlotCount] =
    {
        ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull
    };
    uint32_t borrowedFrameSlot = 0;
    uint64_t borrowedCurrentFrameNumber = 0;
    uint32_t borrowedStaticPaintSetCursor = 0;
    uint32_t borrowedPictureSetCursor = 0;
    std::vector<piVulkanBorrowedUpload> borrowedUploads;
    std::vector<piVulkanBorrowedPipeline> borrowedPipelines;
    uint32_t presentFrameIndex = 0;
    bool realPresentReported = false;
    bool texturePresentReported = false;
    bool directTexturePresentReported = false;
    bool textureImageReported = false;
    bool bufferReported = false;
    bool shaderModuleReported = false;
    bool descriptorLayoutReported = false;
    bool descriptorSetReported = false;
    bool descriptorSetFailureReported = false;
    bool graphicsPipelineReported = false;
    bool drawSubmittedReported = false;
    bool drawSubmitFailureReported = false;
    bool clearTextureReported = false;
    bool gpuReadbackReported = false;
    bool gpuReadbackFailureReported = false;
    bool gpuPaintActive = false;
    uint32_t gpuReadbackBestNonblack = 0;
    int windowWidth = 1;
    int windowHeight = 1;
#if defined(WINDOWS)
    HMODULE vulkanLibrary = nullptr;
    HWND window = nullptr;
    bool captureWritten = false;
#elif defined(ANDROID)
    void *vulkanLibrary = nullptr;
    ANativeWindow *window = nullptr;
#endif
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
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
    PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage = nullptr;
    PFN_vkCmdClearAttachments vkCmdClearAttachments = nullptr;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer = nullptr;
    PFN_vkCmdResolveImage vkCmdResolveImage = nullptr;
    PFN_vkCmdBlitImage vkCmdBlitImage = nullptr;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
    PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
    PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
    PFN_vkCmdDraw vkCmdDraw = nullptr;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
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
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCreateSampler vkCreateSampler = nullptr;
    PFN_vkDestroySampler vkDestroySampler = nullptr;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
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
#elif defined(ANDROID)
    PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = nullptr;
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
    bool depthWriteEnabled = true;
    bool depthTestEnabled = true;
    piTexture textures[16] = {};
    piSampler samplers[8] = {};
    piBuffer constantBuffers[16] = {};
    piBuffer shaderBuffers[16] = {};
    VkDescriptorSetLayout staticPaintDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
    VkDescriptorPool staticPaintDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
    VkDescriptorSet staticPaintDescriptorSet = VK_NULL_DESCRIPTOR_SET;
    VkDescriptorSet staticPaintDescriptorSets[kBorrowedStaticPaintSetCount] = {};
    VkPipelineLayout staticPaintPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
    VkDescriptorSetLayout pictureDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
    VkDescriptorPool pictureDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
    VkDescriptorSet pictureDescriptorSet = VK_NULL_DESCRIPTOR_SET;
    VkDescriptorSet pictureDescriptorSets[kBorrowedPictureSetCount] = {};
    VkPipelineLayout picturePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
    VkDescriptorSetLayout presentDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
    VkDescriptorPool presentDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
    VkDescriptorSet presentDescriptorSet = VK_NULL_DESCRIPTOR_SET;
    VkPipelineLayout presentPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
    VkShaderModule presentVertexModule = VK_NULL_SHADER_MODULE;
    VkShaderModule presentFragmentModule = VK_NULL_SHADER_MODULE;
    VkPipeline presentPipeline = VK_NULL_PIPELINE;
    VkSampler presentSampler = VK_NULL_SAMPLER;
    VkPipelineLayout hostDebugTrianglePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
    VkShaderModule hostDebugTriangleVertexModule = VK_NULL_SHADER_MODULE;
    VkShaderModule hostIndexedControlVertexModule = VK_NULL_SHADER_MODULE;
    VkShaderModule hostDebugTriangleFragmentModule = VK_NULL_SHADER_MODULE;
    VkShaderModule hostCenterDiagnosticVertexModule = VK_NULL_SHADER_MODULE;
    VkShaderModule hostDescriptorDiagnosticFragmentModule = VK_NULL_SHADER_MODULE;
    piBuffer hostDebugIndexBuffer = nullptr;
    piBuffer hostDebugIndexSource = nullptr;
    uint32_t hostDebugIndexSourceBase = 0;
    uint32_t hostDebugIndexCount = 0;
    piBuffer hostDebugResourceBuffers[4] = { nullptr, nullptr, nullptr, nullptr };
    VkPipeline hostDebugTrianglePipeline = VK_NULL_PIPELINE;
    VkRenderPass hostDebugTriangleRenderPass = VK_NULL_RENDER_PASS;
    uint32_t hostDebugTriangleSubpass = 0;
    VkSampleCountFlagBits hostDebugTriangleSampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkPipeline hostDescriptorDiagnosticPipeline = VK_NULL_PIPELINE;
    VkRenderPass hostDescriptorDiagnosticRenderPass = VK_NULL_RENDER_PASS;
    uint32_t hostDescriptorDiagnosticSubpass = 0;
    VkSampleCountFlagBits hostDescriptorDiagnosticSampleCount = VK_SAMPLE_COUNT_1_BIT;
    piQuery perfQueries[2] = { nullptr, nullptr };
    int currentPerformanceQuery = 0;
    bool unsupportedReported[(int)piVulkanUnsupportedFeature::Count] = {};
    bool cpuDrawDiagnosticReported = false;
    bool cpuPaintDiagnosticReported = false;
    bool cpuPresentDiagnosticReported = false;
    bool cpuPictureDiagnosticReported = false;
    bool pictureLayoutReported = false;
    bool pictureDescriptorReported = false;
    bool picturePipelineReported = false;
    bool pictureDrawReported = false;
    bool pictureDrawFailureReported = false;
    bool presentLayoutReported = false;
    bool presentDescriptorReported = false;
    bool presentPipelineReported = false;
    bool presentPassReported = false;
    bool presentSelectionReported = false;
    uint32_t cpuPaintDrawCount = 0;
    uint32_t gpuPaintDrawCount = 0;
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

static void iDebugLog(const char *message)
{
    const char *path = std::getenv("IMM_VULKAN_DEBUG_LOG_PATH");
    if (!path || !message)
    {
        return;
    }
    FILE *file = std::fopen(path, "ab");
    if (!file)
    {
        return;
    }
    std::fprintf(file, "%s\n", message);
    std::fclose(file);
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

static uint32_t iVertexFormatSize(piRenderer::Format format)
{
    switch (format)
    {
        case piRenderer::Format::C1_32_FLOAT: return 4;
        case piRenderer::Format::C2_32_FLOAT: return 8;
        case piRenderer::Format::C3_32_FLOAT: return 12;
        case piRenderer::Format::C4_32_FLOAT: return 16;
        default: return 0;
    }
}

static VkFormat iVertexFormatPiToVulkan(piRenderer::Format format)
{
    switch (format)
    {
        case piRenderer::Format::C2_32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case piRenderer::Format::C3_32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case piRenderer::Format::C4_32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
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

static float iDecodeUnsignedFloat(uint32_t bits, uint32_t mantissaBits)
{
    const uint32_t exponentMask = 31u;
    const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const uint32_t exponent = (bits >> mantissaBits) & exponentMask;
    const uint32_t mantissa = bits & mantissaMask;
    if (exponent == 0)
    {
        if (mantissa == 0)
        {
            return 0.0f;
        }
        return std::ldexp((float)mantissa / (float)(1u << mantissaBits), -14);
    }
    if (exponent == exponentMask)
    {
        return mantissa == 0 ? 65504.0f : 0.0f;
    }
    return std::ldexp(1.0f + (float)mantissa / (float)(1u << mantissaBits), (int)exponent - 15);
}

static uint8_t iFloatToByte(float value)
{
    if (value <= 0.0f)
    {
        return 0;
    }
    if (value >= 1.0f)
    {
        return 255;
    }
    return (uint8_t)(value * 255.0f + 0.5f);
}

static uint8_t iLinearFloatToSrgbByte(float value)
{
    if (value <= 0.0f)
    {
        return 0;
    }
    if (value >= 1.0f)
    {
        return 255;
    }
    const float encoded = value < 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    return iFloatToByte(encoded);
}

static void iConvertB10G11R11ToRgba8(uint8_t *dst, const uint8_t *src, size_t pixelCount)
{
    for (size_t i = 0; i < pixelCount; ++i)
    {
        uint32_t packed = 0;
        std::memcpy(&packed, src + i * 4u, sizeof(packed));
        const uint32_t r = packed & 0x7ffu;
        const uint32_t g = (packed >> 11u) & 0x7ffu;
        const uint32_t b = (packed >> 22u) & 0x3ffu;
        dst[i * 4u + 0u] = iFloatToByte(iDecodeUnsignedFloat(r, 6));
        dst[i * 4u + 1u] = iFloatToByte(iDecodeUnsignedFloat(g, 6));
        dst[i * 4u + 2u] = iFloatToByte(iDecodeUnsignedFloat(b, 5));
        dst[i * 4u + 3u] = 255;
    }
}

static uint32_t iEncodeUnsignedFloat(float value, uint32_t mantissaBits)
{
    if (value <= 0.0f)
    {
        return 0;
    }
    if (value > 65000.0f)
    {
        value = 65000.0f;
    }
    int exponent = 0;
    float normalized = std::frexp(value, &exponent) * 2.0f;
    exponent -= 1;
    int biasedExponent = exponent + 15;
    if (biasedExponent <= 0)
    {
        const float scaled = std::ldexp(value, 14 + (int)mantissaBits);
        const uint32_t mantissa = (uint32_t)(scaled + 0.5f);
        const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
        return mantissa > mantissaMask ? mantissaMask : mantissa;
    }
    if (biasedExponent >= 31)
    {
        return 31u << mantissaBits;
    }
    const float fraction = normalized - 1.0f;
    const uint32_t mantissa = (uint32_t)(fraction * (float)(1u << mantissaBits) + 0.5f);
    if (mantissa >= (1u << mantissaBits))
    {
        ++biasedExponent;
        if (biasedExponent >= 31)
        {
            return 31u << mantissaBits;
        }
        return (uint32_t)biasedExponent << mantissaBits;
    }
    return ((uint32_t)biasedExponent << mantissaBits) | mantissa;
}

static uint32_t iPackRgba8ToB10G11R11(const uint8_t *src)
{
    const float r = (float)src[0] / 255.0f;
    const float g = (float)src[1] / 255.0f;
    const float b = (float)src[2] / 255.0f;
    return (iEncodeUnsignedFloat(r, 6) & 0x7ffu) |
           ((iEncodeUnsignedFloat(g, 6) & 0x7ffu) << 11u) |
           ((iEncodeUnsignedFloat(b, 5) & 0x3ffu) << 22u);
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
static bool iCaptureRequested()
{
    const char *path = std::getenv("IMM_VULKAN_CPU_CAPTURE_PATH");
    return path && path[0] != 0;
}

static void iWritePpmCapture(piVulkanState *state, piTexture texture)
{
    if (!state || !texture || !texture->data)
    {
        return;
    }
    if (!iCaptureRequested())
    {
        return;
    }
    const char *path = std::getenv("IMM_VULKAN_CPU_CAPTURE_PATH");
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
    char tempPath[1024];
    std::snprintf(tempPath, sizeof(tempPath), "%s.tmp", path);
    FILE *file = std::fopen(tempPath, "wb");
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
            if (std::fwrite(rgb, 1, sizeof(rgb), file) != sizeof(rgb))
            {
                std::fclose(file);
                std::remove(tempPath);
                return;
            }
        }
    }
    std::fclose(file);
    std::remove(path);
    if (std::rename(tempPath, path) != 0)
    {
        std::remove(tempPath);
        return;
    }
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
#elif defined(ANDROID)
    state->vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)state->vkGetInstanceProcAddr(state->instance, "vkCreateAndroidSurfaceKHR");
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
    state->vkCmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage)state->vkGetDeviceProcAddr(state->device, "vkCmdClearDepthStencilImage");
    state->vkCmdClearAttachments = (PFN_vkCmdClearAttachments)state->vkGetDeviceProcAddr(state->device, "vkCmdClearAttachments");
    state->vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)state->vkGetDeviceProcAddr(state->device, "vkCmdCopyBufferToImage");
    state->vkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)state->vkGetDeviceProcAddr(state->device, "vkCmdCopyImageToBuffer");
    state->vkCmdResolveImage = (PFN_vkCmdResolveImage)state->vkGetDeviceProcAddr(state->device, "vkCmdResolveImage");
    state->vkCmdBlitImage = (PFN_vkCmdBlitImage)state->vkGetDeviceProcAddr(state->device, "vkCmdBlitImage");
    state->vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)state->vkGetDeviceProcAddr(state->device, "vkCmdBeginRenderPass");
    state->vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)state->vkGetDeviceProcAddr(state->device, "vkCmdEndRenderPass");
    state->vkCmdSetViewport = (PFN_vkCmdSetViewport)state->vkGetDeviceProcAddr(state->device, "vkCmdSetViewport");
    state->vkCmdSetScissor = (PFN_vkCmdSetScissor)state->vkGetDeviceProcAddr(state->device, "vkCmdSetScissor");
    state->vkCmdBindPipeline = (PFN_vkCmdBindPipeline)state->vkGetDeviceProcAddr(state->device, "vkCmdBindPipeline");
    state->vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)state->vkGetDeviceProcAddr(state->device, "vkCmdBindDescriptorSets");
    state->vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)state->vkGetDeviceProcAddr(state->device, "vkCmdBindVertexBuffers");
    state->vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)state->vkGetDeviceProcAddr(state->device, "vkCmdBindIndexBuffer");
    state->vkCmdDraw = (PFN_vkCmdDraw)state->vkGetDeviceProcAddr(state->device, "vkCmdDraw");
    state->vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)state->vkGetDeviceProcAddr(state->device, "vkCmdDrawIndexed");
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
    state->vkCreateShaderModule = (PFN_vkCreateShaderModule)state->vkGetDeviceProcAddr(state->device, "vkCreateShaderModule");
    state->vkDestroyShaderModule = (PFN_vkDestroyShaderModule)state->vkGetDeviceProcAddr(state->device, "vkDestroyShaderModule");
    state->vkCreateSampler = (PFN_vkCreateSampler)state->vkGetDeviceProcAddr(state->device, "vkCreateSampler");
    state->vkDestroySampler = (PFN_vkDestroySampler)state->vkGetDeviceProcAddr(state->device, "vkDestroySampler");
    state->vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)state->vkGetDeviceProcAddr(state->device, "vkCreateDescriptorSetLayout");
    state->vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)state->vkGetDeviceProcAddr(state->device, "vkDestroyDescriptorSetLayout");
    state->vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)state->vkGetDeviceProcAddr(state->device, "vkCreateDescriptorPool");
    state->vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)state->vkGetDeviceProcAddr(state->device, "vkDestroyDescriptorPool");
    state->vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)state->vkGetDeviceProcAddr(state->device, "vkAllocateDescriptorSets");
    state->vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)state->vkGetDeviceProcAddr(state->device, "vkUpdateDescriptorSets");
    state->vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)state->vkGetDeviceProcAddr(state->device, "vkCreatePipelineLayout");
    state->vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)state->vkGetDeviceProcAddr(state->device, "vkDestroyPipelineLayout");
    state->vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)state->vkGetDeviceProcAddr(state->device, "vkCreateGraphicsPipelines");
    state->vkDestroyPipeline = (PFN_vkDestroyPipeline)state->vkGetDeviceProcAddr(state->device, "vkDestroyPipeline");
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
    const bool needsSwapchainEntryPoints = state->surface != VK_NULL_SURFACE_KHR;
    if ((needsSwapchainEntryPoints && (!state->vkCreateSwapchainKHR || !state->vkDestroySwapchainKHR || !state->vkGetSwapchainImagesKHR ||
                                      !state->vkAcquireNextImageKHR || !state->vkQueuePresentKHR)) ||
        !state->vkCreateCommandPool || !state->vkDestroyCommandPool || !state->vkAllocateCommandBuffers ||
        !state->vkResetCommandBuffer || !state->vkBeginCommandBuffer || !state->vkEndCommandBuffer ||
        !state->vkCmdPipelineBarrier || !state->vkCmdClearColorImage || !state->vkCmdClearDepthStencilImage || !state->vkCmdCopyBufferToImage || !state->vkCmdCopyImageToBuffer || !state->vkCmdResolveImage || !state->vkCmdBlitImage ||
        !state->vkCmdBeginRenderPass || !state->vkCmdEndRenderPass ||
        !state->vkCmdSetViewport || !state->vkCmdSetScissor || !state->vkCmdBindPipeline ||
        !state->vkCmdBindDescriptorSets || !state->vkCmdBindVertexBuffers || !state->vkCmdBindIndexBuffer || !state->vkCmdDraw || !state->vkCmdDrawIndexed ||
        !state->vkCreateBuffer || !state->vkDestroyBuffer || !state->vkGetBufferMemoryRequirements ||
        !state->vkCreateImage || !state->vkDestroyImage || !state->vkGetImageMemoryRequirements ||
        !state->vkCreateImageView || !state->vkDestroyImageView ||
        !state->vkCreateRenderPass || !state->vkDestroyRenderPass || !state->vkCreateFramebuffer || !state->vkDestroyFramebuffer ||
        !state->vkCreateShaderModule || !state->vkDestroyShaderModule || !state->vkCreateSampler || !state->vkDestroySampler ||
        !state->vkCreateDescriptorSetLayout || !state->vkDestroyDescriptorSetLayout ||
        !state->vkCreateDescriptorPool || !state->vkDestroyDescriptorPool || !state->vkAllocateDescriptorSets || !state->vkUpdateDescriptorSets ||
        !state->vkCreatePipelineLayout || !state->vkDestroyPipelineLayout || !state->vkCreateGraphicsPipelines || !state->vkDestroyPipeline ||
        !state->vkAllocateMemory || !state->vkFreeMemory || !state->vkBindBufferMemory || !state->vkBindImageMemory ||
        !state->vkMapMemory || !state->vkUnmapMemory || !state->vkCreateSemaphore ||
        !state->vkDestroySemaphore || !state->vkCreateFence || !state->vkDestroyFence || !state->vkWaitForFences ||
        !state->vkResetFences || !state->vkQueueSubmit)
    {
        iError(reporter, "Vulkan renderer could not load required device frame entry points");
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
#elif defined(ANDROID)
    if (!state->window)
    {
        return true;
    }
    if (!state->vkCreateAndroidSurfaceKHR)
    {
        iError(reporter, "Vulkan renderer could not load vkCreateAndroidSurfaceKHR");
        return false;
    }
    VkAndroidSurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = state->window;
    const VkResult result = state->vkCreateAndroidSurfaceKHR(state->instance, &surfaceInfo, nullptr, &state->surface);
    if (result != VK_SUCCESS || state->surface == VK_NULL_SURFACE_KHR)
    {
        iError(reporter, "Vulkan renderer failed to create Android VkSurfaceKHR");
        return false;
    }
    iReport(reporter, "Vulkan renderer created Android surface");
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
    bool hasMailboxPresent = false;
    for (uint32_t i = 0; i < presentModeCount; ++i)
    {
        if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            selectedPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            hasMailboxPresent = true;
        }
    }
    if (selectedPresentMode == VK_PRESENT_MODE_FIFO_KHR && hasMailboxPresent)
    {
        selectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
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
    const VkSurfaceTransformFlagBitsKHR selectedTransform =
        (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : capabilities.currentTransform;

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
    swapchainInfo.preTransform = selectedTransform;
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
    {
        char message[256];
        std::snprintf(message,
                      sizeof(message),
                      "IMMAVAL swapchain transform current=%u supported=0x%x selected=%u",
                      (unsigned int)capabilities.currentTransform,
                      (unsigned int)capabilities.supportedTransforms,
                      (unsigned int)selectedTransform);
        iReport(reporter, message);
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
                  "Vulkan renderer created swapchain %ux%u images=%u format=%u presentMode=%u",
                  extent.width,
                  extent.height,
                  swapchainImageCount,
                  selectedFormat.format,
                  selectedPresentMode);
    iReport(reporter, message);
    return true;
}

static bool iCreateVulkanFrameResources(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state)
    {
        return false;
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

    const bool needsSwapchainSemaphores = state->swapchain != VK_NULL_SWAPCHAIN_KHR;
    if (needsSwapchainSemaphores)
    {
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

static bool iCreateDeviceLocalImage(piVulkanState *state,
                                    uint32_t width,
                                    uint32_t height,
                                    VkFormat format,
                                    VkSampleCountFlagBits samples,
                                    VkImageUsageFlags usage,
                                    VkImage *outImage,
                                    VkDeviceMemory *outMemory,
                                    piRenderer::piReporter *reporter)
{
    if (!state || !outImage || !outMemory || width == 0 || height == 0 ||
        !state->vkCreateImage || !state->vkGetImageMemoryRequirements || !state->vkAllocateMemory || !state->vkBindImageMemory)
    {
        return false;
    }

    *outImage = 0;
    *outMemory = VK_NULL_DEVICE_MEMORY;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = state->vkCreateImage(state->device, &imageInfo, nullptr, outImage);
    if (result != VK_SUCCESS || *outImage == 0)
    {
        iError(reporter, "Vulkan renderer failed to create device-local image");
        *outImage = 0;
        return false;
    }

    VkMemoryRequirements requirements = {};
    state->vkGetImageMemoryRequirements(state->device, *outImage, &requirements);
    uint32_t memoryTypeIndex = 0;
    if (!iFindMemoryType(state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryTypeIndex))
    {
        iError(reporter, "Vulkan renderer failed to find device-local image memory");
        state->vkDestroyImage(state->device, *outImage, nullptr);
        *outImage = 0;
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, outMemory);
    if (result != VK_SUCCESS || *outMemory == VK_NULL_DEVICE_MEMORY)
    {
        iError(reporter, "Vulkan renderer failed to allocate device-local image memory");
        state->vkDestroyImage(state->device, *outImage, nullptr);
        *outImage = 0;
        *outMemory = VK_NULL_DEVICE_MEMORY;
        return false;
    }

    result = state->vkBindImageMemory(state->device, *outImage, *outMemory, 0);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to bind device-local image memory");
        state->vkDestroyImage(state->device, *outImage, nullptr);
        state->vkFreeMemory(state->device, *outMemory, nullptr);
        *outImage = 0;
        *outMemory = VK_NULL_DEVICE_MEMORY;
        return false;
    }
    return true;
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
    buffer->descriptorBuffer = buffer->buffer;
    buffer->descriptorOffset = 0;

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

static piBuffer iCloneHostDebugBuffer(piVulkanState *state, piBuffer source, piRenderer::piReporter *reporter)
{
    if (!state || !source || !source->data || source->size == 0u)
    {
        return nullptr;
    }
    piBuffer clone = new piBufferS();
    clone->size = source->size;
    clone->type = source->type;
    clone->use = source->use;
    clone->data = static_cast<uint8_t *>(std::malloc(clone->size));
    if (!clone->data)
    {
        delete clone;
        return nullptr;
    }
    std::memcpy(clone->data, source->data, clone->size);
    if (!iCreateBufferObject(state, clone, clone->data, reporter))
    {
        std::free(clone->data);
        delete clone;
        return nullptr;
    }
    return clone;
}

static bool iEnsureHostTransientUniformBuffer(piVulkanState *state, VkDeviceSize size, piRenderer::piReporter *reporter)
{
    if (!state || state->device == VK_NULL_DEVICE)
    {
        return false;
    }
    if (state->hostTransientUniformBuffer != VK_NULL_BUFFER && state->hostTransientUniformMapped && state->hostTransientUniformSize >= size)
    {
        return true;
    }
    if (state->hostTransientUniformMapped)
    {
        state->vkUnmapMemory(state->device, state->hostTransientUniformMemory);
        state->hostTransientUniformMapped = nullptr;
    }
    if (state->hostTransientUniformBuffer != VK_NULL_BUFFER && state->vkDestroyBuffer)
    {
        state->vkDestroyBuffer(state->device, state->hostTransientUniformBuffer, nullptr);
        state->hostTransientUniformBuffer = VK_NULL_BUFFER;
    }
    if (state->hostTransientUniformMemory != VK_NULL_DEVICE_MEMORY && state->vkFreeMemory)
    {
        state->vkFreeMemory(state->device, state->hostTransientUniformMemory, nullptr);
        state->hostTransientUniformMemory = VK_NULL_DEVICE_MEMORY;
    }

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = state->vkCreateBuffer(state->device, &bufferInfo, nullptr, &state->hostTransientUniformBuffer);
    if (result != VK_SUCCESS || state->hostTransientUniformBuffer == VK_NULL_BUFFER)
    {
        iError(reporter, "Vulkan renderer failed to create host transient uniform buffer");
        state->hostTransientUniformBuffer = VK_NULL_BUFFER;
        return false;
    }

    VkMemoryRequirements requirements = {};
    state->vkGetBufferMemoryRequirements(state->device, state->hostTransientUniformBuffer, &requirements);
    uint32_t memoryTypeIndex = 0;
    if (!iFindMemoryType(state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memoryTypeIndex))
    {
        iError(reporter, "Vulkan renderer failed to find host transient uniform buffer memory");
        state->vkDestroyBuffer(state->device, state->hostTransientUniformBuffer, nullptr);
        state->hostTransientUniformBuffer = VK_NULL_BUFFER;
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, &state->hostTransientUniformMemory);
    if (result != VK_SUCCESS || state->hostTransientUniformMemory == VK_NULL_DEVICE_MEMORY)
    {
        iError(reporter, "Vulkan renderer failed to allocate host transient uniform buffer memory");
        state->vkDestroyBuffer(state->device, state->hostTransientUniformBuffer, nullptr);
        state->hostTransientUniformBuffer = VK_NULL_BUFFER;
        state->hostTransientUniformMemory = VK_NULL_DEVICE_MEMORY;
        return false;
    }
    result = state->vkBindBufferMemory(state->device, state->hostTransientUniformBuffer, state->hostTransientUniformMemory, 0);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to bind host transient uniform buffer memory");
        state->vkDestroyBuffer(state->device, state->hostTransientUniformBuffer, nullptr);
        state->vkFreeMemory(state->device, state->hostTransientUniformMemory, nullptr);
        state->hostTransientUniformBuffer = VK_NULL_BUFFER;
        state->hostTransientUniformMemory = VK_NULL_DEVICE_MEMORY;
        return false;
    }
    void *mapped = nullptr;
    result = state->vkMapMemory(state->device, state->hostTransientUniformMemory, 0, requirements.size, 0, &mapped);
    if (result != VK_SUCCESS || !mapped)
    {
        iError(reporter, "Vulkan renderer failed to map host transient uniform buffer memory");
        state->vkDestroyBuffer(state->device, state->hostTransientUniformBuffer, nullptr);
        state->vkFreeMemory(state->device, state->hostTransientUniformMemory, nullptr);
        state->hostTransientUniformBuffer = VK_NULL_BUFFER;
        state->hostTransientUniformMemory = VK_NULL_DEVICE_MEMORY;
        return false;
    }
    state->hostTransientUniformMapped = static_cast<uint8_t *>(mapped);
    state->hostTransientUniformSize = requirements.size;
    state->hostTransientUniformOffset = 0;
    return true;
}

static VkDeviceSize iAlignVkDeviceSize(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static bool iAllocateHostTransientUniformSlice(piVulkanState *state, piBuffer buffer, const void *data, unsigned int len, piRenderer::piReporter *reporter)
{
    static const VkDeviceSize kAlignment = 256;
    if (!state || !buffer || !data || len == 0)
    {
        return false;
    }
    if (!iEnsureHostTransientUniformBuffer(state, kBorrowedUniformTotalBytes, reporter))
    {
        return false;
    }
    VkDeviceSize offset = iAlignVkDeviceSize(state->hostTransientUniformOffset, kAlignment);
    const VkDeviceSize limit = state->hostTransientUniformLimit != 0
        ? state->hostTransientUniformLimit
        : state->hostTransientUniformSize;
    if (offset + buffer->size > limit)
    {
        iError(reporter, "Vulkan renderer host transient uniform buffer exhausted");
        return false;
    }
    std::memset(state->hostTransientUniformMapped + offset, 0, buffer->size);
    std::memcpy(state->hostTransientUniformMapped + offset, data, len);
    buffer->descriptorBuffer = state->hostTransientUniformBuffer;
    buffer->descriptorOffset = offset;
    state->hostTransientUniformOffset = offset + buffer->size;
    return true;
}

static VkDescriptorBufferInfo iDescriptorBufferInfo(piBuffer buffer)
{
    VkDescriptorBufferInfo info = {};
    if (buffer)
    {
        info.buffer = buffer->descriptorBuffer != VK_NULL_BUFFER ? buffer->descriptorBuffer : buffer->buffer;
        info.offset = buffer->descriptorOffset;
        info.range = buffer->size;
    }
    return info;
}

static bool iLooksLikeSpirv(const uint8_t *code, int len)
{
    if (!code || len < 4 || (len & 3) != 0)
    {
        return false;
    }
    const uint32_t magic = ((const uint32_t *)code)[0];
    return magic == 0x07230203u;
}

static bool iCreateShaderModule(piVulkanState *state, const uint8_t *code, int len, VkShaderModule *outModule, piRenderer::piReporter *reporter)
{
    if (!outModule)
    {
        return false;
    }
    *outModule = VK_NULL_SHADER_MODULE;
    if (!state || state->device == VK_NULL_DEVICE || !state->vkCreateShaderModule || !iLooksLikeSpirv(code, len))
    {
        return true;
    }
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = (size_t)len;
    createInfo.pCode = (const uint32_t *)code;
    const VkResult result = state->vkCreateShaderModule(state->device, &createInfo, nullptr, outModule);
    if (result != VK_SUCCESS || *outModule == VK_NULL_SHADER_MODULE)
    {
        iError(reporter, "Vulkan renderer failed to create shader module");
        *outModule = VK_NULL_SHADER_MODULE;
        return false;
    }
    return true;
}

static VkFilter iToVulkanFilter(piRenderer::TextureFilter filter)
{
    return (filter == piRenderer::TextureFilter::LINEAR ||
            filter == piRenderer::TextureFilter::MIPMAP ||
            filter == piRenderer::TextureFilter::PCF) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}

static VkSamplerMipmapMode iToVulkanMipmapMode(piRenderer::TextureFilter filter)
{
    return (filter == piRenderer::TextureFilter::MIPMAP ||
            filter == piRenderer::TextureFilter::NONE_MIPMAP) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

static VkSamplerAddressMode iToVulkanAddressMode(piRenderer::TextureWrap wrap)
{
    switch (wrap)
    {
        case piRenderer::TextureWrap::REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case piRenderer::TextureWrap::MIRROR_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case piRenderer::TextureWrap::MIRROR_CLAMP: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case piRenderer::TextureWrap::CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case piRenderer::TextureWrap::CLAMP:
        default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

static bool iCreateSamplerObject(piVulkanState *state, piRenderer::TextureFilter filter, piRenderer::TextureWrap wrap, float anisotropy, VkSampler *outSampler, piRenderer::piReporter *reporter)
{
    if (!outSampler)
    {
        return false;
    }
    *outSampler = VK_NULL_SAMPLER;
    if (!state || state->device == VK_NULL_DEVICE || !state->vkCreateSampler)
    {
        return true;
    }
    VkSamplerCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = iToVulkanFilter(filter);
    createInfo.minFilter = iToVulkanFilter(filter);
    createInfo.mipmapMode = iToVulkanMipmapMode(filter);
    createInfo.addressModeU = iToVulkanAddressMode(wrap);
    createInfo.addressModeV = iToVulkanAddressMode(wrap);
    createInfo.addressModeW = iToVulkanAddressMode(wrap);
    (void)anisotropy;
    createInfo.maxAnisotropy = 1.0f;
    createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    createInfo.maxLod = 1000.0f;
    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    const VkResult result = state->vkCreateSampler(state->device, &createInfo, nullptr, outSampler);
    if (result != VK_SUCCESS || *outSampler == VK_NULL_SAMPLER)
    {
        iError(reporter, "Vulkan renderer failed to create sampler");
        *outSampler = VK_NULL_SAMPLER;
        return false;
    }
    return true;
}

static bool iEnsureStaticPaintPipelineLayout(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->device == VK_NULL_DEVICE)
    {
        return true;
    }
    if (state->staticPaintPipelineLayout != VK_NULL_PIPELINE_LAYOUT)
    {
        return true;
    }
    if (!state->vkCreateDescriptorSetLayout || !state->vkCreatePipelineLayout ||
        !state->vkCreateDescriptorPool || !state->vkAllocateDescriptorSets)
    {
        return true;
    }

    VkDescriptorSetLayoutBinding bindings[7] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 3;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding = 4;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].binding = 5;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[4].binding = 7;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[5].binding = 8;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[6].binding = 9;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = 7;
    setLayoutInfo.pBindings = bindings;
    VkResult result = state->vkCreateDescriptorSetLayout(state->device, &setLayoutInfo, nullptr, &state->staticPaintDescriptorSetLayout);
    if (result != VK_SUCCESS || state->staticPaintDescriptorSetLayout == VK_NULL_DESCRIPTOR_SET_LAYOUT)
    {
        iError(reporter, "Vulkan renderer failed to create static paint descriptor layout");
        state->staticPaintDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &state->staticPaintDescriptorSetLayout;
    result = state->vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, nullptr, &state->staticPaintPipelineLayout);
    if (result != VK_SUCCESS || state->staticPaintPipelineLayout == VK_NULL_PIPELINE_LAYOUT)
    {
        iError(reporter, "Vulkan renderer failed to create static paint pipeline layout");
        if (state->vkDestroyDescriptorSetLayout)
        {
            state->vkDestroyDescriptorSetLayout(state->device, state->staticPaintDescriptorSetLayout, nullptr);
        }
        state->staticPaintDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        state->staticPaintPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        return false;
    }

    if (!state->descriptorLayoutReported)
    {
        iReport(reporter, "Vulkan renderer created static paint descriptor and pipeline layouts");
        state->descriptorLayoutReported = true;
    }

    VkDescriptorPoolSize poolSizes[3] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 5 * kBorrowedStaticPaintSetCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = kBorrowedStaticPaintSetCount;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = kBorrowedStaticPaintSetCount;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kBorrowedStaticPaintSetCount;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    result = state->vkCreateDescriptorPool(state->device, &poolInfo, nullptr, &state->staticPaintDescriptorPool);
    if (result != VK_SUCCESS || state->staticPaintDescriptorPool == VK_NULL_DESCRIPTOR_POOL)
    {
        iError(reporter, "Vulkan renderer failed to create static paint descriptor pool");
        state->vkDestroyPipelineLayout(state->device, state->staticPaintPipelineLayout, nullptr);
        state->vkDestroyDescriptorSetLayout(state->device, state->staticPaintDescriptorSetLayout, nullptr);
        state->staticPaintPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        state->staticPaintDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        state->staticPaintDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
        return false;
    }

    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = state->staticPaintDescriptorPool;
    VkDescriptorSetLayout staticPaintLayouts[kBorrowedStaticPaintSetCount] = {};
    for (uint32_t i = 0; i < kBorrowedStaticPaintSetCount; ++i)
    {
        staticPaintLayouts[i] = state->staticPaintDescriptorSetLayout;
    }
    allocateInfo.descriptorSetCount = kBorrowedStaticPaintSetCount;
    allocateInfo.pSetLayouts = staticPaintLayouts;
    result = state->vkAllocateDescriptorSets(
        state->device,
        &allocateInfo,
        state->staticPaintDescriptorSets);
    state->staticPaintDescriptorSet = state->staticPaintDescriptorSets[0];
    if (result != VK_SUCCESS || state->staticPaintDescriptorSet == VK_NULL_DESCRIPTOR_SET)
    {
        iError(reporter, "Vulkan renderer failed to allocate static paint descriptor set");
        if (state->vkDestroyDescriptorPool)
        {
            state->vkDestroyDescriptorPool(state->device, state->staticPaintDescriptorPool, nullptr);
        }
        state->vkDestroyPipelineLayout(state->device, state->staticPaintPipelineLayout, nullptr);
        state->vkDestroyDescriptorSetLayout(state->device, state->staticPaintDescriptorSetLayout, nullptr);
        state->staticPaintDescriptorSet = VK_NULL_DESCRIPTOR_SET;
        state->staticPaintDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
        state->staticPaintPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        state->staticPaintDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        return false;
    }
    return true;
}

static bool iUpdateStaticPaintDescriptorSet(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->staticPaintDescriptorSet == VK_NULL_DESCRIPTOR_SET || !state->vkUpdateDescriptorSets)
    {
        return true;
    }
    piTexture blueNoise = state->textures[7];
    piBuffer frameBuffer = state->constantBuffers[0];
    piBuffer layerBuffer = state->constantBuffers[3];
    piBuffer displayBuffer = state->constantBuffers[4];
    piBuffer passBuffer = state->constantBuffers[5];
    piBuffer vertexData = state->shaderBuffers[8];
    piBuffer chunkBuffer = state->constantBuffers[9];
    if (!blueNoise || blueNoise->imageView == VK_NULL_IMAGE_VIEW || blueNoise->sampler == VK_NULL_SAMPLER ||
        !frameBuffer || frameBuffer->buffer == VK_NULL_BUFFER ||
        !layerBuffer || layerBuffer->buffer == VK_NULL_BUFFER ||
        !displayBuffer || displayBuffer->buffer == VK_NULL_BUFFER ||
        !passBuffer || passBuffer->buffer == VK_NULL_BUFFER ||
        !vertexData || vertexData->buffer == VK_NULL_BUFFER ||
        !chunkBuffer || chunkBuffer->buffer == VK_NULL_BUFFER)
    {
        return false;
    }
    if (state->borrowedFrameResourcesActive)
    {
        if (state->borrowedStaticPaintSetCursor >= kBorrowedStaticPaintSetsPerFrame)
        {
            iError(reporter, "Vulkan renderer exhausted borrowed static paint descriptor sets");
            return false;
        }
        const uint32_t descriptorIndex =
            state->borrowedFrameSlot * kBorrowedStaticPaintSetsPerFrame +
            state->borrowedStaticPaintSetCursor++;
        state->staticPaintDescriptorSet = state->staticPaintDescriptorSets[descriptorIndex];
    }
    else
    {
        state->staticPaintDescriptorSet = state->staticPaintDescriptorSets[0];
    }

    VkDescriptorBufferInfo bufferInfos[6] = {};
    bufferInfos[0] = iDescriptorBufferInfo(frameBuffer);
    bufferInfos[1] = iDescriptorBufferInfo(layerBuffer);
    bufferInfos[2] = iDescriptorBufferInfo(displayBuffer);
    bufferInfos[3] = iDescriptorBufferInfo(passBuffer);
    bufferInfos[4] = iDescriptorBufferInfo(vertexData);
    bufferInfos[5] = iDescriptorBufferInfo(chunkBuffer);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = blueNoise->sampler;
    imageInfo.imageView = blueNoise->imageView;
    imageInfo.imageLayout = blueNoise->imageLayout;

    VkWriteDescriptorSet writes[7] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = state->staticPaintDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &bufferInfos[0];
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = state->staticPaintDescriptorSet;
    writes[1].dstBinding = 3;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &bufferInfos[1];
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = state->staticPaintDescriptorSet;
    writes[2].dstBinding = 4;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &bufferInfos[2];
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = state->staticPaintDescriptorSet;
    writes[3].dstBinding = 5;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &bufferInfos[3];
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = state->staticPaintDescriptorSet;
    writes[4].dstBinding = 7;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].pImageInfo = &imageInfo;
    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = state->staticPaintDescriptorSet;
    writes[5].dstBinding = 8;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[5].pBufferInfo = &bufferInfos[4];
    writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet = state->staticPaintDescriptorSet;
    writes[6].dstBinding = 9;
    writes[6].descriptorCount = 1;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[6].pBufferInfo = &bufferInfos[5];
    state->vkUpdateDescriptorSets(state->device, 7, writes, 0, nullptr);

    if (!state->descriptorSetReported)
    {
        iReport(reporter, "Vulkan renderer updated static paint descriptor set");
        state->descriptorSetReported = true;
    }
    return true;
}

static bool iEnsurePicturePipelineLayout(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->device == VK_NULL_DEVICE)
    {
        return true;
    }
    if (state->picturePipelineLayout != VK_NULL_PIPELINE_LAYOUT)
    {
        return true;
    }
    if (!state->vkCreateDescriptorSetLayout || !state->vkCreatePipelineLayout ||
        !state->vkCreateDescriptorPool || !state->vkAllocateDescriptorSets)
    {
        return true;
    }

    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 3;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding = 4;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[3].binding = 5;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = 4;
    setLayoutInfo.pBindings = bindings;
    VkResult result = state->vkCreateDescriptorSetLayout(state->device, &setLayoutInfo, nullptr, &state->pictureDescriptorSetLayout);
    if (result != VK_SUCCESS || state->pictureDescriptorSetLayout == VK_NULL_DESCRIPTOR_SET_LAYOUT)
    {
        iError(reporter, "Vulkan renderer failed to create picture descriptor layout");
        state->pictureDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &state->pictureDescriptorSetLayout;
    result = state->vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, nullptr, &state->picturePipelineLayout);
    if (result != VK_SUCCESS || state->picturePipelineLayout == VK_NULL_PIPELINE_LAYOUT)
    {
        iError(reporter, "Vulkan renderer failed to create picture pipeline layout");
        state->vkDestroyDescriptorSetLayout(state->device, state->pictureDescriptorSetLayout, nullptr);
        state->pictureDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        state->picturePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        return false;
    }

    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = kBorrowedPictureSetCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 3 * kBorrowedPictureSetCount;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kBorrowedPictureSetCount;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    result = state->vkCreateDescriptorPool(state->device, &poolInfo, nullptr, &state->pictureDescriptorPool);
    if (result != VK_SUCCESS || state->pictureDescriptorPool == VK_NULL_DESCRIPTOR_POOL)
    {
        iError(reporter, "Vulkan renderer failed to create picture descriptor pool");
        state->vkDestroyPipelineLayout(state->device, state->picturePipelineLayout, nullptr);
        state->vkDestroyDescriptorSetLayout(state->device, state->pictureDescriptorSetLayout, nullptr);
        state->picturePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        state->pictureDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        state->pictureDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
        return false;
    }

    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = state->pictureDescriptorPool;
    VkDescriptorSetLayout pictureLayouts[kBorrowedPictureSetCount] = {};
    for (uint32_t i = 0; i < kBorrowedPictureSetCount; ++i)
    {
        pictureLayouts[i] = state->pictureDescriptorSetLayout;
    }
    allocateInfo.descriptorSetCount = kBorrowedPictureSetCount;
    allocateInfo.pSetLayouts = pictureLayouts;
    result = state->vkAllocateDescriptorSets(
        state->device,
        &allocateInfo,
        state->pictureDescriptorSets);
    state->pictureDescriptorSet = state->pictureDescriptorSets[0];
    if (result != VK_SUCCESS || state->pictureDescriptorSet == VK_NULL_DESCRIPTOR_SET)
    {
        iError(reporter, "Vulkan renderer failed to allocate picture descriptor set");
        state->vkDestroyDescriptorPool(state->device, state->pictureDescriptorPool, nullptr);
        state->vkDestroyPipelineLayout(state->device, state->picturePipelineLayout, nullptr);
        state->vkDestroyDescriptorSetLayout(state->device, state->pictureDescriptorSetLayout, nullptr);
        state->pictureDescriptorSet = VK_NULL_DESCRIPTOR_SET;
        state->pictureDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
        state->picturePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        state->pictureDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        return false;
    }
    if (!state->pictureLayoutReported)
    {
        iReport(reporter, "Vulkan renderer created picture descriptor and pipeline layouts");
        state->pictureLayoutReported = true;
    }
    return true;
}

static bool iUpdatePictureDescriptorSet(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->pictureDescriptorSet == VK_NULL_DESCRIPTOR_SET || !state->vkUpdateDescriptorSets)
    {
        return true;
    }
    piTexture picture = state->textures[0];
    piBuffer layerBuffer = state->constantBuffers[3];
    piBuffer displayBuffer = state->constantBuffers[4];
    piBuffer passBuffer = state->constantBuffers[5];
    if (!picture || picture->imageView == VK_NULL_IMAGE_VIEW || picture->sampler == VK_NULL_SAMPLER ||
        !layerBuffer || layerBuffer->buffer == VK_NULL_BUFFER ||
        !displayBuffer || displayBuffer->buffer == VK_NULL_BUFFER ||
        !passBuffer || passBuffer->buffer == VK_NULL_BUFFER)
    {
        return false;
    }
    if (state->borrowedFrameResourcesActive)
    {
        if (state->borrowedPictureSetCursor >= kBorrowedPictureSetsPerFrame)
        {
            iError(reporter, "Vulkan renderer exhausted borrowed picture descriptor sets");
            return false;
        }
        const uint32_t descriptorIndex =
            state->borrowedFrameSlot * kBorrowedPictureSetsPerFrame +
            state->borrowedPictureSetCursor++;
        state->pictureDescriptorSet = state->pictureDescriptorSets[descriptorIndex];
    }
    else
    {
        state->pictureDescriptorSet = state->pictureDescriptorSets[0];
    }

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = picture->sampler;
    imageInfo.imageView = picture->imageView;
    imageInfo.imageLayout = picture->imageLayout;

    VkDescriptorBufferInfo bufferInfos[3] = {};
    bufferInfos[0] = iDescriptorBufferInfo(layerBuffer);
    bufferInfos[1] = iDescriptorBufferInfo(displayBuffer);
    bufferInfos[2] = iDescriptorBufferInfo(passBuffer);

    VkWriteDescriptorSet writes[4] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = state->pictureDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &imageInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = state->pictureDescriptorSet;
    writes[1].dstBinding = 3;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &bufferInfos[0];
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = state->pictureDescriptorSet;
    writes[2].dstBinding = 4;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &bufferInfos[1];
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = state->pictureDescriptorSet;
    writes[3].dstBinding = 5;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &bufferInfos[2];
    state->vkUpdateDescriptorSets(state->device, 4, writes, 0, nullptr);

    if (!state->pictureDescriptorReported)
    {
        iReport(reporter, "Vulkan renderer updated picture descriptor set");
        state->pictureDescriptorReported = true;
    }
    return true;
}

static const uint32_t kSrgbPresentVS[] = {
    0x07230203u, 0x00010000u, 0x000d000bu, 0x00000034u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0008000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000017u, 0x0000001du, 0x00000028u,
    0x00040047u, 0x00000017u, 0x0000000bu, 0x0000002au, 0x00040047u, 0x0000001du, 0x0000001eu, 0x00000000u,
    0x00030047u, 0x00000026u, 0x00000002u, 0x00050048u, 0x00000026u, 0x00000000u, 0x0000000bu, 0x00000000u,
    0x00050048u, 0x00000026u, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x00000026u, 0x00000002u,
    0x0000000bu, 0x00000003u, 0x00050048u, 0x00000026u, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00020013u,
    0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u,
    0x00000007u, 0x00000006u, 0x00000002u, 0x00040015u, 0x00000008u, 0x00000020u, 0x00000000u, 0x0004002bu,
    0x00000008u, 0x00000009u, 0x00000003u, 0x0004001cu, 0x0000000au, 0x00000007u, 0x00000009u, 0x0004002bu,
    0x00000006u, 0x0000000du, 0xbf800000u, 0x0005002cu, 0x00000007u, 0x0000000eu, 0x0000000du, 0x0000000du,
    0x0004002bu, 0x00000006u, 0x0000000fu, 0x40400000u, 0x0005002cu, 0x00000007u, 0x00000010u, 0x0000000fu,
    0x0000000du, 0x0005002cu, 0x00000007u, 0x00000011u, 0x0000000du, 0x0000000fu, 0x0006002cu, 0x0000000au,
    0x00000012u, 0x0000000eu, 0x00000010u, 0x00000011u, 0x00040020u, 0x00000013u, 0x00000007u, 0x00000007u,
    0x00040015u, 0x00000015u, 0x00000020u, 0x00000001u, 0x00040020u, 0x00000016u, 0x00000001u, 0x00000015u,
    0x0004003bu, 0x00000016u, 0x00000017u, 0x00000001u, 0x00040020u, 0x0000001cu, 0x00000003u, 0x00000007u,
    0x0004003bu, 0x0000001cu, 0x0000001du, 0x00000003u, 0x0004002bu, 0x00000006u, 0x0000001fu, 0x3f000000u,
    0x00040017u, 0x00000023u, 0x00000006u, 0x00000004u, 0x0004002bu, 0x00000008u, 0x00000024u, 0x00000001u,
    0x0004001cu, 0x00000025u, 0x00000006u, 0x00000024u, 0x0006001eu, 0x00000026u, 0x00000023u, 0x00000006u,
    0x00000025u, 0x00000025u, 0x00040020u, 0x00000027u, 0x00000003u, 0x00000026u, 0x0004003bu, 0x00000027u,
    0x00000028u, 0x00000003u, 0x0004002bu, 0x00000015u, 0x00000029u, 0x00000000u, 0x0004002bu, 0x00000006u,
    0x0000002bu, 0x00000000u, 0x0004002bu, 0x00000006u, 0x0000002cu, 0x3f800000u, 0x00040020u, 0x00000030u,
    0x00000003u, 0x00000023u, 0x00040020u, 0x00000032u, 0x00000007u, 0x0000000au, 0x0005002cu, 0x00000007u,
    0x00000033u, 0x0000001fu, 0x0000001fu, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u,
    0x000200f8u, 0x00000005u, 0x0004003bu, 0x00000032u, 0x0000000cu, 0x00000007u, 0x0003003eu, 0x0000000cu,
    0x00000012u, 0x0004003du, 0x00000015u, 0x00000018u, 0x00000017u, 0x00050041u, 0x00000013u, 0x0000001au,
    0x0000000cu, 0x00000018u, 0x0004003du, 0x00000007u, 0x0000001bu, 0x0000001au, 0x0005008eu, 0x00000007u,
    0x00000020u, 0x0000001bu, 0x0000001fu, 0x00050081u, 0x00000007u, 0x00000022u, 0x00000020u, 0x00000033u,
    0x0003003eu, 0x0000001du, 0x00000022u, 0x00050051u, 0x00000006u, 0x0000002du, 0x0000001bu, 0x00000000u,
    0x00050051u, 0x00000006u, 0x0000002eu, 0x0000001bu, 0x00000001u, 0x00070050u, 0x00000023u, 0x0000002fu,
    0x0000002du, 0x0000002eu, 0x0000002bu, 0x0000002cu, 0x00050041u, 0x00000030u, 0x00000031u, 0x00000028u,
    0x00000029u, 0x0003003eu, 0x00000031u, 0x0000002fu, 0x000100fdu, 0x00010038u
};

static const uint32_t kSrgbPresentFS[] = {
    0x07230203u, 0x00010000u, 0x000d000bu, 0x0000005au, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0007000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000037u, 0x0000003du, 0x00030010u,
    0x00000004u, 0x00000007u, 0x00040047u, 0x00000033u, 0x00000021u, 0x00000000u, 0x00040047u, 0x00000033u,
    0x00000022u, 0x00000000u, 0x00040047u, 0x00000037u, 0x0000001eu, 0x00000000u, 0x00040047u, 0x0000003du,
    0x0000001eu, 0x00000000u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u,
    0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000003u, 0x00020014u, 0x0000000du,
    0x00040017u, 0x0000000eu, 0x0000000du, 0x00000003u, 0x0004002bu, 0x00000006u, 0x00000012u, 0x3b4d2e1cu,
    0x0006002cu, 0x00000007u, 0x00000013u, 0x00000012u, 0x00000012u, 0x00000012u, 0x0004002bu, 0x00000006u,
    0x00000017u, 0x414eb852u, 0x0004002bu, 0x00000006u, 0x0000001au, 0x3f870a3du, 0x0004002bu, 0x00000006u,
    0x0000001cu, 0x00000000u, 0x0006002cu, 0x00000007u, 0x0000001du, 0x0000001cu, 0x0000001cu, 0x0000001cu,
    0x0004002bu, 0x00000006u, 0x0000001fu, 0x3ed55555u, 0x0006002cu, 0x00000007u, 0x00000020u, 0x0000001fu,
    0x0000001fu, 0x0000001fu, 0x0004002bu, 0x00000006u, 0x00000023u, 0x3d6147aeu, 0x0004002bu, 0x00000006u,
    0x00000029u, 0x3f800000u, 0x0006002cu, 0x00000007u, 0x0000002au, 0x00000029u, 0x00000029u, 0x00000029u,
    0x00090019u, 0x00000030u, 0x00000006u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u,
    0x00000000u, 0x0003001bu, 0x00000031u, 0x00000030u, 0x00040020u, 0x00000032u, 0x00000000u, 0x00000031u,
    0x0004003bu, 0x00000032u, 0x00000033u, 0x00000000u, 0x00040017u, 0x00000035u, 0x00000006u, 0x00000002u,
    0x00040020u, 0x00000036u, 0x00000001u, 0x00000035u, 0x0004003bu, 0x00000036u, 0x00000037u, 0x00000001u,
    0x00040017u, 0x00000039u, 0x00000006u, 0x00000004u, 0x00040020u, 0x0000003cu, 0x00000003u, 0x00000039u,
    0x0004003bu, 0x0000003cu, 0x0000003du, 0x00000003u, 0x0006002cu, 0x00000007u, 0x00000059u, 0x00000023u,
    0x00000023u, 0x00000023u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
    0x00000005u, 0x0004003du, 0x00000031u, 0x00000034u, 0x00000033u, 0x0004003du, 0x00000035u, 0x00000038u,
    0x00000037u, 0x00050057u, 0x00000039u, 0x0000003au, 0x00000034u, 0x00000038u, 0x0008004fu, 0x00000007u,
    0x0000003bu, 0x0000003au, 0x0000003au, 0x00000000u, 0x00000001u, 0x00000002u, 0x000500b8u, 0x0000000eu,
    0x0000004bu, 0x0000003bu, 0x00000013u, 0x0005008eu, 0x00000007u, 0x0000004du, 0x0000003bu, 0x00000017u,
    0x0007000cu, 0x00000007u, 0x0000004fu, 0x00000001u, 0x00000028u, 0x0000003bu, 0x0000001du, 0x0007000cu,
    0x00000007u, 0x00000050u, 0x00000001u, 0x0000001au, 0x0000004fu, 0x00000020u, 0x0005008eu, 0x00000007u,
    0x00000051u, 0x00000050u, 0x0000001au, 0x00050083u, 0x00000007u, 0x00000053u, 0x00000051u, 0x00000059u,
    0x000600a9u, 0x00000007u, 0x00000057u, 0x0000004bu, 0x0000002au, 0x0000001du, 0x0008000cu, 0x00000007u,
    0x00000058u, 0x00000001u, 0x0000002eu, 0x00000053u, 0x0000004du, 0x00000057u, 0x00050051u, 0x00000006u,
    0x00000041u, 0x00000058u, 0x00000000u, 0x00050051u, 0x00000006u, 0x00000042u, 0x00000058u, 0x00000001u,
    0x00050051u, 0x00000006u, 0x00000043u, 0x00000058u, 0x00000002u, 0x00070050u, 0x00000039u, 0x00000044u,
    0x00000041u, 0x00000042u, 0x00000043u, 0x00000029u, 0x0003003eu, 0x0000003du, 0x00000044u, 0x000100fdu,
    0x00010038u
};

#include "piVulkan_HostDiagnosticShaders.inc"
#include "piVulkan_HostCenterDiagnosticShader.inc"
#include "piVulkan_HostVertexDescriptorDiagnosticShader.inc"

// Constant cyan fragment shader for isolating Unity host-render-pass raster
// integration. It intentionally has no descriptors, inputs, or uniforms.
static const uint32_t kHostDebugTriangleFS[] = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x0000000du, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0006000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x00030010u, 0x00000004u,
    0x00000007u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
    0x00050005u, 0x00000009u, 0x4374756fu, 0x726f6c6fu, 0x00000000u, 0x00040047u, 0x00000009u, 0x0000001eu,
    0x00000000u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u,
    0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u, 0x00000008u, 0x00000003u,
    0x00000007u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000003u, 0x0004002bu, 0x00000006u, 0x0000000au,
    0x00000000u, 0x0004002bu, 0x00000006u, 0x0000000bu, 0x3f800000u, 0x0007002cu, 0x00000007u, 0x0000000cu,
    0x0000000au, 0x0000000bu, 0x0000000bu, 0x0000000bu, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u,
    0x00000003u, 0x000200f8u, 0x00000005u, 0x0003003eu, 0x00000009u, 0x0000000cu, 0x000100fdu, 0x00010038u,
};

// Descriptor-free vertex shader for proving that the real IMM index buffer is
// consumed by Unity's live command buffer. It maps each fetched index modulo
// three onto a fixed triangle, preserving indexed submission and restart data.
static const uint32_t kHostIndexedControlVS[] = {
    0x07230203u, 0x00010000u, 0x000d000bu, 0x0000002au, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0007000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000du, 0x0000001bu, 0x00030047u,
    0x0000000bu, 0x00000002u, 0x00050048u, 0x0000000bu, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u,
    0x0000000bu, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000bu, 0x00000002u, 0x0000000bu,
    0x00000003u, 0x00050048u, 0x0000000bu, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00040047u, 0x0000001bu,
    0x0000000bu, 0x0000002au, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u,
    0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040015u, 0x00000008u,
    0x00000020u, 0x00000000u, 0x0004002bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x0004001cu, 0x0000000au,
    0x00000006u, 0x00000009u, 0x0006001eu, 0x0000000bu, 0x00000007u, 0x00000006u, 0x0000000au, 0x0000000au,
    0x00040020u, 0x0000000cu, 0x00000003u, 0x0000000bu, 0x0004003bu, 0x0000000cu, 0x0000000du, 0x00000003u,
    0x00040015u, 0x0000000eu, 0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000eu, 0x0000000fu, 0x00000000u,
    0x00040017u, 0x00000010u, 0x00000006u, 0x00000002u, 0x0004002bu, 0x00000008u, 0x00000011u, 0x00000003u,
    0x0004001cu, 0x00000012u, 0x00000010u, 0x00000011u, 0x0004002bu, 0x00000006u, 0x00000013u, 0xbf400000u,
    0x0005002cu, 0x00000010u, 0x00000014u, 0x00000013u, 0x00000013u, 0x0004002bu, 0x00000006u, 0x00000015u,
    0x3f400000u, 0x0005002cu, 0x00000010u, 0x00000016u, 0x00000015u, 0x00000013u, 0x0004002bu, 0x00000006u,
    0x00000017u, 0x00000000u, 0x0005002cu, 0x00000010u, 0x00000018u, 0x00000017u, 0x00000015u, 0x0006002cu,
    0x00000012u, 0x00000019u, 0x00000014u, 0x00000016u, 0x00000018u, 0x00040020u, 0x0000001au, 0x00000001u,
    0x0000000eu, 0x0004003bu, 0x0000001au, 0x0000001bu, 0x00000001u, 0x00040020u, 0x0000001fu, 0x00000007u,
    0x00000012u, 0x00040020u, 0x00000021u, 0x00000007u, 0x00000010u, 0x0004002bu, 0x00000006u, 0x00000024u,
    0x3f800000u, 0x00040020u, 0x00000028u, 0x00000003u, 0x00000007u, 0x00050036u, 0x00000002u, 0x00000004u,
    0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003bu, 0x0000001fu, 0x00000020u, 0x00000007u,
    0x0004003du, 0x0000000eu, 0x0000001cu, 0x0000001bu, 0x0004007cu, 0x00000008u, 0x0000001du, 0x0000001cu,
    0x00050089u, 0x00000008u, 0x0000001eu, 0x0000001du, 0x00000011u, 0x0003003eu, 0x00000020u, 0x00000019u,
    0x00050041u, 0x00000021u, 0x00000022u, 0x00000020u, 0x0000001eu, 0x0004003du, 0x00000010u, 0x00000023u,
    0x00000022u, 0x00050051u, 0x00000006u, 0x00000025u, 0x00000023u, 0x00000000u, 0x00050051u, 0x00000006u,
    0x00000026u, 0x00000023u, 0x00000001u, 0x00070050u, 0x00000007u, 0x00000027u, 0x00000025u, 0x00000026u,
    0x00000017u, 0x00000024u, 0x00050041u, 0x00000028u, 0x00000029u, 0x0000000du, 0x0000000fu, 0x0003003eu,
    0x00000029u, 0x00000027u, 0x000100fdu, 0x00010038u,
};

// Brush-2 index strips alternate vertices from adjacent seven-vertex rings.
// Fold both the ring and within-ring index into the fixed triangle corner so
// consecutive fetched indices cannot collapse to repeated corners.
static const uint32_t kHostIndexedStripControlVS[] = {
    0x07230203u, 0x00010000u, 0x000d000bu, 0x00000033u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0007000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000bu, 0x0000001cu, 0x00040047u,
    0x0000000bu, 0x0000000bu, 0x0000002au, 0x00030047u, 0x0000001au, 0x00000002u, 0x00050048u, 0x0000001au,
    0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x0000001au, 0x00000001u, 0x0000000bu, 0x00000001u,
    0x00050048u, 0x0000001au, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x0000001au, 0x00000003u,
    0x0000000bu, 0x00000004u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00040015u,
    0x00000006u, 0x00000020u, 0x00000000u, 0x00040015u, 0x00000009u, 0x00000020u, 0x00000001u, 0x00040020u,
    0x0000000au, 0x00000001u, 0x00000009u, 0x0004003bu, 0x0000000au, 0x0000000bu, 0x00000001u, 0x0004002bu,
    0x00000006u, 0x00000011u, 0x00000007u, 0x0004002bu, 0x00000006u, 0x00000014u, 0x00000003u, 0x00030016u,
    0x00000016u, 0x00000020u, 0x00040017u, 0x00000017u, 0x00000016u, 0x00000004u, 0x0004002bu, 0x00000006u,
    0x00000018u, 0x00000001u, 0x0004001cu, 0x00000019u, 0x00000016u, 0x00000018u, 0x0006001eu, 0x0000001au,
    0x00000017u, 0x00000016u, 0x00000019u, 0x00000019u, 0x00040020u, 0x0000001bu, 0x00000003u, 0x0000001au,
    0x0004003bu, 0x0000001bu, 0x0000001cu, 0x00000003u, 0x0004002bu, 0x00000009u, 0x0000001du, 0x00000000u,
    0x00040017u, 0x0000001eu, 0x00000016u, 0x00000002u, 0x0004001cu, 0x0000001fu, 0x0000001eu, 0x00000014u,
    0x0004002bu, 0x00000016u, 0x00000020u, 0xbf400000u, 0x0005002cu, 0x0000001eu, 0x00000021u, 0x00000020u,
    0x00000020u, 0x0004002bu, 0x00000016u, 0x00000022u, 0x3f400000u, 0x0005002cu, 0x0000001eu, 0x00000023u,
    0x00000022u, 0x00000020u, 0x0004002bu, 0x00000016u, 0x00000024u, 0x00000000u, 0x0005002cu, 0x0000001eu,
    0x00000025u, 0x00000024u, 0x00000022u, 0x0006002cu, 0x0000001fu, 0x00000026u, 0x00000021u, 0x00000023u,
    0x00000025u, 0x00040020u, 0x00000028u, 0x00000007u, 0x0000001fu, 0x00040020u, 0x0000002au, 0x00000007u,
    0x0000001eu, 0x0004002bu, 0x00000016u, 0x0000002du, 0x3f800000u, 0x00040020u, 0x00000031u, 0x00000003u,
    0x00000017u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u,
    0x0004003bu, 0x00000028u, 0x00000029u, 0x00000007u, 0x0004003du, 0x00000009u, 0x0000000cu, 0x0000000bu,
    0x0004007cu, 0x00000006u, 0x0000000du, 0x0000000cu, 0x00050086u, 0x00000006u, 0x00000012u, 0x0000000du,
    0x00000011u, 0x00050080u, 0x00000006u, 0x00000013u, 0x0000000du, 0x00000012u, 0x00050089u, 0x00000006u,
    0x00000015u, 0x00000013u, 0x00000014u, 0x0003003eu, 0x00000029u, 0x00000026u, 0x00050041u, 0x0000002au,
    0x0000002bu, 0x00000029u, 0x00000015u, 0x0004003du, 0x0000001eu, 0x0000002cu, 0x0000002bu, 0x00050051u,
    0x00000016u, 0x0000002eu, 0x0000002cu, 0x00000000u, 0x00050051u, 0x00000016u, 0x0000002fu, 0x0000002cu,
    0x00000001u, 0x00070050u, 0x00000017u, 0x00000030u, 0x0000002eu, 0x0000002fu, 0x00000024u, 0x0000002du,
    0x00050041u, 0x00000031u, 0x00000032u, 0x0000001cu, 0x0000001du, 0x0003003eu, 0x00000032u, 0x00000030u,
    0x000100fdu, 0x00010038u,
};

static bool iEnsureSrgbPresentResources(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->device == VK_NULL_DEVICE)
    {
        return false;
    }
    if (state->presentPipelineLayout != VK_NULL_PIPELINE_LAYOUT &&
        state->presentDescriptorSet != VK_NULL_DESCRIPTOR_SET &&
        state->presentVertexModule != VK_NULL_SHADER_MODULE &&
        state->presentFragmentModule != VK_NULL_SHADER_MODULE &&
        state->presentSampler != VK_NULL_SAMPLER)
    {
        return true;
    }
    if (!state->vkCreateDescriptorSetLayout || !state->vkCreatePipelineLayout ||
        !state->vkCreateDescriptorPool || !state->vkAllocateDescriptorSets || !state->vkCreateSampler)
    {
        return false;
    }

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = 1;
    setLayoutInfo.pBindings = &binding;
    VkResult result = state->vkCreateDescriptorSetLayout(state->device, &setLayoutInfo, nullptr, &state->presentDescriptorSetLayout);
    if (result != VK_SUCCESS || state->presentDescriptorSetLayout == VK_NULL_DESCRIPTOR_SET_LAYOUT)
    {
        iError(reporter, "Vulkan renderer failed to create sRGB present descriptor layout");
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &state->presentDescriptorSetLayout;
    result = state->vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, nullptr, &state->presentPipelineLayout);
    if (result != VK_SUCCESS || state->presentPipelineLayout == VK_NULL_PIPELINE_LAYOUT)
    {
        iError(reporter, "Vulkan renderer failed to create sRGB present pipeline layout");
        return false;
    }

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    result = state->vkCreateDescriptorPool(state->device, &poolInfo, nullptr, &state->presentDescriptorPool);
    if (result != VK_SUCCESS || state->presentDescriptorPool == VK_NULL_DESCRIPTOR_POOL)
    {
        iError(reporter, "Vulkan renderer failed to create sRGB present descriptor pool");
        return false;
    }

    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = state->presentDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &state->presentDescriptorSetLayout;
    result = state->vkAllocateDescriptorSets(state->device, &allocateInfo, &state->presentDescriptorSet);
    if (result != VK_SUCCESS || state->presentDescriptorSet == VK_NULL_DESCRIPTOR_SET)
    {
        iError(reporter, "Vulkan renderer failed to allocate sRGB present descriptor set");
        return false;
    }

    if (!iCreateSamplerObject(state, piRenderer::TextureFilter::LINEAR, piRenderer::TextureWrap::CLAMP, 1.0f, &state->presentSampler, reporter) ||
        !iCreateShaderModule(state, reinterpret_cast<const uint8_t *>(kSrgbPresentVS), (int)sizeof(kSrgbPresentVS), &state->presentVertexModule, reporter) ||
        !iCreateShaderModule(state, reinterpret_cast<const uint8_t *>(kSrgbPresentFS), (int)sizeof(kSrgbPresentFS), &state->presentFragmentModule, reporter))
    {
        iError(reporter, "Vulkan renderer failed to create sRGB present shader resources");
        return false;
    }

    if (!state->presentLayoutReported)
    {
        iReport(reporter, "Vulkan renderer created sRGB present descriptor and pipeline layouts");
        state->presentLayoutReported = true;
    }
    return true;
}

static bool iUpdateSrgbPresentDescriptorSet(piVulkanState *state, piTexture source, piRenderer::piReporter *reporter)
{
    if (!state || !source || state->presentDescriptorSet == VK_NULL_DESCRIPTOR_SET || !state->vkUpdateDescriptorSets ||
        source->imageView == VK_NULL_IMAGE_VIEW || state->presentSampler == VK_NULL_SAMPLER)
    {
        return false;
    }

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = state->presentSampler;
    imageInfo.imageView = source->imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = state->presentDescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    state->vkUpdateDescriptorSets(state->device, 1, &write, 0, nullptr);

    if (!state->presentDescriptorReported)
    {
        iReport(reporter, "Vulkan renderer updated sRGB present descriptor set");
        state->presentDescriptorReported = true;
    }
    return true;
}

static bool iEnsureSrgbPresentPipeline(piVulkanState *state, piRenderer::piReporter *reporter)
{
    if (!state || state->device == VK_NULL_DEVICE || state->swapchainRenderPass == VK_NULL_RENDER_PASS)
    {
        return false;
    }
    if (state->presentPipeline != VK_NULL_PIPELINE)
    {
        return true;
    }
    if (!iEnsureSrgbPresentResources(state, reporter) || !state->vkCreateGraphicsPipelines)
    {
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = state->presentVertexModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = state->presentFragmentModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = state->presentPipelineLayout;
    pipelineInfo.renderPass = state->swapchainRenderPass;
    pipelineInfo.subpass = 0;

    const VkResult result = state->vkCreateGraphicsPipelines(state->device, VK_NULL_PIPELINE_CACHE, 1, &pipelineInfo, nullptr, &state->presentPipeline);
    if (result != VK_SUCCESS || state->presentPipeline == VK_NULL_PIPELINE)
    {
        iError(reporter, "Vulkan renderer failed to create sRGB present graphics pipeline");
        state->presentPipeline = VK_NULL_PIPELINE;
        return false;
    }

    if (!state->presentPipelineReported)
    {
        iReport(reporter, "Vulkan renderer created sRGB present graphics pipeline");
        state->presentPipelineReported = true;
    }
    return true;
}

static VkSampleCountFlagBits iRequestedTextureSampleCount(const piRenderer::TextureInfo &info);
static bool iEnsureStagingBuffer(piVulkanState *state, VkDeviceSize size, piRenderer::piReporter *reporter);

static VkCullModeFlags iToVulkanCullMode(piRenderer::CullMode mode)
{
    switch (mode)
    {
        case piRenderer::CullMode::FRONT: return VK_CULL_MODE_FRONT_BIT;
        case piRenderer::CullMode::BACK: return VK_CULL_MODE_BACK_BIT;
        case piRenderer::CullMode::NONE:
        default: return VK_CULL_MODE_NONE;
    }
}

static void iRetireGraphicsPipeline(piVulkanState *state, VkPipeline pipeline)
{
    if (!state || pipeline == VK_NULL_PIPELINE || !state->vkDestroyPipeline)
    {
        return;
    }
    if (state->borrowedFrameResourcesActive)
    {
        const piVulkanBorrowedPipeline borrowedPipeline = {
            pipeline,
            state->borrowedCurrentFrameNumber
        };
        state->borrowedPipelines.push_back(borrowedPipeline);
        static uint32_t retireReportCount = 0;
        if (retireReportCount < 16)
        {
            std::fprintf(
                stderr,
                "[IMM_UNITY_VK_DEFER_PIPELINE_20260731] pipeline=0x%llx frame=%llu\n",
                static_cast<unsigned long long>(pipeline),
                static_cast<unsigned long long>(state->borrowedCurrentFrameNumber));
            ++retireReportCount;
        }
        return;
    }
    state->vkDestroyPipeline(state->device, pipeline, nullptr);
}

static bool iDrawHostDescriptorDiagnostic(piVulkanState *state, piRTarget target, piRenderer::piReporter *reporter)
{
    if (!state || !target || !state->hostRenderPassFrameActive ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        state->staticPaintPipelineLayout == VK_NULL_PIPELINE_LAYOUT ||
        state->staticPaintDescriptorSet == VK_NULL_DESCRIPTOR_SET)
    {
        return false;
    }
    if (state->hostDebugTriangleVertexModule == VK_NULL_SHADER_MODULE &&
        !iCreateShaderModule(state,
                             reinterpret_cast<const uint8_t *>(kSrgbPresentVS),
                             static_cast<int>(sizeof(kSrgbPresentVS)),
                             &state->hostDebugTriangleVertexModule,
                             reporter))
    {
        return false;
    }
    if (state->hostDescriptorDiagnosticFragmentModule == VK_NULL_SHADER_MODULE &&
        !iCreateShaderModule(state,
                             reinterpret_cast<const uint8_t *>(kHostDescriptorDiagnosticFS),
                             static_cast<int>(sizeof(kHostDescriptorDiagnosticFS)),
                             &state->hostDescriptorDiagnosticFragmentModule,
                             reporter))
    {
        return false;
    }

    const VkSampleCountFlagBits sampleCount = target->color[0]
                                                  ? target->color[0]->sampleCount
                                                  : VK_SAMPLE_COUNT_1_BIT;
    if (state->hostDescriptorDiagnosticPipeline == VK_NULL_PIPELINE ||
        state->hostDescriptorDiagnosticRenderPass != target->renderPass ||
        state->hostDescriptorDiagnosticSubpass != target->subpass ||
        state->hostDescriptorDiagnosticSampleCount != sampleCount)
    {
        if (state->hostDescriptorDiagnosticPipeline != VK_NULL_PIPELINE)
        {
            iRetireGraphicsPipeline(state, state->hostDescriptorDiagnosticPipeline);
            state->hostDescriptorDiagnosticPipeline = VK_NULL_PIPELINE;
        }
        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = state->hostDebugTriangleVertexModule;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = state->hostDescriptorDiagnosticFragmentModule;
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterization = {};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = sampleCount;
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = 0;
        depthStencil.depthWriteEnable = 0;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend = {};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blendAttachment;
        VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = target->hasDepth ? &depthStencil : nullptr;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = state->staticPaintPipelineLayout;
        pipelineInfo.renderPass = target->renderPass;
        pipelineInfo.subpass = target->subpass;
        const VkResult result = state->vkCreateGraphicsPipelines(
            state->device,
            VK_NULL_PIPELINE_CACHE,
            1,
            &pipelineInfo,
            nullptr,
            &state->hostDescriptorDiagnosticPipeline);
        if (result != VK_SUCCESS || state->hostDescriptorDiagnosticPipeline == VK_NULL_PIPELINE)
        {
            iError(reporter, "[IMM_UNITY_VK_GPU_DESCRIPTOR_DIAG_20260731] failed to create pipeline");
            return false;
        }
        state->hostDescriptorDiagnosticRenderPass = target->renderPass;
        state->hostDescriptorDiagnosticSubpass = target->subpass;
        state->hostDescriptorDiagnosticSampleCount = sampleCount;
        iReport(reporter, "[IMM_UNITY_VK_GPU_DESCRIPTOR_DIAG_20260731] created four-bar descriptor pipeline");
    }
    state->vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->hostDescriptorDiagnosticPipeline);
    state->vkCmdBindDescriptorSets(state->commandBuffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   state->staticPaintPipelineLayout,
                                   0,
                                   1,
                                   &state->staticPaintDescriptorSet,
                                   0,
                                   nullptr);
    state->vkCmdDraw(state->commandBuffer, 3, 1, 0, 0);
    return true;
}

static bool iEnsureStaticPaintGraphicsPipeline(piVulkanState *state, piShader shader, piRTarget target, piRenderer::piReporter *reporter)
{
    if (!state || !shader || !target || state->device == VK_NULL_DEVICE)
    {
        return true;
    }
    if (shader->vertexModule == VK_NULL_SHADER_MODULE || shader->fragmentModule == VK_NULL_SHADER_MODULE ||
        shader->pipelineLayout == VK_NULL_PIPELINE_LAYOUT || target->renderPass == VK_NULL_RENDER_PASS ||
        !state->vkCreateGraphicsPipelines)
    {
        return true;
    }

    bool useHostDebugFragment = false;
#if defined(__ANDROID__) || defined(ANDROID)
    useHostDebugFragment = state->hostRenderPassFrameActive;
#endif
    if (useHostDebugFragment && state->hostDebugTriangleFragmentModule == VK_NULL_SHADER_MODULE &&
        !iCreateShaderModule(
            state,
            reinterpret_cast<const uint8_t *>(kHostDebugTriangleFS),
            static_cast<int>(sizeof(kHostDebugTriangleFS)),
            &state->hostDebugTriangleFragmentModule,
            reporter))
    {
        iError(reporter, "[IMM_UNITY_VK_STATIC_VERTEX_CONTROL_20260731] failed to create constant fragment module");
        return false;
    }
    if (useHostDebugFragment && state->hostCenterDiagnosticVertexModule == VK_NULL_SHADER_MODULE &&
        !iCreateShaderModule(
            state,
            reinterpret_cast<const uint8_t *>(kHostVertexDescriptorDiagnosticVS),
            static_cast<int>(sizeof(kHostVertexDescriptorDiagnosticVS)),
            &state->hostCenterDiagnosticVertexModule,
            reporter))
    {
        iError(reporter, "[IMM_UNITY_VK_TRANSFORM_XY_DIAG_20260801] failed to create transform X/Y module");
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = useHostDebugFragment ? state->hostCenterDiagnosticVertexModule : shader->vertexModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = useHostDebugFragment ? state->hostDebugTriangleFragmentModule : shader->fragmentModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    inputAssembly.primitiveRestartEnable = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    piRasterState rasterState = state->currentRasterState;
    VkCullModeFlags cullMode = rasterState ? iToVulkanCullMode(rasterState->cullMode) : VK_CULL_MODE_NONE;
#if defined(__ANDROID__) || defined(ANDROID)
    if (state->hostRenderPassFrameActive)
    {
        // Unity owns the dynamic viewport in this path. Remove face orientation
        // from the diagnostic while retaining the real indexed geometry,
        // descriptors, transforms, and static-paint vertex shader.
        cullMode = VK_CULL_MODE_NONE;
    }
#endif
    const VkFrontFace frontFace = useHostDebugFragment
                                      ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                      : (rasterState && rasterState->frontIsCounterClockWise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE);
    const VkSampleCountFlagBits sampleCount = target->color[0] ? target->color[0]->sampleCount : VK_SAMPLE_COUNT_1_BIT;
    const bool wireframe = !useHostDebugFragment && rasterState && rasterState->wireframe;
    const bool depthClamp = !useHostDebugFragment && rasterState && rasterState->depthClamp;
    const bool mayUseDepth = target->hasDepth &&
                             (!state->hostRenderPassFrameActive || state->externalFrameUsesHostDepth);
    const bool depthTest = mayUseDepth && state->depthTestEnabled && state->currentDepthState && state->currentDepthState->depthEnable;
    const bool useHostReverseZCompare = state->externalFrameUsesHostDepth &&
                                        state->externalFrameHostDepthReverseZ &&
                                        target == state->externalFrameRenderTarget;
    const bool depthWrite = depthTest && state->depthWriteEnabled;
    const VkCompareOp depthCompareOp = useHostDebugFragment
                                           ? VK_COMPARE_OP_ALWAYS
                                           : (useHostReverseZCompare || (state->currentDepthState && !state->currentDepthState->lessEqual) ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL);
    piBlendState blendState = state->currentBlendState;
    const bool alphaToCoverage = !useHostDebugFragment && blendState && blendState->alphaToCoverage;
    const bool blendEnabled = !useHostDebugFragment && blendState && blendState->enabled0;
    if (shader->pipeline != VK_NULL_PIPELINE &&
        shader->pipelineRenderPass == target->renderPass &&
        shader->pipelineCullMode == cullMode &&
        shader->pipelineFrontFace == frontFace &&
        shader->pipelineSampleCount == sampleCount &&
        shader->pipelineWireframe == wireframe &&
        shader->pipelineDepthClamp == depthClamp &&
        shader->pipelineDepthTest == depthTest &&
        shader->pipelineDepthWrite == depthWrite &&
        shader->pipelineDepthCompareOp == depthCompareOp &&
        shader->pipelineAlphaToCoverage == alphaToCoverage &&
        shader->pipelineBlendEnabled == blendEnabled)
    {
        return true;
    }
    if (shader->pipeline != VK_NULL_PIPELINE && state->vkDestroyPipeline)
    {
        iRetireGraphicsPipeline(state, shader->pipeline);
        shader->pipeline = VK_NULL_PIPELINE;
        shader->pipelineRenderPass = VK_NULL_RENDER_PASS;
    }

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = depthClamp ? 1u : 0u;
    rasterization.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterization.cullMode = cullMode;
    rasterization.frontFace = frontFace;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = sampleCount;
    multisample.alphaToCoverageEnable = alphaToCoverage ? 1u : 0u;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthTest ? 1u : 0u;
    depthStencil.depthWriteEnable = depthWrite ? 1u : 0u;
    depthStencil.depthCompareOp = depthCompareOp;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.blendEnable = blendEnabled ? 1u : 0u;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOp = VK_LOGIC_OP_COPY;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = target->hasDepth ? &depthStencil : nullptr;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = shader->pipelineLayout;
    pipelineInfo.renderPass = target->renderPass;
    pipelineInfo.subpass = target->subpass;

    const VkResult result = state->vkCreateGraphicsPipelines(state->device, VK_NULL_PIPELINE_CACHE, 1, &pipelineInfo, nullptr, &shader->pipeline);
    if (result != VK_SUCCESS || shader->pipeline == VK_NULL_PIPELINE)
    {
        iError(reporter, "Vulkan renderer failed to create static paint graphics pipeline");
        shader->pipeline = VK_NULL_PIPELINE;
        shader->pipelineRenderPass = VK_NULL_RENDER_PASS;
        return false;
    }
    shader->pipelineRenderPass = target->renderPass;
    shader->pipelineCullMode = cullMode;
    shader->pipelineFrontFace = frontFace;
    shader->pipelineSampleCount = sampleCount;
    shader->pipelineWireframe = wireframe;
    shader->pipelineDepthClamp = depthClamp;
    shader->pipelineDepthTest = depthTest;
    shader->pipelineDepthWrite = depthWrite;
    shader->pipelineDepthCompareOp = depthCompareOp;
    shader->pipelineAlphaToCoverage = alphaToCoverage;
    shader->pipelineBlendEnabled = blendEnabled;
    if (!state->graphicsPipelineReported)
    {
        iReport(
            reporter,
            useHostDebugFragment
                ? "[IMM_UNITY_VK_GPU_CENTER_DIAG_20260731] created simplified center-geometry pipeline"
                : "Vulkan renderer created static paint graphics pipeline");
        state->graphicsPipelineReported = true;
    }
    return true;
}

static bool iSubmitStaticPaintDraw(piVulkanState *state, piShader shader, piRTarget target, piVertexArray vertexArray, uint32_t num, uint32_t numInstances, uint32_t baseVertex, uint32_t baseInstance, uint32_t baseIndex, piRenderer::piReporter *reporter)
{
    const bool hostRenderPass = state && state->hostRenderPassFrameActive;
    const bool borrowedCommandBuffer = state && state->externalCommandBufferFrameActive;
    if (!state || !shader || !target || !vertexArray || state->device == VK_NULL_DEVICE ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER || (!hostRenderPass && !borrowedCommandBuffer && state->frameFence == VK_NULL_FENCE) ||
        shader->pipeline == VK_NULL_PIPELINE || shader->pipelineLayout == VK_NULL_PIPELINE_LAYOUT ||
        state->staticPaintDescriptorSet == VK_NULL_DESCRIPTOR_SET ||
        target->renderPass == VK_NULL_RENDER_PASS || target->framebuffer == VK_NULL_FRAMEBUFFER ||
        !vertexArray->indexBuffer || vertexArray->indexBuffer->buffer == VK_NULL_BUFFER)
    {
        return true;
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = VK_SUCCESS;
    if (!hostRenderPass)
    {
        if (!borrowedCommandBuffer)
        {
            result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
            if (result != VK_SUCCESS)
            {
                return false;
            }
            state->vkResetFences(state->device, 1, &state->frameFence);
            state->vkResetCommandBuffer(state->commandBuffer, 0);

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
            if (result != VK_SUCCESS)
            {
                return false;
            }
        }

        VkRenderPassBeginInfo renderPassBegin = {};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = target->renderPass;
        renderPassBegin.framebuffer = target->framebuffer;
        renderPassBegin.renderArea.offset.x = 0;
        renderPassBegin.renderArea.offset.y = 0;
        renderPassBegin.renderArea.extent.width = target->width;
        renderPassBegin.renderArea.extent.height = target->height;
        state->vkCmdBeginRenderPass(state->commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = (float)target->height;
    viewport.width = (float)target->width;
    viewport.height = -(float)target->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
#if defined(__ANDROID__) || defined(ANDROID)
    // Diagnostic control for Unity Android Vulkan composition. Collapse only
    // IMM geometry draws to reversed-Z near; picture draws use their separate
    // submission path and retain their normal far-backdrop depth. If Unity's
    // explicitly ordered probes are then rejected, the shared attachment and
    // native depth writes are sound and the defect is projected depth values.
    if (state->externalFrameUsesHostDepth && target == state->externalFrameRenderTarget)
    {
        viewport.minDepth = 1.0f;
        viewport.maxDepth = 1.0f;
        static bool reportedForcedNearDepth = false;
        if (!reportedForcedNearDepth)
        {
            reportedForcedNearDepth = true;
            iReport(reporter, "[IMM_UNITY_ANDROID_VK_FORCE_NEAR_DEPTH_20260802] static geometry viewport depth=1");
        }
    }
#endif
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = target->width;
    scissor.extent.height = target->height;

    state->vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);
    state->vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);
    static bool reportedExplicitHostViewport = false;
    if (hostRenderPass && !reportedExplicitHostViewport)
    {
        reportedExplicitHostViewport = true;
        iReport(reporter, "[IMM_UNITY_VK_EXPLICIT_VIEWPORT_20260801] set host viewport and scissor to the Unity render-target extent");
    }
    state->vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);
    state->vkCmdBindDescriptorSets(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipelineLayout, 0, 1, &state->staticPaintDescriptorSet, 0, nullptr);
    const VkIndexType indexType = vertexArray->indexFormat == piRenderer::IndexArrayFormat::UINT_32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
#if defined(__ANDROID__) || defined(ANDROID)
    if (hostRenderPass)
    {
        if (state->hostDebugIndexBuffer == nullptr)
        {
            const uint32_t indexStride = indexType == VK_INDEX_TYPE_UINT32 ? 4u : 2u;
            const uint32_t availableIndices = vertexArray->indexBuffer->size / indexStride;
            const uint32_t copyCount = baseIndex < availableIndices ? std::min(num, availableIndices - baseIndex) : 0u;
            if (copyCount == 0u)
            {
                iError(reporter, "[IMM_UNITY_VK_CLONED_INDEX_BUFFER_20260731] source index range is empty");
                return false;
            }
            piBuffer buffer = new piBufferS();
            buffer->size = copyCount * indexStride;
            buffer->use = piRenderer::BufferUse::Index;
            buffer->data = static_cast<uint8_t *>(std::malloc(buffer->size));
            if (!buffer->data)
            {
                delete buffer;
                iError(reporter, "[IMM_UNITY_VK_CLONED_INDEX_BUFFER_20260731] failed to allocate CPU index clone");
                return false;
            }
            std::memcpy(buffer->data, vertexArray->indexBuffer->data + baseIndex * indexStride, buffer->size);
            if (!iCreateBufferObject(state, buffer, buffer->data, reporter))
            {
                std::free(buffer->data);
                delete buffer;
                iError(reporter, "[IMM_UNITY_VK_CLONED_INDEX_BUFFER_20260731] failed to create Vulkan index clone");
                return false;
            }
            state->hostDebugIndexBuffer = buffer;
            state->hostDebugIndexSource = vertexArray->indexBuffer;
            state->hostDebugIndexSourceBase = baseIndex;
            state->hostDebugIndexCount = copyCount;
            char message[256];
            std::snprintf(
                message,
                sizeof(message),
                "[IMM_UNITY_VK_CLONED_INDEX_BUFFER_20260731] created bytes=%u count=%u base=%u type=%s",
                buffer->size,
                copyCount,
                baseIndex,
                indexType == VK_INDEX_TYPE_UINT32 ? "uint32" : "uint16");
            iReport(reporter, message);
        }
        if (state->hostDebugIndexSource != vertexArray->indexBuffer || state->hostDebugIndexSourceBase != baseIndex)
        {
            return true;
        }
        if (state->hostDebugResourceBuffers[0] == nullptr)
        {
            piBuffer sources[4] = {
                state->constantBuffers[3],
                state->constantBuffers[4],
                state->shaderBuffers[8],
                state->constantBuffers[9]
            };
            for (uint32_t i = 0; i < 4u; ++i)
            {
                state->hostDebugResourceBuffers[i] = iCloneHostDebugBuffer(state, sources[i], reporter);
                if (!state->hostDebugResourceBuffers[i])
                {
                    iError(reporter, "[IMM_UNITY_VK_CLONED_RESOURCES_20260731] failed to clone production shader resource");
                    return false;
                }
            }
            iReport(reporter, "[IMM_UNITY_VK_CLONED_RESOURCES_20260731] cloned layer, display, vertex, and chunk buffers");
        }
        VkDescriptorBufferInfo clonedInfos[4] = {};
        VkWriteDescriptorSet clonedWrites[4] = {};
        const uint32_t clonedBindings[4] = { 3u, 4u, 8u, 9u };
        const VkDescriptorType clonedTypes[4] = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        };
        for (uint32_t i = 0; i < 4u; ++i)
        {
            clonedInfos[i] = iDescriptorBufferInfo(state->hostDebugResourceBuffers[i]);
            clonedWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            clonedWrites[i].dstSet = state->staticPaintDescriptorSet;
            clonedWrites[i].dstBinding = clonedBindings[i];
            clonedWrites[i].descriptorCount = 1;
            clonedWrites[i].descriptorType = clonedTypes[i];
            clonedWrites[i].pBufferInfo = &clonedInfos[i];
        }
        state->vkUpdateDescriptorSets(state->device, 4, clonedWrites, 0, nullptr);
        state->vkCmdBindIndexBuffer(state->commandBuffer, state->hostDebugIndexBuffer->buffer, 0, indexType);
    }
    else
#endif
    {
        state->vkCmdBindIndexBuffer(state->commandBuffer, vertexArray->indexBuffer->buffer, 0, indexType);
    }
#if defined(__ANDROID__) || defined(ANDROID)
    if (hostRenderPass)
    {
        state->vkCmdDraw(state->commandBuffer, 3, 1, 0, 0);
        static bool reportedVertexDescriptorHostDraw = false;
        if (!reportedVertexDescriptorHostDraw)
        {
            reportedVertexDescriptorHostDraw = true;
            iReport(reporter, "[IMM_UNITY_VK_TRANSFORM_XY_DIAG_20260801] submitted large triangle around production transform X/Y");
        }
    }
    else
#endif
    {
        state->vkCmdDrawIndexed(state->commandBuffer, num, numInstances, baseIndex, (int32_t)baseVertex, baseInstance);
    }
    if (hostRenderPass)
    {
        return true;
    }

    state->vkCmdEndRenderPass(state->commandBuffer);
    if (borrowedCommandBuffer)
    {
        return true;
    }
    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    if (target->color[0])
    {
        target->color[0]->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (!state->drawSubmittedReported)
    {
        iReport(reporter, "Vulkan renderer submitted static paint draw commands");
        state->drawSubmittedReported = true;
    }
    return true;
}

static bool iTransitionColorTextureToShaderRead(piVulkanState *state, piTexture texture)
{
    if (!state || !texture || texture->image == 0 ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER || state->frameFence == VK_NULL_FENCE ||
        !state->vkCmdPipelineBarrier)
    {
        return false;
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    state->vkResetFences(state->device, 1, &state->frameFence);
    state->vkResetCommandBuffer(state->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier toShader = {};
    toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShader.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShader.oldLayout = texture->imageLayout;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.image = texture->image;
    toShader.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toShader);

    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return true;
}

static bool iEnsurePictureGraphicsPipeline(piVulkanState *state, piShader shader, piRTarget target, const piVertexArray vertexArray, piRenderer::piReporter *reporter)
{
    if (!state || !shader || !target)
    {
        return false;
    }
    const bool usesVertexArray = !shader->isPicture2D;
    if (usesVertexArray && !vertexArray)
    {
        return false;
    }
    const bool useHostDepthTarget = state->externalFrameUsesHostDepth &&
                                    target == state->externalFrameRenderTarget;
    const bool hostDepthBackdrop = useHostDepthTarget && !shader->isPicture2D;
    const uint32_t hostDepthBackdropMode = hostDepthBackdrop ? (state->externalFrameHostDepthReverseZ ? 1u : 2u) : 0u;
    const VkSampleCountFlagBits sampleCount = target->color[0] ? target->color[0]->sampleCount : VK_SAMPLE_COUNT_1_BIT;
    const bool mayUseDepth = target->hasDepth &&
                             (!state->hostRenderPassFrameActive || state->externalFrameUsesHostDepth);
    const bool depthTest = mayUseDepth && state->depthTestEnabled && state->currentDepthState && state->currentDepthState->depthEnable;
    const VkCompareOp depthCompareOp = useHostDepthTarget && state->externalFrameHostDepthReverseZ ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
    if (shader->pipeline != VK_NULL_PIPELINE &&
        shader->pipelineRenderPass == target->renderPass &&
        shader->pipelineSampleCount == sampleCount &&
        shader->pipelineDepthTest == depthTest &&
        shader->pipelineDepthCompareOp == depthCompareOp &&
        shader->pipelineHostDepthBackdropMode == hostDepthBackdropMode)
    {
        return true;
    }
    if (shader->pipeline != VK_NULL_PIPELINE && state->vkDestroyPipeline)
    {
        iRetireGraphicsPipeline(state, shader->pipeline);
        shader->pipeline = VK_NULL_PIPELINE;
        shader->pipelineRenderPass = VK_NULL_RENDER_PASS;
    }
    if (shader->vertexModule == VK_NULL_SHADER_MODULE || shader->fragmentModule == VK_NULL_SHADER_MODULE ||
        shader->pipelineLayout == VK_NULL_PIPELINE_LAYOUT || target->renderPass == VK_NULL_RENDER_PASS ||
        !state->vkCreateGraphicsPipelines)
    {
        return false;
    }
    if (usesVertexArray && (vertexArray->attributeCount == 0 || vertexArray->stride[0] == 0))
    {
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = shader->vertexModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = shader->fragmentModule;
    stages[1].pName = "main";

    const uint32_t hostDepthBackdropValue = hostDepthBackdropMode;
    const VkSpecializationMapEntry hostDepthBackdropEntry = { 0u, 0u, sizeof(hostDepthBackdropValue) };
    const VkSpecializationInfo hostDepthBackdropSpecialization = { 1u, &hostDepthBackdropEntry, sizeof(hostDepthBackdropValue), &hostDepthBackdropValue };
    if (hostDepthBackdrop)
    {
        stages[0].pSpecializationInfo = &hostDepthBackdropSpecialization;
    }

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = usesVertexArray ? vertexArray->stride[0] : 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = usesVertexArray ? 1u : 0u;
    vertexInput.pVertexBindingDescriptions = usesVertexArray ? &binding : nullptr;
    vertexInput.vertexAttributeDescriptionCount = usesVertexArray ? vertexArray->attributeCount : 0u;
    vertexInput.pVertexAttributeDescriptions = usesVertexArray ? vertexArray->attributes : nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = sampleCount;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.blendEnable = 1;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthTest ? 1 : 0;
    depthStencil.depthWriteEnable = 0;
    depthStencil.depthCompareOp = depthCompareOp;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = target->hasDepth ? &depthStencil : nullptr;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = shader->pipelineLayout;
    pipelineInfo.renderPass = target->renderPass;
    pipelineInfo.subpass = target->subpass;

    const VkResult result = state->vkCreateGraphicsPipelines(state->device, VK_NULL_PIPELINE_CACHE, 1, &pipelineInfo, nullptr, &shader->pipeline);
    if (result != VK_SUCCESS || shader->pipeline == VK_NULL_PIPELINE)
    {
        iError(reporter, "Vulkan renderer failed to create picture graphics pipeline");
        shader->pipeline = VK_NULL_PIPELINE;
        shader->pipelineRenderPass = VK_NULL_RENDER_PASS;
        return false;
    }
    shader->pipelineRenderPass = target->renderPass;
    shader->pipelineSampleCount = sampleCount;
    shader->pipelineDepthTest = depthTest;
    shader->pipelineDepthCompareOp = depthCompareOp;
    shader->pipelineHostDepthBackdropMode = hostDepthBackdropMode;
    if (!state->picturePipelineReported)
    {
        iReport(reporter, "Vulkan renderer created picture graphics pipeline");
        state->picturePipelineReported = true;
    }
    return true;
}

static bool iSubmitPictureDraw(piVulkanState *state, piShader shader, piRTarget target, const piVertexArray vertexArray, uint32_t num, uint32_t numInstances, uint32_t baseIndex, piRenderer::piReporter *reporter)
{
    const bool hostRenderPass = state && state->hostRenderPassFrameActive;
    const bool borrowedCommandBuffer = state && state->externalCommandBufferFrameActive;
    if (!state || !shader || !target || !vertexArray || !vertexArray->vertexBuffer[0] || !vertexArray->indexBuffer ||
        vertexArray->indexBuffer->buffer == VK_NULL_BUFFER ||
        shader->pipeline == VK_NULL_PIPELINE || shader->pipelineLayout == VK_NULL_PIPELINE_LAYOUT ||
        target->renderPass == VK_NULL_RENDER_PASS || target->framebuffer == VK_NULL_FRAMEBUFFER ||
        state->pictureDescriptorSet == VK_NULL_DESCRIPTOR_SET)
    {
        return false;
    }
    const uint64_t timeout = 5000000000ull;
    VkResult result = VK_SUCCESS;
    if (!hostRenderPass)
    {
        if (!borrowedCommandBuffer)
        {
            result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
            if (result != VK_SUCCESS)
            {
                return false;
            }
            state->vkResetFences(state->device, 1, &state->frameFence);
            state->vkResetCommandBuffer(state->commandBuffer, 0);

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
            if (result != VK_SUCCESS)
            {
                return false;
            }
        }

        VkRenderPassBeginInfo renderPassBegin = {};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = target->renderPass;
        renderPassBegin.framebuffer = target->framebuffer;
        renderPassBegin.renderArea.offset.x = 0;
        renderPassBegin.renderArea.offset.y = 0;
        renderPassBegin.renderArea.extent.width = target->width;
        renderPassBegin.renderArea.extent.height = target->height;
        state->vkCmdBeginRenderPass(state->commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = (float)target->height;
    viewport.width = (float)target->width;
    viewport.height = -(float)target->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = target->width;
    scissor.extent.height = target->height;
    if (!hostRenderPass)
    {
        state->vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);
        state->vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);
    }
    state->vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);
    state->vkCmdBindDescriptorSets(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipelineLayout, 0, 1, &state->pictureDescriptorSet, 0, nullptr);
    VkDeviceSize vertexOffset = 0;
    state->vkCmdBindVertexBuffers(state->commandBuffer, 0, 1, &vertexArray->vertexBuffer[0]->buffer, &vertexOffset);
    const VkIndexType indexType = vertexArray->indexFormat == piRenderer::IndexArrayFormat::UINT_32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    state->vkCmdBindIndexBuffer(state->commandBuffer, vertexArray->indexBuffer->buffer, 0, indexType);
    state->vkCmdDrawIndexed(state->commandBuffer, num, numInstances, baseIndex, 0, 0);
    if (hostRenderPass)
    {
        return true;
    }

    state->vkCmdEndRenderPass(state->commandBuffer);
    if (borrowedCommandBuffer)
    {
        return true;
    }
    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    if (target->color[0])
    {
        target->color[0]->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (!state->pictureDrawReported)
    {
        iReport(reporter, "Vulkan renderer submitted picture draw commands");
        state->pictureDrawReported = true;
    }
    return true;
}

static bool iSubmitPictureQuadDraw(piVulkanState *state, piShader shader, piRTarget target, uint32_t numInstances, piRenderer::piReporter *reporter)
{
    const bool hostRenderPass = state && state->hostRenderPassFrameActive;
    const bool borrowedCommandBuffer = state && state->externalCommandBufferFrameActive;
    if (!state || !shader || !target || !state->vkCmdDraw ||
        shader->pipeline == VK_NULL_PIPELINE || shader->pipelineLayout == VK_NULL_PIPELINE_LAYOUT ||
        target->renderPass == VK_NULL_RENDER_PASS || target->framebuffer == VK_NULL_FRAMEBUFFER ||
        state->pictureDescriptorSet == VK_NULL_DESCRIPTOR_SET)
    {
        return false;
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = VK_SUCCESS;
    if (!hostRenderPass)
    {
        if (!borrowedCommandBuffer)
        {
            result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
            if (result != VK_SUCCESS)
            {
                return false;
            }
            state->vkResetFences(state->device, 1, &state->frameFence);
            state->vkResetCommandBuffer(state->commandBuffer, 0);

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
            if (result != VK_SUCCESS)
            {
                return false;
            }
        }

        VkRenderPassBeginInfo renderPassBegin = {};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = target->renderPass;
        renderPassBegin.framebuffer = target->framebuffer;
        renderPassBegin.renderArea.offset.x = 0;
        renderPassBegin.renderArea.offset.y = 0;
        renderPassBegin.renderArea.extent.width = target->width;
        renderPassBegin.renderArea.extent.height = target->height;
        state->vkCmdBeginRenderPass(state->commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = (float)target->height;
    viewport.width = (float)target->width;
    viewport.height = -(float)target->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = target->width;
    scissor.extent.height = target->height;
    if (!hostRenderPass)
    {
        state->vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);
        state->vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);
    }
    state->vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);
    state->vkCmdBindDescriptorSets(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipelineLayout, 0, 1, &state->pictureDescriptorSet, 0, nullptr);
    state->vkCmdDraw(state->commandBuffer, 6, numInstances, 0, 0);
    if (hostRenderPass)
    {
        return true;
    }

    state->vkCmdEndRenderPass(state->commandBuffer);
    if (borrowedCommandBuffer)
    {
        return true;
    }
    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    if (target->color[0])
    {
        target->color[0]->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (!state->pictureDrawReported)
    {
        iReport(reporter, "Vulkan renderer submitted picture draw commands");
        state->pictureDrawReported = true;
    }
    return true;
}

static bool iReadBackTextureImage(piVulkanState *state, piTexture texture, piRenderer::piReporter *reporter)
{
    if (!state || !texture || !texture->data || texture->dataSize == 0 || texture->image == 0 ||
        texture->info.mFormat != piRenderer::Format::C3_11_11_10_FLOAT ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER || state->frameFence == VK_NULL_FENCE || !state->vkCmdCopyImageToBuffer)
    {
        return true;
    }
    const VkDeviceSize size = (VkDeviceSize)texture->info.mXres * (VkDeviceSize)texture->info.mYres * 4ull;
    if (!iEnsureStagingBuffer(state, size, reporter))
    {
        return false;
    }

    VkImage readbackImage = texture->image;
    VkDeviceMemory readbackMemory = VK_NULL_DEVICE_MEMORY;
    VkImageLayout readbackLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const bool needsResolve = texture->sampleCount != VK_SAMPLE_COUNT_1_BIT;
    if (needsResolve)
    {
        if (!state->vkCmdResolveImage || !state->vkCreateImage || !state->vkGetImageMemoryRequirements || !state->vkBindImageMemory || !state->vkAllocateMemory)
        {
            return false;
        }

        VkImageCreateInfo resolveInfo = {};
        resolveInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        resolveInfo.imageType = VK_IMAGE_TYPE_2D;
        resolveInfo.format = texture->vkFormat;
        resolveInfo.extent.width = (uint32_t)texture->info.mXres;
        resolveInfo.extent.height = (uint32_t)texture->info.mYres;
        resolveInfo.extent.depth = 1;
        resolveInfo.mipLevels = 1;
        resolveInfo.arrayLayers = 1;
        resolveInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        resolveInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        resolveInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        resolveInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult createResult = state->vkCreateImage(state->device, &resolveInfo, nullptr, &readbackImage);
        if (createResult != VK_SUCCESS || readbackImage == 0)
        {
            iError(reporter, "Vulkan renderer failed to create MSAA resolve readback image");
            return false;
        }

        VkMemoryRequirements requirements = {};
        state->vkGetImageMemoryRequirements(state->device, readbackImage, &requirements);
        uint32_t memoryTypeIndex = 0;
        if (!iFindMemoryType(state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryTypeIndex))
        {
            iError(reporter, "Vulkan renderer failed to find MSAA resolve readback memory");
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            return false;
        }

        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = memoryTypeIndex;
        createResult = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, &readbackMemory);
        if (createResult != VK_SUCCESS || readbackMemory == VK_NULL_DEVICE_MEMORY)
        {
            iError(reporter, "Vulkan renderer failed to allocate MSAA resolve readback memory");
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            return false;
        }

        createResult = state->vkBindImageMemory(state->device, readbackImage, readbackMemory, 0);
        if (createResult != VK_SUCCESS)
        {
            iError(reporter, "Vulkan renderer failed to bind MSAA resolve readback memory");
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            state->vkFreeMemory(state->device, readbackMemory, nullptr);
            return false;
        }
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        if (needsResolve)
        {
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            state->vkFreeMemory(state->device, readbackMemory, nullptr);
        }
        return false;
    }
    state->vkResetFences(state->device, 1, &state->frameFence);
    state->vkResetCommandBuffer(state->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toTransfer.oldLayout = texture->imageLayout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture->image;
    toTransfer.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toTransfer);

    if (needsResolve)
    {
        VkImageMemoryBarrier resolveToTransfer = {};
        resolveToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        resolveToTransfer.srcAccessMask = 0;
        resolveToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        resolveToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        resolveToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        resolveToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        resolveToTransfer.image = readbackImage;
        resolveToTransfer.subresourceRange = range;
        state->vkCmdPipelineBarrier(state->commandBuffer,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &resolveToTransfer);

        VkImageResolve resolveRegion = {};
        resolveRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveRegion.srcSubresource.mipLevel = 0;
        resolveRegion.srcSubresource.baseArrayLayer = 0;
        resolveRegion.srcSubresource.layerCount = 1;
        resolveRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveRegion.dstSubresource.mipLevel = 0;
        resolveRegion.dstSubresource.baseArrayLayer = 0;
        resolveRegion.dstSubresource.layerCount = 1;
        resolveRegion.extent.width = (uint32_t)texture->info.mXres;
        resolveRegion.extent.height = (uint32_t)texture->info.mYres;
        resolveRegion.extent.depth = 1;
        state->vkCmdResolveImage(state->commandBuffer,
                                 texture->image,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 readbackImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 1,
                                 &resolveRegion);

        VkImageMemoryBarrier resolveToCopy = {};
        resolveToCopy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        resolveToCopy.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        resolveToCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        resolveToCopy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        resolveToCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        resolveToCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        resolveToCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        resolveToCopy.image = readbackImage;
        resolveToCopy.subresourceRange = range;
        state->vkCmdPipelineBarrier(state->commandBuffer,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &resolveToCopy);
    }

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
    copyRegion.imageExtent.width = (uint32_t)texture->info.mXres;
    copyRegion.imageExtent.height = (uint32_t)texture->info.mYres;
    copyRegion.imageExtent.depth = 1;
    state->vkCmdCopyImageToBuffer(state->commandBuffer, readbackImage, readbackLayout, state->stagingBuffer, 1, &copyRegion);

    VkImageMemoryBarrier toColor = {};
    toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColor.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toColor.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    toColor.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = texture->image;
    toColor.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toColor);

    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        if (needsResolve)
        {
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            state->vkFreeMemory(state->device, readbackMemory, nullptr);
        }
        return false;
    }
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        if (needsResolve)
        {
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            state->vkFreeMemory(state->device, readbackMemory, nullptr);
        }
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        if (needsResolve)
        {
            state->vkDestroyImage(state->device, readbackImage, nullptr);
            state->vkFreeMemory(state->device, readbackMemory, nullptr);
        }
        return false;
    }
    if (needsResolve)
    {
        state->vkDestroyImage(state->device, readbackImage, nullptr);
        state->vkFreeMemory(state->device, readbackMemory, nullptr);
    }

    void *mapped = nullptr;
    result = state->vkMapMemory(state->device, state->stagingMemory, 0, size, 0, &mapped);
    if (result != VK_SUCCESS || !mapped)
    {
        return false;
    }
    const size_t pixelCount = (size_t)texture->info.mXres * (size_t)texture->info.mYres;
    if (texture->vkFormat == VK_FORMAT_B10G11R11_UFLOAT_PACK32)
    {
        iConvertB10G11R11ToRgba8(texture->data, (const uint8_t *)mapped, pixelCount);
    }
    else
    {
        std::memcpy(texture->data, mapped, (size_t)size);
    }
    state->vkUnmapMemory(state->device, state->stagingMemory);
    texture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    uint32_t nonblack = 0;
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const uint8_t *src = texture->data + i * 4u;
        if (src[0] != 0 || src[1] != 0 || src[2] != 0)
        {
            ++nonblack;
        }
    }
    if (!state->gpuReadbackReported || nonblack > state->gpuReadbackBestNonblack)
    {
        state->gpuReadbackBestNonblack = nonblack;
        char message[256];
        std::snprintf(message, sizeof(message), "Vulkan renderer read back static paint GPU target nonblack=%u size=%dx%d draw=%u", nonblack, texture->info.mXres, texture->info.mYres, state->gpuPaintDrawCount);
        iReport(reporter, message);
        state->gpuReadbackReported = true;
    }
    return true;
}

static bool iClearColorTextureImage(piVulkanState *state, piTexture texture, const float *color, piRenderer::piReporter *reporter)
{
    if (!state || !texture || texture->image == 0 || texture->info.mFormat == piRenderer::Format::D1_32_FLOAT ||
        texture->info.mFormat == piRenderer::Format::D1_16_UNORM || texture->info.mFormat == piRenderer::Format::DS_24_8_UINT ||
        texture->info.mFormat == piRenderer::Format::DS_32_8_UINT || state->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        state->frameFence == VK_NULL_FENCE || !state->vkCmdClearColorImage)
    {
        return true;
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    state->vkResetFences(state->device, 1, &state->frameFence);
    state->vkResetCommandBuffer(state->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = texture->imageLayout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture->image;
    toTransfer.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toTransfer);

    VkClearColorValue clearColor = {};
    clearColor.float32[0] = color ? color[0] : 0.0f;
    clearColor.float32[1] = color ? color[1] : 0.0f;
    clearColor.float32[2] = color ? color[2] : 0.0f;
    clearColor.float32[3] = color ? color[3] : 1.0f;
    state->vkCmdClearColorImage(state->commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

    VkImageMemoryBarrier toColor = {};
    toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColor.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    toColor.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = texture->image;
    toColor.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toColor);

    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    texture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (!state->clearTextureReported)
    {
        iReport(reporter, "Vulkan renderer cleared GPU color render target");
        state->clearTextureReported = true;
    }
    return true;
}

static bool iClearDepthTextureImage(piVulkanState *state, piTexture texture, piRenderer::piReporter *reporter)
{
    if (!state || !texture || texture->image == 0 ||
        (texture->info.mFormat != piRenderer::Format::D1_32_FLOAT &&
         texture->info.mFormat != piRenderer::Format::D1_16_UNORM &&
         texture->info.mFormat != piRenderer::Format::DS_24_8_UINT &&
         texture->info.mFormat != piRenderer::Format::DS_32_8_UINT) ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER || state->frameFence == VK_NULL_FENCE ||
        !state->vkCmdClearDepthStencilImage)
    {
        return true;
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    state->vkResetFences(state->device, 1, &state->frameFence);
    state->vkResetCommandBuffer(state->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (texture->info.mFormat == piRenderer::Format::DS_24_8_UINT || texture->info.mFormat == piRenderer::Format::DS_32_8_UINT)
    {
        range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = texture->imageLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = texture->imageLayout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture->image;
    toTransfer.subresourceRange = range;
    const VkPipelineStageFlags sourceStage = texture->imageLayout == VK_IMAGE_LAYOUT_UNDEFINED
                                                 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                 : (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                sourceStage,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toTransfer);

    VkClearDepthStencilValue clearDepth = {};
    clearDepth.depth = 1.0f;
    clearDepth.stencil = 0;
    state->vkCmdClearDepthStencilImage(state->commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1, &range);

    VkImageMemoryBarrier toDepth = {};
    toDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDepth.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toDepth.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toDepth.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    toDepth.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDepth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDepth.image = texture->image;
    toDepth.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toDepth);

    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    texture->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return true;
}

static bool iUploadCpuColorToGpuColorAttachment(piVulkanState *state, piTexture texture, piRenderer::piReporter *reporter)
{
    if (!state || !texture || !texture->data || texture->dataSize == 0 || texture->image == 0 ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER || state->frameFence == VK_NULL_FENCE)
    {
        return true;
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
        iError(reporter, "Vulkan renderer failed to map color attachment upload staging memory");
        return false;
    }
    if (texture->vkFormat == VK_FORMAT_B10G11R11_UFLOAT_PACK32)
    {
        uint32_t *dst = (uint32_t *)mapped;
        const size_t pixelCount = (size_t)texture->info.mXres * (size_t)texture->info.mYres;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            dst[i] = iPackRgba8ToB10G11R11(texture->data + i * 4u);
        }
    }
    else
    {
        std::memcpy(mapped, texture->data, (size_t)size);
    }
    state->vkUnmapMemory(state->device, state->stagingMemory);

    const uint64_t timeout = 5000000000ull;
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    state->vkResetFences(state->device, 1, &state->frameFence);
    state->vkResetCommandBuffer(state->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = texture->imageLayout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture->image;
    toTransfer.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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
    copyRegion.imageExtent.width = (uint32_t)texture->info.mXres;
    copyRegion.imageExtent.height = (uint32_t)texture->info.mYres;
    copyRegion.imageExtent.depth = 1;
    state->vkCmdCopyBufferToImage(state->commandBuffer,
                                  state->stagingBuffer,
                                  texture->image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  1,
                                  &copyRegion);

    VkImageMemoryBarrier toColor = {};
    toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColor.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    toColor.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = texture->image;
    toColor.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toColor);

    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    texture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

static VkSampleCountFlagBits iRequestedTextureSampleCount(const piRenderer::TextureInfo &info)
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

static VkSampleCountFlagBits iTextureSampleCount(const piTexture texture)
{
    return texture ? texture->sampleCount : VK_SAMPLE_COUNT_1_BIT;
}

static bool iCreateTextureImage(piVulkanState *state, piTexture texture, int bindUsage, piRenderer::piReporter *reporter)
{
    (void)bindUsage;
    const bool is2D = texture && texture->info.mType == piRenderer::TextureType::T2D;
    const bool is2DArray = texture && texture->info.mType == piRenderer::TextureType::T2D_ARRAY;
    if (!state || !texture || state->device == VK_NULL_DEVICE || (!is2D && !is2DArray) ||
        texture->info.mXres <= 0 || texture->info.mYres <= 0 || (is2DArray && texture->info.mZres <= 0))
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
    imageInfo.arrayLayers = is2DArray ? (uint32_t)texture->info.mZres : 1u;
    texture->sampleCount = is2DArray ? VK_SAMPLE_COUNT_1_BIT : iRequestedTextureSampleCount(texture->info);
    imageInfo.samples = texture->sampleCount;
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
    viewInfo.viewType = is2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = iTextureAspectMask(texture->info.mFormat);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = imageInfo.arrayLayers;
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
            sampleCount = iTextureSampleCount(color);
        }
        else if (width != (uint32_t)color->info.mXres || height != (uint32_t)color->info.mYres)
        {
            iError(reporter, "Vulkan render target color attachment sizes do not match");
            return false;
        }
        if (sampleCount != iTextureSampleCount(color))
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
            sampleCount = iTextureSampleCount(depth);
        }
        else if (width != (uint32_t)depth->info.mXres || height != (uint32_t)depth->info.mYres)
        {
            iError(reporter, "Vulkan render target depth attachment size does not match");
            return false;
        }
        if (sampleCount != iTextureSampleCount(depth))
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
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

static void iCollectBorrowedUploads(piVulkanState *state, uint64_t safeFrameNumber)
{
    if (!state)
    {
        return;
    }
    auto upload = state->borrowedUploads.begin();
    while (upload != state->borrowedUploads.end())
    {
        if (upload->frameNumber <= safeFrameNumber)
        {
            if (upload->buffer != VK_NULL_BUFFER && state->vkDestroyBuffer)
            {
                state->vkDestroyBuffer(state->device, upload->buffer, nullptr);
            }
            if (upload->memory != VK_NULL_DEVICE_MEMORY && state->vkFreeMemory)
            {
                state->vkFreeMemory(state->device, upload->memory, nullptr);
            }
            upload = state->borrowedUploads.erase(upload);
        }
        else
        {
            ++upload;
        }
    }
}

static void iCollectBorrowedPipelines(piVulkanState *state, uint64_t safeFrameNumber)
{
    if (!state)
    {
        return;
    }
    auto pipeline = state->borrowedPipelines.begin();
    while (pipeline != state->borrowedPipelines.end())
    {
        if (pipeline->frameNumber <= safeFrameNumber)
        {
            if (pipeline->pipeline != VK_NULL_PIPELINE && state->vkDestroyPipeline)
            {
                state->vkDestroyPipeline(state->device, pipeline->pipeline, nullptr);
            }
            pipeline = state->borrowedPipelines.erase(pipeline);
        }
        else
        {
            ++pipeline;
        }
    }
}

static bool iCreateBorrowedUploadBuffer(
    piVulkanState *state,
    const void *data,
    VkDeviceSize size,
    piVulkanBorrowedUpload *upload,
    piRenderer::piReporter *reporter)
{
    if (!state || !data || size == 0 || !upload)
    {
        return false;
    }

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = state->vkCreateBuffer(state->device, &bufferInfo, nullptr, &upload->buffer);
    if (result != VK_SUCCESS || upload->buffer == VK_NULL_BUFFER)
    {
        iError(reporter, "Vulkan renderer failed to create borrowed upload buffer");
        return false;
    }

    VkMemoryRequirements requirements = {};
    state->vkGetBufferMemoryRequirements(state->device, upload->buffer, &requirements);
    uint32_t memoryTypeIndex = 0;
    if (!iFindMemoryType(
            state,
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &memoryTypeIndex))
    {
        iError(reporter, "Vulkan renderer failed to find borrowed upload memory");
        state->vkDestroyBuffer(state->device, upload->buffer, nullptr);
        upload->buffer = VK_NULL_BUFFER;
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = state->vkAllocateMemory(state->device, &allocateInfo, nullptr, &upload->memory);
    if (result != VK_SUCCESS || upload->memory == VK_NULL_DEVICE_MEMORY)
    {
        iError(reporter, "Vulkan renderer failed to allocate borrowed upload memory");
        state->vkDestroyBuffer(state->device, upload->buffer, nullptr);
        upload->buffer = VK_NULL_BUFFER;
        return false;
    }

    result = state->vkBindBufferMemory(state->device, upload->buffer, upload->memory, 0);
    if (result != VK_SUCCESS)
    {
        iError(reporter, "Vulkan renderer failed to bind borrowed upload memory");
        state->vkDestroyBuffer(state->device, upload->buffer, nullptr);
        state->vkFreeMemory(state->device, upload->memory, nullptr);
        upload->buffer = VK_NULL_BUFFER;
        upload->memory = VK_NULL_DEVICE_MEMORY;
        return false;
    }

    void *mapped = nullptr;
    result = state->vkMapMemory(state->device, upload->memory, 0, size, 0, &mapped);
    if (result != VK_SUCCESS || !mapped)
    {
        iError(reporter, "Vulkan renderer failed to map borrowed upload memory");
        state->vkDestroyBuffer(state->device, upload->buffer, nullptr);
        state->vkFreeMemory(state->device, upload->memory, nullptr);
        upload->buffer = VK_NULL_BUFFER;
        upload->memory = VK_NULL_DEVICE_MEMORY;
        return false;
    }
    std::memcpy(mapped, data, static_cast<size_t>(size));
    state->vkUnmapMemory(state->device, upload->memory);
    upload->frameNumber = state->borrowedCurrentFrameNumber;
    return true;
}

static bool iUploadTextureImageData(piVulkanState *state, piTexture texture, piRenderer::piReporter *reporter)
{
    if (state && state->hostRenderPassFrameActive)
    {
        if (!state->hostTextureUploadRejectedReported)
        {
            iError(reporter, "[IMM_UNITY_VK_PREWARM_20260731] rejected lazy texture upload inside Unity host render pass");
            state->hostTextureUploadRejectedReported = true;
        }
        return false;
    }
    const bool borrowedCommandBuffer = state && state->externalCommandBufferFrameActive;
    if (!state || !texture || !texture->data || texture->dataSize == 0 || texture->image == 0 ||
        state->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        (!borrowedCommandBuffer && state->frameFence == VK_NULL_FENCE))
    {
        return true;
    }

    piVulkanBorrowedUpload borrowedUpload = {};
    VkBuffer uploadBuffer = state->stagingBuffer;
    VkResult result = VK_SUCCESS;
    if (borrowedCommandBuffer)
    {
        if (!iCreateBorrowedUploadBuffer(
                state,
                texture->data,
                static_cast<VkDeviceSize>(texture->dataSize),
                &borrowedUpload,
                reporter))
        {
            return false;
        }
        uploadBuffer = borrowedUpload.buffer;
    }
    else
    {
        if (!iEnsureStagingBuffer(state, static_cast<VkDeviceSize>(texture->dataSize), reporter))
        {
            return false;
        }
        uploadBuffer = state->stagingBuffer;
        void *mapped = nullptr;
        result = state->vkMapMemory(state->device, state->stagingMemory, 0, static_cast<VkDeviceSize>(texture->dataSize), 0, &mapped);
        if (result != VK_SUCCESS || !mapped)
        {
            iError(reporter, "Vulkan renderer failed to map texture staging memory");
            return false;
        }
        std::memcpy(mapped, texture->data, texture->dataSize);
        state->vkUnmapMemory(state->device, state->stagingMemory);
    }

    const uint64_t timeout = 5000000000ull;
    if (!borrowedCommandBuffer)
    {
        result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
        if (result != VK_SUCCESS)
        {
            return false;
        }
        state->vkResetFences(state->device, 1, &state->frameFence);
        state->vkResetCommandBuffer(state->commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = state->vkBeginCommandBuffer(state->commandBuffer, &beginInfo);
        if (result != VK_SUCCESS)
        {
            return false;
        }
    }

    VkImageSubresourceRange range = {};
    range.aspectMask = iTextureAspectMask(texture->info.mFormat);
    range.baseMipLevel = 0;
    range.levelCount = texture->info.mMultisample > 1 ? 1u : (texture->info.mNumMips > 0 ? texture->info.mNumMips : 1u);
    range.baseArrayLayer = 0;
    range.layerCount = texture->info.mType == piRenderer::TextureType::T2D_ARRAY ? (uint32_t)texture->info.mZres : 1u;

    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = texture->imageLayout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture->image;
    toTransfer.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
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
    copyRegion.imageSubresource.aspectMask = range.aspectMask;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = range.layerCount;
    copyRegion.imageOffset.x = 0;
    copyRegion.imageOffset.y = 0;
    copyRegion.imageOffset.z = 0;
    copyRegion.imageExtent.width = (uint32_t)texture->info.mXres;
    copyRegion.imageExtent.height = (uint32_t)texture->info.mYres;
    copyRegion.imageExtent.depth = 1;
    state->vkCmdCopyBufferToImage(state->commandBuffer,
                                  uploadBuffer,
                                  texture->image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  1,
                                  &copyRegion);

    VkImageMemoryBarrier toShader = {};
    toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.image = texture->image;
    toShader.subresourceRange = range;
    state->vkCmdPipelineBarrier(state->commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &toShader);

    if (borrowedCommandBuffer)
    {
        state->borrowedUploads.push_back(borrowedUpload);
        texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return true;
    }

    result = state->vkEndCommandBuffer(state->commandBuffer);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    result = state->vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->frameFence);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    result = state->vkWaitForFences(state->device, 1, &state->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return false;
    }
    texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
#elif defined(ANDROID)
    const char *instanceExtensions[] = { "VK_KHR_surface", "VK_KHR_android_surface" };
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
#elif defined(ANDROID)
    if (hwnd && hwnd[0])
    {
        mState->window = (ANativeWindow *)hwnd[0];
        const int width = ANativeWindow_getWidth(mState->window);
        const int height = ANativeWindow_getHeight(mState->window);
        if (width > 0 && height > 0)
        {
            mState->windowWidth = width;
            mState->windowHeight = height;
        }
    }
#endif

    const piVulkanExternalDevice *externalDevice = static_cast<const piVulkanExternalDevice *>(device);
    if (externalDevice && externalDevice->instance && externalDevice->physicalDevice &&
        externalDevice->device && externalDevice->graphicsQueue && externalDevice->getInstanceProcAddr)
    {
        mState->instance = static_cast<VkInstance>(externalDevice->instance);
        mState->physicalDevice = static_cast<VkPhysicalDevice>(externalDevice->physicalDevice);
        mState->device = static_cast<VkDevice>(externalDevice->device);
        mState->graphicsQueue = static_cast<VkQueue>(externalDevice->graphicsQueue);
        mState->graphicsQueueFamilyIndex = externalDevice->graphicsQueueFamilyIndex;
        mState->vkGetInstanceProcAddr =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(externalDevice->getInstanceProcAddr);
        if (!iLoadVulkanInstanceEntryPoints(mState, mReporter) ||
            !iLoadVulkanSwapchainEntryPoints(mState, mReporter) ||
            !iCreateVulkanFrameResources(mState, mReporter))
        {
            Deinitialize();
            return false;
        }
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
    EndExternalImageFrame();
    if (mState->device != VK_NULL_DEVICE)
    {
        if (mState->vkDeviceWaitIdle)
        {
            mState->vkDeviceWaitIdle(mState->device);
        }
        if (mState->borrowedExternalRenderTarget)
        {
            DestroyRenderTarget(mState->borrowedExternalRenderTarget);
            mState->borrowedExternalRenderTarget = nullptr;
        }
        if (mState->borrowedExternalDepthTexture)
        {
            DestroyTexture(mState->borrowedExternalDepthTexture);
            mState->borrowedExternalDepthTexture = nullptr;
        }
        if (mState->borrowedExternalColorTexture)
        {
            DestroyTexture(mState->borrowedExternalColorTexture);
            mState->borrowedExternalColorTexture = nullptr;
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
        if (mState->staticPaintPipelineLayout != VK_NULL_PIPELINE_LAYOUT && mState->vkDestroyPipelineLayout)
        {
            mState->vkDestroyPipelineLayout(mState->device, mState->staticPaintPipelineLayout, nullptr);
            mState->staticPaintPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        }
        if (mState->picturePipelineLayout != VK_NULL_PIPELINE_LAYOUT && mState->vkDestroyPipelineLayout)
        {
            mState->vkDestroyPipelineLayout(mState->device, mState->picturePipelineLayout, nullptr);
            mState->picturePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        }
        if (mState->staticPaintDescriptorPool != VK_NULL_DESCRIPTOR_POOL && mState->vkDestroyDescriptorPool)
        {
            mState->vkDestroyDescriptorPool(mState->device, mState->staticPaintDescriptorPool, nullptr);
            mState->staticPaintDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
            mState->staticPaintDescriptorSet = VK_NULL_DESCRIPTOR_SET;
        }
        if (mState->pictureDescriptorPool != VK_NULL_DESCRIPTOR_POOL && mState->vkDestroyDescriptorPool)
        {
            mState->vkDestroyDescriptorPool(mState->device, mState->pictureDescriptorPool, nullptr);
            mState->pictureDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
            mState->pictureDescriptorSet = VK_NULL_DESCRIPTOR_SET;
        }
        if (mState->presentDescriptorPool != VK_NULL_DESCRIPTOR_POOL && mState->vkDestroyDescriptorPool)
        {
            mState->vkDestroyDescriptorPool(mState->device, mState->presentDescriptorPool, nullptr);
            mState->presentDescriptorPool = VK_NULL_DESCRIPTOR_POOL;
            mState->presentDescriptorSet = VK_NULL_DESCRIPTOR_SET;
        }
        if (mState->staticPaintDescriptorSetLayout != VK_NULL_DESCRIPTOR_SET_LAYOUT && mState->vkDestroyDescriptorSetLayout)
        {
            mState->vkDestroyDescriptorSetLayout(mState->device, mState->staticPaintDescriptorSetLayout, nullptr);
            mState->staticPaintDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        }
        if (mState->pictureDescriptorSetLayout != VK_NULL_DESCRIPTOR_SET_LAYOUT && mState->vkDestroyDescriptorSetLayout)
        {
            mState->vkDestroyDescriptorSetLayout(mState->device, mState->pictureDescriptorSetLayout, nullptr);
            mState->pictureDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        }
        if (mState->presentPipeline != VK_NULL_PIPELINE && mState->vkDestroyPipeline)
        {
            mState->vkDestroyPipeline(mState->device, mState->presentPipeline, nullptr);
            mState->presentPipeline = VK_NULL_PIPELINE;
        }
        if (mState->hostDebugTrianglePipeline != VK_NULL_PIPELINE && mState->vkDestroyPipeline)
        {
            mState->vkDestroyPipeline(mState->device, mState->hostDebugTrianglePipeline, nullptr);
            mState->hostDebugTrianglePipeline = VK_NULL_PIPELINE;
        }
        if (mState->hostDescriptorDiagnosticPipeline != VK_NULL_PIPELINE && mState->vkDestroyPipeline)
        {
            mState->vkDestroyPipeline(mState->device, mState->hostDescriptorDiagnosticPipeline, nullptr);
            mState->hostDescriptorDiagnosticPipeline = VK_NULL_PIPELINE;
        }
        if (mState->hostDebugTrianglePipelineLayout != VK_NULL_PIPELINE_LAYOUT && mState->vkDestroyPipelineLayout)
        {
            mState->vkDestroyPipelineLayout(mState->device, mState->hostDebugTrianglePipelineLayout, nullptr);
            mState->hostDebugTrianglePipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        }
        if (mState->hostDebugTriangleVertexModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->hostDebugTriangleVertexModule, nullptr);
            mState->hostDebugTriangleVertexModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->hostIndexedControlVertexModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->hostIndexedControlVertexModule, nullptr);
            mState->hostIndexedControlVertexModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->hostDebugTriangleFragmentModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->hostDebugTriangleFragmentModule, nullptr);
            mState->hostDebugTriangleFragmentModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->hostCenterDiagnosticVertexModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->hostCenterDiagnosticVertexModule, nullptr);
            mState->hostCenterDiagnosticVertexModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->hostDescriptorDiagnosticFragmentModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->hostDescriptorDiagnosticFragmentModule, nullptr);
            mState->hostDescriptorDiagnosticFragmentModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->hostDebugIndexBuffer)
        {
            if (mState->hostDebugIndexBuffer->buffer != VK_NULL_BUFFER && mState->vkDestroyBuffer)
            {
                mState->vkDestroyBuffer(mState->device, mState->hostDebugIndexBuffer->buffer, nullptr);
            }
            if (mState->hostDebugIndexBuffer->memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
            {
                mState->vkFreeMemory(mState->device, mState->hostDebugIndexBuffer->memory, nullptr);
            }
            std::free(mState->hostDebugIndexBuffer->data);
            delete mState->hostDebugIndexBuffer;
            mState->hostDebugIndexBuffer = nullptr;
        }
        for (piBuffer &buffer : mState->hostDebugResourceBuffers)
        {
            if (!buffer)
            {
                continue;
            }
            if (buffer->buffer != VK_NULL_BUFFER && mState->vkDestroyBuffer)
            {
                mState->vkDestroyBuffer(mState->device, buffer->buffer, nullptr);
            }
            if (buffer->memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
            {
                mState->vkFreeMemory(mState->device, buffer->memory, nullptr);
            }
            std::free(buffer->data);
            delete buffer;
            buffer = nullptr;
        }
        if (mState->presentPipelineLayout != VK_NULL_PIPELINE_LAYOUT && mState->vkDestroyPipelineLayout)
        {
            mState->vkDestroyPipelineLayout(mState->device, mState->presentPipelineLayout, nullptr);
            mState->presentPipelineLayout = VK_NULL_PIPELINE_LAYOUT;
        }
        if (mState->presentDescriptorSetLayout != VK_NULL_DESCRIPTOR_SET_LAYOUT && mState->vkDestroyDescriptorSetLayout)
        {
            mState->vkDestroyDescriptorSetLayout(mState->device, mState->presentDescriptorSetLayout, nullptr);
            mState->presentDescriptorSetLayout = VK_NULL_DESCRIPTOR_SET_LAYOUT;
        }
        if (mState->presentVertexModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->presentVertexModule, nullptr);
            mState->presentVertexModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->presentFragmentModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, mState->presentFragmentModule, nullptr);
            mState->presentFragmentModule = VK_NULL_SHADER_MODULE;
        }
        if (mState->presentSampler != VK_NULL_SAMPLER && mState->vkDestroySampler)
        {
            mState->vkDestroySampler(mState->device, mState->presentSampler, nullptr);
            mState->presentSampler = VK_NULL_SAMPLER;
        }
        if (mState->hostTransientUniformMapped)
        {
            mState->vkUnmapMemory(mState->device, mState->hostTransientUniformMemory);
            mState->hostTransientUniformMapped = nullptr;
        }
        if (mState->hostTransientUniformBuffer != VK_NULL_BUFFER && mState->vkDestroyBuffer)
        {
            mState->vkDestroyBuffer(mState->device, mState->hostTransientUniformBuffer, nullptr);
            mState->hostTransientUniformBuffer = VK_NULL_BUFFER;
        }
        if (mState->hostTransientUniformMemory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
        {
            mState->vkFreeMemory(mState->device, mState->hostTransientUniformMemory, nullptr);
            mState->hostTransientUniformMemory = VK_NULL_DEVICE_MEMORY;
            mState->hostTransientUniformSize = 0;
            mState->hostTransientUniformOffset = 0;
            mState->hostTransientUniformLimit = 0;
        }
        for (const piVulkanBorrowedUpload &upload : mState->borrowedUploads)
        {
            if (upload.buffer != VK_NULL_BUFFER && mState->vkDestroyBuffer)
            {
                mState->vkDestroyBuffer(mState->device, upload.buffer, nullptr);
            }
            if (upload.memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
            {
                mState->vkFreeMemory(mState->device, upload.memory, nullptr);
            }
        }
        mState->borrowedUploads.clear();
        for (const piVulkanBorrowedPipeline &pipeline : mState->borrowedPipelines)
        {
            if (pipeline.pipeline != VK_NULL_PIPELINE && mState->vkDestroyPipeline)
            {
                mState->vkDestroyPipeline(mState->device, pipeline.pipeline, nullptr);
            }
        }
        mState->borrowedPipelines.clear();
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
        if (mState->ownsDevice && mState->vkDestroyDevice)
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

bool piRendererVulkan::UsesExternalHostDepth(void) const { return mState && mState->externalFrameUsesHostDepth; }
bool piRendererVulkan::IsExternalHostFrame(void) const { return mState && mState->hostRenderPassFrameActive; }

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

static bool iRecordSrgbPresentPass(piVulkanState *state, piTexture source, uint32_t imageIndex, piRenderer::piReporter *reporter)
{
    if (!state || !source || imageIndex >= state->swapchainImageCount ||
        state->swapchainRenderPass == VK_NULL_RENDER_PASS || state->swapchainFramebuffers[imageIndex] == VK_NULL_FRAMEBUFFER ||
        source->image == 0 || source->imageView == VK_NULL_IMAGE_VIEW)
    {
        return false;
    }
    if (!iEnsureSrgbPresentPipeline(state, reporter))
    {
        return false;
    }

    VkImageSubresourceRange colorRange = {};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = 1;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = 1;

    const bool sourceNeedsShaderTransition = source->imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (sourceNeedsShaderTransition)
    {
        VkImageMemoryBarrier toShader = {};
        toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShader.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.oldLayout = source->imageLayout;
        toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.image = source->image;
        toShader.subresourceRange = colorRange;
        state->vkCmdPipelineBarrier(state->commandBuffer,
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &toShader);
        source->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if (!iUpdateSrgbPresentDescriptorSet(state, source, reporter))
    {
        return false;
    }

    VkClearValue clearValue = {};
    clearValue.color.float32[0] = 0.0f;
    clearValue.color.float32[1] = 0.0f;
    clearValue.color.float32[2] = 0.0f;
    clearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo renderPassBegin = {};
    renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBegin.renderPass = state->swapchainRenderPass;
    renderPassBegin.framebuffer = state->swapchainFramebuffers[imageIndex];
    renderPassBegin.renderArea.offset.x = 0;
    renderPassBegin.renderArea.offset.y = 0;
    renderPassBegin.renderArea.extent = state->swapchainExtent;
    renderPassBegin.clearValueCount = 1;
    renderPassBegin.pClearValues = &clearValue;
    state->vkCmdBeginRenderPass(state->commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)state->swapchainExtent.width;
    viewport.height = (float)state->swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = state->swapchainExtent;
    state->vkCmdSetViewport(state->commandBuffer, 0, 1, &viewport);
    state->vkCmdSetScissor(state->commandBuffer, 0, 1, &scissor);
    state->vkCmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->presentPipeline);
    state->vkCmdBindDescriptorSets(state->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->presentPipelineLayout, 0, 1, &state->presentDescriptorSet, 0, nullptr);
    state->vkCmdDraw(state->commandBuffer, 3, 1, 0, 0);
    state->vkCmdEndRenderPass(state->commandBuffer);

    if (sourceNeedsShaderTransition)
    {
        VkImageMemoryBarrier backToColor = {};
        backToColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        backToColor.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        backToColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        backToColor.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        backToColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        backToColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        backToColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        backToColor.image = source->image;
        backToColor.subresourceRange = colorRange;
        state->vkCmdPipelineBarrier(state->commandBuffer,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &backToColor);
        source->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (!state->presentPassReported)
    {
        iReport(reporter, "IMM_VK_SRGB_PRESENT: rendered linear B10G11R11 target through GPU sRGB present pass");
        state->presentPassReported = true;
    }
    return true;
}

static void iDestroyPresentScratch(piVulkanState *state, VkImage image, VkImageView view, VkDeviceMemory memory)
{
    if (!state || state->device == VK_NULL_DEVICE)
    {
        return;
    }
    if (view != VK_NULL_IMAGE_VIEW && state->vkDestroyImageView)
    {
        state->vkDestroyImageView(state->device, view, nullptr);
    }
    if (memory != VK_NULL_DEVICE_MEMORY)
    {
        if (image != 0 && state->vkDestroyImage)
        {
            state->vkDestroyImage(state->device, image, nullptr);
        }
        if (state->vkFreeMemory)
        {
            state->vkFreeMemory(state->device, memory, nullptr);
        }
    }
}

void piRendererVulkan::SwapBuffers(void)
{
    if (!mState || mState->swapchain == VK_NULL_SWAPCHAIN_KHR || mState->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        mState->imageAvailableSemaphore == VK_NULL_SEMAPHORE || mState->renderFinishedSemaphore == VK_NULL_SEMAPHORE ||
        mState->frameFence == VK_NULL_FENCE)
    {
        return;
    }

    const uint64_t timeout = 5000000000ull;
    VkResult result = mState->vkWaitForFences(mState->device, 1, &mState->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        return;
    }

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
    VkImage directPresentImage = presentTexture ? presentTexture->image : 0;
    VkImageView directPresentImageView = VK_NULL_IMAGE_VIEW;
    VkDeviceMemory directPresentMemory = VK_NULL_DEVICE_MEMORY;
    const bool directPresentNeedsResolve = presentTexture && presentTexture->sampleCount != VK_SAMPLE_COUNT_1_BIT;
    bool directPresentTexture =
        presentTexture && presentTexture->image != 0 &&
        presentTexture->info.mXres == (int)mState->swapchainExtent.width &&
        presentTexture->info.mYres == (int)mState->swapchainExtent.height &&
        mState->gpuPaintDrawCount > 0 &&
        mState->vkCmdBlitImage;
    bool presentTextureHasColor = directPresentTexture;
    if (!presentTextureHasColor && presentTexture && presentTexture->data)
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
    const bool srgbGpuPresentTexture =
        directPresentTexture &&
        presentTexture &&
        presentTexture->vkFormat == VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    if (directPresentTexture && directPresentNeedsResolve)
    {
        const VkImageUsageFlags resolveUsage = srgbGpuPresentTexture
                                                   ? (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                                   : (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        directPresentTexture = iCreateDeviceLocalImage(mState,
                                                       (uint32_t)presentTexture->info.mXres,
                                                       (uint32_t)presentTexture->info.mYres,
                                                       presentTexture->vkFormat,
                                                       VK_SAMPLE_COUNT_1_BIT,
                                                       resolveUsage,
                                                       &directPresentImage,
                                                       &directPresentMemory,
                                                       mReporter);
        if (directPresentTexture && srgbGpuPresentTexture)
        {
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = directPresentImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = presentTexture->vkFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            VkResult viewResult = mState->vkCreateImageView(mState->device, &viewInfo, nullptr, &directPresentImageView);
            if (viewResult != VK_SUCCESS || directPresentImageView == VK_NULL_IMAGE_VIEW)
            {
                iError(mReporter, "Vulkan renderer failed to create sRGB present resolve image view");
                directPresentTexture = false;
            }
        }
    }
    if (!mState->presentSelectionReported && presentTexture)
    {
        mState->presentSelectionReported = true;
        char message[512];
        std::snprintf(message,
                      sizeof(message),
                      "IMMAVAL present select direct=%d srgb=%d needsResolve=%d sampleCount=%u resolveImage=%d resolveView=%d format=%u size=%dx%d gpuPaintDraws=%u",
                      directPresentTexture ? 1 : 0,
                      srgbGpuPresentTexture ? 1 : 0,
                      directPresentNeedsResolve ? 1 : 0,
                      (unsigned int)presentTexture->sampleCount,
                      directPresentImage != 0 ? 1 : 0,
                      directPresentImageView != VK_NULL_IMAGE_VIEW ? 1 : 0,
                      (unsigned int)presentTexture->vkFormat,
                      presentTexture->info.mXres,
                      presentTexture->info.mYres,
                      mState->gpuPaintDrawCount);
        iReport(mReporter, message);
    }
    bool copyTexture =
        !directPresentTexture &&
        presentTexture && presentTexture->data &&
        presentTexture->info.mXres == (int)mState->swapchainExtent.width &&
        presentTexture->info.mYres == (int)mState->swapchainExtent.height &&
        iUploadTextureToStaging(mState, presentTexture, mReporter);

    mState->vkResetFences(mState->device, 1, &mState->frameFence);
    mState->vkResetCommandBuffer(mState->commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = mState->vkBeginCommandBuffer(mState->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        iDestroyPresentScratch(mState, directPresentImage, directPresentImageView, directPresentMemory);
        return;
    }

    if (srgbGpuPresentTexture)
    {
        piTextureS resolvedPresentTexture = {};
        piTexture srgbPresentSource = presentTexture;
        if (directPresentNeedsResolve)
        {
            VkImageSubresourceRange colorRange = {};
            colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorRange.baseMipLevel = 0;
            colorRange.levelCount = 1;
            colorRange.baseArrayLayer = 0;
            colorRange.layerCount = 1;

            VkImageMemoryBarrier sourceToTransfer = {};
            sourceToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            sourceToTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
            sourceToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceToTransfer.oldLayout = presentTexture->imageLayout;
            sourceToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            sourceToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sourceToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sourceToTransfer.image = presentTexture->image;
            sourceToTransfer.subresourceRange = colorRange;
            mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         1,
                                         &sourceToTransfer);

            VkImageMemoryBarrier resolveToTransfer = {};
            resolveToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            resolveToTransfer.srcAccessMask = 0;
            resolveToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            resolveToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolveToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            resolveToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolveToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolveToTransfer.image = directPresentImage;
            resolveToTransfer.subresourceRange = colorRange;
            mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         1,
                                         &resolveToTransfer);

            VkImageResolve resolveRegion = {};
            resolveRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolveRegion.srcSubresource.mipLevel = 0;
            resolveRegion.srcSubresource.baseArrayLayer = 0;
            resolveRegion.srcSubresource.layerCount = 1;
            resolveRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolveRegion.dstSubresource.mipLevel = 0;
            resolveRegion.dstSubresource.baseArrayLayer = 0;
            resolveRegion.dstSubresource.layerCount = 1;
            resolveRegion.extent.width = (uint32_t)presentTexture->info.mXres;
            resolveRegion.extent.height = (uint32_t)presentTexture->info.mYres;
            resolveRegion.extent.depth = 1;
            mState->vkCmdResolveImage(mState->commandBuffer,
                                      presentTexture->image,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      directPresentImage,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      1,
                                      &resolveRegion);

            VkImageMemoryBarrier barriers[2] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].image = presentTexture->image;
            barriers[0].subresourceRange = colorRange;
            barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].image = directPresentImage;
            barriers[1].subresourceRange = colorRange;
            mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         2,
                                         barriers);
            presentTexture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            resolvedPresentTexture = *presentTexture;
            resolvedPresentTexture.image = directPresentImage;
            resolvedPresentTexture.imageView = directPresentImageView;
            resolvedPresentTexture.memory = VK_NULL_DEVICE_MEMORY;
            resolvedPresentTexture.sampleCount = VK_SAMPLE_COUNT_1_BIT;
            resolvedPresentTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srgbPresentSource = &resolvedPresentTexture;
        }

        if (!iRecordSrgbPresentPass(mState, srgbPresentSource, imageIndex, mReporter))
        {
            if (!mState->gpuReadbackFailureReported)
            {
                mState->gpuReadbackFailureReported = true;
                iError(mReporter, "Vulkan renderer failed to record sRGB present pass");
            }
            iDestroyPresentScratch(mState, directPresentImage, directPresentImageView, directPresentMemory);
            return;
        }
        directPresentTexture = false;
    }
    else if (directPresentTexture)
    {
        VkImageSubresourceRange colorRange = {};
        colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorRange.baseMipLevel = 0;
        colorRange.levelCount = 1;
        colorRange.baseArrayLayer = 0;
        colorRange.layerCount = 1;

        VkImageMemoryBarrier sourceToTransfer = {};
        sourceToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        sourceToTransfer.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        sourceToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceToTransfer.oldLayout = presentTexture->imageLayout;
        sourceToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        sourceToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        sourceToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        sourceToTransfer.image = presentTexture->image;
        sourceToTransfer.subresourceRange = colorRange;
        mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &sourceToTransfer);

        if (directPresentNeedsResolve)
        {
            VkImageMemoryBarrier resolveToTransfer = {};
            resolveToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            resolveToTransfer.srcAccessMask = 0;
            resolveToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            resolveToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolveToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            resolveToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolveToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolveToTransfer.image = directPresentImage;
            resolveToTransfer.subresourceRange = colorRange;
            mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         1,
                                         &resolveToTransfer);

            VkImageResolve resolveRegion = {};
            resolveRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolveRegion.srcSubresource.mipLevel = 0;
            resolveRegion.srcSubresource.baseArrayLayer = 0;
            resolveRegion.srcSubresource.layerCount = 1;
            resolveRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolveRegion.dstSubresource.mipLevel = 0;
            resolveRegion.dstSubresource.baseArrayLayer = 0;
            resolveRegion.dstSubresource.layerCount = 1;
            resolveRegion.extent.width = (uint32_t)presentTexture->info.mXres;
            resolveRegion.extent.height = (uint32_t)presentTexture->info.mYres;
            resolveRegion.extent.depth = 1;
            mState->vkCmdResolveImage(mState->commandBuffer,
                                      presentTexture->image,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      directPresentImage,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      1,
                                      &resolveRegion);

            VkImageMemoryBarrier resolveToSource = {};
            resolveToSource.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            resolveToSource.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            resolveToSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            resolveToSource.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            resolveToSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            resolveToSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolveToSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolveToSource.image = directPresentImage;
            resolveToSource.subresourceRange = colorRange;
            mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         1,
                                         &resolveToSource);
        }

        VkImageMemoryBarrier swapchainToTransfer = {};
        swapchainToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapchainToTransfer.srcAccessMask = 0;
        swapchainToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapchainToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchainToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapchainToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchainToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchainToTransfer.image = mState->swapchainImages[imageIndex];
        swapchainToTransfer.subresourceRange = colorRange;
        mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &swapchainToTransfer);

        VkImageBlit blitRegion = {};
        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.mipLevel = 0;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcOffsets[1].x = presentTexture->info.mXres;
        blitRegion.srcOffsets[1].y = presentTexture->info.mYres;
        blitRegion.srcOffsets[1].z = 1;
        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.mipLevel = 0;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstOffsets[1].x = (int32_t)mState->swapchainExtent.width;
        blitRegion.dstOffsets[1].y = (int32_t)mState->swapchainExtent.height;
        blitRegion.dstOffsets[1].z = 1;
        mState->vkCmdBlitImage(mState->commandBuffer,
                               directPresentImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               mState->swapchainImages[imageIndex],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &blitRegion,
                               VK_FILTER_NEAREST);

        VkImageMemoryBarrier barriers[2] = {};
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = presentTexture->image;
        barriers[0].subresourceRange = colorRange;
        barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].image = mState->swapchainImages[imageIndex];
        barriers[1].subresourceRange = colorRange;
        mState->vkCmdPipelineBarrier(mState->commandBuffer,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     2,
                                     barriers);
        presentTexture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else if (copyTexture)
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
        iDestroyPresentScratch(mState, directPresentImage, directPresentImageView, directPresentMemory);
        return;
    }

    VkPipelineStageFlags waitStage = (directPresentTexture || copyTexture) ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
        iDestroyPresentScratch(mState, directPresentImage, directPresentImageView, directPresentMemory);
        return;
    }
    result = mState->vkWaitForFences(mState->device, 1, &mState->frameFence, 1, timeout);
    if (result != VK_SUCCESS)
    {
        iDestroyPresentScratch(mState, directPresentImage, directPresentImageView, directPresentMemory);
        return;
    }
#if defined(WINDOWS)
    const bool captureRequested = iCaptureRequested();
#else
    const bool captureRequested = false;
#endif
    if (directPresentTexture && captureRequested && mState->gpuPaintDrawCount > 0)
    {
        if (!iReadBackTextureImage(mState, presentTexture, mReporter) && !mState->gpuReadbackFailureReported)
        {
            mState->gpuReadbackFailureReported = true;
            iError(mReporter, "Vulkan renderer failed to read back presented GPU target");
        }
        if (mState->gpuReadbackReported)
        {
#if defined(WINDOWS)
            iWritePpmCapture(mState, presentTexture);
#endif
        }
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
        if (mState->vkDeviceWaitIdle)
        {
            mState->vkDeviceWaitIdle(mState->device);
        }
        ++mState->presentFrameIndex;
        if (srgbGpuPresentTexture && presentTextureHasColor && !mState->directTexturePresentReported)
        {
            mState->directTexturePresentReported = true;
            iReport(mReporter, "Vulkan renderer presented swapchain sRGB GPU present frame");
        }
        else if (directPresentTexture && presentTextureHasColor && !mState->directTexturePresentReported)
        {
            mState->directTexturePresentReported = true;
            iReport(mReporter, "Vulkan renderer presented swapchain direct GPU texture frame");
        }
        else if (copyTexture && presentTextureHasColor && !mState->texturePresentReported)
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
    iDestroyPresentScratch(mState, directPresentImage, directPresentImageView, directPresentMemory);
}

void piRendererVulkan::EndExternalImageFrame(void)
{
    if (!mState)
    {
        return;
    }

    const bool wasHostRenderPassFrame = mState->hostRenderPassFrameActive;
    const bool wasExternalCommandBufferFrame = mState->externalCommandBufferFrameActive;
    if (wasHostRenderPassFrame && mState->hostDrawBisectionEnabled && mState->hostDrawBisectionAttempted > 0)
    {
        char message[256];
        std::snprintf(
            message,
            sizeof(message),
            "[IMM_UNITY_VK_DRAW_BISECT_20260731] stage=%u mask=%u attempted=%u admitted=%u frame=%llu",
            mState->hostDrawBisectionStage,
            mState->hostDrawBisectionMask,
            mState->hostDrawBisectionAttempted,
            mState->hostDrawBisectionAdmitted,
            static_cast<unsigned long long>(mState->borrowedCurrentFrameNumber));
        iReport(mReporter, message);
        if (mState->hostDrawBisectionStage < 3)
        {
            ++mState->hostDrawBisectionStage;
        }
    }
    if (mState->externalFrameColorTexture &&
        !mState->externalFramePreservesHostColor &&
        !iTransitionColorTextureToShaderRead(mState, mState->externalFrameColorTexture))
    {
        iError(mReporter, "Vulkan renderer failed to transition external image frame for host sampling");
    }
    if (!wasHostRenderPassFrame)
    {
        SetRenderTarget(nullptr);
    }
    if (mState->externalFrameRenderTarget && !wasExternalCommandBufferFrame)
    {
        DestroyRenderTarget(mState->externalFrameRenderTarget);
    }
    if (mState->externalFrameDepthTexture && !wasExternalCommandBufferFrame)
    {
        DestroyTexture(mState->externalFrameDepthTexture);
    }
    if (mState->externalFrameColorTexture && !wasExternalCommandBufferFrame)
    {
        DestroyTexture(mState->externalFrameColorTexture);
    }
    mState->externalFrameRenderTarget = nullptr;
    mState->externalFrameDepthTexture = nullptr;
    mState->externalFrameColorTexture = nullptr;
    mState->externalFrameUsesHostDepth = false;
    mState->externalFrameHostDepthReverseZ = false;
    mState->externalFramePreservesHostColor = false;
    mState->externalCommandBufferFrameActive = false;
    mState->borrowedFrameResourcesActive = false;
    mState->hostRenderPassFrameActive = false;
    if (wasHostRenderPassFrame || wasExternalCommandBufferFrame)
    {
        mState->commandBuffer = mState->hostPreviousCommandBuffer;
        mState->hostPreviousCommandBuffer = VK_NULL_COMMAND_BUFFER;
        if (wasHostRenderPassFrame)
        {
            SetRenderTarget(mState->hostPreviousRenderTarget);
            mState->hostPreviousRenderTarget = nullptr;
        }
    }
}

bool piRendererVulkan::BeginHostRenderPassFrame(void *commandBuffer, uint64_t currentFrameNumber, uint64_t safeFrameNumber, void *renderPass, void *framebuffer, uint32_t colorVkFormat, uint32_t colorVkSamples, bool hasDepthAttachment, bool useHostDepth, bool hostDepthReverseZ, uint32_t subpass, int width, int height)
{
    EndExternalImageFrame();
    if (!mState || commandBuffer == nullptr || renderPass == nullptr || framebuffer == nullptr || colorVkFormat == 0 || width <= 0 || height <= 0)
    {
        return false;
    }

    iCollectBorrowedPipelines(mState, safeFrameNumber);

    uint32_t frameSlot = kBorrowedFrameSlotCount;
    bool continuingFrame = false;
    for (uint32_t i = 0; i < kBorrowedFrameSlotCount; ++i)
    {
        if (mState->borrowedFrameNumbers[i] == currentFrameNumber)
        {
            frameSlot = i;
            continuingFrame = true;
            break;
        }
    }
    if (!continuingFrame)
    {
        for (uint32_t i = 0; i < kBorrowedFrameSlotCount; ++i)
        {
            const uint64_t slotFrame = mState->borrowedFrameNumbers[i];
            if (slotFrame == ~0ull || slotFrame <= safeFrameNumber)
            {
                frameSlot = i;
                break;
            }
        }
    }
    if (frameSlot >= kBorrowedFrameSlotCount)
    {
        iError(mReporter, "Vulkan renderer has no safe Unity render-pass frame slot");
        return false;
    }

    piTextureS *colorTexture = new piTextureS();
    colorTexture->info = { TextureType::T2D, Format::C4_8_UNORM, width, height, 1, static_cast<int>(colorVkSamples != 0 ? colorVkSamples : VK_SAMPLE_COUNT_1_BIT), 1, 0 };
    colorTexture->filter = TextureFilter::NONE;
    colorTexture->wrap = TextureWrap::CLAMP;
    colorTexture->vkFormat = static_cast<VkFormat>(colorVkFormat);
    colorTexture->sampleCount = static_cast<VkSampleCountFlagBits>(colorVkSamples != 0 ? colorVkSamples : VK_SAMPLE_COUNT_1_BIT);
    ++mState->liveTextures;

    piRTargetS *target = new piRTargetS();
    target->color[0] = colorTexture;
    target->renderPass = static_cast<VkRenderPass>(reinterpret_cast<uintptr_t>(renderPass));
    target->framebuffer = static_cast<VkFramebuffer>(reinterpret_cast<uintptr_t>(framebuffer));
    target->subpass = subpass;
    target->width = static_cast<uint32_t>(width);
    target->height = static_cast<uint32_t>(height);
    target->colorAttachmentCount = 1;
    target->hasDepth = hasDepthAttachment;
    target->ownsRenderPassObjects = false;
    ++mState->liveRenderTargets;

    mState->hostPreviousCommandBuffer = mState->commandBuffer;
    mState->hostPreviousRenderTarget = mState->currentRenderTarget;
    mState->commandBuffer = static_cast<VkCommandBuffer>(commandBuffer);
    mState->externalFrameColorTexture = colorTexture;
    mState->externalFrameRenderTarget = target;
    mState->externalFrameUsesHostDepth = useHostDepth;
    mState->externalFrameHostDepthReverseZ = useHostDepth && hostDepthReverseZ;
    mState->externalFramePreservesHostColor = true;
    mState->borrowedFrameResourcesActive = true;
    mState->hostRenderPassFrameActive = true;
    mState->borrowedFrameSlot = frameSlot;
    mState->borrowedCurrentFrameNumber = currentFrameNumber;
    mState->hostDrawBisectionAttempted = 0;
    mState->hostDrawBisectionAdmitted = 0;
    if (mState->hostDrawBisectionEnabled)
    {
        // Bit 0 admits indexed paint/picture draws; bit 1 admits the
        // independent picture-quad path used by DrawUnitQuad_XY.
        static constexpr uint32_t kStageMasks[] = { 0u, 1u, 2u, 3u };
        mState->hostDrawBisectionMask = kStageMasks[mState->hostDrawBisectionStage];
    }
    if (!continuingFrame)
    {
        mState->borrowedFrameNumbers[frameSlot] = currentFrameNumber;
        mState->borrowedStaticPaintSetCursor = 0;
        mState->borrowedPictureSetCursor = 0;
        mState->hostTransientUniformOffset =
            static_cast<VkDeviceSize>(frameSlot) * kBorrowedUniformBytesPerFrame;
        mState->hostTransientUniformLimit =
            mState->hostTransientUniformOffset + kBorrowedUniformBytesPerFrame;
    }
    SetRenderTarget(target);

    if (!mState->hostRenderPassFrameReported)
    {
        mState->hostRenderPassFrameReported = true;
        iReport(mReporter, useHostDepth ? "Vulkan renderer began host render pass frame with host depth" : "Vulkan renderer began host render pass frame");
    }
    return true;
}

void piRendererVulkan::SetHostDrawBisectionEnabled(bool enabled)
{
    if (!mState)
    {
        return;
    }
    if (enabled && !mState->hostDrawBisectionEnabled)
    {
        mState->hostDrawBisectionStage = 0;
        iReport(mReporter, "[IMM_UNITY_VK_DRAW_BISECT_20260731] enabled stages=none,indexed-only,unit-quad-only,all");
    }
    mState->hostDrawBisectionEnabled = enabled;
}

bool piRendererVulkan::BeginUnityCommandBufferUploadFrame(void *commandBuffer, uint64_t currentFrameNumber, uint64_t safeFrameNumber)
{
    EndExternalImageFrame();
    if (!mState || commandBuffer == nullptr)
    {
        return false;
    }

    iCollectBorrowedUploads(mState, safeFrameNumber);
    iCollectBorrowedPipelines(mState, safeFrameNumber);

    uint32_t frameSlot = kBorrowedFrameSlotCount;
    bool continuingFrame = false;
    for (uint32_t i = 0; i < kBorrowedFrameSlotCount; ++i)
    {
        if (mState->borrowedFrameNumbers[i] == currentFrameNumber)
        {
            frameSlot = i;
            continuingFrame = true;
            break;
        }
    }
    if (!continuingFrame)
    {
        for (uint32_t i = 0; i < kBorrowedFrameSlotCount; ++i)
        {
            const uint64_t slotFrame = mState->borrowedFrameNumbers[i];
            if (slotFrame == ~0ull || slotFrame <= safeFrameNumber)
            {
                frameSlot = i;
                break;
            }
        }
    }
    if (frameSlot >= kBorrowedFrameSlotCount)
    {
        iError(mReporter, "Vulkan renderer has no safe Unity upload frame slot");
        return false;
    }

    mState->hostPreviousCommandBuffer = mState->commandBuffer;
    mState->commandBuffer = static_cast<VkCommandBuffer>(commandBuffer);
    mState->externalCommandBufferFrameActive = true;
    mState->borrowedFrameResourcesActive = true;
    mState->borrowedFrameSlot = frameSlot;
    mState->borrowedCurrentFrameNumber = currentFrameNumber;
    // RenderPreparedCamera updates per-draw constants while Unity's render pass
    // is active. Allocate and map the backing ring here, while the prepare event
    // is explicitly outside the render pass, so the inside-pass callback only
    // writes existing mapped memory and records graphics commands.
    if (!iEnsureHostTransientUniformBuffer(mState, kBorrowedUniformTotalBytes, mReporter))
    {
        iError(mReporter, "[IMM_UNITY_VK_DRAW_BISECT_20260731] failed to preallocate host transient uniforms outside render pass");
        EndExternalImageFrame();
        return false;
    }
    if (!continuingFrame)
    {
        mState->borrowedFrameNumbers[frameSlot] = currentFrameNumber;
        mState->borrowedStaticPaintSetCursor = 0;
        mState->borrowedPictureSetCursor = 0;
        mState->hostTransientUniformOffset =
            static_cast<VkDeviceSize>(frameSlot) * kBorrowedUniformBytesPerFrame;
        mState->hostTransientUniformLimit =
            mState->hostTransientUniformOffset + kBorrowedUniformBytesPerFrame;
    }
    return true;
}

bool piRendererVulkan::DebugClearHostRenderPassColor(float red, float green, float blue, float alpha)
{
    if (!mState || !mState->hostRenderPassFrameActive || mState->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        !mState->externalFrameRenderTarget || !mState->vkCmdClearAttachments)
    {
        return false;
    }

    VkClearAttachment attachment = {};
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = 0;
    attachment.clearValue.color.float32[0] = red;
    attachment.clearValue.color.float32[1] = green;
    attachment.clearValue.color.float32[2] = blue;
    attachment.clearValue.color.float32[3] = alpha;

    VkClearRect rect = {};
    rect.rect.offset.x = 0;
    rect.rect.offset.y = 0;
    rect.rect.extent.width = mState->externalFrameRenderTarget->width;
    rect.rect.extent.height = mState->externalFrameRenderTarget->height;
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;

    mState->vkCmdClearAttachments(mState->commandBuffer, 1, &attachment, 1, &rect);
    return true;
}

bool piRendererVulkan::DebugDrawHostRenderPassTriangle(void)
{
    if (!mState || !mState->hostRenderPassFrameActive || mState->commandBuffer == VK_NULL_COMMAND_BUFFER ||
        !mState->externalFrameRenderTarget || !mState->vkCreatePipelineLayout || !mState->vkCreateGraphicsPipelines ||
        !mState->vkCmdBindPipeline || !mState->vkCmdDraw)
    {
        return false;
    }

    if (mState->hostDebugTrianglePipelineLayout == VK_NULL_PIPELINE_LAYOUT)
    {
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        if (mState->vkCreatePipelineLayout(mState->device, &layoutInfo, nullptr, &mState->hostDebugTrianglePipelineLayout) != VK_SUCCESS ||
            mState->hostDebugTrianglePipelineLayout == VK_NULL_PIPELINE_LAYOUT)
        {
            iError(mReporter, "[IMM_UNITY_VK_TRIANGLE_CONTROL_20260731] failed to create pipeline layout");
            return false;
        }
    }
    if (mState->hostDebugTriangleVertexModule == VK_NULL_SHADER_MODULE &&
        !iCreateShaderModule(mState, reinterpret_cast<const uint8_t *>(kSrgbPresentVS), static_cast<int>(sizeof(kSrgbPresentVS)), &mState->hostDebugTriangleVertexModule, mReporter))
    {
        iError(mReporter, "[IMM_UNITY_VK_TRIANGLE_CONTROL_20260731] failed to create vertex module");
        return false;
    }
    if (mState->hostDebugTriangleFragmentModule == VK_NULL_SHADER_MODULE &&
        !iCreateShaderModule(mState, reinterpret_cast<const uint8_t *>(kHostDebugTriangleFS), static_cast<int>(sizeof(kHostDebugTriangleFS)), &mState->hostDebugTriangleFragmentModule, mReporter))
    {
        iError(mReporter, "[IMM_UNITY_VK_TRIANGLE_CONTROL_20260731] failed to create fragment module");
        return false;
    }

    const piRTarget target = mState->externalFrameRenderTarget;
    const VkSampleCountFlagBits sampleCount = target->color[0] ? target->color[0]->sampleCount : VK_SAMPLE_COUNT_1_BIT;
    if (mState->hostDebugTrianglePipeline == VK_NULL_PIPELINE ||
        mState->hostDebugTriangleRenderPass != target->renderPass ||
        mState->hostDebugTriangleSubpass != target->subpass ||
        mState->hostDebugTriangleSampleCount != sampleCount)
    {
        if (mState->hostDebugTrianglePipeline != VK_NULL_PIPELINE)
        {
            iRetireGraphicsPipeline(mState, mState->hostDebugTrianglePipeline);
            mState->hostDebugTrianglePipeline = VK_NULL_PIPELINE;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = mState->hostDebugTriangleVertexModule;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = mState->hostDebugTriangleFragmentModule;
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterization = {};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = sampleCount;
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = 0;
        depthStencil.depthWriteEnable = 0;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend = {};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blendAttachment;
        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = target->hasDepth ? &depthStencil : nullptr;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = mState->hostDebugTrianglePipelineLayout;
        pipelineInfo.renderPass = target->renderPass;
        pipelineInfo.subpass = target->subpass;
        const VkResult result = mState->vkCreateGraphicsPipelines(
            mState->device, VK_NULL_PIPELINE_CACHE, 1, &pipelineInfo, nullptr, &mState->hostDebugTrianglePipeline);
        if (result != VK_SUCCESS || mState->hostDebugTrianglePipeline == VK_NULL_PIPELINE)
        {
            iError(mReporter, "[IMM_UNITY_VK_TRIANGLE_CONTROL_20260731] failed to create graphics pipeline");
            mState->hostDebugTrianglePipeline = VK_NULL_PIPELINE;
            return false;
        }
        mState->hostDebugTriangleRenderPass = target->renderPass;
        mState->hostDebugTriangleSubpass = target->subpass;
        mState->hostDebugTriangleSampleCount = sampleCount;
        iReport(mReporter, "[IMM_UNITY_VK_TRIANGLE_CONTROL_20260731] created graphics pipeline");
    }

    mState->vkCmdBindPipeline(mState->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mState->hostDebugTrianglePipeline);
    mState->vkCmdDraw(mState->commandBuffer, 3, 1, 0, 0);
    return true;
}

bool piRendererVulkan::DebugReadbackExternalFrameColor(uint8_t rgba[4])
{
    if (!mState || !rgba || !mState->externalFrameColorTexture ||
        !mState->externalFrameColorTexture->data)
    {
        return false;
    }
    if (!iReadBackTextureImage(mState, mState->externalFrameColorTexture, mReporter))
    {
        return false;
    }
    std::memcpy(rgba, mState->externalFrameColorTexture->data, 4);
    return true;
}

bool piRendererVulkan::BeginExternalImageFrame(void *image, uint32_t vkFormat, int width, int height, int arrayLayers)
{
    EndExternalImageFrame();
    if (!mState || image == nullptr || width <= 0 || height <= 0 || arrayLayers <= 0 || vkFormat == 0 || !mState->vkCreateImageView)
    {
        return false;
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(image));
    viewInfo.viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = static_cast<VkFormat>(vkFormat);
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = static_cast<uint32_t>(arrayLayers);

    VkImageView imageView = VK_NULL_IMAGE_VIEW;
    VkResult result = mState->vkCreateImageView(mState->device, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS || imageView == VK_NULL_IMAGE_VIEW)
    {
        iError(mReporter, "Vulkan renderer failed to create external image view");
        return false;
    }

    if (!BeginExternalImageFrameWithView(image, reinterpret_cast<void *>(imageView), vkFormat, VK_SAMPLE_COUNT_1_BIT, nullptr, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, width, height, arrayLayers, false, false, true, false, false))
    {
        mState->vkDestroyImageView(mState->device, imageView, nullptr);
        return false;
    }
    if (mState->externalFrameColorTexture)
    {
        mState->externalFrameColorTexture->ownsImageView = true;
    }

    iReport(mReporter, "Vulkan renderer began external image frame with owned image view");
    return true;
}

bool piRendererVulkan::BeginExternalImageFrame(void *image, void *imageView, uint32_t vkFormat, int width, int height)
{
    return BeginExternalImageFrameWithView(image, imageView, vkFormat, VK_SAMPLE_COUNT_1_BIT, nullptr, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, width, height, 1, false, false, true, false, false);
}

bool piRendererVulkan::BeginExternalImageFrame(void *image, void *imageView, uint32_t vkFormat, void *depthImage, void *depthImageView, uint32_t depthVkFormat, int width, int height, bool clearExternalDepth)
{
    return BeginExternalImageFrameWithView(image, imageView, vkFormat, VK_SAMPLE_COUNT_1_BIT, depthImage, depthImageView, depthVkFormat, VK_SAMPLE_COUNT_1_BIT, width, height, 1, false, false, clearExternalDepth, clearExternalDepth, false);
}

bool piRendererVulkan::BeginExternalImageFramePreserveColor(void *image, uint32_t vkFormat, uint32_t colorVkSamples, void *depthImage, uint32_t depthVkFormat, uint32_t depthVkSamples, int width, int height, bool hostDepthReverseZ)
{
    EndExternalImageFrame();
    if (!mState || image == nullptr || width <= 0 || height <= 0 || vkFormat == 0 || !mState->vkCreateImageView)
    {
        return false;
    }

    VkImageViewCreateInfo colorViewInfo = {};
    colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(image));
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = static_cast<VkFormat>(vkFormat);
    colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;

    VkImageView colorImageView = VK_NULL_IMAGE_VIEW;
    VkResult result = mState->vkCreateImageView(mState->device, &colorViewInfo, nullptr, &colorImageView);
    if (result != VK_SUCCESS || colorImageView == VK_NULL_IMAGE_VIEW)
    {
        iError(mReporter, "Vulkan renderer failed to create Unity external color image view");
        return false;
    }

    VkImageView depthImageView = VK_NULL_IMAGE_VIEW;
    const bool useHostDepth = depthImage != nullptr && depthVkFormat != 0 && depthVkSamples == colorVkSamples;
    if (depthImage != nullptr && depthVkFormat != 0 && depthVkSamples != colorVkSamples)
    {
        iError(mReporter, "Vulkan renderer skipping Unity external depth image because color/depth sample counts differ");
    }
    if (useHostDepth)
    {
        VkImageViewCreateInfo depthViewInfo = colorViewInfo;
        depthViewInfo.image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(depthImage));
        depthViewInfo.format = static_cast<VkFormat>(depthVkFormat);
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        result = mState->vkCreateImageView(mState->device, &depthViewInfo, nullptr, &depthImageView);
        if (result != VK_SUCCESS || depthImageView == VK_NULL_IMAGE_VIEW)
        {
            mState->vkDestroyImageView(mState->device, colorImageView, nullptr);
            iError(mReporter, "Vulkan renderer failed to create Unity external depth image view");
            return false;
        }
    }

    if (!BeginExternalImageFrameWithView(image, reinterpret_cast<void *>(colorImageView), vkFormat, colorVkSamples, useHostDepth ? depthImage : nullptr, useHostDepth ? reinterpret_cast<void *>(depthImageView) : nullptr, useHostDepth ? depthVkFormat : 0, useHostDepth ? depthVkSamples : VK_SAMPLE_COUNT_1_BIT, width, height, 1, true, useHostDepth, false, false, hostDepthReverseZ))
    {
        if (depthImageView != VK_NULL_IMAGE_VIEW)
        {
            mState->vkDestroyImageView(mState->device, depthImageView, nullptr);
        }
        mState->vkDestroyImageView(mState->device, colorImageView, nullptr);
        return false;
    }

    iReport(mReporter, useHostDepth ? "Vulkan renderer began external image frame preserving host color with host depth" : "Vulkan renderer began external image frame preserving host color without host depth");
    return true;
}

bool piRendererVulkan::BeginExternalImageCommandBufferFramePreserveColor(void *commandBuffer, uint64_t currentFrameNumber, uint64_t safeFrameNumber, void *image, uint32_t vkFormat, uint32_t colorVkSamples, void *depthImage, uint32_t depthVkFormat, uint32_t depthVkSamples, int width, int height, bool hostDepthReverseZ)
{
    EndExternalImageFrame();
    if (!mState || commandBuffer == nullptr || image == nullptr || depthImage == nullptr ||
        vkFormat == 0 || depthVkFormat == 0 || width <= 0 || height <= 0)
    {
        return false;
    }

    iCollectBorrowedPipelines(mState, safeFrameNumber);

    uint32_t frameSlot = kBorrowedFrameSlotCount;
    bool continuingFrame = false;
    for (uint32_t i = 0; i < kBorrowedFrameSlotCount; ++i)
    {
        if (mState->borrowedFrameNumbers[i] == currentFrameNumber)
        {
            frameSlot = i;
            continuingFrame = true;
            break;
        }
    }
    if (!continuingFrame)
    {
        for (uint32_t i = 0; i < kBorrowedFrameSlotCount; ++i)
        {
            const uint64_t slotFrame = mState->borrowedFrameNumbers[i];
            if (slotFrame == ~0ull || slotFrame <= safeFrameNumber)
            {
                frameSlot = i;
                break;
            }
        }
    }
    if (frameSlot >= kBorrowedFrameSlotCount)
    {
        iError(mReporter, "Vulkan renderer has no safe borrowed command-buffer frame slot");
        return false;
    }

    const VkSampleCountFlagBits requestedColorSamples =
        static_cast<VkSampleCountFlagBits>(colorVkSamples != 0 ? colorVkSamples : VK_SAMPLE_COUNT_1_BIT);
    const VkSampleCountFlagBits requestedDepthSamples =
        static_cast<VkSampleCountFlagBits>(depthVkSamples != 0 ? depthVkSamples : VK_SAMPLE_COUNT_1_BIT);
    const bool cacheMatches =
        mState->borrowedExternalColorTexture &&
        mState->borrowedExternalDepthTexture &&
        mState->borrowedExternalRenderTarget &&
        mState->borrowedExternalColorTexture->externalHandle == reinterpret_cast<uint64_t>(image) &&
        mState->borrowedExternalColorTexture->vkFormat == static_cast<VkFormat>(vkFormat) &&
        mState->borrowedExternalColorTexture->sampleCount == requestedColorSamples &&
        mState->borrowedExternalColorTexture->info.mXres == width &&
        mState->borrowedExternalColorTexture->info.mYres == height &&
        mState->borrowedExternalDepthTexture->externalHandle == reinterpret_cast<uint64_t>(depthImage) &&
        mState->borrowedExternalDepthTexture->vkFormat == static_cast<VkFormat>(depthVkFormat) &&
        mState->borrowedExternalDepthTexture->sampleCount == requestedDepthSamples &&
        mState->borrowedExternalDepthTexture->info.mXres == width &&
        mState->borrowedExternalDepthTexture->info.mYres == height;

    if (!cacheMatches)
    {
        // Unity can recreate Android RenderTextures after a resolution change
        // or app resume. The cached framebuffer and image views must outlive
        // every Unity command buffer that references them.
        if ((mState->borrowedExternalRenderTarget ||
             mState->borrowedExternalDepthTexture ||
             mState->borrowedExternalColorTexture) &&
            mState->vkDeviceWaitIdle)
        {
            mState->vkDeviceWaitIdle(mState->device);
        }
        if (mState->borrowedExternalRenderTarget)
        {
            DestroyRenderTarget(mState->borrowedExternalRenderTarget);
            mState->borrowedExternalRenderTarget = nullptr;
        }
        if (mState->borrowedExternalDepthTexture)
        {
            DestroyTexture(mState->borrowedExternalDepthTexture);
            mState->borrowedExternalDepthTexture = nullptr;
        }
        if (mState->borrowedExternalColorTexture)
        {
            DestroyTexture(mState->borrowedExternalColorTexture);
            mState->borrowedExternalColorTexture = nullptr;
        }

        if (!BeginExternalImageFramePreserveColor(
                image,
                vkFormat,
                colorVkSamples,
                depthImage,
                depthVkFormat,
                depthVkSamples,
                width,
                height,
                hostDepthReverseZ))
        {
            return false;
        }
        mState->borrowedExternalColorTexture = mState->externalFrameColorTexture;
        mState->borrowedExternalDepthTexture = mState->externalFrameDepthTexture;
        mState->borrowedExternalRenderTarget = mState->externalFrameRenderTarget;
    }
    else
    {
        mState->externalFrameColorTexture = mState->borrowedExternalColorTexture;
        mState->externalFrameDepthTexture = mState->borrowedExternalDepthTexture;
        mState->externalFrameRenderTarget = mState->borrowedExternalRenderTarget;
        mState->externalFrameUsesHostDepth = true;
        mState->externalFrameHostDepthReverseZ = hostDepthReverseZ;
        mState->externalFramePreservesHostColor = true;
        SetRenderTarget(mState->externalFrameRenderTarget);
    }

    VkCommandBuffer previousCommandBuffer = mState->commandBuffer;
    mState->hostPreviousCommandBuffer = previousCommandBuffer;
    mState->commandBuffer = static_cast<VkCommandBuffer>(commandBuffer);
    mState->externalCommandBufferFrameActive = true;
    mState->borrowedFrameResourcesActive = true;
    mState->borrowedFrameSlot = frameSlot;
    mState->borrowedCurrentFrameNumber = currentFrameNumber;
    if (!continuingFrame)
    {
        mState->borrowedFrameNumbers[frameSlot] = currentFrameNumber;
        mState->borrowedStaticPaintSetCursor = 0;
        mState->borrowedPictureSetCursor = 0;
        mState->hostTransientUniformOffset =
            static_cast<VkDeviceSize>(frameSlot) * kBorrowedUniformBytesPerFrame;
        mState->hostTransientUniformLimit =
            mState->hostTransientUniformOffset + kBorrowedUniformBytesPerFrame;
    }
    if (!mState->hostRenderPassFrameReported)
    {
        mState->hostRenderPassFrameReported = true;
        iReport(mReporter, "Vulkan renderer began Unity-owned command buffer frame with IMM render pass");
    }
    return true;
}

bool piRendererVulkan::BeginExternalImageFrameWithView(void *image, void *imageView, uint32_t vkFormat, uint32_t colorVkSamples, void *depthImage, void *depthImageView, uint32_t depthVkFormat, uint32_t depthVkSamples, int width, int height, int arrayLayers, bool ownsColorImageView, bool ownsDepthImageView, bool clearColor, bool clearExternalDepth, bool hostDepthReverseZ)
{
    EndExternalImageFrame();
    if (!mState || image == nullptr || imageView == nullptr || width <= 0 || height <= 0 || arrayLayers <= 0 || vkFormat == 0)
    {
        return false;
    }

    piTextureS *colorTexture = new piTextureS();
    colorTexture->info = { TextureType::T2D, Format::C4_8_UNORM, width, height, arrayLayers, 1, 1, 0 };
    colorTexture->filter = TextureFilter::NONE;
    colorTexture->wrap = TextureWrap::CLAMP;
    colorTexture->dataSize = (size_t)width * (size_t)height * (size_t)arrayLayers * 4u;
    colorTexture->data = (uint8_t *)std::calloc(1, colorTexture->dataSize);
    colorTexture->externalHandle = reinterpret_cast<uint64_t>(image);
    colorTexture->image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(image));
    colorTexture->imageView = static_cast<VkImageView>(reinterpret_cast<uintptr_t>(imageView));
    colorTexture->ownsImageView = ownsColorImageView;
    colorTexture->vkFormat = static_cast<VkFormat>(vkFormat);
    colorTexture->imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    colorTexture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorTexture->sampleCount = static_cast<VkSampleCountFlagBits>(colorVkSamples != 0 ? colorVkSamples : VK_SAMPLE_COUNT_1_BIT);
    if (!colorTexture->data)
    {
        delete colorTexture;
        return false;
    }
    ++mState->liveTextures;

    const bool hasExternalDepth = depthImage != nullptr && depthImageView != nullptr && depthVkFormat != 0;
    piTexture depthTexture = nullptr;
    if (hasExternalDepth)
    {
        piTextureS *externalDepthTexture = new piTextureS();
        externalDepthTexture->info = { TextureType::T2D, Format::D1_32_FLOAT, width, height, 1, 1, 1, 0 };
        externalDepthTexture->filter = TextureFilter::NONE;
        externalDepthTexture->wrap = TextureWrap::CLAMP;
        externalDepthTexture->dataSize = (size_t)width * (size_t)height * 4u;
        externalDepthTexture->data = (uint8_t *)std::calloc(1, externalDepthTexture->dataSize);
        externalDepthTexture->externalHandle = reinterpret_cast<uint64_t>(depthImage);
        externalDepthTexture->image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(depthImage));
        externalDepthTexture->imageView = static_cast<VkImageView>(reinterpret_cast<uintptr_t>(depthImageView));
        externalDepthTexture->ownsImageView = ownsDepthImageView;
        externalDepthTexture->vkFormat = static_cast<VkFormat>(depthVkFormat);
        externalDepthTexture->imageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        externalDepthTexture->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        externalDepthTexture->sampleCount = static_cast<VkSampleCountFlagBits>(depthVkSamples != 0 ? depthVkSamples : VK_SAMPLE_COUNT_1_BIT);
        if (!externalDepthTexture->data)
        {
            DestroyTexture(colorTexture);
            delete externalDepthTexture;
            return false;
        }
        ++mState->liveTextures;
        depthTexture = externalDepthTexture;
    }
    else
    {
        const TextureInfo depthInfo = { TextureType::T2D, Format::D1_32_FLOAT, width, height, 1, static_cast<int>(colorTexture->sampleCount), 1, 0 };
        depthTexture = CreateTexture(L"imm_external_image_depth", &depthInfo, false, TextureFilter::NONE, TextureWrap::CLAMP, 1.0f, nullptr);
    }
    if (!depthTexture)
    {
        DestroyTexture(colorTexture);
        return false;
    }

    piRTarget renderTarget = CreateRenderTarget(colorTexture, nullptr, nullptr, nullptr, depthTexture);
    if (!renderTarget || !SetRenderTarget(renderTarget))
    {
        if (renderTarget) DestroyRenderTarget(renderTarget);
        DestroyTexture(depthTexture);
        DestroyTexture(colorTexture);
        return false;
    }
    const float transparentBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (clearColor && !iClearColorTextureImage(mState, colorTexture, transparentBlack, mReporter))
    {
        DestroyRenderTarget(renderTarget);
        DestroyTexture(depthTexture);
        DestroyTexture(colorTexture);
        return false;
    }
    if ((!hasExternalDepth || clearExternalDepth) && !iClearDepthTextureImage(mState, depthTexture, mReporter))
    {
        DestroyRenderTarget(renderTarget);
        DestroyTexture(depthTexture);
        DestroyTexture(colorTexture);
        return false;
    }

    mState->externalFrameColorTexture = colorTexture;
    mState->externalFrameDepthTexture = depthTexture;
    mState->externalFrameRenderTarget = renderTarget;
    mState->externalFrameUsesHostDepth = hasExternalDepth && !clearExternalDepth;
    mState->externalFrameHostDepthReverseZ = mState->externalFrameUsesHostDepth && hostDepthReverseZ;
    mState->externalFramePreservesHostColor = !clearColor;
    iReport(mReporter, hasExternalDepth ? (clearExternalDepth ? "Vulkan renderer began external image frame with owned external depth" : "Vulkan renderer began external image frame with host depth") : "Vulkan renderer began external image frame");
    return true;
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
    if (mState && mState->device != VK_NULL_DEVICE && obj->ownsRenderPassObjects)
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
void piRendererVulkan::SetWriteMask(bool c0, bool c1, bool c2, bool c3, bool z) { (void)c0; (void)c1; (void)c2; (void)c3; if (mState) mState->depthWriteEnabled = z; }
void piRendererVulkan::SetShadingSamples(int shadingSamples) { (void)shadingSamples; }
void piRendererVulkan::RenderTargetGetDefaultSampleLocation(piRTarget vdst, const int id, float *location) { (void)vdst; (void)id; if (location) { location[0] = 0.5f; location[1] = 0.5f; } }
void piRendererVulkan::Clear(const float *color0, const float *color1, const float *color2, const float *color3, const bool depth0)
{
    (void)color1;
    (void)color2;
    (void)color3;
    if (!mState || !mState->currentRenderTarget)
    {
        return;
    }
    if (depth0 && mState->currentRenderTarget->depth &&
        !iClearDepthTextureImage(mState, mState->currentRenderTarget->depth, mReporter))
    {
        iError(mReporter, "Vulkan renderer failed to clear GPU depth render target");
    }
    if (!mState->currentRenderTarget->color[0] || !mState->currentRenderTarget->color[0]->data)
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
    if (!iClearColorTextureImage(mState, texture, color0, mReporter))
    {
        iError(mReporter, "Vulkan renderer failed to clear GPU color render target");
    }
}
void piRendererVulkan::SetState(piState state, bool value)
{
    if (!mState)
    {
        return;
    }
    if (state == piSTATE_DEPTH_TEST)
    {
        mState->depthTestEnabled = value;
    }
}
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
piDepthState piRendererVulkan::CreateDepthState(bool depthEnable, bool lessEqual) { piDepthStateS *state = new piDepthStateS(); state->depthEnable = depthEnable; state->lessEqual = lessEqual; if (mState) ++mState->liveDepthStates; return state; }
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
    if (mState && !iCreateSamplerObject(mState, filter, wrap1, aniso, &texture->sampler, mReporter))
    {
        if (texture->imageView != VK_NULL_IMAGE_VIEW && mState->vkDestroyImageView)
        {
            mState->vkDestroyImageView(mState->device, texture->imageView, nullptr);
        }
        if (texture->image != 0 && mState->vkDestroyImage)
        {
            mState->vkDestroyImage(mState->device, texture->image, nullptr);
        }
        if (texture->memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
        {
            mState->vkFreeMemory(mState->device, texture->memory, nullptr);
        }
        std::free(texture->data);
        delete texture;
        return nullptr;
    }
    if (mState && buffer && !iUploadTextureImageData(mState, texture, mReporter))
    {
        if (texture->sampler != VK_NULL_SAMPLER && mState->vkDestroySampler)
        {
            mState->vkDestroySampler(mState->device, texture->sampler, nullptr);
        }
        if (texture->imageView != VK_NULL_IMAGE_VIEW && mState->vkDestroyImageView)
        {
            mState->vkDestroyImageView(mState->device, texture->imageView, nullptr);
        }
        if (texture->image != 0 && mState->vkDestroyImage)
        {
            mState->vkDestroyImage(mState->device, texture->image, nullptr);
        }
        if (texture->memory != VK_NULL_DEVICE_MEMORY && mState->vkFreeMemory)
        {
            mState->vkFreeMemory(mState->device, texture->memory, nullptr);
        }
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
        if (obj->sampler != VK_NULL_SAMPLER && mState->vkDestroySampler)
        {
            mState->vkDestroySampler(mState->device, obj->sampler, nullptr);
            obj->sampler = VK_NULL_SAMPLER;
        }
        const bool ownsVulkanImage = obj->externalHandle == 0;
        if ((ownsVulkanImage || obj->ownsImageView) && obj->imageView != VK_NULL_IMAGE_VIEW && mState->vkDestroyImageView)
        {
            mState->vkDestroyImageView(mState->device, obj->imageView, nullptr);
            obj->imageView = VK_NULL_IMAGE_VIEW;
        }
        if (ownsVulkanImage && obj->image != 0 && mState->vkDestroyImage)
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
void piRendererVulkan::GetTextureContent(piTexture me, void *data, const Format fmt)
{
    if (!me || !data)
    {
        return;
    }
    if (me->image != 0 && mState && mState->gpuPaintDrawCount > 0 && !iReadBackTextureImage(mState, me, mReporter))
    {
        iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::TextureReadback, "Vulkan texture GPU readback failed");
        return;
    }
    if (!me->data)
    {
        iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::TextureReadback, "Vulkan texture GPU readback is not implemented yet");
        return;
    }
    if (fmt == Format::C3_11_11_10_FLOAT && me->info.mFormat == Format::C3_11_11_10_FLOAT)
    {
        const size_t pixelCount = (size_t)me->info.mXres * (size_t)me->info.mYres;
        uint32_t *dst = (uint32_t *)data;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            dst[i] = iPackRgba8ToB10G11R11(me->data + i * 4u);
        }
        return;
    }
    std::memcpy(data, me->data, me->dataSize);
}
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

piSampler piRendererVulkan::CreateSampler(TextureFilter filter, TextureWrap wrap, float anisotropy)
{
    piSamplerS *sampler = new piSamplerS();
    sampler->filter = filter;
    sampler->wrap = wrap;
    sampler->anisotropy = anisotropy;
    if (mState && !iCreateSamplerObject(mState, filter, wrap, anisotropy, &sampler->sampler, mReporter))
    {
        delete sampler;
        return nullptr;
    }
    if (mState) ++mState->liveSamplers;
    return sampler;
}
void piRendererVulkan::DestroySampler(piSampler obj)
{
    if (!obj) return;
    if (mState && mState->device != VK_NULL_DEVICE && obj->sampler != VK_NULL_SAMPLER && mState->vkDestroySampler)
    {
        mState->vkDestroySampler(mState->device, obj->sampler, nullptr);
        obj->sampler = VK_NULL_SAMPLER;
    }
    if (mState && mState->liveSamplers > 0) --mState->liveSamplers;
    delete obj;
}
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
piShader piRendererVulkan::CreateShaderBinary(const piShaderOptions *options, const uint8_t *vs, const int vs_len, const uint8_t *cs, const int cs_len, const uint8_t *es, const int es_len, const uint8_t *gs, const int gs_len, const uint8_t *fs, const int fs_len, char *error)
{
    (void)cs; (void)cs_len; (void)es; (void)es_len; (void)gs; (void)gs_len;
    piShaderS *shader = new piShaderS();
    shader->vs = vs;
    shader->vsLen = vs_len;
    shader->fs = fs;
    shader->fsLen = fs_len;
    if (options)
    {
        shader->options = *options;
        shader->hasOptions = true;
        shader->isPicture = iShaderOption(shader, "PICTURE", 0) != 0;
        shader->isPicture2D = iShaderOption(shader, "PICTURE_2D", 0) != 0;
    }
    if (!iCreateShaderModule(mState, vs, vs_len, &shader->vertexModule, mReporter) ||
        !iCreateShaderModule(mState, fs, fs_len, &shader->fragmentModule, mReporter))
    {
        if (shader->vertexModule != VK_NULL_SHADER_MODULE && mState && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, shader->vertexModule, nullptr);
        }
        if (shader->fragmentModule != VK_NULL_SHADER_MODULE && mState && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, shader->fragmentModule, nullptr);
        }
        delete shader;
        return nullptr;
    }
    const bool hasShaderModules = shader->vertexModule != VK_NULL_SHADER_MODULE || shader->fragmentModule != VK_NULL_SHADER_MODULE;
    const bool hasPipelineLayout = shader->isPicture ?
        iEnsurePicturePipelineLayout(mState, mReporter) :
        iEnsureStaticPaintPipelineLayout(mState, mReporter);
    if (hasShaderModules && !hasPipelineLayout)
    {
        if (shader->vertexModule != VK_NULL_SHADER_MODULE && mState && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, shader->vertexModule, nullptr);
        }
        if (shader->fragmentModule != VK_NULL_SHADER_MODULE && mState && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, shader->fragmentModule, nullptr);
        }
        delete shader;
        return nullptr;
    }
    if (mState)
    {
        shader->pipelineLayout = shader->isPicture ? mState->picturePipelineLayout : mState->staticPaintPipelineLayout;
    }
    if (error) error[0] = 0;
    if (mState && !mState->shaderModuleReported &&
        (shader->vertexModule != VK_NULL_SHADER_MODULE || shader->fragmentModule != VK_NULL_SHADER_MODULE))
    {
        iReport(mReporter, "Vulkan renderer created SPIR-V shader modules");
        mState->shaderModuleReported = true;
    }
    if (mState) ++mState->liveShaders;
    return shader;
}
void piRendererVulkan::DestroyShader(piShader obj)
{
    if (!obj) return;
    if (mState && mState->device != VK_NULL_DEVICE)
    {
        if (obj->pipeline != VK_NULL_PIPELINE && mState->vkDestroyPipeline)
        {
            mState->vkDestroyPipeline(mState->device, obj->pipeline, nullptr);
            obj->pipeline = VK_NULL_PIPELINE;
            obj->pipelineRenderPass = VK_NULL_RENDER_PASS;
        }
        if (obj->vertexModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, obj->vertexModule, nullptr);
            obj->vertexModule = VK_NULL_SHADER_MODULE;
        }
        if (obj->fragmentModule != VK_NULL_SHADER_MODULE && mState->vkDestroyShaderModule)
        {
            mState->vkDestroyShaderModule(mState->device, obj->fragmentModule, nullptr);
            obj->fragmentModule = VK_NULL_SHADER_MODULE;
        }
    }
    if (mState && mState->liveShaders > 0) --mState->liveShaders;
    delete obj;
}
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
    if (mState && (mState->hostRenderPassFrameActive || mState->externalCommandBufferFrameActive) && obj->use == BufferUse::Constant && offset == 0)
    {
        if (iAllocateHostTransientUniformSlice(mState, obj, obj->data, obj->size, mReporter))
        {
            return;
        }
    }
    if (mState)
    {
        iUploadBufferData(mState, obj, data, (unsigned int)offset, (unsigned int)len, mReporter);
        obj->descriptorBuffer = obj->buffer;
        obj->descriptorOffset = 0;
    }
}
void piRendererVulkan::AttachPixelPackBuffer(piBuffer obj) { (void)obj; iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::PixelPackBuffer, "Vulkan pixel pack buffers are not implemented yet"); }
void piRendererVulkan::DettachPixelPackBuffer(void) { iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::PixelPackBuffer, "Vulkan pixel pack buffers are not implemented yet"); }

piVertexArray piRendererVulkan::CreateVertexArray(int numStreams, piBuffer vb0, const piRArrayLayout *streamLayout0, piBuffer vb1, const piRArrayLayout *streamLayout1, piBuffer eb, const IndexArrayFormat ebFormat)
{
    piVertexArrayS *vertexArray = new piVertexArrayS();
    vertexArray->vertexBuffer[0] = vb0;
    vertexArray->vertexBuffer[1] = vb1;
    vertexArray->indexBuffer = eb;
    vertexArray->indexFormat = ebFormat;
    const piRArrayLayout *layouts[2] = { streamLayout0, streamLayout1 };
    uint32_t attribute = 0;
    for (int stream = 0; stream < numStreams && stream < 2; ++stream)
    {
        const piRArrayLayout *layout = layouts[stream];
        if (!layout)
        {
            continue;
        }
        vertexArray->stride[stream] = (uint32_t)layout->mStride;
        uint32_t offset = 0;
        for (int entry = 0; entry < layout->mNumElements && attribute < 12; ++entry)
        {
            VkFormat format = 0;
            uint32_t size = 0;
            if (layout->mEntry[entry].mType == piRArrayType_Float)
            {
                switch (layout->mEntry[entry].mNumComponents)
                {
                    case 1: format = VK_FORMAT_R32_SFLOAT; size = 4; break;
                    case 2: format = VK_FORMAT_R32G32_SFLOAT; size = 8; break;
                    case 3: format = VK_FORMAT_R32G32B32_SFLOAT; size = 12; break;
                    case 4: format = VK_FORMAT_R32G32B32A32_SFLOAT; size = 16; break;
                    default: break;
                }
            }
            if (format != 0 && size != 0)
            {
                VkVertexInputAttributeDescription *desc = &vertexArray->attributes[attribute];
                desc->location = attribute;
                desc->binding = (uint32_t)stream;
                desc->format = format;
                desc->offset = offset;
                ++attribute;
            }
            offset += size;
        }
    }
    vertexArray->attributeCount = attribute;
    if (mState) ++mState->liveVertexArrays;
    return vertexArray;
}
void piRendererVulkan::DestroyVertexArray(piVertexArray obj) { if (!obj) return; if (mState && mState->liveVertexArrays > 0) --mState->liveVertexArrays; delete obj; }
void piRendererVulkan::AttachVertexArray(piVertexArray obj) { if (mState) mState->currentVertexArray = obj; }
void piRendererVulkan::DettachVertexArray(void) { if (mState) mState->currentVertexArray = nullptr; }
piVertexArray piRendererVulkan::CreateVertexArray2(int numStreams, piBuffer vb0, const ArrayLayout2 *streamLayout0, piBuffer vb1, const ArrayLayout2 *streamLayout1, const void *shaderBinary, size_t shaderBinarySize, piBuffer ib, const IndexArrayFormat ebFormat)
{
    (void)shaderBinary;
    (void)shaderBinarySize;
    piVertexArrayS *vertexArray = new piVertexArrayS();
    vertexArray->vertexBuffer[0] = vb0;
    vertexArray->vertexBuffer[1] = vb1;
    vertexArray->indexBuffer = ib;
    vertexArray->indexFormat = ebFormat;
    uint32_t attribute = 0;
    for (int stream = 0; stream < numStreams && stream < 2; ++stream)
    {
        const ArrayLayout2 *layout = stream == 0 ? streamLayout0 : streamLayout1;
        if (!layout)
        {
            continue;
        }
        uint32_t offset = 0;
        for (int entry = 0; entry < layout->mNumElements && attribute < 12; ++entry)
        {
            VkVertexInputAttributeDescription *desc = &vertexArray->attributes[attribute];
            desc->location = attribute;
            desc->binding = stream;
            desc->format = iVertexFormatPiToVulkan(layout->mEntry[entry].mFormat);
            desc->offset = offset;
            offset += iVertexFormatSize(layout->mEntry[entry].mFormat);
            ++attribute;
        }
        vertexArray->stride[stream] = offset;
    }
    vertexArray->attributeCount = attribute;
    if (mState) ++mState->liveVertexArrays;
    return vertexArray;
}
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
    if (mState->hostRenderPassFrameActive && mState->hostDrawBisectionEnabled)
    {
        ++mState->hostDrawBisectionAttempted;
        if ((mState->hostDrawBisectionMask & 1u) == 0)
        {
            return;
        }
        ++mState->hostDrawBisectionAdmitted;
    }
    if (pt == PrimitiveType::Triangle && mState->currentShader && mState->currentShader->isPicture &&
        mState->currentRenderTarget && mState->currentRenderTarget->color[0] &&
        mState->currentVertexArray && mState->currentVertexArray->indexBuffer && mState->textures[0])
    {
        piTexture target = mState->currentRenderTarget->color[0];
        if (iUpdatePictureDescriptorSet(mState, mReporter) &&
            iEnsurePictureGraphicsPipeline(mState, mState->currentShader, mState->currentRenderTarget, mState->currentVertexArray, mReporter) &&
            iSubmitPictureDraw(mState, mState->currentShader, mState->currentRenderTarget, mState->currentVertexArray, num, numInstances, baseIndex, mReporter))
        {
            char message[256];
            std::snprintf(message,
                          sizeof(message),
                          "[IMM_VK_COMPOSE_TRACE_20260612] picture_gpu host=%d num=%u instances=%u target=%ux%u",
                          mState->hostRenderPassFrameActive ? 1 : 0,
                          num,
                          numInstances,
                          mState->currentRenderTarget->width,
                          mState->currentRenderTarget->height);
            iDebugLog(message);
            mState->pendingPresentTexture = target;
            return;
        }
        if (!mState->drawSubmitFailureReported)
        {
            mState->drawSubmitFailureReported = true;
            iError(mReporter, "Vulkan renderer failed to submit picture draw commands");
        }
    }
    if (pt == PrimitiveType::Triangle && mState->currentRenderTarget && mState->currentRenderTarget->color[0] &&
        mState->currentRenderTarget->color[0]->data && mState->textures[0] && mState->textures[0]->data &&
        mState->textures[0]->info.mXres > 0 && mState->textures[0]->info.mYres > 0)
    {
        char fallbackMessage[256];
        std::snprintf(fallbackMessage,
                      sizeof(fallbackMessage),
                      "[IMM_VK_COMPOSE_TRACE_20260612] picture_cpu_fallback host=%d num=%u instances=%u target=%ux%u",
                      mState->hostRenderPassFrameActive ? 1 : 0,
                      num,
                      numInstances,
                      mState->currentRenderTarget->width,
                      mState->currentRenderTarget->height);
        iDebugLog(fallbackMessage);
        piTexture target = mState->currentRenderTarget->color[0];
        piTexture source = mState->textures[0];
        const int targetWidth = target->info.mXres;
        const int targetHeight = target->info.mYres;
        const int sourceWidth = source->info.mXres;
        const int sourceHeight = source->info.mYres;
        uint32_t visiblePixels = 0;
        for (int y = 0; y < targetHeight; ++y)
        {
            const int sy = (int)(((uint64_t)y * (uint64_t)sourceHeight) / (uint64_t)targetHeight);
            for (int x = 0; x < targetWidth; ++x)
            {
                const int sxBase = (int)(((uint64_t)x * (uint64_t)sourceWidth) / (uint64_t)targetWidth);
                const int sx = (sxBase + sourceWidth / 2) % sourceWidth;
                const uint8_t *src = source->data + ((size_t)sy * (size_t)sourceWidth + (size_t)sx) * 4u;
                uint8_t *dst = target->data + ((size_t)y * (size_t)targetWidth + (size_t)x) * 4u;
                dst[0] = (uint8_t)((uint32_t)src[0] * 3u / 5u);
                dst[1] = (uint8_t)((uint32_t)src[1] * 3u / 5u);
                dst[2] = (uint8_t)((uint32_t)src[2] * 3u / 5u);
                dst[3] = 255;
                if (dst[0] > 32 || dst[1] > 32 || dst[2] > 32)
                {
                    ++visiblePixels;
                }
            }
        }
        if (!iUploadCpuColorToGpuColorAttachment(mState, target, mReporter) && !mState->drawSubmitFailureReported)
        {
            mState->drawSubmitFailureReported = true;
            iError(mReporter, "Vulkan renderer failed to upload picture fallback target");
        }
        if (!mState->cpuPictureDiagnosticReported)
        {
            mState->cpuPictureDiagnosticReported = true;
            char message[256];
            std::snprintf(message,
                          sizeof(message),
                          "IMM_VK_CPU: picture fallback filled target visible=%u source=%dx%d target=%dx%d",
                          visiblePixels,
                          sourceWidth,
                          sourceHeight,
                          targetWidth,
                          targetHeight);
            iReport(mReporter, message);
        }
        mState->pendingPresentTexture = target;
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
    if (!iUpdateStaticPaintDescriptorSet(mState, mReporter) && !mState->descriptorSetFailureReported)
    {
        mState->descriptorSetFailureReported = true;
        iError(mReporter, "Vulkan renderer failed to update static paint descriptor set");
    }
    if (!iEnsureStaticPaintGraphicsPipeline(mState, mState->currentShader, mState->currentRenderTarget, mReporter))
    {
        return;
    }
    const bool hasStaticPaintGpuPath =
        mState->currentShader->pipeline != VK_NULL_PIPELINE &&
        mState->currentRenderTarget->framebuffer != VK_NULL_FRAMEBUFFER &&
        mState->staticPaintDescriptorSet != VK_NULL_DESCRIPTOR_SET;
    if (!iSubmitStaticPaintDraw(mState, mState->currentShader, mState->currentRenderTarget, mState->currentVertexArray, num, numInstances, baseVertex, baseInstance, baseIndex, mReporter) &&
        !mState->drawSubmitFailureReported)
    {
        mState->drawSubmitFailureReported = true;
        iError(mReporter, "Vulkan renderer failed to submit static paint draw commands");
    }
    if (hasStaticPaintGpuPath)
    {
        char message[256];
        std::snprintf(message,
                      sizeof(message),
                      "[IMM_VK_COMPOSE_TRACE_20260612] paint_gpu host=%d num=%u instances=%u target=%ux%u drawCount=%u",
                      mState->hostRenderPassFrameActive ? 1 : 0,
                      num,
                      numInstances,
                      mState->currentRenderTarget->width,
                      mState->currentRenderTarget->height,
                      mState->gpuPaintDrawCount + 1u);
        iDebugLog(message);
        mState->gpuPaintActive = true;
        ++mState->gpuPaintDrawCount;
    }
    if (hasStaticPaintGpuPath)
    {
        mState->pendingPresentTexture = target;
    }
    // On the Unity Android Vulkan host path, audit the first GPU draw against
    // the renderer's CPU mirrors before returning. This does not replace or
    // modify the recorded Vulkan draw; it tells us whether the exact indexed
    // geometry and matrices should cover the host viewport.
    const bool gpuProjectionAudit = hasStaticPaintGpuPath && !mState->cpuPaintDiagnosticReported;
    if (hasStaticPaintGpuPath && !gpuProjectionAudit)
    {
        return;
    }

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
    uint32_t insideClipVolume = 0;
    uint32_t positiveW = 0;
    float minX = 1000000.0f;
    float minY = 1000000.0f;
    float maxX = -1000000.0f;
    float maxY = -1000000.0f;
    float minNdcZ = 1000000.0f;
    float maxNdcZ = -1000000.0f;
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
        const float ndcZ = clip[2] / clip[3];
        if (clip[3] > 0.0f)
        {
            ++positiveW;
            if (clip[0] >= -clip[3] && clip[0] <= clip[3] &&
                clip[1] >= -clip[3] && clip[1] <= clip[3] &&
                clip[2] >= 0.0f && clip[2] <= clip[3])
            {
                ++insideClipVolume;
            }
        }
        if (ndcZ < minNdcZ) minNdcZ = ndcZ;
        if (ndcZ > maxNdcZ) maxNdcZ = ndcZ;
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
            if (!gpuProjectionAudit && target->data)
            {
                iDrawCpuLine(target, previous[0], previous[1], sx, sy, width, r, g, b, a);
            }
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
        const size_t pixelCount = target->data
            ? (size_t)target->info.mXres * (size_t)target->info.mYres
            : 0u;
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
                      "[IMM_UNITY_VK_CPU_CLIP_AUDIT_20260731] gpu=%d num=%u projected=%u xyInside=%u clipInside=%u positiveW=%u ndcZ=(%.5f,%.5f) segments=%u nonblack=%u brush=%d maxColor=%u,%u,%u maxA=%.3f screen=(%.1f,%.1f)-(%.1f,%.1f) target=%dx%d",
                      gpuProjectionAudit ? 1 : 0,
                      num,
                      projected,
                      insidePoints,
                      insideClipVolume,
                      positiveW,
                      minNdcZ,
                      maxNdcZ,
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
    if (!mState->gpuPaintActive && (mState->cpuPaintDrawCount == 1u || (mState->cpuPaintDrawCount & 31u) == 0u))
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
    if (!mState || !mState->textures[0])
    {
        if (!mState || !mState->currentShader || !mState->currentShader->isPicture2D)
        {
            iUnsupported(mState, mReporter, piVulkanUnsupportedFeature::DrawSubmission, "Vulkan draw submission is not implemented yet");
        }
        return;
    }
    if (mState->currentShader && mState->currentShader->isPicture2D)
    {
        if (mState->hostRenderPassFrameActive && mState->hostDrawBisectionEnabled)
        {
            ++mState->hostDrawBisectionAttempted;
            if ((mState->hostDrawBisectionMask & 2u) == 0)
            {
                return;
            }
            ++mState->hostDrawBisectionAdmitted;
        }
        if (!mState->currentRenderTarget || !mState->currentRenderTarget->color[0])
        {
            return;
        }
        piTexture target = mState->currentRenderTarget->color[0];
        const uint32_t instanceCount = numInstanced > 0 ? (uint32_t)numInstanced : 1u;
        if (iUpdatePictureDescriptorSet(mState, mReporter) &&
            iEnsurePictureGraphicsPipeline(mState, mState->currentShader, mState->currentRenderTarget, nullptr, mReporter) &&
            iSubmitPictureQuadDraw(mState, mState->currentShader, mState->currentRenderTarget, instanceCount, mReporter))
        {
            mState->pendingPresentTexture = target;
            return;
        }
        if (!mState->pictureDrawFailureReported)
        {
            iError(mReporter, "Vulkan renderer failed to submit 2D picture draw commands");
            mState->pictureDrawFailureReported = true;
        }
        return;
    }
    const bool displaySrgbOutput =
        mState->currentShader &&
        iShaderOption(mState->currentShader, "OUTPUT_ENCODING", 0) == 1;
    if (!mState->currentRenderTarget && mState->gpuPaintActive)
    {
        if (displaySrgbOutput)
        {
            mState->pendingPresentTexture = mState->textures[0];
        }
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
