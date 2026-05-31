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

static constexpr VkInstance VK_NULL_INSTANCE = nullptr;
static constexpr VkPhysicalDevice VK_NULL_PHYSICAL_DEVICE = nullptr;
static constexpr VkDevice VK_NULL_DEVICE = nullptr;
static constexpr VkQueue VK_NULL_QUEUE = nullptr;
static constexpr VkSurfaceKHR VK_NULL_SURFACE_KHR = 0;
static constexpr VkResult VK_SUCCESS = 0;
static constexpr VkStructureType VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;
static constexpr VkStructureType VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3;
static constexpr VkStructureType VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000;
static constexpr VkQueueFlags VK_QUEUE_GRAPHICS_BIT = 0x00000001;

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
};

struct piBufferS
{
    uint8_t *data = nullptr;
    unsigned int size = 0;
    piRenderer::BufferType type = piRenderer::BufferType::Static;
    piRenderer::BufferUse use = piRenderer::BufferUse::Vertex;
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
static uint8_t *iBuildBgraCopy(piTexture texture)
{
    if (!texture || !texture->data || texture->info.mXres <= 0 || texture->info.mYres <= 0)
    {
        return nullptr;
    }
    const size_t pixelCount = (size_t)texture->info.mXres * (size_t)texture->info.mYres;
    uint8_t *bgra = (uint8_t *)std::malloc(pixelCount * 4u);
    if (!bgra)
    {
        return nullptr;
    }
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const uint8_t *src = texture->data + i * 4u;
        uint8_t *dst = bgra + i * 4u;
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = 255;
    }
    return bgra;
}

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

static void iPresentTextureToWindow(piVulkanState *state, piTexture texture)
{
    if (!state || !state->window || !texture || !texture->data)
    {
        return;
    }
    HDC dc = GetDC(state->window);
    if (!dc)
    {
        return;
    }
    RECT rect = {};
    GetClientRect(state->window, &rect);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = texture->info.mXres;
    bmi.bmiHeader.biHeight = -texture->info.mYres;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    uint8_t *bgra = iBuildBgraCopy(texture);
    if (!bgra)
    {
        ReleaseDC(state->window, dc);
        return;
    }
    StretchDIBits(dc,
                  0,
                  0,
                  rect.right - rect.left,
                  rect.bottom - rect.top,
                  0,
                  0,
                  texture->info.mXres,
                  texture->info.mYres,
                  bgra,
                  &bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY);
    std::free(bgra);
    iWritePpmCapture(state, texture);
    ReleaseDC(state->window, dc);
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
#if defined(WINDOWS)
    state->vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)state->vkGetInstanceProcAddr(state->instance, "vkCreateWin32SurfaceKHR");
#endif
    if (!state->vkDestroyInstance || !state->vkEnumeratePhysicalDevices || !state->vkGetPhysicalDeviceQueueFamilyProperties ||
        !state->vkCreateDevice || !state->vkDestroyDevice || !state->vkGetDeviceQueue || !state->vkDeviceWaitIdle)
    {
        iError(reporter, "Vulkan renderer could not load required Vulkan entry points");
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
    return state->graphicsQueue != VK_NULL_QUEUE;
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
void piRendererVulkan::SwapBuffers(void) {}
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
    if (mState) ++mState->liveRenderTargets;
    return target;
}

void piRendererVulkan::DestroyRenderTarget(piRTarget obj)
{
    if (!obj) return;
    if (mState && mState->liveRenderTargets > 0) --mState->liveRenderTargets;
    delete obj;
}

bool piRendererVulkan::SetRenderTarget(piRTarget obj)
{
    if (mState) mState->currentRenderTarget = obj;
    return true;
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
    (void)key; (void)compress; (void)aniso; (void)bindUsage;
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
    if (mState) ++mState->liveTextures;
    return texture;
}

void piRendererVulkan::DestroyTexture(piTexture obj) { if (!obj) return; std::free(obj->data); if (mState && mState->liveTextures > 0) --mState->liveTextures; delete obj; }
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

piBuffer piRendererVulkan::CreateBuffer(const void *data, unsigned int amount, BufferType mode, BufferUse use) { if (amount == 0) return nullptr; piBufferS *buffer = new piBufferS(); buffer->size = amount; buffer->type = mode; buffer->use = use; buffer->data = (uint8_t *)std::malloc(amount); if (!buffer->data) { delete buffer; return nullptr; } if (data) std::memcpy(buffer->data, data, amount); else std::memset(buffer->data, 0, amount); if (mState) ++mState->liveBuffers; return buffer; }
piBuffer piRendererVulkan::CreateStructuredBuffer(const void *data, unsigned int numElements, unsigned int elementSize, BufferType mode, BufferUse use) { return CreateBuffer(data, numElements * elementSize, mode, use); }
piBuffer piRendererVulkan::CreateBufferMapped_Start(void **ptr, unsigned int amount, BufferUse use) { piBuffer buffer = CreateBuffer(nullptr, amount, BufferType::Dynamic, use); if (ptr) *ptr = buffer ? buffer->data : nullptr; return buffer; }
void piRendererVulkan::CreateBufferMapped_End(piBuffer vme) { (void)vme; }
void piRendererVulkan::DestroyBuffer(piBuffer obj) { if (!obj) return; std::free(obj->data); if (mState && mState->liveBuffers > 0) --mState->liveBuffers; delete obj; }
void piRendererVulkan::UpdateBuffer(piBuffer obj, const void *data, int offset, int len, bool invalidate) { (void)invalidate; if (!obj || !data || offset < 0 || len < 0 || (unsigned int)(offset + len) > obj->size) return; std::memcpy(obj->data + offset, data, (size_t)len); }
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
    if (!mState->captureWritten || (mState->cpuPaintDrawCount & 31u) == 0u)
    {
        iPresentTextureToWindow(mState, target);
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
    iPresentTextureToWindow(mState, mState->textures[0]);
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
