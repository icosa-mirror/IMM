#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define IMMGODOT_EXPORT __declspec(dllexport)
#else
#define IMMGODOT_EXPORT
#endif

extern "C"
{
    struct ImmGodotSpawnArea
    {
        enum Type : uint32_t
        {
            EyeLevel = 0,
            FloorLevel = 1,
        };

        const char *name;
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
    IMMGODOT_EXPORT void ImmGodot_Shutdown();
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

    IMMGODOT_EXPORT int ImmGodot_LoadFromFile(char *fileName);
    IMMGODOT_EXPORT void ImmGodot_Unload(int id);
    IMMGODOT_EXPORT void ImmGodot_SetDocumentToWorld(int id, float *doc2world);

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

    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaCount(int docId);
    IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaList(int docId, int spawnAreaIdsSize, int *spawnAreaIds);
    IMMGODOT_EXPORT int ImmGodot_GetActiveSpawnAreaId(int docId);
    IMMGODOT_EXPORT int ImmGodot_GetInitialSpawnAreaId(int docId);
    IMMGODOT_EXPORT void ImmGodot_SetActiveSpawnAreaId(int docId, int spawnAreaId);
    IMMGODOT_EXPORT bool ImmGodot_GetSpawnAreaInfo(int docId, int spawnAreaId, ImmGodotSpawnArea &spawnArea);
}
