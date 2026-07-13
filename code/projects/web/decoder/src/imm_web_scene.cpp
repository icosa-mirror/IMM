#include "imm_web_decoder.h"

#include "appImmStrokeReader/src/strokeStore.h"
#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace
{
    std::unique_ptr<ImmStrokeReader::StrokeStore> gStore;
    float gBackgroundColor[3] = {0.0f, 0.0f, 0.0f};

    void setError(ImmWebError* error, ImmWebStatus status, const char* message)
    {
        if (error == nullptr)
        {
            return;
        }
        std::memset(error, 0, sizeof(*error));
        error->status = static_cast<uint32_t>(status);
        const size_t copyLength = std::min(std::strlen(message), static_cast<size_t>(IMM_WEB_ERROR_MESSAGE_CAPACITY - 1u));
        std::memcpy(error->message, message, copyLength);
    }

    ImmWebTransform convertTransform(const ImmStrokeReader::StrokeLayerTransformC& source)
    {
        ImmWebTransform result{};
        std::copy(source.rotation, source.rotation + 4, result.rotation);
        result.scale = source.scale;
        result.flip = static_cast<uint32_t>(source.flip);
        std::copy(source.translation, source.translation + 3, result.translation);
        return result;
    }
}

extern "C" ImmWebStatus imm_web_decode_scene(
    const uint8_t* source,
    size_t sourceSize,
    ImmWebError* outError)
{
    imm_web_release_scene();
    if (source == nullptr || sourceSize == 0u)
    {
        setError(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, "Scene source is required");
        return IMM_WEB_STATUS_INVALID_ARGUMENT;
    }

    ImmCore::piTArray<uint8_t> data;
    if (!data.Init(0u, false))
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Could not initialize scene input");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }
    data.Set(const_cast<uint8_t*>(source), static_cast<uint64_t>(sourceSize));

    ImmCore::piLog log;
    ImmImporter::Sequence sequence;
    auto store = std::make_unique<ImmStrokeReader::StrokeStore>();
    const bool decoded = ImmImporter::ImportFromMemory(
        &data,
        &sequence,
        &log,
        ImmImporter::Drawing::ColorSpace::Gamma,
        ImmImporter::Drawing::PaintRenderingTechnique::Static,
        store.get());
    if (!decoded)
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Native IMM scene importer failed");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }

    const ImmCore::vec3 background = sequence.GetBackgroundColor();
    gBackgroundColor[0] = background.x;
    gBackgroundColor[1] = background.y;
    gBackgroundColor[2] = background.z;
    sequence.Deinit(&log);
    gStore = std::move(store);
    setError(outError, IMM_WEB_STATUS_OK, "");
    return IMM_WEB_STATUS_OK;
}

extern "C" void imm_web_release_scene(void)
{
    gStore.reset();
    gBackgroundColor[0] = 0.0f;
    gBackgroundColor[1] = 0.0f;
    gBackgroundColor[2] = 0.0f;
}

extern "C" uint32_t imm_web_get_layer_count(void)
{
    return gStore == nullptr ? 0u : static_cast<uint32_t>(gStore->GetLayerCount());
}

extern "C" uint32_t imm_web_get_background_color(float* outRgb, uint32_t floatCapacity)
{
    if (outRgb == nullptr || floatCapacity < 3u)
    {
        return 0u;
    }
    std::copy(gBackgroundColor, gBackgroundColor + 3, outRgb);
    return 3u;
}

extern "C" uint32_t imm_web_get_layer_info(uint32_t layerIndex, ImmWebLayerInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerInfoC source{};
    if (!gStore->GetLayerInfo(static_cast<int>(layerIndex), &source))
    {
        return 0u;
    }
    std::memset(outInfo, 0, sizeof(*outInfo));
    outInfo->id = static_cast<uint32_t>(source.id);
    outInfo->type = static_cast<uint32_t>(source.type);
    outInfo->drawing_count = static_cast<uint32_t>(source.numDrawings);
    outInfo->visible = static_cast<uint32_t>(source.visible);
    outInfo->opacity = source.opacity;
    std::memcpy(outInfo->name, source.name, sizeof(outInfo->name));
    return 1u;
}

extern "C" uint32_t imm_web_get_layer_transforms(
    uint32_t layerIndex,
    ImmWebTransform* outLocal,
    ImmWebTransform* outWorld,
    ImmWebTransform* outPivot)
{
    if (gStore == nullptr || outLocal == nullptr || outWorld == nullptr || outPivot == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerTransformC local{};
    ImmStrokeReader::StrokeLayerTransformC world{};
    if (!gStore->GetLayerTransform(static_cast<int>(layerIndex), &local, &world))
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerInfoC layer{};
    if (!gStore->GetLayerInfo(static_cast<int>(layerIndex), &layer))
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerTransformC pivot{};
    std::copy(layer.pivotRotation, layer.pivotRotation + 4, pivot.rotation);
    pivot.scale = layer.pivotScale;
    pivot.flip = layer.pivotFlip;
    std::copy(layer.pivotTranslation, layer.pivotTranslation + 3, pivot.translation);
    *outLocal = convertTransform(local);
    *outWorld = convertTransform(world);
    *outPivot = convertTransform(pivot);
    return 1u;
}

extern "C" uint32_t imm_web_get_animation_info(uint32_t layerIndex, ImmWebAnimationInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    uint32_t frameRate = 0u;
    uint32_t frameCount = 0u;
    uint32_t repeatCount = 0u;
    if (!gStore->GetLayerAnimationInfo(static_cast<int>(layerIndex), &frameRate, &frameCount, &repeatCount))
    {
        return 0u;
    }
    *outInfo = {frameRate, frameCount, repeatCount, 0u};
    return 1u;
}

extern "C" uint32_t imm_web_get_frame_buffer(uint32_t layerIndex, uint32_t* outFrames, uint32_t frameCapacity)
{
    if (gStore == nullptr || outFrames == nullptr || frameCapacity == 0u)
    {
        return 0u;
    }
    ImmWebAnimationInfo info{};
    if (imm_web_get_animation_info(layerIndex, &info) == 0u ||
        !gStore->GetFrameBuffer(static_cast<int>(layerIndex), outFrames, static_cast<int>(frameCapacity)))
    {
        return 0u;
    }
    return std::min(frameCapacity, info.frame_count);
}

extern "C" uint32_t imm_web_get_drawing_count(uint32_t layerIndex)
{
    return gStore == nullptr ? 0u : static_cast<uint32_t>(gStore->GetDrawingCount(static_cast<int>(layerIndex)));
}

extern "C" float imm_web_get_drawing_biggest_stroke(uint32_t layerIndex, uint32_t drawingIndex)
{
    float result = 0.0f;
    if (gStore != nullptr)
    {
        gStore->GetDrawingBiggestStroke(static_cast<int>(layerIndex), static_cast<int>(drawingIndex), &result);
    }
    return result;
}

extern "C" uint32_t imm_web_get_stroke_count(uint32_t layerIndex, uint32_t drawingIndex)
{
    return gStore == nullptr ? 0u : static_cast<uint32_t>(gStore->GetStrokeCount(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex)));
}

extern "C" uint32_t imm_web_get_stroke_info(
    uint32_t layerIndex,
    uint32_t drawingIndex,
    uint32_t strokeIndex,
    ImmWebStrokeInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokeInfoC source{};
    if (!gStore->GetStrokeInfo(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex), static_cast<int>(strokeIndex), &source))
    {
        return 0u;
    }
    *outInfo = {};
    outInfo->brush_type = static_cast<uint32_t>(source.brushType);
    outInfo->visibility_mode = static_cast<uint32_t>(source.visibilityMode);
    outInfo->point_count = static_cast<uint32_t>(source.numPoints);
    outInfo->bounds_min[0] = source.bboxMinX;
    outInfo->bounds_min[1] = source.bboxMinY;
    outInfo->bounds_min[2] = source.bboxMinZ;
    outInfo->bounds_max[0] = source.bboxMaxX;
    outInfo->bounds_max[1] = source.bboxMaxY;
    outInfo->bounds_max[2] = source.bboxMaxZ;
    return 1u;
}

extern "C" uint32_t imm_web_get_stroke_points(
    uint32_t layerIndex,
    uint32_t drawingIndex,
    uint32_t strokeIndex,
    ImmWebStrokePoint* outPoints,
    uint32_t pointCapacity)
{
    if (gStore == nullptr || outPoints == nullptr || pointCapacity == 0u)
    {
        return 0u;
    }
    ImmWebStrokeInfo info{};
    if (imm_web_get_stroke_info(layerIndex, drawingIndex, strokeIndex, &info) == 0u)
    {
        return 0u;
    }
    const uint32_t count = std::min(pointCapacity, info.point_count);
    auto points = std::make_unique<ImmStrokeReader::StrokePointC[]>(count);
    if (!gStore->GetStrokePoints(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex), static_cast<int>(strokeIndex),
        points.get(), static_cast<int>(count)))
    {
        return 0u;
    }
    for (uint32_t index = 0u; index < count; ++index)
    {
        const ImmStrokeReader::StrokePointC& source = points[index];
        ImmWebStrokePoint& target = outPoints[index];
        target.position[0] = source.px;
        target.position[1] = source.py;
        target.position[2] = source.pz;
        target.normal[0] = source.nx;
        target.normal[1] = source.ny;
        target.normal[2] = source.nz;
        target.direction[0] = source.dx;
        target.direction[1] = source.dy;
        target.direction[2] = source.dz;
        target.color[0] = source.r;
        target.color[1] = source.g;
        target.color[2] = source.b;
        target.alpha = source.alpha;
        target.width = source.width;
    }
    return count;
}

extern "C" uint32_t imm_web_get_picture_info(uint32_t layerIndex, ImmWebPictureInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokePictureInfoC source{};
    if (!gStore->GetPictureInfo(static_cast<int>(layerIndex), &source))
    {
        return 0u;
    }
    *outInfo = {
        static_cast<uint32_t>(source.layerId),
        static_cast<uint32_t>(source.contentType),
        static_cast<uint32_t>(source.isViewerLocked),
        static_cast<uint32_t>(source.width),
        static_cast<uint32_t>(source.height),
        static_cast<uint32_t>(source.hasAlpha),
        static_cast<uint32_t>(source.dataSize),
    };
    return 1u;
}

extern "C" uint32_t imm_web_get_picture_pixels(uint32_t layerIndex, uint8_t* outPixels, uint32_t byteCapacity)
{
    if (gStore == nullptr || outPixels == nullptr || byteCapacity == 0u)
    {
        return 0u;
    }
    ImmStrokeReader::StrokePictureInfoC info{};
    if (!gStore->GetPictureInfo(static_cast<int>(layerIndex), &info) ||
        !gStore->GetPicturePixels(static_cast<int>(layerIndex), outPixels, static_cast<int>(byteCapacity)))
    {
        return 0u;
    }
    return std::min(byteCapacity, static_cast<uint32_t>(info.dataSize));
}
