#include "Log.h"

#include "libImmCore/src/libBasics/piTimer.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libSound/piSound.h"

#include "libImmCore/src/libRender/piRenderer.h"
#include "libImmPlayer/src/player.h"

#include "../../viewer/viewer.h"
#include "../../settings.h"

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <jni.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <mutex>
#include <vector>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <cmath>

#if defined(IMM_ANDROID_XR_RUNTIME_OPENXR)
#define XR_USE_PLATFORM_ANDROID 1
#define XR_USE_GRAPHICS_API_VULKAN 1
#include <dlfcn.h>
#include <vulkan/vulkan.h>
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#endif

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif
#if !defined(EGL_CONTEXT_MINOR_VERSION_KHR)
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif

using namespace ImmCore;
using namespace ExePlayer;

namespace {

struct EngineState {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int width = 0;
    int height = 0;
    int validationRenderWidth = 0;
    int validationRenderHeight = 0;
    bool hasWindow = false;
    bool running = false;
    bool useVulkan = false;

    piRenderer* renderer = nullptr;
    piLog* log = nullptr;
    piTimer* timer = nullptr;
    piSoundEngineBackend* soundBackend = nullptr;
    Viewer* viewer = nullptr;
    Settings* activeSettings = nullptr;
    piTexture colorTexture = nullptr;
    piTexture depthTexture = nullptr;
    piRTarget renderTarget = nullptr;
    ImmPlayer::StereoMode stereoMode = ImmPlayer::StereoMode::None;
    bool viewerInitialized = false;
    bool firstFrame = true;
    uint32_t frameCount = 0;
    bool validationCaptureWritten = false;

    std::wstring playerSpawnLocation = L"Default";
    ExePlayer::Settings::Rendering::Technique renderingTechnique =
        ExePlayer::Settings::Rendering::Technique::Static;

    // Touch input state
    struct TouchState {
        bool active = false;
        float startX = 0.0f;
        float startY = 0.0f;
        float lastX = 0.0f;
        float lastY = 0.0f;
        float deltaX = 0.0f;
        float deltaY = 0.0f;
    };
    TouchState touch1;  // Single finger (camera rotation)
    TouchState touch2;  // Second finger (forward/backward movement)
    float pinchStartDistance = 0.0f;
    bool isPinching = false;
};

EngineState gEngine;
std::mutex gMessageMutex;
std::wstring gPendingPath;
bool gTriedAutoLoad = false;
std::string gAssetDirectory;
std::string gExternalFilesDirectory;

static int renderWidth() {
    return gEngine.validationRenderWidth > 0 ? gEngine.validationRenderWidth : gEngine.width;
}

static int renderHeight() {
    return gEngine.validationRenderHeight > 0 ? gEngine.validationRenderHeight : gEngine.height;
}

static float iDecodeUnsignedFloat(uint32_t bits, int mantissaBits) {
    const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const uint32_t mantissa = bits & mantissaMask;
    const uint32_t exponent = (bits >> mantissaBits) & 0x1fu;
    if (exponent == 0) {
        return ldexpf(static_cast<float>(mantissa) / static_cast<float>(1u << mantissaBits), -14);
    }
    if (exponent == 31) {
        return 1.0f;
    }
    return ldexpf(1.0f + static_cast<float>(mantissa) / static_cast<float>(1u << mantissaBits), static_cast<int>(exponent) - 15);
}

static uint8_t iFloatToByte(float value) {
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }
    return static_cast<uint8_t>(value * 255.0f + 0.5f);
}

static uint8_t iLinearFloatToSrgbByte(float value) {
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }
    const float encoded = value < 0.0031308f ? value * 12.92f : 1.055f * powf(value, 1.0f / 2.4f) - 0.055f;
    return iFloatToByte(encoded);
}

static bool iWriteRgbPpm(const char *path, const uint8_t *rgb, int width, int height) {
    if (!path || !path[0] || !rgb || width <= 0 || height <= 0) {
        return false;
    }
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;
    const size_t written = fwrite(rgb, 1, expected, file);
    fclose(file);
    return written == expected;
}

static bool iHasVisibleRgbContent(const uint8_t *rgb, int width, int height) {
    if (!rgb || width <= 0 || height <= 0) {
        return false;
    }
    uint8_t minLuma = 255;
    uint8_t maxLuma = 0;
    uint64_t visiblePixels = 0;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t r = rgb[i * 3u + 0];
        const uint8_t g = rgb[i * 3u + 1];
        const uint8_t b = rgb[i * 3u + 2];
        const uint8_t luma = static_cast<uint8_t>((54u * r + 183u * g + 19u * b) >> 8);
        if (luma < minLuma) {
            minLuma = luma;
        }
        if (luma > maxLuma) {
            maxLuma = luma;
        }
        if (luma > 32 && (r > 40 || g > 40 || b > 40)) {
            ++visiblePixels;
        }
    }
    return visiblePixels >= 20000 && (maxLuma - minLuma) >= 16;
}

static bool iWriteRG11B10Ppm(const char *path, const uint32_t *pixels, int width, int height, bool flipVertical) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);
    for (int y = 0; y < height; ++y) {
        const int sourceY = flipVertical ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            const uint32_t value = pixels[static_cast<size_t>(sourceY) * width + x];
            const size_t out = (static_cast<size_t>(y) * width + x) * 3u;
            rgb[out + 0] = iLinearFloatToSrgbByte(iDecodeUnsignedFloat(value & 0x7ffu, 6));
            rgb[out + 1] = iLinearFloatToSrgbByte(iDecodeUnsignedFloat((value >> 11u) & 0x7ffu, 6));
            rgb[out + 2] = iLinearFloatToSrgbByte(iDecodeUnsignedFloat((value >> 22u) & 0x3ffu, 5));
        }
    }
    if (!iHasVisibleRgbContent(rgb.data(), width, height)) {
        return false;
    }
    return iWriteRgbPpm(path, rgb.data(), width, height);
}

static bool iWriteGlesFramebufferPpm(const char *path, const char *rejectedPath, int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);
    for (int y = 0; y < height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const size_t in = (static_cast<size_t>(sourceY) * width + x) * 4u;
            const size_t out = (static_cast<size_t>(y) * width + x) * 3u;
            rgb[out + 0] = rgba[in + 0];
            rgb[out + 1] = rgba[in + 1];
            rgb[out + 2] = rgba[in + 2];
        }
    }
    if (!iHasVisibleRgbContent(rgb.data(), width, height)) {
        if (rejectedPath && rejectedPath[0]) {
            iWriteRgbPpm(rejectedPath, rgb.data(), width, height);
        }
        return false;
    }
    return iWriteRgbPpm(path, rgb.data(), width, height);
}

static bool isValidationFrameReady(const ImmPlayer::Player::PerformanceInfo &perf) {
    return perf.numDrawCalls > 0 &&
           perf.numPaintDrawCalls > 0 &&
           perf.numTriangles > 0;
}

static void writeValidationCaptureIfReady(const ImmPlayer::Player::PerformanceInfo &perf) {
    if (gEngine.validationCaptureWritten || gEngine.frameCount < 5 || gExternalFilesDirectory.empty()) {
        return;
    }
    if ((gEngine.frameCount % 15u) != 0u) {
        return;
    }
    if (!isValidationFrameReady(perf)) {
        if (gEngine.frameCount == 60 || gEngine.frameCount == 120 || gEngine.frameCount == 180) {
            ALOGV("IMMAVAL native render capture waiting frame=%u playerFrame=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d triangles=%d renderer=%s",
                  gEngine.frameCount,
                  static_cast<unsigned long long>(perf.validationTimeFrame),
                  perf.numDrawCalls,
                  perf.numPaintDrawCalls,
                  perf.numPictureDrawCalls,
                  perf.numTriangles,
                  gEngine.useVulkan ? "Vulkan" : "GLES");
        }
        return;
    }

    const std::string artifactDir = gExternalFilesDirectory + "/imm-ftl";
    mkdir(artifactDir.c_str(), 0777);
    const std::string capturePath = artifactDir + "/native-render-after.ppm";
    const std::string rejectedCapturePath = artifactDir + "/native-render-rejected.ppm";

    bool wrote = false;
    if (gEngine.useVulkan) {
        if (gEngine.renderer && gEngine.colorTexture) {
            const int width = renderWidth();
            const int height = renderHeight();
            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<uint32_t> pixels(pixelCount);
            gEngine.renderer->GetTextureContent(gEngine.colorTexture, pixels.data(), piRenderer::Format::C3_11_11_10_FLOAT);
            wrote = iWriteRG11B10Ppm(capturePath.c_str(), pixels.data(), width, height, false);
        }
    } else {
        glFinish();
        wrote = iWriteGlesFramebufferPpm(capturePath.c_str(), rejectedCapturePath.c_str(), renderWidth(), renderHeight());
    }

    if (wrote) {
        gEngine.validationCaptureWritten = true;
        ALOGV("IMMAVAL native render capture written path=%s frame=%u playerFrame=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d triangles=%d size=%dx%d renderer=%s",
              capturePath.c_str(),
              gEngine.frameCount,
              static_cast<unsigned long long>(perf.validationTimeFrame),
              perf.numDrawCalls,
              perf.numPaintDrawCalls,
              perf.numPictureDrawCalls,
              perf.numTriangles,
              renderWidth(),
              renderHeight(),
              gEngine.useVulkan ? "Vulkan" : "GLES");
    } else {
        ALOGE("IMMAVAL native render capture failed path=%s frame=%u playerFrame=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d triangles=%d size=%dx%d renderer=%s",
              capturePath.c_str(),
              gEngine.frameCount,
              static_cast<unsigned long long>(perf.validationTimeFrame),
              perf.numDrawCalls,
              perf.numPaintDrawCalls,
              perf.numPictureDrawCalls,
              perf.numTriangles,
              renderWidth(),
              renderHeight(),
              gEngine.useVulkan ? "Vulkan" : "GLES");
    }
}

bool iEqualsIgnoreCase(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    return strcasecmp(a, b) == 0;
}

class AndroidRenderReporter final : public piRenderer::piReporter {
public:
    void Info(const char* str) override {
        ALOGV("%s", str ? str : "");
    }

    void Error(const char* str, int) override {
        ALOGE("%s", str ? str : "");
    }

    void Begin(uint64_t, uint64_t, int, int) override {}
    void Texture(const wchar_t*, uint64_t, piRenderer::Format, bool, int, int, int) override {}
    void End(void) override {}
};

AndroidRenderReporter gRenderReporter;

#if defined(IMM_ANDROID_XR_RUNTIME_OPENXR)
const char* XrResultName(XrResult result) {
    switch (result) {
        case XR_SUCCESS: return "XR_SUCCESS";
        case XR_TIMEOUT_EXPIRED: return "XR_TIMEOUT_EXPIRED";
        case XR_ERROR_VALIDATION_FAILURE: return "XR_ERROR_VALIDATION_FAILURE";
        case XR_ERROR_RUNTIME_FAILURE: return "XR_ERROR_RUNTIME_FAILURE";
        case XR_ERROR_OUT_OF_MEMORY: return "XR_ERROR_OUT_OF_MEMORY";
        case XR_ERROR_API_VERSION_UNSUPPORTED: return "XR_ERROR_API_VERSION_UNSUPPORTED";
        case XR_ERROR_INITIALIZATION_FAILED: return "XR_ERROR_INITIALIZATION_FAILED";
        case XR_ERROR_FUNCTION_UNSUPPORTED: return "XR_ERROR_FUNCTION_UNSUPPORTED";
        case XR_ERROR_FEATURE_UNSUPPORTED: return "XR_ERROR_FEATURE_UNSUPPORTED";
        case XR_ERROR_EXTENSION_NOT_PRESENT: return "XR_ERROR_EXTENSION_NOT_PRESENT";
        case XR_ERROR_LIMIT_REACHED: return "XR_ERROR_LIMIT_REACHED";
        case XR_ERROR_SIZE_INSUFFICIENT: return "XR_ERROR_SIZE_INSUFFICIENT";
        case XR_ERROR_HANDLE_INVALID: return "XR_ERROR_HANDLE_INVALID";
        case XR_ERROR_INSTANCE_LOST: return "XR_ERROR_INSTANCE_LOST";
        case XR_ERROR_SYSTEM_INVALID: return "XR_ERROR_SYSTEM_INVALID";
        case XR_ERROR_FORM_FACTOR_UNAVAILABLE: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case XR_ERROR_FORM_FACTOR_UNSUPPORTED: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
        case XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED: return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
        default: return "XR_UNKNOWN_RESULT";
    }
}

bool HasOpenXrExtension(const std::vector<XrExtensionProperties>& extensions, const char* name) {
    for (const XrExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

template <typename T>
bool LoadOpenXrSymbol(void* loader, const char* name, T* out) {
    *out = reinterpret_cast<T>(dlsym(loader, name));
    if (*out == nullptr) {
        ALOGE("IMM_ANDROID_OPENXR_PROBE missingSymbol=%s", name);
        return false;
    }
    return true;
}

void RunAndroidOpenXrStartupProbe(android_app* app) {
    ALOGV("IMM_ANDROID_OPENXR_PROBE begin");
    if (app == nullptr || app->activity == nullptr || app->activity->vm == nullptr || app->activity->clazz == nullptr) {
        ALOGE("IMM_ANDROID_OPENXR_PROBE missingNativeActivityContext app=%p activity=%p", app, app ? app->activity : nullptr);
        return;
    }

    void* loader = dlopen("libopenxr_loader.so", RTLD_NOW | RTLD_LOCAL);
    if (loader == nullptr) {
        ALOGE("IMM_ANDROID_OPENXR_PROBE dlopenResult=0 error=%s", dlerror());
        return;
    }

    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties = nullptr;
    PFN_xrCreateInstance xrCreateInstance = nullptr;
    PFN_xrGetSystem xrGetSystem = nullptr;
    PFN_xrGetSystemProperties xrGetSystemProperties = nullptr;
    PFN_xrEnumerateViewConfigurations xrEnumerateViewConfigurations = nullptr;
    PFN_xrEnumerateViewConfigurationViews xrEnumerateViewConfigurationViews = nullptr;
    PFN_xrDestroyInstance xrDestroyInstance = nullptr;

    const bool loaded =
        LoadOpenXrSymbol(loader, "xrInitializeLoaderKHR", &xrInitializeLoaderKHR) &&
        LoadOpenXrSymbol(loader, "xrEnumerateInstanceExtensionProperties", &xrEnumerateInstanceExtensionProperties) &&
        LoadOpenXrSymbol(loader, "xrCreateInstance", &xrCreateInstance) &&
        LoadOpenXrSymbol(loader, "xrGetSystem", &xrGetSystem) &&
        LoadOpenXrSymbol(loader, "xrGetSystemProperties", &xrGetSystemProperties) &&
        LoadOpenXrSymbol(loader, "xrEnumerateViewConfigurations", &xrEnumerateViewConfigurations) &&
        LoadOpenXrSymbol(loader, "xrEnumerateViewConfigurationViews", &xrEnumerateViewConfigurationViews) &&
        LoadOpenXrSymbol(loader, "xrDestroyInstance", &xrDestroyInstance);
    if (!loaded) {
        dlclose(loader);
        return;
    }

    XrLoaderInitInfoAndroidKHR loaderInit = {};
    loaderInit.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
    loaderInit.applicationVM = app->activity->vm;
    loaderInit.applicationContext = app->activity->clazz;
    XrResult result = xrInitializeLoaderKHR(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInit));
    ALOGV("IMM_ANDROID_OPENXR_PROBE initializeLoaderResult=%d resultName=%s", result, XrResultName(result));
    if (XR_FAILED(result)) {
        dlclose(loader);
        return;
    }

    uint32_t extensionCount = 0;
    result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    ALOGV("IMM_ANDROID_OPENXR_PROBE enumerateExtensionsResult=%d resultName=%s count=%u", result, XrResultName(result), extensionCount);
    if (XR_FAILED(result)) {
        dlclose(loader);
        return;
    }

    std::vector<XrExtensionProperties> extensions(extensionCount);
    for (XrExtensionProperties& extension : extensions) {
        extension.type = XR_TYPE_EXTENSION_PROPERTIES;
    }
    result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());
    ALOGV("IMM_ANDROID_OPENXR_PROBE enumerateExtensionsFillResult=%d resultName=%s count=%u", result, XrResultName(result), extensionCount);
    if (XR_FAILED(result)) {
        dlclose(loader);
        return;
    }

    for (const XrExtensionProperties& extension : extensions) {
        if (std::strstr(extension.extensionName, "vulkan") != nullptr ||
            std::strstr(extension.extensionName, "android") != nullptr ||
            std::strstr(extension.extensionName, "Android") != nullptr) {
            ALOGV("IMM_ANDROID_OPENXR_PROBE extension=%s version=%u", extension.extensionName, extension.extensionVersion);
        }
    }

    if (!HasOpenXrExtension(extensions, XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME)) {
        ALOGE("IMM_ANDROID_OPENXR_PROBE missingRequiredExtension=%s", XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
        dlclose(loader);
        return;
    }

    std::vector<const char*> enabledExtensions;
    enabledExtensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
    if (HasOpenXrExtension(extensions, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME)) {
        enabledExtensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
    }
    if (HasOpenXrExtension(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) {
        enabledExtensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
    }

    XrInstanceCreateInfoAndroidKHR androidCreateInfo = {};
    androidCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR;
    androidCreateInfo.applicationVM = app->activity->vm;
    androidCreateInfo.applicationActivity = app->activity->clazz;

    XrInstanceCreateInfo createInfo = {};
    createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
    createInfo.next = &androidCreateInfo;
    std::strncpy(createInfo.applicationInfo.applicationName, "IMM Android OpenXR Probe", XR_MAX_APPLICATION_NAME_SIZE - 1);
    std::strncpy(createInfo.applicationInfo.engineName, "IMM", XR_MAX_ENGINE_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.enabledExtensionNames = enabledExtensions.data();

    XrInstance instance = XR_NULL_HANDLE;
    result = xrCreateInstance(&createInfo, &instance);
    ALOGV("IMM_ANDROID_OPENXR_PROBE createInstanceResult=%d resultName=%s instance=%p enabledExtensions=%u", result, XrResultName(result), reinterpret_cast<void*>(instance), createInfo.enabledExtensionCount);
    if (XR_FAILED(result)) {
        dlclose(loader);
        return;
    }

    XrSystemGetInfo systemInfo = {};
    systemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    result = xrGetSystem(instance, &systemInfo, &systemId);
    ALOGV("IMM_ANDROID_OPENXR_PROBE getHmdSystemResult=%d resultName=%s systemId=%llu", result, XrResultName(result), static_cast<unsigned long long>(systemId));

    if (XR_SUCCEEDED(result)) {
        XrSystemProperties systemProperties = {};
        systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
        result = xrGetSystemProperties(instance, systemId, &systemProperties);
        ALOGV("IMM_ANDROID_OPENXR_PROBE getSystemPropertiesResult=%d resultName=%s systemName=%s vendorId=%u", result, XrResultName(result), systemProperties.systemName, systemProperties.vendorId);

        uint32_t viewConfigCount = 0;
        result = xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigCount, nullptr);
        ALOGV("IMM_ANDROID_OPENXR_PROBE enumerateViewConfigsResult=%d resultName=%s count=%u", result, XrResultName(result), viewConfigCount);
        if (XR_SUCCEEDED(result) && viewConfigCount > 0) {
            uint32_t stereoViewCount = 0;
            result = xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &stereoViewCount, nullptr);
            ALOGV("IMM_ANDROID_OPENXR_PROBE enumerateStereoViewsResult=%d resultName=%s count=%u", result, XrResultName(result), stereoViewCount);
            if (XR_SUCCEEDED(result) && stereoViewCount > 0) {
                std::vector<XrViewConfigurationView> stereoViews(stereoViewCount);
                for (XrViewConfigurationView& view : stereoViews) {
                    view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                }
                result = xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, stereoViewCount, &stereoViewCount, stereoViews.data());
                ALOGV("IMM_ANDROID_OPENXR_PROBE enumerateStereoViewsFillResult=%d resultName=%s count=%u", result, XrResultName(result), stereoViewCount);
                for (uint32_t i = 0; i < stereoViewCount; ++i) {
                    ALOGV("IMM_ANDROID_OPENXR_PROBE stereoView[%u]=recommended=%ux%u max=%ux%u samples=%u",
                          i,
                          stereoViews[i].recommendedImageRectWidth,
                          stereoViews[i].recommendedImageRectHeight,
                          stereoViews[i].maxImageRectWidth,
                          stereoViews[i].maxImageRectHeight,
                          stereoViews[i].recommendedSwapchainSampleCount);
                }
            }
        }
    }

    result = xrDestroyInstance(instance);
    ALOGV("IMM_ANDROID_OPENXR_PROBE destroyInstanceResult=%d resultName=%s", result, XrResultName(result));
    dlclose(loader);
    ALOGV("IMM_ANDROID_OPENXR_PROBE end");
}
#endif

void setAssetDirectory(const char* dir) {
    gAssetDirectory = dir ? dir : "";
}

const char* getAssetDirectory() {
    return gAssetDirectory.c_str();
}

void setExternalFilesDirectory(const char* dir) {
    gExternalFilesDirectory = dir ? dir : "";
}

const char* getExternalFilesDirectory() {
    return gExternalFilesDirectory.c_str();
}

bool initEgl(android_app* app) {
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    EGLConfig config = nullptr;

    gEngine.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (gEngine.display == EGL_NO_DISPLAY) {
        ALOGE("EGL: no display");
        return false;
    }

    if (eglInitialize(gEngine.display, nullptr, nullptr) == EGL_FALSE) {
        ALOGE("EGL: initialize failed");
        return false;
    }

    if (eglChooseConfig(gEngine.display, attribs, &config, 1, &numConfigs) == EGL_FALSE || numConfigs == 0) {
        ALOGE("EGL: choose config failed");
        return false;
    }

    EGLint format = 0;
    eglGetConfigAttrib(gEngine.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);

    const EGLint contextAttribs31[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, 1,
        EGL_NONE
    };
    gEngine.context = eglCreateContext(gEngine.display, config, EGL_NO_CONTEXT, contextAttribs31);
    if (gEngine.context == EGL_NO_CONTEXT) {
        ALOGW("EGL: 3.1 context failed, falling back to 3.0");
        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };
        gEngine.context = eglCreateContext(gEngine.display, config, EGL_NO_CONTEXT, contextAttribs);
    }
    if (gEngine.context == EGL_NO_CONTEXT) {
        ALOGE("EGL: create context failed");
        return false;
    }

    gEngine.surface = eglCreateWindowSurface(gEngine.display, config, app->window, nullptr);
    if (gEngine.surface == EGL_NO_SURFACE) {
        ALOGE("EGL: create window surface failed");
        return false;
    }

    if (eglMakeCurrent(gEngine.display, gEngine.surface, gEngine.surface, gEngine.context) == EGL_FALSE) {
        ALOGE("EGL: make current failed");
        return false;
    }

    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    ALOGV("GL_VERSION: %s", glVersion ? glVersion : "unknown");
    ALOGV("GLSL_VERSION: %s", glslVersion ? glslVersion : "unknown");

    eglQuerySurface(gEngine.display, gEngine.surface, EGL_WIDTH, &gEngine.width);
    eglQuerySurface(gEngine.display, gEngine.surface, EGL_HEIGHT, &gEngine.height);
    gEngine.hasWindow = true;
    return true;
}

bool initVulkanWindow(android_app* app) {
    if (app == nullptr || app->window == nullptr) {
        ALOGE("Vulkan: no Android window");
        return false;
    }

    ANativeWindow_setBuffersGeometry(app->window, 0, 0, WINDOW_FORMAT_RGBX_8888);
    gEngine.width = ANativeWindow_getWidth(app->window);
    gEngine.height = ANativeWindow_getHeight(app->window);
    if (gEngine.width <= 0 || gEngine.height <= 0) {
        ALOGE("Vulkan: invalid Android window size %dx%d", gEngine.width, gEngine.height);
        return false;
    }

    gEngine.hasWindow = true;
    ALOGV("Vulkan: Android window ready %dx%d", gEngine.width, gEngine.height);
    return true;
}

bool FindNewestImmInDirectory(const char *dirPath, std::string &outPath) {
    DIR *dir = opendir(dirPath);
    if (dir == nullptr) {
        return false;
    }

    time_t newestTime = 0;
    std::string newestPath;

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        const char *name = entry->d_name;
        const size_t nameLen = strlen(name);
        if (nameLen < 4) {
            continue;
        }

        const char *ext = name + (nameLen - 4);
        if (strcasecmp(ext, ".imm") != 0) {
            continue;
        }

        std::string candidatePath = std::string(dirPath) + "/" + name;
        struct stat st;
        if (stat(candidatePath.c_str(), &st) != 0) {
            continue;
        }

        if (st.st_mtime >= newestTime) {
            newestTime = st.st_mtime;
            newestPath = candidatePath;
        }
    }

    closedir(dir);

    if (newestPath.empty()) {
        return false;
    }

    outPath = newestPath;
    return true;
}

bool ResolveImmPathInDirectory(const char *dirPath, std::string &outPath) {
    std::string defaultImmPath = std::string(dirPath) + "/default.imm";
    std::string defaultAuthoringPath = std::string(dirPath) + "/default";

    FILE *fp = fopen(defaultImmPath.c_str(), "rb");
    if (fp) {
        fclose(fp);
        outPath = defaultImmPath;
        return true;
    }

    fp = fopen(defaultAuthoringPath.c_str(), "rb");
    if (fp) {
        fclose(fp);
        outPath = defaultAuthoringPath;
        return true;
    }

    return FindNewestImmInDirectory(dirPath, outPath);
}

std::string ResolveInitialImmPath() {
    // NOTE: /sdcard/IMM requires MANAGE_EXTERNAL_STORAGE on Android 11+.
    // Keep this disabled until we add UI/flow for "All files access" permission.
    // const char *primaryDir = "/sdcard/IMM";
    std::string appDir;
    if (!gExternalFilesDirectory.empty()) {
        appDir = gExternalFilesDirectory + "/IMM";
    } else {
        appDir = "/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM";
    }
    std::string resolvedPath;

    // if (ResolveImmPathInDirectory(primaryDir, resolvedPath)) {
    //     return resolvedPath;
    // }

    if (ResolveImmPathInDirectory(appDir.c_str(), resolvedPath)) {
        ALOGV("IMMAVAL autoload appDir path=%s", resolvedPath.c_str());
        return resolvedPath;
    }

    if (!gAssetDirectory.empty()) {
        std::string assetImmPath = gAssetDirectory;
        if (assetImmPath.back() != '/') {
            assetImmPath += "/";
        }
        assetImmPath += "sample1.imm";
        FILE *fp = fopen(assetImmPath.c_str(), "rb");
        if (fp) {
            fclose(fp);
            ALOGV("IMMAVAL autoload asset path=%s", assetImmPath.c_str());
            return assetImmPath;
        }
        ALOGV("IMMAVAL autoload asset missing path=%s", assetImmPath.c_str());
    }

    ALOGV("IMMAVAL autoload none appDir=%s assetDir=%s externalDir=%s",
          appDir.c_str(),
          gAssetDirectory.c_str(),
          gExternalFilesDirectory.c_str());
    return std::string();
}

void shutdownEgl() {
    if (gEngine.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(gEngine.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (gEngine.context != EGL_NO_CONTEXT) {
            eglDestroyContext(gEngine.display, gEngine.context);
        }
        if (gEngine.surface != EGL_NO_SURFACE) {
            eglDestroySurface(gEngine.display, gEngine.surface);
        }
        eglTerminate(gEngine.display);
    }
    gEngine.display = EGL_NO_DISPLAY;
    gEngine.context = EGL_NO_CONTEXT;
    gEngine.surface = EGL_NO_SURFACE;
    gEngine.hasWindow = false;
}

void shutdownViewer() {
    if (!gEngine.viewer) {
        return;
    }

    gEngine.viewer->Deinit();
    gEngine.viewerInitialized = false;
    delete gEngine.viewer;
    gEngine.viewer = nullptr;

    if (gEngine.renderer) {
        if (gEngine.renderTarget) {
            gEngine.renderer->DestroyRenderTarget(gEngine.renderTarget);
            gEngine.renderTarget = nullptr;
        }
        if (gEngine.depthTexture) {
            gEngine.renderer->DestroyTexture(gEngine.depthTexture);
            gEngine.depthTexture = nullptr;
        }
        if (gEngine.colorTexture) {
            gEngine.renderer->DestroyTexture(gEngine.colorTexture);
            gEngine.colorTexture = nullptr;
        }
    }

    if (gEngine.soundBackend) {
        gEngine.soundBackend->Deinit();
        piDestroySoundEngineBackend(gEngine.soundBackend);
        gEngine.soundBackend = nullptr;
    }

    if (gEngine.renderer) {
        gEngine.renderer->Deinitialize();
        delete gEngine.renderer;
        gEngine.renderer = nullptr;
    }

    if (gEngine.timer) {
        gEngine.timer->End();
        delete gEngine.timer;
        gEngine.timer = nullptr;
    }

    if (gEngine.log) {
        gEngine.log->End();
        delete gEngine.log;
        gEngine.log = nullptr;
    }

    if (gEngine.activeSettings) {
        gEngine.activeSettings->End();
        delete gEngine.activeSettings;
        gEngine.activeSettings = nullptr;
    }
}

bool ensureRenderTarget() {
    if (!gEngine.useVulkan) {
        return true;
    }
    const int width = renderWidth();
    const int height = renderHeight();
    if (!gEngine.renderer || width <= 0 || height <= 0) {
        return false;
    }
    if (gEngine.renderTarget) {
        return true;
    }

    const piRenderer::TextureInfo colorInfo = {
        piRenderer::TextureType::T2D,
        piRenderer::Format::C3_11_11_10_FLOAT,
        width,
        height,
        1,
        8,
        1,
        0
    };
    const piRenderer::TextureInfo depthInfo = {
        piRenderer::TextureType::T2D,
        piRenderer::Format::DS_24_8_UINT,
        width,
        height,
        1,
        8,
        1,
        0
    };

    gEngine.colorTexture = gEngine.renderer->CreateTexture2(L"android_vulkan_color", &colorInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, nullptr, 1 + 2);
    gEngine.depthTexture = gEngine.renderer->CreateTexture2(L"android_vulkan_depth", &depthInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, nullptr, 2);
    if (!gEngine.colorTexture || !gEngine.depthTexture) {
        ALOGE("Failed to create Android Vulkan render textures");
        return false;
    }

    gEngine.renderTarget = gEngine.renderer->CreateRenderTarget(gEngine.colorTexture, nullptr, nullptr, nullptr, gEngine.depthTexture);
    if (!gEngine.renderTarget) {
        ALOGE("Failed to create Android Vulkan render target");
        return false;
    }

    ALOGV("Android Vulkan render target ready %dx%d", width, height);
    return true;
}

void initViewer() {
    if (gEngine.viewer) {
        return;
    }

    gEngine.log = new piLog();
    gEngine.timer = new piTimer();
    gEngine.timer->Init();

    const piRenderer::API rendererApi = gEngine.useVulkan ? piRenderer::API::Vulkan : piRenderer::API::GLES;
    gEngine.renderer = piRenderer::Create(rendererApi);
    if (!gEngine.renderer) {
        ALOGF("Could not create piRenderer");
    }

    const void *nativeWindowHandles[1] = { gEngine.app != nullptr ? gEngine.app->window : nullptr };
    const void **rendererWindow = gEngine.useVulkan ? nativeWindowHandles : nullptr;
    if (!gEngine.renderer->Initialize(0, rendererWindow, 1, false, false, &gRenderReporter, false, nullptr)) {
        ALOGF("Could not initialize piRenderer");
    }
    ALOGV("IMM Android renderer API: %s", gEngine.useVulkan ? "Vulkan" : "GLES");

    gEngine.stereoMode = ImmPlayer::StereoMode::None;
    gEngine.soundBackend = piCreateSoundEngineBackend(piSoundEngineBackend::API::Android, gEngine.log);
    if (!gEngine.soundBackend) {
        ALOGW("Android audio backend unavailable; using stereo rendering fallback");
        gEngine.stereoMode = ImmPlayer::StereoMode::Fallback;
    } else {
        piSoundEngineBackend::Configuration config;
        config.mLowLatency = true;
        config.mSampleRate = 48000;
        config.mBufferSize = 512;
        const char *tempPath = getAssetDirectory();
        if ((!tempPath || !*tempPath) && gEngine.app && gEngine.app->activity)
            tempPath = gEngine.app->activity->internalDataPath;
        config.mTempPath = tempPath;
        if (!gEngine.soundBackend->Init(nullptr, -1, &config)) {
            ALOGW("Android audio backend init failed; using stereo rendering fallback");
            gEngine.soundBackend->Deinit();
            piDestroySoundEngineBackend(gEngine.soundBackend);
            gEngine.soundBackend = nullptr;
            gEngine.stereoMode = ImmPlayer::StereoMode::Fallback;
        }
    }

    gEngine.viewer = new Viewer();
}

bool loadPath(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    initViewer();

    if (gEngine.activeSettings) {
        gEngine.activeSettings->End();
        delete gEngine.activeSettings;
        gEngine.activeSettings = nullptr;
    }

    Settings *settings = new Settings();
    gEngine.activeSettings = settings;
    settings->mPlayback.mLocation = ImmCore::trans3d::identity();
    settings->mPlayback.mPlayerSpawn.mLocation.InitCopyW(gEngine.playerSpawnLocation.c_str());
    settings->mPlayback.mPlayerSpawn.mCustom = ImmCore::trans3d::identity();

    settings->mRendering.mRenderingAPI = gEngine.useVulkan ? Settings::Rendering::API::Vulkan : Settings::Rendering::API::GLES;
    settings->mRendering.mXRRuntime = Settings::Rendering::XRRuntime::Legacy;
    settings->mRendering.mRenderingTechnique = gEngine.renderingTechnique;
    settings->mRendering.mEnableVR = false;

    if (!settings->mFiles.mLoad.Init(16, false)) {
        return false;
    }
    settings->mFiles.mLoad.New(1, true);
    settings->mFiles.mLoad[0].InitCopyW(path.c_str());
    char *utf8Path = piws2str(path.c_str());
    ALOGV("IMMAVAL loadPath path=%s settings=%p renderer=%s",
          utf8Path ? utf8Path : "",
          settings,
          gEngine.useVulkan ? "Vulkan" : "GLES");
    free(utf8Path);

    const bool success = gEngine.viewer->Init(
        0,
        gEngine.renderer,
        gEngine.soundBackend ? gEngine.soundBackend->GetEngine() : nullptr,
        gEngine.log,
        gEngine.timer,
        gEngine.stereoMode,
        settings);

    gEngine.viewerInitialized = success;
    gEngine.firstFrame = true;
    gEngine.frameCount = 0;
    gEngine.validationCaptureWritten = false;
    ALOGV("IMMAVAL loadPath result=%d settings=%p", success ? 1 : 0, settings);
    return success;
}

void pollMessages() {
    std::lock_guard<std::mutex> lock(gMessageMutex);
    if (gPendingPath.empty()) {
        if (!gEngine.viewerInitialized && !gTriedAutoLoad) {
            gTriedAutoLoad = true;
            const std::string autoPath = ResolveInitialImmPath();
            if (!autoPath.empty()) {
                wchar_t *widePath = pistr2ws(autoPath.c_str());
                gPendingPath = widePath ? widePath : L"";
                free(widePath);
            }
            ALOGV("IMMAVAL pollMessages autoload tried pathPresent=%d", gPendingPath.empty() ? 0 : 1);
        }

        if (gPendingPath.empty()) {
            return;
        }
    }

    const std::wstring path = gPendingPath;
    gPendingPath.clear();

    if (gEngine.viewerInitialized && gEngine.viewer) {
        gEngine.viewer->UnloadAllSync();
        gEngine.viewer->Deinit();
        gEngine.viewerInitialized = false;
    }

    if (!loadPath(path)) {
        ALOGW("Failed to load IMM at path");
    }
}

void renderFrame() {
    if (!gEngine.viewerInitialized) {
        return;
    }

    if (!gEngine.useVulkan) {
        glViewport(0, 0, renderWidth(), renderHeight());
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else if (!ensureRenderTarget() || !gEngine.renderer->SetRenderTarget(gEngine.renderTarget)) {
        ALOGE("Failed to bind Android Vulkan render target");
        return;
    }

    const double now = gEngine.timer->GetTime();
    static double lastTime = now;
    const float dtime = float(now - lastTime);
    lastTime = now;

    const int width = renderWidth();
    const int height = renderHeight();
    const vec4 monoProjectionFov = vec4(0.0f);
    const trans3d vrToHead = trans3d::identity();

    gEngine.viewer->GlobalWork(nullptr, false, vrToHead, nullptr, nullptr, gEngine.log, dtime,
                               ivec2(width, height), true, 8000, gEngine.firstFrame);
    gEngine.viewer->GlobalRender(vrToHead, monoProjectionFov);
    gEngine.viewer->RenderMono(ivec2(width, height), vrToHead, 0);
    const ImmPlayer::Player::PerformanceInfo &perf = gEngine.viewer->GetPerformanceInfoForFrame();
    if (!gEngine.useVulkan) {
        writeValidationCaptureIfReady(perf);
    }
    if (gEngine.frameCount == 0 || gEngine.frameCount == 60) {
        ALOGV("IMMAVAL renderFrame frame=%u playerFrame=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d triangles=%d size=%dx%d renderer=%s firstFrame=%d renderTarget=%p colorTexture=%p",
              gEngine.frameCount,
              static_cast<unsigned long long>(perf.validationTimeFrame),
              perf.numDrawCalls,
              perf.numPaintDrawCalls,
              perf.numPictureDrawCalls,
              perf.numTriangles,
              width,
              height,
              gEngine.useVulkan ? "Vulkan" : "GLES",
              gEngine.firstFrame ? 1 : 0,
              gEngine.renderTarget,
              gEngine.colorTexture);
    }
    ++gEngine.frameCount;

    if (gEngine.useVulkan) {
        gEngine.renderer->SetRenderTarget(nullptr);
        gEngine.renderer->SwapBuffers();
        writeValidationCaptureIfReady(perf);
    } else {
        eglSwapBuffers(gEngine.display, gEngine.surface);
    }
}

static float getPinchDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

static int32_t handleInput(android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }

    int32_t action = AMotionEvent_getAction(event);
    int32_t pointerCount = AMotionEvent_getPointerCount(event);
    int32_t actionType = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    // Single finger: camera rotation (look around)
    if (pointerCount == 1) {
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        switch (actionType) {
            case AMOTION_EVENT_ACTION_DOWN:
                gEngine.touch1.active = true;
                gEngine.touch1.startX = x;
                gEngine.touch1.startY = y;
                gEngine.touch1.lastX = x;
                gEngine.touch1.lastY = y;
                gEngine.touch1.deltaX = 0;
                gEngine.touch1.deltaY = 0;
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                if (gEngine.touch1.active) {
                    gEngine.touch1.deltaX = x - gEngine.touch1.lastX;
                    gEngine.touch1.deltaY = y - gEngine.touch1.lastY;
                    gEngine.touch1.lastX = x;
                    gEngine.touch1.lastY = y;
                }
                break;

            case AMOTION_EVENT_ACTION_UP:
                gEngine.touch1.active = false;
                gEngine.touch1.deltaX = 0;
                gEngine.touch1.deltaY = 0;
                break;
        }
    }
    // Two fingers: forward/backward movement
    else if (pointerCount == 2) {
        float x1 = AMotionEvent_getX(event, 0);
        float y1 = AMotionEvent_getY(event, 0);
        float x2 = AMotionEvent_getX(event, 1);
        float y2 = AMotionEvent_getY(event, 1);

        switch (actionType) {
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                // Second finger went down - start pinch tracking
                gEngine.isPinching = true;
                gEngine.pinchStartDistance = getPinchDistance(x1, y1, x2, y2);
                gEngine.touch2.startX = (x1 + x2) * 0.5f;
                gEngine.touch2.startY = (y1 + y2) * 0.5f;
                gEngine.touch2.lastX = gEngine.touch2.startX;
                gEngine.touch2.lastY = gEngine.touch2.startY;
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                if (gEngine.isPinching) {
                    float currentPinch = getPinchDistance(x1, y1, x2, y2);
                    float pinchDelta = currentPinch - gEngine.pinchStartDistance;
                    
                    // Also track vertical drag of both fingers
                    float avgY = (y1 + y2) * 0.5f;
                    gEngine.touch2.deltaY = avgY - gEngine.touch2.lastY;
                    gEngine.touch2.lastY = avgY;
                    
                    // Use pinch delta for forward/backward movement
                    gEngine.touch2.deltaX = pinchDelta;
                    gEngine.pinchStartDistance = currentPinch;
                }
                break;

            case AMOTION_EVENT_ACTION_POINTER_UP:
                if (pointerIndex == 1) {
                    // Second finger lifted, go back to single touch mode
                    gEngine.isPinching = false;
                    gEngine.touch2.active = false;
                    gEngine.touch2.deltaX = 0;
                    gEngine.touch2.deltaY = 0;
                }
                break;
        }
    }

    return 1;
}

void updateCameraFromTouch(float dtime) {
    if (!gEngine.viewer || !gEngine.viewerInitialized) return;

    const float rotationSensitivity = 3.0f;
    const float movementSensitivity = 0.003f;

    // Single finger rotation (drag to look around)
    if (gEngine.touch1.active && (fabsf(gEngine.touch1.deltaX) > 0.5f || fabsf(gEngine.touch1.deltaY) > 0.5f)) {
        float rotX = gEngine.touch1.deltaX * rotationSensitivity / gEngine.width;
        float rotY = gEngine.touch1.deltaY * rotationSensitivity / gEngine.height;
        
        // Apply rotation through viewer
        gEngine.viewer->RotateCamera(rotX, rotY);
        
        // Decay the deltas
        gEngine.touch1.deltaX *= 0.8f;
        gEngine.touch1.deltaY *= 0.8f;
    }

    // Two finger movement (pinch or vertical drag to move forward/backward)
    if (gEngine.isPinching && fabsf(gEngine.touch2.deltaX) > 1.0f) {
        float moveDistance = gEngine.touch2.deltaX * movementSensitivity;
        
        // Apply movement through viewer
        gEngine.viewer->MoveCameraForward(moveDistance);
        
        // Decay
        gEngine.touch2.deltaX *= 0.8f;
    }
}

void handleCmd(android_app* app, int32_t cmd) {
    ALOGV("IMMAVAL appCmd cmd=%d hasWindow=%d viewerInitialized=%d window=%p",
          cmd,
          gEngine.hasWindow ? 1 : 0,
          gEngine.viewerInitialized ? 1 : 0,
          app ? app->window : nullptr);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr && !gEngine.hasWindow) {
                if (gEngine.useVulkan) {
                    if (!initVulkanWindow(app)) {
                        ALOGF("Failed to init Vulkan window");
                    }
                } else {
                    if (!initEgl(app)) {
                        ALOGF("Failed to init EGL");
                    }
                }
                initViewer();
                pollMessages();
            }
            break;
        case APP_CMD_TERM_WINDOW:
            shutdownEgl();
            break;
        case APP_CMD_DESTROY:
            gEngine.running = false;
            break;
        default:
            break;
    }
}

} // namespace

extern "C" {

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetAssetDirectory(
    JNIEnv* jni,
    jclass,
    jstring assetDirectory) {
    const char* assetDirUtf = assetDirectory ? jni->GetStringUTFChars(assetDirectory, 0) : "";
    setAssetDirectory(assetDirUtf);
    if (assetDirectory) {
        jni->ReleaseStringUTFChars(assetDirectory, assetDirUtf);
    }
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetExternalFilesDirectory(
    JNIEnv* jni,
    jclass,
    jstring externalDirectory) {
    const char* externalDirUtf = externalDirectory ? jni->GetStringUTFChars(externalDirectory, 0) : "";
    setExternalFilesDirectory(externalDirUtf);
    if (externalDirectory) {
        jni->ReleaseStringUTFChars(externalDirectory, externalDirUtf);
    }
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSendMessage(
    JNIEnv* jni,
    jclass,
    jstring jMessage,
    jint jMessageType) {
    if (jMessageType != 0 || jMessage == nullptr) {
        return;
    }
    const char* messageUtf = jni->GetStringUTFChars(jMessage, 0);
    std::lock_guard<std::mutex> lock(gMessageMutex);
    wchar_t* widePath = pistr2ws(messageUtf);
    gPendingPath = widePath ? widePath : L"";
    free(widePath);
    jni->ReleaseStringUTFChars(jMessage, messageUtf);
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetQuillRenderingTechnique(
    JNIEnv*,
    jclass,
    jint renderingTechnique) {
    gEngine.renderingTechnique = static_cast<Settings::Rendering::Technique>(renderingTechnique);
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetRenderingApi(
    JNIEnv* jni,
    jclass,
    jstring jRenderingApi) {
    if (!jRenderingApi) {
        return;
    }

    const char* renderingApiUtf = jni->GetStringUTFChars(jRenderingApi, 0);
    if (gEngine.renderer) {
        ALOGW("IMM Android renderer API change ignored after renderer initialization: %s", renderingApiUtf ? renderingApiUtf : "");
        jni->ReleaseStringUTFChars(jRenderingApi, renderingApiUtf);
        return;
    }

    if (iEqualsIgnoreCase(renderingApiUtf, "vulkan")) {
        gEngine.useVulkan = true;
        ALOGV("IMM Android requested renderer API: Vulkan");
    } else if (iEqualsIgnoreCase(renderingApiUtf, "gles") || iEqualsIgnoreCase(renderingApiUtf, "opengles") || iEqualsIgnoreCase(renderingApiUtf, "opengl")) {
        gEngine.useVulkan = false;
        ALOGV("IMM Android requested renderer API: GLES");
    } else {
        ALOGW("IMM Android ignoring unknown renderer API: %s", renderingApiUtf ? renderingApiUtf : "");
    }
    jni->ReleaseStringUTFChars(jRenderingApi, renderingApiUtf);
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetValidationRenderSize(
    JNIEnv*,
    jclass,
    jint width,
    jint height) {
    if (width <= 0 || height <= 0) {
        ALOGW("IMMAVAL ignoring invalid validation render size: %dx%d", width, height);
        return;
    }
    if (gEngine.renderer) {
        ALOGW("IMMAVAL validation render size change ignored after renderer initialization: %dx%d", width, height);
        return;
    }
    gEngine.validationRenderWidth = width;
    gEngine.validationRenderHeight = height;
    ALOGV("IMMAVAL validation render size: %dx%d", width, height);
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetEyeBufferScale(
    JNIEnv*,
    jclass,
    jfloat) {
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetPlayerSpawnLocation(
    JNIEnv* jni,
    jclass,
    jstring jSpawnLocation) {
    if (!jSpawnLocation) {
        return;
    }
    const char* spawnLocationUtf = jni->GetStringUTFChars(jSpawnLocation, 0);
    gEngine.playerSpawnLocation = pistr2ws(spawnLocationUtf);
    jni->ReleaseStringUTFChars(jSpawnLocation, spawnLocationUtf);
}

void Java_org_linuxfoundation_imm_player_MainActivity_nativeSetTrackingTransformLevel(
    JNIEnv*,
    jclass,
    jstring) {
}

} // extern "C"

void android_main(android_app* app) {
#if defined(IMM_ANDROID_RENDERER_VULKAN)
    gEngine.useVulkan = true;
#endif
    app->onAppCmd = handleCmd;
    app->onInputEvent = handleInput;
    gEngine.app = app;
    gEngine.running = true;

#if defined(IMM_ANDROID_XR_RUNTIME_OPENXR)
    RunAndroidOpenXrStartupProbe(app);
#endif

    while (gEngine.running) {
        int events = 0;
        android_poll_source* source = nullptr;
        while (ALooper_pollAll(gEngine.hasWindow ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                gEngine.running = false;
                break;
            }
        }

        if (!gEngine.hasWindow) {
            continue;
        }

        pollMessages();
        
        // Update camera from touch input before rendering
        const double now = gEngine.timer->GetTime();
        static double lastTime = now;
        const float dtime = float(now - lastTime);
        lastTime = now;
        updateCameraFromTouch(dtime);
        
        renderFrame();
    }

    shutdownViewer();
    shutdownEgl();
}
