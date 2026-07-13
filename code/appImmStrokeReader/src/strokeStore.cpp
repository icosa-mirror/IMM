#include "strokeStore.h"

#include <algorithm>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "libImmCore/src/libBasics/piStr.h"

namespace ImmStrokeReader
{

namespace
{
    StrokeLayerTransformC ToTransformC(const ImmCore::trans3d& transform)
    {
        StrokeLayerTransformC out{};
        out.rotation[0] = static_cast<float>(transform.mRotation.x);
        out.rotation[1] = static_cast<float>(transform.mRotation.y);
        out.rotation[2] = static_cast<float>(transform.mRotation.z);
        out.rotation[3] = static_cast<float>(transform.mRotation.w);
        if (out.rotation[0] == 0.0f && out.rotation[1] == 0.0f &&
            out.rotation[2] == 0.0f && out.rotation[3] == 0.0f)
        {
            out.rotation[3] = 1.0f;
        }
        out.scale = static_cast<float>(transform.mScale);
        out.flip = static_cast<int>(transform.mFlip);
        out.translation[0] = static_cast<float>(transform.mTranslation.x);
        out.translation[1] = static_cast<float>(transform.mTranslation.y);
        out.translation[2] = static_cast<float>(transform.mTranslation.z);
        return out;
    }

    std::string ToUtf8(const std::wstring& input)
    {
        if (input.empty())
        {
            return std::string();
        }

        char* res = ImmCore::piws2str(input.c_str());
        if (!res)
        {
            return std::string();
        }

        std::string out(res);
        free(res);
        return out;
    }
}

StrokeStore::StrokeStore()
    : mCurrentLayer(nullptr)
    , mCurrentDrawing(nullptr)
{
}

StrokeStore::~StrokeStore()
{
    Clear();
}

void StrokeStore::OnBeginLayer(uint32_t layerId, uint32_t layerType, const wchar_t* name, bool visible, float opacity)
{
    StoredLayer layer;
    layer.layerId = layerId;
    layer.layerType = layerType;
    layer.name = name ? name : L"";
    layer.visible = visible;
    layer.opacity = opacity;
    layer.pivotTransform = ImmCore::trans3d::identity();
    layer.localTransform = ImmCore::trans3d::identity();
    layer.worldTransform = ImmCore::trans3d::identity();
    mDocument.layers.push_back(layer);
    mCurrentLayer = &mDocument.layers.back();
}

void StrokeStore::OnLayerTransform(uint32_t layerId, const ImmCore::trans3d& localTransform, const ImmCore::trans3d& worldTransform, const ImmCore::trans3d& pivotTransform)
{
    if (mCurrentLayer && mCurrentLayer->layerId == layerId)
    {
        mCurrentLayer->localTransform = localTransform;
        mCurrentLayer->worldTransform = worldTransform;
        mCurrentLayer->pivotTransform = pivotTransform;
        return;
    }

    for (auto& layer : mDocument.layers)
    {
        if (layer.layerId == layerId)
        {
            layer.localTransform = localTransform;
            layer.worldTransform = worldTransform;
            layer.pivotTransform = pivotTransform;
            return;
        }
    }
}

void StrokeStore::OnPictureLayer(
    uint32_t layerId,
    uint32_t contentType,
    bool isViewerLocked,
    int width,
    int height,
    bool hasAlpha,
    const uint8_t* pixels,
    int pixelDataSize)
{
    if (layerId == 0 || width <= 0 || height <= 0 || !pixels || pixelDataSize <= 0)
    {
        return;
    }

    for (auto& layer : mDocument.layers)
    {
        if (layer.layerId != layerId)
        {
            continue;
        }

        layer.hasPicture = true;
        layer.pictureContentType = contentType;
        layer.pictureViewerLocked = isViewerLocked;
        layer.pictureWidth = width;
        layer.pictureHeight = height;
        layer.pictureHasAlpha = hasAlpha;
        layer.picturePixels.assign(pixels, pixels + pixelDataSize);
        return;
    }
}

void StrokeStore::OnSpawnArea(uint32_t layerId, bool isDefault)
{
    bool hasDefault = false;
    for (const auto& layer : mDocument.layers)
    {
        hasDefault = hasDefault || layer.isDefaultSpawn;
    }
    if (isDefault)
    {
        for (auto& layer : mDocument.layers)
        {
            layer.isDefaultSpawn = false;
        }
    }
    for (auto& layer : mDocument.layers)
    {
        if (layer.layerId == layerId)
        {
            layer.isDefaultSpawn = isDefault || !hasDefault;
            return;
        }
    }
}

void StrokeStore::OnPaintLayerInfo(uint32_t frameRate, uint32_t numFrames, uint32_t maxRepeatCount)
{
    if (!mCurrentLayer) return;

    mCurrentLayer->frameRate = frameRate;
    mCurrentLayer->numFrames = numFrames;
    mCurrentLayer->maxRepeatCount = maxRepeatCount;
}

void StrokeStore::OnFrameBuffer(const uint32_t* frameBuffer, uint32_t numFrames)
{
    if (!mCurrentLayer) return;
    if (frameBuffer == nullptr || numFrames == 0) return;

    mCurrentLayer->frameBuffer.resize(numFrames);
    for (uint32_t i = 0; i < numFrames; i++)
    {
        mCurrentLayer->frameBuffer[i] = frameBuffer[i];
    }
}

void StrokeStore::OnBeginDrawing(uint32_t drawingId)
{
    if (!mCurrentLayer) return;

    StoredDrawing drawing;
    drawing.drawingId = drawingId;
    mCurrentLayer->drawings.push_back(drawing);
    mCurrentDrawing = &mCurrentLayer->drawings.back();
}

void StrokeStore::OnDrawingBiggestStroke(uint32_t drawingId, float biggestStroke)
{
    if (!mCurrentLayer)
        return;

    // Expected case: callback corresponds to the current drawing.
    if (mCurrentDrawing && mCurrentDrawing->drawingId == drawingId)
    {
        mCurrentDrawing->biggestStroke = biggestStroke;
        return;
    }

    // Fallback: locate by id.
    for (auto& drawing : mCurrentLayer->drawings)
    {
        if (drawing.drawingId == drawingId)
        {
            drawing.biggestStroke = biggestStroke;
            return;
        }
    }
}

void StrokeStore::OnStroke(
    uint32_t strokeId,
    uint8_t brushType,
    uint8_t visibilityMode,
    uint32_t numPoints,
    const ImmImporter::Point* points,
    const ImmCore::bound3& bbox)
{
    if (!mCurrentDrawing) return;

    StoredStroke stroke;
    stroke.strokeId = strokeId;
    stroke.brushType = brushType;
    stroke.visibilityMode = visibilityMode;
    stroke.bbox = bbox;

    // Copy all points
    stroke.points.resize(numPoints);
    for (uint32_t i = 0; i < numPoints; i++)
    {
        stroke.points[i] = points[i];
    }

    mCurrentDrawing->strokes.push_back(std::move(stroke));
}

void StrokeStore::OnEndDrawing()
{
    mCurrentDrawing = nullptr;
}

void StrokeStore::OnEndLayer()
{
    mCurrentLayer = nullptr;
}

int StrokeStore::GetLayerCount() const
{
    return static_cast<int>(mDocument.layers.size());
}

bool StrokeStore::GetLayerInfo(int layerIdx, StrokeLayerInfoC* info) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    if (!info)
        return false;

    const StoredLayer& layer = mDocument.layers[layerIdx];
    info->id = static_cast<int>(layer.layerId);
    info->type = static_cast<int>(layer.layerType);
    info->numDrawings = static_cast<int>(layer.drawings.size());
    info->visible = layer.visible ? 1 : 0;
    info->opacity = static_cast<float>(layer.opacity);
    info->isDefaultSpawn = layer.isDefaultSpawn ? 1 : 0;

    StrokeLayerTransformC pivot = ToTransformC(layer.pivotTransform);
    info->pivotRotation[0] = pivot.rotation[0];
    info->pivotRotation[1] = pivot.rotation[1];
    info->pivotRotation[2] = pivot.rotation[2];
    info->pivotRotation[3] = pivot.rotation[3];
    info->pivotScale = pivot.scale;
    info->pivotFlip = pivot.flip;
    info->pivotTranslation[0] = pivot.translation[0];
    info->pivotTranslation[1] = pivot.translation[1];
    info->pivotTranslation[2] = pivot.translation[2];

    const std::string utf8Name = ToUtf8(layer.name);
    std::memset(info->name, 0, sizeof(info->name));
    const size_t copyLen = std::min(utf8Name.size(), sizeof(info->name) - 1);
    std::memcpy(info->name, utf8Name.data(), copyLen);

    return true;
}

bool StrokeStore::GetLayerTransform(int layerIdx, StrokeLayerTransformC* local, StrokeLayerTransformC* world) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    if (!local || !world)
        return false;

    const StoredLayer& layer = mDocument.layers[layerIdx];
    *local = ToTransformC(layer.localTransform);
    *world = ToTransformC(layer.worldTransform);
    return true;
}

int StrokeStore::GetDrawingCount(int layerIdx) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return 0;
    return static_cast<int>(mDocument.layers[layerIdx].drawings.size());
}

bool StrokeStore::GetDrawingBiggestStroke(int layerIdx, int drawingIdx, float* biggestStroke) const
{
    if (!biggestStroke)
        return false;
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    const auto& drawings = mDocument.layers[layerIdx].drawings;
    if (drawingIdx < 0 || drawingIdx >= static_cast<int>(drawings.size()))
        return false;

    *biggestStroke = drawings[drawingIdx].biggestStroke;
    return true;
}

int StrokeStore::GetStrokeCount(int layerIdx, int drawingIdx) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return 0;
    const auto& drawings = mDocument.layers[layerIdx].drawings;
    if (drawingIdx < 0 || drawingIdx >= static_cast<int>(drawings.size()))
        return 0;
    return static_cast<int>(drawings[drawingIdx].strokes.size());
}

bool StrokeStore::GetStrokeInfo(int layerIdx, int drawingIdx, int strokeIdx, StrokeInfoC* info) const
{
    if (!info)
        return false;
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    const auto& drawings = mDocument.layers[layerIdx].drawings;
    if (drawingIdx < 0 || drawingIdx >= static_cast<int>(drawings.size()))
        return false;
    const auto& strokes = drawings[drawingIdx].strokes;
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(strokes.size()))
        return false;

    const StoredStroke& stroke = strokes[strokeIdx];
    info->brushType = stroke.brushType;
    info->visibilityMode = stroke.visibilityMode;
    info->numPoints = static_cast<int>(stroke.points.size());
    info->bboxMinX = stroke.bbox.mMinX;
    info->bboxMinY = stroke.bbox.mMinY;
    info->bboxMinZ = stroke.bbox.mMinZ;
    info->bboxMaxX = stroke.bbox.mMaxX;
    info->bboxMaxY = stroke.bbox.mMaxY;
    info->bboxMaxZ = stroke.bbox.mMaxZ;

    return true;
}

bool StrokeStore::GetStrokePoints(int layerIdx, int drawingIdx, int strokeIdx, StrokePointC* points, int maxPoints) const
{
    if (!points || maxPoints <= 0)
        return false;
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    const auto& drawings = mDocument.layers[layerIdx].drawings;
    if (drawingIdx < 0 || drawingIdx >= static_cast<int>(drawings.size()))
        return false;
    const auto& strokes = drawings[drawingIdx].strokes;
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(strokes.size()))
        return false;

    const StoredStroke& stroke = strokes[strokeIdx];
    const float biggestStroke = drawings[drawingIdx].biggestStroke;
    const float widthScale = (biggestStroke > 0.0f) ? ((1.7f * biggestStroke) / 32767.0f) : 0.0f;
    int copyCount = std::min(static_cast<int>(stroke.points.size()), maxPoints);

    for (int i = 0; i < copyCount; i++)
    {
        const ImmImporter::Point& src = stroke.points[i];
        StrokePointC& dst = points[i];

        dst.px = src.mPos.x;
        dst.py = src.mPos.y;
        dst.pz = src.mPos.z;
        dst.nx = src.mNor.x;
        dst.ny = src.mNor.y;
        dst.nz = src.mNor.z;
        dst.dx = src.mDir.x;
        dst.dy = src.mDir.y;
        dst.dz = src.mDir.z;
        dst.r = src.mCol.x;
        dst.g = src.mCol.y;
        dst.b = src.mCol.z;
        // mTra is stored as 0-255, convert to 0-1 range
        dst.alpha = static_cast<float>(src.mTra) / 255.0f;
        // Reconstruct width from the quantized 15-bit value.
        // See Element::Point::mWid comment for the canonical formula.
        int widQ = src.mWid;
        if (widQ < 0) widQ = 0;
        if (widQ > 32767) widQ = 32767;
        dst.width = widthScale * static_cast<float>(widQ);
    }

    return true;
}

bool StrokeStore::GetStrokePointTimes(int layerIdx, int drawingIdx, int strokeIdx, float* times, int maxPoints) const
{
    if (!times || maxPoints <= 0 || layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    const auto& drawings = mDocument.layers[layerIdx].drawings;
    if (drawingIdx < 0 || drawingIdx >= static_cast<int>(drawings.size()))
        return false;
    const auto& strokes = drawings[drawingIdx].strokes;
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(strokes.size()))
        return false;
    const StoredStroke& stroke = strokes[strokeIdx];
    const int copyCount = std::min(static_cast<int>(stroke.points.size()), maxPoints);
    for (int index = 0; index < copyCount; ++index)
        times[index] = stroke.points[index].mTim;
    return true;
}

bool StrokeStore::GetPictureInfo(int layerIdx, StrokePictureInfoC* info) const
{
    if (!info)
    {
        return false;
    }

    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
    {
        return false;
    }

    const StoredLayer& layer = mDocument.layers[layerIdx];
    if (!layer.hasPicture)
    {
        return false;
    }

    info->layerId = static_cast<int>(layer.layerId);
    info->contentType = static_cast<int>(layer.pictureContentType);
    info->isViewerLocked = layer.pictureViewerLocked ? 1 : 0;
    info->width = layer.pictureWidth;
    info->height = layer.pictureHeight;
    info->hasAlpha = layer.pictureHasAlpha ? 1 : 0;
    info->dataSize = static_cast<int>(layer.picturePixels.size());
    return true;
}

bool StrokeStore::GetPicturePixels(int layerIdx, uint8_t* pixels, int maxBytes) const
{
    if (!pixels || maxBytes <= 0)
    {
        return false;
    }

    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
    {
        return false;
    }

    const StoredLayer& layer = mDocument.layers[layerIdx];
    if (!layer.hasPicture || layer.picturePixels.empty())
    {
        return false;
    }

    const int copyBytes = std::min(maxBytes, static_cast<int>(layer.picturePixels.size()));
    memcpy(pixels, layer.picturePixels.data(), static_cast<size_t>(copyBytes));
    return true;
}

int StrokeStore::GetChapterCount() const
{
    if (mDocument.chapterStartTimes.empty())
    {
        return 1;
    }

    return static_cast<int>(mDocument.chapterStartTimes.size());
}

int StrokeStore::GetCurrentChapter() const
{
    return mDocument.currentChapter;
}

bool StrokeStore::SetCurrentChapter(int chapterIndex)
{
    if (chapterIndex < 0 || chapterIndex >= GetChapterCount())
    {
        return false;
    }

    mDocument.currentChapter = chapterIndex;
    return true;
}

void StrokeStore::SetChapterStartTimes(const std::vector<ImmCore::piTick>& chapterStartTimes)
{
    mDocument.chapterStartTimes = chapterStartTimes;
    if (mDocument.chapterStartTimes.empty())
    {
        mDocument.chapterStartTimes.push_back(ImmCore::piTick(0));
    }

    if (mDocument.currentChapter < 0 || mDocument.currentChapter >= static_cast<int>(mDocument.chapterStartTimes.size()))
    {
        mDocument.currentChapter = 0;
    }
}

int StrokeStore::GetDrawingIndexForChapter(int layerIdx, int chapterIndex) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size())) return 0;
    if (chapterIndex < 0 || chapterIndex >= GetChapterCount()) return 0;

    const StoredLayer& layer = mDocument.layers[layerIdx];

    if (layer.frameRate == 0 || layer.frameBuffer.empty()) return 0;

    // A chapter is a pause point: the artwork is frozen at the END of the chapter
    // (when the animation stops and waits for user input), not at the start.
    // The end of chapter N is the start of chapter N+1. For the last chapter,
    // use the final frame of the animation.
    int64_t targetFrame;
    int chapterCount = GetChapterCount();

    if (chapterIndex + 1 < chapterCount)
    {
        ImmCore::piTick nextChapterTick = mDocument.chapterStartTimes[chapterIndex + 1];
        targetFrame = ImmCore::piTick::ToFramesFloor(nextChapterTick, static_cast<int>(layer.frameRate));
    }
    else
    {
        targetFrame = static_cast<int64_t>(layer.frameBuffer.size()) - 1;
    }

    if (targetFrame < 0) targetFrame = 0;
    if (targetFrame >= static_cast<int64_t>(layer.frameBuffer.size()))
        targetFrame = static_cast<int64_t>(layer.frameBuffer.size()) - 1;

    uint32_t drawingIndex = layer.frameBuffer[static_cast<size_t>(targetFrame)];
    if (static_cast<int>(drawingIndex) >= static_cast<int>(layer.drawings.size()))
        drawingIndex = 0;

    return static_cast<int>(drawingIndex);
}

bool StrokeStore::GetLayerAnimationInfo(int layerIdx, uint32_t* frameRate, uint32_t* numFrames, uint32_t* maxRepeatCount) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    
    const StoredLayer& layer = mDocument.layers[layerIdx];
    if (frameRate) *frameRate = layer.frameRate;
    if (numFrames) *numFrames = layer.numFrames;
    if (maxRepeatCount) *maxRepeatCount = layer.maxRepeatCount;
    return true;
}

bool StrokeStore::GetFrameBuffer(int layerIdx, uint32_t* frames, int maxFrames) const
{
    if (!frames || maxFrames <= 0)
        return false;
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return false;
    
    const StoredLayer& layer = mDocument.layers[layerIdx];
    int copyCount = std::min(maxFrames, static_cast<int>(layer.frameBuffer.size()));
    for (int i = 0; i < copyCount; i++)
    {
        frames[i] = layer.frameBuffer[i];
    }
    return true;
}

void StrokeStore::Clear()
{
    mDocument.layers.clear();
    mDocument.chapterStartTimes.clear();
    mDocument.currentChapter = 0;
    mCurrentLayer = nullptr;
    mCurrentDrawing = nullptr;
}

} // namespace ImmStrokeReader
