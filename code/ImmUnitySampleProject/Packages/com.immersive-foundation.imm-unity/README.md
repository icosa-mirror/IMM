# IMM Unity

Local UPM package for the IMM Unity runtime, editor tools, and samples.

## Android Vulkan rendering contract

On Android Vulkan, IMM does not access Unity's display render buffer or
swapchain. Flat cameras render IMM color into explicit, non-MSAA Unity
`RenderTexture` slots. Depth-aware composition uses a corresponding explicit
depth texture. The current render-buffer pointers are supplied with each render
event, so a resolution change or Unity texture recreation cannot leave the
native renderer using a stale image.

The Android player requests an additional graphics queue during Vulkan device
creation. IMM submits its offscreen work on that distinct queue and signals the
slot's bridge semaphore. Unity's graphics queue waits on that semaphore before
the camera consumes the slot. Queue, command-buffer, fence, semaphore, and image
wrapper reuse is tied to the same slot lifetime. The native renderer uses this
path only after confirming the additional queue was created; otherwise it uses
the host-queue fallback and reports the selected mode.

Unity retains ownership of the Android surface, orientation, color conversion,
frame pacing, and presentation. In the built-in render pipeline the original
camera's `OnRenderImage` hook samples the completed IMM texture and writes to
Unity's supplied destination. The depth-aware material compares Unity's camera
depth with IMM's sampled reverse-Z depth before selecting color. Application
code must not cache `Display.main.colorBuffer` for this path: some Android
Vulkan devices expose only a 1x1 placeholder instead of the camera surface.

The CI validation APK runs this contract on physical Firebase hardware. Its
internal render and composition captures are diagnostic; passing requires the
external device video to contain a stable frame that satisfies the perceptual
baseline contract.

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

## Install in a new Unity project

1. Install `ImmStrokeReaderPlugin-Unity` first, then install `ImmPlayerPlugin-Unity`.
   - From a GitHub release zip: unzip each package and add it with **Window > Package Manager > + > Add package from disk...**, selecting each package's `package.json`.
   - From the UPM branch: add the package URL shown in the release notes.
2. Copy a sample IMM file into your project:
   - Create `Assets/StreamingAssets/` if it does not exist.
   - Copy `sample1.imm` into `Assets/StreamingAssets/sample1.imm`.
3. Create a new scene with a camera:
   - Add a `Camera` and tag it `MainCamera`.
   - Create an empty GameObject named `ImmDocument`.
   - Add the script below to that GameObject.
4. Press Play. The script creates/initializes `ImmPlayerManager`, loads `sample1.imm`, applies the first authored spawn area when available, and submits render events through the camera.

```csharp
using System.Collections;
using System.IO;
using ImmPlayer;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Rendering;

public sealed class ImmSamplePlayer : MonoBehaviour
{
    [SerializeField] private Camera targetCamera;
    [SerializeField] private string fileName = "sample1.imm";

    private ImmDocument _document;
    private bool _usesScriptableRenderPipeline;

    private IEnumerator Start()
    {
        targetCamera ??= Camera.main;
        _usesScriptableRenderPipeline = GraphicsSettings.currentRenderPipeline != null;

        string sourcePath = Path.Combine(Application.streamingAssetsPath, fileName);
        string playablePath = Path.Combine(Application.persistentDataPath, fileName);

        using (UnityWebRequest request = UnityWebRequest.Get(sourcePath))
        {
            yield return request.SendWebRequest();
            if (request.result != UnityWebRequest.Result.Success)
            {
                Debug.LogError($"Could not read IMM sample: {sourcePath} ({request.error})");
                yield break;
            }
            File.WriteAllBytes(playablePath, request.downloadHandler.data);
        }

        ImmPlayerManager manager = ImmPlayerManager.Instance;
        yield return new WaitForEndOfFrame();

        _document = manager.LoadDocument(playablePath);
        _document.Resume();
        _document.Show();
        _document.SetTransform(transform);

        StartCoroutine(ApplyInitialSpawnArea());
    }

    private void OnEnable()
    {
        RenderPipelineManager.endCameraRendering += OnEndCameraRendering;
    }

    private void OnDisable()
    {
        RenderPipelineManager.endCameraRendering -= OnEndCameraRendering;
        if (_document != null)
            ImmPlayerManager.Instance.UnloadDocument(_document);
    }

    private void Update()
    {
        if (!_usesScriptableRenderPipeline)
            QueueCamera();
    }

    private void OnRenderObject()
    {
        if (!_usesScriptableRenderPipeline)
            ImmPlayerManager.Instance.IssueRenderEvent(0);
    }

    private void OnEndCameraRendering(ScriptableRenderContext context, Camera camera)
    {
        if (camera != targetCamera)
            return;
        QueueCamera();
        ImmPlayerManager.Instance.IssueRenderEvent(0);
    }

    private void QueueCamera()
    {
        if (_document == null || !_document.IsLoaded || targetCamera == null)
            return;
        ImmPlayerManager.Instance.SetCameraMatrices(0, targetCamera, ImmPlayerManager.StereoMode.Mono);
    }

    private IEnumerator ApplyInitialSpawnArea()
    {
        for (int i = 0; i < 120; ++i)
        {
            if (_document != null && _document.IsLoaded && targetCamera != null &&
                _document.TryGetActiveSpawnAreaViewTargetPose(transform, targetCamera.transform, targetCamera.transform, true, out Pose pose))
            {
                targetCamera.transform.SetPositionAndRotation(pose.position, pose.rotation);
                yield break;
            }
            yield return null;
        }
    }
}
```

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
