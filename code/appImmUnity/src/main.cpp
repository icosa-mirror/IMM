// ------------------------
// Host API
// ------------------------
// bool Init( Options )                                // once per process and session
// void End()                                           // once per process and session
// void GlobalWork(objectToWorldMatrix)                 // once per frame, global for all cameras
// void SetMatrices(cameraID, stereoType, ... )         // once per frame and per camera
// PlayerInfo GetPlayerInfo()                           // live data, query every frame
//
//
// Options = { ColorSpace, Antialiasing, RequestViewpointDisplay }
//
// PlayerInfo =
// {
//	  Array of Viewpoints
//	  {
//        Name
//        Location // position, scale and orientation
//        Extent   // box or sphere volume of allowed viewer locations
//        Allowed  // locomotion restrictions, bitmask of { TranslateHorizontal | TranslateVertical | RotateHoritonal | RotateVertical | Scale }
//    }
//    Lighting
//    {
//        BackgroundColor
//        Array of Colorizers
//        {
//            Type       // { Sphere, Point, Gradient, ... }
//            Paramaters = { ... }
//        }
//    }
//    Array of Grablables
//    {
//    }
// }
//
// ------------------------
// Document API
// ------------------------
// id   Load(char *fileName)
// id   LoadStreaming(char *url)
// void Unload(id)
// DocumentInfo  GetDocumentInfo(id)
//
// DocumentInfo
// {
// 	 DocumentType // { Still, Animated, Comic }
// 	 IsGrabbable
// 	 HasSound
// }
//
//
// ------------------------
// Playback API
// ------------------------
// void   Next(id)           // jump to next chapter
// void   Prev(id)           // jump to prev chapter
// void   Pause(id)          // pause
// void   Resume(id)         // resume
// void   Restart(id)        // restart the whole comic
// void   Replay(id)         // restart the current chapter
// bool   HasNext(id)        // is it possible to call Next
// bool   HasPrev(id)        // is it possible to call Prev
// void   Show(id)           // show
// void   Hide(id)           // hide
// void   SetVolume(volvume) // sound volume
// time   GetSynchTime()     // to synch across clients
// void   SetSynchTime(time) // to synch across clients
//
// DocumentState GetDocumentState(id);
//
// DocumentState
// {
//	   LoadingState  // { Unloaded, Loading, Loaded, Unloading, Failed }
//	   PlaybackState // { Playing, Paused, PausedAndHidden, Waiting, Finished }
// }


#define VERBOSE 0

#if defined(__APPLE__)
#import <Metal/Metal.h>
#endif

#include "appImmShared/src/imm_engine_bridge.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmPlayer/src/player.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#include <chrono>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#if defined(WINDOWS)
#include "libImmExporter/src/document/sequence.h"
#include "libImmExporter/src/document/layerPaint.h"
#include "libImmExporter/src/document/layerPaint/element.h"
#include "libImmExporter/src/toImmersive/toImmersive.h"
#include "libImmExporter/src/toImmersive/toImmersiveLayerSound.h"
#include <windows.h>
#include <array>
#include <string>
#endif
#include "IUnityGraphics.h"
#if defined(WINDOWS)
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphicsD3D12.h"
#endif
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
#include "IUnityGraphicsVulkanMinimal.h"
#include "libImmCore/src/libRender/vulkan/piVulkan_Renderer.h"
#endif
#if defined(__APPLE__)
#include "IUnityGraphicsMetal.h"
#include "libImmCore/src/libRender/metal/piMetal_Renderer.h"
#include <cstdlib>
#include <cstdio>
#include <mutex>
#endif
using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

#if defined(__ANDROID__) || defined(ANDROID)
#define _stdcall
#include <android/log.h>
#include <GLES3/gl3.h>
#endif

#if defined(WINDOWS)
namespace
{
    bool iFileExists(const std::wstring &path)
    {
        const DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::wstring iDirName(const std::wstring &path)
    {
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L"";
        }
        return path.substr(0, slash);
    }

    void iPreloadSharedRuntimeDependencies(HMODULE moduleHandle)
    {
        wchar_t modulePath[MAX_PATH] = {};
        const DWORD len = GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
        {
            return;
        }

        const std::wstring modulePathStr(modulePath);
        const std::wstring unityPluginDir = iDirName(modulePathStr);
        const std::wstring unityPackageDir = iDirName(iDirName(unityPluginDir));
        const std::wstring packagesDir = iDirName(unityPackageDir);
        const std::wstring strokePluginDir = packagesDir + L"\\com.immersive-foundation.imm-stroke-reader\\Plugins\\x86_64";

        constexpr std::array<const wchar_t*, 5> kSharedDeps = {
            L"zlib1.dll",
            L"jpeg62.dll",
            L"libpng16.dll",
            L"ogg.dll",
            L"vorbis.dll",
        };

        for (const wchar_t* depName : kSharedDeps)
        {
            const std::wstring fullPath = strokePluginDir + L"\\" + depName;
            if (!iFileExists(fullPath))
            {
                continue;
            }

            LoadLibraryW(fullPath.c_str());
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        iPreloadSharedRuntimeDependencies(hModule);
    }

    return TRUE;
}
#endif

struct ImmUnityPlugin
{
	//----------------------------
	// Unity provided classes
	//----------------------------
	struct
		{
			IUnityInterfaces * mUnityInterfaces = nullptr;
			IUnityGraphics   * mGraphics = nullptr;
			void             * mDevice = nullptr;
	        UnityGfxRenderer mRenderer = kUnityGfxRendererNull;
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
            IUnityGraphicsVulkan *mVulkan = nullptr;
            UnityVulkanInstance mVulkanInstance = {};
            struct
            {
                UnityRenderBuffer color = nullptr;
                void *colorTexture = nullptr;
                UnityRenderBuffer depth = nullptr;
                int width = 0;
                int height = 0;
                int samples = 1;
            } mVulkanCameraTarget[256];
#endif
#if defined(__APPLE__)
	        IUnityGraphicsMetalV2 *mMetalV2 = nullptr;
	        IUnityGraphicsMetalV1 *mMetal = nullptr;
#endif
		}UnityAPI;

	    ImmShared::ImmEngineBridge mBridge;

#if defined(__APPLE__)
	    struct
	    {
	        int mViewportWidth = 0;
	        int mViewportHeight = 0;
	    } mMetalCameraViewport[256];
#endif
	};

// ----------------------------------------------------------------------------------------------------------------------------------------------------

// this is the only global data structure, live cycle is plugin load/unload
static ImmUnityPlugin gImmUnityPlugin;

static Player &iPlayer()
{
    return *gImmUnityPlugin.mBridge.GetPlayer();
}

static piLog &iLog()
{
    return *gImmUnityPlugin.mBridge.GetLog();
}

#if defined(__APPLE__)
static std::recursive_mutex sImmUnityNativeMutex;
#define IMM_UNITY_NATIVE_LOCK() \
    std::lock_guard<std::recursive_mutex> immUnityNativeLock(sImmUnityNativeMutex)
#else
#define IMM_UNITY_NATIVE_LOCK()
#endif

#if defined(__ANDROID__) || defined(ANDROID)
// Deferred initialization for Android - renderer must be initialized on render thread
static struct {
	bool needsInit = false;
	bool isInitialized = false;
	int colorSpace = 0;
	int antialiasing = 8;
} sAndroidDeferredInit;

// Called on render thread to complete initialization
static bool AndroidCompleteInit() {
	__android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "AndroidCompleteInit - completing deferred init on render thread");

	__android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "Initializing GLES renderer...");
	if (!gImmUnityPlugin.mBridge.CompleteGraphicsInitialization())
	{
		__android_log_print(ANDROID_LOG_ERROR, "ImmUnityPlugin", "Failed to initialize GLES renderer in deferred init");
		return false;
	}
	__android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "GLES renderer initialized in deferred init");
	__android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "ImmPlayer initialized in deferred init - SUCCESS");

	sAndroidDeferredInit.isInitialized = true;
	sAndroidDeferredInit.needsInit = false;
	return true;
}
#endif

#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
static constexpr int kUnityVulkanPrepareEventFlag = 0x80;
static constexpr int kUnityVulkanCustomBlitEventID = 6;
static void iConfigureUnityVulkanEvent(int eventID, bool logEvent);
#endif

static void UNITY_INTERFACE_API iOnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    IMM_UNITY_NATIVE_LOCK();
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		UnityGfxRenderer apiType = gImmUnityPlugin.UnityAPI.mGraphics->GetRenderer();
	    gImmUnityPlugin.UnityAPI.mRenderer = apiType;

#if defined(WINDOWS)
		if (apiType == kUnityGfxRendererD3D11)
		{
			IUnityGraphicsD3D11* ud3d = gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsD3D11>();
			gImmUnityPlugin.UnityAPI.mDevice = ud3d->GetDevice();
		}
		else if (apiType == kUnityGfxRendererD3D12)
		{
			IUnityGraphicsD3D12v2* ud3d = gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsD3D12v2>();
			gImmUnityPlugin.UnityAPI.mDevice = ud3d->GetDevice();
		}
		else if(apiType == kUnityGfxRendererOpenGLCore || apiType == kUnityGfxRendererOpenGLES20 || apiType == kUnityGfxRendererOpenGLES30)
		{
			gImmUnityPlugin.UnityAPI.mDevice = nullptr;
		}
        else if (apiType == kUnityGfxRendererVulkan)
        {
            gImmUnityPlugin.UnityAPI.mVulkan = gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsVulkan>();
            if (gImmUnityPlugin.UnityAPI.mVulkan)
            {
                gImmUnityPlugin.UnityAPI.mVulkanInstance = gImmUnityPlugin.UnityAPI.mVulkan->Instance();
                for (int cameraID = 0; cameraID < 256; ++cameraID)
                {
                    iConfigureUnityVulkanEvent((cameraID << 8) | 0, false);
                    iConfigureUnityVulkanEvent((cameraID << 8) | 1, false);
                    iConfigureUnityVulkanEvent((cameraID << 8) | kUnityVulkanPrepareEventFlag, false);
                }
                iConfigureUnityVulkanEvent(kUnityVulkanCustomBlitEventID, false);
            }
            gImmUnityPlugin.UnityAPI.mDevice = gImmUnityPlugin.UnityAPI.mVulkanInstance.device;
        }
#elif defined(__ANDROID__) || defined(ANDROID)
		if (apiType == kUnityGfxRendererOpenGLES30)
		{
			gImmUnityPlugin.UnityAPI.mDevice = nullptr;
		}
        else if (apiType == kUnityGfxRendererVulkan)
        {
            gImmUnityPlugin.UnityAPI.mVulkan = gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsVulkan>();
            if (gImmUnityPlugin.UnityAPI.mVulkan)
            {
                gImmUnityPlugin.UnityAPI.mVulkanInstance = gImmUnityPlugin.UnityAPI.mVulkan->Instance();
                for (int cameraID = 0; cameraID < 256; ++cameraID)
                {
                    iConfigureUnityVulkanEvent((cameraID << 8) | 0, false);
                    iConfigureUnityVulkanEvent((cameraID << 8) | 1, false);
                    iConfigureUnityVulkanEvent((cameraID << 8) | kUnityVulkanPrepareEventFlag, false);
                }
                iConfigureUnityVulkanEvent(kUnityVulkanCustomBlitEventID, false);
            }
            gImmUnityPlugin.UnityAPI.mDevice = gImmUnityPlugin.UnityAPI.mVulkanInstance.device;
            __android_log_print(
                ANDROID_LOG_INFO,
                "ImmUnityPlugin",
                "Unity Android Vulkan interface initialized: interface=%p instance=%p physicalDevice=%p device=%p queue=%p family=%u",
                gImmUnityPlugin.UnityAPI.mVulkan,
                gImmUnityPlugin.UnityAPI.mVulkanInstance.instance,
                gImmUnityPlugin.UnityAPI.mVulkanInstance.physicalDevice,
                gImmUnityPlugin.UnityAPI.mVulkanInstance.device,
                gImmUnityPlugin.UnityAPI.mVulkanInstance.graphicsQueue,
                gImmUnityPlugin.UnityAPI.mVulkanInstance.queueFamilyIndex);
        }
#else
		if (apiType == kUnityGfxRendererOpenGLCore)
		{
			gImmUnityPlugin.UnityAPI.mDevice = nullptr;
		}
		else if (apiType == kUnityGfxRendererMetal)
		{
	        gImmUnityPlugin.UnityAPI.mMetalV2 =
	            gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsMetalV2>();
	        gImmUnityPlugin.UnityAPI.mMetal =
	            gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsMetalV1>();
	        if (gImmUnityPlugin.UnityAPI.mMetalV2)
	        {
	            gImmUnityPlugin.UnityAPI.mDevice = gImmUnityPlugin.UnityAPI.mMetalV2->MetalDevice();
	        }
	        else if (gImmUnityPlugin.UnityAPI.mMetal)
	        {
	            gImmUnityPlugin.UnityAPI.mDevice = gImmUnityPlugin.UnityAPI.mMetal->MetalDevice();
	        }
	        else
	        {
	            gImmUnityPlugin.UnityAPI.mDevice = nullptr;
	        }
            std::fprintf(
                stderr,
                "IMM_UNITY_METAL_DEVICE_INIT renderer=%d v1=%d v2=%d device=%p\n",
                static_cast<int>(apiType),
                gImmUnityPlugin.UnityAPI.mMetal ? 1 : 0,
                gImmUnityPlugin.UnityAPI.mMetalV2 ? 1 : 0,
                gImmUnityPlugin.UnityAPI.mDevice);
            std::fflush(stderr);
		}
#endif
	}
	else if (eventType == kUnityGfxDeviceEventShutdown)
	{
	}
}

static int sRenderEventCount = 0;
static int sUnityMetalRenderReportCount = 0;
static int sUnityMetalRenderBoundaryReportCount = 0;

static bool iEnvFlagEnabled(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

static uint32_t iEnvUIntOrDefault(const char *name, uint32_t fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
    {
        return fallback;
    }
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 0);
    if (end == value)
    {
        return fallback;
    }
    return static_cast<uint32_t>(parsed);
}

#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
static UnityVulkanPluginEventConfig iMakeUnityVulkanEventConfig(int eventID)
{
    UnityVulkanPluginEventConfig config = {};
    const bool prepareEvent = (eventID & kUnityVulkanPrepareEventFlag) != 0;
#if defined(__ANDROID__) || defined(ANDROID)
    const bool useHostRenderPass = iEnvFlagEnabled("IMM_UNITY_VK_USE_HOST_RENDER_PASS");
    config.renderPassPrecondition = (prepareEvent || !useHostRenderPass)
        ? kUnityVulkanRenderPass_EnsureOutside
        : kUnityVulkanRenderPass_EnsureInside;
#else
    config.renderPassPrecondition = prepareEvent ? kUnityVulkanRenderPass_EnsureOutside : kUnityVulkanRenderPass_EnsureInside;
#endif
    if (!prepareEvent && iEnvFlagEnabled("IMM_UNITY_VK_DONTCARE_RENDERPASS"))
    {
        config.renderPassPrecondition = kUnityVulkanRenderPass_DontCare;
    }
    config.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
    // Match Unity's supported direct-recording sample: Unity must submit the
    // preceding frame before plugin-owned buffers, descriptors, or pipelines
    // are recycled using safeFrameNumber.
    config.flags =
        kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission |
        kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
    if (iEnvFlagEnabled("IMM_UNITY_VK_NO_MODIFIES_STATE"))
    {
        config.flags &= ~kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
    }
    if (iEnvFlagEnabled("IMM_UNITY_VK_ENSURE_PREVIOUS"))
    {
        config.flags |= kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission;
    }
    return config;
}

static void iConfigureUnityVulkanEvent(int eventID, bool logEvent)
{
    if (!gImmUnityPlugin.UnityAPI.mVulkan || gImmUnityPlugin.UnityAPI.mRenderer != kUnityGfxRendererVulkan)
    {
        return;
    }
    UnityVulkanPluginEventConfig config = iMakeUnityVulkanEventConfig(eventID);
    gImmUnityPlugin.UnityAPI.mVulkan->ConfigureEvent(eventID, &config);
    if (logEvent)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_EVENTCFG_20260612] configured event=%d renderPassPrecondition=%d graphicsQueueAccess=%d flags=0x%x",
            eventID,
            static_cast<int>(config.renderPassPrecondition),
            static_cast<int>(config.graphicsQueueAccess),
            config.flags);
    }
}
#endif

#if defined(__APPLE__)
static piTexture sUnityMetalOffscreenColor = nullptr;
static piTexture sUnityMetalOffscreenDepth = nullptr;
static piRTarget sUnityMetalOffscreenTarget = nullptr;
static int sUnityMetalOffscreenWidth = 0;
static int sUnityMetalOffscreenHeight = 0;

static bool iEnsureUnityMetalOffscreenTarget(piRenderer *renderer, int width, int height)
{
    if (!renderer || width <= 0 || height <= 0)
    {
        return false;
    }
    if (sUnityMetalOffscreenTarget && sUnityMetalOffscreenWidth == width && sUnityMetalOffscreenHeight == height)
    {
        return true;
    }

    if (sUnityMetalOffscreenTarget)
    {
        renderer->DestroyRenderTarget(sUnityMetalOffscreenTarget);
        sUnityMetalOffscreenTarget = nullptr;
    }
    if (sUnityMetalOffscreenColor)
    {
        renderer->DestroyTexture(sUnityMetalOffscreenColor);
        sUnityMetalOffscreenColor = nullptr;
    }
    if (sUnityMetalOffscreenDepth)
    {
        renderer->DestroyTexture(sUnityMetalOffscreenDepth);
        sUnityMetalOffscreenDepth = nullptr;
    }

    const piRenderer::TextureInfo colorInfo = {
        piRenderer::TextureType::T2D,
        piRenderer::Format::C3_11_11_10_FLOAT,
        width,
        height,
        1,
        1,
        1,
        0
    };
    const piRenderer::TextureInfo depthInfo = {
        piRenderer::TextureType::T2D,
        piRenderer::Format::D1_32_FLOAT,
        width,
        height,
        1,
        1,
        1,
        0
    };

    sUnityMetalOffscreenColor = renderer->CreateTexture(nullptr, &colorInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, nullptr);
    sUnityMetalOffscreenDepth = renderer->CreateTexture(nullptr, &depthInfo, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::CLAMP, 1.0f, nullptr);
    sUnityMetalOffscreenTarget = renderer->CreateRenderTarget(sUnityMetalOffscreenColor, nullptr, nullptr, nullptr, sUnityMetalOffscreenDepth);
    if (!sUnityMetalOffscreenColor || !sUnityMetalOffscreenDepth || !sUnityMetalOffscreenTarget)
    {
        return false;
    }

    sUnityMetalOffscreenWidth = width;
    sUnityMetalOffscreenHeight = height;
    return true;
}
#endif

static bool IsReasonableBound3(const bound3& b)
{
	const float limit = 1.0e6f;
	if (b.mMinX > b.mMaxX || b.mMinY > b.mMaxY || b.mMinZ > b.mMaxZ)
		return false;
	if (b.mMinX < -limit || b.mMinX > limit) return false;
	if (b.mMaxX < -limit || b.mMaxX > limit) return false;
	if (b.mMinY < -limit || b.mMinY > limit) return false;
	if (b.mMaxY < -limit || b.mMaxY > limit) return false;
	if (b.mMinZ < -limit || b.mMinZ > limit) return false;
	if (b.mMaxZ < -limit || b.mMaxZ > limit) return false;
	return true;
}

static void UNITY_INTERFACE_API iOnRenderEvent(int event_id);

#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
struct UnityVulkanRenderContext
{
    piRenderer *renderer = nullptr;
    int cameraID = 0;
    int eventID = 0;
    uint64_t colorImage = 0;
    uint32_t colorFormat = 0;
    uint32_t colorSamples = 1;
    uint64_t depthImage = 0;
    uint32_t depthFormat = 0;
    uint32_t depthSamples = 1;
    int width = 0;
    int height = 0;
};

static UnityVulkanRenderContext sUnityVulkanRenderContext[256];
static int sUnityVulkanRenderTargetDiagnosticCount = 0;
static std::atomic<bool> sUnityVulkanHostDrawBisectionEnabled(false);
static std::atomic<uint32_t> sUnityVulkanHostDrawBisectionInvocation(0);
static int sUnityVulkanCustomBlitDiagnosticCount = 0;

struct UnityRenderingExtCustomBlitParamsMinimal
{
    UnityTextureID source;
    UnityRenderBuffer destination;
    unsigned int command;
    unsigned int commandParam;
    unsigned int commandFlags;
};

static bool iRecordUnityVulkanCamera(
    UnityVulkanRenderContext *context,
    VkCommandBuffer unityCommandBuffer,
    uint64_t currentFrameNumber,
    uint64_t safeFrameNumber)
{
    const auto renderStart = std::chrono::steady_clock::now();
    if (!context || !context->renderer || !unityCommandBuffer)
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan command recording skipped: missing context or command buffer");
        return false;
    }

    piRendererVulkan *vulkanRenderer = static_cast<piRendererVulkan *>(context->renderer);
    if (!vulkanRenderer->BeginExternalImageCommandBufferFramePreserveColor(
            unityCommandBuffer,
            currentFrameNumber,
            safeFrameNumber,
            reinterpret_cast<void *>(static_cast<uintptr_t>(context->colorImage)),
            context->colorFormat,
            context->colorSamples,
            reinterpret_cast<void *>(static_cast<uintptr_t>(context->depthImage)),
            context->depthFormat,
            context->depthSamples,
            context->width,
            context->height,
            true))
    {
        iLog().Printf(
            LT_ERROR,
            L"Unity Vulkan command recording skipped: failed to begin borrowed command buffer frame for camera=%d",
            context->cameraID);
        return false;
    }

    const ImmShared::ImmEngineBridge::ViewportInfo viewport = {
        0.0f, 0.0f, static_cast<float>(context->width), static_cast<float>(context->height), 0.0f, 1.0f, true
    };
    const int eyeID = context->eventID & 1;
    const bool rendered = gImmUnityPlugin.mBridge.RenderCamera(context->cameraID, viewport, eyeID, true);
    vulkanRenderer->EndExternalImageFrame();

    const Player::PerformanceInfo &perf = iPlayer().GetPerformanceInfoForFrame();
    static uint64_t renderReportCount = 0;
    static uint64_t renderPerfFrames = 0;
    static double renderPerfTotalMilliseconds = 0.0;
    static double renderPerfMaxMilliseconds = 0.0;
    const double renderMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - renderStart).count();
    ++renderPerfFrames;
    renderPerfTotalMilliseconds += renderMilliseconds;
    if (renderMilliseconds > renderPerfMaxMilliseconds)
        renderPerfMaxMilliseconds = renderMilliseconds;
    if (renderReportCount < 8)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"Unity Vulkan render: camera=%d viewport=%dx%d rendered=%d drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture360DrawCalls=%d",
            context->cameraID,
            context->width,
            context->height,
            rendered ? 1 : 0,
            perf.numDrawCalls,
            perf.numPaintDrawCalls,
            perf.numPictureDrawCalls,
            perf.numPicture360DrawCalls);
        ++renderReportCount;
    }
    if (renderPerfFrames == 60)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_EXTERNAL_PERF_20260730] frames=%llu averageMilliseconds=%.3f maxMilliseconds=%.3f viewport=%dx%d",
            static_cast<unsigned long long>(renderPerfFrames),
            renderPerfTotalMilliseconds / static_cast<double>(renderPerfFrames),
            renderPerfMaxMilliseconds,
            context->width,
            context->height);
        renderPerfFrames = 0;
        renderPerfTotalMilliseconds = 0.0;
        renderPerfMaxMilliseconds = 0.0;
    }
    return rendered;
}

static bool iRenderUnityVulkanCamera(int cameraID, int event_id, piRenderer *renderer, UnityRenderBuffer colorOverride = nullptr)
{
    if (!gImmUnityPlugin.UnityAPI.mVulkan || gImmUnityPlugin.UnityAPI.mRenderer != kUnityGfxRendererVulkan)
    {
        return false;
    }

    const auto &target = gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID];
    UnityRenderBuffer colorTarget = colorOverride ? colorOverride : target.color;
    if (!colorTarget || !target.depth || target.width <= 0 || target.height <= 0)
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan render skipped: missing color/depth render buffers for camera=%d", cameraID);
        return true;
    }

    constexpr VkImageLayout kColorAttachmentLayout = 2; // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    constexpr VkImageLayout kDepthAttachmentLayout = 3; // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    constexpr VkPipelineStageFlags kColorAttachmentStage = 0x00000400; // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    constexpr VkPipelineStageFlags kDepthAttachmentStages = 0x00000100 | 0x00000200; // EARLY/LATE_FRAGMENT_TESTS
    constexpr VkAccessFlags kColorAttachmentAccess = 0x00000100 | 0x00000080; // COLOR_ATTACHMENT_WRITE/READ
    constexpr VkAccessFlags kDepthAttachmentAccess = 0x00000400 | 0x00000200; // DEPTH_STENCIL_ATTACHMENT_WRITE/READ

    gImmUnityPlugin.UnityAPI.mVulkan->EnsureOutsideRenderPass();

    UnityVulkanImage colorImage = {};
    const bool accessedColor = gImmUnityPlugin.UnityAPI.mVulkan->AccessRenderBufferTexture(
        colorTarget,
        UnityVulkanWholeImage,
        kColorAttachmentLayout,
        kColorAttachmentStage,
        kColorAttachmentAccess,
        kUnityVulkanResourceAccess_PipelineBarrier,
        &colorImage);
    if (!accessedColor)
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan render skipped: failed to access color render buffer for camera=%d", cameraID);
        return true;
    }

    UnityVulkanImage depthImage = {};
    if (!gImmUnityPlugin.UnityAPI.mVulkan->AccessRenderBufferTexture(
            target.depth,
            UnityVulkanWholeImage,
            kDepthAttachmentLayout,
            kDepthAttachmentStages,
            kDepthAttachmentAccess,
            kUnityVulkanResourceAccess_PipelineBarrier,
            &depthImage))
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan render skipped: failed to access depth render buffer for camera=%d", cameraID);
        return true;
    }

    if (sUnityVulkanRenderTargetDiagnosticCount < 24)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_DIRECT_RT_20260731] camera=%d colorRB=%p colorTexture=%p depthRB=%p colorImage=0x%llx colorFormat=%u colorLayout=%u colorUsage=0x%x colorSamples=%u colorExtent=%ux%ux%u depthImage=0x%llx depthFormat=%u depthLayout=%u depthUsage=0x%x depthSamples=%u depthExtent=%ux%ux%u",
            cameraID,
            colorTarget,
            nullptr,
            target.depth,
            static_cast<unsigned long long>(colorImage.image),
            static_cast<unsigned int>(colorImage.format),
            static_cast<unsigned int>(colorImage.layout),
            static_cast<unsigned int>(colorImage.usage),
            static_cast<unsigned int>(colorImage.samples),
            static_cast<unsigned int>(colorImage.extent.width),
            static_cast<unsigned int>(colorImage.extent.height),
            static_cast<unsigned int>(colorImage.extent.depth),
            static_cast<unsigned long long>(depthImage.image),
            static_cast<unsigned int>(depthImage.format),
            static_cast<unsigned int>(depthImage.layout),
            static_cast<unsigned int>(depthImage.usage),
            static_cast<unsigned int>(depthImage.samples),
            static_cast<unsigned int>(depthImage.extent.width),
            static_cast<unsigned int>(depthImage.extent.height),
            static_cast<unsigned int>(depthImage.extent.depth));
        ++sUnityVulkanRenderTargetDiagnosticCount;
    }

    const int width = colorImage.extent.width > 0 ? static_cast<int>(colorImage.extent.width) : target.width;
    const int height = colorImage.extent.height > 0 ? static_cast<int>(colorImage.extent.height) : target.height;
    UnityVulkanRenderContext &context = sUnityVulkanRenderContext[cameraID];
    context.renderer = renderer;
    context.cameraID = cameraID;
    context.eventID = event_id;
    context.colorImage = static_cast<uint64_t>(colorImage.image);
    context.colorFormat = static_cast<uint32_t>(colorImage.format);
    context.colorSamples = static_cast<uint32_t>(colorImage.samples);
    context.depthImage = static_cast<uint64_t>(depthImage.image);
    context.depthFormat = static_cast<uint32_t>(depthImage.format);
    context.depthSamples = static_cast<uint32_t>(depthImage.samples);
    context.width = width;
    context.height = height;

    UnityVulkanRecordingState recordingState = {};
    if (!gImmUnityPlugin.UnityAPI.mVulkan->CommandRecordingState(
            &recordingState,
            kUnityVulkanGraphicsQueueAccess_DontCare) ||
        !recordingState.commandBuffer)
    {
        iLog().Printf(
            LT_ERROR,
            L"Unity Vulkan render skipped: Unity did not provide a command buffer for camera=%d",
            cameraID);
        return true;
    }

    static int preparedTargetReportCount = 0;
    if (preparedTargetReportCount < 8)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_PRESENT_TARGET_20260730] camera=%d event=%d colorImage=0x%llx depthImage=0x%llx commandBuffer=%p viewport=%dx%d",
            cameraID,
            event_id,
            static_cast<unsigned long long>(context.colorImage),
            static_cast<unsigned long long>(context.depthImage),
            recordingState.commandBuffer,
            context.width,
            context.height);
        ++preparedTargetReportCount;
    }

    const bool rendered = iRecordUnityVulkanCamera(
        &context,
        recordingState.commandBuffer,
        recordingState.currentFrameNumber,
        recordingState.safeFrameNumber);

    constexpr VkImageLayout kShaderReadLayout = 5; // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    constexpr VkPipelineStageFlags kFragmentShaderStage = 0x00000080; // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    constexpr VkAccessFlags kShaderReadAccess = 0x00000020; // VK_ACCESS_SHADER_READ_BIT
    UnityVulkanImage sampledColorImage = {};
    const bool finalizedForSampling = gImmUnityPlugin.UnityAPI.mVulkan->AccessRenderBufferTexture(
        colorTarget,
        UnityVulkanWholeImage,
        kShaderReadLayout,
        kFragmentShaderStage,
        kShaderReadAccess,
        kUnityVulkanResourceAccess_PipelineBarrier,
        &sampledColorImage);
    if (!finalizedForSampling)
    {
        iLog().Printf(
            LT_ERROR,
            L"[IMM_UNITY_VK_SAMPLE_TRANSITION_20260801] failed camera=%d colorRB=%p",
            cameraID,
            colorTarget);
    }
    static int commandBufferReportCount = 0;
    if (commandBufferReportCount < 8)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_UNITY_COMMAND_BUFFER_20260730] camera=%d event=%d commandBuffer=%p currentFrame=%llu safeFrame=%llu rendered=%d",
            cameraID,
            event_id,
            recordingState.commandBuffer,
            static_cast<unsigned long long>(recordingState.currentFrameNumber),
            static_cast<unsigned long long>(recordingState.safeFrameNumber),
            rendered ? 1 : 0);
        ++commandBufferReportCount;
    }

    static int finalizeReportCount = 0;
    if (finalizeReportCount < 8)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_PRESENT_FINALIZE_20260730] camera=%d colorImage=0x%llx layout=%u transition=explicit-shader-read success=%d",
            cameraID,
            static_cast<unsigned long long>(colorImage.image),
            static_cast<unsigned int>(kShaderReadLayout),
            finalizedForSampling ? 1 : 0);
        ++finalizeReportCount;
    }
    return true;
}

static bool iRenderUnityVulkanCameraInHostRenderPass(int cameraID, int event_id, piRenderer *renderer, UnityRenderBuffer colorTarget)
{
    if (!gImmUnityPlugin.UnityAPI.mVulkan || gImmUnityPlugin.UnityAPI.mRenderer != kUnityGfxRendererVulkan)
    {
        return false;
    }
    const auto &target = gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID];
    if (target.width <= 0 || target.height <= 0)
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan host render skipped: missing render target for camera=%d", cameraID);
        return true;
    }

    UnityVulkanRecordingState recordingState = {};
    const bool hasRecordingState = gImmUnityPlugin.UnityAPI.mVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare);
    if (!hasRecordingState || !recordingState.commandBuffer || recordingState.renderPass == 0 || recordingState.framebuffer == 0)
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan host render skipped: missing recording state for camera=%d hasRecordingState=%d", cameraID, hasRecordingState ? 1 : 0);
        return true;
    }
    if (iEnvFlagEnabled("IMM_UNITY_VK_QUERY_STATE_ONLY"))
    {
        if (!iEnvFlagEnabled("IMM_UNITY_VK_QUERY_STATE_NO_LOG"))
        {
            iLog().Printf(
                LT_MESSAGE,
                L"[IMM_UNITY_VK_QUERY_ONLY_20260612] camera=%d cmd=%p renderPass=0x%llx framebuffer=0x%llx subpass=%d",
                cameraID,
                recordingState.commandBuffer,
                static_cast<unsigned long long>(recordingState.renderPass),
                static_cast<unsigned long long>(recordingState.framebuffer),
                recordingState.subPassIndex);
        }
        if (iEnvFlagEnabled("IMM_UNITY_VK_QUERY_RESTORE_INSIDE"))
        {
            gImmUnityPlugin.UnityAPI.mVulkan->EnsureInsideRenderPass();
            iLog().Printf(LT_MESSAGE, L"[IMM_UNITY_VK_QUERY_RESTORE_20260612] camera=%d", cameraID);
        }
        return true;
    }

    const int width = target.width;
    const int height = target.height;
#if defined(__ANDROID__) || defined(ANDROID)
    const uint32_t colorFormat = iEnvUIntOrDefault("IMM_UNITY_VK_HOST_COLOR_FORMAT", 43u); // Explicit Android RenderTexture reports VK_FORMAT_B8G8R8A8_UNORM.
#else
    const uint32_t colorFormat = iEnvUIntOrDefault("IMM_UNITY_VK_HOST_COLOR_FORMAT", 44u);
#endif
    const uint32_t colorSamples = static_cast<uint32_t>(target.samples > 0 ? target.samples : 1);
#if defined(__ANDROID__) || defined(ANDROID)
    // The managed Android host path explicitly binds CameraTarget with Unity's
    // depth target before this EnsureInside event. Display render-buffer
    // pointers are opaque placeholders on Android and are not evidence of
    // whether the active render pass contains depth.
    const bool assumeHostDepth = true;
#else
    const bool assumeHostDepth = iEnvFlagEnabled("IMM_UNITY_VK_ASSUME_HOST_DEPTH");
#endif
    // Display render-buffer pointers can be null while Unity's active Vulkan render pass still owns depth.
    const bool hostRenderPassHasDepth = iEnvFlagEnabled("IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH") || assumeHostDepth;
    const bool hasDepthAttachment = target.depth != nullptr || hostRenderPassHasDepth;
#if defined(__ANDROID__) || defined(ANDROID)
    // Unity exposes the active render pass, but not the Android depth image's
    // format or contents through CommandRecordingState. Keep the attachment
    // bound for render-pass compatibility while this control run verifies that
    // treating its opaque contents as reverse-Z is what rejects every IMM draw.
    const bool useHostDepth = false;
#else
    const bool useHostDepth = hasDepthAttachment && iEnvFlagEnabled("IMM_UNITY_VK_USE_HOST_DEPTH");
#endif
    if (sUnityVulkanRenderTargetDiagnosticCount < 24)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_HOST_RT_20260612] camera=%d colorRB=%p colorFormat=%u colorSamples=%u depthAttachment=%d hostDepth=%d hostRenderPassHasDepth=%d assumeHostDepth=%d colorExtent=%dx%d cmd=%p renderPass=0x%llx framebuffer=0x%llx subpass=%d accessRenderBuffer=0",
            cameraID,
            colorTarget,
            static_cast<unsigned int>(colorFormat),
            static_cast<unsigned int>(colorSamples),
            hasDepthAttachment ? 1 : 0,
            useHostDepth ? 1 : 0,
            hostRenderPassHasDepth ? 1 : 0,
            assumeHostDepth ? 1 : 0,
            width,
            height,
            recordingState.commandBuffer,
            static_cast<unsigned long long>(recordingState.renderPass),
            static_cast<unsigned long long>(recordingState.framebuffer),
            recordingState.subPassIndex);
        ++sUnityVulkanRenderTargetDiagnosticCount;
    }
#if defined(__ANDROID__) || defined(ANDROID)
    if (sUnityVulkanRenderTargetDiagnosticCount == 1)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_NO_HOST_DEPTH_CONTROL_20260731] hasDepthAttachment=%d useHostDepth=0",
            hasDepthAttachment ? 1 : 0);
    }
#endif

    piRendererVulkan *vulkanRenderer = static_cast<piRendererVulkan *>(renderer);
    vulkanRenderer->SetHostDrawBisectionEnabled(sUnityVulkanHostDrawBisectionEnabled.load());
    if (!vulkanRenderer->BeginHostRenderPassFrame(
            recordingState.commandBuffer,
            recordingState.currentFrameNumber,
            recordingState.safeFrameNumber,
            reinterpret_cast<void *>(static_cast<uintptr_t>(recordingState.renderPass)),
            reinterpret_cast<void *>(static_cast<uintptr_t>(recordingState.framebuffer)),
            colorFormat,
            colorSamples,
            hasDepthAttachment,
            useHostDepth,
            useHostDepth,
            recordingState.subPassIndex >= 0 ? static_cast<uint32_t>(recordingState.subPassIndex) : 0u,
            width,
            height))
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan host render skipped: failed to begin host render pass frame for camera=%d", cameraID);
        return true;
    }

    const bool debugClearHost = iEnvFlagEnabled("IMM_UNITY_VK_DEBUG_HOST_CLEAR") || iEnvFlagEnabled("IMM_UNITY_VK_DEBUG_HOST_CLEAR_ONLY");
    if (debugClearHost)
    {
        const bool cleared = vulkanRenderer->DebugClearHostRenderPassColor(1.0f, 0.0f, 1.0f, 1.0f);
        iLog().Printf(LT_MESSAGE, L"[IMM_UNITY_VK_HOST_CLEAR_20260612] camera=%d cleared=%d", cameraID, cleared ? 1 : 0);
    }

    const ImmShared::ImmEngineBridge::ViewportInfo viewport = {
        0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f, true
    };
    const int eyeID = event_id & 1;
    const bool beginEndOnly = iEnvFlagEnabled("IMM_UNITY_VK_BEGIN_END_ONLY");
    const bool debugClearOnly = iEnvFlagEnabled("IMM_UNITY_VK_DEBUG_HOST_CLEAR_ONLY");
    const bool debugTriangleOnly = iEnvFlagEnabled("IMM_UNITY_VK_DEBUG_HOST_TRIANGLE_ONLY");
    const bool drawBisectionEnabled = sUnityVulkanHostDrawBisectionEnabled.load();
    const uint32_t drawBisectionInvocation = drawBisectionEnabled
        ? sUnityVulkanHostDrawBisectionInvocation.fetch_add(1) + 1
        : 0;
    const bool noBridgeControl = drawBisectionEnabled && drawBisectionInvocation == 2;
    const bool rendered = debugTriangleOnly
        ? vulkanRenderer->DebugDrawHostRenderPassTriangle()
        : ((beginEndOnly || debugClearOnly || noBridgeControl)
            ? false
            : gImmUnityPlugin.mBridge.RenderPreparedCamera(cameraID, viewport, eyeID, false));
    vulkanRenderer->EndExternalImageFrame();

    static int triangleControlReportCount = 0;
    if (debugTriangleOnly && triangleControlReportCount < 8)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_TRIANGLE_CONTROL_20260731] camera=%d rendered=%d cmd=%p renderPass=0x%llx subpass=%d samples=%u",
            cameraID,
            rendered ? 1 : 0,
            recordingState.commandBuffer,
            static_cast<unsigned long long>(recordingState.renderPass),
            recordingState.subPassIndex,
            static_cast<unsigned int>(colorSamples));
        ++triangleControlReportCount;
    }

    if (drawBisectionEnabled && drawBisectionInvocation <= 8)
    {
        iLog().Printf(
            LT_MESSAGE,
            L"[IMM_UNITY_VK_DRAW_BISECT_20260731] invocation=%u phase=%ls rendered=%d",
            drawBisectionInvocation,
            noBridgeControl ? L"no-bridge-control" : L"bridge",
            rendered ? 1 : 0);
    }

    const Player::PerformanceInfo &perf = iPlayer().GetPerformanceInfoForFrame();
    iLog().Printf(
        LT_MESSAGE,
        L"Unity Vulkan host render: camera=%d viewport=%dx%d rendered=%d drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture360DrawCalls=%d beginEndOnly=%d",
        cameraID,
        width,
        height,
        rendered ? 1 : 0,
        perf.numDrawCalls,
        perf.numPaintDrawCalls,
        perf.numPictureDrawCalls,
        perf.numPicture360DrawCalls,
        beginEndOnly ? 1 : 0);
    return true;
}

static bool iPrepareUnityVulkanCamera(int cameraID)
{
    if (!gImmUnityPlugin.UnityAPI.mVulkan || gImmUnityPlugin.UnityAPI.mRenderer != kUnityGfxRendererVulkan)
    {
        return false;
    }
    if (cameraID < 0 || cameraID >= ImmShared::ImmEngineBridge::kMaxCameras)
    {
        iLog().Printf(LT_ERROR, L"Unity Vulkan prepare skipped: invalid camera=%d", cameraID);
        return false;
    }

    UnityVulkanRecordingState recordingState = {};
    if (!gImmUnityPlugin.UnityAPI.mVulkan->CommandRecordingState(
            &recordingState,
            kUnityVulkanGraphicsQueueAccess_DontCare) ||
        !recordingState.commandBuffer)
    {
        iLog().Printf(
            LT_ERROR,
            L"Unity Vulkan prepare skipped: Unity did not provide an upload command buffer for camera=%d",
            cameraID);
        return false;
    }

    piRenderer *renderer = gImmUnityPlugin.mBridge.GetRenderer();
    if (!renderer || renderer->GetAPI() != piRenderer::API::Vulkan)
    {
        return false;
    }
    piRendererVulkan *vulkanRenderer = static_cast<piRendererVulkan *>(renderer);
    if (!vulkanRenderer->BeginUnityCommandBufferUploadFrame(
            recordingState.commandBuffer,
            recordingState.currentFrameNumber,
            recordingState.safeFrameNumber))
    {
        iLog().Printf(
            LT_ERROR,
            L"Unity Vulkan prepare skipped: failed to begin Unity upload command buffer for camera=%d",
            cameraID);
        return false;
    }

    const bool prepared = gImmUnityPlugin.mBridge.PrepareCamera(cameraID);
    bool resourcesPrepared = false;
    const auto &target = gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID];
    if (prepared && target.width > 0 && target.height > 0)
    {
        // GlobalRender prepares the player frame but the layer renderers perform
        // lazy texture uploads during their first DisplayRender traversal. Run
        // that traversal while Unity has explicitly placed this event outside
        // its render pass. With no IMM render target bound the draw submission
        // paths are inert, while texture barriers/copies are recorded into the
        // borrowed Unity upload command buffer.
        const ImmShared::ImmEngineBridge::ViewportInfo uploadViewport = {
            0.0f,
            0.0f,
            static_cast<float>(target.width),
            static_cast<float>(target.height),
            0.0f,
            1.0f,
            true
        };
        resourcesPrepared = gImmUnityPlugin.mBridge.RenderPreparedCamera(
            cameraID,
            uploadViewport,
            0,
            false);
    }
    vulkanRenderer->EndExternalImageFrame();
    iLog().Printf(
        LT_MESSAGE,
        L"[IMM_UNITY_VK_PREWARM_20260731] camera=%d prepared=%d resourcesPrepared=%d target=%dx%d cmd=%p currentFrame=%llu safeFrame=%llu",
        cameraID,
        prepared ? 1 : 0,
        resourcesPrepared ? 1 : 0,
        target.width,
        target.height,
        recordingState.commandBuffer,
        recordingState.currentFrameNumber,
        recordingState.safeFrameNumber);
    return true;
}

static void UNITY_INTERFACE_API iOnRenderEventAndData(int event_id, void *data)
{
    IMM_UNITY_NATIVE_LOCK();
    piRenderer *renderer = gImmUnityPlugin.mBridge.GetRenderer();
    if (!renderer)
    {
        return;
    }

    if (gImmUnityPlugin.UnityAPI.mRenderer == kUnityGfxRendererVulkan && data)
    {
        UnityRenderingExtCustomBlitParamsMinimal *params = static_cast<UnityRenderingExtCustomBlitParamsMinimal *>(data);
        const int vulkanEventId = static_cast<int>(params->command);
        const int cameraID = (vulkanEventId >> 8) & 0xff;
        if (iEnvFlagEnabled("IMM_UNITY_VK_SKIP_HOST_RENDER"))
        {
            if (sUnityVulkanCustomBlitDiagnosticCount < 24)
            {
                iLog().Printf(
                    LT_MESSAGE,
                    L"[IMM_UNITY_VK_SKIP_HOST_20260611] skipped Vulkan custom blit event=%d command=%u camera=%d source=%p destination=%p",
                    event_id,
                    params->command,
                    cameraID,
                    reinterpret_cast<void *>(params->source),
                    params->destination);
                ++sUnityVulkanCustomBlitDiagnosticCount;
            }
            return;
        }
        UnityVulkanRecordingState recordingState = {};
        const bool hasRecordingState = gImmUnityPlugin.UnityAPI.mVulkan &&
            gImmUnityPlugin.UnityAPI.mVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare);
        if (sUnityVulkanCustomBlitDiagnosticCount < 24)
        {
            iLog().Printf(
                LT_MESSAGE,
                L"[IMM_UNITY_VK_BLIT_20260610] event=%d command=%u camera=%d source=%p destination=%p commandParam=%u commandFlags=%u hasRecordingState=%d cmd=%p renderPass=0x%llx framebuffer=0x%llx subpass=%d frame=%llu",
                event_id,
                params->command,
                cameraID,
                reinterpret_cast<void *>(params->source),
                params->destination,
                params->commandParam,
                params->commandFlags,
                hasRecordingState ? 1 : 0,
                hasRecordingState ? recordingState.commandBuffer : nullptr,
                hasRecordingState ? static_cast<unsigned long long>(recordingState.renderPass) : 0ULL,
                hasRecordingState ? static_cast<unsigned long long>(recordingState.framebuffer) : 0ULL,
                hasRecordingState ? recordingState.subPassIndex : -1,
                hasRecordingState ? recordingState.currentFrameNumber : 0ULL);
            ++sUnityVulkanCustomBlitDiagnosticCount;
        }
        if (cameraID >= 0 && cameraID < 256)
        {
            const auto &target = gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID];
            UnityRenderBuffer colorTarget = params->destination ? params->destination : target.color;
            if (hasRecordingState)
            {
                iRenderUnityVulkanCameraInHostRenderPass(cameraID, vulkanEventId, renderer, colorTarget);
            }
            else
            {
                iRenderUnityVulkanCamera(cameraID, vulkanEventId, renderer, colorTarget);
            }
        }
        return;
    }

    iOnRenderEvent(event_id);
}
#endif

static void UNITY_INTERFACE_API iOnRenderEvent(int event_id)
{
#if defined(__ANDROID__) || defined(ANDROID)
	if (sRenderEventCount < 5) {
		__android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "iOnRenderEvent called, event_id=%d", event_id);
		sRenderEventCount++;
	}

	// Complete deferred initialization on first render event (we now have GL context)
	if (sAndroidDeferredInit.needsInit && !sAndroidDeferredInit.isInitialized) {
		if (!AndroidCompleteInit()) {
			__android_log_print(ANDROID_LOG_ERROR, "ImmUnityPlugin", "Deferred init failed - rendering disabled");
			sAndroidDeferredInit.needsInit = false; // Don't keep retrying
			return;
		}
	}

	// Skip rendering if not initialized
	if (!sAndroidDeferredInit.isInitialized) {
		return;
	}
#endif
    IMM_UNITY_NATIVE_LOCK();
	int numVp = 1;
	float oldVp[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	piRenderer *renderer = gImmUnityPlugin.mBridge.GetRenderer();
	if (renderer == nullptr)
		return;

//const int eventType = (event_id >> 0) & 0xff;
	const int cameraID  = (event_id >> 8) & 0xff;
    const int unityVulkanCameraID = (event_id == 1 && iEnvFlagEnabled("IMM_UNITY_VK_SAMPLE_EVENT1")) ? 1 : cameraID;
	if (cameraID < 0 || cameraID >= 256)
	{
#if defined(__ANDROID__) || defined(ANDROID)
		__android_log_print(ANDROID_LOG_ERROR, "ImmUnityPlugin", "Invalid cameraID: %d", cameraID);
#else
		iLog().Printf(LT_ERROR, L"Invalid cameraID: %d", cameraID);
#endif
		return;
	}

#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    if (gImmUnityPlugin.UnityAPI.mRenderer == kUnityGfxRendererVulkan)
    {
        if ((event_id & kUnityVulkanPrepareEventFlag) != 0)
        {
            iPrepareUnityVulkanCamera(unityVulkanCameraID);
            return;
        }
        if (iEnvFlagEnabled("IMM_UNITY_VK_SKIP_HOST_RENDER"))
        {
            if (sUnityVulkanRenderTargetDiagnosticCount < 24)
            {
                iLog().Printf(LT_MESSAGE, L"[IMM_UNITY_VK_SKIP_HOST_20260611] skipped Vulkan host render event=%d camera=%d", event_id, unityVulkanCameraID);
                ++sUnityVulkanRenderTargetDiagnosticCount;
            }
            return;
        }
        const auto &target = gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[unityVulkanCameraID];
#if defined(__ANDROID__) || defined(ANDROID)
        if (!iEnvFlagEnabled("IMM_UNITY_VK_USE_HOST_RENDER_PASS"))
        {
            iRenderUnityVulkanCamera(unityVulkanCameraID, event_id, renderer, target.color);
            return;
        }
        iRenderUnityVulkanCameraInHostRenderPass(unityVulkanCameraID, event_id, renderer, target.color);
#else
        if (iEnvFlagEnabled("IMM_UNITY_VK_FORCE_EXTERNAL_IMAGE"))
        {
            iRenderUnityVulkanCamera(unityVulkanCameraID, event_id, renderer, target.color);
            return;
        }
        iRenderUnityVulkanCameraInHostRenderPass(unityVulkanCameraID, event_id, renderer, target.color);
#endif
        return;
    }
#endif

#if defined(__APPLE__)
    const bool isUnityMetal = (gImmUnityPlugin.UnityAPI.mRenderer == kUnityGfxRendererMetal);
    void *unityMetalCommandBuffer = nullptr;
    void *unityMetalCommandEncoder = nullptr;
    void *unityMetalRenderPassDescriptor = nullptr;
    bool unityMetalFrameBegun = false;
    const bool useUnityMetalOffscreen = isUnityMetal && iEnvFlagEnabled("IMM_UNITY_METAL_OFFSCREEN");
    // ImmEngineBridge owns both the global and camera render phases. Begin the
    // external frame before entering the bridge so all GPU work is encoded in
    // Unity's active command buffer.
    const bool deferUnityMetalFrameBegin = false;
    const bool useUnityMetalPluginCommandBuffer = isUnityMetal && gImmUnityPlugin.UnityAPI.mMetalV2 && iEnvFlagEnabled("IMM_UNITY_METAL_USE_PLUGIN_COMMAND_BUFFER");
    const bool useUnityMetalOwnedEncoder = isUnityMetal && !useUnityMetalPluginCommandBuffer && iEnvFlagEnabled("IMM_UNITY_METAL_USE_OWNED_ENCODER");
    if (isUnityMetal)
    {
        if (sUnityMetalRenderBoundaryReportCount < 16)
        {
            std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE camera=%d v1=%d v2=%d pluginCB=%d owned=%d\n",
                cameraID,
                gImmUnityPlugin.UnityAPI.mMetal ? 1 : 0,
                gImmUnityPlugin.UnityAPI.mMetalV2 ? 1 : 0,
                useUnityMetalPluginCommandBuffer ? 1 : 0,
                useUnityMetalOwnedEncoder ? 1 : 0);
            std::fflush(stderr);
        }
        if (!renderer)
        {
            std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN missing-interface-or-renderer\n");
            std::fflush(stderr);
            return;
        }
        if (iEnvFlagEnabled("IMM_UNITY_METAL_NOOP_EVENT"))
        {
            if (sUnityMetalRenderBoundaryReportCount < 16)
            {
                iLog().Printf(LT_MESSAGE, L"Unity Metal render boundary: noop event camera=%d", cameraID);
                ++sUnityMetalRenderBoundaryReportCount;
            }
            return;
        }

        const int viewportWidth = gImmUnityPlugin.mMetalCameraViewport[cameraID].mViewportWidth;
        const int viewportHeight = gImmUnityPlugin.mMetalCameraViewport[cameraID].mViewportHeight;
        if (useUnityMetalOffscreen)
        {
            if (!iEnsureUnityMetalOffscreenTarget(renderer, viewportWidth, viewportHeight) ||
                !renderer->SetRenderTarget(sUnityMetalOffscreenTarget))
            {
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN offscreen-target-failed\n");
                std::fflush(stderr);
                return;
            }
            const int vp[4] = { 0, 0, viewportWidth, viewportHeight };
            renderer->SetViewport(0, vp);
            const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            renderer->Clear(clear, nullptr, nullptr, nullptr, true);
            oldVp[2] = static_cast<float>((viewportWidth > 0) ? viewportWidth : 1);
            oldVp[3] = static_cast<float>((viewportHeight > 0) ? viewportHeight : 1);
        }
        else
        {
            if ((!gImmUnityPlugin.UnityAPI.mMetal && !gImmUnityPlugin.UnityAPI.mMetalV2))
            {
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN missing-interface\n");
                std::fflush(stderr);
                return;
            }

            if ((!useUnityMetalPluginCommandBuffer && (!gImmUnityPlugin.UnityAPI.mMetal || !gImmUnityPlugin.UnityAPI.mMetal->CurrentCommandBuffer)) ||
                (!useUnityMetalOwnedEncoder && !useUnityMetalPluginCommandBuffer && !gImmUnityPlugin.UnityAPI.mMetal->CurrentCommandEncoder) ||
                (useUnityMetalOwnedEncoder && (!gImmUnityPlugin.UnityAPI.mMetal->EndCurrentCommandEncoder || !gImmUnityPlugin.UnityAPI.mMetal->CurrentRenderPassDescriptor)) ||
                (useUnityMetalPluginCommandBuffer && (!gImmUnityPlugin.UnityAPI.mMetalV2->EndCurrentCommandEncoder || !gImmUnityPlugin.UnityAPI.mMetalV2->CurrentRenderPassDescriptor)) ||
                (useUnityMetalPluginCommandBuffer && (!gImmUnityPlugin.UnityAPI.mMetalV2->CommitCurrentCommandBuffer || !gImmUnityPlugin.UnityAPI.mMetalV2->CommandQueue)))
            {
                iLog().Printf(LT_ERROR, L"Unity Metal interface is missing required command-buffer/encoder functions");
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN missing-functions\n");
                std::fflush(stderr);
                return;
            }

            if (!useUnityMetalPluginCommandBuffer)
            {
                unityMetalCommandBuffer = gImmUnityPlugin.UnityAPI.mMetal->CurrentCommandBuffer();
            }
            void *unityMetalCommandQueue = nullptr;
            if (useUnityMetalPluginCommandBuffer)
            {
                unityMetalRenderPassDescriptor = gImmUnityPlugin.UnityAPI.mMetalV2->CurrentRenderPassDescriptor();
                gImmUnityPlugin.UnityAPI.mMetalV2->EndCurrentCommandEncoder();
                gImmUnityPlugin.UnityAPI.mMetalV2->CommitCurrentCommandBuffer();
                unityMetalCommandQueue = gImmUnityPlugin.UnityAPI.mMetalV2->CommandQueue();
            }
            else if (useUnityMetalOwnedEncoder)
            {
                unityMetalRenderPassDescriptor = gImmUnityPlugin.UnityAPI.mMetal->CurrentRenderPassDescriptor();
                gImmUnityPlugin.UnityAPI.mMetal->EndCurrentCommandEncoder();
            }
            else
            {
                unityMetalCommandEncoder = gImmUnityPlugin.UnityAPI.mMetal->CurrentCommandEncoder();
                unityMetalRenderPassDescriptor = gImmUnityPlugin.UnityAPI.mMetal->CurrentRenderPassDescriptor();
            }
            if ((!useUnityMetalPluginCommandBuffer && !unityMetalCommandBuffer) ||
                (!useUnityMetalOwnedEncoder && !useUnityMetalPluginCommandBuffer && !unityMetalCommandEncoder) ||
                (useUnityMetalOwnedEncoder && !unityMetalRenderPassDescriptor) ||
                (useUnityMetalPluginCommandBuffer && (!unityMetalRenderPassDescriptor || !unityMetalCommandQueue)))
            {
                iLog().Printf(LT_ERROR, L"Unity Metal render event did not provide the required command buffer, encoder, or render pass descriptor");
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN missing-objects commandBuffer=%p commandEncoder=%p descriptor=%p queue=%p\n",
                    unityMetalCommandBuffer,
                    unityMetalCommandEncoder,
                    unityMetalRenderPassDescriptor,
                    unityMetalCommandQueue);
                std::fflush(stderr);
                return;
            }

            if (!deferUnityMetalFrameBegin && useUnityMetalPluginCommandBuffer &&
                !static_cast<piRendererMetal *>(renderer)->BeginExternalCommandQueueRenderPassFrame(unityMetalCommandQueue, unityMetalRenderPassDescriptor, viewportWidth, viewportHeight))
            {
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN begin-plugin-command-buffer-failed\n");
                std::fflush(stderr);
                return;
            }
            if (!deferUnityMetalFrameBegin && useUnityMetalOwnedEncoder &&
                !static_cast<piRendererMetal *>(renderer)->BeginExternalRenderPassFrame(unityMetalCommandBuffer, unityMetalRenderPassDescriptor, viewportWidth, viewportHeight))
            {
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN begin-owned-encoder-failed\n");
                std::fflush(stderr);
                return;
            }
            if (!deferUnityMetalFrameBegin && !useUnityMetalPluginCommandBuffer && !useUnityMetalOwnedEncoder &&
                !static_cast<piRendererMetal *>(renderer)->BeginExternalCommandEncoderFrame(unityMetalCommandBuffer, unityMetalCommandEncoder, unityMetalRenderPassDescriptor, viewportWidth, viewportHeight))
            {
                std::fprintf(stderr, "IMM_UNITY_METAL_EVENT_NATIVE_RETURN begin-current-encoder-failed\n");
                std::fflush(stderr);
                return;
            }
            unityMetalFrameBegun = !deferUnityMetalFrameBegin;
            if (sUnityMetalRenderBoundaryReportCount < 16)
            {
                iLog().Printf(
                    LT_MESSAGE,
                    deferUnityMetalFrameBegin ? L"Unity Metal render boundary: defer begin camera=%d viewport=%dx%d" : L"Unity Metal render boundary: begin camera=%d viewport=%dx%d",
                    cameraID,
                    viewportWidth,
                    viewportHeight);
            }
            if (!deferUnityMetalFrameBegin && iEnvFlagEnabled("IMM_UNITY_METAL_SKIP_DRAW"))
            {
                if (sUnityMetalRenderBoundaryReportCount < 16)
                {
                    iLog().Printf(LT_MESSAGE, L"Unity Metal render boundary: skip draw camera=%d", cameraID);
                }
                renderer->SwapBuffers();
                if (sUnityMetalRenderBoundaryReportCount < 16)
                {
                    iLog().Printf(LT_MESSAGE, L"Unity Metal render boundary: skip draw end camera=%d", cameraID);
                    ++sUnityMetalRenderBoundaryReportCount;
                }
                return;
            }
            oldVp[2] = static_cast<float>((viewportWidth > 0) ? viewportWidth : 1);
            oldVp[3] = static_cast<float>((viewportHeight > 0) ? viewportHeight : 1);
        }
    }
    else
#endif
    {
        renderer->GetViewports(&numVp, oldVp);
    }

#if defined(__ANDROID__) || defined(ANDROID)
	GLint glViewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, glViewport);
	oldVp[0] = static_cast<float>(glViewport[0]);
	oldVp[1] = static_cast<float>(glViewport[1]);
	oldVp[2] = static_cast<float>(glViewport[2]);
	oldVp[3] = static_cast<float>(glViewport[3]);
#endif

	if (numVp < 1) return;

	const ivec2 res = ivec2(int(oldVp[2]), int(oldVp[3]));
	if (res.x <= 0 || res.y <= 0)
	{
#if defined(__ANDROID__) || defined(ANDROID)
		__android_log_print(ANDROID_LOG_WARN, "ImmUnityPlugin", "[IMMDBG_RENDER_20260211A] Skip render due to invalid viewport %dx%d", res.x, res.y);
#endif
#if defined(__APPLE__)
        if (isUnityMetal)
        {
            renderer->SwapBuffers();
        }
#endif
		return;
	}

#if defined(__ANDROID__) || defined(ANDROID)
	glDisable(GL_SCISSOR_TEST);
#endif

    const ImmShared::ImmEngineBridge::ViewportInfo viewport = {
        oldVp[0], oldVp[1], oldVp[2], oldVp[3], oldVp[4], oldVp[5],
        true
	    };
    const int eyeID = event_id & 1;
    const bool rendered = gImmUnityPlugin.mBridge.RenderCamera(cameraID, viewport, eyeID, true);

#if defined(__APPLE__)
    if (isUnityMetal && sUnityMetalRenderReportCount < 8)
    {
        const Player::PerformanceInfo &perf = iPlayer().GetPerformanceInfoForFrame();
        if (perf.numDrawCalls > 0 || sUnityMetalRenderReportCount == 0)
        {
            const ImmShared::ImmEngineBridge::CameraState *cameraState = gImmUnityPlugin.mBridge.GetCameraState(cameraID);
            const int stereoType = (cameraState != nullptr) ? cameraState->stereoType : 0;
            iLog().Printf(
                LT_MESSAGE,
                L"Unity Metal render: camera=%d stereo=%d viewport=%dx%d drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture360DrawCalls=%d triangles=%d",
                cameraID,
                stereoType,
                res.x,
                res.y,
                perf.numDrawCalls,
                perf.numPaintDrawCalls,
                perf.numPictureDrawCalls,
                perf.numPicture360DrawCalls,
                perf.numTriangles);
            std::fprintf(
                stderr,
                "IMM_UNITY_METAL_RENDER camera=%d stereo=%d viewport=%dx%d drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture360DrawCalls=%d triangles=%d\n",
                cameraID,
                stereoType,
                res.x,
                res.y,
                perf.numDrawCalls,
                perf.numPaintDrawCalls,
                perf.numPictureDrawCalls,
                perf.numPicture360DrawCalls,
                perf.numTriangles);
            ++sUnityMetalRenderReportCount;
        }
    }

    if (isUnityMetal)
    {
        if (sUnityMetalRenderBoundaryReportCount < 16)
        {
            iLog().Printf(LT_MESSAGE, L"Unity Metal render boundary: before-swap camera=%d", cameraID);
        }
        renderer->SwapBuffers();
        if (sUnityMetalRenderBoundaryReportCount < 16)
        {
            iLog().Printf(LT_MESSAGE, L"Unity Metal render boundary: end camera=%d rendered=%d begun=%d", cameraID, rendered ? 1 : 0, unityMetalFrameBegun ? 1 : 0);
            ++sUnityMetalRenderBoundaryReportCount;
        }
    }
#else
    (void)rendered;
#endif
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// PLUGIN INTERFACE
// ----------------------------------------------------------------------------------------------------------------------------------------------------


extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	gImmUnityPlugin.UnityAPI.mUnityInterfaces = unityInterfaces;
	gImmUnityPlugin.UnityAPI.mGraphics = gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphics>();
#if defined(__APPLE__)
    std::fprintf(
        stderr,
        "IMM_UNITY_METAL_PLUGIN_LOAD interfaces=%p graphics=%p\n",
        static_cast<void *>(unityInterfaces),
        static_cast<void *>(gImmUnityPlugin.UnityAPI.mGraphics));
    std::fflush(stderr);
#endif
	gImmUnityPlugin.UnityAPI.mGraphics->RegisterDeviceEventCallback(iOnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	iOnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	gImmUnityPlugin.UnityAPI.mGraphics->UnregisterDeviceEventCallback(iOnGraphicsDeviceEvent);
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return iOnRenderEvent;
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventAndDataFunc()
{
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    return iOnRenderEventAndData;
#else
    return nullptr;
#endif
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ConfigureVulkanRenderEvent(int eventID)
{
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    if (!gImmUnityPlugin.UnityAPI.mVulkan || gImmUnityPlugin.UnityAPI.mRenderer != kUnityGfxRendererVulkan)
    {
        return 0;
    }
    iConfigureUnityVulkanEvent(eventID, true);
    return 1;
#else
    (void)eventID;
    return 0;
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetVulkanHostDrawBisectionEnabled(int enabled)
{
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    sUnityVulkanHostDrawBisectionEnabled.store(enabled != 0);
    if (enabled == 0)
    {
        sUnityVulkanHostDrawBisectionInvocation.store(0);
    }
    iLog().Printf(
        LT_MESSAGE,
        L"[IMM_UNITY_VK_DRAW_BISECT_20260731] native-request enabled=%d",
        enabled != 0 ? 1 : 0);
#else
    (void)enabled;
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT Debug()
{
    if (!gImmUnityPlugin.mBridge.IsInitialized())
        return;

	if(gImmUnityPlugin.mBridge.GetRenderer() == nullptr)
        gImmUnityPlugin.mBridge.GetLog()->Printf(LT_DEBUG, L"Renderer has not initialized");
    else
        gImmUnityPlugin.mBridge.GetLog()->Printf(LT_DEBUG, L"Renderer has initialized");

}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API IsReadyForDocumentLoad()
{
#if defined(__ANDROID__) || defined(ANDROID)
    if (gImmUnityPlugin.UnityAPI.mRenderer == kUnityGfxRendererVulkan)
        return gImmUnityPlugin.mBridge.IsInitialized() ? 1 : 0;
    return (gImmUnityPlugin.mBridge.IsInitialized() && sAndroidDeferredInit.isInitialized) ? 1 : 0;
#else
    return gImmUnityPlugin.mBridge.IsInitialized() ? 1 : 0;
#endif
}

static mat4x4 iUnityToPilibs(const float *m)
{
	return mat4x4(m[0], m[4], m[ 8], m[12],
		          m[1], m[5], m[ 9], m[13],
		          m[2], m[6], m[10], m[14],
		          m[3], m[7], m[11], m[15]);
}

static trans3d iUnityToTrans3d(const float *m)
{
    return fromMatrix(f2d(iUnityToPilibs(m)) * mat4x4d::flipZ());
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GlobalWork(int enabled)
{
    gImmUnityPlugin.mBridge.GlobalWork(enabled == 1, 9000);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PrepareCamera(int cameraID)
{
    if (cameraID < 0 || cameraID > 255)
        return 0;

    IMM_UNITY_NATIVE_LOCK();
    return gImmUnityPlugin.mBridge.PrepareCamera(cameraID) ? 1 : 0;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetMatrices( int cameraID, int stereoType,
	                                                                    float *world2head, float *prjHead,
	                                                                    float *world2leye, float *prjLeft,
	                                                                    float *world2reye, float *prjRight )
{
	if (cameraID > 255)return;

    const mat4x4 head = (world2head != nullptr) ? iUnityToPilibs(world2head) : mat4x4();
    const mat4x4 left = (world2leye != nullptr) ? iUnityToPilibs(world2leye) : mat4x4();
    const mat4x4 right = (world2reye != nullptr) ? iUnityToPilibs(world2reye) : mat4x4();
    const mat4x4 headPrj = (prjHead != nullptr) ? iUnityToPilibs(prjHead) : mat4x4();
    const mat4x4 leftPrj = (prjLeft != nullptr) ? iUnityToPilibs(prjLeft) : mat4x4();
    const mat4x4 rightPrj = (prjRight != nullptr) ? iUnityToPilibs(prjRight) : mat4x4();

    gImmUnityPlugin.mBridge.SetCameraMatrices(cameraID,
                                              stereoType,
                                              (world2head != nullptr) ? &head : nullptr,
                                              (prjHead != nullptr) ? &headPrj : nullptr,
                                              (world2leye != nullptr) ? &left : nullptr,
                                              (prjLeft != nullptr) ? &leftPrj : nullptr,
                                              (world2reye != nullptr) ? &right : nullptr,
	                                              (prjRight != nullptr) ? &rightPrj : nullptr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetCameraViewport(int cameraID, int width, int height)
{
    if (cameraID < 0 || cameraID > 255) return;
#if defined(__APPLE__)
    IMM_UNITY_NATIVE_LOCK();
    gImmUnityPlugin.mMetalCameraViewport[cameraID].mViewportWidth = width;
    gImmUnityPlugin.mMetalCameraViewport[cameraID].mViewportHeight = height;
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetVulkanCameraRenderBuffers(int cameraID, void *colorRenderBuffer, void *depthRenderBuffer, int width, int height, int samples)
{
    if (cameraID < 0 || cameraID > 255) return;
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    IMM_UNITY_NATIVE_LOCK();
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].color = static_cast<UnityRenderBuffer>(colorRenderBuffer);
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].colorTexture = nullptr;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].depth = static_cast<UnityRenderBuffer>(depthRenderBuffer);
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].width = width;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].height = height;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].samples = samples > 0 ? samples : 1;
#else
    (void)colorRenderBuffer;
    (void)depthRenderBuffer;
    (void)width;
    (void)height;
    (void)samples;
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetVulkanCameraTexture(int cameraID, void *colorTexture, void *depthRenderBuffer, int width, int height, int samples)
{
    if (cameraID < 0 || cameraID > 255) return;
#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    IMM_UNITY_NATIVE_LOCK();
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].color = nullptr;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].colorTexture = colorTexture;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].depth = static_cast<UnityRenderBuffer>(depthRenderBuffer);
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].width = width;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].height = height;
    gImmUnityPlugin.UnityAPI.mVulkanCameraTarget[cameraID].samples = samples > 0 ? samples : 1;
#else
    (void)colorTexture;
    (void)depthRenderBuffer;
    (void)width;
    (void)height;
    (void)samples;
#endif
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Init( int colorSpace, // 0=linear 1=gamma
	                                        int antialiasing, // 8
											char *logFileName,
											char *tmpFolferName)
{
    ImmShared::ImmEngineBridge::InitConfig config = {};
    config.colorSpace = colorSpace;
    config.antialiasing = antialiasing;
    config.logFileName = logFileName;
    config.tmpFolderName = tmpFolferName;

#if defined(WINDOWS) || defined(__ANDROID__) || defined(ANDROID)
    if (gImmUnityPlugin.UnityAPI.mRenderer == kUnityGfxRendererVulkan)
    {
        if (!gImmUnityPlugin.UnityAPI.mVulkan ||
            !gImmUnityPlugin.UnityAPI.mVulkanInstance.instance ||
            !gImmUnityPlugin.UnityAPI.mVulkanInstance.physicalDevice ||
            !gImmUnityPlugin.UnityAPI.mVulkanInstance.device ||
            !gImmUnityPlugin.UnityAPI.mVulkanInstance.graphicsQueue)
        {
#if defined(__ANDROID__) || defined(ANDROID)
            __android_log_print(ANDROID_LOG_ERROR, "ImmUnityPlugin", "Unity Vulkan interface is unavailable in ImmUnityPlugin");
#else
            std::fprintf(stderr, "Unity Vulkan interface is unavailable in ImmUnityPlugin.\n");
            std::fflush(stderr);
#endif
            return -1;
        }
        static piVulkanExternalDevice unityVulkanDevice = {};
        unityVulkanDevice.instance = gImmUnityPlugin.UnityAPI.mVulkanInstance.instance;
        unityVulkanDevice.physicalDevice = gImmUnityPlugin.UnityAPI.mVulkanInstance.physicalDevice;
        unityVulkanDevice.device = gImmUnityPlugin.UnityAPI.mVulkanInstance.device;
        unityVulkanDevice.graphicsQueue = gImmUnityPlugin.UnityAPI.mVulkanInstance.graphicsQueue;
        unityVulkanDevice.getInstanceProcAddr = reinterpret_cast<void *>(
            gImmUnityPlugin.UnityAPI.mVulkanInstance.getInstanceProcAddr);
        unityVulkanDevice.graphicsQueueFamilyIndex = gImmUnityPlugin.UnityAPI.mVulkanInstance.queueFamilyIndex;
        config.rendererApi = piRenderer::API::Vulkan;
        config.graphicsDevice = &unityVulkanDevice;
        // ImmPlayerManager passes Vulkan's zero-to-one, reversed-Z
        // GL.GetGPUProjectionMatrix output.
        config.reverseDepthBuffer = true;
        config.initializeRendererOnInit = true;
        config.initializeFullscreen = false;
    }
    else
    {
#if defined(__ANDROID__) || defined(ANDROID)
        config.rendererApi = piRenderer::API::GLES;
        config.initializeRendererOnInit = false;
        config.initializeDisplay = 1;
#else
        config.rendererApi = (gImmUnityPlugin.UnityAPI.mDevice == nullptr) ? piRenderer::API::GL : piRenderer::API::DX;
        config.graphicsDevice = gImmUnityPlugin.UnityAPI.mDevice;
        config.initializeRendererOnInit = true;
        config.initializeFullscreen = true;
#endif
    }
#else
    UnityGfxRenderer gfx = gImmUnityPlugin.UnityAPI.mGraphics->GetRenderer();
    if (gfx == kUnityGfxRendererMetal)
    {
        if (!gImmUnityPlugin.UnityAPI.mMetal || !gImmUnityPlugin.UnityAPI.mDevice)
        {
            std::fprintf(stderr, "Unity Metal interface or device is unavailable.\n");
            std::fflush(stderr);
            return -1;
        }
        config.rendererApi = piRenderer::API::Metal;
        config.graphicsDevice = gImmUnityPlugin.UnityAPI.mDevice;
        config.metalUnityProjectionAdjusted = true;
        config.reverseDepthBuffer = true;
    }
    else if (gfx == kUnityGfxRendererOpenGLCore)
    {
        config.rendererApi = piRenderer::API::GL;
    }
    else
    {
        std::fprintf(stderr, "Unsupported renderer on Apple platform. Expected Metal or OpenGLCore.\n");
        std::fflush(stderr);
        return -1;
    }
    config.initializeRendererOnInit = true;
#endif

    if (!gImmUnityPlugin.mBridge.Init(config))
        return -1;

#if defined(__ANDROID__) || defined(ANDROID)
    if (gImmUnityPlugin.UnityAPI.mRenderer == kUnityGfxRendererVulkan)
    {
        sAndroidDeferredInit.needsInit = false;
        sAndroidDeferredInit.isInitialized = true;
        __android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "Unity Android Vulkan renderer initialized from host device");
    }
    else
    {
        sAndroidDeferredInit.colorSpace = colorSpace;
        sAndroidDeferredInit.antialiasing = antialiasing;
        sAndroidDeferredInit.needsInit = true;
        sAndroidDeferredInit.isInitialized = false;
        __android_log_print(ANDROID_LOG_INFO, "ImmUnityPlugin", "Init complete - GLES renderer init deferred to render thread");
    }
#endif
    return 0;
}

extern "C" void UNITY_INTERFACE_EXPORT End(void)
{
    gImmUnityPlugin.mBridge.Shutdown();
}

extern "C" int UNITY_INTERFACE_EXPORT LoadFromFile(char *fileName)
{
    if (fileName == nullptr || fileName[0] == '\0')
    {
        iLog().Printf(LT_ERROR, L"LoadFromFile: filename is null or empty");
        return -1;
    }

#if defined(__ANDROID__) || defined(ANDROID)
    // Android Unity creates IMM's GLES renderer from the first render-thread
    // plugin event. Loading before that event leaves Player partially
    // initialized and crashes in Player::Load; report a load failure instead
    // so managed code can wait for the render-thread readiness signal.
    if (gImmUnityPlugin.UnityAPI.mRenderer != kUnityGfxRendererVulkan && !sAndroidDeferredInit.isInitialized)
    {
        __android_log_print(ANDROID_LOG_ERROR, "ImmUnityPlugin", "LoadFromFile blocked until Android deferred renderer init completes");
        iLog().Printf(LT_ERROR, L"LoadFromFile blocked until Android deferred renderer init completes");
        return -1;
    }
#endif

    iLog().Printf(LT_DEBUG, L"loading from file: %s", pistr2ws(fileName));

    try
    {
        int result = iPlayer().Load(pistr2ws(fileName));
        if (result < 0)
        {
            iLog().Printf(LT_ERROR, L"LoadFromFile: Player.Load failed with code %d for file: %s", result, pistr2ws(fileName));
        }
        else
        {
            iLog().Printf(LT_MESSAGE, L"LoadFromFile: Successfully loaded file: %s (ID: %d)", pistr2ws(fileName), result);
        }
        return result;
    }
    catch (...)
    {
        iLog().Printf(LT_ERROR, L"LoadFromFile: Exception caught while loading file: %s", pistr2ws(fileName));
        return -1;
    }
}

extern "C" int UNITY_INTERFACE_EXPORT LoadFromMemory(char *fileName, int size, void* data) // ToDo: need to pass data from managed code to native code.
{
#if defined(__ANDROID__) || defined(ANDROID)
    // See LoadFromFile. Memory-backed loads still enter Player::Load and need
    // the same render-thread GLES initialization to have completed first.
    if (!sAndroidDeferredInit.isInitialized)
    {
        __android_log_print(ANDROID_LOG_ERROR, "ImmUnityPlugin", "LoadFromMemory blocked until Android deferred renderer init completes");
        iLog().Printf(LT_ERROR, L"LoadFromMemory blocked until Android deferred renderer init completes");
        return -1;
    }
#endif

    if (!data || size == 0)
    {
        iLog().Printf(LT_DEBUG, L"loading from memory...\nfile name is %s\nsize is %d", pistr2ws(fileName), size);
        iLog().Printf(LT_DEBUG, L"Data is empty");
        return -1;

    }
    iLog().Printf(LT_DEBUG, L"loading from memory...\nfile name is %s\nsize is %d", pistr2ws(fileName), size);

    return iPlayer().Load(
        static_cast<const uint8_t*>(data),
        static_cast<uint64_t>(size),
        pistr2ws(fileName));

}

extern "C" bool UNITY_INTERFACE_EXPORT IsDocumentActive(int id)
{
    return iPlayer().IsDocumentActive(id);
}

extern "C" void UNITY_INTERFACE_EXPORT Unload(int id)
{
	iPlayer().Unload(id);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetDocumentToWorld(int id, float *doc2world)
{
	iPlayer().SetDocumentToWorld(id, fromMatrix(f2d(iUnityToPilibs(doc2world)) * mat4x4d::flipZ()));
}

extern "C" bool UNITY_INTERFACE_EXPORT SetLayerVisible(int docId, int layerId, int visible)
{
    return iPlayer().SetLayerVisible(docId, layerId, visible != 0);
}

extern "C" bool UNITY_INTERFACE_EXPORT ClearLayerVisibilityOverride(int docId, int layerId)
{
    return iPlayer().ClearLayerVisibilityOverride(docId, layerId);
}

extern "C" bool UNITY_INTERFACE_EXPORT SetLayerOpacity(int docId, int layerId, float opacity)
{
    return iPlayer().SetLayerOpacity(docId, layerId, opacity);
}

extern "C" bool UNITY_INTERFACE_EXPORT SetLayerTransform(int docId, int layerId, float *layerToWorld)
{
    return iPlayer().SetLayerTransform(docId, layerId, iUnityToTrans3d(layerToWorld));
}

extern "C" bool UNITY_INTERFACE_EXPORT ClearLayerTransformOverride(int docId, int layerId)
{
    return iPlayer().ClearLayerTransformOverride(docId, layerId);
}

extern "C" bool UNITY_INTERFACE_EXPORT GetLayerDiagnostics(int docId, int layerId, Player::LayerDiagnostics *outDiag)
{
    if (outDiag == nullptr)
        return false;
    return iPlayer().GetLayerDiagnostics(docId, layerId, *outDiag);
}

extern "C" void UNITY_INTERFACE_EXPORT Pause(int id)
{
	iPlayer().Pause(id );
}

extern "C" void UNITY_INTERFACE_EXPORT Resume(int id)
{
	iPlayer().Resume(id);
}

extern "C" void UNITY_INTERFACE_EXPORT Hide(int id)
{
	iPlayer().Hide(id);
}

extern "C" void UNITY_INTERFACE_EXPORT Show(int id)
{
	iPlayer().Show(id);
}

extern "C" void UNITY_INTERFACE_EXPORT Continue(int id)
{
	iPlayer().Continue(id);
}

extern "C" void UNITY_INTERFACE_EXPORT SkipForward(int id)
{
    iPlayer().SkipForward(id);
}

extern "C" void UNITY_INTERFACE_EXPORT SkipBack(int id)
{
	iPlayer().SkipBack(id);
}

extern "C" void UNITY_INTERFACE_EXPORT SetChapter(int id, int chapterIndex)
{
    iPlayer().SetChapter(id, chapterIndex);
}

extern "C" void UNITY_INTERFACE_EXPORT Restart(int id)
{
	iPlayer().Restart(id);
}


extern "C" int UNITY_INTERFACE_EXPORT GetChapterCount(int id)
{
    return iPlayer().GetChapterCount(id);
}

extern "C" int UNITY_INTERFACE_EXPORT GetCurrentChapter(int id)
{
    return iPlayer().GetCurrentChapter(id);
}

extern "C" void UNITY_INTERFACE_EXPORT SetTime(int id, int64_t timeSinceStart, int64_t timeSinceStop)
{
	iPlayer().SetTime(id, piTick(timeSinceStart), piTick(timeSinceStop));
}

extern "C" void UNITY_INTERFACE_EXPORT GetTime(int id, int64_t * timeSinceStart, int64_t * timeSinceStop)
{
	iPlayer().GetTime(id, (piTick*)timeSinceStart, (piTick*)timeSinceStop);
}

extern "C" int64_t UNITY_INTERFACE_EXPORT GetPlayTime(int id)
{
    piTick startTime;
    piTick stopTime;

    iPlayer().GetTime(id, &startTime, &stopTime);

    return piTick::CastInt(startTime);
}

extern "C" void UNITY_INTERFACE_EXPORT GetPlayerInfo(Player::PlayerInfo & info)
{
	iPlayer().GetPlayerInfo(info);
}

extern "C" void UNITY_INTERFACE_EXPORT GetDocumentState(Player::DocumentState & state, int id)
{
	iPlayer().GetDocumentState(state, id);
}

extern "C" uint32_t UNITY_INTERFACE_EXPORT GetDocumentInfoEx(int id)
{
    return iPlayer().GetDocumentInfoEx(id);
}

extern "C" float UNITY_INTERFACE_EXPORT GetSound(int id)
{
    return iPlayer().GetDocumentVolume(id);
}

extern "C" void UNITY_INTERFACE_EXPORT SetSound(int id, float volume)
{
    iPlayer().SetDocumentVolume(id, volume);
}

extern "C" void UNITY_INTERFACE_EXPORT GetBoundingBox(int id, bound3& bound)
{
    bound = d2f(iPlayer().GetDocumentBBox(id));
    const int layerCount = iPlayer().GetLayerCount(id);

    bound3 filtered = bound3(1.0e30f);
    for (int i = 0; i < layerCount; ++i)
    {
        Player::LayerInfo li;
        if (!iPlayer().GetLayerInfoByIndex(id, i, li))
            continue;
        if (li.hasBBox == 0)
            continue;
        if (!IsReasonableBound3(li.bbox))
            continue;
        filtered = include(filtered, li.bbox);
    }
    if (filtered.mMinX <= filtered.mMaxX)
    {
        bound = filtered;
    }
}

extern "C" bool UNITY_INTERFACE_EXPORT IsSequenceReady(int docId)
{
    return iPlayer().IsSequenceReady(docId);
}

extern "C" int UNITY_INTERFACE_EXPORT GetLayerCount(int docId)
{
    return iPlayer().GetLayerCount(docId);
}

struct UnityLayerInfo
{
    int id;
    int type;
    int parentId;
    int isTimeline;
    int isLoaded;
    int isVisible;
    float opacity;
    int hasBBox;
    bound3 bbox;
    int numChildren;
    int assetId;
    int paintNumDrawings;
    int paintNumFrames;
    int paintNumStrokes;
    char16_t name[128];
    char16_t fullName[256];
};

static_assert(sizeof(char16_t) == 2, "Unity layer strings must be UTF-16 sized");

static void iCopyWideToUnityUtf16(char16_t *dst, size_t dstCount, const wchar_t *src)
{
    if (dstCount == 0)
        return;

    size_t out = 0;
    if (src != nullptr)
    {
        for (size_t i = 0; src[i] != 0 && out + 1 < dstCount; ++i)
        {
            const uint32_t cp = static_cast<uint32_t>(src[i]);
            if (cp <= 0xffff)
            {
                dst[out++] = static_cast<char16_t>(cp);
            }
            else if (out + 2 < dstCount && cp <= 0x10ffff)
            {
                const uint32_t v = cp - 0x10000;
                dst[out++] = static_cast<char16_t>(0xd800 + (v >> 10));
                dst[out++] = static_cast<char16_t>(0xdc00 + (v & 0x3ff));
            }
            else
            {
                dst[out++] = static_cast<char16_t>('?');
            }
        }
    }
    dst[out] = 0;
}

extern "C" bool UNITY_INTERFACE_EXPORT GetLayerInfoByIndex(int docId, int index, UnityLayerInfo & info)
{
    Player::LayerInfo nativeInfo;
    if (!iPlayer().GetLayerInfoByIndex(docId, index, nativeInfo))
        return false;

    std::memset(&info, 0, sizeof(info));
    info.id = nativeInfo.id;
    info.type = nativeInfo.type;
    info.parentId = nativeInfo.parentId;
    info.isTimeline = nativeInfo.isTimeline;
    info.isLoaded = nativeInfo.isLoaded;
    info.isVisible = nativeInfo.isVisible;
    info.opacity = nativeInfo.opacity;
    info.hasBBox = nativeInfo.hasBBox;
    info.bbox = nativeInfo.bbox;
    info.numChildren = nativeInfo.numChildren;
    info.assetId = nativeInfo.assetId;
    info.paintNumDrawings = nativeInfo.paintNumDrawings;
    info.paintNumFrames = nativeInfo.paintNumFrames;
    info.paintNumStrokes = nativeInfo.paintNumStrokes;
    iCopyWideToUnityUtf16(info.name, sizeof(info.name) / sizeof(info.name[0]), nativeInfo.name);
    iCopyWideToUnityUtf16(info.fullName, sizeof(info.fullName) / sizeof(info.fullName[0]), nativeInfo.fullName);
    return true;
}

#pragma region SpawnArea

extern "C" int UNITY_INTERFACE_EXPORT GetSpawnAreaCount(int docId)
{
    return iPlayer().GetSpawnAreaCount(docId);
}

extern "C" int UNITY_INTERFACE_EXPORT GetSpawnAreaList(int docId, int spawnAreaIdsSize, int* pSpawnAreaIds)
{
    const int num = iPlayer().GetSpawnAreaCount(docId);
    piAssert(num <= spawnAreaIdsSize);
    for (int i = 0; i < num; ++i)
    {
        pSpawnAreaIds[i] = i;
    }
    return num;
}

extern "C" int UNITY_INTERFACE_EXPORT GetActiveSpawnAreaId(int docId)
{
    return iPlayer().GetSpawnArea(docId);
}

extern "C" int UNITY_INTERFACE_EXPORT GetInitialSpawnAreaId(int docId)
{
    return iPlayer().GetInitialSpawnArea(docId);
}

extern "C" void UNITY_INTERFACE_EXPORT SetActiveSpawnAreaId(int docId, int activeSpawnAreaId)
{
    iPlayer().SetSpawnArea(docId, activeSpawnAreaId);
}

struct SerializedSpawnArea
{
    enum class Type : uint32_t
    {
        EyeLevel = 0,
        FloorLevel = 1,
    };

    const char* mName;
    int mVersion;
    Type mType;
    bool mAnimated;
    struct Volume
    {
        enum Type
        {
            Sphere = 0,
            Box = 1,
        }type;

        struct
        {
            float r;
        }sphereExtent;
        struct
        {
            float x, y, z;
        } boxExtent;
        struct
        {
            float x, y, z;
        } offset;
    }volume;

    struct // Transform
    {
        float posx; // position
        float posy;
        float posz;
        float rotx; // rotation (quaternion)
        float roty;
        float rotz;
        float rotw;
        float sca;  // scale
    }transform;

    int locomotion;
    struct //Screenshot
    {
        uint32_t format;
        int32_t width;
        int32_t height;
        void* pData;
    }screenshot;
};

extern "C" bool UNITY_INTERFACE_EXPORT GetSpawnAreaInfo(int docId, int spawnareaId, SerializedSpawnArea& serializedSpawnArea)
{
    Document::SpawnAreaInfo spawnAreaInfo;
    if (!iPlayer().GetSpawnAreaInfo(spawnAreaInfo, docId, spawnareaId))
        return false;
    serializedSpawnArea.mName = piws2str(spawnAreaInfo.mName);
    serializedSpawnArea.mVersion = spawnAreaInfo.mVersion;
    serializedSpawnArea.mType = spawnAreaInfo.mIsFloorLevel ? SerializedSpawnArea::Type::FloorLevel : SerializedSpawnArea::Type::EyeLevel;
    serializedSpawnArea.mAnimated = spawnAreaInfo.mAnimated;
    trans3d mat = (spawnAreaInfo.mSpawnAreaToWorld);
    serializedSpawnArea.transform.posx = (float)mat.mTranslation.x;
    serializedSpawnArea.transform.posy = (float)mat.mTranslation.y;
    serializedSpawnArea.transform.posz = (float)mat.mTranslation.z;
    serializedSpawnArea.transform.rotx = (float)mat.mRotation.x;
    serializedSpawnArea.transform.roty = (float)mat.mRotation.y;
    serializedSpawnArea.transform.rotz = (float)mat.mRotation.z;
    serializedSpawnArea.transform.rotw = (float)mat.mRotation.w;
    serializedSpawnArea.transform.sca = (float)mat.mScale;

    switch (spawnAreaInfo.mVolume.mType)
    {
    case LayerSpawnArea::Volume::Type::Sphere:
    {
        serializedSpawnArea.volume.type = SerializedSpawnArea::Volume::Type::Sphere;
        const vec4 sph = spawnAreaInfo.mVolume.mShape.mSphere;
        serializedSpawnArea.volume.offset.x = sph.x;
        serializedSpawnArea.volume.offset.y = sph.y;
        serializedSpawnArea.volume.offset.z = sph.z;
        serializedSpawnArea.volume.sphereExtent.r = sph.w;
    } break;
    case LayerSpawnArea::Volume::Type::Box:
    {
        serializedSpawnArea.volume.type = SerializedSpawnArea::Volume::Type::Box;
        const vec3 cen = getcenter(spawnAreaInfo.mVolume.mShape.mBox);
        const vec3 ext = getradiius(spawnAreaInfo.mVolume.mShape.mBox);
        serializedSpawnArea.volume.offset.x = cen.x;
        serializedSpawnArea.volume.offset.y = cen.y;
        serializedSpawnArea.volume.offset.z = cen.z;
        serializedSpawnArea.volume.boxExtent.x = ext.x;
        serializedSpawnArea.volume.boxExtent.y = ext.y;
        serializedSpawnArea.volume.boxExtent.z = ext.z;
    } break;
    default: { piAssert(false); } break;
    }

    serializedSpawnArea.locomotion =
        (((spawnAreaInfo.mVolume.mAllowTranslationX ? 1 : 0) << 2) |
         ((spawnAreaInfo.mVolume.mAllowTranslationY ? 1 : 0) << 1) |
         ((spawnAreaInfo.mVolume.mAllowTranslationZ ? 1 : 0) << 0));


    const piImage* pScreenshot = iPlayer().GetSpawnAreaScreenshot(docId, spawnareaId);
    if (pScreenshot==nullptr)
    {
        serializedSpawnArea.screenshot.height = 0;
        serializedSpawnArea.screenshot.width = 0;
        serializedSpawnArea.screenshot.pData = nullptr;
        serializedSpawnArea.screenshot.format = 0;
    }
    else if (pScreenshot->GetNumChannels()==1)
    {
        serializedSpawnArea.screenshot.format = pScreenshot->GetFormat(0);
        serializedSpawnArea.screenshot.width = pScreenshot->GetXRes();
        serializedSpawnArea.screenshot.height = pScreenshot->GetYRes();
        serializedSpawnArea.screenshot.pData = pScreenshot->GetData(0);
    }
    return true;
}



#pragma endregion

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// Exporter API (C ABI for Unity)
// ----------------------------------------------------------------------------------------------------------------------------------------------------
struct ImmExporterTransformC
{
    float tx, ty, tz;
    float qx, qy, qz, qw;
    float scale;
};

struct ImmExporterPointC
{
    float px, py, pz;
    float nx, ny, nz;
    float dx, dy, dz;
    float r, g, b;
    float a;
    float width;
    float length;
    float time;
};

// Exporter functionality - not available on Android
#if defined(WINDOWS)

struct ImmExporterDrawingHandle
{
    ImmExporter::LayerPaint* paint = nullptr;
    ImmExporter::Drawing* drawing = nullptr;
    uint32_t index = 0;
};

struct ImmExporterMemoryHandle
{
    ImmCore::piTArray<uint8_t> data;
};

static ImmCore::trans3d ImmExporterMakeTransform(const ImmExporterTransformC* t)
{
    if (t == nullptr)
        return ImmCore::trans3d::identity();

    ImmCore::quatd q(t->qx, t->qy, t->qz, t->qw);
    ImmCore::vec3d tr(t->tx, t->ty, t->tz);
    return ImmCore::trans3d(q, static_cast<double>(t->scale), ImmCore::flip3::N, tr);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API ImmExporter_CreateSequence(
    int type,
    int caps,
    float bgR, float bgG, float bgB,
    uint32_t frameRate,
    int64_t maxMemory,
    int64_t maxRenderCalls,
    int64_t maxTriangles,
    int64_t maxSoundChannels)
{
    ImmExporter::Sequence* seq = new ImmExporter::Sequence();
    ImmExporter::Sequence::Requirements reqs = {};
    reqs.mMaxMemory = maxMemory;
    reqs.mMaxRenderCalls = maxRenderCalls;
    reqs.mMaxTriangles = maxTriangles;
    reqs.mMaxSoundChannels = maxSoundChannels;

    ImmExporter::Sequence::Type seqType = ImmExporter::Sequence::Type::Still;
    if (type >= 0 && type < static_cast<int>(ImmExporter::Sequence::Type::COUNT))
        seqType = static_cast<ImmExporter::Sequence::Type>(type);

    ImmCore::vec3 bg(bgR, bgG, bgB);
    if (!seq->Init(seqType, static_cast<uint8_t>(caps), reqs, bg, frameRate))
    {
        delete seq;
        return nullptr;
    }
    return seq;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ImmExporter_DestroySequence(void* sequenceHandle)
{
    ImmExporter::Sequence* seq = reinterpret_cast<ImmExporter::Sequence*>(sequenceHandle);
    if (seq == nullptr)
        return;

    seq->Deinit();
    delete seq;
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API ImmExporter_CreatePaintLayer(
    void* sequenceHandle,
    void* parentLayerHandle,
    const char* name,
    int visible,
    float opacity,
    const ImmExporterTransformC* transform,
    const ImmExporterTransformC* pivot,
    int isTimeline,
    int64_t durationTicks,
    uint32_t maxRepeatCount)
{
    ImmExporter::Sequence* seq = reinterpret_cast<ImmExporter::Sequence*>(sequenceHandle);
    if (seq == nullptr)
        return nullptr;

    ImmExporter::Layer* parent = reinterpret_cast<ImmExporter::Layer*>(parentLayerHandle);
    if (parent == nullptr)
        parent = seq->GetRoot();
    ImmExporter::Layer* layer = seq->CreateLayer(parent);
    if (layer == nullptr)
        return nullptr;

    const ImmCore::trans3d t = ImmExporterMakeTransform(transform);
    const ImmCore::trans3d p = ImmExporterMakeTransform(pivot);

    ImmCore::piString wname;
    if (name != nullptr && name[0] != '\0')
        wname.InitCopyS(name);
    else
        wname.InitCopyW(L"Paint");

    const bool ok = layer->Init(
        ImmExporter::Layer::Type::Paint,
        wname.GetS(),
        visible != 0,
        t,
        p,
        opacity,
        isTimeline != 0,
        static_cast<ImmCore::piTick>(durationTicks),
        maxRepeatCount);

    wname.End();
    if (!ok)
        return nullptr;

    ImmExporter::LayerPaint* paint = new ImmExporter::LayerPaint();
    paint->Init();
    layer->SetImplementation(paint);
    return layer;
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API ImmExporter_CreateGroupLayer(
    void* sequenceHandle,
    void* parentLayerHandle,
    const char* name,
    int visible,
    float opacity,
    const ImmExporterTransformC* transform,
    const ImmExporterTransformC* pivot,
    int isTimeline,
    int64_t durationTicks,
    uint32_t maxRepeatCount)
{
    ImmExporter::Sequence* seq = reinterpret_cast<ImmExporter::Sequence*>(sequenceHandle);
    if (seq == nullptr)
        return nullptr;

    ImmExporter::Layer* parent = reinterpret_cast<ImmExporter::Layer*>(parentLayerHandle);
    if (parent == nullptr)
        parent = seq->GetRoot();
    ImmExporter::Layer* layer = seq->CreateLayer(parent);
    if (layer == nullptr)
        return nullptr;

    const ImmCore::trans3d t = ImmExporterMakeTransform(transform);
    const ImmCore::trans3d p = ImmExporterMakeTransform(pivot);

    ImmCore::piString wname;
    if (name != nullptr && name[0] != '\0')
        wname.InitCopyS(name);
    else
        wname.InitCopyW(L"Group");

    const bool ok = layer->Init(
        ImmExporter::Layer::Type::Group,
        wname.GetS(),
        visible != 0,
        t,
        p,
        opacity,
        isTimeline != 0,
        static_cast<ImmCore::piTick>(durationTicks),
        maxRepeatCount);

    wname.End();
    if (!ok)
        return nullptr;

    return layer;
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API ImmExporter_CreateDrawing(void* paintLayerHandle)
{
    ImmExporter::Layer* layer = reinterpret_cast<ImmExporter::Layer*>(paintLayerHandle);
    if (layer == nullptr || layer->GetType() != ImmExporter::Layer::Type::Paint)
        return nullptr;

    ImmExporter::LayerPaint* paint = reinterpret_cast<ImmExporter::LayerPaint*>(layer->GetImplementation());
    if (paint == nullptr)
        return nullptr;

    const uint32_t drawingIndex = paint->GetNumDrawings();
    ImmExporter::Drawing* drawing = paint->CreateDrawing();
    if (drawing == nullptr)
        return nullptr;

    ImmExporterDrawingHandle* handle = new ImmExporterDrawingHandle();
    handle->paint = paint;
    handle->drawing = drawing;
    handle->index = drawingIndex;
    return handle;
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_LayerAddAnimationKey(
    void* layerHandle,
    int property,
    int64_t timeTicks,
    int interpolation,
    int boolValue,
    uint32_t intValue,
    float floatValue,
    double doubleValue,
    const ImmExporterTransformC* transformValue)
{
    ImmExporter::Layer* layer = reinterpret_cast<ImmExporter::Layer*>(layerHandle);
    if (layer == nullptr || timeTicks < 0 ||
        property < 0 || property >= static_cast<int>(ImmExporter::Layer::AnimProperty::MAX) ||
        interpolation < 0 || interpolation >= static_cast<int>(ImmExporter::Layer::InterpolationType::MAX))
    {
        return false;
    }

    ImmExporter::Layer::AnimValue value;
    value.mBool = boolValue != 0;
    value.mInt = intValue;
    value.mFloat = floatValue;
    value.mDouble = doubleValue;
    value.mTransform = ImmExporterMakeTransform(transformValue);
    return layer->AddKey(
        ImmCore::piTick(timeTicks),
        static_cast<ImmExporter::Layer::AnimProperty>(property),
        value,
        static_cast<ImmExporter::Layer::InterpolationType>(interpolation));
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ImmExporter_DestroyDrawing(void* drawingHandle)
{
    ImmExporterDrawingHandle* handle = reinterpret_cast<ImmExporterDrawingHandle*>(drawingHandle);
    if (handle == nullptr)
        return;
    delete handle;
}

extern "C" UNITY_INTERFACE_EXPORT uint32_t UNITY_INTERFACE_API ImmExporter_GetDrawingIndex(void* drawingHandle)
{
    ImmExporterDrawingHandle* handle = reinterpret_cast<ImmExporterDrawingHandle*>(drawingHandle);
    if (handle == nullptr)
        return 0;
    return handle->index;
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_DrawingInit(void* drawingHandle, uint32_t numElements, int flipped)
{
    ImmExporterDrawingHandle* handle = reinterpret_cast<ImmExporterDrawingHandle*>(drawingHandle);
    if (handle == nullptr || handle->drawing == nullptr)
        return false;
    return handle->drawing->Init(numElements, flipped != 0);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API ImmExporter_DrawingGetElement(void* drawingHandle, uint32_t elementIndex)
{
    ImmExporterDrawingHandle* handle = reinterpret_cast<ImmExporterDrawingHandle*>(drawingHandle);
    if (handle == nullptr || handle->drawing == nullptr)
        return nullptr;
    return handle->drawing->GetElement(elementIndex);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_ElementInit(
    void* elementHandle,
    uint32_t numPoints,
    int brushSectionType,
    int visibilityType)
{
    ImmExporter::Element* element = reinterpret_cast<ImmExporter::Element*>(elementHandle);
    if (element == nullptr)
        return false;
    return element->Init(
        numPoints,
        static_cast<ImmExporter::Element::BrushSectionType>(brushSectionType),
        static_cast<ImmExporter::Element::VisibilityType>(visibilityType));
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_ElementSetPoint(
    void* elementHandle,
    uint32_t pointIndex,
    const ImmExporterPointC* point)
{
    ImmExporter::Element* element = reinterpret_cast<ImmExporter::Element*>(elementHandle);
    if (element == nullptr || point == nullptr)
        return false;

    ImmExporter::Point* p = element->GetPoint(pointIndex);
    if (p == nullptr)
        return false;

    p->mPos = ImmCore::vec3(point->px, point->py, point->pz);
    p->mNor = ImmCore::vec3(point->nx, point->ny, point->nz);
    p->mDir = ImmCore::vec3(point->dx, point->dy, point->dz);
    p->mCol = ImmCore::vec3(point->r, point->g, point->b);
    p->mTra = point->a;
    p->mWid = point->width;
    p->mLen = point->length;
    p->mTim = point->time;
    return true;
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_ElementSetPoints(
    void* elementHandle,
    uint32_t startPointIndex,
    const ImmExporterPointC* points,
    uint32_t pointCount)
{
    ImmExporter::Element* element = reinterpret_cast<ImmExporter::Element*>(elementHandle);
    if (element == nullptr || (points == nullptr && pointCount != 0))
        return false;
    if (startPointIndex > element->GetNumPoints() || pointCount > element->GetNumPoints() - startPointIndex)
        return false;

    for (uint32_t i = 0; i < pointCount; ++i)
    {
        ImmExporter::Point* destination = element->GetPoint(startPointIndex + i);
        const ImmExporterPointC& source = points[i];
        if (destination == nullptr)
            return false;
        destination->mPos = ImmCore::vec3(source.px, source.py, source.pz);
        destination->mNor = ImmCore::vec3(source.nx, source.ny, source.nz);
        destination->mDir = ImmCore::vec3(source.dx, source.dy, source.dz);
        destination->mCol = ImmCore::vec3(source.r, source.g, source.b);
        destination->mTra = source.a;
        destination->mWid = source.width;
        destination->mLen = source.length;
        destination->mTim = source.time;
    }
    return true;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ImmExporter_ComputeElementBounds(void* elementHandle)
{
    ImmExporter::Element* element = reinterpret_cast<ImmExporter::Element*>(elementHandle);
    if (element == nullptr)
        return;
    element->ComputeBoundingBox();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ImmExporter_ComputeDrawingBounds(void* drawingHandle)
{
    ImmExporterDrawingHandle* handle = reinterpret_cast<ImmExporterDrawingHandle*>(drawingHandle);
    if (handle == nullptr || handle->drawing == nullptr)
        return;
    handle->drawing->ComputeBoundingBox();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ImmExporter_PaintAddFrame(void* paintLayerHandle, uint32_t drawingIndex)
{
    ImmExporter::Layer* layer = reinterpret_cast<ImmExporter::Layer*>(paintLayerHandle);
    if (layer == nullptr || layer->GetType() != ImmExporter::Layer::Type::Paint)
        return;

    ImmExporter::LayerPaint* paint = reinterpret_cast<ImmExporter::LayerPaint*>(layer->GetImplementation());
    if (paint == nullptr)
        return;

    paint->AddFrame(drawingIndex);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_PaintSetMaxRepeatCount(
    void* paintLayerHandle,
    uint32_t maxRepeatCount)
{
    ImmExporter::Layer* layer = reinterpret_cast<ImmExporter::Layer*>(paintLayerHandle);
    if (layer == nullptr || layer->GetType() != ImmExporter::Layer::Type::Paint)
        return false;
    ImmExporter::LayerPaint* paint = reinterpret_cast<ImmExporter::LayerPaint*>(layer->GetImplementation());
    if (paint == nullptr)
        return false;
    paint->SetMaxRepeatCount(maxRepeatCount);
    return true;
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ImmExporter_ExportToFile(
    void* sequenceHandle,
    const char* fileName,
    int opusBitrate,
    int audioType)
{
    ImmExporter::Sequence* seq = reinterpret_cast<ImmExporter::Sequence*>(sequenceHandle);
    if (seq == nullptr || fileName == nullptr || fileName[0] == '\0')
        return false;

    return ImmExporter::ExportToFile(
        fileName,
        seq,
        opusBitrate,
        static_cast<ImmExporter::tiLayerSound::AudioType>(audioType));
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API ImmExporter_ExportToMemory(
    void* sequenceHandle,
    int opusBitrate,
    int audioType)
{
    ImmExporter::Sequence* seq = reinterpret_cast<ImmExporter::Sequence*>(sequenceHandle);
    if (seq == nullptr)
        return nullptr;

    ImmExporterMemoryHandle* result = new ImmExporterMemoryHandle();
    if (!result->data.Init(1024, false) || !ImmExporter::ExportToMemory(
            &result->data,
            seq,
            opusBitrate,
            static_cast<ImmExporter::tiLayerSound::AudioType>(audioType),
            nullptr))
    {
        result->data.End();
        delete result;
        return nullptr;
    }
    return result;
}

extern "C" UNITY_INTERFACE_EXPORT const void* UNITY_INTERFACE_API ImmExporter_GetMemoryData(void* memoryHandle)
{
    ImmExporterMemoryHandle* handle = reinterpret_cast<ImmExporterMemoryHandle*>(memoryHandle);
    if (handle == nullptr || handle->data.GetLength() == 0)
        return nullptr;
    return handle->data.GetAddress(0);
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API ImmExporter_GetMemorySize(void* memoryHandle)
{
    ImmExporterMemoryHandle* handle = reinterpret_cast<ImmExporterMemoryHandle*>(memoryHandle);
    return handle == nullptr ? 0 : handle->data.GetLength();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ImmExporter_DestroyMemory(void* memoryHandle)
{
    ImmExporterMemoryHandle* handle = reinterpret_cast<ImmExporterMemoryHandle*>(memoryHandle);
    if (handle == nullptr)
        return;
    handle->data.End();
    delete handle;
}

#endif // WINDOWS - End of exporter functionality
