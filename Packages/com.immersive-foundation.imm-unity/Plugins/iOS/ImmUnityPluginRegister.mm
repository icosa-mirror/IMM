#include "UnityInterface.h"

extern "C" void UnityPluginLoad(IUnityInterfaces *unityInterfaces);
extern "C" void UnityPluginUnload(void);

extern "C" void ImmUnityRegisterRenderingPlugin(void)
{
    static bool registered = false;
    if (!registered)
    {
        UnityRegisterRenderingPluginV5(UnityPluginLoad, UnityPluginUnload);
        registered = true;
    }
}
