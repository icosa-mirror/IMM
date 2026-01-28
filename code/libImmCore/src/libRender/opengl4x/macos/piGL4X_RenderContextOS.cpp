//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <OpenGL/OpenGL.h>
#include "../piGL4X_RenderContext.h"

namespace ImmCore {

struct piGL4X_RenderContextOS
{
    CGLContextObj context;
};

int piGL4X_RenderContext::Create(const void **hwnd, int num, bool disableVSynch, bool doublebuffered, bool antialias, bool disableErrors)
{
    (void)hwnd;
    (void)num;
    (void)disableVSynch;
    (void)doublebuffered;
    (void)antialias;
    (void)disableErrors;
    return 0;
}

bool piGL4X_RenderContext::CreateFromCurrent(void)
{
    CGLContextObj ctx = CGLGetCurrentContext();
    if (!ctx)
        return false;
    piGL4X_RenderContextOS *me = new piGL4X_RenderContextOS();
    me->context = ctx;
    mData = (void*)me;
    return true;
}

int piGL4X_RenderContext::SetActiveWindow(int id)
{
    (void)id;
    return 0;
}

void piGL4X_RenderContext::Destroy(void)
{
}

void piGL4X_RenderContext::Enable(void)
{
}

void piGL4X_RenderContext::Disable(bool doSwapBuffers)
{
    (void)doSwapBuffers;
}

void piGL4X_RenderContext::SwapBuffers(void)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (me && me->context)
        CGLFlushDrawable(me->context);
}

void piGL4X_RenderContext::Delete(void)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (me)
        delete me;
    mData = 0;
}

}
