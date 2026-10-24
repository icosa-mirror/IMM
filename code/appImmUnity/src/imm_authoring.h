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
    IMM_AUTHORING_MAX_POINTS_PER_DRAWING = 1048576,
    IMM_AUTHORING_MAX_FRAMES_PER_SEQUENCE = 1000000
};

typedef enum ImmAuthoringLayerPropertyMask
{
    IMM_AUTHORING_LAYER_PROPERTY_VISIBILITY = 1u << 0,
    IMM_AUTHORING_LAYER_PROPERTY_OPACITY = 1u << 1,
    IMM_AUTHORING_LAYER_PROPERTY_TRANSFORM = 1u << 2
} ImmAuthoringLayerPropertyMask;

typedef enum ImmAuthoringLayerType
{
    IMM_AUTHORING_LAYER_GROUP = 0,
    IMM_AUTHORING_LAYER_PAINT = 1,
    IMM_AUTHORING_LAYER_SPAWN_AREA = 2
} ImmAuthoringLayerType;

typedef enum ImmAuthoringAnimProperty
{
    IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY = 0,
    IMM_AUTHORING_ANIM_PROPERTY_OPACITY = 1,
    IMM_AUTHORING_ANIM_PROPERTY_DRAW_IN_TIME = 5,
    IMM_AUTHORING_ANIM_PROPERTY_ACTION = 6,
    IMM_AUTHORING_ANIM_PROPERTY_LOOP = 7,
    IMM_AUTHORING_ANIM_PROPERTY_OFFSET = 8,
    IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM = 9
} ImmAuthoringAnimProperty;

typedef enum ImmAuthoringInterpolation
{
    IMM_AUTHORING_INTERPOLATION_NONE = 0,
    IMM_AUTHORING_INTERPOLATION_LINEAR = 1,
    IMM_AUTHORING_INTERPOLATION_SMOOTHSTEP = 2,
    IMM_AUTHORING_INTERPOLATION_EASE_IN = 3,
    IMM_AUTHORING_INTERPOLATION_EASE_OUT = 4,
    IMM_AUTHORING_INTERPOLATION_SPLINE = 5,
    IMM_AUTHORING_INTERPOLATION_AUTO = 6
} ImmAuthoringInterpolation;

typedef enum ImmAuthoringSpawnVolumeType
{
    IMM_AUTHORING_SPAWN_VOLUME_SPHERE = 0,
    IMM_AUTHORING_SPAWN_VOLUME_BOX = 1
} ImmAuthoringSpawnVolumeType;

typedef enum ImmAuthoringSpawnTrackingLevel
{
    IMM_AUTHORING_SPAWN_TRACKING_FLOOR = 0,
    IMM_AUTHORING_SPAWN_TRACKING_EYE = 1
} ImmAuthoringSpawnTrackingLevel;

typedef enum ImmAuthoringSpawnTranslationMask
{
    IMM_AUTHORING_SPAWN_TRANSLATION_X = 1u << 0,
    IMM_AUTHORING_SPAWN_TRANSLATION_Y = 1u << 1,
    IMM_AUTHORING_SPAWN_TRANSLATION_Z = 1u << 2
} ImmAuthoringSpawnTranslationMask;

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
    int32_t transformFlip; // 0=None, 1=X, 2=Y, 3=Z.
    uint32_t reserved1;
} ImmAuthoringLayerProperties;

// Pivot is independent of canonical and animated transforms. Flip: 0=None, 1=X, 2=Y, 3=Z.
typedef struct ImmAuthoringLayerPivot
{
    uint32_t structSize;
    uint32_t structVersion;
    float tx, ty, tz;
    float qx, qy, qz, qw;
    float scale;
    int32_t flip;
    uint32_t reserved;
} ImmAuthoringLayerPivot;

typedef struct ImmAuthoringKey
{
    uint32_t structSize;
    uint32_t structVersion;
    int32_t property;
    int32_t interpolation;
    double timeSeconds;
    int32_t boolValue;
    float floatValue;
    float tx, ty, tz;
    float qx, qy, qz, qw;
    float scale;
    uint32_t uintValue; // Offset uses exact IMM ticks, never float seconds.
    int32_t transformFlip; // 0=None, 1=X, 2=Y, 3=Z.
    double doubleValue; // DrawInTime preserves its native scalar precision.
} ImmAuthoringKey;

enum
{
    IMM_AUTHORING_TIMELINE_ROOT = 1u << 0,
    IMM_AUTHORING_TIMELINE_PLAYING = 1u << 1, // Logical intent, not proof that the clock advances.
    IMM_AUTHORING_TIMELINE_WORLD_VISIBLE = 1u << 2,
    IMM_AUTHORING_TIMELINE_HELD = 1u << 3, // Own/ancestor hold, including a finite ancestor's end.
    IMM_AUTHORING_TIMELINE_ACTION_HELD = 1u << 4, // This timeline/root is held by an authored action.
    IMM_AUTHORING_TIMELINE_CLAMPED = 1u << 5, // Native finite-repeat end branch is selected.
    IMM_AUTHORING_TIMELINE_ACTIVE_SPAN = 1u << 6,
    IMM_AUTHORING_TIMELINE_INACTIVE = 1u << 7,
    IMM_AUTHORING_TIMELINE_PREVIEW = 1u << 8, // Independent session clock, not main-scene occurrence.
    IMM_AUTHORING_TIMELINE_PREVIEW_AUDIO = 1u << 9,
    IMM_AUTHORING_TIMELINE_PREVIEW_AUDIO_PENDING = 1u << 10
};

// Unversioned, read-only 96-byte snapshot at the completed CPU timestamp.
// Root reported time preserves GetTime's stopped +1 convention; evaluated time
// is GetTimelineElapsed. localTicks uses the native wrap/clamp rule. cycleIndex
// is elapsed/duration, including beyond a finite end (zero for duration zero).
// Nonroot active spans require an established evaluated visibility span and
// world visibility. Inactive snapshots return elapsed/local/cycle zero; those
// zeros are not a newly evaluated clock. Playing and Held may both be set.
// These are observations of the current occurrence, not a root-seek mapping.
// PREVIEW snapshots use the independent evaluation context; its boundary is
// active even when its main-scene occurrence is inactive. Root time fields
// retain the frozen main-scene clock until the session exits.
typedef struct ImmAuthoringTimelineClock
{
    int32_t timelineLayerId, parentTimelineLayerId;
    int64_t rootReportedTicks, rootEvaluatedTicks, elapsedTicks, localTicks, durationTicks;
    uint64_t cycleIndex;
    uint32_t canonicalRepeat, effectiveRepeat, flags, reserved;
    uint64_t evaluationRevision, presentedRevision, requestedRevision;
} ImmAuthoringTimelineClock;

typedef struct ImmAuthoringSpawnArea
{
    uint32_t structSize;
    uint32_t structVersion;
    int32_t volumeType;
    int32_t trackingLevel;
    uint32_t translationMask;
    uint32_t reserved0;
    float sphereX, sphereY, sphereZ, sphereRadius;
    float boxMinX, boxMinY, boxMinZ;
    float boxMaxX, boxMaxY, boxMaxZ;
    float tx, ty, tz;
    float qx, qy, qz, qw;
    float scale;
    uint32_t reserved1[4];
} ImmAuthoringSpawnArea;

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
static_assert(offsetof(ImmAuthoringLayerProperties, transformFlip) == 56,
    "ImmAuthoringLayerProperties reflection offset changed");
static_assert(sizeof(ImmAuthoringLayerPivot) == 48, "ImmAuthoringLayerPivot ABI changed");
static_assert(sizeof(ImmAuthoringKey) == 80, "ImmAuthoringKey ABI changed");
static_assert(sizeof(ImmAuthoringTimelineClock) == 96, "ImmAuthoringTimelineClock ABI changed");
static_assert(offsetof(ImmAuthoringTimelineClock, timelineLayerId) == 0, "Timeline clock layer offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, parentTimelineLayerId) == 4, "Timeline clock parent offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, rootReportedTicks) == 8, "Timeline clock reported offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, rootEvaluatedTicks) == 16, "Timeline clock evaluated offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, elapsedTicks) == 24, "Timeline clock elapsed offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, localTicks) == 32, "Timeline clock local offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, durationTicks) == 40, "Timeline clock duration offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, cycleIndex) == 48, "Timeline clock cycle offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, canonicalRepeat) == 56, "Timeline clock canonical offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, effectiveRepeat) == 60, "Timeline clock effective offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, flags) == 64, "Timeline clock flags offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, reserved) == 68, "Timeline clock reserved offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, evaluationRevision) == 72, "Timeline clock evaluation offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, presentedRevision) == 80, "Timeline clock presented offset changed");
static_assert(offsetof(ImmAuthoringTimelineClock, requestedRevision) == 88, "Timeline clock requested offset changed");
static_assert(offsetof(ImmAuthoringKey, uintValue) == 64, "ImmAuthoringKey value offset changed");
static_assert(offsetof(ImmAuthoringKey, transformFlip) == 68, "ImmAuthoringKey reflection offset changed");
static_assert(offsetof(ImmAuthoringKey, doubleValue) == 72, "ImmAuthoringKey double value offset changed");
static_assert(sizeof(ImmAuthoringSpawnArea) == 112, "ImmAuthoringSpawnArea ABI changed");
#endif

IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_Attach(int32_t docId);
// Nonroot group/paint/spawn only. Readback reports the presented pivot.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerSetPivot(int32_t docId,
    int32_t layerId, const ImmAuthoringLayerPivot * pivot);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerGetPivot(int32_t docId,
    int32_t layerId, ImmAuthoringLayerPivot * pivotOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_Detach(int32_t docId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_IsAttached(int32_t docId, int32_t * attachedOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_DiscardPending(int32_t docId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_Commit(int32_t docId, uint64_t * requestedRevisionOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_GetRevisions(int32_t docId, ImmAuthoringRevisions * revisionsOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_GetCommitStatus(int32_t docId, uint64_t revision,
    ImmAuthoringCommitStatus * statusOut);
// Paint starts with one empty drawing and frame zero mapped to it; query its
// handle after publication and use the normal drawing geometry edit operations.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerCreate(int32_t docId,
    int32_t parentLayerId, int32_t layerType, const char * nameUtf8, uint32_t nameLength,
    int32_t * layerIdOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerDestroy(int32_t docId,
    int32_t layerId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerReparent(int32_t docId,
    int32_t layerId, int32_t parentLayerId, int32_t childIndex);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerGetProperties(int32_t docId, int32_t layerId,
    ImmAuthoringLayerProperties * propertiesOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerSetProperties(int32_t docId, int32_t layerId,
    const ImmAuthoringLayerProperties * properties);
// Nonroot group/paint/spawn name edit. UTF-8 bytes are copied when queued.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerSetName(int32_t docId,
    int32_t layerId, const char * nameUtf8, uint32_t nameLength);
// requiredBytes includes the terminating NUL. Null buffer/capacity zero queries
// size. Insufficient capacity reports requiredBytes and preserves the buffer.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerGetName(int32_t docId,
    int32_t layerId, char * nameUtf8, uint32_t capacity, uint32_t * requiredBytes);
// Visibility keys refresh ordinary group/static-paint/spawn subtrees. Nonroot
// timeline groups and media/instance/other-resource descendants are unsupported.
// DrawInTime accepts finite nonnegative doubleValue on static paint. Publication
// resamples a held pose; removing its final key restores the layer's base value.
// Draw-in rendering availability is separate from canonical key authoring.
// Action uses uintValue: 0 Stop on timeline groups, 1 Play / 2 Loop on root,
// 3 MakeDefault on viewpoints. MakeDefault tracks actual execution order;
// removal/Undo changes effect membership without replay or active-view selection.
// Step interpolation only; boolValue stays zero, never a consumed-action flag.
// Publication preserves existing holds and unchanged live consumption. New past
// identities are consumed, exact/future identities armed.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_KeySet(int32_t docId, int32_t layerId,
    const ImmAuthoringKey * key);
// Canonical static-paint repeat count; zero repeats indefinitely. Publication
// re-evaluates active Loop keys without replacing the canonical value.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_PaintSetMaxRepeatCount(int32_t docId,
    int32_t layerId, uint32_t count);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_PaintGetMaxRepeatCount(int32_t docId,
    int32_t layerId, uint32_t * countOut);
// Read-only evaluated timing. Offset is the currently evaluated Offset key in
// parent-timeline seconds; repeat counts remain canonical (zero means infinite).
// Non-paint layers return zero paint frame rate/frame count.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerGetTimeline(int32_t docId,
    int32_t layerId, int32_t * isTimelineOut, double * durationSecondsOut,
    double * startOffsetSecondsOut, uint32_t * paintFrameRateOut,
    uint32_t * paintNumFramesOut, uint32_t * maxRepeatCountOut);
// Root or nonroot timeline Group only. Requires at least sizeof(clock) output
// capacity; invalid/not-ready/async-owned queries leave caller memory unchanged.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_TimelineGetClock(int32_t docId,
    int32_t timelineLayerId, ImmAuthoringTimelineClock * clockOut, uint32_t capacityBytes);
// Independent, initially muted preview of a nested timeline. Actions: 0 begin
// paused, 1 seek (unwrapped ticks, paused), 2 play, 3 pause, 4 exit,
// 5 enable independent audio, 6 mute/release preview voices. Calls publish
// under the scene mutex. Main runtime state is restored on exit; committed
// descendant keys/properties/frame tables remain and resample at the main clock.
// Boundary, sound, structure and drawing-geometry edits are unsupported in preview.
// Visibility edits retain static-subtree restrictions. Exit returns QUEUE_FULL
// while a commit is pending and automatically completes once that commit drains.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_TimelinePreviewControl(int32_t docId,
    int32_t timelineLayerId, int32_t action, int64_t ticks);
// Accumulated evaluated layer/ancestor transform in document coordinates;
// excludes document placement and viewer/camera transforms. Reuses the 48-byte
// pivot layout as a transform value; initialize its size/version before calling.
// Preview descendants use the active composition boundary's coordinates.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_LayerGetWorldTransform(int32_t docId,
    int32_t layerId, ImmAuthoringLayerPivot * transformOut);
// Stored key times use the parent timeline's coordinates, before content Offset.
// Keys are indexed in time order. The key output uses the existing 80-byte layout.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_KeyGetCount(int32_t docId,
    int32_t layerId, int32_t property, uint32_t * countOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_KeyGet(int32_t docId,
    int32_t layerId, int32_t property, uint32_t index, ImmAuthoringKey * keyOut);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_KeyRemove(int32_t docId, int32_t layerId,
    int32_t property, double timeSeconds);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_SetInitialSpawnArea(int32_t docId,
    int32_t layerId);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_SpawnAreaSet(int32_t docId,
    int32_t layerId, const ImmAuthoringSpawnArea * spawnArea);
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_SpawnAreaGet(int32_t docId,
    int32_t layerId, ImmAuthoringSpawnArea * spawnAreaOut);
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
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_FrameGetDrawingHandle(int32_t docId,
    int32_t layerId, int32_t frameIndex, uint64_t * drawingIdOut);
// A batch may target a handle reserved by DrawingCreate after DrawingSetGeometry has supplied
// that drawing's contents. Creation, geometry publication, and frame mapping then present as
// one revision.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_FrameSetHandle(int32_t docId, int32_t layerId,
    int32_t frameIndex, uint64_t drawingId);
// Replace the entire frame table atomically. Zero frames accepts a null pointer and
// renders no drawing. Nonempty tables use stable drawing handles from this layer;
// holds are represented by repeated handles. The input is copied before returning.
IMM_AUTHORING_EXPORT int32_t IMM_AUTHORING_CALL ImmAuthoring_FrameSequenceSetHandles(int32_t docId,
    int32_t layerId, const uint64_t * drawingIds, uint32_t frameCount);

#ifdef __cplusplus
}
#endif

#undef IMM_AUTHORING_CALL
#undef IMM_AUTHORING_EXPORT
