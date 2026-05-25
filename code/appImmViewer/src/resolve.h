#pragma once

#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libRender/piRenderer.h"
#include "libImmCore/src/libBasics/piTypes.h"

namespace ExePlayer
{
    class Resolve
    {
    private:
        ImmCore::piRasterState mRenderStateResolve = nullptr;
        ImmCore::piShader mAAResolveShader = nullptr;
        ImmCore::piBlendState mBlendStateNone = nullptr;
        int mFadeLoc = 0;
        int mOffsetLoc = 1;
        int mTexLoc = 0;
        bool mExplicitUniforms = true;

    public:

        bool Init(ImmCore::piRenderer* renderer, int superSample, int msaaSamples);
        void DeInit(ImmCore::piRenderer* renderer);
        void Do(ImmCore::piRenderer* renderer, ImmCore::piRTarget target, const int *vp, const int unXOffset, const float fade, ImmCore::piTexture colorTextureM);
    };
}
