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
#include <dirent.h>
#include <mutex>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <cmath>

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
    bool hasWindow = false;
    bool running = false;
    bool useVulkan = false;

    piRenderer* renderer = nullptr;
    piLog* log = nullptr;
    piTimer* timer = nullptr;
    piSoundEngineBackend* soundBackend = nullptr;
    Viewer* viewer = nullptr;
    piTexture colorTexture = nullptr;
    piTexture depthTexture = nullptr;
    piRTarget renderTarget = nullptr;
    ImmPlayer::StereoMode stereoMode = ImmPlayer::StereoMode::None;
    bool viewerInitialized = false;
    bool firstFrame = true;

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

std::string gAssetDirectory;
std::string gExternalFilesDirectory;

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
            return assetImmPath;
        }
    }

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
}

bool ensureRenderTarget() {
    if (!gEngine.useVulkan) {
        return true;
    }
    if (!gEngine.renderer || gEngine.width <= 0 || gEngine.height <= 0) {
        return false;
    }
    if (gEngine.renderTarget) {
        return true;
    }

    const piRenderer::TextureInfo colorInfo = {
        piRenderer::TextureType::T2D,
        piRenderer::Format::C3_11_11_10_FLOAT,
        gEngine.width,
        gEngine.height,
        1,
        4,
        1,
        0
    };
    const piRenderer::TextureInfo depthInfo = {
        piRenderer::TextureType::T2D,
        piRenderer::Format::DS_24_8_UINT,
        gEngine.width,
        gEngine.height,
        1,
        4,
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

    ALOGV("Android Vulkan render target ready %dx%d", gEngine.width, gEngine.height);
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

    Settings settings;
    settings.mPlayback.mLocation = ImmCore::trans3d::identity();
    settings.mPlayback.mPlayerSpawn.mLocation.InitCopyW(gEngine.playerSpawnLocation.c_str());
    settings.mPlayback.mPlayerSpawn.mCustom = ImmCore::trans3d::identity();

    settings.mRendering.mRenderingAPI = gEngine.useVulkan ? Settings::Rendering::API::Vulkan : Settings::Rendering::API::GLES;
    settings.mRendering.mRenderingTechnique = gEngine.renderingTechnique;
    settings.mRendering.mEnableVR = false;

    if (!settings.mFiles.mLoad.Init(16, false)) {
        return false;
    }
    settings.mFiles.mLoad.New(1, true);
    settings.mFiles.mLoad[0].InitCopyW(path.c_str());

    const bool success = gEngine.viewer->Init(
        0,
        gEngine.renderer,
        gEngine.soundBackend ? gEngine.soundBackend->GetEngine() : nullptr,
        gEngine.log,
        gEngine.timer,
        gEngine.stereoMode,
        &settings);

    settings.mFiles.mLoad[0].End();
    settings.mFiles.mLoad.End();

    gEngine.viewerInitialized = success;
    gEngine.firstFrame = true;
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
        glViewport(0, 0, gEngine.width, gEngine.height);
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

    const float aspect = (gEngine.height > 0) ? (float)gEngine.width / (float)gEngine.height : 1.0f;
    const mat4x4 projection = setPerspective(50.0f, aspect, 0.01f, 1000.0f);
    const trans3d vrToHead = trans3d::identity();

    gEngine.viewer->GlobalWork(nullptr, false, vrToHead, nullptr, nullptr, gEngine.log, dtime,
                               ivec2(gEngine.width, gEngine.height), true, 8000, gEngine.firstFrame);
    gEngine.viewer->GlobalRender(vrToHead, projection);
    gEngine.viewer->RenderMono(ivec2(gEngine.width, gEngine.height), vrToHead, 0);

    if (gEngine.useVulkan) {
        gEngine.renderer->SetRenderTarget(nullptr);
        gEngine.renderer->SwapBuffers();
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
