#if defined(__APPLE__)
#define AA 1
#else
#define AA 8
#endif

// Set this to 1, ONLY if you need to build the viewer without the Oculus SDF installed
// This flag is NOT meant to force mono rendering. Mono rendering can always be forced
// from the config file even if the viewer is built to do VR
#ifndef DISABLE_VR
#define DISABLE_VR 0
#endif

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WINDOWS)
#include <windows.h>
#endif
#include "libImmCore/src/libBasics/piTimer.h"
#include "libImmCore/src/libBasics/piWindow.h"
#include "libImmCore/src/libBasics/piImage.h"
#include "libImmCore/src/libBasics/piFile.h"
#if DISABLE_VR==0
#include "libImmCore/src/libVR/piVR.h"
#endif
#include "viewer/viewer.h"
#include "settings.h"
#include "resolve.h"
using namespace ImmCore;
using namespace ImmImporter;
using namespace ExePlayer;

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

//-----

static const char* vsMirror = ""

"layout(location = 0) in vec2 inVertex;"

"out V2FData"
"{"
"vec2 uv;"
"}vf;"

"void main()"
"{"
"vf.uv = 0.5 + 0.5*inVertex;"
"gl_Position = vec4(inVertex, 0.0, 1.0);"
"}";

static const char* fsMirror = ""

"layout(binding = 0) uniform sampler2D unTex0;"
"layout(location = 0, index = 0) out vec4 outColor;"
"layout(location = 1) uniform vec2 unRes;"

"in V2FData"
"{"
"vec2 uv;"
"}vf;"

"void main()"
"{"
"vec3 col = texture( unTex0, vec2( vf.uv.x, 0.5 + (vf.uv.y-0.5)/(unRes.x/unRes.y) ) ).xyz;"
"col = pow(col, vec3(0.4545));"
"outColor = vec4(col, 1.0);"
"}";

#if defined(WINDOWS)
namespace
{
    static constexpr int32_t kXrSuccess = 0;
    static constexpr int32_t kXrTypeExtensionProperties = 2;
    static constexpr int32_t kXrTypeInstanceCreateInfo = 3;
    static constexpr int32_t kXrTypeSystemGetInfo = 4;
    static constexpr int32_t kXrTypeSystemProperties = 5;
    static constexpr int32_t kXrTypeSessionCreateInfo = 8;
    static constexpr int32_t kXrTypeSwapchainCreateInfo = 9;
    static constexpr int32_t kXrTypeGraphicsBindingVulkanKhr = 1000025000;
    static constexpr int32_t kXrTypeSessionBeginInfo = 10;
    static constexpr int32_t kXrTypeFrameWaitInfo = 33;
    static constexpr int32_t kXrTypeFrameEndInfo = 12;
    static constexpr int32_t kXrTypeCompositionLayerProjection = 35;
    static constexpr int32_t kXrTypeFrameState = 44;
    static constexpr int32_t kXrTypeFrameBeginInfo = 46;
    static constexpr int32_t kXrTypeCompositionLayerProjectionView = 48;
    static constexpr int32_t kXrTypeViewConfigurationView = 41;
    static constexpr int32_t kXrTypeSwapchainImageAcquireInfo = 55;
    static constexpr int32_t kXrTypeSwapchainImageWaitInfo = 56;
    static constexpr int32_t kXrTypeSwapchainImageReleaseInfo = 57;
    static constexpr int32_t kXrTypeSwapchainImageVulkanKhr = 1000025001;
    static constexpr int32_t kXrTypeGraphicsRequirementsVulkanKhr = 1000025002;
    static constexpr int32_t kXrFormFactorHmd = 1;
    static constexpr int32_t kXrViewConfigurationPrimaryStereo = 2;
    static constexpr int32_t kXrEnvironmentBlendModeOpaque = 1;
    static constexpr uint64_t kXrSwapchainUsageColorAttachmentBit = 0x00000001ULL;
    static constexpr uint64_t kXrCurrentApiVersion = (1ULL << 48);
    static constexpr uint64_t kFakeVulkanInstance = 0x1111222233334444ULL;
    static constexpr uint64_t kFakeVulkanPhysicalDevice = 0x2222333344445555ULL;
    static constexpr uint64_t kFakeVulkanDevice = 0x3333444455556666ULL;

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

    struct XrVector3fImm
    {
        float x;
        float y;
        float z;
    };

    struct XrQuaternionfImm
    {
        float x;
        float y;
        float z;
        float w;
    };

    struct XrPosefImm
    {
        XrQuaternionfImm orientation;
        XrVector3fImm position;
    };

    struct XrFovfImm
    {
        float angleLeft;
        float angleRight;
        float angleUp;
        float angleDown;
    };

    struct XrOffset2DiImm
    {
        int32_t x;
        int32_t y;
    };

    struct XrExtent2DiImm
    {
        int32_t width;
        int32_t height;
    };

    struct XrRect2DiImm
    {
        XrOffset2DiImm offset;
        XrExtent2DiImm extent;
    };

    struct XrSwapchainSubImageImm
    {
        uint64_t swapchain;
        XrRect2DiImm imageRect;
        uint32_t imageArrayIndex;
    };

    struct XrCompositionLayerProjectionViewImm
    {
        int32_t type;
        const void *next;
        XrPosefImm pose;
        XrFovfImm fov;
        XrSwapchainSubImageImm subImage;
    };

    struct XrCompositionLayerProjectionImm
    {
        int32_t type;
        const void *next;
        uint64_t layerFlags;
        uint64_t space;
        uint32_t viewCount;
        const XrCompositionLayerProjectionViewImm *views;
    };

    typedef int32_t(__stdcall *PFN_xrEnumerateInstanceExtensionPropertiesImm)(const char *, uint32_t, uint32_t *, XrExtensionPropertiesImm *);
    typedef int32_t(__stdcall *PFN_xrCreateInstanceImm)(const XrInstanceCreateInfoImm *, uint64_t *);
    typedef int32_t(__stdcall *PFN_xrGetInstanceProcAddrImm)(uint64_t, const char *, void **);
    typedef int32_t(__stdcall *PFN_xrDestroyInstanceImm)(uint64_t);
    typedef int32_t(__stdcall *PFN_xrGetSystemImm)(uint64_t, const XrSystemGetInfoImm *, uint64_t *);
    typedef int32_t(__stdcall *PFN_xrGetSystemPropertiesImm)(uint64_t, uint64_t, XrSystemPropertiesImm *);
    typedef int32_t(__stdcall *PFN_xrEnumerateViewConfigurationsImm)(uint64_t, uint64_t, uint32_t, uint32_t *, int32_t *);
    typedef int32_t(__stdcall *PFN_xrEnumerateViewConfigurationViewsImm)(uint64_t, uint64_t, int32_t, uint32_t, uint32_t *, XrViewConfigurationViewImm *);
    typedef int32_t(__stdcall *PFN_xrCreateSessionImm)(uint64_t, const XrSessionCreateInfoImm *, uint64_t *);
    typedef int32_t(__stdcall *PFN_xrDestroySessionImm)(uint64_t);
    typedef int32_t(__stdcall *PFN_xrBeginSessionImm)(uint64_t, const XrSessionBeginInfoImm *);
    typedef int32_t(__stdcall *PFN_xrEndSessionImm)(uint64_t);
    typedef int32_t(__stdcall *PFN_xrWaitFrameImm)(uint64_t, const XrFrameWaitInfoImm *, XrFrameStateImm *);
    typedef int32_t(__stdcall *PFN_xrBeginFrameImm)(uint64_t, const XrFrameBeginInfoImm *);
    typedef int32_t(__stdcall *PFN_xrEndFrameImm)(uint64_t, const XrFrameEndInfoImm *);
    typedef int32_t(__stdcall *PFN_xrEnumerateSwapchainFormatsImm)(uint64_t, uint32_t, uint32_t *, int64_t *);
    typedef int32_t(__stdcall *PFN_xrCreateSwapchainImm)(uint64_t, const XrSwapchainCreateInfoImm *, uint64_t *);
    typedef int32_t(__stdcall *PFN_xrDestroySwapchainImm)(uint64_t);
    typedef int32_t(__stdcall *PFN_xrEnumerateSwapchainImagesImm)(uint64_t, uint32_t, uint32_t *, XrSwapchainImageVulkanKhrImm *);
    typedef int32_t(__stdcall *PFN_xrAcquireSwapchainImageImm)(uint64_t, const XrSwapchainImageAcquireInfoImm *, uint32_t *);
    typedef int32_t(__stdcall *PFN_xrWaitSwapchainImageImm)(uint64_t, const XrSwapchainImageWaitInfoImm *);
    typedef int32_t(__stdcall *PFN_xrReleaseSwapchainImageImm)(uint64_t, const XrSwapchainImageReleaseInfoImm *);
    typedef int32_t(__stdcall *PFN_xrGetVulkanInstanceExtensionsKhrImm)(uint64_t, uint64_t, uint32_t, uint32_t *, char *);
    typedef int32_t(__stdcall *PFN_xrGetVulkanDeviceExtensionsKhrImm)(uint64_t, uint64_t, uint32_t, uint32_t *, char *);
    typedef int32_t(__stdcall *PFN_xrGetVulkanGraphicsRequirementsKhrImm)(uint64_t, uint64_t, XrGraphicsRequirementsVulkanKhrImm *);

    static bool iQueryRegistryString(HKEY root, const wchar_t *keyPath, const wchar_t *valueName, wchar_t *out, DWORD outCount)
    {
        DWORD type = 0;
        DWORD bytes = outCount * sizeof(wchar_t);
        const LSTATUS result = RegGetValueW(root, keyPath, valueName, RRF_RT_REG_SZ, &type, out, &bytes);
        return result == ERROR_SUCCESS && bytes > sizeof(wchar_t);
    }

    static bool iGetOpenXRLoaderPath(wchar_t *out, DWORD outCount)
    {
        if (GetModuleFileNameW(nullptr, out, outCount) == 0)
        {
            return false;
        }

        wchar_t runtimeJson[PATH_MAX] = {};
        const wchar_t *keyPath = L"SOFTWARE\\Khronos\\OpenXR\\1";
        if (!iQueryRegistryString(HKEY_CURRENT_USER, keyPath, L"ActiveRuntime", runtimeJson, PATH_MAX) &&
            !iQueryRegistryString(HKEY_LOCAL_MACHINE, keyPath, L"ActiveRuntime", runtimeJson, PATH_MAX) &&
            !iQueryRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Khronos\\OpenXR\\1", L"ActiveRuntime", runtimeJson, PATH_MAX))
        {
            return false;
        }

        wcsncpy(out, runtimeJson, outCount - 1);
        out[outCount - 1] = 0;
        wchar_t *slash = wcsrchr(out, L'\\');
        if (!slash)
        {
            return false;
        }

        *slash = 0;
        const wchar_t *loaderSuffix = L"\\bin\\win64\\openxr_loader.dll";
        if (wcslen(out) + wcslen(loaderSuffix) + 1 >= outCount)
        {
            return false;
        }
        wcscat(out, loaderSuffix);
        return true;
    }

    template <typename T>
    static bool iLoadXrProc(HMODULE module, const char *name, T *out)
    {
        *out = reinterpret_cast<T>(GetProcAddress(module, name));
        return *out != nullptr;
    }

    static const wchar_t *iXrResultName(int32_t result)
    {
        switch (result)
        {
        case 0: return L"XR_SUCCESS";
        case -1: return L"XR_ERROR_VALIDATION_FAILURE";
        case -2: return L"XR_ERROR_RUNTIME_FAILURE";
        case -3: return L"XR_ERROR_OUT_OF_MEMORY";
        case -4: return L"XR_ERROR_API_VERSION_UNSUPPORTED";
        case -6: return L"XR_ERROR_INITIALIZATION_FAILED";
        case -7: return L"XR_ERROR_FUNCTION_UNSUPPORTED";
        case -8: return L"XR_ERROR_FEATURE_UNSUPPORTED";
        case -9: return L"XR_ERROR_EXTENSION_NOT_PRESENT";
        case -34: return L"XR_ERROR_FORM_FACTOR_UNSUPPORTED";
        case -35: return L"XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case -51: return L"XR_ERROR_RUNTIME_UNAVAILABLE";
        default: return L"XR_RESULT_UNKNOWN";
        }
    }

    static void iLogOpenXRText(piLog *log, const wchar_t *label, const char *text)
    {
        wchar_t tmp[512];
        pistr2ws(tmp, 512, text ? text : "");
        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE %s=%s", label, tmp);
    }

    static bool iProbeStandaloneOpenXR(piLog *log)
    {
        wchar_t loaderPath[PATH_MAX] = {};
        wchar_t envLoaderPath[PATH_MAX] = {};
        HMODULE loader = nullptr;
        const DWORD envLoaderPathLength = GetEnvironmentVariableW(L"IMM_OPENXR_LOADER_DLL", envLoaderPath, PATH_MAX);
        if (envLoaderPathLength > 0 && envLoaderPathLength < PATH_MAX)
        {
            loader = LoadLibraryW(envLoaderPath);
            if (loader)
            {
                wcsncpy(loaderPath, envLoaderPath, PATH_MAX - 1);
                loaderPath[PATH_MAX - 1] = 0;
            }
        }
        else
        {
            loader = LoadLibraryW(L"openxr_loader.dll");
        }
        if (!loader)
        {
            if (!iGetOpenXRLoaderPath(loaderPath, PATH_MAX))
            {
                log->Printf(LT_ERROR, L"IMM_OPENXR_STANDALONE missing=loaderPath");
                return false;
            }
            loader = LoadLibraryW(loaderPath);
        }
        if (!loader)
        {
            log->Printf(LT_ERROR, L"IMM_OPENXR_STANDALONE loadLoaderResult=%d", GetLastError());
            return false;
        }
        if (loaderPath[0] != 0)
        {
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE loader=%s", loaderPath);
        }

        PFN_xrEnumerateInstanceExtensionPropertiesImm xrEnumerateInstanceExtensionProperties = nullptr;
        PFN_xrCreateInstanceImm xrCreateInstance = nullptr;
        PFN_xrGetInstanceProcAddrImm xrGetInstanceProcAddr = nullptr;
        PFN_xrDestroyInstanceImm xrDestroyInstance = nullptr;
        PFN_xrGetSystemImm xrGetSystem = nullptr;
        PFN_xrGetSystemPropertiesImm xrGetSystemProperties = nullptr;
        PFN_xrEnumerateViewConfigurationsImm xrEnumerateViewConfigurations = nullptr;
        PFN_xrEnumerateViewConfigurationViewsImm xrEnumerateViewConfigurationViews = nullptr;
        PFN_xrCreateSessionImm xrCreateSession = nullptr;
        PFN_xrDestroySessionImm xrDestroySession = nullptr;
        PFN_xrBeginSessionImm xrBeginSession = nullptr;
        PFN_xrEndSessionImm xrEndSession = nullptr;
        PFN_xrWaitFrameImm xrWaitFrame = nullptr;
        PFN_xrBeginFrameImm xrBeginFrame = nullptr;
        PFN_xrEndFrameImm xrEndFrame = nullptr;
        PFN_xrEnumerateSwapchainFormatsImm xrEnumerateSwapchainFormats = nullptr;
        PFN_xrCreateSwapchainImm xrCreateSwapchain = nullptr;
        PFN_xrDestroySwapchainImm xrDestroySwapchain = nullptr;
        PFN_xrEnumerateSwapchainImagesImm xrEnumerateSwapchainImages = nullptr;
        PFN_xrAcquireSwapchainImageImm xrAcquireSwapchainImage = nullptr;
        PFN_xrWaitSwapchainImageImm xrWaitSwapchainImage = nullptr;
        PFN_xrReleaseSwapchainImageImm xrReleaseSwapchainImage = nullptr;
        if (!iLoadXrProc(loader, "xrEnumerateInstanceExtensionProperties", &xrEnumerateInstanceExtensionProperties) ||
            !iLoadXrProc(loader, "xrCreateInstance", &xrCreateInstance) ||
            !iLoadXrProc(loader, "xrGetInstanceProcAddr", &xrGetInstanceProcAddr) ||
            !iLoadXrProc(loader, "xrDestroyInstance", &xrDestroyInstance) ||
            !iLoadXrProc(loader, "xrGetSystem", &xrGetSystem) ||
            !iLoadXrProc(loader, "xrGetSystemProperties", &xrGetSystemProperties) ||
            !iLoadXrProc(loader, "xrEnumerateViewConfigurations", &xrEnumerateViewConfigurations) ||
            !iLoadXrProc(loader, "xrEnumerateViewConfigurationViews", &xrEnumerateViewConfigurationViews) ||
            !iLoadXrProc(loader, "xrCreateSession", &xrCreateSession) ||
            !iLoadXrProc(loader, "xrDestroySession", &xrDestroySession) ||
            !iLoadXrProc(loader, "xrBeginSession", &xrBeginSession) ||
            !iLoadXrProc(loader, "xrEndSession", &xrEndSession) ||
            !iLoadXrProc(loader, "xrWaitFrame", &xrWaitFrame) ||
            !iLoadXrProc(loader, "xrBeginFrame", &xrBeginFrame) ||
            !iLoadXrProc(loader, "xrEndFrame", &xrEndFrame) ||
            !iLoadXrProc(loader, "xrEnumerateSwapchainFormats", &xrEnumerateSwapchainFormats) ||
            !iLoadXrProc(loader, "xrCreateSwapchain", &xrCreateSwapchain) ||
            !iLoadXrProc(loader, "xrDestroySwapchain", &xrDestroySwapchain) ||
            !iLoadXrProc(loader, "xrEnumerateSwapchainImages", &xrEnumerateSwapchainImages) ||
            !iLoadXrProc(loader, "xrAcquireSwapchainImage", &xrAcquireSwapchainImage) ||
            !iLoadXrProc(loader, "xrWaitSwapchainImage", &xrWaitSwapchainImage) ||
            !iLoadXrProc(loader, "xrReleaseSwapchainImage", &xrReleaseSwapchainImage))
        {
            log->Printf(LT_ERROR, L"IMM_OPENXR_STANDALONE missing=requiredExport");
            FreeLibrary(loader);
            return false;
        }

        uint32_t extensionCount = 0;
        int32_t xr = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateExtensionsResult=%d count=%u", xr, extensionCount);
        if (xr != kXrSuccess)
        {
            FreeLibrary(loader);
            return false;
        }

        XrExtensionPropertiesImm extensions[64] = {};
        const uint32_t extensionCapacity = extensionCount < 64 ? extensionCount : 64;
        for (uint32_t i = 0; i < extensionCapacity; ++i)
        {
            extensions[i].type = kXrTypeExtensionProperties;
        }
        uint32_t filledExtensionCount = extensionCapacity;
        xr = xrEnumerateInstanceExtensionProperties(nullptr, extensionCapacity, &filledExtensionCount, extensions);
        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateExtensionsFillResult=%d count=%u", xr, filledExtensionCount);
        if (xr == kXrSuccess)
        {
            const uint32_t logCount = filledExtensionCount < 12 ? filledExtensionCount : 12;
            for (uint32_t i = 0; i < logCount; ++i)
            {
                iLogOpenXRText(log, L"extension", extensions[i].extensionName);
            }
        }
        else
        {
            FreeLibrary(loader);
            return false;
        }

        XrInstanceCreateInfoImm createInfo = {};
        createInfo.type = kXrTypeInstanceCreateInfo;
        strcpy(createInfo.applicationInfo.applicationName, "IMM Standalone");
        createInfo.applicationInfo.applicationVersion = 1;
        strcpy(createInfo.applicationInfo.engineName, "IMM");
        createInfo.applicationInfo.engineVersion = 1;
        createInfo.applicationInfo.apiVersion = kXrCurrentApiVersion;

        uint64_t instance = 0;
        xr = xrCreateInstance(&createInfo, &instance);
        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE createInstanceResult=%d resultName=%s instance=%llu", xr, iXrResultName(xr), static_cast<unsigned long long>(instance));
        if (xr != kXrSuccess)
        {
            FreeLibrary(loader);
            return false;
        }

        XrSystemGetInfoImm systemInfo = {};
        systemInfo.type = kXrTypeSystemGetInfo;
        systemInfo.formFactor = kXrFormFactorHmd;
        uint64_t systemId = 0;
        xr = xrGetSystem(instance, &systemInfo, &systemId);
        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getHmdSystemResult=%d resultName=%s systemId=%llu", xr, iXrResultName(xr), static_cast<unsigned long long>(systemId));
        if (xr == kXrSuccess)
        {
            PFN_xrGetVulkanInstanceExtensionsKhrImm xrGetVulkanInstanceExtensionsKHR = nullptr;
            PFN_xrGetVulkanDeviceExtensionsKhrImm xrGetVulkanDeviceExtensionsKHR = nullptr;
            PFN_xrGetVulkanGraphicsRequirementsKhrImm xrGetVulkanGraphicsRequirementsKHR = nullptr;
            void *xrProc = nullptr;
            int32_t procResult = xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR", &xrProc);
            xrGetVulkanInstanceExtensionsKHR = reinterpret_cast<PFN_xrGetVulkanInstanceExtensionsKhrImm>(xrProc);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanInstanceExtensionsProcResult=%d resultName=%s available=%u", procResult, iXrResultName(procResult), xrGetVulkanInstanceExtensionsKHR ? 1u : 0u);
            xrProc = nullptr;
            procResult = xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR", &xrProc);
            xrGetVulkanDeviceExtensionsKHR = reinterpret_cast<PFN_xrGetVulkanDeviceExtensionsKhrImm>(xrProc);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanDeviceExtensionsProcResult=%d resultName=%s available=%u", procResult, iXrResultName(procResult), xrGetVulkanDeviceExtensionsKHR ? 1u : 0u);
            xrProc = nullptr;
            procResult = xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR", &xrProc);
            xrGetVulkanGraphicsRequirementsKHR = reinterpret_cast<PFN_xrGetVulkanGraphicsRequirementsKhrImm>(xrProc);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanGraphicsRequirementsProcResult=%d resultName=%s available=%u", procResult, iXrResultName(procResult), xrGetVulkanGraphicsRequirementsKHR ? 1u : 0u);

            if (xrGetVulkanInstanceExtensionsKHR)
            {
                uint32_t vulkanInstanceExtensionBytes = 0;
                xr = xrGetVulkanInstanceExtensionsKHR(instance, systemId, 0, &vulkanInstanceExtensionBytes, nullptr);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanInstanceExtensionsResult=%d resultName=%s bytes=%u", xr, iXrResultName(xr), vulkanInstanceExtensionBytes);
                if (xr == kXrSuccess && vulkanInstanceExtensionBytes > 0 && vulkanInstanceExtensionBytes < 512)
                {
                    char vulkanInstanceExtensions[512] = {};
                    xr = xrGetVulkanInstanceExtensionsKHR(instance, systemId, vulkanInstanceExtensionBytes, &vulkanInstanceExtensionBytes, vulkanInstanceExtensions);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanInstanceExtensionsFillResult=%d resultName=%s bytes=%u", xr, iXrResultName(xr), vulkanInstanceExtensionBytes);
                    if (xr == kXrSuccess)
                    {
                        iLogOpenXRText(log, L"vulkanInstanceExtensions", vulkanInstanceExtensions);
                    }
                }
            }

            if (xr == kXrSuccess && xrGetVulkanDeviceExtensionsKHR)
            {
                uint32_t vulkanDeviceExtensionBytes = 0;
                xr = xrGetVulkanDeviceExtensionsKHR(instance, systemId, 0, &vulkanDeviceExtensionBytes, nullptr);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanDeviceExtensionsResult=%d resultName=%s bytes=%u", xr, iXrResultName(xr), vulkanDeviceExtensionBytes);
                if (xr == kXrSuccess && vulkanDeviceExtensionBytes > 0 && vulkanDeviceExtensionBytes < 512)
                {
                    char vulkanDeviceExtensions[512] = {};
                    xr = xrGetVulkanDeviceExtensionsKHR(instance, systemId, vulkanDeviceExtensionBytes, &vulkanDeviceExtensionBytes, vulkanDeviceExtensions);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanDeviceExtensionsFillResult=%d resultName=%s bytes=%u", xr, iXrResultName(xr), vulkanDeviceExtensionBytes);
                    if (xr == kXrSuccess)
                    {
                        iLogOpenXRText(log, L"vulkanDeviceExtensions", vulkanDeviceExtensions);
                    }
                }
            }

            if (xr == kXrSuccess && xrGetVulkanGraphicsRequirementsKHR)
            {
                XrGraphicsRequirementsVulkanKhrImm graphicsRequirements = {};
                graphicsRequirements.type = kXrTypeGraphicsRequirementsVulkanKhr;
                xr = xrGetVulkanGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getVulkanGraphicsRequirementsResult=%d resultName=%s minApi=0x%llx maxApi=0x%llx",
                            xr,
                            iXrResultName(xr),
                            static_cast<unsigned long long>(graphicsRequirements.minApiVersionSupported),
                            static_cast<unsigned long long>(graphicsRequirements.maxApiVersionSupported));
            }

            XrSystemPropertiesImm properties = {};
            properties.type = kXrTypeSystemProperties;
            xr = xrGetSystemProperties(instance, systemId, &properties);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE getSystemPropertiesResult=%d", xr);
            if (xr == kXrSuccess)
            {
                iLogOpenXRText(log, L"systemName", properties.systemName);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE maxSwapchain=%ux%u maxLayers=%u",
                            properties.graphicsProperties.maxSwapchainImageWidth,
                            properties.graphicsProperties.maxSwapchainImageHeight,
                            properties.graphicsProperties.maxLayerCount);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE tracking orientation=%u position=%u",
                            properties.trackingProperties.orientationTracking,
                            properties.trackingProperties.positionTracking);
            }

            uint32_t viewConfigurationCount = 0;
            xr = xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigurationCount, nullptr);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateViewConfigurationsResult=%d count=%u", xr, viewConfigurationCount);
            int32_t viewConfigurationTypes[8] = {};
            if (xr == kXrSuccess && viewConfigurationCount > 0)
            {
                uint32_t viewCapacity = viewConfigurationCount < 8 ? viewConfigurationCount : 8;
                xr = xrEnumerateViewConfigurations(instance, systemId, viewCapacity, &viewCapacity, viewConfigurationTypes);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateViewConfigurationsFillResult=%d count=%u", xr, viewCapacity);
                for (uint32_t i = 0; xr == kXrSuccess && i < viewCapacity; ++i)
                {
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE viewConfigurationType=%d", viewConfigurationTypes[i]);
                }
            }

            uint32_t stereoViewCount = 0;
            xr = xrEnumerateViewConfigurationViews(instance, systemId, kXrViewConfigurationPrimaryStereo, 0, &stereoViewCount, nullptr);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateStereoViewsResult=%d count=%u", xr, stereoViewCount);
            if (xr == kXrSuccess && stereoViewCount > 0 && stereoViewCount <= 8)
            {
                XrViewConfigurationViewImm views[8] = {};
                for (uint32_t i = 0; i < stereoViewCount; ++i)
                {
                    views[i].type = kXrTypeViewConfigurationView;
                }
                xr = xrEnumerateViewConfigurationViews(instance, systemId, kXrViewConfigurationPrimaryStereo, stereoViewCount, &stereoViewCount, views);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateStereoViewsFillResult=%d count=%u", xr, stereoViewCount);
                for (uint32_t i = 0; xr == kXrSuccess && i < stereoViewCount; ++i)
                {
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE stereoView[%u] recommended=%ux%u max=%ux%u samples=%u/%u",
                                i,
                                views[i].recommendedImageRectWidth,
                                views[i].recommendedImageRectHeight,
                                views[i].maxImageRectWidth,
                                views[i].maxImageRectHeight,
                                views[i].recommendedSwapchainSampleCount,
                                views[i].maxSwapchainSampleCount);
                }
            }

            XrSessionCreateInfoImm sessionCreateInfo = {};
            sessionCreateInfo.type = kXrTypeSessionCreateInfo;
            sessionCreateInfo.systemId = systemId;
            XrGraphicsBindingVulkanKhrImm graphicsBinding = {};
            graphicsBinding.type = kXrTypeGraphicsBindingVulkanKhr;
            graphicsBinding.instance = kFakeVulkanInstance;
            graphicsBinding.physicalDevice = kFakeVulkanPhysicalDevice;
            graphicsBinding.device = kFakeVulkanDevice;
            graphicsBinding.queueFamilyIndex = 7;
            graphicsBinding.queueIndex = 1;
            sessionCreateInfo.next = &graphicsBinding;
            log->Printf(LT_MESSAGE,
                        L"IMM_OPENXR_STANDALONE vulkanGraphicsBinding type=%d instance=0x%llx physicalDevice=0x%llx device=0x%llx queueFamily=%u queueIndex=%u",
                        graphicsBinding.type,
                        static_cast<unsigned long long>(graphicsBinding.instance),
                        static_cast<unsigned long long>(graphicsBinding.physicalDevice),
                        static_cast<unsigned long long>(graphicsBinding.device),
                        graphicsBinding.queueFamilyIndex,
                        graphicsBinding.queueIndex);
            uint64_t session = 0;
            xr = xrCreateSession(instance, &sessionCreateInfo, &session);
            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE createSessionResult=%d resultName=%s session=%llu", xr, iXrResultName(xr), static_cast<unsigned long long>(session));
            if (xr == kXrSuccess)
            {
                uint64_t swapchain = 0;
                XrSwapchainCreateInfoImm swapchainCreateInfo = {};
                uint32_t formatCount = 0;
                xr = xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateSwapchainFormatsResult=%d resultName=%s count=%u", xr, iXrResultName(xr), formatCount);
                int64_t swapchainFormats[8] = {};
                if (xr == kXrSuccess && formatCount > 0)
                {
                    uint32_t formatCapacity = formatCount < 8 ? formatCount : 8;
                    xr = xrEnumerateSwapchainFormats(session, formatCapacity, &formatCapacity, swapchainFormats);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateSwapchainFormatsFillResult=%d resultName=%s count=%u firstFormat=%lld", xr, iXrResultName(xr), formatCapacity, static_cast<long long>(swapchainFormats[0]));
                }

                if (xr == kXrSuccess)
                {
                    swapchainCreateInfo.type = kXrTypeSwapchainCreateInfo;
                    swapchainCreateInfo.usageFlags = kXrSwapchainUsageColorAttachmentBit;
                    swapchainCreateInfo.format = swapchainFormats[0];
                    swapchainCreateInfo.sampleCount = 1;
                    swapchainCreateInfo.width = 1600;
                    swapchainCreateInfo.height = 1600;
                    swapchainCreateInfo.faceCount = 1;
                    swapchainCreateInfo.arraySize = 2;
                    swapchainCreateInfo.mipCount = 1;
                    xr = xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE createSwapchainResult=%d resultName=%s swapchain=%llu", xr, iXrResultName(xr), static_cast<unsigned long long>(swapchain));
                }

                if (xr == kXrSuccess)
                {
                    uint32_t imageCount = 0;
                    xr = xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateSwapchainImagesResult=%d resultName=%s count=%u", xr, iXrResultName(xr), imageCount);
                    if (xr == kXrSuccess && imageCount > 0 && imageCount <= 4)
                    {
                        XrSwapchainImageVulkanKhrImm images[4] = {};
                        for (uint32_t i = 0; i < imageCount; ++i)
                        {
                            images[i].type = kXrTypeSwapchainImageVulkanKhr;
                        }
                        xr = xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, images);
                        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE enumerateSwapchainImagesFillResult=%d resultName=%s count=%u firstVkImage=0x%llx", xr, iXrResultName(xr), imageCount, static_cast<unsigned long long>(images[0].image));
                        if (xr == kXrSuccess && imageCount > 0)
                        {
                            log->Printf(LT_MESSAGE,
                                        L"IMM_OPENXR_STANDALONE rendererExternalImageFrameCandidate image=0x%llx vkFormat=%lld width=%u height=%u arrayLayers=%u",
                                        static_cast<unsigned long long>(images[0].image),
                                        static_cast<long long>(swapchainCreateInfo.format),
                                        swapchainCreateInfo.width,
                                        swapchainCreateInfo.height,
                                        swapchainCreateInfo.arraySize);
                        }
                    }
                }

                XrSessionBeginInfoImm sessionBeginInfo = {};
                sessionBeginInfo.type = kXrTypeSessionBeginInfo;
                sessionBeginInfo.primaryViewConfigurationType = kXrViewConfigurationPrimaryStereo;
                if (xr == kXrSuccess)
                {
                    xr = xrBeginSession(session, &sessionBeginInfo);
                }
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE beginSessionResult=%d resultName=%s", xr, iXrResultName(xr));

                if (xr == kXrSuccess)
                {
                    XrFrameWaitInfoImm frameWaitInfo = {};
                    frameWaitInfo.type = kXrTypeFrameWaitInfo;
                    XrFrameStateImm frameState = {};
                    frameState.type = kXrTypeFrameState;
                    xr = xrWaitFrame(session, &frameWaitInfo, &frameState);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE waitFrameResult=%d resultName=%s shouldRender=%u predictedDisplayTime=%lld", xr, iXrResultName(xr), frameState.shouldRender, static_cast<long long>(frameState.predictedDisplayTime));

                    if (xr == kXrSuccess)
                    {
                        XrFrameBeginInfoImm frameBeginInfo = {};
                        frameBeginInfo.type = kXrTypeFrameBeginInfo;
                        xr = xrBeginFrame(session, &frameBeginInfo);
                        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE beginFrameResult=%d resultName=%s", xr, iXrResultName(xr));

                        if (xr == kXrSuccess)
                        {
                            uint32_t swapchainImageIndex = 0;
                            XrSwapchainImageAcquireInfoImm acquireInfo = {};
                            acquireInfo.type = kXrTypeSwapchainImageAcquireInfo;
                            xr = xrAcquireSwapchainImage(swapchain, &acquireInfo, &swapchainImageIndex);
                            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE acquireSwapchainImageResult=%d resultName=%s index=%u", xr, iXrResultName(xr), swapchainImageIndex);
                        }

                        if (xr == kXrSuccess)
                        {
                            XrSwapchainImageWaitInfoImm waitInfo = {};
                            waitInfo.type = kXrTypeSwapchainImageWaitInfo;
                            waitInfo.timeout = 0;
                            xr = xrWaitSwapchainImage(swapchain, &waitInfo);
                            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE waitSwapchainImageResult=%d resultName=%s", xr, iXrResultName(xr));
                        }

                        if (xr == kXrSuccess)
                        {
                            XrSwapchainImageReleaseInfoImm releaseInfo = {};
                            releaseInfo.type = kXrTypeSwapchainImageReleaseInfo;
                            xr = xrReleaseSwapchainImage(swapchain, &releaseInfo);
                            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE releaseSwapchainImageResult=%d resultName=%s", xr, iXrResultName(xr));
                        }

                        if (xr == kXrSuccess)
                        {
                            XrCompositionLayerProjectionViewImm projectionViews[2] = {};
                            for (uint32_t i = 0; i < 2; ++i)
                            {
                                projectionViews[i].type = kXrTypeCompositionLayerProjectionView;
                                projectionViews[i].pose.orientation.w = 1.0f;
                                projectionViews[i].fov.angleLeft = -0.7f;
                                projectionViews[i].fov.angleRight = 0.7f;
                                projectionViews[i].fov.angleUp = 0.7f;
                                projectionViews[i].fov.angleDown = -0.7f;
                                projectionViews[i].subImage.swapchain = swapchain;
                                projectionViews[i].subImage.imageRect.extent.width = 1600;
                                projectionViews[i].subImage.imageRect.extent.height = 1600;
                                projectionViews[i].subImage.imageArrayIndex = i;
                            }
                            XrCompositionLayerProjectionImm projectionLayer = {};
                            projectionLayer.type = kXrTypeCompositionLayerProjection;
                            projectionLayer.space = 0;
                            projectionLayer.viewCount = 2;
                            projectionLayer.views = projectionViews;
                            const void *layers[1] = { &projectionLayer };
                            XrFrameEndInfoImm frameEndInfo = {};
                            frameEndInfo.type = kXrTypeFrameEndInfo;
                            frameEndInfo.displayTime = frameState.predictedDisplayTime;
                            frameEndInfo.environmentBlendMode = kXrEnvironmentBlendModeOpaque;
                            frameEndInfo.layerCount = 1;
                            frameEndInfo.layers = layers;
                            xr = xrEndFrame(session, &frameEndInfo);
                            log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE endFrameResult=%d resultName=%s layerCount=%u projectionViews=%u", xr, iXrResultName(xr), frameEndInfo.layerCount, projectionLayer.viewCount);
                        }
                    }

                    const int32_t endSessionResult = xrEndSession(session);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE endSessionResult=%d resultName=%s", endSessionResult, iXrResultName(endSessionResult));
                    if (xr == kXrSuccess)
                    {
                        xr = endSessionResult;
                    }
                }

                if (swapchain != 0)
                {
                    const int32_t destroySwapchainResult = xrDestroySwapchain(swapchain);
                    log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE destroySwapchainResult=%d resultName=%s", destroySwapchainResult, iXrResultName(destroySwapchainResult));
                    if (xr == kXrSuccess)
                    {
                        xr = destroySwapchainResult;
                    }
                }

                const int32_t destroySessionResult = xrDestroySession(session);
                log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE destroySessionResult=%d resultName=%s", destroySessionResult, iXrResultName(destroySessionResult));
                if (xr == kXrSuccess)
                {
                    xr = destroySessionResult;
                }
            }
        }

        const int32_t destroyResult = xrDestroyInstance(instance);
        log->Printf(LT_MESSAGE, L"IMM_OPENXR_STANDALONE destroyInstanceResult=%d", destroyResult);
        FreeLibrary(loader);
        return xr == kXrSuccess && destroyResult == kXrSuccess;
    }
}
#endif


class MainRenderReporter : public piRenderer::piReporter
{
private:
    piLog* mLog;
    wchar_t tmp[2048];

public:
    MainRenderReporter(piLog* log) : piRenderer::piReporter() { mLog = log; }
    virtual ~MainRenderReporter() {}
    void Info(const char* str)
    {
        pistr2ws(tmp, 2048, str);
#if defined(WINDOWS)
        mLog->Printf(LT_MESSAGE, L"%s", tmp);
#else
        mLog->Printf(LT_MESSAGE, L"%ls", tmp);
#endif
    }
    void Error(const char* str, int level)
    {
        pistr2ws(tmp, 2048, str);
#if defined(WINDOWS)
        mLog->Printf(LT_ERROR, L"%s", tmp);
#else
        mLog->Printf(LT_ERROR, L"%ls", tmp);
#endif
    }
    void Begin(uint64_t memCurrent, uint64_t memPeak, int texCurrent, int texPeak)
    {
        mLog->Printf(LT_MESSAGE, L"---- Renderer Report ---- ");
        mLog->Printf(LT_MESSAGE, L"Max Used : %d MB in %d textures and buffers", memPeak >> 20, texPeak);
        mLog->Printf(LT_MESSAGE, L"Leaked   : %d MB in %d textures and buffer", memCurrent >> 20, texCurrent);
    }
    void End(void)
    {
        mLog->Printf(LT_MESSAGE, L"---- Renderer Report ---- ");
    }
    void Texture(const wchar_t* key, uint64_t kb, piRenderer::Format format, bool compressed, int xres, int yres, int zres)
    {
        mLog->Printf(LT_MESSAGE, L"* Texture: %5d kb, %4d x %4d x %4d %2d (%s)", (int)kb, xres, yres, zres, format, (key == nullptr) ? L"null" : key);
    }
};

//----------------------------------------------------------------------------------

static float iDecodeUnsignedFloat(uint32_t bits, int mantissaBits)
{
    const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const uint32_t mantissa = bits & mantissaMask;
    const uint32_t exponent = (bits >> mantissaBits) & 0x1fu;
    if (exponent == 0)
    {
        return ldexpf((float)mantissa / (float)(1u << mantissaBits), -14);
    }
    if (exponent == 31)
    {
        return 1.0f;
    }
    return ldexpf(1.0f + (float)mantissa / (float)(1u << mantissaBits), (int)exponent - 15);
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

static bool iPathHasExtension(const char *path, const char *extension)
{
    if (!path || !extension)
    {
        return false;
    }
    const size_t pathLength = strlen(path);
    const size_t extensionLength = strlen(extension);
    if (pathLength < extensionLength)
    {
        return false;
    }
    const char *tail = path + pathLength - extensionLength;
    for (size_t i = 0; i < extensionLength; ++i)
    {
        char a = tail[i];
        char b = extension[i];
        if (a >= 'A' && a <= 'Z')
        {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}

static void iDecodeRG11B10Pixels(uint8_t *dstRGB, const uint32_t *pixels, int width, int height)
{
    for (int y = 0; y < height; ++y)
    {
        const uint32_t *srcRow = pixels + (size_t)y * (size_t)width;
        uint8_t *dstRow = dstRGB + (size_t)y * (size_t)width * 3u;
        for (int x = 0; x < width; ++x)
        {
            const uint32_t pixel = srcRow[x];
            dstRow[3 * x + 0] = iFloatToByte(iDecodeUnsignedFloat(pixel & 0x7ffu, 6));
            dstRow[3 * x + 1] = iFloatToByte(iDecodeUnsignedFloat((pixel >> 11u) & 0x7ffu, 6));
            dstRow[3 * x + 2] = iFloatToByte(iDecodeUnsignedFloat((pixel >> 22u) & 0x3ffu, 5));
        }
    }
}

static bool iWriteRG11B10PPM(const char *path, const uint32_t *pixels, int width, int height)
{
    if (!path || !path[0] || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file)
    {
        return false;
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);
    uint8_t *rgb = (uint8_t*)malloc((size_t)width * (size_t)height * 3u);
    if (!rgb)
    {
        fclose(file);
        return false;
    }
    iDecodeRG11B10Pixels(rgb, pixels, width, height);
    const bool ok = fwrite(rgb, 3u, (size_t)width * (size_t)height, file) == (size_t)width * (size_t)height;
    free(rgb);

    fclose(file);
    return ok;
}

static bool iWriteRG11B10PNG(const char *path, const uint32_t *pixels, int width, int height)
{
    if (!path || !path[0] || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    uint8_t *rgb = (uint8_t*)malloc((size_t)width * (size_t)height * 3u);
    if (!rgb)
    {
        return false;
    }
    iDecodeRG11B10Pixels(rgb, pixels, width, height);

    wchar_t *widePath = pistr2ws(path);
    if (!widePath)
    {
        free(rgb);
        return false;
    }

    piImage image;
    image.InitWrap(piImage::TYPE_2D, width, height, 1, piImage::FORMAT_I_RGB, rgb);
    const bool ok = image.WriteToDisk(widePath, 0, L"png");
    image.Free();
    free(widePath);
    free(rgb);
    return ok;
}

static bool iWriteRG11B10Capture(const char *path, const uint32_t *pixels, int width, int height)
{
    if (iPathHasExtension(path, ".png"))
    {
        return iWriteRG11B10PNG(path, pixels, width, height);
    }
    return iWriteRG11B10PPM(path, pixels, width, height);
}

static bool iFileExistsUtf8(const char *path)
{
    if (!path || !path[0])
    {
        return false;
    }

    wchar_t *widePath = pistr2ws(path);
    if (!widePath)
    {
        return false;
    }

    const bool exists = piFile::Exists(widePath);
    free(widePath);
    return exists;
}

static const char *iGetValidationEnv(const char *genericName, const char *legacyName)
{
    const char *value = getenv(genericName);
    if (value && value[0])
    {
        return value;
    }
    return getenv(legacyName);
}

static bool iHasImmExtension(const wchar_t *path)
{
    if (!path)
        return false;
    const wchar_t *dot = wcsrchr(path, L'.');
#if defined(WINDOWS)
    return dot && _wcsicmp(dot, L".imm") == 0;
#else
    return dot && wcscasecmp(dot, L".imm") == 0;
#endif
}

static bool iSetSingleLoadedFile(ExePlayer::Settings *settings, const wchar_t *path)
{
    if (!settings || !path || !path[0])
        return false;

    for (ImmCore::piString &load : settings->mFiles.mLoad)
    {
        load.End();
    }
    settings->mFiles.mLoad.SetLength(0);

    ImmCore::piString *fileToLoad = settings->mFiles.mLoad.GetAddress(0);
    new (fileToLoad) ImmCore::piString();
    settings->mFiles.mLoad.SetLength(1);
    return fileToLoad && fileToLoad->InitCopyW(path);
}

static bool iFindFirstImmBesideExecutable(const wchar_t *executablePath, wchar_t *dst, size_t dstLen)
{
    if (!executablePath || !dst || dstLen == 0)
        return false;

    wchar_t directory[PATH_MAX] = {};
    wcsncpy(directory, executablePath, PATH_MAX - 1);
    wchar_t *slash = wcsrchr(directory, L'\\');
    wchar_t *forwardSlash = wcsrchr(directory, L'/');
    if (!slash || (forwardSlash && forwardSlash > slash))
        slash = forwardSlash;
    if (!slash)
        return false;
    *slash = 0;

#if defined(WINDOWS)
    wchar_t pattern[PATH_MAX] = {};
    swprintf(pattern, PATH_MAX, L"%s\\*.imm", directory);
    WIN32_FIND_DATAW findData = {};
    HANDLE findHandle = FindFirstFileW(pattern, &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            swprintf(dst, dstLen, L"%s\\%s", directory, findData.cFileName);
            found = true;
            break;
        }
    } while (FindNextFileW(findHandle, &findData));
    FindClose(findHandle);
    return found;
#else
    (void)iHasImmExtension;
    return false;
#endif
}

static bool iApplyDefaultImmWhenNoLoadConfigured(ExePlayer::Settings *settings, const wchar_t *executablePath, ImmCore::piLog *log)
{
    if (!settings || settings->mFiles.mLoad.GetLength() > 0)
        return true;

    wchar_t defaultImmPath[PATH_MAX] = {};
    if (!iFindFirstImmBesideExecutable(executablePath, defaultImmPath, PATH_MAX))
        return true;
    if (!iSetSingleLoadedFile(settings, defaultImmPath))
        return false;

#if defined(WINDOWS)
    if (log)
        log->Printf(LT_MESSAGE, L"Using default IMM beside executable: %s", defaultImmPath);
#else
    if (log)
        log->Printf(LT_MESSAGE, L"Using default IMM beside executable: %ls", defaultImmPath);
#endif
    return true;
}

#if !defined(ANDROID)
#if defined(WINDOWS)
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif
#endif


int piMainFunc(const wchar_t* path, const wchar_t** args, int numArgs, void* instance)
{
    const char *validationFrameEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_FRAME", "IMM_GL_VALIDATE_FRAME");
    const bool validationRequested = validationFrameEnv && validationFrameEnv[0];
    const int validationStartupExitCode = validationRequested ? 2 : 0;

    ExePlayer::Viewer mViewer;
    piLog mLog;
    int  mSuperSample;
    piWindowMgr mWinMgr;
    piWindow mWindow;
    piRenderer* mRenderer;
    piSoundEngineBackend* mSoundEngineBackend;
    piRenderer::piReporter* mRenderReporter;
    #if DISABLE_VR==0
    piVRHMD* mHMD;
    piShader mMirrorShader;
    #endif
    ivec2 mWindowSize;
    Settings mSettings;
    piTimer       mTimer;
    ImmPlayer::StereoMode mStereoMode;
    piTexture mColorTextureM;
    piTexture mDepthTextureM;
    piRTarget mRenderTargetM;
    ivec2 mRenderSize;

    Resolve mResolve;
    #if DISABLE_VR==0
    struct TextureChain
    {
        int mRenderNumTextures;
        piTexture mRenderTexture[32];
        piRTarget mRenderTarget[32];
    } mTextureChain[2];
    #endif

#ifdef DEBUG
    //_controlfp(_EM_UNDERFLOW | _EM_INEXACT, _MCW_EM);
#endif

    if (!mLog.Init(L"debug.txt", PILOG_TXT + PILOG_CNS))
        return false;

    if (!mTimer.Init())
        return false;

    //------------------------

    const wchar_t* settingsFileName = L"settings.json";
    if (numArgs > 1)
    {
        settingsFileName = args[1];
    }
#if defined(WINDOWS)
    mLog.Printf(LT_MESSAGE, L"Reading config file \"%s\"...", settingsFileName);
#else
    mLog.Printf(LT_MESSAGE, L"Reading config file \"%ls\"...", settingsFileName);
#endif

    if (!mSettings.Init(settingsFileName, &mLog))
    {
        mLog.Printf(LT_ERROR, L"Failed to load settings");
        return false;
    }
    mLog.Printf(LT_MESSAGE, L"Settings loaded");
    if (!iApplyDefaultImmWhenNoLoadConfigured(&mSettings, path, &mLog))
    {
        mLog.Printf(LT_ERROR, L"Failed to apply default IMM file");
        return false;
    }

    mSuperSample = mSettings.mRendering.mSupersampling;
    if (mSuperSample < 1) { mSuperSample = 1; mLog.Printf(LT_WARNING, L"Supersampling factor must be between 1 and 3 (1 sample per pixel and 3x3 samples per pixel"); }
    if (mSuperSample > 3) { mSuperSample = 3; mLog.Printf(LT_WARNING, L"Supersampling factor must be between 1 and 3 (1 sample per pixel and 3x3 samples per pixel"); }


    mWinMgr = piWindowMgr_Init();
    if (!mWinMgr)
    {
        mLog.Printf(LT_ERROR, L"Failed to init window manager");
        mSettings.End();
        return false;
    }
    mWindow = piWindow_init(mWinMgr, L"rendering", mSettings.mWindow.mPositionX, mSettings.mWindow.mPositionY, mSettings.mWindow.mWidth, mSettings.mWindow.mHeight, mSettings.mWindow.mFullScreen, !mSettings.mWindow.mFullScreen, false, mSettings.mWindow.mFullScreen);
    if (!mWindow)
    {
        mLog.Printf(LT_ERROR, L"Failed to create window");
        mSettings.End();
        return false;
    }
    piWindow_show(mWindow);

    const wchar_t *renderingBackend =
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::DX) ? L"DirectX" :
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Metal) ? L"Metal" :
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Vulkan) ? L"Vulkan" :
        (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::GLES) ? L"OpenGLES" :
        L"OpenGL";
    mLog.Printf(LT_MESSAGE, L"Rendering Backened: %s", renderingBackend);
    mLog.Printf(LT_MESSAGE, L"Rendering Technique: %s", (mSettings.mRendering.mRenderingTechnique==Settings::Rendering::Technique::Static)?L"Static":L"Pretessellated" );
    mLog.Printf(LT_MESSAGE, L"XR Runtime: %s", (mSettings.mRendering.mXRRuntime == Settings::Rendering::XRRuntime::OpenXR) ? L"OpenXR" : L"Legacy");
    #if DISABLE_VR==0
    mLog.Printf(LT_MESSAGE, L"Rendering in VR: %s", (mSettings.mRendering.mEnableVR) ? L"yes" : L"no");
    #else
    mLog.Printf(LT_MESSAGE, L"Rendering in VR: no" );;
    #endif

    // renderer
    mRenderReporter = new MainRenderReporter(&mLog);

    piRenderer::API rendererAPI = piRenderer::API::GL;
    if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::DX)
    {
        rendererAPI = piRenderer::API::DX;
    }
    else if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::GLES)
    {
        rendererAPI = piRenderer::API::GLES;
    }
    else if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Metal)
    {
        rendererAPI = piRenderer::API::Metal;
    }
    else if (mSettings.mRendering.mRenderingAPI == Settings::Rendering::API::Vulkan)
    {
        rendererAPI = piRenderer::API::Vulkan;
    }

    mRenderer = piRenderer::Create(rendererAPI);
    if (!mRenderer)
    {
        mSettings.End();
        return false;
    }

    // renderer
    const void* hwnds[1] = { piWindow_getHandle(mWindow) };
    bool disableRendererErrors = false; // can set this to true
    if (!mRenderer->Initialize(0, hwnds, 1, true, disableRendererErrors, mRenderReporter, true, nullptr))
    {
        mLog.Printf(LT_ERROR, L"Can't create renderer");
        {
            mSettings.End();
            return false;
        }
    }
    mRenderer->SetActiveWindow(0);
    mLog.Printf(LT_MESSAGE, L"Renderer initialized");

    //=============
    mWindowSize = ivec2(mSettings.mWindow.mWidth, mSettings.mWindow.mHeight);

    #if DISABLE_VR==0
    if (mSettings.mRendering.mEnableVR)
    {
        mHMD = nullptr;

        if (mSettings.mRendering.mXRRuntime == Settings::Rendering::XRRuntime::OpenXR)
        {
            if (iProbeStandaloneOpenXR(&mLog))
            {
                mLog.Printf(LT_ERROR, L"OpenXR standalone startup probe passed; the OpenXR VR backend is not implemented yet");
            }
            else
            {
                mLog.Printf(LT_ERROR, L"OpenXR standalone startup probe failed; the OpenXR VR backend is not implemented yet");
            }
            mSettings.End();
            return false;
        }

        float pd = mSettings.mRendering.mPixelDensity;
        if (pd < 0.1f) { pd = 0.1f; mLog.Printf(LT_WARNING, L"Pixel Density too small"); }
        if (pd > 3.0f) { pd = 3.0f; mLog.Printf(LT_WARNING, L"Pixel Density too big"); }

        mHMD = piVRHMD::Create(piVRHMD::ANY_AVAILABLE, nullptr, 0, pd, &mLog, &mTimer);
        if (mHMD == nullptr)
        {
            mLog.Printf(LT_ERROR, L"Cannot do VR");
            mSettings.End();
            return false;
        }
        mLog.Printf(LT_MESSAGE,
                    L"IMM_LEGACY_VR_SMOKE hmd_initialized type=%d renderSize=%dx%d pixelDensity=%.3f",
                    static_cast<int>(mHMD->mType),
                    mHMD->mInfo.mVRXres,
                    mHMD->mInfo.mVRYres,
                    pd);
        mStereoMode = ImmPlayer::StereoMode::Preferred;
        mRenderSize = ivec2(mHMD->mInfo.mVRXres, mHMD->mInfo.mVRYres);

        //-----------------------
        if (mHMD->mType == piVRHMD::Oculus_Rift || mHMD->mType == piVRHMD::Oculus_RiftS || mHMD->mType == piVRHMD::Oculus_Quest)
        {
            if (!mHMD->AttachToWindow(true, mWindowSize.x, mWindowSize.y))
            {
                mSettings.End();
                return false;
            }
            for (int j = 0; j < 2; j++)
            {
                TextureChain* tc = mTextureChain + j;
                tc->mRenderNumTextures = mHMD->mInfo.mTexture[j].mNum;
                for (int i = 0; i < tc->mRenderNumTextures; i++)
                {
                    tc->mRenderTexture[i] = mRenderer->CreateTextureFromID(mHMD->mInfo.mTexture[j].mTexIDColor[i], piRenderer::TextureFilter::MIPMAP);
                    tc->mRenderTarget[i] = mRenderer->CreateRenderTarget(tc->mRenderTexture[i], 0, 0, 0, 0);
                    if (!tc->mRenderTarget[i])
                    {
                        mSettings.End();
                        return false;
                    }
                }
            }

        }
        else if (mHMD->mType == piVRHMD::HTC_Vive)
        {
            for (int j = 0; j < 2; j++)
            {
                TextureChain* tc = mTextureChain + j;
                tc->mRenderNumTextures = mHMD->mInfo.mTexture[j].mNum;
                for (int i = 0; i < tc->mRenderNumTextures; i++)
                {
                    const piRenderer::TextureInfo infocm = { piRenderer::TextureType::T2D, piRenderer::Format::C4_8_UNORM_SRGB, mRenderSize.x, mRenderSize.y, 1, 1 };
                    tc->mRenderTexture[i] = mRenderer->CreateTexture(0, &infocm, false, piRenderer::TextureFilter::LINEAR, piRenderer::TextureWrap::CLAMP, 1.0f, 0);
                    if (!tc->mRenderTexture[i])
                    {
                        mSettings.End();
                        return false;
                    }
                    tc->mRenderTarget[i] = mRenderer->CreateRenderTarget(tc->mRenderTexture[i], 0, 0, 0, 0);
                    if (!tc->mRenderTarget[i])
                    {
                        mSettings.End();
                        return false;
                    }
                }
            }

            piRenderer::TextureInfo info[2];
            mRenderer->GetTextureInfo(mTextureChain[0].mRenderTexture[0], info + 0);
            mRenderer->GetTextureInfo(mTextureChain[1].mRenderTexture[0], info + 1);
            if (!mHMD->AttachToWindow2(reinterpret_cast<void*>(static_cast<uint64_t>(info[0].mDeleteMe)), reinterpret_cast<void*>(static_cast<uint64_t>(info[1].mDeleteMe))))
            {
                mSettings.End();
                return false;
            }
        }
        else
        {
            return false;
        }

        //-----------------------

        char error[2048];
        mMirrorShader = mRenderer->CreateShader(nullptr, vsMirror, nullptr, nullptr, nullptr, fsMirror, error);
        if (!mMirrorShader)
        {
            mLog.Printf(LT_ERROR, L"Can't create mirror shader");
            mSettings.End();
            return false;
        }


        // Legacy Oculus/OpenGL currently has correct eye placement in the slow
        // multipass path and visibly wrong eye placement in the fast single-pass
        // path. Keep that runtime on slow stereo by default so it remains a
        // usable fallback while standalone OpenXR work proceeds. The opt-in env
        // var is only for future debugging of the old fast path.
        const bool forceSlowStereo = getenv("IMM_VIEWER_FORCE_SLOW_STEREO") != nullptr;
        const bool allowLegacyOculusFastStereo = getenv("IMM_VIEWER_ENABLE_LEGACY_OCULUS_FAST_STEREO") != nullptr;
        const bool legacyOculusHmd =
            mHMD->mType == piVRHMD::Oculus_Rift ||
            mHMD->mType == piVRHMD::Oculus_RiftS ||
            mHMD->mType == piVRHMD::Oculus_Quest;
        const bool forceLegacyOculusSlowStereo = legacyOculusHmd && !allowLegacyOculusFastStereo;
        if (forceSlowStereo ||
            forceLegacyOculusSlowStereo ||
            !mRenderer->SupportsFeature(piRenderer::RendererFeature::VERTEX_VIEWPORT) ||
            !mRenderer->SupportsFeature(piRenderer::RendererFeature::VIEWPORT_ARRAY))
        {
            if (forceSlowStereo)
            {
                mLog.Printf(LT_WARNING, L"Fast stereo disabled by IMM_VIEWER_FORCE_SLOW_STEREO, falling back to slow stereo");
            }
            else if (forceLegacyOculusSlowStereo)
            {
                mLog.Printf(LT_WARNING, L"Fast stereo disabled for legacy Oculus/OpenGL, falling back to slow stereo");
            }
            else
            {
                mLog.Printf(LT_WARNING, L"Fast stereo is not available, falling back to slow stereo");
            }
            mStereoMode = ImmPlayer::StereoMode::Fallback;
        }
        else
        {
            mLog.Printf(LT_MESSAGE, L"Fast stereo enabled");
            mStereoMode = ImmPlayer::StereoMode::Preferred;
        }

        mHMD->SetTrackingOriginType(piVRHMD::TrackingOrigin::FloorLevel);
    }
    else
    #endif
    {
        #if DISABLE_VR==0
        mHMD = nullptr;
        #endif
        mStereoMode = ImmPlayer::StereoMode::None;
        mRenderSize = mWindowSize;
    }

    //------------------------------

    const char *disableAudioForValidation = getenv("IMM_VIEWER_VALIDATE_DISABLE_AUDIO");
    const bool useNullSoundBackend = disableAudioForValidation && disableAudioForValidation[0];
    mSoundEngineBackend = piCreateSoundEngineBackend(useNullSoundBackend ? piSoundEngineBackend::API::Null : piSoundEngineBackend::API::DirectSoundOVR, &mLog);
    if (!mSoundEngineBackend)
    {
        mLog.Printf(LT_WARNING, L"Sound backend unavailable; continuing without audio");
        mSoundEngineBackend = piCreateSoundEngineBackend(piSoundEngineBackend::API::Null, &mLog);
        if (!mSoundEngineBackend)
        {
            mSettings.End();
            return false;
        }
    }

    const int num = mSoundEngineBackend->GetNumDevices();
    mLog.Printf(LT_MESSAGE, L"%d sound devices", num);
    for (int i = 0; i < num; ++i)
    {
        const wchar_t * deviceName = mSoundEngineBackend->GetDeviceName(i);
#if defined(WINDOWS)
        mLog.Printf(LT_MESSAGE, L"    %d: %s", i, deviceName);
#else
        mLog.Printf(LT_MESSAGE, L"    %d: %ls", i, deviceName);
#endif
    }

    int soundDevice = -1;
    if (mSettings.mSound.mDevice.EqualW(L"Default"))
    {
        #if DISABLE_VR==0
        if (mHMD)
        {
            void* deviceGUID = mHMD->GetSoundOutputGUID();
            if (deviceGUID == nullptr)
            {
                soundDevice = -1;
            }
            else
            {
                soundDevice = mSoundEngineBackend->GetDeviceFromGUID(deviceGUID);
                if (soundDevice == -1)
                {
                    mLog.Printf(LT_WARNING, L"Headset headphones are off. Switching to default sound device");
                    soundDevice = -1;
                }
            }
        }
        else
        #endif
        {
            soundDevice = -1;
        }
    }
    else
    {
        soundDevice = mSoundEngineBackend->GetDeviceFromName(mSettings.mSound.mDevice.GetS());
        if (soundDevice == -1)
        {
            mLog.Printf(LT_ERROR, L"Couldn't find specified sound device");
            mSettings.End();
            return false;
        }
    }

    const wchar_t *deviceName = (soundDevice == -1) ? L"Default" : mSoundEngineBackend->GetDeviceName(soundDevice);
#if defined(WINDOWS)
    mLog.Printf(LT_MESSAGE, L"Sound device selected: \"%s\", requested \"%s\"", deviceName, mSettings.mSound.mDevice.GetS());
#else
    mLog.Printf(LT_MESSAGE, L"Sound device selected: \"%ls\", requested \"%ls\"", deviceName, mSettings.mSound.mDevice.GetS());
#endif

    piSoundEngineBackend::Configuration config;

    if (!mSoundEngineBackend->Init(piWindow_getHandle(mWindow), soundDevice, &config)) // TODO: copy max sounds setting from app
    {
        mLog.Printf(LT_WARNING, L"Sound backend init failed; continuing without audio");
    }



    const int vpmult = (mStereoMode == ImmPlayer::StereoMode::Preferred) ? 2 : 1;

    if (mRenderer->GetAPI() != piRenderer::API::DX)
    {
        const piRenderer::TextureInfo infocm = { piRenderer::TextureType::T2D, piRenderer::Format::C3_11_11_10_FLOAT, mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        const piRenderer::TextureInfo infozm = { piRenderer::TextureType::T2D, piRenderer::Format::DS_24_8_UINT, mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        mLog.Printf(LT_MESSAGE, L"Creating render textures (%d x %d)", mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample);
        mColorTextureM = mRenderer->CreateTexture(0, &infocm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0);
        if (!mColorTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create color render texture");
            return false;
        }
        mDepthTextureM = mRenderer->CreateTexture(0, &infozm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0);
        if (!mDepthTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create depth render texture");
            return false;
        }
    }
    else
    {
        const piRenderer::TextureInfo infocm = { piRenderer::TextureType::T2D, piRenderer::Format::C3_11_11_10_FLOAT, mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        const piRenderer::TextureInfo infozm = { piRenderer::TextureType::T2D, piRenderer::Format::DS_24_8_UINT,            mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        //const piRenderer::TextureInfo2 infozm = { piRenderer::TextureType::T2D, piRenderer::Format::D1_32_FLOAT,            mRenderSize.x * vpmult * mSuperSample, mRenderSize.y * mSuperSample, 1, AA };
        mColorTextureM = mRenderer->CreateTexture2(0, &infocm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0, 1 + 2);
        if (!mColorTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create color render texture (DX path)");
            return false;
        }
        mDepthTextureM = mRenderer->CreateTexture2(0, &infozm, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, 0, 2);
        if (!mDepthTextureM)
        {
            mLog.Printf(LT_ERROR, L"Failed to create depth render texture (DX path)");
            return false;
        }
    }
    if (!mColorTextureM || !mDepthTextureM)
    {
        mLog.Printf(LT_ERROR, L"Render textures missing");
        mSettings.End();
        return false;
    }
    mRenderTargetM = mRenderer->CreateRenderTarget(mColorTextureM, 0, 0, 0, mDepthTextureM);
    if (!mRenderTargetM)
    {
        mLog.Printf(LT_ERROR, L"Failed to create render target");
        return false;
    }


    mLog.Printf(LT_MESSAGE, L"Initializing resolve");
    if (!mResolve.Init(mRenderer, mSuperSample, AA, Resolve::OutputEncoding::DisplaySrgb))
    {
        mLog.Printf(LT_ERROR, L"Resolve init failed");
        mSettings.End();
        return false;
    }
    mLog.Printf(LT_MESSAGE, L"Resolve initialized");

    piSoundEngine* soundEngine = mSoundEngineBackend->GetEngine();

    if (!mViewer.Init(nullptr, mRenderer, soundEngine, &mLog, &mTimer, mStereoMode, &mSettings))
    {
        mLog.Printf(LT_ERROR, L"Viewer init failed");
        mSettings.End();
        return validationStartupExitCode;
    }
    mLog.Printf(LT_MESSAGE, L"Viewer initialized");

    // enter render loop

    double to = mTimer.GetTime();
    double renderFpsTo = 0.0;
    int renderFrame = 0;
    float renderFps = 0.0;
    int totalFrames = 0;
    int done = 0;
    bool loggedLegacyVrSmokeFrameSubmitted = false;
    bool doSave = true;
    double oldTime;
    bool enabled = true;

    oldTime = to;


#if defined(WINDOWS)
    static const uint32_t kRenderBudgetMicroseconds = 9000;
#elif defined(ANDROID)
    static const uint32_t kRenderBudgetMicroseconds = 5000;
#else
    static const uint32_t kRenderBudgetMicroseconds = 9000;
#endif


    mLog.Printf(LT_MESSAGE, L"X = next,  Z = prev,  C = restart,   v = replay,   P = pause/resume");
    int frameid = 0;
    bool isFirstFrame = true;
    const bool validationEnabled = validationRequested;
    const uint64_t validationFrame = validationEnabled ? strtoull(validationFrameEnv, nullptr, 10) : 0;
    double validationFixedDt = -1.0;
    uint64_t validationMaxFrame = validationFrame + 300;
    uint64_t validationMinNonZeroPixels = 16;
    uint64_t validationMinDrawCalls = 1;
    uint64_t validationMinPictureDrawCalls = 1;
    uint64_t validationMinPicture360DrawCalls = 1;
    uint64_t validationMinPicture360EquirectDrawCalls = 0;
    uint64_t validationMinPicture360CubemapDrawCalls = 0;
    uint64_t validationMinTriangles = 1;
    uint64_t validationPlayerFrame = 0;
    bool validationPlayerFrameEnabled = false;
    bool validationDone = false;
    int validationExitCode = 0;
    char validationCapturePath[PATH_MAX] = {};

    const char *validationMaxFrameEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MAX_FRAME", "IMM_GL_VALIDATE_MAX_FRAME");
    if (validationMaxFrameEnv && validationMaxFrameEnv[0])
    {
        validationMaxFrame = strtoull(validationMaxFrameEnv, nullptr, 10);
    }
    const char *validationFixedDtEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_FIXED_DT", "IMM_GL_VALIDATE_FIXED_DT");
    if (validationFixedDtEnv && validationFixedDtEnv[0])
    {
        validationFixedDt = atof(validationFixedDtEnv);
    }
    const char *validationMinNonZeroEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_NONZERO", "IMM_GL_VALIDATE_MIN_NONZERO");
    if (validationMinNonZeroEnv && validationMinNonZeroEnv[0])
    {
        validationMinNonZeroPixels = strtoull(validationMinNonZeroEnv, nullptr, 10);
    }
    const char *validationMinDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_DRAWCALLS", "IMM_GL_VALIDATE_MIN_DRAWCALLS");
    if (validationMinDrawCallsEnv && validationMinDrawCallsEnv[0])
    {
        validationMinDrawCalls = strtoull(validationMinDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPictureDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE_DRAWCALLS");
    if (validationMinPictureDrawCallsEnv && validationMinPictureDrawCallsEnv[0])
    {
        validationMinPictureDrawCalls = strtoull(validationMinPictureDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPicture360DrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE360_DRAWCALLS");
    if (validationMinPicture360DrawCallsEnv && validationMinPicture360DrawCallsEnv[0])
    {
        validationMinPicture360DrawCalls = strtoull(validationMinPicture360DrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPicture360EquirectDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS");
    if (validationMinPicture360EquirectDrawCallsEnv && validationMinPicture360EquirectDrawCallsEnv[0])
    {
        validationMinPicture360EquirectDrawCalls = strtoull(validationMinPicture360EquirectDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinPicture360CubemapDrawCallsEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS", "IMM_GL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS");
    if (validationMinPicture360CubemapDrawCallsEnv && validationMinPicture360CubemapDrawCallsEnv[0])
    {
        validationMinPicture360CubemapDrawCalls = strtoull(validationMinPicture360CubemapDrawCallsEnv, nullptr, 10);
    }
    const char *validationMinTrianglesEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_MIN_TRIANGLES", "IMM_GL_VALIDATE_MIN_TRIANGLES");
    if (validationMinTrianglesEnv && validationMinTrianglesEnv[0])
    {
        validationMinTriangles = strtoull(validationMinTrianglesEnv, nullptr, 10);
    }
    const char *validationPlayerFrameEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_PLAYER_FRAME", "IMM_GL_VALIDATE_PLAYER_FRAME");
    if (validationPlayerFrameEnv && validationPlayerFrameEnv[0])
    {
        validationPlayerFrame = strtoull(validationPlayerFrameEnv, nullptr, 10);
        validationPlayerFrameEnabled = true;
    }
    const char *validationCapturePathEnv = iGetValidationEnv("IMM_VIEWER_VALIDATE_CAPTURE_PATH", "IMM_GL_VALIDATE_CAPTURE_PATH");
    if (validationCapturePathEnv && validationCapturePathEnv[0])
    {
        strncpy(validationCapturePath, validationCapturePathEnv, sizeof(validationCapturePath) - 1);
        validationCapturePath[sizeof(validationCapturePath) - 1] = 0;
    }

    while (!done)
    {
        frameid++;
        const double time = mTimer.GetTime() - to;
        float dtime = float(time - oldTime);
        if (validationEnabled && validationFixedDt >= 0.0)
        {
            dtime = (float)validationFixedDt;
        }
        oldTime = time;

        float mQuitFade = 1.0f;

        // events
        piWindowEvents_Erase(mWindow);
        piWindowMgr_MessageLoop(mWinMgr);
        done |= piWindow_getExitReq(mWindow);
        piWindowEvents* evt = piWindow_getEvents(mWindow);
        piWindowEvents_GetMouse_D(&evt->mouse);

        #if DISABLE_VR==0
        if (mStereoMode != ImmPlayer::StereoMode::None)
        {
            int tid[2];
            bool needMipMapping;
            mHMD->BeginFrame(tid + 0, tid + 1, &needMipMapping);

            const trans3d vr_to_head = fromMatrix(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));

            mViewer.GlobalWork(evt, mStereoMode != ImmPlayer::StereoMode::None, vr_to_head, mHMD->mInfo.mController, &mHMD->mInfo.mRemote, &mLog, dtime, mWindowSize, enabled ? 1 : 0, kRenderBudgetMicroseconds, isFirstFrame);


            mViewer.GlobalRender(vr_to_head, vec4(mHMD->mInfo.mHead.mProjection));

            if (mStereoMode == ImmPlayer::StereoMode::Fallback)
            {
                const int vpS[4] = { 0, 0, mRenderSize.x, mRenderSize.y };
                const int vpM[4] = { 0, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
                for (int i = 0; i < 2; i++)
                {
                    mRenderer->SetRenderTarget(mRenderTargetM);
                    mRenderer->SetViewport(0, vpM);
                    mRenderer->SetWriteMask(true, false, false, false, true);
                    const mat4x4d headToEye = f2d(mat4x4(mHMD->mInfo.mEye[i].mCamera)) * invert(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));
                    mViewer.RenderStereoMultiPass(mRenderSize*mSuperSample, i, headToEye, vec4(mHMD->mInfo.mEye[i].mProjection), vr_to_head);
                    // resolve multisampling and postpro
                    mResolve.Do(mRenderer, mTextureChain[i].mRenderTarget[tid[i]], vpS, 0, mQuitFade, mColorTextureM);
                }
            }
            else
            {
                mRenderer->SetRenderTarget(mRenderTargetM);
                #if 0
                const int vpL[4] = {                            0, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
                const int vpR[4] = { mRenderSize.x * mSuperSample, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
                mRenderer->SetViewport(0, vpL);
                mRenderer->SetViewport(1, vpR);
                #else
                const float data[12] = {
                    0.0f,                              0.0f, float(mRenderSize.x*mSuperSample), float(mRenderSize.y*mSuperSample), 0.0f, 0.0f,
                    float(mRenderSize.x*mSuperSample), 0.0f, float(mRenderSize.x*mSuperSample), float(mRenderSize.y*mSuperSample), 0.0f, 0.0f };
                mRenderer->SetViewports(2, data);
                #endif
                const mat4x4d headToLEye = f2d(mat4x4(mHMD->mInfo.mEye[0].mCamera)) * invert(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));
                const mat4x4d headToREye = f2d(mat4x4(mHMD->mInfo.mEye[1].mCamera)) * invert(f2d(mat4x4(mHMD->mInfo.mHead.mCamera)));

                mRenderer->SetWriteMask(true, false, false, false, true);
                mViewer.RenderStereoSinglePass(mRenderSize*mSuperSample, vr_to_head, headToLEye, vec4(mHMD->mInfo.mEye[0].mProjection), headToREye, vec4(mHMD->mInfo.mEye[1].mProjection), mHMD);
                // resolve multisampling and postpro
                for (int i = 0; i < 2; i++)
                {
                    const int unXOffset = i * mRenderSize.x;
                    mResolve.Do(mRenderer, mTextureChain[i].mRenderTarget[tid[i]], mHMD->mInfo.mEye[i].mVP, unXOffset, mQuitFade, mColorTextureM);
                }
            }

            // compute mipmaps before distortion occurs
            mRenderer->ComputeMipmaps(mTextureChain[0].mRenderTexture[tid[0]]);
            if (needMipMapping)
            {
                mRenderer->ComputeMipmaps(mTextureChain[1].mRenderTexture[tid[1]]);
            }

            // mirror
            const int wvp[4] = { 0, 0, mWindowSize.x, mWindowSize.y };
            mRenderer->SetRenderTarget(nullptr);
            mRenderer->SetViewport(0, wvp);
            mRenderer->SetWriteMask(true, false, false, false, false);
            mRenderer->SetState(piSTATE_CULL_FACE, false);
            mRenderer->SetState(piSTATE_DEPTH_TEST, false);
            // mRenderer->AttachSamplers(1, mMirrorSampler );
            mRenderer->AttachTextures(1, mTextureChain[0].mRenderTexture[tid[0]]);
            // mRenderer->AttachTextures(1, mMirrorRenderTexture);
            mRenderer->AttachShader(mMirrorShader);
            const float data[2] = { float(mWindowSize.x), float(mWindowSize.y) };
            mRenderer->SetShaderConstant2F(1, data, 1);
            //mRenderer->SetShaderConstantSampler(0, 0);
            mRenderer->DrawUnitQuad_XY(1);
            mRenderer->DettachTextures();
            mRenderer->DettachSamplers();
            mRenderer->DettachShader();

            mHMD->EndFrame();
            if (!loggedLegacyVrSmokeFrameSubmitted)
            {
                loggedLegacyVrSmokeFrameSubmitted = true;
                mLog.Printf(LT_MESSAGE,
                            L"IMM_LEGACY_VR_SMOKE frame_submitted frame=%d stereoMode=%d renderSize=%dx%d tid0=%d tid1=%d",
                            frameid,
                            static_cast<int>(mStereoMode),
                            mRenderSize.x,
                            mRenderSize.y,
                            tid[0],
                            tid[1]);
            }

            mRenderer->SwapBuffers();
        }
        else
        #endif
        {
            const int vpM[4] = { 0, 0, mRenderSize.x * mSuperSample, mRenderSize.y * mSuperSample };
            const trans3d vr_to_head = trans3d::identity();

            mRenderer->SetRenderTarget(mRenderTargetM);
            mRenderer->SetViewport(0, vpM);
			mRenderer->SetWriteMask(true, false, false, false, true);
            #if DISABLE_VR==0
            mViewer.GlobalWork(evt, mStereoMode != ImmPlayer::StereoMode::None, vr_to_head, mHMD->mInfo.mController, &mHMD->mInfo.mRemote, &mLog, dtime, mWindowSize, enabled ? 1 : 0, kRenderBudgetMicroseconds, isFirstFrame);
            #else
            mViewer.GlobalWork(evt, mStereoMode != ImmPlayer::StereoMode::None, vr_to_head, nullptr, nullptr, &mLog, dtime, mWindowSize, enabled ? 1 : 0, kRenderBudgetMicroseconds, isFirstFrame);
            #endif

            // render
            mViewer.GlobalRender(vr_to_head, vec4(0.0f));
            // render
            mViewer.RenderMono(mRenderSize*mSuperSample, vr_to_head, 0);

            // Resolve before validation so capture/readback observes the frame just rendered.
            mResolve.Do(mRenderer, nullptr, vpM, 0, mQuitFade, mColorTextureM);

            if (validationEnabled && !validationDone && (uint64_t)frameid >= validationFrame)
            {
                const size_t pixelCount = (size_t)mRenderSize.x * (size_t)mRenderSize.y;
                uint32_t *pixels = (uint32_t*)malloc(pixelCount * sizeof(uint32_t));
                if (!pixels)
                {
                    mLog.Printf(LT_ERROR, L"IMM GL validation failed: could not allocate readback buffer");
                    validationDone = true;
                    validationExitCode = 2;
                    done = 1;
                }
                else
                {
                    mRenderer->GetTextureContent(mColorTextureM, pixels, piRenderer::Format::C3_11_11_10_FLOAT);

                    uint64_t nonZeroPixels = 0;
                    uint64_t hash = 1469598103934665603ull;
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        if (pixels[i] != 0)
                        {
                            ++nonZeroPixels;
                        }
                        hash ^= pixels[i];
                        hash *= 1099511628211ull;
                    }

                    const ImmPlayer::Player::PerformanceInfo &perf = mViewer.GetPerformanceInfoForFrame();
                    const bool passed =
                        nonZeroPixels >= validationMinNonZeroPixels &&
                        (uint64_t)perf.numDrawCalls >= validationMinDrawCalls &&
                        (uint64_t)perf.numPictureDrawCalls >= validationMinPictureDrawCalls &&
                        (uint64_t)perf.numPicture360DrawCalls >= validationMinPicture360DrawCalls &&
                        (uint64_t)perf.numPicture360EquirectDrawCalls >= validationMinPicture360EquirectDrawCalls &&
                        (uint64_t)perf.numPicture360CubemapDrawCalls >= validationMinPicture360CubemapDrawCalls &&
                        (uint64_t)perf.numTriangles >= validationMinTriangles &&
                        (!validationPlayerFrameEnabled || perf.validationTimeFrame >= validationPlayerFrame);

                    if (!passed && (uint64_t)frameid < validationMaxFrame)
                    {
                        free(pixels);
                    }
                    else
                    {
                        validationDone = true;
                        if (!passed)
                        {
                            mLog.Printf(LT_ERROR,
                                        L"IMM GL validation failed: frame=%d pixels=%llu nonZero=%llu minNonZero=%llu hash=%llu drawCalls=%d minDrawCalls=%llu paintDrawCalls=%d pictureDrawCalls=%d minPictureDrawCalls=%llu picture2DDrawCalls=%d picture360DrawCalls=%d minPicture360DrawCalls=%llu picture360EquirectDrawCalls=%d minPicture360EquirectDrawCalls=%llu picture360CubemapDrawCalls=%d minPicture360CubemapDrawCalls=%llu modelDrawCalls=%d triangles=%d minTriangles=%llu playerFrame=%llu culledCalls=%d",
                                        frameid,
                                        (unsigned long long)pixelCount,
                                        (unsigned long long)nonZeroPixels,
                                        (unsigned long long)validationMinNonZeroPixels,
                                        (unsigned long long)hash,
                                        perf.numDrawCalls,
                                        (unsigned long long)validationMinDrawCalls,
                                        perf.numPaintDrawCalls,
                                        perf.numPictureDrawCalls,
                                        (unsigned long long)validationMinPictureDrawCalls,
                                        perf.numPicture2DDrawCalls,
                                        perf.numPicture360DrawCalls,
                                        (unsigned long long)validationMinPicture360DrawCalls,
                                        perf.numPicture360EquirectDrawCalls,
                                        (unsigned long long)validationMinPicture360EquirectDrawCalls,
                                        perf.numPicture360CubemapDrawCalls,
                                        (unsigned long long)validationMinPicture360CubemapDrawCalls,
                                        perf.numModelDrawCalls,
                                        perf.numTriangles,
                                        (unsigned long long)validationMinTriangles,
                                        (unsigned long long)perf.validationTimeFrame,
                                        perf.numDrawCallsCulled);
                            validationExitCode = 2;
                        }
                        else
                        {
                            mLog.Printf(LT_MESSAGE,
                                        L"IMM GL validation: frame=%d pixels=%llu nonZero=%llu hash=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture2DDrawCalls=%d picture360DrawCalls=%d picture360EquirectDrawCalls=%d picture360CubemapDrawCalls=%d modelDrawCalls=%d triangles=%d playerFrame=%llu culledCalls=%d",
                                        frameid,
                                        (unsigned long long)pixelCount,
                                        (unsigned long long)nonZeroPixels,
                                        (unsigned long long)hash,
                                        perf.numDrawCalls,
                                        perf.numPaintDrawCalls,
                                        perf.numPictureDrawCalls,
                                        perf.numPicture2DDrawCalls,
                                        perf.numPicture360DrawCalls,
                                        perf.numPicture360EquirectDrawCalls,
                                        perf.numPicture360CubemapDrawCalls,
                                        perf.numModelDrawCalls,
                                        perf.numTriangles,
                                        (unsigned long long)perf.validationTimeFrame,
                                        perf.numDrawCallsCulled);
                            if (validationCapturePath[0])
                            {
                                wchar_t *wideCapturePath = pistr2ws(validationCapturePath);
                                if (wideCapturePath)
                                {
#if defined(WINDOWS)
                                    mLog.Printf(LT_MESSAGE, L"IMM GL validation capture path: %s", wideCapturePath);
#else
                                    mLog.Printf(LT_MESSAGE, L"IMM GL validation capture path: %ls", wideCapturePath);
#endif
                                    free(wideCapturePath);
                                }
                                if (iWriteRG11B10Capture(validationCapturePath, pixels, mRenderSize.x, mRenderSize.y))
                                {
                                    if (iFileExistsUtf8(validationCapturePath))
                                    {
                                        mLog.Printf(LT_MESSAGE, L"IMM GL validation capture written");
                                    }
                                    else
                                    {
                                        mLog.Printf(LT_ERROR, L"IMM GL validation capture write reported success but file is missing");
                                        validationExitCode = 2;
                                    }
                                }
                                else
                                {
                                    mLog.Printf(LT_ERROR, L"IMM GL validation capture failed");
                                    validationExitCode = 2;
                                }
                            }
                        }
                        free(pixels);
                        done = 1;
                    }
                }
            }

            if (!done)
            {
                mRenderer->SwapBuffers();
            }
        }

        mSoundEngineBackend->Tick();

        totalFrames++;

        // update fps counter
        renderFrame++;
        const double dt = time - renderFpsTo;
        if (dt > 1.0)
        {
            renderFps = (float)renderFrame / (float)dt;
            renderFrame = 0;
            renderFpsTo = time;
        }
        if ((totalFrames & 63) == 0)
        {
            wchar_t str[64];
            piwsprintf(str, 63, L"%.1f fps :: %.2f", renderFps, time);
            piWindow_setText(mWindow, str);
        }
    }

    mViewer.Deinit();

    mSoundEngineBackend->Deinit();

    piDestroySoundEngineBackend(mSoundEngineBackend);

    mResolve.DeInit(mRenderer);

    mRenderer->DestroyRenderTarget(mRenderTargetM);
    mRenderer->DestroyTexture(mDepthTextureM);
    mRenderer->DestroyTexture(mColorTextureM);
    #if DISABLE_VR==0
    if (mStereoMode != ImmPlayer::StereoMode::None)
    {
        mRenderer->DestroyShader(mMirrorShader);
        for (int j = 0; j < 2; j++)
        {
            for (int i = 0; i < mTextureChain[j].mRenderNumTextures; i++)
            {
                mRenderer->DestroyTexture(mTextureChain[j].mRenderTexture[i]);
                mRenderer->DestroyRenderTarget(mTextureChain[j].mRenderTarget[i]);
            }
        }
    }

    if (mStereoMode != ImmPlayer::StereoMode::None)
    {
        piVRHMD::Destroy(mHMD);
    }
    #endif

    mRenderer->Deinitialize();
    mRenderer->Report();

    delete mRenderReporter;
    delete mRenderer;
    piWindow_end(mWindow);
    piWindowMgr_End(mWinMgr);

    mSettings.End();

    mLog.End();

    return validationEnabled ? validationExitCode : 1;
}
