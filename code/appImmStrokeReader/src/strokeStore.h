#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "libImmImporter/src/fromImmersive/strokeCollector.h"
#include "libImmImporter/src/document/layerPaint/element.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piTick.h"

namespace ImmImporter
{
class Sequence;
}

namespace ImmStrokeReader
{

// C-friendly structs for Unity interop
struct StrokeLayerInfoC
{
    int id;
    int type;
    int numDrawings;
    char name[256];
    int visible;
    float opacity;
    int isDefaultSpawn;
    float pivotRotation[4];
    float pivotScale;
    int pivotFlip;
    float pivotTranslation[3];
    int parentId;
    int childIndex;
    int isTimeline;
    int64_t durationTicks;
    uint32_t maxRepeatCount;
};

struct StrokeLayerTransformC
{
    float rotation[4];
    float scale;
    int flip;
    float translation[3];
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
    float length;
    float time;
};

struct StrokeDocumentInfoC
{
    int sequenceType;
    uint32_t frameRate;
    float backgroundColor[3];
    uint32_t capabilities;
    int rootAnimationKeyCount;
};

struct StrokeAnimationKeyC
{
    int property;
    int64_t timeTicks;
    int interpolation;
    int boolValue;
    uint32_t intValue;
    float floatValue;
    double doubleValue;
    StrokeLayerTransformC transformValue;
};

struct StrokePictureInfoC
{
    int layerId;
    int contentType;
    int isViewerLocked;
    int width;
    int height;
    int hasAlpha;
    int dataSize;
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
    float biggestStroke = 0.0f;
    std::vector<StoredStroke> strokes;
};

struct StoredLayer
{
    uint32_t layerId;
    uint32_t layerType;
    std::wstring name;
    bool visible = true;
    double opacity = 1.0;
    ImmCore::trans3d pivotTransform = ImmCore::trans3d::identity();
    ImmCore::trans3d localTransform = ImmCore::trans3d::identity();
    ImmCore::trans3d worldTransform = ImmCore::trans3d::identity();
    bool hasPicture = false;
    uint32_t pictureContentType = 0;
    bool pictureViewerLocked = false;
    int pictureWidth = 0;
    int pictureHeight = 0;
    bool pictureHasAlpha = false;
    bool isDefaultSpawn = false;
    int32_t parentId = 0;
    int32_t childIndex = 0;
    bool isTimeline = false;
    int64_t durationTicks = 0;
    uint32_t maxRepeatCount = 0;
    std::vector<StrokeAnimationKeyC> animationKeys;
    std::vector<uint8_t> picturePixels;
    std::vector<StoredDrawing> drawings;
    
    // Animation info for paint layers
    uint32_t frameRate = 24;
    uint32_t numFrames = 0;
    uint32_t paintMaxRepeatCount = 1;
    std::vector<uint32_t> frameBuffer;
};

struct StoredDocument
{
    int sequenceType = 0;
    uint32_t frameRate = 24;
    float backgroundColor[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t capabilities = 0;
    int rootAnimationKeyCount = 0;
    std::vector<StoredLayer> layers;
    std::vector<ImmCore::piTick> chapterStartTimes;
    int currentChapter = 0;
};

// Collector implementation that stores stroke data
class StrokeStore : public ImmImporter::IStrokeCollector
{
public:
    StrokeStore();
    ~StrokeStore();

    // IStrokeCollector interface
    void OnBeginLayer(uint32_t layerId, uint32_t layerType, const wchar_t* name, bool visible, float opacity) override;
    void OnLayerTransform(uint32_t layerId, const ImmCore::trans3d& localTransform, const ImmCore::trans3d& worldTransform, const ImmCore::trans3d& pivotTransform) override;
    void OnPictureLayer(
        uint32_t layerId,
        uint32_t contentType,
        bool isViewerLocked,
        int width,
        int height,
        bool hasAlpha,
        const uint8_t* pixels,
        int pixelDataSize) override;
    void OnSpawnArea(uint32_t layerId, bool isDefault) override;
    void OnPaintLayerInfo(uint32_t frameRate, uint32_t numFrames, uint32_t maxRepeatCount) override;
    void OnFrameBuffer(const uint32_t* frameBuffer, uint32_t numFrames) override;
    void OnBeginDrawing(uint32_t drawingId) override;
    void OnDrawingBiggestStroke(uint32_t drawingId, float biggestStroke) override;
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
    bool GetLayerTransform(int layerIdx, StrokeLayerTransformC* local, StrokeLayerTransformC* world) const;
    int GetDrawingCount(int layerIdx) const;
    bool GetDrawingBiggestStroke(int layerIdx, int drawingIdx, float* biggestStroke) const;
    int GetStrokeCount(int layerIdx, int drawingIdx) const;
    bool GetStrokeInfo(int layerIdx, int drawingIdx, int strokeIdx, StrokeInfoC* info) const;
    bool GetStrokePoints(int layerIdx, int drawingIdx, int strokeIdx, StrokePointC* points, int maxPoints) const;
    bool GetStrokePointTimes(int layerIdx, int drawingIdx, int strokeIdx, float* times, int maxPoints) const;
    bool GetPictureInfo(int layerIdx, StrokePictureInfoC* info) const;
    bool GetPicturePixels(int layerIdx, uint8_t* pixels, int maxBytes) const;
    
    // Frame buffer queries
    bool GetLayerAnimationInfo(int layerIdx, uint32_t* frameRate, uint32_t* numFrames, uint32_t* maxRepeatCount) const;
    bool GetFrameBuffer(int layerIdx, uint32_t* frames, int maxFrames) const;
    
    int GetChapterCount() const;
    int GetCurrentChapter() const;
    bool SetCurrentChapter(int chapterIndex);
    void SetChapterStartTimes(const std::vector<ImmCore::piTick>& chapterStartTimes);
    int GetDrawingIndexForChapter(int layerIdx, int chapterIndex) const;
    void CaptureSequenceMetadata(ImmImporter::Sequence& sequence);
    bool GetDocumentInfo(StrokeDocumentInfoC* info) const;
    int GetLayerAnimationKeyCount(int layerIdx) const;
    bool GetLayerAnimationKey(int layerIdx, int keyIdx, StrokeAnimationKeyC* key) const;

    // Clear all stored data
    void Clear();

private:
    StoredDocument mDocument;
    StoredLayer* mCurrentLayer;
    StoredDrawing* mCurrentDrawing;
};

} // namespace ImmStrokeReader
