# IMM Unity

Local UPM package for the IMM Unity runtime, editor tools, and samples.

## Experimental URP RenderGraph sample

The repository sample project includes `Assets/Scenes/SampleSceneURP.unity` for
Unity 6.6 / URP 17.6 on Windows D3D12. Start the Editor with `-force-d3d12`, open
that scene, and enter Play Mode. Use a plugin built from the same source revision.
The sample loads `StreamingAssets/sample1.imm` and frames its bounds at three seconds.

`Assets/Settings/ImmURPRenderer.asset` contains `ImmRendererFeature`; the scene's
camera has `ImmCamera`. `ImmUrpSample` selects the configured URP asset for the
scene's lifetime and restores the previous quality pipeline when destroyed. For
an application, assign the URP asset in its graphics/quality settings, add the
renderer feature, add `ImmCamera` to each participating camera, and initialize
an `ImmPlayerManager`. Rendering uses RenderGraph at `AfterRenderingSkybox`.

Select `ImmPlayerManager.UrpPaintRenderingTechnique` before initialization: `Static`
is the default; `Pretessellated` selects the other native paint import/render path.
Changing the technique requires shutting down the manager and initializing it again.
The mono CI content gate loads the fixture corpus independently under both techniques.

The implementation accepts perspective or orthographic base cameras, a full
viewport, matching colour/depth attachments with 1, 2, 4 or 8 samples and no
dynamic resolution. D3D12 has mono and two-slice single-pass packet paths;
Metal currently accepts mono cameras. Vulkan also has an experimental two-view
single-pass path requiring matching two-layer attachments and observed logical-device
multiview enablement. Actual Unity/OpenXR headset acceptance remains outstanding.
Unsupported MSAA levels must
be skipped explicitly rather than reported as validated at a downgraded level.
See `LLM Docs/IMM-URP-single-pass-feature-request.md` in the repository for scoped
CI evidence and remaining layer/platform/headset acceptance work.

The Vulkan native plugin must load at startup to observe the features enabled
on Unity's logical device. The shipped Windows and Android plugin metadata
sets `Load on startup`. Device creation is forwarded unchanged. Late plugin
loading cannot establish enabled multiview capability, so two-view packets are
rejected in that case. Native GPU adapter tests establish two-slice submission;
they do not establish Quest headset correctness.

## Submission diagnostics

`ImmRenderingDiagnostics.SubmissionCompleted` reports a native scene event after
its completion is acknowledged on the main thread. Each record includes the
source camera, recorded frame, request sequence, XR pass, view count, native
scene-event count and result. Mono uses XR pass `-1`; the supported single
two-view XR pass uses `0`. Direct transport probes can have a null camera.
Callbacks run during completion polling: do not queue or dispose sessions from
a callback. Avoid allocating in listeners during performance measurements.
With no subscribers, the transport skips diagnostic record construction.
Idle diagnostic fixtures should call `PollCompletions()` on the main thread after
their GPU readback completes; it polls acknowledgements without waiting or rendering.

This requires packet ABI version 4 (496 bytes). Ship matching managed and native
plugin revisions; older packet layouts are rejected. Automated mono checks cover
two distinct cameras and camera opt-out; synthetic stereo checks cover one native
scene event for a two-view request. These counters do not prove GPU draw counts
or actual headset single-pass execution.

`ManagedAllocationMeasured` optionally detects managed allocations on the current thread inside
`AddRenderPasses`, `RecordRenderGraph` and `ExecuteNative`, including transport work
and any submission listeners they invoke. No profiler recorders are created without
a measurement subscriber. Measurement listeners run after recording stops
and must not allocate or change rendering state. Native/GPU allocations and surrounding
Unity frame work are outside these scopes. CI calibrates Unity's `GC.Alloc` recorder,
warms the mono pass for 16 frames, then requires no allocation samples over at least
32 calls to each callback. A one-sample recorder detects any allocation; it does not
report total allocated bytes. The CLR thread-allocation counter is unimplemented in IL2CPP.
Allocation probes require a working `GC.Alloc` marker (CI uses Development players);
failed calibration must not be interpreted as zero allocations.
The hardware XR probe warms for 32 acknowledged stereo frames, then requires
no allocation samples over at least 64 calls to each callback. Hosted mono and hardware
stereo execution of these allocation checks remain outstanding.

## OpenXR sample builds

The repository sample has dedicated OpenXR builds:
`ImmPlayer.Editor.ImmUrpXrBuild.BuildWindowsOpenXRPlayer` (D3D12) and
`ImmPlayer.Editor.ImmUrpXrBuild.BuildAndroidOpenXRQuestPlayer` (Vulkan/ARM64).
They build `SampleSceneURP`, require one configured OpenXR loader, select
Single Pass Instanced and add a tracked head camera under a separate viewing
origin. Use `-immUrpXrPlayerPath` or `-immQuestPlayerPath` for the output path.
Build-time settings are restored afterwards. CI builds both variants; actual
OpenXR rendering/headset acceptance is a separate outstanding check.

On a Windows runner with an active OpenXR headset session, launch
`ImmUnityOpenXR.exe -force-d3d12 -immUrpXrSmoke -immUrpXrCapturePath "<absolute PNG path>"`.
The opt-in hardware probe waits for a loaded document and running stereo display,
requires one display pass with two views and 120 distinct camera-attributed stereo
frames with one successful native scene event each, then reads both layers of
the actual XR colour target. It checks visible baseline content and Unity opaque
and transparent geometry in front of/behind IMM in both eyes. Ten per-eye PNGs
are written under `imm-urp-xr-<run ID>` beside the mirror capture before exit.
Failure or timeout exits with an error. The Windows hardware CI lane runs this
same-commit player instead of the legacy Editor scene. This prepared probe still
needs hardware execution; its depth checks do not replace visual/MSAA edge
acceptance or GPU draw-count evidence.

For Quest, run `code/projects/android/run-unity-quest-urp-smoke.ps1 -Apk <Quest APK>`
from the repository root, optionally adding `-Serial <ADB serial>`. It requires an
authorized, awake headset and an inactive Unity sample. The harness installs the
specified APK, launches the same probe through Unity's Android intent arguments,
uses a unique run ID to reject stale logs, pulls a mirror PNG and all ten per-eye
depth PNGs, and stops its own
sample instance afterwards. It preserves shared logcat buffers. The gated
`Unity Quest URP OpenXR VR` CI lane downloads the same-commit APK and retains
correlated logs, capture and manifest. Both XR matrix rows remain deferred pending
actual hardware evidence; the automated probe does not replace per-eye acceptance.

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

The production authoring surface is available on Windows, macOS, Android and iOS
(64-bit). Query `ImmAuthoringRuntime.Capabilities` at runtime instead of inferring
support from the presence of a playback plugin. Detailed ownership, threading, limits,
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
