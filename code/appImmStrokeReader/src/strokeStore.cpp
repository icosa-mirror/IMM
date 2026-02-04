#include "strokeStore.h"
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

void StrokeStore::OnBeginLayer(uint32_t layerId, uint32_t layerType, const wchar_t* name)
{
    StoredLayer layer;
    layer.layerId = layerId;
    layer.layerType = layerType;
    layer.name = name ? name : L"";
    layer.localTransform = ImmCore::trans3d::identity();
    layer.worldTransform = ImmCore::trans3d::identity();
    mDocument.layers.push_back(layer);
    mCurrentLayer = &mDocument.layers.back();
}

void StrokeStore::OnLayerTransform(uint32_t layerId, const ImmCore::trans3d& localTransform, const ImmCore::trans3d& worldTransform)
{
    if (mCurrentLayer && mCurrentLayer->layerId == layerId)
    {
        mCurrentLayer->localTransform = localTransform;
        mCurrentLayer->worldTransform = worldTransform;
        return;
    }

    for (auto& layer : mDocument.layers)
    {
        if (layer.layerId == layerId)
        {
            layer.localTransform = localTransform;
            layer.worldTransform = worldTransform;
            return;
        }
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

void StrokeStore::Clear()
{
    mDocument.layers.clear();
    mCurrentLayer = nullptr;
    mCurrentDrawing = nullptr;
}

} // namespace ImmStrokeReader
