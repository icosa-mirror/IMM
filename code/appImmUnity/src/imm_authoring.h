#pragma once

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
    IMM_AUTHORING_IO_FAILED = -9,
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

enum { IMM_AUTHORING_STRUCT_VERSION_1 = 1 };

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

IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_Attach(int32_t docId);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_Detach(int32_t docId);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_IsAttached(int32_t docId, int32_t * attachedOut);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_DiscardPending(int32_t docId);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_Commit(int32_t docId, uint64_t * requestedRevisionOut);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_GetRevisions(int32_t docId, ImmAuthoringRevisions * revisionsOut);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_GetCommitStatus(int32_t docId, uint64_t revision,
    ImmAuthoringCommitStatus * statusOut);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_DrawingGetHandle(int32_t docId, int32_t layerId,
    int32_t drawingIndex, uint64_t * drawingIdOut);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_DrawingSetGeometry(int32_t docId, int32_t layerId,
    uint64_t drawingId, int32_t brush, int32_t visible, const ImmAuthoringPoint * points,
    int32_t numPoints, float biggestStroke, int32_t colorSpace);

// Reserved exports retained while creation and frame mapping are implemented on the batch
// model. Both currently return IMM_AUTHORING_UNSUPPORTED.
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_DrawingAdd(int32_t docId, int32_t layerId,
    int32_t brush, int32_t visible, const ImmAuthoringPoint * points, int32_t numPoints,
    float biggestStroke, int32_t colorSpace, int32_t frameIndex, int32_t * drawingIndexOut);
IMM_AUTHORING_EXPORT int IMM_AUTHORING_CALL ImmAuthoring_FrameSet(int32_t docId, int32_t layerId,
    int32_t frameIndex, int32_t drawingIndex);

#ifdef __cplusplus
}
#endif

#undef IMM_AUTHORING_CALL
#undef IMM_AUTHORING_EXPORT
