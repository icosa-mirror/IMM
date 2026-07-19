# IMM Unity

Local UPM package for the IMM Unity runtime, editor tools, and samples.

The production authoring surface is currently available on Windows x64. Query
`ImmAuthoringRuntime.Capabilities` at runtime instead of inferring support from
the presence of a playback plugin. Detailed ownership, threading, limits,
progress, cancellation, and recovery behavior is documented in
`Documentation~/runtime-authoring.md`.

The authoring graph uses stable document-local IDs for layers, drawings, frame
mappings, strokes, and animation keys. Frame IDs remain stable when their list
positions or referenced drawings change.

## Runtime authoring preview

`ImmAuthoringPreviewCoordinator` turns a specific `ImmAuthoringDocument`
revision into an authoritative native-player preview without application UI:

```csharp
ImmAuthoringPreviewCoordinator preview =
    gameObject.AddComponent<ImmAuthoringPreviewCoordinator>();

ImmAuthoringResult<ImmAuthoringPreviewRequest> result =
    preview.RequestPreview(document, document.Revision);
```

Compilation uses an immutable snapshot on a serialized worker task. Native
player loading and playback state changes stay on Unity's main thread. A newer
request cancels and supersedes obsolete queued, compiling, or loading work.
The old native document remains installed until the replacement reaches the
fully-loaded state. Failed and cancelled replacements leave the last valid
preview intact.

The overload accepting `ImmAuthoringPreviewSettings` applies an explicit
playback state, playback time, and document-to-world matrix. The overload
without settings captures those values from the installed preview when making
a replacement request. `InstalledRevision`, `InstalledAuthoringDocumentId`,
`InstalledDocument`, and `InstalledRequest` expose the authoritative result.
Each request reports state transitions, structured errors, source revision,
compiled byte count, graph/serialization timing, player-load timing, and total
latency. Call `CancelPreview` for active work and `ClearPreview` when its owned
native document should be unloaded.

## Supported paint import and round trip

`ImmAuthoringImporter` loads the supported paint-oriented IMM subset from a
file or byte array into the same mutable graph used for procedural authoring:

```csharp
ImmAuthoringImportResult import =
    ImmAuthoringImporter.ImportFromMemory(immBytes);

if (import.Succeeded && import.CanOverwriteSource)
{
    ImmAuthoringDocument document = import.Document;
    // Query or mutate layers, drawings, strokes, frames, and supported keys.
}
```

The result exposes `Lossiness`, `CanOverwriteSource`, structured `Issues`, and
import statistics. Unsupported or repaired source content makes the import
lossy before any later export is attempted. Ownership of a successful
`Document` belongs to the caller and it must be disposed.

The verified round-trip subset contains nested group and paint layers; segment,
circle, ellipse, and square strokes; always-visible and quadratic-fade stroke
visibility; frame mappings; and visibility, opacity, transform, draw-in-time,
action, loop, and offset animation keys. Point brushes and obsolete component
position/rotation/scale keys are rejected before native export.

`ImmAuthoringStructuralComparer.Compare` checks hierarchy, ordering, layer
properties, frame mappings, supported animation keys, and stroke geometry
within an explicit tolerance. It compares semantic IMM data: view direction on
always-visible strokes is omitted by the binary format, while point length and
time are derived by the format rather than stored verbatim.

The Runtime Authoring sample demonstrates memory export, lossless import,
structural verification, stable-ID mutation of imported strokes, a frame
mapping, and an animation key, and native preview replacement without writing
an IMM file. It also exposes capability results and operation progress, and runs
controlled resource-limit, cancellation, and malformed-input checks without
disturbing the installed preview.

## Production operation controls

Compiler and importer overloads accept `ImmAuthoringOperationOptions`:

```csharp
ImmAuthoringOperationOptions options = new ImmAuthoringOperationOptions(
    cancellationToken,
    progress,
    ImmAuthoringLimits.Default);

ImmAuthoringExportResult export =
    ImmAuthoringCompiler.ExportToMemory(document, options);
```

Progress is reported across validation, graph compilation/import,
serialization, and atomic file writing. Cancellation returns
`ImmAuthoringErrorCode.Cancelled` without a
partial public document or memory buffer. Configured safety envelopes return
`ResourceLimitExceeded`; unreadable IMM input returns `CorruptInput`. File
export uses a same-directory temporary file so a cancelled or failed operation
does not replace an existing destination.

## Runtime navigation APIs

`ImmDocument` now exposes direct chapter and spawn-area navigation helpers that can be used from any project code (not only the example scripts).

### Chapters

- `SetChapter(int chapterIndex)`
- `GetChapterCount()`
- `GetCurrentChapter()`

### Spawn areas / viewpoints

- `GetSpawnAreaCount()`
- `GetSpawnAreaList()`
- `GetActiveSpawnAreaId()`
- `SetActiveSpawnAreaId(int spawnAreaId)`
- `GetSpawnAreaInfoManaged(int spawnAreaId)`

### World/view pose helpers

Use these to convert spawn area data into Unity world/view transforms in a reusable way:

- `TryGetSpawnAreaWorldPose(int spawnAreaId, Transform documentRoot, out Pose worldPose)`
- `TryGetActiveSpawnAreaWorldPose(Transform documentRoot, out Pose worldPose)`
- `TryGetSpawnAreaViewTargetPose(int spawnAreaId, Transform documentRoot, Transform currentViewTarget, Transform currentHead, bool keepHeadHeightForFloorAreas, out Pose targetPose)`
- `TryGetActiveSpawnAreaViewTargetPose(Transform documentRoot, Transform currentViewTarget, Transform currentHead, bool keepHeadHeightForFloorAreas, out Pose targetPose)`

These helper methods include IMM-to-Unity coordinate conversion and upright/yaw-safe view alignment so camera-rig movement logic can be shared across scenes.
