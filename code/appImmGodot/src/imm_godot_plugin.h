#pragma once

#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#if defined(_WIN32)
#if defined(IMMGODOT_BUILD)
#define IMMGODOT_EXPORT __declspec(dllexport)
#else
#define IMMGODOT_EXPORT __declspec(dllimport)
#endif
#else
#define IMMGODOT_EXPORT
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    enum
    {
        IMM_GODOT_SPAWN_AREA_EYE_LEVEL = 0,
        IMM_GODOT_SPAWN_AREA_FLOOR_LEVEL = 1,
    };

    enum
    {
        IMM_GODOT_SPAWN_VOLUME_SPHERE = 0,
        IMM_GODOT_SPAWN_VOLUME_BOX = 1,
    };

    typedef struct ImmGodotViewport
    {
        float x;
        float y;
        float width;
        float height;
        float minDepth;
        float maxDepth;
    } ImmGodotViewport;

    typedef struct ImmGodotRenderAdapter
    {
        void *userData;
        void (*graphicsInitialized)(void *userData);
        void (*graphicsShutdown)(void *userData);
        int (*beforeRenderCamera)(void *userData, int cameraId, int eyeId, const ImmGodotViewport *viewport);
        void (*afterRenderCamera)(void *userData, int cameraId, int eyeId, const ImmGodotViewport *viewport, int renderResult);
    } ImmGodotRenderAdapter;

    typedef struct ImmGodotSpawnArea
    {
        char name[256];
        int version;
        uint32_t type;
        int animated;
        struct Volume
        {
            uint32_t type;

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
    } ImmGodotSpawnArea;

    typedef struct ImmGodotPlayerInfo
    {
        struct BackgroundColor
        {
            float red;
            float green;
            float blue;
        } backgroundColor;
    } ImmGodotPlayerInfo;

    typedef struct ImmGodotDocumentState
    {
        int loadingState;
        int playbackState;
    } ImmGodotDocumentState;

    typedef struct ImmGodotBounds
    {
        float minX;
        float maxX;
        float minY;
        float maxY;
        float minZ;
        float maxZ;
    } ImmGodotBounds;

    IMMGODOT_EXPORT int ImmGodot_Init(int colorSpace, int antialiasing, const char *logFileName, const char *tmpFolderName);
    IMMGODOT_EXPORT void ImmGodot_Shutdown();
    IMMGODOT_EXPORT void ImmGodot_GlobalWork(int enabled);
    IMMGODOT_EXPORT void ImmGodot_SetRenderAdapter(const ImmGodotRenderAdapter *adapter);
    IMMGODOT_EXPORT void ImmGodot_ClearRenderAdapter();
    IMMGODOT_EXPORT void ImmGodot_SetMatrixDebugLogging(int enabled);
    IMMGODOT_EXPORT void ImmGodot_SetCameraMatrices(int cameraID,
                                                    int stereoType,
                                                    const float *world2head,
                                                    const float *prjHead,
                                                    const float *world2leftEye,
                                                    const float *prjLeft,
                                                    const float *world2rightEye,
                                                    const float *prjRight);
    IMMGODOT_EXPORT int ImmGodot_RenderCamera(int cameraID,
                                              int eyeID,
                                              float viewportX,
                                              float viewportY,
                                              float viewportWidth,
                                              float viewportHeight,
                                              float minDepth,
                                              float maxDepth);

    IMMGODOT_EXPORT int ImmGodot_LoadFromFile(const char *fileName);
    IMMGODOT_EXPORT void ImmGodot_Unload(int id);
    IMMGODOT_EXPORT void ImmGodot_SetDocumentToWorld(int id, const float *doc2world);

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
    IMMGODOT_EXPORT void ImmGodot_SetVolume(int id, float volume);
    IMMGODOT_EXPORT float ImmGodot_GetVolume(int id);
    IMMGODOT_EXPORT void ImmGodot_GetDocumentState(int id, ImmGodotDocumentState *state);
    IMMGODOT_EXPORT void ImmGodot_GetBoundingBox(int id, ImmGodotBounds *bounds);

    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaCount(int docId);
    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaList(int docId, int spawnAreaIdsSize, int *spawnAreaIds);
    IMMGODOT_EXPORT int ImmGodot_GetActiveSpawnAreaId(int docId);
    IMMGODOT_EXPORT int ImmGodot_GetInitialSpawnAreaId(int docId);
    IMMGODOT_EXPORT void ImmGodot_SetActiveSpawnAreaId(int docId, int spawnAreaId);
    IMMGODOT_EXPORT bool ImmGodot_GetSpawnAreaInfo(int docId, int spawnAreaId, ImmGodotSpawnArea *spawnArea);
    IMMGODOT_EXPORT void ImmGodot_GetPlayerInfo(ImmGodotPlayerInfo *info);

#ifdef __cplusplus
}
#endif
