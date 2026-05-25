#include "appImmShared/src/imm_engine_bridge.h"
#include "imm_godot_plugin.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmImporter/src/document/layerSpawnArea.h"

using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

namespace
{
    ImmShared::ImmEngineBridge gBridge;

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

}

extern "C" IMMGODOT_EXPORT int ImmGodot_Init(int colorSpace,
                                             int antialiasing,
                                             char *logFileName,
                                             char *tmpFolderName)
{
    ImmShared::ImmEngineBridge::InitConfig config = {};
    config.colorSpace = colorSpace;
    config.antialiasing = antialiasing;
    config.logFileName = logFileName;
    config.tmpFolderName = tmpFolderName;
    config.rendererApi = piRenderer::API::GL;
    config.initializeRendererOnInit = true;
    config.initializeFullscreen = true;
    return gBridge.Init(config) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Shutdown()
{
    gBridge.Shutdown();
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GlobalWork(int enabled)
{
    gBridge.GlobalWork(enabled == 1, 9000);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetCameraMatrices(int cameraID,
                                                           int stereoType,
                                                           float *world2head,
                                                           float *prjHead,
                                                           float *world2leftEye,
                                                           float *prjLeft,
                                                           float *world2rightEye,
                                                           float *prjRight)
{
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
    const ImmShared::ImmEngineBridge::ViewportInfo viewport = {
        viewportX, viewportY, viewportWidth, viewportHeight, minDepth, maxDepth, true
    };
    return gBridge.RenderCamera(cameraID, viewport, eyeID, true) ? 0 : -1;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_LoadFromFile(char *fileName)
{
    if (fileName == nullptr || fileName[0] == '\0')
        return -1;
    return iPlayer().Load(pistr2ws(fileName));
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Unload(int id)
{
    iPlayer().Unload(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetDocumentToWorld(int id, float *doc2world)
{
    iPlayer().SetDocumentToWorld(id, fromMatrix(f2d(iHostToPilibs(doc2world)) * mat4x4d::flipZ()));
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Pause(int id)
{
    iPlayer().Pause(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Resume(int id)
{
    iPlayer().Resume(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Restart(int id)
{
    iPlayer().Restart(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Show(int id)
{
    iPlayer().Show(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Hide(int id)
{
    iPlayer().Hide(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_Continue(int id)
{
    iPlayer().Continue(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SkipForward(int id)
{
    iPlayer().SkipForward(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SkipBack(int id)
{
    iPlayer().SkipBack(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetChapterCount(int id)
{
    return iPlayer().GetChapterCount(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetCurrentChapter(int id)
{
    return iPlayer().GetCurrentChapter(id);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetChapter(int id, int chapterIndex)
{
    iPlayer().SetChapter(id, chapterIndex);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetVolume(int id, float volume)
{
    iPlayer().SetDocumentVolume(id, volume);
}

extern "C" IMMGODOT_EXPORT float ImmGodot_GetVolume(int id)
{
    return iPlayer().GetDocumentVolume(id);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaCount(int docId)
{
    return iPlayer().GetSpawnAreaCount(docId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetSpawnAreaList(int docId, int spawnAreaIdsSize, int *spawnAreaIds)
{
    const int num = iPlayer().GetSpawnAreaCount(docId);
    piAssert(num <= spawnAreaIdsSize);
    for (int i = 0; i < num; ++i)
    {
        spawnAreaIds[i] = i;
    }
    return num;
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetActiveSpawnAreaId(int docId)
{
    return iPlayer().GetSpawnArea(docId);
}

extern "C" IMMGODOT_EXPORT int ImmGodot_GetInitialSpawnAreaId(int docId)
{
    return iPlayer().GetInitialSpawnArea(docId);
}

extern "C" IMMGODOT_EXPORT void ImmGodot_SetActiveSpawnAreaId(int docId, int spawnAreaId)
{
    iPlayer().SetSpawnArea(docId, spawnAreaId);
}

extern "C" IMMGODOT_EXPORT bool ImmGodot_GetSpawnAreaInfo(int docId, int spawnAreaId, ImmGodotSpawnArea &serializedSpawnArea)
{
    Document::SpawnAreaInfo spawnAreaInfo;
    if (!iPlayer().GetSpawnAreaInfo(spawnAreaInfo, docId, spawnAreaId))
        return false;

    serializedSpawnArea.name = piws2str(spawnAreaInfo.mName);
    serializedSpawnArea.version = spawnAreaInfo.mVersion;
    serializedSpawnArea.type = spawnAreaInfo.mIsFloorLevel ? ImmGodotSpawnArea::FloorLevel : ImmGodotSpawnArea::EyeLevel;
    serializedSpawnArea.animated = spawnAreaInfo.mAnimated;

    const trans3d transform = spawnAreaInfo.mSpawnAreaToWorld;
    serializedSpawnArea.transform.posx = static_cast<float>(transform.mTranslation.x);
    serializedSpawnArea.transform.posy = static_cast<float>(transform.mTranslation.y);
    serializedSpawnArea.transform.posz = static_cast<float>(transform.mTranslation.z);
    serializedSpawnArea.transform.rotx = static_cast<float>(transform.mRotation.x);
    serializedSpawnArea.transform.roty = static_cast<float>(transform.mRotation.y);
    serializedSpawnArea.transform.rotz = static_cast<float>(transform.mRotation.z);
    serializedSpawnArea.transform.rotw = static_cast<float>(transform.mRotation.w);
    serializedSpawnArea.transform.sca = static_cast<float>(transform.mScale);

    switch (spawnAreaInfo.mVolume.mType)
    {
    case ImmImporter::LayerSpawnArea::Volume::Type::Sphere:
    {
        serializedSpawnArea.volume.type = ImmGodotSpawnArea::Volume::Sphere;
        const vec4 sphere = spawnAreaInfo.mVolume.mShape.mSphere;
        serializedSpawnArea.volume.offset.x = sphere.x;
        serializedSpawnArea.volume.offset.y = sphere.y;
        serializedSpawnArea.volume.offset.z = sphere.z;
        serializedSpawnArea.volume.sphereExtent.r = sphere.w;
    } break;
    case ImmImporter::LayerSpawnArea::Volume::Type::Box:
    {
        serializedSpawnArea.volume.type = ImmGodotSpawnArea::Volume::Box;
        const vec3 center = getcenter(spawnAreaInfo.mVolume.mShape.mBox);
        const vec3 extent = getradiius(spawnAreaInfo.mVolume.mShape.mBox);
        serializedSpawnArea.volume.offset.x = center.x;
        serializedSpawnArea.volume.offset.y = center.y;
        serializedSpawnArea.volume.offset.z = center.z;
        serializedSpawnArea.volume.boxExtent.x = extent.x;
        serializedSpawnArea.volume.boxExtent.y = extent.y;
        serializedSpawnArea.volume.boxExtent.z = extent.z;
    } break;
    default:
        return false;
    }

    serializedSpawnArea.locomotion =
        (((spawnAreaInfo.mVolume.mAllowTranslationX ? 1 : 0) << 2) |
         ((spawnAreaInfo.mVolume.mAllowTranslationY ? 1 : 0) << 1) |
         ((spawnAreaInfo.mVolume.mAllowTranslationZ ? 1 : 0) << 0));

    return true;
}

extern "C" IMMGODOT_EXPORT void ImmGodot_GetPlayerInfo(Player::PlayerInfo &info)
{
    iPlayer().GetPlayerInfo(info);
}
