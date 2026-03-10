#include "appImmShared/src/imm_engine_bridge.h"
#include "libImmCore/src/libBasics/piStr.h"

using namespace ImmCore;
using namespace ImmPlayer;

#if defined(_WIN32)
#define IMMGODOT_EXPORT __declspec(dllexport)
#else
#define IMMGODOT_EXPORT
#endif

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

extern "C" IMMGODOT_EXPORT void ImmGodot_GetPlayerInfo(Player::PlayerInfo &info)
{
    iPlayer().GetPlayerInfo(info);
}
