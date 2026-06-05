#include <stdint.h>
#include <string.h>

static constexpr int32_t XR_SUCCESS = 0;
static constexpr int32_t XR_ERROR_VALIDATION_FAILURE = -1;
static constexpr int32_t XR_ERROR_SIZE_INSUFFICIENT = -11;
static constexpr int32_t XR_TYPE_EXTENSION_PROPERTIES = 2;
static constexpr int32_t XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR = 1000025000;
static constexpr int32_t XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR = 1000025002;
static constexpr int32_t XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR = 1000025001;
static constexpr int32_t XR_TYPE_SWAPCHAIN_CREATE_INFO = 9;
static constexpr int32_t XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO = 2;
static constexpr uint64_t XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT = 0x00000001ull;
static constexpr int64_t XR_FAKE_SWAPCHAIN_FORMAT = 44;
static constexpr uint32_t XR_FAKE_SWAPCHAIN_WIDTH = 1600;
static constexpr uint32_t XR_FAKE_SWAPCHAIN_HEIGHT = 1600;
static constexpr uint32_t XR_FAKE_SWAPCHAIN_ARRAY_SIZE = 2;
static constexpr uint64_t XR_FAKE_SESSION = 0x87654321ull;
static constexpr uint64_t XR_FAKE_SWAPCHAIN = 0x2468ace0ull;
static constexpr uint64_t XR_FAKE_VK_IMAGE = 0x13579bdfull;
static constexpr uint64_t XR_FAKE_VK_INSTANCE = 0x1111222233334444ull;
static constexpr uint64_t XR_FAKE_VK_PHYSICAL_DEVICE = 0x2222333344445555ull;
static constexpr uint64_t XR_FAKE_VK_DEVICE = 0x3333444455556666ull;

struct XrExtensionPropertiesImm
{
    int32_t type;
    void *next;
    char extensionName[128];
    uint32_t extensionVersion;
};

struct XrApplicationInfoImm
{
    char applicationName[128];
    uint32_t applicationVersion;
    char engineName[128];
    uint32_t engineVersion;
    uint64_t apiVersion;
};

struct XrInstanceCreateInfoImm
{
    int32_t type;
    const void *next;
    uint64_t createFlags;
    XrApplicationInfoImm applicationInfo;
    uint32_t enabledApiLayerCount;
    const char *const *enabledApiLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *enabledExtensionNames;
};

struct XrSystemGetInfoImm
{
    int32_t type;
    const void *next;
    int32_t formFactor;
};

struct XrSystemGraphicsPropertiesImm
{
    uint32_t maxSwapchainImageHeight;
    uint32_t maxSwapchainImageWidth;
    uint32_t maxLayerCount;
};

struct XrSystemTrackingPropertiesImm
{
    uint32_t orientationTracking;
    uint32_t positionTracking;
};

struct XrSystemPropertiesImm
{
    int32_t type;
    void *next;
    uint64_t systemId;
    uint32_t vendorId;
    char systemName[256];
    XrSystemGraphicsPropertiesImm graphicsProperties;
    XrSystemTrackingPropertiesImm trackingProperties;
};

struct XrViewConfigurationViewImm
{
    int32_t type;
    void *next;
    uint32_t recommendedImageRectWidth;
    uint32_t maxImageRectWidth;
    uint32_t recommendedImageRectHeight;
    uint32_t maxImageRectHeight;
    uint32_t recommendedSwapchainSampleCount;
    uint32_t maxSwapchainSampleCount;
};

struct XrSessionCreateInfoImm
{
    int32_t type;
    const void *next;
    uint64_t createFlags;
    uint64_t systemId;
};

struct XrGraphicsBindingVulkanKhrImm
{
    int32_t type;
    const void *next;
    uint64_t instance;
    uint64_t physicalDevice;
    uint64_t device;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
};

struct XrSessionBeginInfoImm
{
    int32_t type;
    const void *next;
    int32_t primaryViewConfigurationType;
};

struct XrFrameWaitInfoImm
{
    int32_t type;
    const void *next;
};

struct XrFrameStateImm
{
    int32_t type;
    void *next;
    int64_t predictedDisplayTime;
    int64_t predictedDisplayPeriod;
    uint32_t shouldRender;
};

struct XrFrameBeginInfoImm
{
    int32_t type;
    const void *next;
};

struct XrFrameEndInfoImm
{
    int32_t type;
    const void *next;
    int64_t displayTime;
    int32_t environmentBlendMode;
    uint32_t layerCount;
    const void *const *layers;
};

struct XrSwapchainCreateInfoImm
{
    int32_t type;
    const void *next;
    uint64_t createFlags;
    uint64_t usageFlags;
    int64_t format;
    uint32_t sampleCount;
    uint32_t width;
    uint32_t height;
    uint32_t faceCount;
    uint32_t arraySize;
    uint32_t mipCount;
};

struct XrSwapchainImageVulkanKhrImm
{
    int32_t type;
    void *next;
    uint64_t image;
};

struct XrGraphicsRequirementsVulkanKhrImm
{
    int32_t type;
    void *next;
    uint64_t minApiVersionSupported;
    uint64_t maxApiVersionSupported;
};

struct XrSwapchainImageAcquireInfoImm
{
    int32_t type;
    const void *next;
};

struct XrSwapchainImageWaitInfoImm
{
    int32_t type;
    const void *next;
    int64_t timeout;
};

struct XrSwapchainImageReleaseInfoImm
{
    int32_t type;
    const void *next;
};

static void CopyText(char *dst, size_t dstSize, const char *src)
{
    if (!dst || dstSize == 0)
    {
        return;
    }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = 0;
}

static int32_t CopyRequiredBufferText(const char *text, uint32_t capacity, uint32_t *count, char *buffer)
{
    const uint32_t required = static_cast<uint32_t>(strlen(text) + 1);
    if (count)
    {
        *count = required;
    }
    if (capacity == 0 || !buffer)
    {
        return XR_SUCCESS;
    }
    if (capacity < required)
    {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    memcpy(buffer, text, required);
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEnumerateInstanceExtensionProperties(
    const char *,
    uint32_t propertyCapacityInput,
    uint32_t *propertyCountOutput,
    XrExtensionPropertiesImm *properties)
{
    static const char *kExtensions[] = {
        "XR_KHR_vulkan_enable",
        "XR_KHR_vulkan_enable2",
        "XR_KHR_win32_convert_performance_counter_time"
    };
    static constexpr uint32_t kExtensionCount = sizeof(kExtensions) / sizeof(kExtensions[0]);

    if (propertyCountOutput)
    {
        *propertyCountOutput = kExtensionCount;
    }
    if (propertyCapacityInput == 0 || !properties)
    {
        return XR_SUCCESS;
    }
    if (propertyCapacityInput < kExtensionCount)
    {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    for (uint32_t i = 0; i < kExtensionCount; ++i)
    {
        properties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        properties[i].next = nullptr;
        CopyText(properties[i].extensionName, sizeof(properties[i].extensionName), kExtensions[i]);
        properties[i].extensionVersion = 1;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrCreateInstance(
    const XrInstanceCreateInfoImm *,
    uint64_t *instance)
{
    if (instance)
    {
        *instance = 0x12345678ull;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrGetVulkanInstanceExtensionsKHR(
    uint64_t,
    uint64_t,
    uint32_t bufferCapacityInput,
    uint32_t *bufferCountOutput,
    char *buffer)
{
    return CopyRequiredBufferText("VK_KHR_surface VK_KHR_win32_surface", bufferCapacityInput, bufferCountOutput, buffer);
}

extern "C" __declspec(dllexport) int32_t __stdcall xrGetVulkanDeviceExtensionsKHR(
    uint64_t,
    uint64_t,
    uint32_t bufferCapacityInput,
    uint32_t *bufferCountOutput,
    char *buffer)
{
    return CopyRequiredBufferText("VK_KHR_swapchain", bufferCapacityInput, bufferCountOutput, buffer);
}

extern "C" __declspec(dllexport) int32_t __stdcall xrGetVulkanGraphicsRequirementsKHR(
    uint64_t,
    uint64_t,
    XrGraphicsRequirementsVulkanKhrImm *graphicsRequirements)
{
    if (graphicsRequirements)
    {
        graphicsRequirements->type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
        graphicsRequirements->minApiVersionSupported = (1ull << 22);
        graphicsRequirements->maxApiVersionSupported = (1ull << 22) | (3ull << 12);
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrGetInstanceProcAddr(
    uint64_t,
    const char *name,
    void **function)
{
    if (!function)
    {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    *function = nullptr;
    if (strcmp(name, "xrGetVulkanInstanceExtensionsKHR") == 0)
    {
        *function = reinterpret_cast<void *>(&xrGetVulkanInstanceExtensionsKHR);
    }
    else if (strcmp(name, "xrGetVulkanDeviceExtensionsKHR") == 0)
    {
        *function = reinterpret_cast<void *>(&xrGetVulkanDeviceExtensionsKHR);
    }
    else if (strcmp(name, "xrGetVulkanGraphicsRequirementsKHR") == 0 ||
             strcmp(name, "xrGetVulkanGraphicsRequirements2KHR") == 0)
    {
        *function = reinterpret_cast<void *>(&xrGetVulkanGraphicsRequirementsKHR);
    }
    return *function ? XR_SUCCESS : -7;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrDestroyInstance(uint64_t)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrGetSystem(
    uint64_t,
    const XrSystemGetInfoImm *,
    uint64_t *systemId)
{
    if (systemId)
    {
        *systemId = 0x42ull;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrGetSystemProperties(
    uint64_t,
    uint64_t systemId,
    XrSystemPropertiesImm *properties)
{
    if (properties)
    {
        properties->systemId = systemId;
        properties->vendorId = 0x494d4d;
        CopyText(properties->systemName, sizeof(properties->systemName), "IMM Fake OpenXR Runtime");
        properties->graphicsProperties.maxSwapchainImageWidth = 4096;
        properties->graphicsProperties.maxSwapchainImageHeight = 4096;
        properties->graphicsProperties.maxLayerCount = 16;
        properties->trackingProperties.orientationTracking = 1;
        properties->trackingProperties.positionTracking = 1;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEnumerateViewConfigurations(
    uint64_t,
    uint64_t,
    uint32_t viewConfigurationTypeCapacityInput,
    uint32_t *viewConfigurationTypeCountOutput,
    int32_t *viewConfigurationTypes)
{
    if (viewConfigurationTypeCountOutput)
    {
        *viewConfigurationTypeCountOutput = 1;
    }
    if (viewConfigurationTypeCapacityInput == 0 || !viewConfigurationTypes)
    {
        return XR_SUCCESS;
    }
    viewConfigurationTypes[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEnumerateViewConfigurationViews(
    uint64_t,
    uint64_t,
    int32_t,
    uint32_t viewCapacityInput,
    uint32_t *viewCountOutput,
    XrViewConfigurationViewImm *views)
{
    if (viewCountOutput)
    {
        *viewCountOutput = 2;
    }
    if (viewCapacityInput == 0 || !views)
    {
        return XR_SUCCESS;
    }
    if (viewCapacityInput < 2)
    {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    for (uint32_t i = 0; i < 2; ++i)
    {
        views[i].recommendedImageRectWidth = 1600;
        views[i].maxImageRectWidth = 4096;
        views[i].recommendedImageRectHeight = 1600;
        views[i].maxImageRectHeight = 4096;
        views[i].recommendedSwapchainSampleCount = 1;
        views[i].maxSwapchainSampleCount = 4;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrCreateSession(
    uint64_t,
    const XrSessionCreateInfoImm *createInfo,
    uint64_t *session)
{
    const XrGraphicsBindingVulkanKhrImm *binding =
        createInfo ? reinterpret_cast<const XrGraphicsBindingVulkanKhrImm *>(createInfo->next) : nullptr;
    if (!binding ||
        binding->type != XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR ||
        binding->instance != XR_FAKE_VK_INSTANCE ||
        binding->physicalDevice != XR_FAKE_VK_PHYSICAL_DEVICE ||
        binding->device != XR_FAKE_VK_DEVICE ||
        binding->queueFamilyIndex != 7 ||
        binding->queueIndex != 1)
    {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    if (session)
    {
        *session = XR_FAKE_SESSION;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrDestroySession(uint64_t)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrBeginSession(
    uint64_t,
    const XrSessionBeginInfoImm *)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEnumerateSwapchainFormats(
    uint64_t,
    uint32_t formatCapacityInput,
    uint32_t *formatCountOutput,
    int64_t *formats)
{
    if (formatCountOutput)
    {
        *formatCountOutput = 1;
    }
    if (formatCapacityInput == 0 || !formats)
    {
        return XR_SUCCESS;
    }
    formats[0] = XR_FAKE_SWAPCHAIN_FORMAT;
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrCreateSwapchain(
    uint64_t,
    const XrSwapchainCreateInfoImm *createInfo,
    uint64_t *swapchain)
{
    if (!createInfo ||
        createInfo->type != XR_TYPE_SWAPCHAIN_CREATE_INFO ||
        (createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) == 0 ||
        createInfo->format != XR_FAKE_SWAPCHAIN_FORMAT ||
        createInfo->sampleCount != 1 ||
        createInfo->width != XR_FAKE_SWAPCHAIN_WIDTH ||
        createInfo->height != XR_FAKE_SWAPCHAIN_HEIGHT ||
        createInfo->faceCount != 1 ||
        createInfo->arraySize != XR_FAKE_SWAPCHAIN_ARRAY_SIZE ||
        createInfo->mipCount != 1)
    {
        return XR_ERROR_VALIDATION_FAILURE;
    }
    if (swapchain)
    {
        *swapchain = XR_FAKE_SWAPCHAIN;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrDestroySwapchain(uint64_t)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEnumerateSwapchainImages(
    uint64_t,
    uint32_t imageCapacityInput,
    uint32_t *imageCountOutput,
    XrSwapchainImageVulkanKhrImm *images)
{
    if (imageCountOutput)
    {
        *imageCountOutput = 1;
    }
    if (imageCapacityInput == 0 || !images)
    {
        return XR_SUCCESS;
    }
    images[0].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
    images[0].next = nullptr;
    images[0].image = XR_FAKE_VK_IMAGE;
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrAcquireSwapchainImage(
    uint64_t,
    const XrSwapchainImageAcquireInfoImm *,
    uint32_t *index)
{
    if (index)
    {
        *index = 0;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrWaitSwapchainImage(
    uint64_t,
    const XrSwapchainImageWaitInfoImm *)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrReleaseSwapchainImage(
    uint64_t,
    const XrSwapchainImageReleaseInfoImm *)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEndSession(uint64_t)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrWaitFrame(
    uint64_t,
    const XrFrameWaitInfoImm *,
    XrFrameStateImm *frameState)
{
    if (frameState)
    {
        frameState->predictedDisplayTime = 123456789;
        frameState->predictedDisplayPeriod = 11111111;
        frameState->shouldRender = 1;
    }
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrBeginFrame(
    uint64_t,
    const XrFrameBeginInfoImm *)
{
    return XR_SUCCESS;
}

extern "C" __declspec(dllexport) int32_t __stdcall xrEndFrame(
    uint64_t,
    const XrFrameEndInfoImm *)
{
    return XR_SUCCESS;
}
