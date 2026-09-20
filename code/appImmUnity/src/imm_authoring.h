#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define IMM_AUTHORING_CALL __stdcall
#if defined(RENDERINGPLUGIN_EXPORTS)
#define IMM_AUTHORING_EXPORT __declspec(dllexport)
#else
#define IMM_AUTHORING_EXPORT __declspec(dllimport)
#endif
#else
#define IMM_AUTHORING_CALL
#define IMM_AUTHORING_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ImmAuthoringResult
{
    IMM_AUTHORING_OK = 0,
    IMM_AUTHORING_NOT_FOUND = -1,
    IMM_AUTHORING_INVALID_ARGUMENT = -2,
    IMM_AUTHORING_UNSUPPORTED = -3,
    IMM_AUTHORING_INVALID_STATE = -4,
    IMM_AUTHORING_OUT_OF_MEMORY = -5,
    IMM_AUTHORING_VALIDATION_FAILED = -6,
    IMM_AUTHORING_QUEUE_FULL = -7,
    IMM_AUTHORING_DEPENDENCY_FAILED = -8,
    IMM_AUTHORING_CANCELLED = -9,
    IMM_AUTHORING_RENDERER_FAILED = -10
} ImmAuthoringResult;

typedef enum ImmAuthoringCommitState
{
    IMM_AUTHORING_COMMIT_UNKNOWN = 0,
    IMM_AUTHORING_COMMIT_QUEUED = 1,
    IMM_AUTHORING_COMMIT_PREPARING = 2,
    IMM_AUTHORING_COMMIT_PREPARED = 3,
    IMM_AUTHORING_COMMIT_PRESENTED = 4,
    IMM_AUTHORING_COMMIT_REJECTED = 5
} ImmAuthoringCommitState;

enum
{
    IMM_AUTHORING_STRUCT_VERSION_1 = 1,
    IMM_AUTHORING_MIN_POINTS_PER_ELEMENT = 2,
    IMM_AUTHORING_MAX_POINTS_PER_ELEMENT = 8192,
    IMM_AUTHORING_MAX_ELEMENTS_PER_DRAWING = 65536,
    IMM_AUTHORING_MAX_POINTS_PER_DRAWING = 1048576
};

typedef enum ImmAuthoringLayerPropertyMask
{
    IMM_AUTHORING_LAYER_PROPERTY_VISIBILITY = 1u << 0,
    IMM_AUTHORING_LAYER_PROPERTY_OPACITY = 1u << 1,
    IMM_AUTHORING_LAYER_PROPERTY_TRANSFORM = 1u << 2
} ImmAuthoringLayerPropertyMask;

typedef struct ImmAuthoringPoint
{
    float px, py, pz;
    float nx, ny, nz;
    float dx, dy, dz;
    float r, g, b;
    float alpha;
    float width;
    float length;
    float time;
} ImmAuthoringPoint;

typedef struct ImmAuthoringElementGeometry
{
    uint32_t structSize;
    uint32_t structVersion;
    int32_t brush;
    int32_t visibility;
    uint32_t pointCount;
    uint32_t reserved;
    const ImmAuthoringPoint * points;
} ImmAuthoringElementGeometry;

typedef struct ImmAuthoringDrawingGeometry
{
    uint32_t structSize;
    uint32_t structVersion;
    uint32_t elementCount;
    uint32_t reserved0;
    const ImmAuthoringElementGeometry * elements;
    float biggestStroke;
    int32_t colorSpace;
    int32_t flipped;
    uint32_t reserved1;
} ImmAuthoringDrawingGeometry;

typedef struct ImmAuthoringRevisions
{
    uint32_t structSize;
    uint32_t structVersion;
    uint64_t requested;
    uint64_t prepared;
    uint64_t presented;
} ImmAuthoringRevisions;

typedef struct ImmAuthoringCommitStatus
{
    uint32_t structSize;
    uint32_t structVersion;
    uint64_t revision;
    int32_t state;
    int32_t result;
    uint32_t failingCommand;
    uint32_t reserved;
    uint64_t object;
} ImmAuthoringCommitStatus;

typedef struct ImmAuthoringLayerProperties
{
    uint32_t structSize;
    uint32_t structVersion;
    uint32_t updateMask;
    uint32_t reserved0;
    int32_t visible;
    float opacity;
    float tx, ty, tz;
    float qx, qy, qz, qw;
    float scale;
    uint32_t reserved1[2];
} ImmAuthoringLayerProperties;

#ifdef __cplusplus
// These layouts cross the native-library boundary. Keep the assertions beside the
// public declarations so every C++ target catches an accidental ABI change.
static_assert(sizeof(ImmAuthoringPoint) == 64, "ImmAuthoringPoint ABI changed");
static_assert(offsetof(ImmAuthoringElementGeometry, points) == 24,
    "ImmAuthoringElementGeometry ABI changed");
static_assert(sizeof(ImmAuthoringElementGeometry) == 24 + sizeof(void *),
    "ImmAuthoringElementGeometry ABI changed");
static_assert(offsetof(ImmAuthoringDrawingGeometry, elements) == 16,
    "ImmAuthoringDrawingGeometry ABI changed");
static_assert(offsetof(ImmAuthoringDrawingGeometry, biggestStroke) == 16 + sizeof(void *),
    "ImmAuthoringDrawingGeometry ABI changed");
static_assert(sizeof(ImmAuthoringDrawingGeometry) == 32 + sizeof(void *),
    "ImmAuthoringDrawingGeometry ABI changed");
static_assert(sizeof(ImmAuthoringRevisions) == 32, "ImmAuthoringRevisions ABI changed");
static_assert(sizeof(ImmAuthoringCommitStatus) == 40, "ImmAuthoringCommitStatus ABI changed");
static_assert(sizeof(ImmAuthoringLayerProperties) == 64,
    "ImmAuthoringLayerProperties ABI changed");
#endif

IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_Attach(int32_t docId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_Detach(int32_t docId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_IsAttached(int32_t docId, int32_t * attachedOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DiscardPending(int32_t docId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_Commit(int32_t docId, uint64_t * requestedRevisionOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_GetRevisions(int32_t docId, ImmAuthoringRevisions * revisionsOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_GetCommitStatus(int32_t docId, uint64_t revision,
    ImmAuthoringCommitStatus * statusOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerSetProperties(int32_t docId, int32_t layerId,
    const ImmAuthoringLayerProperties * properties);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DrawingGetHandle(int32_t docId, int32_t layerId,
    int32_t drawingIndex, uint64_t * drawingIdOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DrawingCreate(int32_t docId, int32_t layerId,
    uint64_t * drawingIdOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DrawingDestroy(int32_t docId, int32_t layerId,
    uint64_t drawingId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DrawingSetGeometry(int32_t docId, int32_t layerId,
    uint64_t drawingId, const ImmAuthoringDrawingGeometry * geometry);

// Legacy index-based creation is retained as an unsupported export. New callers reserve a
// stable handle with ImmAuthoring_DrawingCreate and supply geometry in the same open batch.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DrawingAdd(int32_t docId, int32_t layerId,
    int32_t brush, int32_t visible, const ImmAuthoringPoint * points, int32_t numPoints,
    float biggestStroke, int32_t colorSpace, int32_t frameIndex, int32_t * drawingIndexOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_FrameSet(int32_t docId, int32_t layerId,
    int32_t frameIndex, int32_t drawingIndex);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_FrameSetHandle(int32_t docId, int32_t layerId,
    int32_t frameIndex, uint64_t drawingId);

#ifdef __cplusplus
}
#endif

#undef IMM_AUTHORING_CALL
#undef IMM_AUTHORING_EXPORT
