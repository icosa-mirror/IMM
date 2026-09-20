#define CUSTOM_ALPHA_TO_COVERAGE 1


// IQ-TODO: check if Quest support glDrawIndirect, and if so, add it! 

#include <cstdlib>
#ifdef RENDER_BUDGET
#include <algorithm>
#endif

#include "libImmImporter/src/document/layerPaint/element.h"
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerPaint.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/document/layerPaint/drawingStatic.h"
#include "../../../blue_noise.h"


#include "layerRendererPaintStatic.h"

using namespace ImmCore;
using namespace ImmImporter;

namespace ImmPlayer
{
#if defined(WINDOWS)
    #include "tmp/shader_static_brush_vs_hlsl.inc"
    #include "tmp/shader_static_brush_fs_hlsl.inc"
    #include "tmp/shader_static_brush_vs_spirv.inc"
    #include "tmp/shader_static_brush_fs_spirv.inc"
    #include "shader_static_brush_vs.glsl"
    #include "shader_static_brush_fs.glsl"
#elif defined(ANDROID)
    #include "shader_static_brush_vs.es.glsl"
    #include "shader_static_brush_fs.es.glsl"
    #include "tmp/shader_static_brush_vs_spirv.inc"
    #include "tmp/shader_static_brush_fs_spirv.inc"
#else
    #include "shader_static_brush_vs.glsl"
    #include "shader_static_brush_fs.glsl"
#endif


    typedef struct
    {
        uint32_t mVertexOffset;
        float    mBiggestStroke;
    }ChunkData;

    static int iForcedPaintBrushType()
    {
        const char *value = std::getenv("IMM_FORCE_PAINT_BRUSH_TYPE");
        return (value && value[0]) ? atoi(value) : -1;
    }

    static bool iPaintLayerAllowed(uint32_t layerId)
    {
        const char *value = std::getenv("IMM_FORCE_PAINT_LAYER_IDS");
        if (value && value[0])
        {
            const char *cursor = value;
            while (*cursor)
            {
                char *end = nullptr;
                const long id = strtol(cursor, &end, 10);
                if (end != cursor && id >= 0 && static_cast<uint32_t>(id) == layerId) return true;
                cursor = (*end == ',') ? end + 1 : end;
                if (cursor == end) break;
            }
            return false;
        }

        value = std::getenv("IMM_FORCE_PAINT_LAYER_ID");
        return !(value && value[0]) || static_cast<uint32_t>(atoi(value)) == layerId;
    }

    
    struct iSLayerDrawInfoStatic
    {
        struct BufferData
        {
            piBuffer		  mVertexData;
            piBuffer		  mIBO;
            piVertexArray	  mVertexArray[3];
            piTArray<const DrawingStatic::Geometry::Chunk *> mChunks;
        }mBuffers[LayerRendererPaintStatic::kNumChunkTypes];
        bool              mUploaded;

        //-----
        const DrawingStatic::Geometry *mGeometry;
        LayersState       mLayerState; // per camera pass
        bool              mDrawin;     // per camera pass
        bool              mWiggle;     // per camera pass
        #ifdef RENDER_BUDGET
        float        mDistance;
        #endif

        //-----

        bool Init( const Drawing *dr)
        {
            mUploaded = false;

            mGeometry = dynamic_cast<const DrawingStatic *> (dr)->GetGeometry();

            for (int chunkType = 0; chunkType < LayerRendererPaintStatic::kNumChunkTypes; chunkType++)
            {
                BufferData *buf = mBuffers + chunkType;

                //mBuffers[i].mChunkData = nullptr;
                buf->mVertexData = nullptr;
                buf->mIBO = nullptr;
                buf->mVertexArray[0] = nullptr;
                buf->mVertexArray[1] = nullptr;
                buf->mVertexArray[2] = nullptr;

                const uint32_t numChunks = dr->GetNumGeometryChunks(chunkType);

                if (!buf->mChunks.Init(numChunks, false))
                    return false;
            }

            return true;
        }

        void End(void)
        {
            for (int chunkType = 0; chunkType < LayerRendererPaintStatic::kNumChunkTypes; chunkType++)
            {
                mBuffers[chunkType].mChunks.End();
            }
        }

        void Download(piRenderer* renderer, piLog *log)
        {
            for (int chunkType = 0; chunkType < LayerRendererPaintStatic::kNumChunkTypes; chunkType++)
            {
                if (renderer->GetAPI() == piRenderer::API::DX)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        if (mBuffers[chunkType].mVertexArray[j] != nullptr)
                            renderer->DestroyVertexArray2(mBuffers[chunkType].mVertexArray[j]);
                        mBuffers[chunkType].mVertexArray[j] = nullptr;
                    }
                }
                else
                {
                    if (mBuffers[chunkType].mVertexArray[0] != nullptr)
                        renderer->DestroyVertexArray(mBuffers[chunkType].mVertexArray[0]);
                    mBuffers[chunkType].mVertexArray[0] = nullptr;
                }

                if (mBuffers[chunkType].mVertexData != nullptr)
                    renderer->DestroyBuffer(mBuffers[chunkType].mVertexData);
                if (mBuffers[chunkType].mIBO != nullptr)
                    renderer->DestroyBuffer(mBuffers[chunkType].mIBO);
                mBuffers[chunkType].mVertexData = nullptr;
                mBuffers[chunkType].mIBO = nullptr;
            }
            mUploaded = false;
        }


        bool Upload(piRenderer* renderer, piLog *log)
        {
            for (int chunkType = 0; chunkType < LayerRendererPaintStatic::kNumChunkTypes; chunkType++)
            {
                BufferData *dst = mBuffers + chunkType;
                const DrawingStatic::Geometry::Data *src = mGeometry->mBuffers + chunkType;
                if (src->mPoints.GetLength() == 0) continue;

                const uint32_t nv = static_cast<uint32_t>(src->mPoints.GetLength());
                const uint32_t ni = static_cast<uint32_t>(src->mIndices.GetLength());

                dst->mVertexData = renderer->CreateStructuredBuffer(src->mPoints.GetAddress(0), nv, sizeof(DrawingStatic::MyVertexFormat), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::ShaderResource);
                if (dst->mVertexData == nullptr)
                {
                    log->Printf(LT_ERROR, L"Couldn't create data resource");
                    Download(renderer, log);
                    return false;
                }

                dst->mIBO = renderer->CreateBuffer(src->mIndices.GetAddress(0), ni * sizeof(uint16_t), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Index);
                if (dst->mIBO == nullptr)
                {
                    log->Printf(LT_ERROR, L"Couldn't create mIBO");
                    Download(renderer, log);
                    return false;
                }

#if defined(WINDOWS)
                if (renderer->GetAPI() == piRenderer::API::DX)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        dst->mVertexArray[j] = renderer->CreateVertexArray2(0, nullptr, nullptr, nullptr, nullptr, shader_static_brush_vs_code[chunkType], shader_static_brush_vs_size[chunkType], dst->mIBO, piRenderer::IndexArrayFormat::UINT_16);
                        if (!dst->mVertexArray[j])
                        {
                            log->Printf(LT_ERROR, L"Couldn't create Vertex Array");
                            Download(renderer, log);
                            return false;
                        }
                    }
                }
                else
#endif
                {
                    dst->mVertexArray[0] = renderer->CreateVertexArray(0, nullptr, nullptr, nullptr, nullptr, dst->mIBO, piRenderer::IndexArrayFormat::UINT_16);
                    if (!dst->mVertexArray[0])
                    {
                        log->Printf(LT_ERROR, L"Couldn't create Vertex Array");
                        Download(renderer, log);
                        return false;
                    }
                }
            }
            mUploaded = true;
            return true;
        }

    };

    //===================================================================


    LayerRendererPaintStatic::LayerRendererPaintStatic() : LayerRendererPaint() {}
    LayerRendererPaintStatic::~LayerRendererPaintStatic() {}

bool LayerRendererPaintStatic::Init(piRenderer* renderer, piLog* log, Drawing::ColorSpace colorSpace, bool frontIsCCW)
    {
#if ST_VERTEX_FORMAT == 1
        piAssert( sizeof(DrawingStatic::MyVertexFormat)==28 );
#else
        piAssert( sizeof(DrawingStatic::MyVertexFormat)==36 );
#endif

        mCapLayersToRender = 1;
        mRetirementFrame = 0;
        mPresentationSample = 0;
        mTracePresentationFrames = std::getenv("IMM_LIVE_EDIT_TRACE_FRAMES") != nullptr;
        mRetiredDrawings.clear();

        mColorSpace = colorSpace;
        if (!mLayerInfo.Init(256, sizeof(iSLayerDrawInfoStatic))) // one per layer
            return false;

        if (!mVisibleLayerInfos.Init(256, sizeof(uint32_t)))
            return false;

        int dindex = 0;
        for (int i = 0; i < kNumShaders; i++)
        {
            mShader[i] = nullptr;
        }
        for (int l = 0; l < 5; l++) // brush
        for (int k = 0; k < 2; k++) // wiggle
        for (int j = 0; j < 2; j++) // drawin
        for (int i = 0; i < 3; i++) // stereo
        {
#if defined(ANDROID)
            if (j == 1) continue; // skip the drawin shaders on Android
#endif
            if (static_cast<StereoMode>(i) == StereoMode::Preferred)
            {
                if (renderer->GetAPI() == piRenderer::API::GL &&
                    (!renderer->SupportsFeature(piRenderer::RendererFeature::VIEWPORT_ARRAY) ||
                        !renderer->SupportsFeature(piRenderer::RendererFeature::VERTEX_VIEWPORT)))
                {
                    // skip compiling fast stereo shaders when we don't support the feature
                    dindex++;
                    continue;
                }
                if (renderer->GetAPI() == piRenderer::API::GLES &&
                    !renderer->SupportsFeature(piRenderer::RendererFeature::MULTIVIEW))
                {
                    // skip compiling multiview shaders when the extension isn't available
                    dindex++;
                    continue;
                }
            }

            const piShaderOptions ops = { 6,{ { "COLOR_COMPRESSED", static_cast<int>(colorSpace) },
                                              { "BRUSHTYPE", l },
                                              { "WIGGLE", k },
                                              { "DRAWIN", j },
                                              #if ST_VERTEX_FORMAT == 1
                                              { "VERTEX_FORMAT", 1 },
                                              #else
                                              { "VERTEX_FORMAT", 0 },
                                              #endif
                                              { "STEREOMODE", i } } };

            char error[1024] = { 0 };


            if (renderer->GetAPI() == piRenderer::API::Vulkan)
            {
#if defined(WINDOWS) || defined(ANDROID)
#if ST_VERTEX_FORMAT == 1
                const int vs_index = i +
                    k * 3 +
                    l * 3 * 2 +
                    (static_cast<int>(colorSpace)) * 5 * 3 * 2 +
                    2 * 5 * 3 * 2 * 2;
#else
                const int vs_index = i +
                    j * 3 +
                    k * 3 * 2 +
                    l * 3 * 2 * 2 +
                    (static_cast<int>(colorSpace)) * 5 * 3 * 2 * 2;
#endif
                const int fs_index = i;
                mShader[dindex] = renderer->CreateShaderBinary(&ops,
                    reinterpret_cast<const uint8_t *>(shader_static_brush_vs_spirv_code[vs_index]), shader_static_brush_vs_spirv_size[vs_index],
                    nullptr, 0, nullptr, 0, nullptr, 0,
                    reinterpret_cast<const uint8_t *>(shader_static_brush_fs_spirv_code[fs_index]), shader_static_brush_fs_spirv_size[fs_index],
                    error);
#else
                mShader[dindex] = renderer->CreateShader(&ops, shader_static_brush_vs, nullptr, nullptr, nullptr, shader_static_brush_fs, error);
#endif
            }
            else if (renderer->GetAPI() == piRenderer::API::GL || renderer->GetAPI() == piRenderer::API::GLES)
            {
                mShader[dindex] = renderer->CreateShader(&ops, shader_static_brush_vs, nullptr, nullptr, nullptr, shader_static_brush_fs, error);
            }
            else
            {
#if defined(WINDOWS)
                int vs_index = i +
                    j * 3 +
                    k * 3 * 2 +
                    l * 3 * 2 * 2 +
                    (static_cast<int>(colorSpace)) * 5 * 3 * 2 * 2;
                const int fs_index = i;

                mShader[dindex] = renderer->CreateShaderBinary(nullptr, shader_static_brush_vs_code[vs_index], shader_static_brush_vs_size[vs_index], nullptr, 0, nullptr, 0, nullptr, 0,
                    shader_static_brush_fs_code[fs_index], shader_static_brush_fs_size[fs_index], error);
#else
                if (renderer->GetAPI() == piRenderer::API::Metal)
                {
                    mShader[dindex] = renderer->CreateShader(&ops, nullptr, nullptr, nullptr, nullptr, nullptr, error);
                }
#endif
            }

            if (!mShader[dindex])
            {
                piString tmp; tmp.InitCopyS(error);
                log->Printf(LT_ERROR, L"Could not create shader (%d,%d,%d,%d): %s", i, j, k, l, tmp.GetS());
                tmp.End();
                return false;
            }
            dindex++;
        }

        const bool forcePaintWireframe = std::getenv("IMM_FORCE_PAINT_WIREFRAME") != nullptr;
        mRasterState[0] = renderer->CreateRasterState(forcePaintWireframe, frontIsCCW, piRenderer::CullMode::NONE, true, false);  // double sided, flip NO
        mRasterState[1] = renderer->CreateRasterState(forcePaintWireframe, frontIsCCW, piRenderer::CullMode::BACK, true, false);  // single sided, flip NO
        // Static paint's reflected geometry has the opposite submitted winding.
        // Keep this distinct from the pretessellated renderer: its generated
        // geometry uses a different winding convention for reflected layers.
        mRasterState[2] = renderer->CreateRasterState(forcePaintWireframe,!frontIsCCW, piRenderer::CullMode::NONE, true, false);  // double sided, flip YES
        mRasterState[3] = renderer->CreateRasterState(forcePaintWireframe,!frontIsCCW, piRenderer::CullMode::FRONT, true, false); // single sided, flip YES

        if (!mRasterState[0]) return false;
        if (!mRasterState[1]) return false;
        if (!mRasterState[2]) return false;
        if (!mRasterState[3]) return false;

        mChunkData = renderer->CreateBuffer(nullptr, 128*sizeof(ChunkData), piRenderer::BufferType::Dynamic, piRenderer::BufferUse::Constant);
        if (!mChunkData)
            return false;

        const piRenderer::TextureInfo infob = { piRenderer::TextureType::T2D_ARRAY, piRenderer::Format::C1_8_UNORM, 64, 64, 64, 1 };
        mBlueNoise = renderer->CreateTexture(0, &infob, false, piRenderer::TextureFilter::NONE, piRenderer::TextureWrap::REPEAT, 1.0f, (void*)GetBlueNoise_64x64x64());

        return true;
    }

    void LayerRendererPaintStatic::Deinit(piRenderer* renderer, piLog* log)
    {
        for (RetiredDrawing & retired : mRetiredDrawings)
        {
            iSLayerDrawInfoStatic * info = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(retired.mToken);
            if (info != nullptr)
            {
                info->Download(renderer, log);
                info->End();
                mLayerInfo.Free(retired.mToken);
            }
            if (retired.mDrawing != nullptr)
            {
                retired.mDrawing->Deinit();
                delete retired.mDrawing;
            }
        }
        mRetiredDrawings.clear();

        // Verify everything is freed for memory leaks
        const uint64_t num = mLayerInfo.GetMaxLength();
        bool notDeleted = false;
        bool stillUploaded = false;
        for (uint64_t j = 0; j < num; j++)
        {
            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(j);
            if (mLayerInfo.IsUsed(j))
            {
                notDeleted = true;
                if (me->mUploaded)            stillUploaded = true;
            }
        }
        piAssert(notDeleted == false);
        piAssert(stillUploaded == false);

        for (int i = 0; i < kNumShaders; i++)
        {
            renderer->DestroyShader(mShader[i]);
        }

        renderer->DestroyRasterState(mRasterState[3]);
        renderer->DestroyRasterState(mRasterState[2]);
        renderer->DestroyRasterState(mRasterState[0]);
        renderer->DestroyRasterState(mRasterState[1]);
        renderer->DestroyBuffer(mChunkData);
        renderer->DestroyTexture(mBlueNoise);

        mVisibleLayerInfos.End();
        mLayerInfo.End();
    }

    void LayerRendererPaintStatic::UnloadInCPU(piLog* log, Layer* la)
    {
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();

        const int numDrawings = lp->GetNumDrawings();

        for (int j = 0; j < numDrawings; j++)
        {
            const Drawing *dr = lp->GetDrawing(j);
            const int id = dr->GetGpuId();
            // continue, not return: a drawing with no gpu id (one an editor just added, or one
            // it emptied) must not strand every drawing after it in the layer.
            if (id == -1) continue;
            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);            
            me->End();
            mLayerInfo.Free(id);
        }
    }

    bool LayerRendererPaintStatic::IsLoadedInGPU(Layer * la)
    {
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();
        const int numDrawings = lp->GetNumDrawings();

        for (int j = 0; j < numDrawings; j++)
        {
            Drawing *dr = lp->GetDrawing(j);
            const int id = dr->GetGpuId();
            if (id == -1) return false;
            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);
            if (me->mUploaded) 
                return true;
        }
        return false;
    }   

    bool LayerRendererPaintStatic::UnloadInGPU(piRenderer* renderer, piSoundEngine* sound, piLog* log, Layer* la)
    {
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();

        const int numDrawings = lp->GetNumDrawings();

        for (int j = 0; j < numDrawings; j++)
        {
            Drawing *dr = lp->GetDrawing(j);
            const int id = dr->GetGpuId();
            if (id == -1) continue;
            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);
            me->Download(renderer, log);
        }

        return true;
    }

    bool LayerRendererPaintStatic::UnloadInGPU(ImmCore::piRenderer * renderer, ImmCore::piSoundEngine * sound, ImmCore::piLog * log, Layer * la, unsigned int drawingID)
    {
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();
        Drawing *dr = lp->GetDrawing(drawingID);
        const int id = dr->GetGpuId();
        if (id == -1) return false;
        iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);
        me->Download(renderer, log);
        return true;
    }

    bool LayerRendererPaintStatic::LoadInCPU( piLog* log, Layer* la)
    {
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();

        const int numDrawings = lp->GetNumDrawings();

        int s = sizeof(iSLayerDrawInfoStatic);
        for (int j = 0; j < numDrawings; j++)
        {
            bool isNew = false;
            uint64_t id;
            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.Alloc(&isNew, &id, true);
            if (!me)
            {
                log->Printf(LT_ERROR, L"Couldn't alloc new DrawInfo");
                return false;
            }
            new (me) iSLayerDrawInfoStatic();

            Drawing* dr = lp->GetDrawing(j);
            if (!me->Init( dr ))
            {
                log->Printf(LT_ERROR, L"Couldn't Init DrawInfo");
                return false;
            }
            dr->SetGpuId(static_cast<int>(id));
        }
        return true;
    }

    bool LayerRendererPaintStatic::LoadInGPU(piRenderer* renderer, piSoundEngine* sound, piLog* log, Layer* la)
    {
        return true;
        /*
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();

        const int numDrawings = lp->GetNumDrawings();

        for (int j = 0; j < numDrawings; j++)
        {
            const uint32_t id = lp->GetGpuId(j);
            iSLayerDrawInfo* me = (iSLayerDrawInfo*)mLayerInfo.GetAddress(id);

            if (!me->Upload(renderer, log))
            {
                log->Printf(LT_ERROR, L"Couldn't Init DrawInfo");
                return false;
            }
        }

        return true;
        */
    }


    void LayerRendererPaintStatic::GlobalWork(piRenderer* renderer, piSoundEngine* sound, piLog* log, Layer* la, float masterVolume)
    {

    }

    bool LayerRendererPaintStatic::PrepareDrawingReplacementInCPU(Drawing * replacement,
        uint64_t * tokenOut, piLog * log)
    {
        if (std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_CPU") != nullptr)
        {
            log->Printf(LT_ERROR, L"[IMM_LIVE_EDIT] injected CPU replacement failure");
            return false;
        }
        if (replacement == nullptr || tokenOut == nullptr || dynamic_cast<DrawingStatic *>(replacement) == nullptr)
            return false;

        bool isNew = false;
        uint64_t id = 0;
        iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.Alloc(&isNew, &id, true);
        if (me == nullptr)
        {
            log->Printf(LT_ERROR, L"Live edit: no draw-info slot for replacement drawing");
            return false;
        }

        new (me) iSLayerDrawInfoStatic();

        if (!me->Init(replacement))
        {
            mLayerInfo.Free(id);
            return false;
        }

        replacement->SetGpuId(static_cast<int>(id));
        *tokenOut = id;
        return true;
    }

    bool LayerRendererPaintStatic::PrepareDrawingReplacementInGPU(piRenderer * renderer,
        uint64_t token, piLog * log)
    {
        if (std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_GPU") != nullptr)
        {
            log->Printf(LT_ERROR, L"[IMM_LIVE_EDIT] injected GPU replacement failure token=%llu",
                static_cast<unsigned long long>(token));
            return false;
        }
        if (token >= mLayerInfo.GetMaxLength() || !mLayerInfo.IsUsed(token))
            return false;
        iSLayerDrawInfoStatic * info = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(token);
        return info != nullptr && info->Upload(renderer, log);
    }

    bool LayerRendererPaintStatic::PresentDrawingReplacement(Drawing * active,
        Drawing * replacement, uint64_t token, uint64_t revision, piLog * log)
    {
        if (std::getenv("IMM_LIVE_EDIT_FAIL_PRESENT") != nullptr)
        {
            log->Printf(LT_ERROR, L"[IMM_LIVE_EDIT] injected presentation failure token=%llu",
                static_cast<unsigned long long>(token));
            return false;
        }
        DrawingStatic * activeStatic = dynamic_cast<DrawingStatic *>(active);
        DrawingStatic * replacementStatic = dynamic_cast<DrawingStatic *>(replacement);
        if (activeStatic == nullptr || replacementStatic == nullptr ||
            token >= mLayerInfo.GetMaxLength() || !mLayerInfo.IsUsed(token))
            return false;

        const int oldToken = activeStatic->GetGpuId();
        if (oldToken < 0 || !mLayerInfo.IsUsed(static_cast<uint64_t>(oldToken)))
        {
            log->Printf(LT_ERROR, L"Live edit: active drawing has no renderer slot");
            return false;
        }

        iSLayerDrawInfoStatic * replacementInfo = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(token);
        iSLayerDrawInfoStatic * oldInfo = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(static_cast<uint64_t>(oldToken));
        if (replacementInfo == nullptr || oldInfo == nullptr || !replacementInfo->mUploaded)
            return false;

        if (!activeStatic->SwapGeometry(replacementStatic))
            return false;

        replacementInfo->mGeometry = activeStatic->GetGeometry();
        oldInfo->mGeometry = replacementStatic->GetGeometry();
        activeStatic->SetGpuId(static_cast<int>(token));
        activeStatic->SetAuthoringRevision(revision);
        replacementStatic->SetGpuId(oldToken);

        // Ownership transfers only after model geometry and renderer identity have both moved.
        // The old slot cannot be selected after this swap. Retire its CPU wrappers at the next
        // render boundary; each graphics backend owns the lifetime of already-submitted GPU
        // resources (Vulkan's deferred-destroy queue is fence/ring based, while Metal, D3D,
        // and OpenGL retain or defer resources referenced by submitted commands).
        mRetiredDrawings.push_back(RetiredDrawing{
            replacementStatic, static_cast<uint64_t>(oldToken),
            mRetirementFrame + 1 });
        log->Printf(LT_MESSAGE, L"[IMM_LIVE_EDIT] renderer swap activeToken=%llu retiredToken=%d",
            static_cast<unsigned long long>(token), oldToken);
        return true;
    }

    bool LayerRendererPaintStatic::PresentDrawingCreation(Drawing * created,
        Drawing * prepared, uint64_t token, uint64_t revision, piLog * log)
    {
        if (std::getenv("IMM_LIVE_EDIT_FAIL_PRESENT") != nullptr)
        {
            log->Printf(LT_ERROR, L"[IMM_LIVE_EDIT] injected creation presentation failure token=%llu",
                static_cast<unsigned long long>(token));
            return false;
        }

        DrawingStatic * createdStatic = dynamic_cast<DrawingStatic *>(created);
        DrawingStatic * preparedStatic = dynamic_cast<DrawingStatic *>(prepared);
        if (createdStatic == nullptr || preparedStatic == nullptr ||
            token >= mLayerInfo.GetMaxLength() || !mLayerInfo.IsUsed(token))
            return false;

        iSLayerDrawInfoStatic * info = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(token);
        if (info == nullptr || !info->mUploaded || !createdStatic->SwapGeometry(preparedStatic))
            return false;

        info->mGeometry = createdStatic->GetGeometry();
        createdStatic->SetGpuId(static_cast<int>(token));
        createdStatic->SetLoaded(preparedStatic->GetLoaded());
        createdStatic->SetAuthoringRevision(revision);
        preparedStatic->SetGpuId(-1);
        return true;
    }

    void LayerRendererPaintStatic::CancelDrawingReplacement(piRenderer * renderer,
        uint64_t token, piLog * log)
    {
        if (token >= mLayerInfo.GetMaxLength() || !mLayerInfo.IsUsed(token))
            return;
        iSLayerDrawInfoStatic * info = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(token);
        if (info != nullptr)
        {
            info->Download(renderer, log);
            info->End();
            mLayerInfo.Free(token);
        }
    }

    void LayerRendererPaintStatic::AdvanceDrawingRetirement(piRenderer * renderer, piLog * log)
    {
        mRetirementFrame++;
        for (size_t i = 0; i < mRetiredDrawings.size();)
        {
            RetiredDrawing & retired = mRetiredDrawings[i];
            if (retired.mRetireAfterFrame > mRetirementFrame)
            {
                i++;
                continue;
            }

            iSLayerDrawInfoStatic * info = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(retired.mToken);
            if (info != nullptr)
            {
                info->Download(renderer, log);
                info->End();
                mLayerInfo.Free(retired.mToken);
            }
            if (retired.mDrawing != nullptr)
            {
                retired.mDrawing->Deinit();
                delete retired.mDrawing;
            }
            mRetiredDrawings.erase(mRetiredDrawings.begin() + i);
        }
    }

    void LayerRendererPaintStatic::PrepareForDisplay(StereoMode stereoMode)
    {
        mStereoMode = stereoMode;
        mPresentationSample++;
        mVisibleLayerInfos.SetLength(0);
    }

    void LayerRendererPaintStatic::DisplayPreRender(piRenderer* renderer, piSoundEngine* sound, piLog* log, Layer* la, const frustum3& frus, const trans3d & layerToViewer, float laOpacity)
    {
        LayerPaint* lp = (LayerPaint*)la->GetImplementation();
        if (!la->GetLoaded()) return;

        const Drawing *dr = lp->GetCurrentDrawing();

        if (!dr->GetLoaded())
            return;

        const bound3 bbox = dr->GetBBox();//lp->GetBBox(drawing);

        mDrawCallInfo.numDrawCallsCulled = 0;
        mDrawCallInfo.numTrianglesCulled = 0;

        uint32_t id = dr->GetGpuId();
        if (id == -1)
        {
            LoadInCPU(log, la);
        }
        id = dr->GetGpuId();
        piAssert(id != -1);

        if (mTracePresentationFrames)
        {
            iSLayerDrawInfoStatic * selectedInfo =
                (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(id);
            const DrawingStatic * selectedDrawing = dynamic_cast<const DrawingStatic *>(dr);
            const bool geometryMatches = selectedInfo != nullptr && selectedDrawing != nullptr &&
                selectedInfo->mGeometry == selectedDrawing->GetGeometry();
            if (geometryMatches)
                log->Printf(LT_MESSAGE,
                    L"[IMM_LIVE_EDIT_FRAME] sample=%llu layer=%u revision=%llu rendererToken=%u geometryMatch=1",
                    static_cast<unsigned long long>(mPresentationSample), la->GetID(),
                    static_cast<unsigned long long>(dr->GetAuthoringRevision()), id);
            else
                log->Printf(LT_ERROR,
                    L"[IMM_LIVE_EDIT_FRAME] sample=%llu layer=%u revision=%llu rendererToken=%u geometryMatch=0",
                    static_cast<unsigned long long>(mPresentationSample), la->GetID(),
                    static_cast<unsigned long long>(dr->GetAuthoringRevision()), id);
        }

        // layer frustum culling
        if (boxInFrustum(frus, bbox) == 0)
        {
            
            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);

            for (uint32_t chunkType = 0; chunkType < kNumChunkTypes; chunkType++)
            {
                if (me->mGeometry->mBuffers[chunkType].mPoints.GetLength() == 0) continue;

                const uint64_t numChunks = me->mGeometry->mBuffers[chunkType].mChunks.GetLength();
                mDrawCallInfo.numDrawCallsCulled += static_cast<int>(numChunks);

                for (uint64_t i = 0; i < numChunks; i++)
                {
                    const DrawingStatic::Geometry::Chunk *srcChunk = me->mGeometry->mBuffers[chunkType].mChunks.GetAddress(i);
                    mDrawCallInfo.numTrianglesCulled += srcChunk->mNumIndices;
                }
            }

            //static int kk = 0; log->Printf(LT_MESSAGE, L"culled %s (%d)", la->GetName().GetS(), kk++);
            return;
        }


        // layer size culling (if too small in screen space)
        const vec3  lcen = getcenter(bbox);
        const vec3  lViewerPosition = d2f((invert(layerToViewer)*vec4d(0.0, 0.0, 0.0, 1.0)).xyz());
        const float wDistanceToBBox = float(double(sdBox(lViewerPosition - lcen, getradiius(bbox))) * layerToViewer.mScale); // distance to closest point on the surface of the bbox. It's negative if we are inside.
        if (wDistanceToBBox > 0.0) // if outside bbox
        {
            const vec3d vcen = (layerToViewer*f2d(vec4(lcen, 1.0f))).xyz();
            const float lrad2 = diagonalSquared(bbox);
            const double dis2 = lengthSquared(vcen);
            const double sizeInScreen = layerToViewer.mScale * sqrt(double(lrad2) / dis2);
            if (sizeInScreen < 0.005) // IQ-TODO: do a smooth fade here, super easy by using the layer opacity
            {
                return;
            }
        }
        //---------
        
        iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);

        // chunk frustum culling
        bool anyVisible = false;
        for (uint32_t chunkType = 0; chunkType < kNumChunkTypes; chunkType++)
        {
            me->mBuffers[chunkType].mChunks.SetLength(0);
            if (me->mGeometry->mBuffers[chunkType].mPoints.GetLength() == 0) continue;

            const uint64_t numChunks = me->mGeometry->mBuffers[chunkType].mChunks.GetLength();
            for (uint64_t i = 0; i < numChunks; i++)
            {
                const DrawingStatic::Geometry::Chunk *srcChunk = me->mGeometry->mBuffers[chunkType].mChunks.GetAddress(i);
                const bool visible = boxInFrustum(frus, srcChunk->mBBox) != 0;
                if (!visible)
                {
                    ++mDrawCallInfo.numDrawCallsCulled;
                    mDrawCallInfo.numTrianglesCulled += srcChunk->mNumIndices;
                    continue;
                }

                me->mBuffers[chunkType].mChunks.Append(srcChunk, true);
                anyVisible = true;
            }
        }

        if (!anyVisible) return;

        // at this point, some content in this layer is visible. Mark it as such
        mVisibleLayerInfos.AppendUInt32(id, true);


        // prepare GPU data
        {
            me->mLayerState.setLayerToViewerInfo(layerToViewer);
            me->mLayerState.mOpacity = laOpacity;
            //me->mLayerState.mFlipSign = (la->GetTransformToWorld().mFlip == flip3::N) ? 1.0f : -1.0f;
            me->mLayerState.mDrawInTime = float(la->GetDrawInTime());
            me->mLayerState.mAnimParam.x = float(la->GetAnimParam(0));
            me->mLayerState.mAnimParam.y = float(la->GetAnimParam(1));
            me->mLayerState.mAnimParam.z = float(la->GetAnimParam(2));
            me->mLayerState.mAnimParam.w = float(la->GetAnimParam(3));
            me->mLayerState.mID = la->GetID();

            const KeepAlive * ka = la->GetKeepAlive();

            if (ka->GetType() == KeepAlive::KeepAliveType::Wiggle)
            {
                const KeepAlive::Wiggle *fx = ka->GetDataWiggle();
                me->mLayerState.mKeepAlive.mWiggle.mAmplitude = fx->mAmplitude;
                me->mLayerState.mKeepAlive.mWiggle.mFrequency = fx->mFrequency;
                me->mLayerState.mKeepAlive.mWiggle.mSpeed = fx->mSpeed;
            }
            else if (ka->GetType() == KeepAlive::KeepAliveType::Blink)
            {
                const KeepAlive::Blink *fx = ka->GetDataBlink();
                me->mLayerState.mKeepAlive.mBlink.mWaveForm = fx->mWaveForm;
                me->mLayerState.mKeepAlive.mBlink.mSpeed = fx->mSpeed;
                me->mLayerState.mKeepAlive.mBlink.mMinOut = fx->mMinOut;
                me->mLayerState.mKeepAlive.mBlink.mMaxOut = fx->mMaxOut;
                me->mLayerState.mKeepAlive.mBlink.mMinIn = fx->mMinIn;
                me->mLayerState.mKeepAlive.mBlink.mMaxIn = fx->mMaxIn;
            }

            me->mDrawin = la->GetLayerUsesDrawin();
            me->mWiggle = ka->GetType() == KeepAlive::KeepAliveType::Wiggle;
            #ifdef RENDER_BUDGET
            //me->mDistance = (wDistanceToBBox<0.0) ? wDistanceToBBox : 1.0f/sizeInScreen;
            //me->mDistance = wDistanceToBBox;
            me->mDistance = float(1.0 / sizeInScreen);

            //me->mDistance = float(dis2);
            #endif
        }
    }

    void LayerRendererPaintStatic::DisplayRender(piRenderer* renderer, piLog* log, piBuffer layerStateShaderConstans, int capDelta)
    {
        const uint64_t num = mVisibleLayerInfos.GetLength();
        if (num < 1) return;


        const int forcedBrushType = iForcedPaintBrushType();
        const bool traceDraws = std::getenv("IMM_TRACE_STATIC_PAINT_DRAWS") != nullptr;
        int lastShaderID = -1;
        int lastStateID = -1;
        uint32_t traceDrawIndex = 0;

        const int stereoModeInt = static_cast<int>(mStereoMode);

        // Android uses the GL_OVR_multiview extension to render all layers of a 2D texture array.
        const int numInstances = (mStereoMode == StereoMode::Preferred && renderer->GetAPI() != piRenderer::API::GLES) ? 2 : 1;

        renderer->AttachShaderConstants(mChunkData, 9);


        #ifdef RENDER_BUDGET
        {
            const iSLayerDrawInfoStatic* data = (const iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(0);
            std::sort((uint32_t*)mVisibleLayerInfos.GetAddress(0), (uint32_t*)mVisibleLayerInfos.GetAddress(num - 1),
                [data](int ida, int idb) -> bool
                {
                    return data[ida].mDistance < data[idb].mDistance;
                });
        }

        mCapLayersToRender = static_cast<uint64_t>(clamp( int(mCapLayersToRender) + capDelta, 1, int(num)));
        //log->Printf(LT_MESSAGE, L"%d", mCapLayersToRender);

        const uint64_t numToRender = (num > mCapLayersToRender) ? mCapLayersToRender : num;

        #else
        const uint64_t numToRender = num;
        #endif

        mDrawCallInfo.numDrawCalls = 0;
        mDrawCallInfo.numTriangles = 0;

        // for each visible layer
        for (uint64_t j = 0; j < numToRender; j++)
        {
            const uint32_t id = mVisibleLayerInfos.GetUInt32(j);
            if (!iPaintLayerAllowed(id)) continue;

            iSLayerDrawInfoStatic* me = (iSLayerDrawInfoStatic*)mLayerInfo.GetAddress(id);

            // upload to GPU, if needed. This should be commaded externally as we stream scenes. But for now we do it here
            if (!me->mUploaded)
            {
                if (!me->Upload(renderer, log))
                {
                    log->Printf(LT_ERROR, L"Couldn't upload data to the GPU");
                    return;
                }
            }

            renderer->UpdateBuffer(layerStateShaderConstans, &me->mLayerState, 0, sizeof(LayersState));

            int tmpShaderId = 0;
            tmpShaderId += stereoModeInt;
#if !defined(ANDROID)
            tmpShaderId += (me->mDrawin == true) ? 3 : 0;
#endif
#if defined(ANDROID)
            tmpShaderId += (me->mWiggle == true) ? 3 : 0;
#else
            tmpShaderId += (me->mWiggle == true) ? 6 : 0;
#endif

            renderer->AttachTextures(1, &mBlueNoise, 7);
            
            // for each chunk in that layer
            for (int chunkType = 0; chunkType < kNumChunkTypes; chunkType++)
            {
                if (forcedBrushType >= 0 && chunkType != forcedBrushType) continue;
                const iSLayerDrawInfoStatic::BufferData *info = me->mBuffers + chunkType;
                const uint64_t numChunks = info->mChunks.GetLength();
                if (numChunks == 0) continue;

                // set shader and state
                const int stateID = ((chunkType == static_cast<int>(Element::BrushSectionType::Segment)) ? 0 : 1);
                if (stateID != lastStateID) { lastStateID = stateID; renderer->SetRasterState(mRasterState[stateID]); }

#if !defined(ANDROID)
                int shaderID = tmpShaderId + 2 * 2 * 3 * chunkType;
#else
                int shaderID = tmpShaderId + 3 * 2 * chunkType;
#endif
                if (shaderID != lastShaderID)
                {
                    lastShaderID = shaderID;
                    if (shaderID < 0 || shaderID >= kNumShaders || mShader[shaderID] == nullptr)
                    {
                        log->Printf(LT_ERROR, L"Missing shader id %d (chunk=%d stereo=%d wiggle=%d)", shaderID, chunkType, stereoModeInt, me->mWiggle ? 1 : 0);
                        continue;
                    }
                    renderer->AttachShader(mShader[shaderID]);
                }


                // attach vertex and index data
                renderer->AttachShaderBuffer(me->mBuffers[chunkType].mVertexData, 8);
                if (renderer->GetAPI() == piRenderer::API::GL || renderer->GetAPI() == piRenderer::API::GLES || renderer->GetAPI() == piRenderer::API::Vulkan)
                    renderer->AttachVertexArray(me->mBuffers[chunkType].mVertexArray[0]);
                else
                    renderer->AttachVertexArray2(me->mBuffers[chunkType].mVertexArray[stereoModeInt]);

                mDrawCallInfo.numDrawCalls += static_cast<int>(numChunks);

                // TODO: all this loop below could be implemented with a single render call! (piRenderer::DrawPrimitiveIndexedMultiple)
                for (uint64_t i = 0; i < numChunks; i++)
                {
                    const DrawingStatic::Geometry::Chunk *chunk = info->mChunks.Get(i);
                    const ChunkData cd = { chunk->mVertexOffset, chunk->mBiggestStroke };

                    renderer->UpdateBuffer(mChunkData, &cd, 0, 1 * sizeof(ChunkData)); // TODO:  put this in an buffer so we only do one upload per chunkType, not per chunk

                    if (traceDraws)
                    {
                        log->Printf(LT_MESSAGE,
                                    L"IMM_VKPARITY_DRAW api=%d draw=%u layerId=%u chunkType=%d chunk=%llu shader=%d state=%d drawin=%d wiggle=%d opacity=%.6f drawInTime=%.6f layerStateId=%u vertexOffset=%u indexOffset=%u numIndices=%u biggestStroke=%.6f",
                                    static_cast<int>(renderer->GetAPI()),
                                    traceDrawIndex++,
                                    id,
                                    chunkType,
                                    static_cast<unsigned long long>(i),
                                    shaderID,
                                    stateID,
                                    me->mDrawin ? 1 : 0,
                                    me->mWiggle ? 1 : 0,
                                    me->mLayerState.mOpacity,
                                    me->mLayerState.mDrawInTime,
                                    me->mLayerState.mID,
                                    chunk->mVertexOffset,
                                    chunk->mIndexOffset,
                                    chunk->mNumIndices,
                                    chunk->mBiggestStroke);
                    }
                    renderer->DrawPrimitiveIndexed(piRenderer::PrimitiveType::TriangleStrip, chunk->mNumIndices, numInstances, 0 /*chunk->mVertexOffset*/, 0, chunk->mIndexOffset);
                    mDrawCallInfo.numIndices += chunk->mNumIndices;
                    mDrawCallInfo.numTriangles += chunk->mNumPolygons * 2;
                }
            }
        }

        //log->Printf(LT_DEBUG, L"Total number of indices in viewport: %i", totalNumIndices);
        renderer->DettachTextures();
        renderer->DettachShader();
    }

}
