#include "strokeStore.h"
#include <algorithm>
#include <cstring>

namespace ImmStrokeReader
{

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
    mDocument.layers.push_back(layer);
    mCurrentLayer = &mDocument.layers.back();
}

void StrokeStore::OnBeginDrawing(uint32_t drawingId)
{
    if (!mCurrentLayer) return;

    StoredDrawing drawing;
    drawing.drawingId = drawingId;
    mCurrentLayer->drawings.push_back(drawing);
    mCurrentDrawing = &mCurrentLayer->drawings.back();
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

    // Copy name (truncate if needed)
    size_t copyLen = std::min(layer.name.length(), size_t(127));
    wcsncpy(info->name, layer.name.c_str(), copyLen);
    info->name[copyLen] = L'\0';

    return true;
}

int StrokeStore::GetDrawingCount(int layerIdx) const
{
    if (layerIdx < 0 || layerIdx >= static_cast<int>(mDocument.layers.size()))
        return 0;
    return static_cast<int>(mDocument.layers[layerIdx].drawings.size());
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
        // mWid is stored as quantized int, pass through as-is
        // Consumer needs to know biggestStroke to reconstruct actual width
        dst.width = static_cast<float>(src.mWid);
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
