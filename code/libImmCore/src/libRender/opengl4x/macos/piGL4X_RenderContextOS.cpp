//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <OpenGL/OpenGL.h>
#include <stdlib.h>
#include "../piGL4X_RenderContext.h"

namespace ImmCore {

struct piGL4X_RenderContextOS
{
    int mNumWindows;
    CGLContextObj contexts[8];
    int mActualWindow;
    int mIsDoubleBuffered;
    CGLContextObj prevContext;
};

int piGL4X_RenderContext::Create(const void **hwnd, int num, bool disableVSynch, bool doublebuffered, bool antialias, bool disableErrors)
{
    (void)antialias;
    (void)disableErrors;

    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)malloc(sizeof(piGL4X_RenderContextOS));
    if (!me)
        return 0;
    mData = (void*)me;

    me->mNumWindows = num;
    me->mActualWindow = 0;
    me->mIsDoubleBuffered = doublebuffered ? 1 : 0;
    me->prevContext = 0;

    for (int i = 0; i < num; i++)
    {
        me->contexts[i] = (CGLContextObj)hwnd[i];
        if (!me->contexts[i])
            return 0;

        GLint swap = disableVSynch ? 0 : 1;
        CGLSetParameter(me->contexts[i], kCGLCPSwapInterval, &swap);
    }

    CGLSetCurrentContext(me->contexts[0]);
    return 1;
}

bool piGL4X_RenderContext::CreateFromCurrent(void)
{
    CGLContextObj ctx = CGLGetCurrentContext();
    if (!ctx)
        return false;
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)malloc(sizeof(piGL4X_RenderContextOS));
    if (!me)
        return false;
    me->mNumWindows = 1;
    me->contexts[0] = ctx;
    me->mActualWindow = 0;
    me->mIsDoubleBuffered = 1;
    me->prevContext = 0;
    mData = (void*)me;
    return true;
}

int piGL4X_RenderContext::SetActiveWindow(int id)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (!me || id < 0 || id >= me->mNumWindows)
        return 0;
    me->mActualWindow = id;
    CGLSetCurrentContext(me->contexts[id]);
    return 1;
}

void piGL4X_RenderContext::Destroy(void)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (!me)
        return;
    for (int i = 0; i < me->mNumWindows; i++)
        me->contexts[i] = 0;
}

void piGL4X_RenderContext::Enable(void)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (!me)
        return;
    me->prevContext = CGLGetCurrentContext();
    CGLSetCurrentContext(me->contexts[me->mActualWindow]);
}

void piGL4X_RenderContext::Disable(bool doSwapBuffers)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (!me)
        return;
    if (doSwapBuffers)
        CGLFlushDrawable(me->contexts[me->mActualWindow]);
    CGLSetCurrentContext(me->prevContext);
}

void piGL4X_RenderContext::SwapBuffers(void)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (!me)
        return;
    CGLFlushDrawable(me->contexts[me->mActualWindow]);
}

void piGL4X_RenderContext::Delete(void)
{
    piGL4X_RenderContextOS *me = (piGL4X_RenderContextOS*)mData;
    if (me)
        free(me);
    mData = 0;
}

}
