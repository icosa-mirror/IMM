// Minimal Unity Vulkan plugin interface declarations used by ImmUnityPlugin.
// This avoids adding a Vulkan SDK dependency to the Unity native plugin build.
// Layout and GUIDs match Unity 2022.3 PluginAPI/IUnityGraphicsVulkan.h.
#pragma once

#include "IUnityGraphics.h"

#include <stddef.h>
#include <stdint.h>

typedef uint32_t VkMemoryPropertyFlags;
typedef uint32_t VkPipelineStageFlags;
typedef uint32_t VkAccessFlags;
typedef uint32_t VkImageLayout;
typedef uint32_t VkImageAspectFlags;
typedef uint32_t VkImageUsageFlags;
typedef uint32_t VkImageTiling;
typedef uint32_t VkImageType;
typedef uint32_t VkSampleCountFlagBits;
typedef uint32_t VkCommandBufferLevel;
typedef uint32_t VkFormat;
typedef uint32_t VkImageSubresource;
typedef uint64_t VkDeviceSize;
typedef uint64_t VkPipelineCache;
typedef uint64_t VkDeviceMemory;
typedef uint64_t VkImage;
typedef uint64_t VkImageView;
typedef uint64_t VkBuffer;
typedef uint64_t VkRenderPass;
typedef uint64_t VkFramebuffer;
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkDevice_T *VkDevice;
typedef struct VkQueue_T *VkQueue;
typedef struct VkCommandBuffer_T *VkCommandBuffer;

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *name);

struct UnityVulkanInstance
{
    VkPipelineCache pipelineCache;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr;
    unsigned int queueFamilyIndex;
    void *reserved[8];
};

struct UnityVulkanMemory
{
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    void *mapped;
    VkMemoryPropertyFlags flags;
    unsigned int memoryTypeIndex;
    void *reserved[4];
};

enum UnityVulkanResourceAccessMode
{
    kUnityVulkanResourceAccess_ObserveOnly,
    kUnityVulkanResourceAccess_PipelineBarrier,
    kUnityVulkanResourceAccess_Recreate,
};

struct UnityVulkanImage
{
    UnityVulkanMemory memory;
    VkImage image;
    VkImageLayout layout;
    VkImageAspectFlags aspect;
    VkImageUsageFlags usage;
    VkFormat format;
    struct { uint32_t width, height, depth; } extent;
    VkImageTiling tiling;
    VkImageType type;
    VkSampleCountFlagBits samples;
    int layers;
    int mipCount;
    void *reserved[4];
};

struct UnityVulkanBuffer
{
    UnityVulkanMemory memory;
    VkBuffer buffer;
    size_t sizeInBytes;
    uint32_t usage;
    void *reserved[4];
};

struct UnityVulkanRecordingState
{
    VkCommandBuffer commandBuffer;
    VkCommandBufferLevel commandBufferLevel;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    int subPassIndex;
    unsigned long long currentFrameNumber;
    unsigned long long safeFrameNumber;
    void *reserved[4];
};

enum UnityVulkanEventRenderPassPreCondition
{
    kUnityVulkanRenderPass_DontCare,
    kUnityVulkanRenderPass_EnsureInside,
    kUnityVulkanRenderPass_EnsureOutside
};

enum UnityVulkanGraphicsQueueAccess
{
    kUnityVulkanGraphicsQueueAccess_DontCare,
    kUnityVulkanGraphicsQueueAccess_Allow,
};

struct UnityVulkanPluginEventConfig
{
    UnityVulkanEventRenderPassPreCondition renderPassPrecondition;
    UnityVulkanGraphicsQueueAccess graphicsQueueAccess;
    uint32_t flags;
};

enum UnityVulkanEventConfigFlagBits
{
    kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission = (1 << 0),
    kUnityVulkanEventConfigFlag_FlushCommandBuffers = (1 << 1),
    kUnityVulkanEventConfigFlag_SyncWorkerThreads = (1 << 2),
    kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState = (1 << 3),
};

typedef PFN_vkGetInstanceProcAddr(UNITY_INTERFACE_API *UnityVulkanInitCallback)(PFN_vkGetInstanceProcAddr getInstanceProcAddr, void *userdata);

struct UnityVulkanSwapchainConfiguration
{
    int mode;
};

const VkImageSubresource *const UnityVulkanWholeImage = nullptr;

UNITY_DECLARE_INTERFACE(IUnityGraphicsVulkan)
{
    bool(UNITY_INTERFACE_API *InterceptInitialization)(UnityVulkanInitCallback func, void *userdata);
    PFN_vkVoidFunction(UNITY_INTERFACE_API *InterceptVulkanAPI)(const char *name, PFN_vkVoidFunction func);
    void(UNITY_INTERFACE_API *ConfigureEvent)(int eventID, const UnityVulkanPluginEventConfig *pluginEventConfig);
    UnityVulkanInstance(UNITY_INTERFACE_API *Instance)();
    bool(UNITY_INTERFACE_API *CommandRecordingState)(UnityVulkanRecordingState *outCommandRecordingState, UnityVulkanGraphicsQueueAccess queueAccess);
    bool(UNITY_INTERFACE_API *AccessTexture)(void *nativeTexture, const VkImageSubresource *subResource, VkImageLayout layout, VkPipelineStageFlags pipelineStageFlags, VkAccessFlags accessFlags, UnityVulkanResourceAccessMode accessMode, UnityVulkanImage *outImage);
    bool(UNITY_INTERFACE_API *AccessRenderBufferTexture)(UnityRenderBuffer nativeRenderBuffer, const VkImageSubresource *subResource, VkImageLayout layout, VkPipelineStageFlags pipelineStageFlags, VkAccessFlags accessFlags, UnityVulkanResourceAccessMode accessMode, UnityVulkanImage *outImage);
    bool(UNITY_INTERFACE_API *AccessRenderBufferResolveTexture)(UnityRenderBuffer nativeRenderBuffer, const VkImageSubresource *subResource, VkImageLayout layout, VkPipelineStageFlags pipelineStageFlags, VkAccessFlags accessFlags, UnityVulkanResourceAccessMode accessMode, UnityVulkanImage *outImage);
    bool(UNITY_INTERFACE_API *AccessBuffer)(void *nativeBuffer, VkPipelineStageFlags pipelineStageFlags, VkAccessFlags accessFlags, UnityVulkanResourceAccessMode accessMode, UnityVulkanBuffer *outBuffer);
    void(UNITY_INTERFACE_API *EnsureOutsideRenderPass)();
    void(UNITY_INTERFACE_API *EnsureInsideRenderPass)();
    void(UNITY_INTERFACE_API *AccessQueue)(UnityRenderingEventAndData callback, int eventId, void *userData, bool flush);
    bool(UNITY_INTERFACE_API *ConfigureSwapchain)(const UnityVulkanSwapchainConfiguration *swapChainConfig);
    bool(UNITY_INTERFACE_API *AccessTextureByID)(UnityTextureID textureID, const VkImageSubresource *subResource, VkImageLayout layout, VkPipelineStageFlags pipelineStageFlags, VkAccessFlags accessFlags, UnityVulkanResourceAccessMode accessMode, UnityVulkanImage *outImage);
};
UNITY_REGISTER_INTERFACE_GUID(0x95355348d4ef4e11ULL, 0x9789313dfcffcc87ULL, IUnityGraphicsVulkan)
