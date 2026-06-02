# IMM Unity

Local UPM package for the IMM Unity runtime, editor tools, and samples.

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
