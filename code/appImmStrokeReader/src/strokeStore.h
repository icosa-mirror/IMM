#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "libImmImporter/src/fromImmersive/strokeCollector.h"
#include "libImmImporter/src/document/layerPaint/element.h"
#include "libImmCore/src/libBasics/piVecTypes.h"

namespace ImmStrokeReader
{

// C-friendly structs for Unity interop
struct StrokeLayerInfoC
{
    int id;
    int type;
    int numDrawings;
    wchar_t name[128];
};

struct StrokeInfoC
{
    int brushType;
    int visibilityMode;
    int numPoints;
    float bboxMinX, bboxMinY, bboxMinZ;
    float bboxMaxX, bboxMaxY, bboxMaxZ;
};

struct StrokePointC
{
    float px, py, pz;  // position
    float nx, ny, nz;  // normal
    float dx, dy, dz;  // view direction
    float r, g, b;     // color
    float alpha;       // transparency (0-255 stored as int, converted to float 0-1)
    float width;       // stroke width (quantized int, needs conversion with biggestStroke)
};

// Internal storage structures
struct StoredStroke
{
    uint32_t strokeId;
    uint8_t brushType;
    uint8_t visibilityMode;
    std::vector<ImmImporter::Point> points;
    ImmCore::bound3 bbox;
};

struct StoredDrawing
{
    uint32_t drawingId;
    std::vector<StoredStroke> strokes;
};

struct StoredLayer
{
    uint32_t layerId;
    uint32_t layerType;
    std::wstring name;
    std::vector<StoredDrawing> drawings;
};

struct StoredDocument
{
    std::vector<StoredLayer> layers;
};

// Collector implementation that stores stroke data
class StrokeStore : public ImmImporter::IStrokeCollector
{
public:
    StrokeStore();
    ~StrokeStore();

    // IStrokeCollector interface
    void OnBeginLayer(uint32_t layerId, uint32_t layerType, const wchar_t* name) override;
    void OnBeginDrawing(uint32_t drawingId) override;
    void OnStroke(
        uint32_t strokeId,
        uint8_t brushType,
        uint8_t visibilityMode,
        uint32_t numPoints,
        const ImmImporter::Point* points,
        const ImmCore::bound3& bbox
    ) override;
    void OnEndDrawing() override;
    void OnEndLayer() override;

    // Query interface
    int GetLayerCount() const;
    bool GetLayerInfo(int layerIdx, StrokeLayerInfoC* info) const;
    int GetDrawingCount(int layerIdx) const;
    int GetStrokeCount(int layerIdx, int drawingIdx) const;
    bool GetStrokeInfo(int layerIdx, int drawingIdx, int strokeIdx, StrokeInfoC* info) const;
    bool GetStrokePoints(int layerIdx, int drawingIdx, int strokeIdx, StrokePointC* points, int maxPoints) const;

    // Clear all stored data
    void Clear();

private:
    StoredDocument mDocument;
    StoredLayer* mCurrentLayer;
    StoredDrawing* mCurrentDrawing;
};

} // namespace ImmStrokeReader
