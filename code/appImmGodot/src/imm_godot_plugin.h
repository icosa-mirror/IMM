#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define IMMGODOT_EXPORT __declspec(dllexport)
#else
#define IMMGODOT_EXPORT
#endif

extern "C"
{
    enum
    {
        ImmGodotSpawnAreaNameCapacity = 256,
        ImmGodotLayerNameCapacity = 128,
        ImmGodotLayerFullNameCapacity = 256,
    };

    enum ImmGodotRendererApi
    {
        ImmGodotRendererApi_Auto = 0,
        ImmGodotRendererApi_OpenGL = 1,
        ImmGodotRendererApi_Direct3D = 2,
        ImmGodotRendererApi_GLES = 3,
        ImmGodotRendererApi_Metal = 4,
        ImmGodotRendererApi_Vulkan = 5,
    };

    enum ImmGodotMetalFrameMode
    {
        ImmGodotMetalFrameMode_CommandEncoder = 0,
        ImmGodotMetalFrameMode_CommandBufferRenderPass = 1,
        ImmGodotMetalFrameMode_CommandQueueRenderPass = 2,
    };

    struct ImmGodotViewport
    {
        float x;
        float y;
        float width;
        float height;
        float minDepth;
        float maxDepth;
    };

    struct ImmGodotRenderAdapter
    {
        uint32_t version;
        void *userData;
        int (*beforeRenderCamera)(void *userData, int cameraID, int eyeID, const ImmGodotViewport *viewport);
        void (*afterRenderCamera)(void *userData, int cameraID, int eyeID, const ImmGodotViewport *viewport, int renderResult);
        void (*onGraphicsInitialized)(void *userData);
        void (*onGraphicsShutdown)(void *userData);
    };

    struct ImmGodotMetalFrame
    {
        uint32_t version;
        int mode;
        void *commandQueue;
        void *commandBuffer;
        void *commandEncoder;
        void *renderPassDescriptor;
        int width;
        int height;
    };

    struct ImmGodotVulkanFrame
    {
        uint32_t version;
        void *instance;
        void *physicalDevice;
        void *device;
        void *graphicsQueue;
        uint32_t graphicsQueueFamilyIndex;
        void *colorImage;
        void *colorImageView;
        uint32_t colorFormat;
        int width;
        int height;
    };

    struct ImmGodotPlayerInfo
    {
        float backgroundRed;
        float backgroundGreen;
        float backgroundBlue;
    };

    struct ImmGodotTimeInfo
    {
        int64_t timeSinceStart;
        int64_t timeSinceStop;
        int64_t playTime;
    };

    struct ImmGodotDocumentState
    {
        int loadingState;
        int playbackState;
    };

    struct ImmGodotBounds3
    {
        float minX;
        float maxX;
        float minY;
        float maxY;
        float minZ;
        float maxZ;
    };

    struct ImmGodotLayerInfo
    {
        int id;
        int type;
        int parentId;
        int isTimeline;
        int isLoaded;
        int isVisible;
        float opacity;
        int hasBounds;
        ImmGodotBounds3 bounds;
        int numChildren;
        int assetId;
        int paintNumDrawings;
        int paintNumFrames;
        int paintNumStrokes;
        char name[ImmGodotLayerNameCapacity];
        char fullName[ImmGodotLayerFullNameCapacity];
    };

    struct ImmGodotLayerDiagnostics
    {
        int hasVisibilityKeys;
        int hasOpacityKeys;
        int isVisible;
        float opacity;
        int isWorldVisible;
        float worldOpacity;
        int parentId;
        int visibilityOverrideEnabled;
        int visibilityOverrideValue;
        int hasTransformKeys;
        int transformOverrideEnabled;
    };

    struct ImmGodotRenderPerformanceInfo
    {
        int numDrawCalls;
        int numDrawCallsCulled;
        int numPaintDrawCalls;
        int numPictureDrawCalls;
        int numPicture2DDrawCalls;
        int numPicture360DrawCalls;
        int numPicture360EquirectDrawCalls;
        int numPicture360CubemapDrawCalls;
        int numModelDrawCalls;
        int numTriangles;
        int numTrianglesCulled;
        uint64_t validationTimeFrame;
    };

    struct ImmGodotSpawnArea
    {
        enum Type : uint32_t
        {
            EyeLevel = 0,
            FloorLevel = 1,
        };

        char name[ImmGodotSpawnAreaNameCapacity];
        int version;
        Type type;
        int animated;
        struct Volume
        {
            enum Kind
            {
                Sphere = 0,
                Box = 1,
            } type;

            struct
            {
                float r;
            } sphereExtent;
            struct
            {
                float x, y, z;
            } boxExtent;
            struct
            {
                float x, y, z;
            } offset;
            struct
            {
                int x, y, z;
            } allowTranslation;
        } volume;

        struct Transform
        {
            float posx;
            float posy;
            float posz;
            float rotx;
            float roty;
            float rotz;
            float rotw;
            float sca;
        } transform;

        int locomotion;
    };

    IMMGODOT_EXPORT int ImmGodot_Init(int colorSpace, int antialiasing, char *logFileName, char *tmpFolderName);
    IMMGODOT_EXPORT int ImmGodot_InitEx(int colorSpace, int antialiasing, char *logFileName, char *tmpFolderName, int rendererApi);
    IMMGODOT_EXPORT void ImmGodot_Shutdown();
    IMMGODOT_EXPORT int ImmGodot_IsInitialized();
    IMMGODOT_EXPORT void ImmGodot_SetDebugLogging(int enabled);
    IMMGODOT_EXPORT void ImmGodot_SetRenderAdapter(const ImmGodotRenderAdapter *adapter);
    IMMGODOT_EXPORT int ImmGodot_BeginMetalFrame(const ImmGodotMetalFrame *frame);
    IMMGODOT_EXPORT void ImmGodot_EndMetalFrame();
    IMMGODOT_EXPORT int ImmGodot_BeginVulkanFrame(const ImmGodotVulkanFrame *frame);
    IMMGODOT_EXPORT void ImmGodot_EndVulkanFrame();
    IMMGODOT_EXPORT void ImmGodot_GlobalWork(int enabled);
    IMMGODOT_EXPORT void ImmGodot_SetCameraMatrices(int cameraID,
                                                    int stereoType,
                                                    float *world2head,
                                                    float *prjHead,
                                                    float *world2leftEye,
                                                    float *prjLeft,
                                                    float *world2rightEye,
                                                    float *prjRight);
    IMMGODOT_EXPORT int ImmGodot_RenderCamera(int cameraID,
                                              int eyeID,
                                              float viewportX,
                                              float viewportY,
                                              float viewportWidth,
                                              float viewportHeight,
                                              float minDepth,
                                              float maxDepth);
    IMMGODOT_EXPORT int ImmGodot_GetRenderPerformanceInfo(ImmGodotRenderPerformanceInfo *info);

    IMMGODOT_EXPORT int ImmGodot_LoadFromFile(char *fileName);
    IMMGODOT_EXPORT void ImmGodot_Unload(int id);
    IMMGODOT_EXPORT void ImmGodot_SetDocumentToWorld(int id, float *doc2world);
    IMMGODOT_EXPORT int ImmGodot_GetPlayerInfo(ImmGodotPlayerInfo *info);
    IMMGODOT_EXPORT int ImmGodot_GetDocumentState(int id, ImmGodotDocumentState *state);
    IMMGODOT_EXPORT uint32_t ImmGodot_GetDocumentInfoEx(int id);
    IMMGODOT_EXPORT int ImmGodot_IsSequenceReady(int id);
    IMMGODOT_EXPORT int ImmGodot_GetBoundingBox(int id, ImmGodotBounds3 *bounds);
    IMMGODOT_EXPORT int ImmGodot_GetLayerCount(int id);
    IMMGODOT_EXPORT int ImmGodot_GetLayerInfoByIndex(int id, int index, ImmGodotLayerInfo *info);
    IMMGODOT_EXPORT int ImmGodot_SetLayerVisible(int docId, int layerId, int visible);
    IMMGODOT_EXPORT int ImmGodot_ClearLayerVisibilityOverride(int docId, int layerId);
    IMMGODOT_EXPORT int ImmGodot_SetLayerOpacity(int docId, int layerId, float opacity);
    IMMGODOT_EXPORT int ImmGodot_SetLayerTransform(int docId, int layerId, float *layerToWorld);
    IMMGODOT_EXPORT int ImmGodot_ClearLayerTransformOverride(int docId, int layerId);
    IMMGODOT_EXPORT int ImmGodot_GetLayerDiagnostics(int docId, int layerId, ImmGodotLayerDiagnostics *diagnostics);

    IMMGODOT_EXPORT void ImmGodot_Pause(int id);
    IMMGODOT_EXPORT void ImmGodot_Resume(int id);
    IMMGODOT_EXPORT void ImmGodot_Restart(int id);
    IMMGODOT_EXPORT void ImmGodot_Show(int id);
    IMMGODOT_EXPORT void ImmGodot_Hide(int id);
    IMMGODOT_EXPORT void ImmGodot_Continue(int id);
    IMMGODOT_EXPORT void ImmGodot_SkipForward(int id);
    IMMGODOT_EXPORT void ImmGodot_SkipBack(int id);
    IMMGODOT_EXPORT int ImmGodot_GetChapterCount(int id);
    IMMGODOT_EXPORT int ImmGodot_GetCurrentChapter(int id);
    IMMGODOT_EXPORT void ImmGodot_SetChapter(int id, int chapterIndex);
    IMMGODOT_EXPORT void ImmGodot_SetTime(int id, int64_t timeSinceStart, int64_t timeSinceStop);
    IMMGODOT_EXPORT int ImmGodot_GetTime(int id, ImmGodotTimeInfo *timeInfo);
    IMMGODOT_EXPORT int64_t ImmGodot_GetPlayTime(int id);
    IMMGODOT_EXPORT int64_t ImmGodot_SecondsToTicks(double seconds);
    IMMGODOT_EXPORT double ImmGodot_TicksToSeconds(int64_t ticks);
    IMMGODOT_EXPORT int64_t ImmGodot_GetTicksPerSecond();
    IMMGODOT_EXPORT void ImmGodot_SetVolume(int id, float volume);
    IMMGODOT_EXPORT float ImmGodot_GetVolume(int id);

    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaCount(int docId);
    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaList(int docId, int spawnAreaIdsSize, int *spawnAreaIds);
    IMMGODOT_EXPORT int ImmGodot_GetActiveSpawnAreaId(int docId);
    IMMGODOT_EXPORT int ImmGodot_GetInitialSpawnAreaId(int docId);
    IMMGODOT_EXPORT void ImmGodot_SetActiveSpawnAreaId(int docId, int spawnAreaId);
    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaInfo(int docId, int spawnAreaId, ImmGodotSpawnArea *spawnArea);
}
