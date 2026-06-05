#include "resolve.h"
#include <stdio.h>
static const char* vsShaderAAResolve = ""

"layout(location = 0) in vec2 inVertex;"

"out V2FData"
"{"
	"vec2 uv;"
"}vf;"

"void main()"
"{"
	"vf.uv = inVertex;"
	"gl_Position = vec4(inVertex, 0.0, 1.0);"
"}";

static const char* fsShaderAAResolve = ""

#if EXPLICIT_UNIFORMS
"layout(binding = 0) uniform sampler2DMS unTex0;"
"layout(location = 0) uniform vec4 unFade;"
"layout(location = 1) uniform int  unXOffset;"
#else
"uniform sampler2DMS unTex0;"
"uniform vec4 unFade;"
"uniform int  unXOffset;"
#endif
"layout(location = 0, index = 0) out vec4 outColor;"


"vec3 linear2srgb(vec3 val)"
"{"
	"if (val.x < 0.0031308) val.x *= 12.92; else val.x = 1.055*pow(val.x, 1.0 / 2.4) - 0.055;"
	"if (val.y < 0.0031308) val.y *= 12.92; else val.y = 1.055*pow(val.y, 1.0 / 2.4) - 0.055;"
	"if (val.z < 0.0031308) val.z *= 12.92; else val.z = 1.055*pow(val.z, 1.0 / 2.4) - 0.055;"
	"return val;"
"}\n"

"void main(void)"
"{"
    "ivec2 p = ivec2(gl_FragCoord.xy);"
    "p.x += unXOffset;"

	"\n#if SS>1\n"
	"vec3 col = vec3(0.0);"
	"for (int k = 0; k<SS; k++)"
	"for (int j = 0; j<SS; j++)"
	"for (int i = 0; i<8; i++)"
	    "col += texelFetch(unTex0, SS*p + ivec2(j, k), i).xyz;"
	"col /= (8.0*float(SS*SS));"
	"\n#else\n"
	"vec3 col = vec3(0.0);"
	"for (int i = 0; i<8; i++)"
	    "col += texelFetch(unTex0, p, i).xyz;"
	"col /= 8.0;"
	"\n#endif\n"

    //"col = pow(col, vec3(0.4545));"
	// this would NOT be necessary if we had glEnable(GL_FRAMEBUFFER_SRGB); , which we don't
    "\n#if OUTPUT_ENCODING==1\n"
	"col = linear2srgb(col);"
    "\n#endif\n"

	"col *= unFade.x;"

"outColor = vec4(col, 1.0);"
"}";

static const char* fsShaderResolve = ""

#if EXPLICIT_UNIFORMS
"layout(binding = 0) uniform sampler2D unTex0;"
"layout(location = 0) uniform vec4 unFade;"
"layout(location = 1) uniform int  unXOffset;"
#else
"uniform sampler2D unTex0;"
"uniform vec4 unFade;"
"uniform int  unXOffset;"
#endif
"layout(location = 0, index = 0) out vec4 outColor;"

"vec3 linear2srgb(vec3 val)"
"{"
    "if (val.x < 0.0031308) val.x *= 12.92; else val.x = 1.055*pow(val.x, 1.0 / 2.4) - 0.055;"
    "if (val.y < 0.0031308) val.y *= 12.92; else val.y = 1.055*pow(val.y, 1.0 / 2.4) - 0.055;"
    "if (val.z < 0.0031308) val.z *= 12.92; else val.z = 1.055*pow(val.z, 1.0 / 2.4) - 0.055;"
    "return val;"
"}\n"

"void main(void)"
"{"
    "ivec2 p = ivec2(gl_FragCoord.xy);"
    "p.x += unXOffset;"
    "vec3 col = texelFetch(unTex0, p, 0).xyz;"
    "\n#if OUTPUT_ENCODING==1\n"
    "col = linear2srgb(col);"
    "\n#endif\n"
    "col *= unFade.x;"
    "outColor = vec4(col, 1.0);"
"}";

#if !defined(ANDROID)
#include "tmp/shader_resolve_vs_hlsl.inc"
#include "tmp/shader_resolve_fs_hlsl.inc"
#endif
using namespace ImmCore;

namespace ExePlayer
{
    bool Resolve::Init(piRenderer* renderer, int superSample, int msaaSamples, OutputEncoding outputEncoding)
    {
        mExplicitUniforms = true;
#if defined(__APPLE__)
        mExplicitUniforms = false;
#endif
        if (renderer->GetAPI() == piRenderer::API::Metal)
        {
            mExplicitUniforms = true;
        }
        mRenderStateResolve = renderer->CreateRasterState(false, true, piRenderer::CullMode::NONE, false, false);
        if (!mRenderStateResolve)
            return false;

        mBlendStateNone = renderer->CreateBlendState(false, false);
        if (!mBlendStateNone)
            return false;

        const piShaderOptions ops = {
            3,
            {
                { "SS", superSample },
                { "EXPLICIT_UNIFORMS", mExplicitUniforms ? 1 : 0 },
                { "OUTPUT_ENCODING", static_cast<int>(outputEncoding) }
            }
        };
        char error[2048];
        if (renderer->GetAPI() == piRenderer::API::GL || renderer->GetAPI() == piRenderer::API::GLES)
        {
            const char *fs = (msaaSamples > 1) ? fsShaderAAResolve : fsShaderResolve;
            mAAResolveShader = renderer->CreateShader(&ops, vsShaderAAResolve, nullptr, nullptr, nullptr, fs, error);
        }
        else
        {
#if !defined(ANDROID)
            const int fsIndex = outputEncoding == OutputEncoding::DisplaySrgb ? 2 : 0;
            mAAResolveShader = renderer->CreateShaderBinary(&ops, shader_resolve_vs_code[0], shader_resolve_vs_size[0],
                nullptr, 0, nullptr, 0, nullptr, 0, shader_resolve_fs_code[fsIndex], shader_resolve_fs_size[fsIndex], error);
#endif
        }
        if (!mAAResolveShader)
        {
            fprintf(stderr, "Resolve shader compile failed: %s\n", error);
            return false;
        }

        if (!mExplicitUniforms)
        {
            mFadeLoc = renderer->GetShaderUniformLocation(mAAResolveShader, "unFade");
            mOffsetLoc = renderer->GetShaderUniformLocation(mAAResolveShader, "unXOffset");
            mTexLoc = renderer->GetShaderUniformLocation(mAAResolveShader, "unTex0");
            if (mFadeLoc < 0 || mOffsetLoc < 0 || mTexLoc < 0)
                return false;
        }

        return true;
    }

    void Resolve::DeInit(piRenderer* renderer)
    {
        renderer->DestroyRasterState(mRenderStateResolve);
        mRenderStateResolve = nullptr;
        renderer->DestroyBlendState(mBlendStateNone);
        mBlendStateNone = nullptr;
        renderer->DestroyShader(mAAResolveShader);
        mAAResolveShader = nullptr;
    }

    void Resolve::Do(piRenderer* renderer, piRTarget target, const int *vp, const int unXOffset, const float fade, piTexture colorTextureM)
    {
        renderer->SetRenderTarget(target);
        renderer->SetViewport(0, vp);

        renderer->SetShadingSamples(1);

        if (renderer->GetAPI() == piRenderer::API::DX)
        {
            renderer->SetRasterState(mRenderStateResolve);
            renderer->SetBlendState(mBlendStateNone);
        }
        else
        {
            renderer->SetWriteMask(true, false, false, false, false);
            renderer->SetState(piSTATE_BLEND, false);
            renderer->SetState(piSTATE_DEPTH_TEST, false);
            renderer->SetState(piSTATE_CULL_FACE, false);
            renderer->SetState(piSTATE_FRONT_FACE, false);
        }

        float data[4] = { fade, 0.0f, 0.0f, 0.0f };
        renderer->AttachShader(mAAResolveShader);
        const int fadeLoc = mExplicitUniforms ? 0 : mFadeLoc;
        const int offsetLoc = mExplicitUniforms ? 1 : mOffsetLoc;
        renderer->SetShaderConstant4F(fadeLoc, (float*)data, 1);
        renderer->SetShaderConstant1I(offsetLoc, &unXOffset, 1);
        if (!mExplicitUniforms)
        {
            const int texUnit = 0;
            renderer->SetShaderConstant1I(mTexLoc, &texUnit, 1);
        }
        renderer->AttachTextures(1, colorTextureM);
        renderer->DrawUnitQuad_XY(1);
        renderer->DettachTextures();
        renderer->DettachShader();
    }
}
