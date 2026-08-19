//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015.
// See THIRD_PARTY_LICENSES.txt
//
#include "piMetal_Renderer.h"

#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ImmCore {

struct piShaderS
{
    MTLRenderPipelineDescriptor *descriptor = nil;
    id<MTLRenderPipelineState> pipelineColorWrite = nil;
    id<MTLRenderPipelineState> pipelineNoColorWrite = nil;
    id<MTLRenderPipelineState> pipelineSourceAlphaBlend = nil;
    id<MTLRenderPipelineState> pipelineTargetColorWrite = nil;
    id<MTLRenderPipelineState> pipelineTargetNoColorWrite = nil;
    id<MTLRenderPipelineState> pipelineTargetSourceAlphaBlend = nil;
    MTLPixelFormat pipelineTargetColorFormat = MTLPixelFormatInvalid;
    MTLPixelFormat pipelineTargetDepthFormat = MTLPixelFormatInvalid;
    bool requiresVertexBuffer = true;
};
struct piVertexArrayS
{
    piBuffer vertexBuffer[2] = { nullptr, nullptr };
    piBuffer indexBuffer = nullptr;
    piRenderer::IndexArrayFormat indexFormat = piRenderer::IndexArrayFormat::UINT_32;
};
struct piRTargetS
{
    piTexture color[4] = { nullptr, nullptr, nullptr, nullptr };
    piTexture depth = nullptr;
};

struct piSamplerS
{
    id<MTLSamplerState> sampler = nil;
};
struct piRasterStateS
{
    MTLWinding frontWinding = MTLWindingCounterClockwise;
    MTLCullMode cullMode = MTLCullModeNone;
    bool wireframe = false;
    bool depthClamp = false;
};
struct piBlendStateS
{
    bool alphaToCoverage = false;
    bool enabled0 = false;
};
struct piDepthStateS
{
    id<MTLDepthStencilState> depthWriteState = nil;
    id<MTLDepthStencilState> depthNoWriteState = nil;
    bool depthEnabled = false;
};
struct piQueryS
{
    piRenderer::QueryType type = piRenderer::QueryType::TimeElapsed;
    uint64_t startTicks = 0;
    uint64_t resultNanoseconds = 0;
    bool active = false;
};

struct piTextureS
{
    id<MTLTexture> texture = nil;
    piRenderer::TextureInfo info;
    piRenderer::TextureFilter filter;
    piRenderer::TextureWrap wrap;
};

struct piBufferS
{
    id<MTLBuffer> buffer = nil;
    unsigned int size = 0;
    piRenderer::BufferType type = piRenderer::BufferType::Static;
    piRenderer::BufferUse use = piRenderer::BufferUse::Vertex;
};

static constexpr NSUInteger kImmediateConstantBufferBase = 16;

enum class piMetalUnsupportedFeature : int
{
    DynamicBlending = 0,
    ExternalTexture,
    ImageLoadStore,
    Compute,
    Atomics,
    PixelPackBuffer,
    Query,
    PointSize,
    LineWidth,
    PolygonOffset,
    Count
};

struct piMetalState
{
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLDepthStencilState> disabledDepthState = nil;
    MTLRenderPassDescriptor *nativeRenderPass = nil;
    id<CAMetalDrawable> nativeDrawable = nil;
    id<MTLCommandBuffer> commandBuffer = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
    bool externalCommandEncoder = false;
    bool externalCommandBuffer = false;
    MTLRenderPassDescriptor *activeRenderPass = nil;
    piRTarget currentRenderTarget = nullptr;
    piShader currentShader = nullptr;
    piVertexArray currentVertexArray = nullptr;
    piRasterState currentRasterState = nullptr;
    piBlendState currentBlendState = nullptr;
    piDepthState currentDepthState = nullptr;
    piTexture fragmentTextures[16] = {};
    piSampler fragmentSamplers[8] = {};
    piBuffer constantBuffers[16] = {};
    piBuffer shaderBuffers[16] = {};
    id<MTLBuffer> immediateConstants[16] = {};
    piBuffer unitQuadBuffer = nullptr;
    piVertexArray unitQuadVertexArray = nullptr;
    piBuffer unitCubePositionBuffer = nullptr;
    piVertexArray unitCubePositionVertexArray = nullptr;
    piBuffer unitCubePositionNormalBuffer = nullptr;
    piVertexArray unitCubePositionNormalVertexArray = nullptr;
    piQuery perfQueries[2] = { nullptr, nullptr };
    NSMutableArray<id<MTLBuffer>> *retainedBuffers = nil;
    uint64_t liveRenderTargets = 0;
    uint64_t liveRasterStates = 0;
    uint64_t liveBlendStates = 0;
    uint64_t liveDepthStates = 0;
    uint64_t liveTextures = 0;
    uint64_t liveSamplers = 0;
    uint64_t liveShaders = 0;
    uint64_t liveBuffers = 0;
    uint64_t liveVertexArrays = 0;
    uint64_t liveQueries = 0;
    int currentPerformanceQuery = 0;
    bool frameActive = false;
    bool passTouched = false;
    bool depthTestEnabled = false;
    bool depthWriteEnabled = true;
    bool color0WriteEnabled = true;
    bool dynamicSourceAlphaBlendEnabled = false;
    bool cullFaceEnabled = true;
	    bool frontFaceCCW = true;
	    bool externalShaderAdjust = false;
    bool unityProjectionAdjusted = false;
    bool unsupportedReported[(int)piMetalUnsupportedFeature::Count] = {};
    int numViewports = 1;
    float viewports[6 * 16] = {};
    uint64_t debugIndexedDrawCalls = 0;
    uint64_t debugNonIndexedDrawCalls = 0;
    uint64_t debugSkippedDrawCalls = 0;
    uint64_t debugIssuedDrawCalls = 0;
};

static void iAttachRetainedBufferCleanup(piMetalState *state)
{
    if (!state || !state->commandBuffer || !state->retainedBuffers || state->retainedBuffers.count == 0)
    {
        return;
    }

    NSArray<id<MTLBuffer>> *buffers = [state->retainedBuffers copy];
    [state->commandBuffer addCompletedHandler:^(__unused id<MTLCommandBuffer> commandBuffer) {
        [buffers release];
    }];
    [state->retainedBuffers removeAllObjects];
}

static bool iMetalEnvFlagEnabled(const char *name)
{
    const char *value = getenv(name);
    return value != nullptr && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void iReport(piRenderer::piReporter *reporter, const char *message);

static void iResetDrawCounters(piMetalState *state)
{
    if (!state)
    {
        return;
    }
    state->debugIndexedDrawCalls = 0;
    state->debugNonIndexedDrawCalls = 0;
    state->debugSkippedDrawCalls = 0;
    state->debugIssuedDrawCalls = 0;
}

static void iReportDrawCounters(piMetalState *state, piRenderer::piReporter *reporter)
{
    if (!state || !iMetalEnvFlagEnabled("IMM_METAL_LOG_DRAW_COUNTERS"))
    {
        return;
    }
    char summary[256];
    snprintf(summary,
             sizeof(summary),
             "Metal draw counters: indexed=%llu nonIndexed=%llu issued=%llu skipped=%llu passTouched=%d",
             (unsigned long long)state->debugIndexedDrawCalls,
             (unsigned long long)state->debugNonIndexedDrawCalls,
             (unsigned long long)state->debugIssuedDrawCalls,
             (unsigned long long)state->debugSkippedDrawCalls,
             state->passTouched ? 1 : 0);
    fprintf(stderr, "%s\n", summary);
    iReport(reporter, summary);
}

static id<MTLCommandBuffer> iCreateOwnedCommandBuffer(id<MTLCommandQueue> queue)
{
    return queue ? [[queue commandBuffer] retain] : nil;
}

static void iReleaseOwnedCommandBuffer(id<MTLCommandBuffer> commandBuffer)
{
    [commandBuffer release];
}

static int iSuppressDrawCallsLevel(void)
{
    static int enabled = -1;
    if (enabled < 0)
    {
        const char *value = getenv("IMM_METAL_SUPPRESS_DRAWS");
        enabled = (value && value[0]) ? atoi(value) : 0;
        if (enabled < 0)
        {
            enabled = 0;
        }
    }
    return enabled;
}

static const char *iCommandBufferStatusName(MTLCommandBufferStatus status)
{
    switch (status)
    {
        case MTLCommandBufferStatusNotEnqueued: return "not-enqueued";
        case MTLCommandBufferStatusEnqueued: return "enqueued";
        case MTLCommandBufferStatusCommitted: return "committed";
        case MTLCommandBufferStatusScheduled: return "scheduled";
        case MTLCommandBufferStatusCompleted: return "completed";
        case MTLCommandBufferStatusError: return "error";
        default: return "unknown";
    }
}

static void iReportCommandBufferStatus(const char *label, id<MTLCommandBuffer> commandBuffer)
{
    if (!commandBuffer)
    {
        return;
    }

    NSError *error = commandBuffer.error;
    const char *errorText = error ? (error.localizedDescription.UTF8String ?: "unknown error") : "none";
    fprintf(stderr,
            "IMM_METAL_COMMAND_BUFFER %s status=%s error=%s\n",
            label ? label : "unknown",
            iCommandBufferStatusName(commandBuffer.status),
            errorText);
    fflush(stderr);
}

static void iReport(piRenderer::piReporter *reporter, const char *message)
{
    if (reporter)
    {
        reporter->Info(message);
    }
}

static void iError(piRenderer::piReporter *reporter, const char *message)
{
    if (reporter)
    {
        reporter->Error(message, 0);
    }
}

static void iUnsupported(piMetalState *state, piRenderer::piReporter *reporter, piMetalUnsupportedFeature feature, const char *message)
{
    const int index = (int)feature;
    if (!state || index < 0 || index >= (int)piMetalUnsupportedFeature::Count || state->unsupportedReported[index])
    {
        return;
    }
    state->unsupportedReported[index] = true;
    iError(reporter, message);
}

static uint64_t iMetalCpuTimeNanoseconds(void)
{
    static mach_timebase_info_data_t timebase = {};
    if (timebase.denom == 0)
    {
        mach_timebase_info(&timebase);
    }
    const uint64_t ticks = mach_absolute_time();
    return ticks * (uint64_t)timebase.numer / (uint64_t)timebase.denom;
}

static MTLPixelFormat iFormatPiToMetal(piRenderer::Format format)
{
    switch (format)
    {
        case piRenderer::Format::C4_8_UNORM: return MTLPixelFormatRGBA8Unorm;
        case piRenderer::Format::C4_8_UNORM_SRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
        case piRenderer::Format::C1_8_UNORM: return MTLPixelFormatR8Unorm;
        case piRenderer::Format::C4_16_FLOAT: return MTLPixelFormatRGBA16Float;
        case piRenderer::Format::C4_32_FLOAT: return MTLPixelFormatRGBA32Float;
        case piRenderer::Format::C3_11_11_10_FLOAT: return MTLPixelFormatRG11B10Float;
        case piRenderer::Format::D1_32_FLOAT: return MTLPixelFormatDepth32Float;
        case piRenderer::Format::D1_16_UNORM: return MTLPixelFormatDepth16Unorm;
        case piRenderer::Format::DS_24_8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
        case piRenderer::Format::DS_32_8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
        default: return MTLPixelFormatInvalid;
    }
}

static NSUInteger iBytesPerPixel(piRenderer::Format format)
{
    switch (format)
    {
        case piRenderer::Format::C1_8_UNORM: return 1;
        case piRenderer::Format::C4_8_UNORM:
        case piRenderer::Format::C4_8_UNORM_SRGB:
        case piRenderer::Format::C3_11_11_10_FLOAT:
            return 4;
        case piRenderer::Format::C4_16_FLOAT:
            return 8;
        case piRenderer::Format::C4_32_FLOAT:
            return 16;
        default:
            return 0;
    }
}

static void iEndEncoder(piMetalState *state)
{
    if (state->encoder)
    {
        [state->encoder endEncoding];
        state->encoder = nil;
    }
}

static void iApplyEncoderState(piMetalState *state)
{
    if (!state || !state->encoder)
    {
        return;
    }

    const float *vp = state->viewports;
    if (vp[2] > 0.0f && vp[3] > 0.0f)
    {
        MTLViewport viewport;
        viewport.originX = vp[0];
        viewport.originY = vp[1];
        viewport.width = vp[2];
        viewport.height = vp[3];
        viewport.znear = vp[4];
        viewport.zfar = vp[5];
        [state->encoder setViewport:viewport];
    }

    if (state->currentDepthState && state->depthTestEnabled)
    {
        [state->encoder setDepthStencilState:state->depthWriteEnabled ? state->currentDepthState->depthWriteState : state->currentDepthState->depthNoWriteState];
    }
    else
    {
        [state->encoder setDepthStencilState:state->disabledDepthState];
    }

    if (state->currentRasterState)
    {
        [state->encoder setFrontFacingWinding:state->frontFaceCCW ? MTLWindingCounterClockwise : MTLWindingClockwise];
        [state->encoder setCullMode:state->cullFaceEnabled ? state->currentRasterState->cullMode : MTLCullModeNone];
        [state->encoder setTriangleFillMode:state->currentRasterState->wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
        [state->encoder setDepthClipMode:state->currentRasterState->depthClamp ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
    }
}

static MTLRenderPassDescriptor *iRenderPassForTarget(piRTarget target)
{
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    if (target)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (target->color[i] && target->color[i]->texture)
            {
                pass.colorAttachments[i].texture = target->color[i]->texture;
                pass.colorAttachments[i].loadAction = MTLLoadActionLoad;
                pass.colorAttachments[i].storeAction = MTLStoreActionStore;
            }
        }
        if (target->depth && target->depth->texture)
        {
            pass.depthAttachment.texture = target->depth->texture;
            pass.depthAttachment.loadAction = MTLLoadActionLoad;
            pass.depthAttachment.storeAction = MTLStoreActionStore;
        }
    }
    return pass;
}

static MTLPrimitiveType iPrimitivePiToMetal(piRenderer::PrimitiveType pt)
{
    switch (pt)
    {
        case piRenderer::PrimitiveType::Triangle: return MTLPrimitiveTypeTriangle;
        case piRenderer::PrimitiveType::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        case piRenderer::PrimitiveType::Point: return MTLPrimitiveTypePoint;
        case piRenderer::PrimitiveType::Lines: return MTLPrimitiveTypeLine;
        case piRenderer::PrimitiveType::LineStrip: return MTLPrimitiveTypeLineStrip;
        default: return MTLPrimitiveTypeTriangle;
    }
}

static MTLIndexType iIndexPiToMetal(piRenderer::IndexArrayFormat format)
{
    return format == piRenderer::IndexArrayFormat::UINT_16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
}

static void iBindCommonDrawResources(piMetalState *state)
{
    for (int i = 0; i < 16; ++i)
    {
        if (state->constantBuffers[i] && state->constantBuffers[i]->buffer)
        {
            [state->encoder setVertexBuffer:state->constantBuffers[i]->buffer offset:0 atIndex:(NSUInteger)i];
            [state->encoder setFragmentBuffer:state->constantBuffers[i]->buffer offset:0 atIndex:(NSUInteger)i];
            if (i == 0)
            {
                [state->encoder setVertexBuffer:state->constantBuffers[i]->buffer offset:0 atIndex:6];
                [state->encoder setFragmentBuffer:state->constantBuffers[i]->buffer offset:0 atIndex:6];
            }
        }
        if (state->shaderBuffers[i] && state->shaderBuffers[i]->buffer)
        {
            [state->encoder setVertexBuffer:state->shaderBuffers[i]->buffer offset:0 atIndex:(NSUInteger)i];
            [state->encoder setFragmentBuffer:state->shaderBuffers[i]->buffer offset:0 atIndex:(NSUInteger)i];
        }
        if (state->immediateConstants[i])
        {
            const NSUInteger index = kImmediateConstantBufferBase + (NSUInteger)i;
            [state->encoder setVertexBuffer:state->immediateConstants[i] offset:0 atIndex:index];
            [state->encoder setFragmentBuffer:state->immediateConstants[i] offset:0 atIndex:index];
        }
        if (state->fragmentTextures[i] && state->fragmentTextures[i]->texture)
        {
            [state->encoder setFragmentTexture:state->fragmentTextures[i]->texture atIndex:(NSUInteger)i];
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        if (state->fragmentSamplers[i] && state->fragmentSamplers[i]->sampler)
        {
            [state->encoder setFragmentSamplerState:state->fragmentSamplers[i]->sampler atIndex:(NSUInteger)i];
        }
    }
}

static void iSetImmediateConstant(piMetalState *state, unsigned int pos, const void *value, size_t size)
{
    if (!state->device || !value || pos >= 16 || size == 0)
    {
        return;
    }
    if (!state->immediateConstants[pos] || [state->immediateConstants[pos] length] < size)
    {
        state->immediateConstants[pos] = [state->device newBufferWithLength:size options:MTLResourceStorageModeShared];
    }
    if (state->immediateConstants[pos])
    {
        memcpy([state->immediateConstants[pos] contents], value, size);
    }
}

static void iConfigureSourceAlphaBlend(MTLRenderPipelineColorAttachmentDescriptor *color)
{
    color.blendingEnabled = YES;
    color.rgbBlendOperation = MTLBlendOperationAdd;
    color.alphaBlendOperation = MTLBlendOperationAdd;
    color.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    color.sourceAlphaBlendFactor = MTLBlendFactorOne;
    color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
}

static void iGetActiveRenderPassFormats(piMetalState *state, piShader shader, MTLPixelFormat *colorFormat, MTLPixelFormat *depthFormat)
{
    *colorFormat = shader->descriptor.colorAttachments[0].pixelFormat;
    *depthFormat = shader->descriptor.depthAttachmentPixelFormat;
    if (!state || !state->activeRenderPass)
    {
        return;
    }

    id<MTLTexture> colorTexture = state->activeRenderPass.colorAttachments[0].texture;
    if (colorTexture)
    {
        *colorFormat = colorTexture.pixelFormat;
    }

    id<MTLTexture> depthTexture = state->activeRenderPass.depthAttachment.texture;
    if (depthTexture)
    {
        *depthFormat = depthTexture.pixelFormat;
    }
    else
    {
        *depthFormat = MTLPixelFormatInvalid;
    }
}

static id<MTLRenderPipelineState> iCreatePipelineForTarget(piMetalState *state,
                                                           piShader shader,
                                                           piRenderer::piReporter *reporter,
                                                           MTLPixelFormat colorFormat,
                                                           MTLPixelFormat depthFormat,
                                                           bool sourceAlphaBlend,
                                                           bool colorWrite)
{
    MTLRenderPipelineDescriptor *descriptor = [shader->descriptor copy];
    descriptor.colorAttachments[0].pixelFormat = colorFormat;
    descriptor.depthAttachmentPixelFormat = depthFormat;
    if (sourceAlphaBlend)
    {
        iConfigureSourceAlphaBlend(descriptor.colorAttachments[0]);
    }
    if (!colorWrite)
    {
        descriptor.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
    }

    NSError *pipelineError = nil;
    id<MTLRenderPipelineState> pipeline = [state->device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
    if (!pipeline)
    {
        const char *message = pipelineError.localizedDescription.UTF8String ?: "unknown error";
        fprintf(stderr,
                "Metal target pipeline creation failed: color=%lu depth=%lu sourceAlpha=%d colorWrite=%d error=%s\n",
                (unsigned long)colorFormat,
                (unsigned long)depthFormat,
                sourceAlphaBlend ? 1 : 0,
                colorWrite ? 1 : 0,
                message);
        iError(reporter, message);
    }
    return pipeline;
}

static id<MTLRenderPipelineState> iGetPipelineForCurrentState(piMetalState *state, piShader shader, piRenderer::piReporter *reporter)
{
    if (!state || !state->device || !shader)
    {
        return nil;
    }

    MTLPixelFormat targetColorFormat = MTLPixelFormatInvalid;
    MTLPixelFormat targetDepthFormat = MTLPixelFormatInvalid;
    iGetActiveRenderPassFormats(state, shader, &targetColorFormat, &targetDepthFormat);
    const bool targetFormatDiffers =
        targetColorFormat != shader->descriptor.colorAttachments[0].pixelFormat ||
        targetDepthFormat != shader->descriptor.depthAttachmentPixelFormat;
    if (targetFormatDiffers)
    {
        if (shader->pipelineTargetColorFormat != targetColorFormat || shader->pipelineTargetDepthFormat != targetDepthFormat)
        {
            shader->pipelineTargetColorWrite = nil;
            shader->pipelineTargetNoColorWrite = nil;
            shader->pipelineTargetSourceAlphaBlend = nil;
            shader->pipelineTargetColorFormat = targetColorFormat;
            shader->pipelineTargetDepthFormat = targetDepthFormat;
        }
        if (state->color0WriteEnabled)
        {
            if (state->dynamicSourceAlphaBlendEnabled)
            {
                if (!shader->pipelineTargetSourceAlphaBlend)
                {
                    shader->pipelineTargetSourceAlphaBlend = iCreatePipelineForTarget(state, shader, reporter, targetColorFormat, targetDepthFormat, true, true);
                }
                return shader->pipelineTargetSourceAlphaBlend;
            }
            if (!shader->pipelineTargetColorWrite)
            {
                shader->pipelineTargetColorWrite = iCreatePipelineForTarget(state, shader, reporter, targetColorFormat, targetDepthFormat, false, true);
            }
            return shader->pipelineTargetColorWrite;
        }

        if (!shader->pipelineTargetNoColorWrite)
        {
            shader->pipelineTargetNoColorWrite = iCreatePipelineForTarget(state, shader, reporter, targetColorFormat, targetDepthFormat, false, false);
        }
        return shader->pipelineTargetNoColorWrite;
    }

    if (state->color0WriteEnabled)
    {
        if (state->dynamicSourceAlphaBlendEnabled)
        {
            if (!shader->pipelineSourceAlphaBlend)
            {
                MTLRenderPipelineDescriptor *descriptor = [shader->descriptor copy];
                iConfigureSourceAlphaBlend(descriptor.colorAttachments[0]);
                NSError *pipelineError = nil;
                shader->pipelineSourceAlphaBlend = [state->device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
                if (!shader->pipelineSourceAlphaBlend)
                {
                    const char *message = pipelineError.localizedDescription.UTF8String ?: "unknown error";
                    fprintf(stderr, "Metal source-alpha pipeline creation failed: %s\n", message);
                    iError(reporter, message);
                }
            }
            return shader->pipelineSourceAlphaBlend;
        }
        return shader->pipelineColorWrite;
    }

    if (!shader->pipelineNoColorWrite)
    {
        MTLRenderPipelineDescriptor *descriptor = [shader->descriptor copy];
        descriptor.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
        NSError *pipelineError = nil;
        shader->pipelineNoColorWrite = [state->device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
        if (!shader->pipelineNoColorWrite)
        {
            const char *message = pipelineError.localizedDescription.UTF8String ?: "unknown error";
            fprintf(stderr, "Metal no-color-write pipeline creation failed: %s\n", message);
            iError(reporter, message);
        }
    }

    return shader->pipelineNoColorWrite;
}

static bool iGetShaderOption(const piShaderOptions *options, const char *name, int *value)
{
    if (!options || !name || !value)
    {
        return false;
    }
    for (int i = 0; i < options->mNum; ++i)
    {
        if (strcmp(options->mOption[i].mName, name) == 0)
        {
            *value = options->mOption[i].mValue;
            return true;
        }
    }
    return false;
}

struct piMetalDebugVertex
{
    float position[2];
    float color[4];
};

static const float kUnitCubePositionVertices[] = {
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f
};

static const float kUnitCubePositionNormalVertices[] = {
    -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
    -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
    -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
     1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
     1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
     1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
     1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
     1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
     1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
    -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
    -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
     1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
     1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
    -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
    -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
    -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
     1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
     1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f
};

piRendererMetal::piRendererMetal()
{
    mState = new piMetalState();
    mState->retainedBuffers = [[NSMutableArray alloc] init];
    mReporter = nullptr;
}

piRendererMetal::~piRendererMetal()
{
    Deinitialize();
    [mState->retainedBuffers release];
    mState->retainedBuffers = nil;
    delete mState;
}

bool piRendererMetal::Initialize(int, const void **, int, bool, bool, piReporter *reporter, bool, void *device)
{
    mReporter = reporter;
    mState->device = device ? (__bridge id<MTLDevice>)device : MTLCreateSystemDefaultDevice();
    if (!mState->device)
    {
        iError(mReporter, "Metal renderer could not acquire an MTLDevice");
        return false;
    }

    mState->commandQueue = [mState->device newCommandQueue];
    if (!mState->commandQueue)
    {
        iError(mReporter, "Metal renderer could not create an MTLCommandQueue");
        return false;
    }

    MTLDepthStencilDescriptor *disabledDepthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
    disabledDepthDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
    disabledDepthDescriptor.depthWriteEnabled = NO;
    mState->disabledDepthState = [mState->device newDepthStencilStateWithDescriptor:disabledDepthDescriptor];
    if (!mState->disabledDepthState)
    {
        iError(mReporter, "Metal renderer could not create a disabled depth state");
        return false;
    }

    mState->viewports[2] = 1.0f;
    mState->viewports[3] = 1.0f;
    mState->viewports[5] = 1.0f;

    const piMetalDebugVertex unitQuad[4] = {
        {{-1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ 1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    mState->unitQuadBuffer = CreateBuffer(unitQuad, sizeof(unitQuad), BufferType::Static, BufferUse::Vertex);
    if (mState->unitQuadBuffer)
    {
        piRArrayLayout layout = {};
        layout.mStride = sizeof(piMetalDebugVertex);
        layout.mNumElements = 2;
        layout.mEntry[0].mNumComponents = 2;
        layout.mEntry[0].mType = piRArrayType_Float;
        layout.mEntry[1].mNumComponents = 4;
        layout.mEntry[1].mType = piRArrayType_Float;
        mState->unitQuadVertexArray = CreateVertexArray(1, mState->unitQuadBuffer, &layout, nullptr, nullptr, nullptr, IndexArrayFormat::UINT_32);
    }

    if (!mState->unitQuadVertexArray)
    {
        iError(mReporter, "Metal renderer could not create auxiliary unit quad");
        return false;
    }

    mState->unitCubePositionBuffer = CreateBuffer(kUnitCubePositionVertices, sizeof(kUnitCubePositionVertices), BufferType::Static, BufferUse::Vertex);
    if (mState->unitCubePositionBuffer)
    {
        piRArrayLayout layout = {};
        layout.mStride = 3 * sizeof(float);
        layout.mNumElements = 1;
        layout.mEntry[0].mNumComponents = 3;
        layout.mEntry[0].mType = piRArrayType_Float;
        mState->unitCubePositionVertexArray = CreateVertexArray(1, mState->unitCubePositionBuffer, &layout, nullptr, nullptr, nullptr, IndexArrayFormat::UINT_32);
    }

    mState->unitCubePositionNormalBuffer = CreateBuffer(kUnitCubePositionNormalVertices, sizeof(kUnitCubePositionNormalVertices), BufferType::Static, BufferUse::Vertex);
    if (mState->unitCubePositionNormalBuffer)
    {
        piRArrayLayout layout = {};
        layout.mStride = 6 * sizeof(float);
        layout.mNumElements = 2;
        layout.mEntry[0].mNumComponents = 3;
        layout.mEntry[0].mType = piRArrayType_Float;
        layout.mEntry[1].mNumComponents = 3;
        layout.mEntry[1].mType = piRArrayType_Float;
        mState->unitCubePositionNormalVertexArray = CreateVertexArray(1, mState->unitCubePositionNormalBuffer, &layout, nullptr, nullptr, nullptr, IndexArrayFormat::UINT_32);
    }

    if (!mState->unitCubePositionVertexArray || !mState->unitCubePositionNormalVertexArray)
    {
        iError(mReporter, "Metal renderer could not create auxiliary unit cube geometry");
        return false;
    }

    mState->perfQueries[0] = CreateQuery(QueryType::TimeElapsed);
    mState->perfQueries[1] = CreateQuery(QueryType::TimeElapsed);
    if (!mState->perfQueries[0] || !mState->perfQueries[1])
    {
        iError(mReporter, "Metal renderer could not create performance timing queries");
        return false;
    }

    iReport(mReporter, "Metal renderer initialized");
    return true;
}

void piRendererMetal::Deinitialize(void)
{
    EndNativeFrame();
    if (mState->perfQueries[0])
    {
        DestroyQuery(mState->perfQueries[0]);
        mState->perfQueries[0] = nullptr;
    }
    if (mState->perfQueries[1])
    {
        DestroyQuery(mState->perfQueries[1]);
        mState->perfQueries[1] = nullptr;
    }
    if (mState->unitCubePositionNormalVertexArray)
    {
        DestroyVertexArray(mState->unitCubePositionNormalVertexArray);
        mState->unitCubePositionNormalVertexArray = nullptr;
    }
    if (mState->unitCubePositionNormalBuffer)
    {
        DestroyBuffer(mState->unitCubePositionNormalBuffer);
        mState->unitCubePositionNormalBuffer = nullptr;
    }
    if (mState->unitCubePositionVertexArray)
    {
        DestroyVertexArray(mState->unitCubePositionVertexArray);
        mState->unitCubePositionVertexArray = nullptr;
    }
    if (mState->unitCubePositionBuffer)
    {
        DestroyBuffer(mState->unitCubePositionBuffer);
        mState->unitCubePositionBuffer = nullptr;
    }
    if (mState->unitQuadVertexArray)
    {
        DestroyVertexArray(mState->unitQuadVertexArray);
        mState->unitQuadVertexArray = nullptr;
    }
    if (mState->unitQuadBuffer)
    {
        DestroyBuffer(mState->unitQuadBuffer);
        mState->unitQuadBuffer = nullptr;
    }
    char resourceSummary[512];
    snprintf(resourceSummary,
             sizeof(resourceSummary),
             "Metal renderer resource cleanup: renderTargets=%llu rasterStates=%llu blendStates=%llu depthStates=%llu textures=%llu samplers=%llu shaders=%llu buffers=%llu vertexArrays=%llu queries=%llu retainedBuffers=%lu",
             (unsigned long long)mState->liveRenderTargets,
             (unsigned long long)mState->liveRasterStates,
             (unsigned long long)mState->liveBlendStates,
             (unsigned long long)mState->liveDepthStates,
             (unsigned long long)mState->liveTextures,
             (unsigned long long)mState->liveSamplers,
             (unsigned long long)mState->liveShaders,
             (unsigned long long)mState->liveBuffers,
             (unsigned long long)mState->liveVertexArrays,
             (unsigned long long)mState->liveQueries,
             (unsigned long)(mState->retainedBuffers ? mState->retainedBuffers.count : 0));
    iReport(mReporter, resourceSummary);
    mState->commandQueue = nil;
    mState->disabledDepthState = nil;
    mState->device = nil;
}

bool piRendererMetal::BeginNativeFrame(void *renderPassDescriptor, void *drawable)
{
    EndNativeFrame();

    mState->nativeRenderPass = (__bridge MTLRenderPassDescriptor *)renderPassDescriptor;
    mState->nativeDrawable = (__bridge id<CAMetalDrawable>)drawable;
    if (!mState->nativeRenderPass || !mState->nativeDrawable || !mState->commandQueue)
    {
        iError(mReporter, "Metal native frame is missing render pass, drawable, or command queue");
        mState->nativeRenderPass = nil;
        mState->nativeDrawable = nil;
        return false;
    }

    mState->commandBuffer = iCreateOwnedCommandBuffer(mState->commandQueue);
    if (!mState->commandBuffer)
    {
        iError(mReporter, "Metal native frame could not create command buffer");
        mState->nativeRenderPass = nil;
        mState->nativeDrawable = nil;
        return false;
    }

    mState->frameActive = true;
    mState->externalCommandEncoder = false;
    mState->externalCommandBuffer = false;
    mState->passTouched = false;
    mState->activeRenderPass = mState->nativeRenderPass;
    mState->currentRenderTarget = nullptr;
    iResetDrawCounters(mState);
    return true;
}

void piRendererMetal::SetExternalShaderAdjust(bool enabled)
{
    mState->externalShaderAdjust = enabled;
}

void piRendererMetal::SetUnityProjectionAdjusted(bool enabled)
{
    mState->unityProjectionAdjusted = enabled;
}

bool piRendererMetal::BeginExternalCommandEncoderFrame(void *commandBuffer, void *commandEncoder, void *renderPassDescriptor, int width, int height)
{
    EndNativeFrame();

    mState->commandBuffer = (__bridge id<MTLCommandBuffer>)commandBuffer;
    mState->encoder = (__bridge id<MTLRenderCommandEncoder>)commandEncoder;
    mState->activeRenderPass = (__bridge MTLRenderPassDescriptor *)renderPassDescriptor;
    if (!mState->commandBuffer || !mState->encoder || !mState->activeRenderPass)
    {
        iError(mReporter, "Metal external frame is missing Unity command buffer, command encoder, or render pass descriptor");
        mState->commandBuffer = nil;
        mState->encoder = nil;
        mState->activeRenderPass = nil;
        return false;
    }

    mState->frameActive = true;
    mState->externalCommandEncoder = true;
    mState->externalCommandBuffer = true;
    mState->passTouched = true;
    mState->currentRenderTarget = nullptr;
    mState->nativeDrawable = nil;
    mState->nativeRenderPass = nil;
    mState->viewports[0] = 0.0f;
    mState->viewports[1] = 0.0f;
    mState->viewports[2] = (float)((width > 0) ? width : 1);
    mState->viewports[3] = (float)((height > 0) ? height : 1);
    mState->viewports[4] = 0.0f;
    mState->viewports[5] = 1.0f;
    iApplyEncoderState(mState);
    iResetDrawCounters(mState);
    return true;
}

bool piRendererMetal::BeginExternalRenderPassFrame(void *commandBuffer, void *renderPassDescriptor, int width, int height)
{
    EndNativeFrame();

    mState->commandBuffer = (__bridge id<MTLCommandBuffer>)commandBuffer;
    mState->activeRenderPass = (__bridge MTLRenderPassDescriptor *)renderPassDescriptor;
    if (!mState->commandBuffer || !mState->activeRenderPass)
    {
        iError(mReporter, "Metal external frame is missing Unity command buffer or render pass descriptor");
        mState->commandBuffer = nil;
        mState->activeRenderPass = nil;
        return false;
    }

    mState->encoder = [mState->commandBuffer renderCommandEncoderWithDescriptor:mState->activeRenderPass];
    if (!mState->encoder)
    {
        iError(mReporter, "Metal external frame failed to create a render command encoder");
        mState->commandBuffer = nil;
        mState->activeRenderPass = nil;
        return false;
    }

    mState->frameActive = true;
    mState->externalCommandEncoder = false;
    mState->externalCommandBuffer = true;
    mState->passTouched = true;
    mState->currentRenderTarget = nullptr;
    mState->nativeDrawable = nil;
    mState->nativeRenderPass = nil;
    mState->viewports[0] = 0.0f;
    mState->viewports[1] = 0.0f;
    mState->viewports[2] = (float)((width > 0) ? width : 1);
    mState->viewports[3] = (float)((height > 0) ? height : 1);
    mState->viewports[4] = 0.0f;
    mState->viewports[5] = 1.0f;
    iApplyEncoderState(mState);
    iResetDrawCounters(mState);
    return true;
}

bool piRendererMetal::BeginExternalCommandQueueRenderPassFrame(void *commandQueue, void *renderPassDescriptor, int width, int height)
{
    EndNativeFrame();

    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)commandQueue;
    mState->activeRenderPass = (__bridge MTLRenderPassDescriptor *)renderPassDescriptor;
    if (!queue || !mState->activeRenderPass)
    {
        iError(mReporter, "Metal external frame is missing Unity command queue or render pass descriptor");
        mState->activeRenderPass = nil;
        return false;
    }

    mState->commandBuffer = iCreateOwnedCommandBuffer(queue);
    if (!mState->commandBuffer)
    {
        iError(mReporter, "Metal external frame failed to create a plugin command buffer");
        mState->activeRenderPass = nil;
        return false;
    }

    mState->encoder = [mState->commandBuffer renderCommandEncoderWithDescriptor:mState->activeRenderPass];
    if (!mState->encoder)
    {
        iError(mReporter, "Metal external frame failed to create a render command encoder");
        iReleaseOwnedCommandBuffer(mState->commandBuffer);
        mState->commandBuffer = nil;
        mState->activeRenderPass = nil;
        return false;
    }

    mState->frameActive = true;
    mState->externalCommandEncoder = false;
    mState->externalCommandBuffer = false;
    mState->passTouched = true;
    mState->currentRenderTarget = nullptr;
    mState->nativeDrawable = nil;
    mState->nativeRenderPass = nil;
    mState->viewports[0] = 0.0f;
    mState->viewports[1] = 0.0f;
    mState->viewports[2] = (float)((width > 0) ? width : 1);
    mState->viewports[3] = (float)((height > 0) ? height : 1);
    mState->viewports[4] = 0.0f;
    mState->viewports[5] = 1.0f;
    iApplyEncoderState(mState);
    iResetDrawCounters(mState);
    return true;
}

bool piRendererMetal::CopyNativeDrawableToTexture(void *destinationTexture)
{
    id<MTLTexture> destination = (__bridge id<MTLTexture>)destinationTexture;
    id<MTLTexture> source = mState->nativeRenderPass.colorAttachments[0].texture;
    if (!mState->frameActive || !mState->commandBuffer || !source || !destination ||
        source.width != destination.width || source.height != destination.height ||
        source.pixelFormat != destination.pixelFormat)
    {
        return false;
    }

    iEndEncoder(mState);
    id<MTLBlitCommandEncoder> blit = [mState->commandBuffer blitCommandEncoder];
    if (!blit)
        return false;
    [blit copyFromTexture:source
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(source.width, source.height, 1)
                toTexture:destination
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    return true;
}

void piRendererMetal::EndNativeFrame(bool waitForCompletion)
{
    if (!mState->frameActive)
    {
        return;
    }

    if (!mState->externalCommandEncoder && !mState->encoder && mState->passTouched && mState->activeRenderPass)
    {
        mState->encoder = [mState->commandBuffer renderCommandEncoderWithDescriptor:mState->activeRenderPass];
        iApplyEncoderState(mState);
    }

    if (mState->externalCommandEncoder)
    {
        mState->encoder = nil;
    }
    else
    {
        iEndEncoder(mState);
    }

    iReportDrawCounters(mState, mReporter);

    if (!mState->externalCommandBuffer && !mState->externalCommandEncoder && mState->nativeDrawable)
    {
        [mState->commandBuffer presentDrawable:mState->nativeDrawable];
    }
    // Dynamic buffers replaced while encoding must remain alive until the GPU
    // has consumed this command buffer. Unity owns and commits its external
    // command buffer, but Metal still permits us to attach a completion handler.
    iAttachRetainedBufferCleanup(mState);
    if (!mState->externalCommandBuffer && !mState->externalCommandEncoder)
    {
        id<MTLCommandBuffer> commandBuffer = mState->commandBuffer;
        const bool logCommandBuffer = iMetalEnvFlagEnabled("IMM_METAL_LOG_COMMAND_BUFFER");
        const bool waitCommandBuffer = waitForCompletion || iMetalEnvFlagEnabled("IMM_METAL_WAIT_COMMAND_BUFFER") || mState->nativeDrawable == nil;
        if (logCommandBuffer)
        {
            [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> completedCommandBuffer) {
                iReportCommandBufferStatus("completed-handler", completedCommandBuffer);
            }];
            iReportCommandBufferStatus("before-commit", commandBuffer);
        }
        [commandBuffer commit];
        if (waitCommandBuffer)
        {
            [commandBuffer waitUntilCompleted];
            if (logCommandBuffer)
            {
                iReportCommandBufferStatus("after-wait", commandBuffer);
            }
        }
        iReleaseOwnedCommandBuffer(commandBuffer);
    }

    mState->commandBuffer = nil;
    mState->nativeDrawable = nil;
    mState->nativeRenderPass = nil;
    mState->activeRenderPass = nil;
    mState->currentRenderTarget = nullptr;
    mState->currentShader = nullptr;
    mState->currentVertexArray = nullptr;
    mState->currentRasterState = nullptr;
    mState->currentBlendState = nullptr;
    mState->currentDepthState = nullptr;
    mState->dynamicSourceAlphaBlendEnabled = false;
    memset(mState->fragmentTextures, 0, sizeof(mState->fragmentTextures));
    memset(mState->fragmentSamplers, 0, sizeof(mState->fragmentSamplers));
    memset(mState->constantBuffers, 0, sizeof(mState->constantBuffers));
    memset(mState->shaderBuffers, 0, sizeof(mState->shaderBuffers));
    mState->frameActive = false;
    mState->externalCommandEncoder = false;
    mState->externalCommandBuffer = false;
    mState->passTouched = false;
}

bool piRendererMetal::SupportsFeature(RendererFeature)
{
    return false;
}

piRenderer::API piRendererMetal::GetAPI(void)
{
    return API::Metal;
}

void piRendererMetal::Report(void)
{
    iReport(mReporter, "Metal renderer report: standaloneProven=nativeFrame,offscreenRenderTarget,clear,viewport,depthTest,depthWrite,colorWriteMask,culling,sourceAlphaBlend,staticBuffers,dynamicBufferUpdate,immediateConstants,textures2D,texturesCube,samplers,renderShaders,staticPaint,pretessellatedPaint,picture2D,picture360Equirect,picture360CubemapShaderSmoke,indexedDraw,nonIndexedDraw,indirectDrawCpuFallback,unitQuad,unitCube,textureReadback,pngCapture,cpuTiming");
    iReport(mReporter, "Metal renderer report: standaloneUnsupported=externalTextureWrapping,imageLoadStore,computeShaders,atomicBuffers,pixelPackBuffers,nonDefaultPointSize,nonDefaultLineWidth,polygonOffset,multiRenderTargetBlend,multiRenderTargetColorMask,gpuTimestampQueries");
}

void piRendererMetal::SetActiveWindow(int) {}
void piRendererMetal::Enable(void) {}
void piRendererMetal::Disable(void) {}
void piRendererMetal::SwapBuffers(void) { EndNativeFrame(); }
void *piRendererMetal::GetContext(void) { return (__bridge void *)mState->commandQueue; }
void piRendererMetal::StartPerformanceMeasure(void)
{
    BeginQuery(mState->perfQueries[mState->currentPerformanceQuery & 1]);
}
void piRendererMetal::EndPerformanceMeasure(void)
{
    EndQuery(mState->perfQueries[mState->currentPerformanceQuery & 1]);
}
uint64_t piRendererMetal::GetPerformanceMeasure(void)
{
    const uint64_t result = GetQueryResult(mState->perfQueries[mState->currentPerformanceQuery & 1]);
    mState->currentPerformanceQuery++;
    return result;
}

piRTarget piRendererMetal::CreateRenderTarget(piTexture vtex0, piTexture vtex1, piTexture vtex2, piTexture vtex3, piTexture zbuf)
{
    piRTargetS *target = new piRTargetS();
    target->color[0] = vtex0;
    target->color[1] = vtex1;
    target->color[2] = vtex2;
    target->color[3] = vtex3;
    target->depth = zbuf;
    ++mState->liveRenderTargets;
    return target;
}
void piRendererMetal::DestroyRenderTarget(piRTarget obj)
{
    if (!obj) return;
    if (mState->liveRenderTargets > 0) --mState->liveRenderTargets;
    delete obj;
}
bool piRendererMetal::SetRenderTarget(piRTarget obj)
{
    if (!mState->commandBuffer)
    {
        mState->commandBuffer = iCreateOwnedCommandBuffer(mState->commandQueue);
        mState->frameActive = (mState->commandBuffer != nil);
        if (!mState->frameActive)
        {
            return false;
        }
    }

    iEndEncoder(mState);
    mState->currentRenderTarget = obj;
    mState->activeRenderPass = obj ? iRenderPassForTarget(obj) : mState->nativeRenderPass;
    return mState->activeRenderPass != nil;
}
void piRendererMetal::RenderTargetSampleLocations(piRTarget, const float *) {}
void piRendererMetal::BlitRenderTarget(piRTarget dst, piRTarget src, bool color, bool depth)
{
    if (!mState->commandQueue || !src || !dst)
    {
        return;
    }

    if (!mState->commandBuffer)
    {
        mState->commandBuffer = iCreateOwnedCommandBuffer(mState->commandQueue);
        mState->frameActive = (mState->commandBuffer != nil);
    }
    if (!mState->commandBuffer)
    {
        return;
    }

    iEndEncoder(mState);
    id<MTLBlitCommandEncoder> blit = [mState->commandBuffer blitCommandEncoder];
    if (!blit)
    {
        return;
    }

    if (color)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!src->color[i] || !dst->color[i] || !src->color[i]->texture || !dst->color[i]->texture)
            {
                continue;
            }
            const NSUInteger width = MIN(src->color[i]->texture.width, dst->color[i]->texture.width);
            const NSUInteger height = MIN(src->color[i]->texture.height, dst->color[i]->texture.height);
            const NSUInteger depthCount = MIN(src->color[i]->texture.depth, dst->color[i]->texture.depth);
            [blit copyFromTexture:src->color[i]->texture
                       sourceSlice:0
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:MTLSizeMake(width, height, depthCount)
                         toTexture:dst->color[i]->texture
                  destinationSlice:0
                  destinationLevel:0
                 destinationOrigin:MTLOriginMake(0, 0, 0)];
        }
    }

    if (depth && src->depth && dst->depth && src->depth->texture && dst->depth->texture)
    {
        const NSUInteger width = MIN(src->depth->texture.width, dst->depth->texture.width);
        const NSUInteger height = MIN(src->depth->texture.height, dst->depth->texture.height);
        [blit copyFromTexture:src->depth->texture
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width, height, 1)
                     toTexture:dst->depth->texture
              destinationSlice:0
              destinationLevel:0
             destinationOrigin:MTLOriginMake(0, 0, 0)];
    }

    [blit endEncoding];
    mState->passTouched = true;
}
void piRendererMetal::SetWriteMask(bool c0, bool, bool, bool, bool z)
{
    mState->color0WriteEnabled = c0;
    mState->depthWriteEnabled = z;
    iApplyEncoderState(mState);
}
void piRendererMetal::SetShadingSamples(int) {}
void piRendererMetal::RenderTargetGetDefaultSampleLocation(piRTarget, const int, float *location)
{
    if (location)
    {
        location[0] = 0.5f;
        location[1] = 0.5f;
    }
}

void piRendererMetal::Clear(const float *color0, const float *, const float *, const float *, const bool depth0)
{
    if (!mState->frameActive || !mState->activeRenderPass)
    {
        return;
    }

    if (color0)
    {
        MTLRenderPassColorAttachmentDescriptor *colorAttachment = mState->activeRenderPass.colorAttachments[0];
        if (colorAttachment)
        {
            colorAttachment.loadAction = MTLLoadActionClear;
            colorAttachment.storeAction = MTLStoreActionStore;
            colorAttachment.clearColor = MTLClearColorMake(color0[0], color0[1], color0[2], color0[3]);
            mState->passTouched = true;
        }
    }

    if (depth0 && mState->activeRenderPass.depthAttachment)
    {
        mState->activeRenderPass.depthAttachment.loadAction = MTLLoadActionClear;
        mState->activeRenderPass.depthAttachment.storeAction = MTLStoreActionStore;
        mState->activeRenderPass.depthAttachment.clearDepth = 1.0;
        mState->passTouched = true;
    }
}
void piRendererMetal::SetState(piState state, bool value)
{
    switch (state)
    {
        case piSTATE_DEPTH_TEST:
            mState->depthTestEnabled = value;
            break;
        case piSTATE_CULL_FACE:
            mState->cullFaceEnabled = value;
            break;
        case piSTATE_FRONT_FACE:
            mState->frontFaceCCW = value;
            break;
        default:
            break;
    }
    iApplyEncoderState(mState);
}
void piRendererMetal::SetBlending(int buf, BlendEquation equRGB, BlendOperations srcRGB, BlendOperations dstRGB, BlendEquation equALP, BlendOperations srcALP, BlendOperations dstALP)
{
    if (buf != 0)
    {
        iUnsupported(mState, mReporter, piMetalUnsupportedFeature::DynamicBlending, "Metal renderer only supports dynamic SetBlending for render target 0");
        return;
    }

    const bool disableBlend =
        equRGB == BlendEquation::piBLEND_ADD &&
        equALP == BlendEquation::piBLEND_ADD &&
        srcRGB == BlendOperations::piBLEND_ONE &&
        dstRGB == BlendOperations::piBLEND_ZERO &&
        srcALP == BlendOperations::piBLEND_ONE &&
        dstALP == BlendOperations::piBLEND_ZERO;

    const bool sourceAlphaBlend =
        equRGB == BlendEquation::piBLEND_ADD &&
        equALP == BlendEquation::piBLEND_ADD &&
        srcRGB == BlendOperations::piBLEND_SRC_ALPHA &&
        dstRGB == BlendOperations::piBLEND_ONE_MINUS_SRC_ALPHA &&
        (srcALP == BlendOperations::piBLEND_ONE || srcALP == BlendOperations::piBLEND_SRC_ALPHA) &&
        dstALP == BlendOperations::piBLEND_ONE_MINUS_SRC_ALPHA;

    if (!disableBlend && !sourceAlphaBlend)
    {
        iUnsupported(mState, mReporter, piMetalUnsupportedFeature::DynamicBlending, "Metal renderer only supports dynamic SetBlending disabled or source-alpha blending");
        return;
    }

    mState->dynamicSourceAlphaBlendEnabled = sourceAlphaBlend;
}
void piRendererMetal::SetViewport(int id, const int *vp)
{
    if (!vp || id < 0 || id >= 16) return;
    mState->numViewports = id + 1 > mState->numViewports ? id + 1 : mState->numViewports;
    float *dst = mState->viewports + id * 6;
    dst[0] = (float)vp[0];
    dst[1] = (float)vp[1];
    dst[2] = (float)vp[2];
    dst[3] = (float)vp[3];
    dst[4] = 0.0f;
    dst[5] = 1.0f;
    iApplyEncoderState(mState);
}

void piRendererMetal::SetViewports(int num, const float *viewports)
{
    if (!viewports) return;
    if (num < 0) num = 0;
    if (num > 16) num = 16;
    mState->numViewports = num;
    memcpy(mState->viewports, viewports, sizeof(float) * 6 * num);
    iApplyEncoderState(mState);
}

void piRendererMetal::GetViewports(int *num, float *viewports)
{
    if (num) *num = mState->numViewports;
    if (viewports) memcpy(viewports, mState->viewports, sizeof(float) * 6 * mState->numViewports);
}

piRasterState piRendererMetal::CreateRasterState(bool wireframe, bool frontIsCounterClockWise, CullMode cullMode, bool depthClamp, bool)
{
    piRasterStateS *state = new piRasterStateS();
    state->wireframe = wireframe;
    state->depthClamp = depthClamp;
    state->frontWinding = frontIsCounterClockWise ? MTLWindingCounterClockwise : MTLWindingClockwise;
    switch (cullMode)
    {
        case CullMode::FRONT: state->cullMode = MTLCullModeFront; break;
        case CullMode::BACK: state->cullMode = MTLCullModeBack; break;
        case CullMode::NONE:
        default: state->cullMode = MTLCullModeNone; break;
    }
    ++mState->liveRasterStates;
    return state;
}
void piRendererMetal::SetRasterState(const piRasterState vme)
{
    mState->currentRasterState = vme;
    if (vme)
    {
        mState->frontFaceCCW = vme->frontWinding == MTLWindingCounterClockwise;
    }
    iApplyEncoderState(mState);
}
void piRendererMetal::DestroyRasterState(piRasterState vme)
{
    if (!vme) return;
    if (mState->liveRasterStates > 0) --mState->liveRasterStates;
    delete vme;
}
piBlendState piRendererMetal::CreateBlendState(bool alphaToCoverage, bool enabled0)
{
    piBlendStateS *state = new piBlendStateS();
    state->alphaToCoverage = alphaToCoverage;
    state->enabled0 = enabled0;
    ++mState->liveBlendStates;
    return state;
}
void piRendererMetal::SetBlendState(const piBlendState vme) { mState->currentBlendState = vme; }
void piRendererMetal::DestroyBlendState(piBlendState vme)
{
    if (!vme) return;
    if (mState->liveBlendStates > 0) --mState->liveBlendStates;
    delete vme;
}
piDepthState piRendererMetal::CreateDepthState(bool depthEnable, bool lessEqual)
{
    if (!mState->device)
    {
        return nullptr;
    }
    MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
    descriptor.depthCompareFunction = lessEqual ? MTLCompareFunctionLessEqual : MTLCompareFunctionGreaterEqual;
    descriptor.depthWriteEnabled = YES;

    piDepthStateS *state = new piDepthStateS();
    state->depthEnabled = depthEnable;
    state->depthWriteState = [mState->device newDepthStencilStateWithDescriptor:descriptor];
    descriptor.depthWriteEnabled = NO;
    state->depthNoWriteState = [mState->device newDepthStencilStateWithDescriptor:descriptor];
    if (!state->depthWriteState || !state->depthNoWriteState)
    {
        delete state;
        return nullptr;
    }
    ++mState->liveDepthStates;
    return state;
}
void piRendererMetal::SetDepthState(const piDepthState vme)
{
    mState->currentDepthState = vme;
    mState->depthTestEnabled = vme && vme->depthEnabled;
    iApplyEncoderState(mState);
}
void piRendererMetal::DestroyDepthState(piDepthState vme)
{
    if (!vme) return;
    vme->depthWriteState = nil;
    vme->depthNoWriteState = nil;
    if (mState->liveDepthStates > 0) --mState->liveDepthStates;
    delete vme;
}

piTexture piRendererMetal::CreateTexture(const wchar_t *, const TextureInfo *info, bool, TextureFilter filter, TextureWrap wrap, float, const void *buffer)
{
    if (!mState->device || !info)
    {
        return nullptr;
    }

    const MTLPixelFormat pixelFormat = iFormatPiToMetal(info->mFormat);
    if (pixelFormat == MTLPixelFormatInvalid)
    {
        iError(mReporter, "Metal texture format is not supported yet");
        return nullptr;
    }

    MTLTextureDescriptor *descriptor = nil;
    switch (info->mType)
    {
        case TextureType::T2D:
            descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                            width:(NSUInteger)info->mXres
                                                                           height:(NSUInteger)info->mYres
                                                                        mipmapped:(info->mNumMips > 1)];
            break;
        case TextureType::T2D_ARRAY:
            descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                            width:(NSUInteger)info->mXres
                                                                           height:(NSUInteger)info->mYres
                                                                        mipmapped:(info->mNumMips > 1)];
            descriptor.textureType = MTLTextureType2DArray;
            descriptor.arrayLength = (NSUInteger)(info->mZres > 0 ? info->mZres : 1);
            break;
        case TextureType::TCUBE:
            descriptor = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:pixelFormat
                                                                               size:(NSUInteger)info->mXres
                                                                          mipmapped:(info->mNumMips > 1)];
            break;
        default:
            iError(mReporter, "Metal texture type is not supported yet");
            return nullptr;
    }

    descriptor.sampleCount = (NSUInteger)(info->mMultisample > 1 ? info->mMultisample : 1);
    descriptor.storageMode = buffer ? MTLStorageModeShared : MTLStorageModePrivate;
    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
    if (descriptor.sampleCount > 1)
    {
        descriptor.textureType = MTLTextureType2DMultisample;
        descriptor.mipmapLevelCount = 1;
    }

    piTextureS *texture = new piTextureS();
    texture->texture = [mState->device newTextureWithDescriptor:descriptor];
    if (!texture->texture)
    {
        delete texture;
        return nullptr;
    }

    texture->info = *info;
    texture->filter = filter;
    texture->wrap = wrap;
    if (buffer && descriptor.sampleCount == 1)
    {
        const int slices = (info->mType == TextureType::T2D_ARRAY) ? info->mZres : (info->mType == TextureType::TCUBE ? 6 : 1);
        UpdateTexture(texture, 0, 0, 0, info->mXres, info->mYres, slices, buffer);
    }
    ++mState->liveTextures;
    return texture;
}

piTexture piRendererMetal::CreateTexture2(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap, float aniso, const void *buffer, int)
{
    return CreateTexture(key, info, compress, filter, wrap, aniso, buffer);
}

void piRendererMetal::DestroyTexture(piTexture obj)
{
    if (!obj) return;
    obj->texture = nil;
    if (mState->liveTextures > 0) --mState->liveTextures;
    delete obj;
}
void piRendererMetal::ClearTexture(piTexture vme, int level, const void *data)
{
    if (!vme || !vme->texture || !data || level < 0 || (NSUInteger)level >= vme->texture.mipmapLevelCount)
    {
        return;
    }

    const NSUInteger bytesPerPixel = iBytesPerPixel(vme->info.mFormat);
    if (bytesPerPixel == 0)
    {
        return;
    }

    const NSUInteger width = MAX((NSUInteger)1, vme->texture.width >> (NSUInteger)level);
    const NSUInteger height = MAX((NSUInteger)1, vme->texture.height >> (NSUInteger)level);
    const NSUInteger slices = vme->info.mType == TextureType::T2D_ARRAY ? vme->texture.arrayLength : 1;
    const NSUInteger bytesPerRow = width * bytesPerPixel;
    const NSUInteger imageBytes = bytesPerRow * height;
    char *clearData = (char *)malloc(imageBytes);
    if (!clearData)
    {
        return;
    }

    for (NSUInteger offset = 0; offset < imageBytes; offset += bytesPerPixel)
    {
        memcpy(clearData + offset, data, bytesPerPixel);
    }

    const MTLRegion region = MTLRegionMake3D(0, 0, 0, width, height, 1);
    for (NSUInteger slice = 0; slice < slices; ++slice)
    {
        [vme->texture replaceRegion:region
                         mipmapLevel:(NSUInteger)level
                               slice:slice
                           withBytes:clearData
                         bytesPerRow:bytesPerRow
                       bytesPerImage:imageBytes];
    }
    free(clearData);
}
void piRendererMetal::UpdateTexture(piTexture me, int x0, int y0, int z0, int xres, int yres, int zres, const void *buffer)
{
    if (!me || !me->texture || !buffer || xres <= 0 || yres <= 0 || zres <= 0)
    {
        return;
    }

    const NSUInteger bytesPerPixel = iBytesPerPixel(me->info.mFormat);
    if (bytesPerPixel == 0)
    {
        return;
    }

    const MTLRegion region = MTLRegionMake3D((NSUInteger)x0, (NSUInteger)y0, 0, (NSUInteger)xres, (NSUInteger)yres, 1);
    const NSUInteger bytesPerRow = (NSUInteger)xres * bytesPerPixel;
    const char *src = static_cast<const char *>(buffer);
    const int slices = (me->info.mType == TextureType::T2D_ARRAY || me->info.mType == TextureType::TCUBE) ? zres : 1;
    for (int slice = 0; slice < slices; ++slice)
    {
        [me->texture replaceRegion:region
                        mipmapLevel:0
                              slice:(NSUInteger)(z0 + slice)
                          withBytes:src + (size_t)slice * bytesPerRow * (NSUInteger)yres
                        bytesPerRow:bytesPerRow
                      bytesPerImage:bytesPerRow * (NSUInteger)yres];
    }
}
void piRendererMetal::GetTextureRes(piTexture me, int *res)
{
    if (!me || !res) return;
    res[0] = me->info.mXres;
    res[1] = me->info.mYres;
    res[2] = me->info.mZres;
}
void piRendererMetal::GetTextureFormat(piTexture me, Format *format) { if (me && format) *format = me->info.mFormat; }
void piRendererMetal::GetTextureContent(piTexture me, void *data, const Format)
{
    if (!me || !data)
    {
        return;
    }
    GetTextureContent(me, data, 0, 0, 0, me->info.mXres, me->info.mYres, me->info.mType == TextureType::T2D_ARRAY ? me->info.mZres : 1);
}

void piRendererMetal::GetTextureContent(piTexture vme, void *data, int x, int y, int z, int xres, int yres, int zres)
{
    if (!vme || !vme->texture || !data || !mState->commandQueue || xres <= 0 || yres <= 0 || zres <= 0)
    {
        return;
    }

    const NSUInteger bytesPerPixel = iBytesPerPixel(vme->info.mFormat);
    if (bytesPerPixel == 0)
    {
        return;
    }

    if (mState->frameActive && mState->commandBuffer)
    {
        iEndEncoder(mState);
        iAttachRetainedBufferCleanup(mState);
        [mState->commandBuffer commit];
        [mState->commandBuffer waitUntilCompleted];
        iReleaseOwnedCommandBuffer(mState->commandBuffer);
        mState->commandBuffer = nil;
        mState->activeRenderPass = mState->currentRenderTarget ? iRenderPassForTarget(mState->currentRenderTarget) : mState->nativeRenderPass;
    }

    const NSUInteger rowBytes = (NSUInteger)xres * bytesPerPixel;
    const NSUInteger alignedRowBytes = (rowBytes + 255u) & ~255u;
    const NSUInteger imageBytes = alignedRowBytes * (NSUInteger)yres;
    const NSUInteger totalBytes = imageBytes * (NSUInteger)zres;

    id<MTLBuffer> readback = [mState->device newBufferWithLength:totalBytes options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> commandBuffer = [mState->commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
    if (!readback || !commandBuffer || !blit)
    {
        return;
    }

    for (int slice = 0; slice < zres; ++slice)
    {
        [blit copyFromTexture:vme->texture
                   sourceSlice:(NSUInteger)(z + slice)
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake((NSUInteger)x, (NSUInteger)y, 0)
                    sourceSize:MTLSizeMake((NSUInteger)xres, (NSUInteger)yres, 1)
                      toBuffer:readback
             destinationOffset:(NSUInteger)slice * imageBytes
        destinationBytesPerRow:alignedRowBytes
      destinationBytesPerImage:imageBytes];
    }

    [blit endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    const char *src = (const char *)[readback contents];
    char *dst = (char *)data;
    for (int slice = 0; slice < zres; ++slice)
    {
        for (int row = 0; row < yres; ++row)
        {
            memcpy(dst + ((size_t)slice * (size_t)yres + (size_t)row) * rowBytes,
                   src + (size_t)slice * imageBytes + (size_t)row * alignedRowBytes,
                   rowBytes);
        }
    }
}
void piRendererMetal::GetTextureInfo(piTexture me, TextureInfo *info) { if (me && info) *info = me->info; }
void piRendererMetal::GetTextureSampling(piTexture me, TextureFilter *rfilter, TextureWrap *rwrap)
{
    if (!me) return;
    if (rfilter) *rfilter = me->filter;
    if (rwrap) *rwrap = me->wrap;
}
void piRendererMetal::ComputeMipmaps(piTexture me)
{
    if (!me || !me->texture || me->texture.mipmapLevelCount <= 1 || !mState->commandQueue)
    {
        return;
    }

    if (!mState->commandBuffer)
    {
        mState->commandBuffer = iCreateOwnedCommandBuffer(mState->commandQueue);
        mState->frameActive = (mState->commandBuffer != nil);
    }
    if (!mState->commandBuffer)
    {
        return;
    }

    iEndEncoder(mState);
    id<MTLBlitCommandEncoder> blit = [mState->commandBuffer blitCommandEncoder];
    if (!blit)
    {
        return;
    }
    [blit generateMipmapsForTexture:me->texture];
    [blit endEncoding];
}
void piRendererMetal::AttachTextures(int num, piTexture vt0, piTexture vt1, piTexture vt2, piTexture vt3, piTexture vt4, piTexture vt5, piTexture vt6, piTexture vt7, piTexture vt8, piTexture vt9, piTexture vt10, piTexture vt11, piTexture vt12, piTexture vt13, piTexture vt14, piTexture vt15)
{
    piTexture textures[16] = { vt0, vt1, vt2, vt3, vt4, vt5, vt6, vt7, vt8, vt9, vt10, vt11, vt12, vt13, vt14, vt15 };
    AttachTextures(num, textures, 0);
}

void piRendererMetal::AttachTextures(int num, piTexture *vt, int offset)
{
    if (!vt || offset < 0 || offset >= 16)
    {
        return;
    }
    if (num > 16 - offset)
    {
        num = 16 - offset;
    }
    for (int i = 0; i < num; ++i)
    {
        mState->fragmentTextures[offset + i] = vt[i];
    }
}

void piRendererMetal::DettachTextures(void)
{
    memset(mState->fragmentTextures, 0, sizeof(mState->fragmentTextures));
}
piTexture piRendererMetal::CreateTextureFromID(unsigned int, TextureFilter)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::ExternalTexture, "Metal renderer does not support CreateTextureFromID/external texture wrapping yet");
    return nullptr;
}
void piRendererMetal::MakeResident(piTexture)
{
}
void piRendererMetal::MakeNonResident(piTexture)
{
}
uint64_t piRendererMetal::GetTextureHandle(piTexture vme)
{
    if (!vme || !vme->texture)
    {
        return 0;
    }
    return (uint64_t)(__bridge void *)vme->texture;
}

piSampler piRendererMetal::CreateSampler(TextureFilter filter, TextureWrap wrap, float)
{
    if (!mState->device)
    {
        return nullptr;
    }
    MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = (filter == TextureFilter::LINEAR || filter == TextureFilter::MIPMAP) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    descriptor.magFilter = (filter == TextureFilter::LINEAR || filter == TextureFilter::MIPMAP) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    descriptor.mipFilter = (filter == TextureFilter::MIPMAP || filter == TextureFilter::NONE_MIPMAP) ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
    const MTLSamplerAddressMode addressMode = (wrap == TextureWrap::REPEAT || wrap == TextureWrap::MIRROR_REPEAT) ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
    descriptor.sAddressMode = addressMode;
    descriptor.tAddressMode = addressMode;
    descriptor.rAddressMode = addressMode;

    piSamplerS *sampler = new piSamplerS();
    sampler->sampler = [mState->device newSamplerStateWithDescriptor:descriptor];
    if (!sampler->sampler)
    {
        delete sampler;
        return nullptr;
    }
    ++mState->liveSamplers;
    return sampler;
}

void piRendererMetal::DestroySampler(piSampler obj)
{
    if (!obj) return;
    obj->sampler = nil;
    if (mState->liveSamplers > 0) --mState->liveSamplers;
    delete obj;
}

void piRendererMetal::AttachSamplers(int num, piSampler vt0, piSampler vt1, piSampler vt2, piSampler vt3, piSampler vt4, piSampler vt5, piSampler vt6, piSampler vt7)
{
    piSampler samplers[8] = { vt0, vt1, vt2, vt3, vt4, vt5, vt6, vt7 };
    if (num > 8) num = 8;
    for (int i = 0; i < num; ++i)
    {
        mState->fragmentSamplers[i] = samplers[i];
    }
}

void piRendererMetal::DettachSamplers(void)
{
    memset(mState->fragmentSamplers, 0, sizeof(mState->fragmentSamplers));
}
void piRendererMetal::AttachImage(int, piTexture, int, bool, int, Format)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::ImageLoadStore, "Metal renderer does not support image load/store bindings yet");
}

piShader piRendererMetal::CreateShader(const piShaderOptions *options, const char *, const char *, const char *, const char *, const char *, char *error)
{
    if (!mState->device)
    {
        if (error) strcpy(error, "Metal device is not initialized");
        return nullptr;
    }

    int brushType = -1;
    int colorCompressed = 0;
    int formatIsStereo = -1;
    int modelLayer = 0;
    int cubemapLayer = 0;
    int pretessellatedPaint = 0;
    int wiggle = 0;
    int drawIn = 0;
    const bool isStaticPaintShader = iGetShaderOption(options, "BRUSHTYPE", &brushType);
    const bool isPretessellatedPaintShader = iGetShaderOption(options, "PRETESSELLATED", &pretessellatedPaint);
    const bool isModelShader = iGetShaderOption(options, "MODEL_LAYER", &modelLayer);
    const bool isPicture360CubemapShader = iGetShaderOption(options, "CUBEMAP", &cubemapLayer);
    const bool isPicture360EquirectShader = iGetShaderOption(options, "FORMAT_IS_STEREO", &formatIsStereo);
    const bool isPictureShader = options && !isStaticPaintShader && !isPretessellatedPaintShader && !isPicture360EquirectShader && !isPicture360CubemapShader && !isModelShader;
    iGetShaderOption(options, "COLOR_COMPRESSED", &colorCompressed);
    iGetShaderOption(options, "WIGGLE", &wiggle);
    iGetShaderOption(options, "DRAWIN", &drawIn);

    NSString *source = nil;
    const char *vertexFunctionName = nullptr;
    const char *fragmentFunctionName = nullptr;
    MTLPixelFormat colorFormat = MTLPixelFormatBGRA8Unorm;
    bool requiresVertexBuffer = true;
    bool enableSourceAlphaBlending = false;

    if (isStaticPaintShader)
    {
        source = [NSString stringWithFormat:
             @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "constant uint kBrushType = %d;\n"
             "constant uint kColorCompressed = %d;\n"
             "constant uint kWiggle = %d;\n"
             "constant uint kDrawIn = %d;\n"
             "struct FrameState { float mTime; int mFrame; int mDummy1; int mDummy2; };\n"
             "struct VertexData {\n"
             "    packed_float3 mPos;\n"
             "    uint mWid;\n"
             "    uint mColAlp;\n"
             "    uint mDirInf;\n"
             "    uint mAxUUn2;\n"
             "    uint mAxVUn3;\n"
             "    float mTim;\n"
             "};\n"
             "struct LayerState {\n"
             "    float4 mLayerToViewer[4];\n"
             "    float mLayerToViewerScale;\n"
             "    float mOpacity;\n"
             "    float mFlipSign;\n"
             "    float mDrawInTime;\n"
             "    float4 mAnimParams;\n"
             "    float4 mKeepAlive[2];\n"
             "    uint mID;\n"
             "};\n"
             "struct DisplayEye { float4 mViewerToEyePrj[4]; };\n"
             "struct DisplayState { DisplayEye mEye[2]; float2 mResolution; uint mEyeIndex; };\n"
             "struct Chunk { uint mVertexOffset; float mBiggestStroke; };\n"
             "struct ChunkData { Chunk mData[128]; };\n"
             "struct VSOut { float4 position [[position]]; float4 color; uint mask [[flat]]; };\n"
             "struct FSOut { float4 color [[color(0)]]; uint sampleMask [[sample_mask]]; };\n"
             "float4 mul_row_major(constant float4 *m, float4 v) {\n"
             "    return float4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v));\n"
             "}\n"
             "float3 transform_dir_row_major(constant float4 *m, float3 v) {\n"
             "    return float3(dot(m[0].xyz, v), dot(m[1].xyz, v), dot(m[2].xyz, v));\n"
             "}\n"
             "float4 unpack4(uint d) {\n"
             "    return float4(d & 255u, (d >> 8u) & 255u, (d >> 16u) & 255u, (d >> 24u) & 255u) / 255.0;\n"
             "}\n"
             "float3 unpack3(uint d) {\n"
             "    return float3(d & 255u, (d >> 8u) & 255u, (d >> 16u) & 255u) / 255.0;\n"
             "}\n"
             "float3 decode_unit_vector(uint data) {\n"
             "    uint2 d = uint2(data & 65535u, data >> 16u);\n"
             "    float2 v = float2(d) / 32767.5 - 1.0;\n"
             "    float3 nor = float3(v, 1.0 - abs(v.x) - abs(v.y));\n"
             "    float t = max(-nor.z, 0.0);\n"
             "    nor.x += (nor.x > 0.0) ? -t : t;\n"
             "    nor.y += (nor.y > 0.0) ? -t : t;\n"
             "    return normalize(nor);\n"
             "}\n"
             "vertex VSOut imm_static_paint_vertex(uint vid [[vertex_id]],\n"
             "                                      constant FrameState &frame [[buffer(6)]],\n"
             "                                      constant LayerState &layer [[buffer(3)]],\n"
             "                                      constant DisplayState &display [[buffer(4)]],\n"
             "                                      const device VertexData *data [[buffer(8)]],\n"
             "                                      constant ChunkData &chunkData [[buffer(9)]]) {\n"
             "    uint realVertexID = vid + chunkData.mData[0].mVertexOffset;\n"
             "    uint bid = realVertexID;\n"
             "    uint brushVertex = 0u;\n"
             "    if (kBrushType == 1u) { bid = realVertexID >> 1u; brushVertex = realVertexID & 1u; }\n"
             "    else if (kBrushType == 2u || kBrushType == 3u) { bid = realVertexID / 7u; brushVertex = realVertexID %% 7u; }\n"
             "    else if (kBrushType == 4u) { bid = realVertexID >> 2u; brushVertex = realVertexID & 3u; }\n"
             "    VertexData vtx = data[bid];\n"
             "    float3 inVertex = float3(vtx.mPos);\n"
             "    uint inInfo = vtx.mDirInf >> 24u;\n"
             "    float inTime = vtx.mTim;\n"
             "    float inWid = 1.7 * chunkData.mData[0].mBiggestStroke * float(vtx.mWid & 0x7fffu) / 32767.0;\n"
             "    float4 inColAlpha = unpack4(vtx.mColAlp);\n"
             "    float3 inOri = normalize(-1.0 + 2.0 * unpack3(vtx.mDirInf));\n"
             "    float3 inAxU = decode_unit_vector(vtx.mAxUUn2);\n"
             "    float3 inAxV = decode_unit_vector(vtx.mAxVUn3);\n"
             "    float3 pos = inVertex;\n"
             "    if (kWiggle == 1u) {\n"
             "        pos += layer.mKeepAlive[0].z * sin(layer.mKeepAlive[0].x * inVertex.yzx + layer.mKeepAlive[0].y * frame.mTime);\n"
             "    }\n"
             "    float3 cpos = mul_row_major(layer.mLayerToViewer, float4(pos, 1.0)).xyz;\n"
             "    float f = 1.0;\n"
             "    if (((inInfo >> 7u) & 1u) == 0u) {\n"
             "        float3 wori = normalize(transform_dir_row_major(layer.mLayerToViewer, inOri));\n"
             "        f = clamp(dot(wori, normalize(cpos)), 0.0, 1.0);\n"
             "        f *= f;\n"
             "    }\n"
             "    float3 col = (kColorCompressed == 0u) ? inColAlpha.xyz * inColAlpha.xyz : inColAlpha.xyz;\n"
             "    float alpha = inColAlpha.w * f * layer.mOpacity;\n"
             "    if (kDrawIn == 1u) {\n"
             "        float drawingT = 2.0 * layer.mDrawInTime - inTime;\n"
             "        alpha *= smoothstep(layer.mAnimParams.z, layer.mAnimParams.z + layer.mAnimParams.w, drawingT);\n"
             "    }\n"
             "    float3 bU = normalize(transform_dir_row_major(layer.mLayerToViewer, inAxU));\n"
             "    float3 bV = normalize(transform_dir_row_major(layer.mLayerToViewer, inAxV));\n"
             "    float3 bWPos = cpos;\n"
             "    if (kBrushType == 1u) {\n"
             "        float u = float(brushVertex);\n"
             "        float wb = (-1.0 + 2.0 * u) * inWid * layer.mLayerToViewerScale;\n"
             "        bWPos = cpos - wb * bU;\n"
             "    } else if (kBrushType == 2u || kBrushType == 3u) {\n"
             "        float u = float(brushVertex) / 7.0;\n"
             "        float a = u * 6.283185;\n"
             "        float2 sc = float2(cos(a), sin(a));\n"
             "        if (kBrushType == 3u) sc *= float2(1.0, 0.3);\n"
             "        float wb = inWid * layer.mLayerToViewerScale;\n"
             "        bWPos = cpos + wb * (bU * sc.x + bV * sc.y);\n"
             "    } else if (kBrushType == 4u) {\n"
             "        float u = float(brushVertex) / 4.0;\n"
             "        float wb = inWid * layer.mLayerToViewerScale;\n"
             "        float2 sc = float2(sign(0.2 - abs(u - 0.65)), sign(abs(u - 0.35) - 0.3));\n"
             "        bWPos = cpos + wb * (bU * sc.x + bV * sc.y);\n"
             "    }\n"
             "    uint eye = min(display.mEyeIndex, 1u);\n"
             "    VSOut out;\n"
             "    out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(bWPos, 1.0));\n"
             "    out.color = float4(col, alpha);\n"
             "    out.mask = (alpha > 0.999) ? layer.mID : inInfo;\n"
             "    return out;\n"
             "}\n"
             "uint imm_alpha_to_coverage(float alpha, float4 position, uint frameID, uint primitiveID, texture2d_array<float> blueNoise) {\n"
             "    const float MSAASampleCount = 8.0;\n"
             "    uint2 pixel = uint2(uint(position.x) & 63u, uint(position.y) & 63u);\n"
             "    uint slice = frameID & 63u;\n"
             "    float ran = blueNoise.read(pixel, slice, 0).x;\n"
             "    alpha = clamp(alpha + 0.99 * (ran - 0.5) / MSAASampleCount, 0.0, 1.0);\n"
             "    uint mask = (0xff00u >> uint(alpha * MSAASampleCount + 0.5)) & 0xffu;\n"
             "    uint shift = (uint(ran * 7.0) + primitiveID) & 7u;\n"
             "    uint barrel = (mask << 8u) | mask;\n"
             "    return (barrel >> shift) & 0xffu;\n"
             "}\n"
             "fragment FSOut imm_static_paint_fragment(VSOut in [[stage_in]],\n"
             "                                         constant FrameState &frame [[buffer(6)]],\n"
             "                                         texture2d_array<float> blueNoise [[texture(7)]]) {\n"
             "    FSOut out;\n"
             "    out.color = float4(in.color.rgb, 1.0);\n"
             "    out.sampleMask = imm_alpha_to_coverage(in.color.a, in.position, uint(frame.mFrame), in.mask, blueNoise);\n"
             "    return out;\n"
             "}\n",
             brushType < 0 ? 0 : brushType,
             colorCompressed ? 1 : 0,
             wiggle ? 1 : 0,
             drawIn ? 1 : 0];
        vertexFunctionName = "imm_static_paint_vertex";
        fragmentFunctionName = "imm_static_paint_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = false;
        enableSourceAlphaBlending = false;
    }
    else if (isPretessellatedPaintShader)
    {
        source = [NSString stringWithFormat:
            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "constant uint kColorCompressed = %d;\n"
             "constant uint kWiggle = %d;\n"
             "constant uint kDrawIn = %d;\n"
             "struct FrameState { float mTime; int mFrame; int mDummy1; int mDummy2; };\n"
             "struct VertexData {\n"
             "    packed_float3 mPos;\n"
             "    uchar4 mColAlp;\n"
             "    uchar4 mDirInfo;\n"
             "    float mTim;\n"
             "};\n"
             "struct LayerState {\n"
             "    float4 mLayerToViewer[4];\n"
             "    float mLayerToViewerScale;\n"
             "    float mOpacity;\n"
             "    float mFlipSign;\n"
             "    float mDrawInTime;\n"
             "    float4 mAnimParams;\n"
             "    float4 mKeepAlive[2];\n"
             "    uint mID;\n"
             "};\n"
             "struct DisplayEye { float4 mViewerToEyePrj[4]; };\n"
             "struct DisplayState { DisplayEye mEye[2]; float2 mResolution; uint mEyeIndex; };\n"
             "struct VSOut { float4 position [[position]]; float4 color; uint mask [[flat]]; };\n"
             "struct FSOut { float4 color [[color(0)]]; uint sampleMask [[sample_mask]]; };\n"
             "float4 mul_row_major(constant float4 *m, float4 v) {\n"
             "    return float4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v));\n"
             "}\n"
             "float3 transform_dir_row_major(constant float4 *m, float3 v) {\n"
             "    return float3(dot(m[0].xyz, v), dot(m[1].xyz, v), dot(m[2].xyz, v));\n"
             "}\n"
             "vertex VSOut imm_pretessellated_paint_vertex(uint vid [[vertex_id]],\n"
             "                                             const device VertexData *vertices [[buffer(0)]],\n"
             "                                             constant FrameState &frame [[buffer(6)]],\n"
             "                                             constant LayerState &layer [[buffer(3)]],\n"
             "                                             constant DisplayState &display [[buffer(4)]]) {\n"
             "    VertexData vtx = vertices[vid];\n"
             "    float3 pos = float3(vtx.mPos);\n"
             "    if (kWiggle == 1u) {\n"
             "        pos += layer.mKeepAlive[0].z * sin(layer.mKeepAlive[0].x * pos.yzx + layer.mKeepAlive[0].y * frame.mTime);\n"
             "    }\n"
             "    float3 cpos = mul_row_major(layer.mLayerToViewer, float4(pos, 1.0)).xyz;\n"
             "    float3 ori = float3(vtx.mDirInfo.xyz) / 127.5 - 1.0;\n"
             "    uint info = uint(vtx.mDirInfo.w);\n"
             "    float directional = 1.0;\n"
             "    if (((info >> 7u) & 1u) == 0u) {\n"
             "        float3 wori = normalize(transform_dir_row_major(layer.mLayerToViewer, ori));\n"
             "        directional = clamp(dot(wori, normalize(cpos)), 0.0, 1.0);\n"
             "        directional *= directional;\n"
             "    }\n"
             "    float4 colAlpha = float4(vtx.mColAlp) / 255.0;\n"
             "    float3 col = (kColorCompressed == 0u) ? colAlpha.rgb * colAlpha.rgb : colAlpha.rgb;\n"
             "    float alpha = colAlpha.a * directional * layer.mOpacity;\n"
             "    if (kDrawIn == 1u) {\n"
             "        float drawingT = 2.0 * layer.mDrawInTime - vtx.mTim;\n"
             "        alpha *= smoothstep(layer.mAnimParams.z, layer.mAnimParams.z + layer.mAnimParams.w, drawingT);\n"
             "    }\n"
             "    uint eye = min(display.mEyeIndex, 1u);\n"
             "    VSOut out;\n"
             "    out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(cpos, 1.0));\n"
             "    out.color = float4(col, alpha);\n"
             "    out.mask = (alpha > 0.999) ? layer.mID : info;\n"
             "    return out;\n"
             "}\n"
             "uint imm_alpha_to_coverage(float alpha, float4 position, uint frameID, uint primitiveID, texture2d_array<float> blueNoise) {\n"
             "    const float MSAASampleCount = 8.0;\n"
             "    uint2 pixel = uint2(uint(position.x) & 63u, uint(position.y) & 63u);\n"
             "    uint slice = frameID & 63u;\n"
             "    float ran = blueNoise.read(pixel, slice, 0).x;\n"
             "    alpha = clamp(alpha + 0.99 * (ran - 0.5) / MSAASampleCount, 0.0, 1.0);\n"
             "    uint mask = (0xff00u >> uint(alpha * MSAASampleCount + 0.5)) & 0xffu;\n"
             "    uint shift = (uint(ran * 7.0) + primitiveID) & 7u;\n"
             "    uint barrel = (mask << 8u) | mask;\n"
             "    return (barrel >> shift) & 0xffu;\n"
             "}\n"
             "fragment FSOut imm_pretessellated_paint_fragment(VSOut in [[stage_in]],\n"
             "                                                   constant FrameState &frame [[buffer(6)]],\n"
             "                                                   texture2d_array<float> blueNoise [[texture(7)]]) {\n"
             "    FSOut out;\n"
             "    out.color = float4(in.color.rgb, 1.0);\n"
             "    out.sampleMask = imm_alpha_to_coverage(in.color.a, in.position, uint(frame.mFrame), in.mask, blueNoise);\n"
             "    return out;\n"
             "}\n",
             colorCompressed ? 1 : 0,
             wiggle ? 1 : 0,
             drawIn ? 1 : 0];
        vertexFunctionName = "imm_pretessellated_paint_vertex";
        fragmentFunctionName = "imm_pretessellated_paint_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = true;
        enableSourceAlphaBlending = false;
    }
    else if (isPictureShader)
    {
        source =
            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "struct VertexIn { packed_float2 position; packed_float4 color; };\n"
             "struct LayerState {\n"
             "    float4 mLayerToViewer[4];\n"
             "    float mLayerToViewerScale;\n"
             "    float mOpacity;\n"
             "    float mFlipSign;\n"
             "    float mDrawInTime;\n"
             "    float4 mAnimParams;\n"
             "    float4 mKeepAlive[2];\n"
             "    uint mID;\n"
             "};\n"
             "struct DisplayEye { float4 mViewerToEyePrj[4]; };\n"
             "struct DisplayState { DisplayEye mEye[2]; float2 mResolution; uint mEyeIndex; };\n"
             "struct PictureConstants { float4 size; };\n"
             "struct VSOut { float4 position [[position]]; float2 uv; };\n"
             "struct FSOut { float4 color [[color(0)]]; uint sampleMask [[sample_mask]]; };\n"
             "struct FrameState { float mTime; int mFrame; int mDummy1; int mDummy2; };\n"
             "float4 mul_row_major(constant float4 *m, float4 v) {\n"
             "    return float4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v));\n"
             "}\n"
             "uint imm_alpha_to_coverage(float alpha, float4 position, uint frameID, texture2d_array<float> blueNoise) {\n"
             "    const float MSAASampleCount = 8.0;\n"
             "    uint2 pixel = uint2(uint(position.x) & 63u, uint(position.y) & 63u);\n"
             "    float ran = blueNoise.read(pixel, frameID & 63u, 0).x;\n"
             "    alpha = clamp(alpha + 0.99 * (ran - 0.5) / MSAASampleCount, 0.0, 1.0);\n"
             "    uint mask = (0xff00u >> uint(alpha * MSAASampleCount + 0.5)) & 0xffu;\n"
             "    uint shift = uint(ran * 7.0) & 7u;\n"
             "    uint barrel = (mask << 8u) | mask;\n"
             "    return (barrel >> shift) & 0xffu;\n"
             "}\n"
             "vertex VSOut imm_picture2d_vertex(uint vid [[vertex_id]],\n"
             "                                  const device VertexIn *vertices [[buffer(0)]],\n"
             "                                  constant PictureConstants &picture [[buffer(16)]],\n"
             "                                  constant LayerState &layer [[buffer(3)]],\n"
             "                                  constant DisplayState &display [[buffer(4)]]) {\n"
             "    float2 p = vertices[vid].position;\n"
             "    float3 opos = float3(picture.size.xy * p, 0.0);\n"
             "    float3 wpos = mul_row_major(layer.mLayerToViewer, float4(opos, 1.0)).xyz;\n"
             "    uint eye = min(display.mEyeIndex, 1u);\n"
             "    VSOut out;\n"
             "    out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(wpos, 1.0));\n"
             "    out.uv = 0.5 + 0.5 * p * float2(1.0, -1.0);\n"
             "    return out;\n"
             "}\n"
             "fragment FSOut imm_picture2d_fragment(VSOut in [[stage_in]],\n"
             "                                    texture2d<float> tex [[texture(0)]],\n"
             "                                    texture2d_array<float> blueNoise [[texture(7)]],\n"
             "                                    sampler texSampler [[sampler(0)]],\n"
             "                                    constant FrameState &frame [[buffer(6)]],\n"
             "                                    constant LayerState &layer [[buffer(3)]]) {\n"
             "    float4 te = tex.sample(texSampler, in.uv);\n"
             "    FSOut out;\n"
             "    out.color = float4(te.rgb * te.rgb, 1.0);\n"
             "    out.sampleMask = imm_alpha_to_coverage(te.a * layer.mOpacity, in.position, uint(frame.mFrame), blueNoise);\n"
             "    return out;\n"
             "}\n";
        vertexFunctionName = "imm_picture2d_vertex";
        fragmentFunctionName = "imm_picture2d_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = true;
        enableSourceAlphaBlending = false;
    }
    else if (isPicture360EquirectShader)
    {
        source = [NSString stringWithFormat:
            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "constant uint kFormatIsStereo = %d;\n"
             "struct VertexIn { packed_float3 position; packed_float3 normal; };\n"
             "struct LayerState {\n"
             "    float4 mLayerToViewer[4];\n"
             "    float mLayerToViewerScale;\n"
             "    float mOpacity;\n"
             "    float mFlipSign;\n"
             "    float mDrawInTime;\n"
             "    float4 mAnimParams;\n"
             "    float4 mKeepAlive[2];\n"
             "    uint mID;\n"
             "};\n"
             "struct DisplayEye { float4 mViewerToEyePrj[4]; };\n"
             "struct DisplayState { DisplayEye mEye[2]; float2 mResolution; uint mEyeIndex; };\n"
             "struct VSOut { float4 position [[position]]; float3 direction; float4 scaleOffset; };\n"
             "struct FSOut { float4 color [[color(0)]]; uint sampleMask [[sample_mask]]; };\n"
             "struct FrameState { float mTime; int mFrame; int mDummy1; int mDummy2; };\n"
             "float4 mul_row_major(constant float4 *m, float4 v) {\n"
             "    return float4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v));\n"
             "}\n"
             "uint imm_alpha_to_coverage(float alpha, float4 position, uint frameID, texture2d_array<float> blueNoise) {\n"
             "    const float MSAASampleCount = 8.0;\n"
             "    uint2 pixel = uint2(uint(position.x) & 63u, uint(position.y) & 63u);\n"
             "    float ran = blueNoise.read(pixel, frameID & 63u, 0).x;\n"
             "    alpha = clamp(alpha + 0.99 * (ran - 0.5) / MSAASampleCount, 0.0, 1.0);\n"
             "    uint mask = (0xff00u >> uint(alpha * MSAASampleCount + 0.5)) & 0xffu;\n"
             "    uint shift = uint(ran * 7.0) & 7u;\n"
             "    uint barrel = (mask << 8u) | mask;\n"
             "    return (barrel >> shift) & 0xffu;\n"
             "}\n"
             "vertex VSOut imm_picture360_equirect_vertex(uint vid [[vertex_id]],\n"
             "                                           const device VertexIn *vertices [[buffer(0)]],\n"
             "                                           constant LayerState &layer [[buffer(3)]],\n"
             "                                           constant DisplayState &display [[buffer(4)]]) {\n"
             "    VertexIn vtx = vertices[vid];\n"
             "    float3 viewerPosition = mul_row_major(layer.mLayerToViewer, float4(vtx.position, 1.0)).xyz;\n"
             "    uint eye = min(display.mEyeIndex, 1u);\n"
             "    VSOut out;\n"
             "    out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(viewerPosition, 1.0));\n"
             "    // 360 pictures are backdrops; keep them at far depth so they do not cover paint.\n"
             "    out.position.z = out.position.w;\n"
             "    out.direction = normalize(vtx.position);\n"
             "    out.scaleOffset = kFormatIsStereo == 1u ? float4(1.0, 0.5, 0.0, 0.5 * float(eye)) : float4(1.0, 1.0, 0.0, 0.0);\n"
             "    return out;\n"
             "}\n"
             "fragment FSOut imm_picture360_equirect_fragment(VSOut in [[stage_in]],\n"
             "                                               texture2d<float> tex [[texture(0)]],\n"
             "                                               texture2d_array<float> blueNoise [[texture(7)]],\n"
             "                                               sampler texSampler [[sampler(0)]],\n"
             "                                               constant FrameState &frame [[buffer(6)]],\n"
             "                                               constant LayerState &layer [[buffer(3)]]) {\n"
             "    constexpr float pi = 3.14159265358979323846;\n"
             "    float3 nor = normalize(in.direction);\n"
             "    float2 uv = float2(0.5 + 0.5 * atan2(nor.x, -nor.z) / pi, acos(clamp(nor.y, -1.0, 1.0)) / pi);\n"
             "    uv = clamp(uv * in.scaleOffset.xy, float2(0.0), in.scaleOffset.xy) + in.scaleOffset.zw;\n"
             "    float4 te = tex.sample(texSampler, uv);\n"
             "    FSOut out;\n"
             "    out.color = float4(te.rgb * te.rgb, 1.0);\n"
             "    out.sampleMask = imm_alpha_to_coverage(te.a * layer.mOpacity, in.position, uint(frame.mFrame), blueNoise);\n"
             "    return out;\n"
             "}\n",
             formatIsStereo == 1 ? 1 : 0];
        vertexFunctionName = "imm_picture360_equirect_vertex";
        fragmentFunctionName = "imm_picture360_equirect_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = true;
        enableSourceAlphaBlending = false;
    }
    else if (isPicture360CubemapShader)
    {
        source =
            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "struct VertexIn { packed_float3 position; packed_float3 normal; };\n"
             "struct LayerState {\n"
             "    float4 mLayerToViewer[4];\n"
             "    float mLayerToViewerScale;\n"
             "    float mOpacity;\n"
             "    float mFlipSign;\n"
             "    float mDrawInTime;\n"
             "    float4 mAnimParams;\n"
             "    float4 mKeepAlive[2];\n"
             "    uint mID;\n"
             "};\n"
             "struct DisplayEye { float4 mViewerToEyePrj[4]; };\n"
             "struct DisplayState { DisplayEye mEye[2]; float2 mResolution; uint mEyeIndex; };\n"
             "struct VSOut { float4 position [[position]]; float3 direction; };\n"
             "struct FSOut { float4 color [[color(0)]]; uint sampleMask [[sample_mask]]; };\n"
             "struct FrameState { float mTime; int mFrame; int mDummy1; int mDummy2; };\n"
             "float4 mul_row_major(constant float4 *m, float4 v) {\n"
             "    return float4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v));\n"
             "}\n"
             "uint imm_alpha_to_coverage(float alpha, float4 position, uint frameID, texture2d_array<float> blueNoise) {\n"
             "    const float MSAASampleCount = 8.0;\n"
             "    uint2 pixel = uint2(uint(position.x) & 63u, uint(position.y) & 63u);\n"
             "    float ran = blueNoise.read(pixel, frameID & 63u, 0).x;\n"
             "    alpha = clamp(alpha + 0.99 * (ran - 0.5) / MSAASampleCount, 0.0, 1.0);\n"
             "    uint mask = (0xff00u >> uint(alpha * MSAASampleCount + 0.5)) & 0xffu;\n"
             "    uint shift = uint(ran * 7.0) & 7u;\n"
             "    uint barrel = (mask << 8u) | mask;\n"
             "    return (barrel >> shift) & 0xffu;\n"
             "}\n"
             "vertex VSOut imm_picture360_cubemap_vertex(uint vid [[vertex_id]],\n"
             "                                          const device VertexIn *vertices [[buffer(0)]],\n"
             "                                          constant LayerState &layer [[buffer(3)]],\n"
             "                                          constant DisplayState &display [[buffer(4)]]) {\n"
             "    VertexIn vtx = vertices[vid];\n"
             "    float3 viewerPosition = mul_row_major(layer.mLayerToViewer, float4(vtx.position, 1.0)).xyz;\n"
             "    uint eye = min(display.mEyeIndex, 1u);\n"
             "    VSOut out;\n"
             "    out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(viewerPosition, 1.0));\n"
             "    // 360 pictures are backdrops; keep them at far depth so they do not cover paint.\n"
             "    out.position.z = out.position.w;\n"
             "    out.direction = vtx.position;\n"
             "    return out;\n"
             "}\n"
             "fragment FSOut imm_picture360_cubemap_fragment(VSOut in [[stage_in]],\n"
             "                                              texturecube<float> tex [[texture(0)]],\n"
             "                                              texture2d_array<float> blueNoise [[texture(7)]],\n"
             "                                              sampler texSampler [[sampler(0)]],\n"
             "                                              constant FrameState &frame [[buffer(6)]],\n"
             "                                              constant LayerState &layer [[buffer(3)]]) {\n"
             "    float4 te = tex.sample(texSampler, normalize(in.direction));\n"
             "    FSOut out;\n"
             "    out.color = float4(te.rgb * te.rgb, 1.0);\n"
             "    out.sampleMask = imm_alpha_to_coverage(te.a * layer.mOpacity, in.position, uint(frame.mFrame), blueNoise);\n"
             "    return out;\n"
             "}\n";
        vertexFunctionName = "imm_picture360_cubemap_vertex";
        fragmentFunctionName = "imm_picture360_cubemap_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = true;
        enableSourceAlphaBlending = false;
    }
    else if (isModelShader)
    {
        source =
            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "struct VertexIn { packed_float3 position; packed_float3 color; packed_float3 normal; };\n"
             "struct LayerState {\n"
             "    float4 mLayerToViewer[4];\n"
             "    float mLayerToViewerScale;\n"
             "    float mOpacity;\n"
             "    float mFlipSign;\n"
             "    float mDrawInTime;\n"
             "    float4 mAnimParams;\n"
             "    float4 mKeepAlive[2];\n"
             "    uint mID;\n"
             "};\n"
             "struct DisplayEye { float4 mViewerToEyePrj[4]; };\n"
             "struct DisplayState { DisplayEye mEye[2]; float2 mResolution; uint mEyeIndex; };\n"
             "struct VSOut { float4 position [[position]]; float3 color; };\n"
             "struct FSOut { float4 color [[color(0)]]; uint sampleMask [[sample_mask]]; };\n"
             "struct FrameState { float mTime; int mFrame; int mDummy1; int mDummy2; };\n"
             "float4 mul_row_major(constant float4 *m, float4 v) {\n"
             "    return float4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v));\n"
             "}\n"
             "uint imm_alpha_to_coverage(float alpha, float4 position, uint frameID, texture2d_array<float> blueNoise) {\n"
             "    const float MSAASampleCount = 8.0;\n"
             "    uint2 pixel = uint2(uint(position.x) & 63u, uint(position.y) & 63u);\n"
             "    float ran = blueNoise.read(pixel, frameID & 63u, 0).x;\n"
             "    alpha = clamp(alpha + 0.99 * (ran - 0.5) / MSAASampleCount, 0.0, 1.0);\n"
             "    uint mask = (0xff00u >> uint(alpha * MSAASampleCount + 0.5)) & 0xffu;\n"
             "    uint shift = uint(ran * 7.0) & 7u;\n"
             "    uint barrel = (mask << 8u) | mask;\n"
             "    return (barrel >> shift) & 0xffu;\n"
             "}\n"
             "vertex VSOut imm_model_vertex(uint vid [[vertex_id]],\n"
             "                              const device VertexIn *vertices [[buffer(0)]],\n"
             "                              constant LayerState &layer [[buffer(3)]],\n"
             "                              constant DisplayState &display [[buffer(4)]]) {\n"
             "    VertexIn vtx = vertices[vid];\n"
             "    float3 viewerPosition = mul_row_major(layer.mLayerToViewer, float4(vtx.position, 1.0)).xyz;\n"
             "    uint eye = min(display.mEyeIndex, 1u);\n"
             "    VSOut out;\n"
             "    out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(viewerPosition, 1.0));\n"
             "    out.color = vtx.color;\n"
             "    return out;\n"
             "}\n"
             "fragment FSOut imm_model_fragment(VSOut in [[stage_in]],\n"
             "                                  texture2d_array<float> blueNoise [[texture(7)]],\n"
             "                                  constant FrameState &frame [[buffer(6)]],\n"
             "                                  constant LayerState &layer [[buffer(3)]]) {\n"
             "    FSOut out;\n"
             "    out.color = float4(in.color, 1.0);\n"
             "    out.sampleMask = imm_alpha_to_coverage(layer.mOpacity, in.position, uint(frame.mFrame), blueNoise);\n"
             "    return out;\n"
             "}\n";
        vertexFunctionName = "imm_model_vertex";
        fragmentFunctionName = "imm_model_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = true;
        enableSourceAlphaBlending = false;
    }
	    else
	    {
	        source =
	            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "struct VertexIn { packed_float2 position; packed_float4 color; };\n"
             "struct VSOut { float4 position [[position]]; float4 color; };\n"
             "vertex VSOut imm_vertex(uint vid [[vertex_id]], const device VertexIn *vertices [[buffer(0)]]) {\n"
             "    VSOut out;\n"
             "    out.position = float4(vertices[vid].position, 0.0, 1.0);\n"
             "    out.color = vertices[vid].color;\n"
             "    return out;\n"
             "}\n"
             "fragment float4 imm_fragment(VSOut in [[stage_in]]) { return in.color; }\n";
        vertexFunctionName = "imm_vertex";
        fragmentFunctionName = "imm_fragment";
        colorFormat = MTLPixelFormatBGRA8Unorm;
	        requiresVertexBuffer = true;
	    }

	    if (mState->externalShaderAdjust || iMetalEnvFlagEnabled("IMM_METAL_EXTERNAL_SHADER_ADJUST"))
	    {
	        source = [source stringByReplacingOccurrencesOfString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(bWPos, 1.0));" withString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(bWPos, 1.0)); out.position.y = -out.position.y;"];
	        source = [source stringByReplacingOccurrencesOfString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(cpos, 1.0));" withString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(cpos, 1.0)); out.position.y = -out.position.y;"];
	        source = [source stringByReplacingOccurrencesOfString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(wpos, 1.0));" withString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(wpos, 1.0)); out.position.y = -out.position.y;"];
	        source = [source stringByReplacingOccurrencesOfString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(viewerPosition, 1.0));" withString:@"out.position = mul_row_major(display.mEye[eye].mViewerToEyePrj, float4(viewerPosition, 1.0)); out.position.y = -out.position.y;"];
	    }

	    if (mState->unityProjectionAdjusted)
	    {
	        // Unity's managed projection resolver already supplies the
	        // render-target-adjusted GPU projection, so no native Y inversion
	        // is needed here.
	        // Unity's Metal targets use reversed Z. A 360 backdrop placed at
	        // z=w becomes the nearest surface and occludes every paint layer;
	        // keep the Unity-hosted backdrop at the far plane instead.
	        source = [source stringByReplacingOccurrencesOfString:@"out.position.z = out.position.w;" withString:@"out.position.z = 0.0;"];
	    }
	
	    NSError *compileError = nil;
    id<MTLLibrary> library = [mState->device newLibraryWithSource:source options:nil error:&compileError];
    if (!library)
    {
        char message[2048];
        snprintf(message, sizeof(message), "Metal library compile failed: %s", compileError.localizedDescription.UTF8String ?: "unknown error");
        fprintf(stderr, "%s\n", message);
        iError(mReporter, message);
        if (error)
        {
            snprintf(error, 2048, "%s", message);
        }
        return nullptr;
    }

    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.vertexFunction = [library newFunctionWithName:[NSString stringWithUTF8String:vertexFunctionName]];
    descriptor.fragmentFunction = [library newFunctionWithName:[NSString stringWithUTF8String:fragmentFunctionName]];
    descriptor.colorAttachments[0].pixelFormat = colorFormat;
    if (enableSourceAlphaBlending)
    {
        iConfigureSourceAlphaBlend(descriptor.colorAttachments[0]);
    }
    descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

    NSError *pipelineError = nil;
    id<MTLRenderPipelineState> pipeline = [mState->device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
    if (!pipeline)
    {
        char message[2048];
        snprintf(message, sizeof(message), "Metal pipeline creation failed: %s", pipelineError.localizedDescription.UTF8String ?: "unknown error");
        fprintf(stderr, "%s\n", message);
        iError(mReporter, message);
        if (error)
        {
            snprintf(error, 2048, "%s", message);
        }
        return nullptr;
    }

    piShaderS *shader = new piShaderS();
    shader->descriptor = descriptor;
    shader->pipelineColorWrite = pipeline;
    shader->requiresVertexBuffer = requiresVertexBuffer;
    if (error) error[0] = 0;
    ++mState->liveShaders;
    return shader;
}
piShader piRendererMetal::CreateShaderBinary(const piShaderOptions *, const uint8_t *vs, const int vs_len, const uint8_t *, const int, const uint8_t *, const int, const uint8_t *, const int, const uint8_t *fs, const int fs_len, char *error)
{
    if (!mState->device)
    {
        if (error) strcpy(error, "Metal device is not initialized");
        return nullptr;
    }

    const bool isResolveShader = vs && vs_len > 0 && fs && fs_len > 0;
    NSString *source = nil;
    const char *vertexFunctionName = nullptr;
    const char *fragmentFunctionName = nullptr;
    MTLPixelFormat colorFormat = MTLPixelFormatBGRA8Unorm;
    bool requiresVertexBuffer = true;

    if (isResolveShader)
    {
        source =
            @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "struct VertexIn { packed_float2 position; packed_float4 color; };\n"
             "struct VSOut { float4 position [[position]]; float2 uv; };\n"
             "vertex VSOut imm_resolve_vertex(uint vid [[vertex_id]], const device VertexIn *vertices [[buffer(0)]]) {\n"
             "    VSOut out;\n"
             "    float2 p = vertices[vid].position;\n"
             "    out.position = float4(p, 0.0, 1.0);\n"
             "    out.uv = float2(0.5 + 0.5 * p.x, 0.5 - 0.5 * p.y);\n"
             "    return out;\n"
             "}\n"
             "struct ResolveConstants { float4 fade; };\n"
             "fragment float4 imm_resolve_fragment(VSOut in [[stage_in]],\n"
             "                                     texture2d<float> tex [[texture(0)]],\n"
             "                                     constant ResolveConstants &c [[buffer(17)]]) {\n"
             "    constexpr sampler s(address::clamp_to_edge, filter::linear);\n"
             "    float3 col = tex.sample(s, in.uv).rgb;\n"
             "    col = select(1.055 * pow(col, float3(1.0 / 2.4)) - 0.055, 12.92 * col, col < float3(0.0031308));\n"
             "    return float4(col * c.fade.x, 1.0);\n"
             "}\n";
        vertexFunctionName = "imm_resolve_vertex";
        fragmentFunctionName = "imm_resolve_fragment";
        colorFormat = MTLPixelFormatBGRA8Unorm;
        requiresVertexBuffer = true;
    }
    else
    {
        source =
             @"#include <metal_stdlib>\n"
             "using namespace metal;\n"
             "struct VSOut { float4 position [[position]]; float4 color; };\n"
             "vertex VSOut imm_placeholder_vertex(uint vid [[vertex_id]]) {\n"
             "    constexpr float2 p[6] = { float2(-0.75, -0.65), float2(0.75, -0.65), float2(0.0, 0.70), float2(-0.85, 0.80), float2(-0.35, 0.80), float2(-0.85, 0.30) };\n"
             "    constexpr float4 c[6] = { float4(0.9, 0.2, 0.15, 1.0), float4(0.15, 0.65, 0.95, 1.0), float4(0.95, 0.85, 0.2, 1.0), float4(0.1, 0.95, 0.2, 1.0), float4(0.1, 0.95, 0.2, 1.0), float4(0.1, 0.95, 0.2, 1.0) };\n"
             "    VSOut out;\n"
             "    uint i = vid % 6;\n"
             "    out.position = float4(p[i], 0.0, 1.0);\n"
             "    out.color = c[i];\n"
             "    return out;\n"
             "}\n"
             "fragment float4 imm_placeholder_fragment(VSOut in [[stage_in]]) { return in.color; }\n";
        vertexFunctionName = "imm_placeholder_vertex";
        fragmentFunctionName = "imm_placeholder_fragment";
        colorFormat = MTLPixelFormatRG11B10Float;
        requiresVertexBuffer = false;
    }

    NSError *compileError = nil;
    id<MTLLibrary> library = [mState->device newLibraryWithSource:source options:nil error:&compileError];
    if (!library)
    {
        char message[2048];
        snprintf(message, sizeof(message), "Metal resolve library compile failed: %s", compileError.localizedDescription.UTF8String ?: "unknown error");
        iError(mReporter, message);
        if (error)
        {
            snprintf(error, 2048, "%s", message);
        }
        return nullptr;
    }

    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.vertexFunction = [library newFunctionWithName:[NSString stringWithUTF8String:vertexFunctionName]];
    descriptor.fragmentFunction = [library newFunctionWithName:[NSString stringWithUTF8String:fragmentFunctionName]];
    descriptor.colorAttachments[0].pixelFormat = colorFormat;
    descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

    NSError *pipelineError = nil;
    id<MTLRenderPipelineState> pipeline = [mState->device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
    if (!pipeline)
    {
        char message[2048];
        snprintf(message, sizeof(message), "Metal resolve pipeline creation failed: %s", pipelineError.localizedDescription.UTF8String ?: "unknown error");
        iError(mReporter, message);
        if (error)
        {
            snprintf(error, 2048, "%s", message);
        }
        return nullptr;
    }

    piShaderS *shader = new piShaderS();
    shader->descriptor = descriptor;
    shader->pipelineColorWrite = pipeline;
    shader->requiresVertexBuffer = requiresVertexBuffer;
    if (error) error[0] = 0;
    ++mState->liveShaders;
    return shader;
}
void piRendererMetal::DestroyShader(piShader obj)
{
    if (!obj) return;
    obj->descriptor = nil;
    obj->pipelineColorWrite = nil;
    obj->pipelineNoColorWrite = nil;
    obj->pipelineSourceAlphaBlend = nil;
    obj->pipelineTargetColorWrite = nil;
    obj->pipelineTargetNoColorWrite = nil;
    obj->pipelineTargetSourceAlphaBlend = nil;
    if (mState->liveShaders > 0) --mState->liveShaders;
    delete obj;
}
void piRendererMetal::AttachShader(piShader obj) { mState->currentShader = obj; }
void piRendererMetal::DettachShader(void) { mState->currentShader = nullptr; }
piShader piRendererMetal::CreateCompute(const piShaderOptions *, const char *, char *error)
{
    const char *message = "Metal compute is not implemented yet";
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::Compute, message);
    if (error) strcpy(error, message);
    return nullptr;
}

void piRendererMetal::SetShaderConstant4F(const unsigned int pos, const float *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(float) * 4 * (size_t)num); }
void piRendererMetal::SetShaderConstant3F(const unsigned int pos, const float *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(float) * 3 * (size_t)num); }
void piRendererMetal::SetShaderConstant2F(const unsigned int pos, const float *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(float) * 2 * (size_t)num); }
void piRendererMetal::SetShaderConstant1F(const unsigned int pos, const float *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(float) * (size_t)num); }
void piRendererMetal::SetShaderConstant1I(const unsigned int pos, const int *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(int) * (size_t)num); }
void piRendererMetal::SetShaderConstant1UI(const unsigned int pos, const unsigned int *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(unsigned int) * (size_t)num); }
void piRendererMetal::SetShaderConstant2UI(const unsigned int pos, const unsigned int *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(unsigned int) * 2 * (size_t)num); }
void piRendererMetal::SetShaderConstant3UI(const unsigned int pos, const unsigned int *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(unsigned int) * 3 * (size_t)num); }
void piRendererMetal::SetShaderConstant4UI(const unsigned int pos, const unsigned int *value, int num) { iSetImmediateConstant(mState, pos, value, sizeof(unsigned int) * 4 * (size_t)num); }
void piRendererMetal::SetShaderConstantMat4F(const unsigned int pos, const float *value, int num, bool) { iSetImmediateConstant(mState, pos, value, sizeof(float) * 16 * (size_t)num); }
void piRendererMetal::SetShaderConstantSampler(const unsigned int, int) {}
void piRendererMetal::AttachShaderConstants(piBuffer obj, int unit)
{
    if (unit >= 0 && unit < 16)
    {
        mState->constantBuffers[unit] = obj;
    }
}
void piRendererMetal::AttachShaderBuffer(piBuffer obj, int unit)
{
    if (unit >= 0 && unit < 16)
    {
        mState->shaderBuffers[unit] = obj;
    }
}
void piRendererMetal::DettachShaderBuffer(int unit)
{
    if (unit >= 0 && unit < 16)
    {
        mState->shaderBuffers[unit] = nullptr;
    }
}
void piRendererMetal::AttachAtomicsBuffer(piBuffer, int)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::Atomics, "Metal renderer does not support atomic buffer bindings yet");
}
void piRendererMetal::DettachAtomicsBuffer(int)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::Atomics, "Metal renderer does not support atomic buffer bindings yet");
}

piBuffer piRendererMetal::CreateBuffer(const void *data, unsigned int amount, BufferType mode, BufferUse use)
{
    if (!mState->device || amount == 0)
    {
        return nullptr;
    }

    piBufferS *buffer = new piBufferS();
    buffer->size = amount;
    buffer->type = mode;
    buffer->use = use;
    if (data)
    {
        buffer->buffer = [mState->device newBufferWithBytes:data length:amount options:MTLResourceStorageModeShared];
    }
    else
    {
        buffer->buffer = [mState->device newBufferWithLength:amount options:MTLResourceStorageModeShared];
    }

    if (!buffer->buffer)
    {
        delete buffer;
        return nullptr;
    }
    ++mState->liveBuffers;
    return buffer;
}
piBuffer piRendererMetal::CreateStructuredBuffer(const void *data, unsigned int numElements, unsigned int elementSize, BufferType mode, BufferUse use)
{
    return CreateBuffer(data, numElements * elementSize, mode, use);
}
piBuffer piRendererMetal::CreateBufferMapped_Start(void **ptr, unsigned int amount, BufferUse use)
{
    piBuffer buffer = CreateBuffer(nullptr, amount, BufferType::Dynamic, use);
    if (ptr) *ptr = (buffer && buffer->buffer) ? [buffer->buffer contents] : nullptr;
    return buffer;
}
void piRendererMetal::CreateBufferMapped_End(piBuffer) {}
void piRendererMetal::DestroyBuffer(piBuffer obj)
{
    if (!obj) return;
    [obj->buffer release];
    obj->buffer = nil;
    if (mState->liveBuffers > 0) --mState->liveBuffers;
    delete obj;
}
void piRendererMetal::UpdateBuffer(piBuffer obj, const void *data, int offset, int len, bool)
{
    if (!obj || !obj->buffer || !data || offset < 0 || len < 0) return;
    if ((unsigned int)(offset + len) > obj->size) return;

    if (mState->frameActive && mState->commandBuffer && obj->type == BufferType::Dynamic)
    {
        id<MTLBuffer> replacement = [mState->device newBufferWithLength:obj->size options:MTLResourceStorageModeShared];
        if (!replacement) return;

        id<MTLBuffer> previous = obj->buffer;
        memcpy([replacement contents], [previous contents], (size_t)obj->size);
        if (mState->retainedBuffers)
        {
            // The array retains the previous allocation through command-buffer
            // completion; relinquish the piBuffer wrapper's ownership below.
            [mState->retainedBuffers addObject:previous];
        }
        obj->buffer = replacement;
        [previous release];
    }

    memcpy((char *)[obj->buffer contents] + offset, data, (size_t)len);
}
void piRendererMetal::AttachPixelPackBuffer(piBuffer)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::PixelPackBuffer, "Metal renderer does not support pixel pack buffers yet");
}
void piRendererMetal::DettachPixelPackBuffer(void)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::PixelPackBuffer, "Metal renderer does not support pixel pack buffers yet");
}

piVertexArray piRendererMetal::CreateVertexArray(int, piBuffer vb0, const piRArrayLayout *, piBuffer vb1, const piRArrayLayout *, piBuffer eb, const IndexArrayFormat ebFormat)
{
    piVertexArrayS *vertexArray = new piVertexArrayS();
    vertexArray->vertexBuffer[0] = vb0;
    vertexArray->vertexBuffer[1] = vb1;
    vertexArray->indexBuffer = eb;
    vertexArray->indexFormat = ebFormat;
    ++mState->liveVertexArrays;
    return vertexArray;
}
void piRendererMetal::DestroyVertexArray(piVertexArray obj)
{
    if (!obj) return;
    if (mState->liveVertexArrays > 0) --mState->liveVertexArrays;
    delete obj;
}
void piRendererMetal::AttachVertexArray(piVertexArray obj) { mState->currentVertexArray = obj; }
void piRendererMetal::DettachVertexArray(void) { mState->currentVertexArray = nullptr; }
piVertexArray piRendererMetal::CreateVertexArray2(int, piBuffer vb0, const ArrayLayout2 *, piBuffer vb1, const ArrayLayout2 *, const void *, size_t, piBuffer ib, const IndexArrayFormat ebFormat)
{
    return CreateVertexArray(0, vb0, nullptr, vb1, nullptr, ib, ebFormat);
}
void piRendererMetal::AttachVertexArray2(piVertexArray vme) { AttachVertexArray(vme); }
void piRendererMetal::DestroyVertexArray2(piVertexArray vme)
{
    if (!vme) return;
    if (mState->liveVertexArrays > 0) --mState->liveVertexArrays;
    delete vme;
}

piQuery piRendererMetal::CreateQuery(piRenderer::QueryType type)
{
    piQueryS *query = new piQueryS();
    query->type = type;
    ++mState->liveQueries;
    return query;
}
void piRendererMetal::DestroyQuery(piQuery vme)
{
    if (!vme) return;
    if (mState->liveQueries > 0) --mState->liveQueries;
    delete vme;
}
void piRendererMetal::BeginQuery(piQuery vme)
{
    if (!vme)
    {
        return;
    }
    vme->startTicks = iMetalCpuTimeNanoseconds();
    vme->active = true;
}
void piRendererMetal::EndQuery(piQuery vme)
{
    if (!vme || !vme->active)
    {
        return;
    }
    const uint64_t now = iMetalCpuTimeNanoseconds();
    vme->resultNanoseconds = now >= vme->startTicks ? now - vme->startTicks : 0;
    vme->active = false;
}
uint64_t piRendererMetal::GetQueryResult(piQuery vme)
{
    return vme ? vme->resultNanoseconds : 0;
}

void piRendererMetal::DrawPrimitiveIndexed(PrimitiveType pt, uint32_t num, uint32_t numInstances, uint32_t baseVertex, uint32_t, uint32_t baseIndex)
{
    if (mState)
    {
        mState->debugIndexedDrawCalls++;
    }
    if (!mState->frameActive || !mState->activeRenderPass || !mState->commandBuffer || !mState->currentShader ||
        !mState->currentVertexArray || !mState->currentVertexArray->indexBuffer ||
        !mState->currentVertexArray->indexBuffer->buffer)
    {
        mState->debugSkippedDrawCalls++;
        return;
    }
    if (iSuppressDrawCallsLevel() >= 3)
    {
        mState->debugSkippedDrawCalls++;
        return;
    }

    if (!mState->encoder)
    {
        mState->encoder = [mState->commandBuffer renderCommandEncoderWithDescriptor:mState->activeRenderPass];
        iApplyEncoderState(mState);
    }
    if (iSuppressDrawCallsLevel() >= 2)
    {
        mState->passTouched = true;
        return;
    }

    id<MTLRenderPipelineState> pipeline = iGetPipelineForCurrentState(mState, mState->currentShader, mReporter);
    if (!mState->encoder || !pipeline)
    {
        mState->debugSkippedDrawCalls++;
        return;
    }
    if (mState->currentShader->requiresVertexBuffer &&
        (!mState->currentVertexArray->vertexBuffer[0] || !mState->currentVertexArray->vertexBuffer[0]->buffer))
    {
        mState->debugSkippedDrawCalls++;
        return;
    }

    [mState->encoder setRenderPipelineState:pipeline];
    iBindCommonDrawResources(mState);
    if (mState->currentVertexArray->vertexBuffer[0] && mState->currentVertexArray->vertexBuffer[0]->buffer)
    {
        [mState->encoder setVertexBuffer:mState->currentVertexArray->vertexBuffer[0]->buffer offset:0 atIndex:0];
    }
    if (iSuppressDrawCallsLevel() >= 1)
    {
        mState->passTouched = true;
        return;
    }

    const NSUInteger indexSize = mState->currentVertexArray->indexFormat == IndexArrayFormat::UINT_16 ? 2 : 4;
    [mState->encoder drawIndexedPrimitives:iPrimitivePiToMetal(pt)
                                indexCount:(NSUInteger)num
                                 indexType:iIndexPiToMetal(mState->currentVertexArray->indexFormat)
                               indexBuffer:mState->currentVertexArray->indexBuffer->buffer
                         indexBufferOffset:(NSUInteger)baseIndex * indexSize
                             instanceCount:(NSUInteger)(numInstances < 1 ? 1 : numInstances)
                                baseVertex:(NSInteger)baseVertex
                              baseInstance:0];
    mState->debugIssuedDrawCalls++;
    mState->passTouched = true;
}
void piRendererMetal::DrawPrimitiveIndirect(PrimitiveType pt, piBuffer cmds, uint32_t offset, uint32_t num)
{
    if (!cmds || !cmds->buffer || num == 0 || offset > cmds->size)
    {
        return;
    }
    const size_t bytesAvailable = (size_t)cmds->size - (size_t)offset;
    const size_t maxCommands = bytesAvailable / sizeof(piDrawElementsIndirectCommand);
    const size_t commandsToDraw = num < maxCommands ? (size_t)num : maxCommands;
    const piDrawElementsIndirectCommand *commands = (const piDrawElementsIndirectCommand *)((const uint8_t *)[cmds->buffer contents] + offset);
    for (size_t i = 0; i < commandsToDraw; ++i)
    {
        const piDrawElementsIndirectCommand &cmd = commands[i];
        if (cmd.count > 0 && cmd.instanceCount > 0)
        {
            DrawPrimitiveIndexed(pt, cmd.count, cmd.instanceCount, cmd.baseVertex, cmd.baseInstance, cmd.firstIndex);
        }
    }
}
void piRendererMetal::DrawPrimitiveNotIndexed(PrimitiveType pt, int first, int num, int numInstances)
{
    if (mState)
    {
        mState->debugNonIndexedDrawCalls++;
    }
    if (!mState->frameActive || !mState->activeRenderPass || !mState->commandBuffer || !mState->currentShader)
    {
        mState->debugSkippedDrawCalls++;
        return;
    }
    if (iSuppressDrawCallsLevel() >= 3)
    {
        mState->debugSkippedDrawCalls++;
        return;
    }

    if (!mState->encoder)
    {
        mState->encoder = [mState->commandBuffer renderCommandEncoderWithDescriptor:mState->activeRenderPass];
        iApplyEncoderState(mState);
    }
    if (iSuppressDrawCallsLevel() >= 2)
    {
        mState->passTouched = true;
        return;
    }

    id<MTLRenderPipelineState> pipeline = iGetPipelineForCurrentState(mState, mState->currentShader, mReporter);
    if (!mState->encoder || !pipeline)
    {
        mState->debugSkippedDrawCalls++;
        return;
    }
    if (mState->currentShader->requiresVertexBuffer &&
        (!mState->currentVertexArray || !mState->currentVertexArray->vertexBuffer[0] || !mState->currentVertexArray->vertexBuffer[0]->buffer))
    {
        mState->debugSkippedDrawCalls++;
        return;
    }

    [mState->encoder setRenderPipelineState:pipeline];
    iBindCommonDrawResources(mState);
    if (mState->currentVertexArray && mState->currentVertexArray->vertexBuffer[0] && mState->currentVertexArray->vertexBuffer[0]->buffer)
    {
        [mState->encoder setVertexBuffer:mState->currentVertexArray->vertexBuffer[0]->buffer offset:0 atIndex:0];
    }
    if (iSuppressDrawCallsLevel() >= 1)
    {
        mState->passTouched = true;
        return;
    }
    [mState->encoder drawPrimitives:iPrimitivePiToMetal(pt)
                        vertexStart:(NSUInteger)first
                        vertexCount:(NSUInteger)num
                      instanceCount:(NSUInteger)(numInstances < 1 ? 1 : numInstances)];
    mState->debugIssuedDrawCalls++;
    mState->passTouched = true;
}
void piRendererMetal::DrawPrimitiveNotIndexedMultiple(PrimitiveType pt, const int *firsts, const int *counts, int num)
{
    if (!firsts || !counts || num <= 0)
    {
        return;
    }
    for (int i = 0; i < num; ++i)
    {
        DrawPrimitiveNotIndexed(pt, firsts[i], counts[i], 1);
    }
}
void piRendererMetal::DrawPrimitiveNotIndexedIndirect(PrimitiveType pt, piBuffer cmds, int num)
{
    if (!cmds || !cmds->buffer || num <= 0)
    {
        return;
    }
    const size_t maxCommands = (size_t)cmds->size / sizeof(piDrawArraysIndirectCommand);
    const size_t commandsToDraw = (size_t)num < maxCommands ? (size_t)num : maxCommands;
    const piDrawArraysIndirectCommand *commands = (const piDrawArraysIndirectCommand *)[cmds->buffer contents];
    for (size_t i = 0; i < commandsToDraw; ++i)
    {
        const piDrawArraysIndirectCommand &cmd = commands[i];
        if (cmd.count > 0 && cmd.instanceCount > 0)
        {
            DrawPrimitiveNotIndexed(pt, (int)cmd.first, (int)cmd.count, (int)cmd.instanceCount);
        }
    }
}
void piRendererMetal::DettachIndirectBuffer(void) {}
void piRendererMetal::DrawUnitCube_XYZ_NOR(int numInstanced)
{
    if (!mState->unitCubePositionNormalVertexArray)
    {
        return;
    }
    AttachVertexArray(mState->unitCubePositionNormalVertexArray);
    for (int face = 0; face < 6; ++face)
    {
        DrawPrimitiveNotIndexed(PrimitiveType::TriangleStrip, face * 4, 4, numInstanced);
    }
    DettachVertexArray();
}
void piRendererMetal::DrawUnitCube_XYZ(int numInstanced)
{
    if (!mState->unitCubePositionVertexArray)
    {
        return;
    }
    AttachVertexArray(mState->unitCubePositionVertexArray);
    for (int face = 0; face < 6; ++face)
    {
        DrawPrimitiveNotIndexed(PrimitiveType::TriangleStrip, face * 4, 4, numInstanced);
    }
    DettachVertexArray();
}
void piRendererMetal::DrawUnitQuad_XY(int numInstanced)
{
    if (!mState->unitQuadVertexArray)
    {
        return;
    }
    AttachVertexArray(mState->unitQuadVertexArray);
    DrawPrimitiveNotIndexed(PrimitiveType::TriangleStrip, 0, 4, numInstanced);
    DettachVertexArray();
}
void piRendererMetal::ExecuteCompute(int, int, int, int, int, int)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::Compute, "Metal compute is not implemented yet");
}
void piRendererMetal::CreateSyncObject(piBuffer &buffer) { buffer = nullptr; }
bool piRendererMetal::CheckSyncObject(piBuffer &) { return true; }
void piRendererMetal::SetPointSize(bool mode, float)
{
    if (mode)
    {
        iUnsupported(mState, mReporter, piMetalUnsupportedFeature::PointSize, "Metal renderer does not support dynamic point size yet");
    }
}
void piRendererMetal::SetLineWidth(float size)
{
    if (size != 1.0f)
    {
        iUnsupported(mState, mReporter, piMetalUnsupportedFeature::LineWidth, "Metal renderer does not support non-default line width");
    }
}
void piRendererMetal::PolygonOffset(bool, bool, float, float)
{
    iUnsupported(mState, mReporter, piMetalUnsupportedFeature::PolygonOffset, "Metal renderer does not support polygon offset yet");
}
void piRendererMetal::RenderMemoryBarrier(BarrierType) {}

} // namespace ImmCore
