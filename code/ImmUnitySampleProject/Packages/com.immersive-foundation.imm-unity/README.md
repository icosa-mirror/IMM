# IMM Unity

Local UPM package for the IMM Unity runtime, editor tools, and samples.

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
