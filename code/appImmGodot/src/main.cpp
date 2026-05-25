#include "appImmShared/src/imm_engine_bridge.h"
#include "imm_godot_plugin.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmImporter/src/document/layerSpawnArea.h"

#include <cstdlib>
#include <cstring>

using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

namespace
{
    ImmShared::ImmEngineBridge gBridge;
    ImmGodotRenderAdapter gRenderAdapter = {};
    bool gHasRenderAdapter = false;
    bool gMatrixDebugLogging = false;

    mat4x4 iHostToPilibs(const float *m)
    {
        return mat4x4(m[0], m[4], m[8], m[12],
                      m[1], m[5], m[9], m[13],
                      m[2], m[6], m[10], m[14],
                      m[3], m[7], m[11], m[15]);
    }

    Player &iPlayer()
    {
        return *gBridge.GetPlayer();
    }

    bool iBackendReady()
    {
        return gBridge.IsInitialized();
    }

    void iLogSubmittedMatrix(const wchar_t *name, const float *m)
    {
        if (!gMatrixDebugLogging || m == nullptr || gBridge.GetLog() == nullptr)
            return;

        gBridge.GetLog()->Printf(LT_DEBUG,
                                 L"%s = [%0.6f %0.6f %0.6f %0.6f; %0.6f %0.6f %0.6f %0.6f; %0.6f %0.6f %0.6f %0.6f; %0.6f %0.6f %0.6f %0.6f]",
                                 name,
                                 m[0], m[1], m[2], m[3],
                                 m[4], m[5], m[6], m[7],
                                 m[8], m[9], m[10], m[11],
                                 m[12], m[13], m[14], m[15]);
    }

}

extern "C" IMMGODOT_EXPORT int ImmGodot_Init(int colorSpace,
                                             int antialiasing,
                                             const char *logFileName,
                                             const char *tmpFolderName)
{
    ImmShared::ImmEngineBridge::InitConfig config = {};
    config.colorSpace = colorSpace;
    config.antialiasing = antialiasing;
    config.logFileName = logFileName;
    config.tmpFolderName = tmpFolderName;
#if defined(__APPLE__)
    config.rendererApi = piRenderer::API::Metal;
#else
    config.rendererApi = piRenderer::API::GL;
#endif
    config.initializeRendererOnInit = true;
    config.initializeFullscreen = true;
    const bool initialized = gBridge.Init(config);
    if (initialized && gHasRenderAdapter && gRenderAdapter.graphicsInitialized != nullptr)
    {
        gRenderAdapter.graphicsInitialized(gRenderAdapter.userData);
    }
    return initialized ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Shutdown()
{
    if (gBridge.IsGraphicsInitialized() && gHasRenderAdapter && gRenderAdapter.graphicsShutdown != nullptr)
    {
        gRenderAdapter.graphicsShutdown(gRenderAdapter.userData);
    }
    gBridge.Shutdown();
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GlobalWork(int enabled)
{
    gBridge.GlobalWork(enabled == 1, 9000);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetRenderAdapter(const ImmGodotRenderAdapter *adapter)
{
    if (adapter == nullptr)
    {
        ImmGodot_ClearRenderAdapter();
        return;
    }

    gRenderAdapter = *adapter;
    gHasRenderAdapter = true;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_ClearRenderAdapter()
{
    gRenderAdapter = {};
    gHasRenderAdapter = false;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetMatrixDebugLogging(int enabled)
{
    gMatrixDebugLogging = (enabled != 0);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetCameraMatrices(int cameraID,
                                                           int stereoType,
                                                           const float *world2head,
                                                           const float *prjHead,
                                                           const float *world2leftEye,
                                                           const float *prjLeft,
                                                           const float *world2rightEye,
                                                           const float *prjRight)
{
    if (!iBackendReady())
        return;

    if (gMatrixDebugLogging && gBridge.GetLog() != nullptr)
    {
        gBridge.GetLog()->Printf(LT_DEBUG, L"ImmGodot_SetCameraMatrices camera=%d stereo=%d", cameraID, stereoType);
        iLogSubmittedMatrix(L"world2head", world2head);
        iLogSubmittedMatrix(L"prjHead", prjHead);
        iLogSubmittedMatrix(L"world2leftEye", world2leftEye);
        iLogSubmittedMatrix(L"prjLeft", prjLeft);
        iLogSubmittedMatrix(L"world2rightEye", world2rightEye);
        iLogSubmittedMatrix(L"prjRight", prjRight);
    }

    const mat4x4 head = (world2head != nullptr) ? iHostToPilibs(world2head) : mat4x4();
    const mat4x4 left = (world2leftEye != nullptr) ? iHostToPilibs(world2leftEye) : mat4x4();
    const mat4x4 right = (world2rightEye != nullptr) ? iHostToPilibs(world2rightEye) : mat4x4();
    const mat4x4 headPrj = (prjHead != nullptr) ? iHostToPilibs(prjHead) : mat4x4();
    const mat4x4 leftPrj = (prjLeft != nullptr) ? iHostToPilibs(prjLeft) : mat4x4();
    const mat4x4 rightPrj = (prjRight != nullptr) ? iHostToPilibs(prjRight) : mat4x4();

    gBridge.SetCameraMatrices(cameraID,
                              stereoType,
                              (world2head != nullptr) ? &head : nullptr,
                              (prjHead != nullptr) ? &headPrj : nullptr,
                              (world2leftEye != nullptr) ? &left : nullptr,
                              (prjLeft != nullptr) ? &leftPrj : nullptr,
                              (world2rightEye != nullptr) ? &right : nullptr,
                              (prjRight != nullptr) ? &rightPrj : nullptr);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_RenderCamera(int cameraID,
                                                     int eyeID,
                                                     float viewportX,
                                                     float viewportY,
                                                     float viewportWidth,
                                                     float viewportHeight,
                                                     float minDepth,
                                                     float maxDepth)
{
    if (!iBackendReady())
        return -1;

    const ImmShared::ImmEngineBridge::ViewportInfo viewport = {
        viewportX, viewportY, viewportWidth, viewportHeight, minDepth, maxDepth, true
    };

    const ImmGodotViewport adapterViewport = {
        viewportX, viewportY, viewportWidth, viewportHeight, minDepth, maxDepth
    };

    if (gHasRenderAdapter && gRenderAdapter.beforeRenderCamera != nullptr)
    {
        const int adapterResult = gRenderAdapter.beforeRenderCamera(gRenderAdapter.userData, cameraID, eyeID, &adapterViewport);
        if (adapterResult != 0)
        {
            if (gRenderAdapter.afterRenderCamera != nullptr)
            {
                gRenderAdapter.afterRenderCamera(gRenderAdapter.userData, cameraID, eyeID, &adapterViewport, adapterResult);
            }
            return adapterResult;
        }
    }

    const int renderResult = gBridge.RenderCamera(cameraID, viewport, eyeID, true) ? 0 : -1;
    if (gHasRenderAdapter && gRenderAdapter.afterRenderCamera != nullptr)
    {
        gRenderAdapter.afterRenderCamera(gRenderAdapter.userData, cameraID, eyeID, &adapterViewport, renderResult);
    }
    return renderResult;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_LoadFromFile(const char *fileName)
{
    if (!iBackendReady() || fileName == nullptr || fileName[0] == '\0')
        return -1;

    wchar_t *wideFileName = pistr2ws(fileName);
    if (wideFileName == nullptr)
        return -1;

    const int documentId = iPlayer().Load(wideFileName);
    std::free(wideFileName);
    return documentId;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Unload(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Unload(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetDocumentToWorld(int id, const float *doc2world)
{
    if (!iBackendReady() || doc2world == nullptr)
        return;

    iPlayer().SetDocumentToWorld(id, fromMatrix(f2d(iHostToPilibs(doc2world)) * mat4x4d::flipZ()));
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Pause(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Pause(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Resume(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Resume(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Restart(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Restart(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Show(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Show(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Hide(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Hide(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Continue(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().Continue(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SkipForward(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().SkipForward(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SkipBack(int id)
{
    if (!iBackendReady())
        return;

    iPlayer().SkipBack(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetChapterCount(int id)
{
    if (!iBackendReady())
        return 0;

    return iPlayer().GetChapterCount(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetCurrentChapter(int id)
{
    if (!iBackendReady())
        return 0;

    return iPlayer().GetCurrentChapter(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetChapter(int id, int chapterIndex)
{
    if (!iBackendReady())
        return;

    iPlayer().SetChapter(id, chapterIndex);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetVolume(int id, float volume)
{
    if (!iBackendReady())
        return;

    iPlayer().SetDocumentVolume(id, volume);
}

extern "C" IMMGODOT_EXPORT float ImmGodot_GetVolume(int id)
{
    if (!iBackendReady())
        return 0.0f;

    return iPlayer().GetDocumentVolume(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GetDocumentState(int id, ImmGodotDocumentState *state)
{
    if (state == nullptr)
        return;

    *state = {};
    if (!iBackendReady())
        return;

    Player::DocumentState playerState = {};
    iPlayer().GetDocumentState(playerState, id);
    state->loadingState = static_cast<int>(playerState.mLoadingState);
    state->playbackState = static_cast<int>(playerState.mPlaybackState);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GetBoundingBox(int id, ImmGodotBounds *bounds)
{
    if (bounds == nullptr)
        return;

    *bounds = {};
    if (!iBackendReady())
        return;

    const bound3 bbox = d2f(iPlayer().GetDocumentBBox(id));
    bounds->minX = bbox.mMinX;
    bounds->maxX = bbox.mMaxX;
    bounds->minY = bbox.mMinY;
    bounds->maxY = bbox.mMaxY;
    bounds->minZ = bbox.mMinZ;
    bounds->maxZ = bbox.mMaxZ;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaCount(int docId)
{
    if (!iBackendReady())
        return 0;

    return iPlayer().GetSpawnAreaCount(docId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaList(int docId, int spawnAreaIdsSize, int *spawnAreaIds)
{
    if (!iBackendReady())
        return 0;

    const int num = iPlayer().GetSpawnAreaCount(docId);
    if (spawnAreaIds == nullptr || spawnAreaIdsSize <= 0)
        return num;

    const int writeCount = (num < spawnAreaIdsSize) ? num : spawnAreaIdsSize;
    for (int i = 0; i < writeCount; ++i)
    {
        spawnAreaIds[i] = i;
    }
    return num;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetActiveSpawnAreaId(int docId)
{
    if (!iBackendReady())
        return -1;

    return iPlayer().GetSpawnArea(docId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetInitialSpawnAreaId(int docId)
{
    if (!iBackendReady())
        return -1;

    return iPlayer().GetInitialSpawnArea(docId);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetActiveSpawnAreaId(int docId, int spawnAreaId)
{
    if (!iBackendReady())
        return;

    iPlayer().SetSpawnArea(docId, spawnAreaId);
}

extern "C" IMMGODOT_EXPORT bool ImmGodot_GetSpawnAreaInfo(int docId, int spawnAreaId, ImmGodotSpawnArea *serializedSpawnArea)
{
    if (serializedSpawnArea == nullptr)
        return false;

    *serializedSpawnArea = {};
    if (!iBackendReady())
        return false;

    const int spawnAreaCount = iPlayer().GetSpawnAreaCount(docId);
    if (spawnAreaId < 0 || spawnAreaId >= spawnAreaCount)
        return false;

    Document::SpawnAreaInfo spawnAreaInfo;
    if (!iPlayer().GetSpawnAreaInfo(spawnAreaInfo, docId, spawnAreaId))
        return false;

    char *spawnAreaName = piws2str(spawnAreaInfo.mName);
    if (spawnAreaName != nullptr)
    {
        std::strncpy(serializedSpawnArea->name, spawnAreaName, sizeof(serializedSpawnArea->name) - 1);
        serializedSpawnArea->name[sizeof(serializedSpawnArea->name) - 1] = '\0';
        std::free(spawnAreaName);
    }
    serializedSpawnArea->version = spawnAreaInfo.mVersion;
    serializedSpawnArea->type = spawnAreaInfo.mIsFloorLevel ? IMM_GODOT_SPAWN_AREA_FLOOR_LEVEL : IMM_GODOT_SPAWN_AREA_EYE_LEVEL;
    serializedSpawnArea->animated = spawnAreaInfo.mAnimated;

    const trans3d transform = spawnAreaInfo.mSpawnAreaToWorld;
    serializedSpawnArea->transform.posx = static_cast<float>(transform.mTranslation.x);
    serializedSpawnArea->transform.posy = static_cast<float>(transform.mTranslation.y);
    serializedSpawnArea->transform.posz = static_cast<float>(transform.mTranslation.z);
    serializedSpawnArea->transform.rotx = static_cast<float>(transform.mRotation.x);
    serializedSpawnArea->transform.roty = static_cast<float>(transform.mRotation.y);
    serializedSpawnArea->transform.rotz = static_cast<float>(transform.mRotation.z);
    serializedSpawnArea->transform.rotw = static_cast<float>(transform.mRotation.w);
    serializedSpawnArea->transform.sca = static_cast<float>(transform.mScale);

    switch (spawnAreaInfo.mVolume.mType)
    {
    case ImmImporter::LayerSpawnArea::Volume::Type::Sphere:
    {
        serializedSpawnArea->volume.type = IMM_GODOT_SPAWN_VOLUME_SPHERE;
        const vec4 sphere = spawnAreaInfo.mVolume.mShape.mSphere;
        serializedSpawnArea->volume.offset.x = sphere.x;
        serializedSpawnArea->volume.offset.y = sphere.y;
        serializedSpawnArea->volume.offset.z = sphere.z;
        serializedSpawnArea->volume.sphereExtent.r = sphere.w;
    } break;
    case ImmImporter::LayerSpawnArea::Volume::Type::Box:
    {
        serializedSpawnArea->volume.type = IMM_GODOT_SPAWN_VOLUME_BOX;
        const vec3 center = getcenter(spawnAreaInfo.mVolume.mShape.mBox);
        const vec3 extent = getradiius(spawnAreaInfo.mVolume.mShape.mBox);
        serializedSpawnArea->volume.offset.x = center.x;
        serializedSpawnArea->volume.offset.y = center.y;
        serializedSpawnArea->volume.offset.z = center.z;
        serializedSpawnArea->volume.boxExtent.x = extent.x;
        serializedSpawnArea->volume.boxExtent.y = extent.y;
        serializedSpawnArea->volume.boxExtent.z = extent.z;
    } break;
    default:
        return false;
    }

    serializedSpawnArea->locomotion =
        (((spawnAreaInfo.mVolume.mAllowTranslationX ? 1 : 0) << 2) |
         ((spawnAreaInfo.mVolume.mAllowTranslationY ? 1 : 0) << 1) |
         ((spawnAreaInfo.mVolume.mAllowTranslationZ ? 1 : 0) << 0));

    return true;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GetPlayerInfo(ImmGodotPlayerInfo *info)
{
    if (info == nullptr)
        return;

    *info = {};
    if (!iBackendReady())
        return;

    Player::PlayerInfo playerInfo = {};
    iPlayer().GetPlayerInfo(playerInfo);
    info->backgroundColor.red = playerInfo.mBackgrundColor.mRed;
    info->backgroundColor.green = playerInfo.mBackgrundColor.mGreen;
    info->backgroundColor.blue = playerInfo.mBackgrundColor.mBlue;
}
