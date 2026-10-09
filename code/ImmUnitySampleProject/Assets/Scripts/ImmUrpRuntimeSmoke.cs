using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using ImmPlayer;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using UnityEngine.Networking;

/// <summary>Opt-in CI probe for the configured URP scene.</summary>
public sealed class ImmUrpRuntimeSmoke : MonoBehaviour
{
    private RenderTexture target;
    private Camera documentCamera;
    private string renderError;
    private int cameraFrames;
    private readonly List<ImmSceneSubmission> sceneSubmissions = new List<ImmSceneSubmission>(64);
    private Camera attributionCamera;
    private bool captureSubmissions, submissionOverflow;
    private readonly int[] allocationSamples = new int[3];
    private readonly int[] allocatingCallbacks = new int[3];
    private UniversalRenderPipeline.SingleCameraRequest request;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Install()
    {
#if IMM_UNITY_ANDROID_URP_CI
        Environment.SetEnvironmentVariable("IMM_UNITY_URP_CAPTURE", Path.Combine(Application.persistentDataPath, "unity-urp-scene.png"));
        bool enabled = true;
#else
        bool enabled = Environment.GetEnvironmentVariable("IMM_UNITY_URP_SMOKE") == "1";
#endif
        if (enabled)
            new GameObject("IMM URP CI probe").AddComponent<ImmUrpRuntimeSmoke>();
    }

    private IEnumerator Start()
    {
        Application.logMessageReceived += OnLog;
        Application.runInBackground = true;
        RenderPipelineManager.beginCameraRendering += OnCamera;
        ImmRenderingDiagnostics.SubmissionCompleted += OnSubmission;
        var probe = Run();
        while (true)
        {
            bool next;
            try { next = probe.MoveNext(); }
            catch (Exception error)
            {
                Debug.LogError($"[IMM_URP_SMOKE] FAIL cameraFrames={cameraFrames} pipeline={GraphicsSettings.currentRenderPipeline} {error}");
                Application.Quit(1);
                yield break;
            }
            if (!next) break;
            yield return probe.Current;
        }
        Debug.Log("[IMM_URP_SMOKE] PASS configured sample rendered, opted out and shut down.");
#if !IMM_UNITY_ANDROID_URP_CI
        Application.Quit(0);
#endif
    }

    private IEnumerator Run()
    {
        bool metal = SystemInfo.graphicsDeviceType == GraphicsDeviceType.Metal;
        bool vulkan = SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan;
        Require(metal || vulkan || SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D12, "D3D12, Metal or Vulkan is required.");
        Debug.Log($"[IMM_URP_SMOKE] graphicsDevice={SystemInfo.graphicsDeviceType}");
        var sample = FindFirstObjectByType<ImmUrpSample>();
        Require(sample != null, "Configured URP sample is missing.");
        var optIn = FindFirstObjectByType<ImmCamera>();
        Require(optIn != null, "Opted-in camera is missing.");
        documentCamera = optIn.GetComponent<Camera>();
        target = new RenderTexture(new RenderTextureDescriptor(256, 256)
        {
            graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
            depthStencilFormat = SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil),
            msaaSamples = 1
        });
        Require(target.Create(), "Could not create the platform-supported colour/depth target.");
        documentCamera.targetTexture = target;
        // Explicit offscreen requests exercise URP and its feature even when the
        // CI player's hidden window does not receive automatic camera rendering.
        request = new UniversalRenderPipeline.SingleCameraRequest { destination = target };
        float deadline = Time.realtimeSinceStartup + 30;
        int visible;
        do
        {
            yield return null;
            RenderPipeline.SubmitRenderRequest(documentCamera, request);
            Require(renderError == null, $"Unity reported: {renderError}");
            visible = ReadVisiblePixels(false);
        } while (visible <= 100 && Time.realtimeSinceStartup < deadline);
        Require(visible > 100, "No visible IMM content in the configured URP scene.");
        var pipeline = QualitySettings.renderPipeline as UniversalRenderPipelineAsset;
        Require(pipeline != null, "Configured URP asset is missing.");
        foreach (int samples in new[] { 1, 2, 4, 8 })
        {
            var requestedDescriptor = target.descriptor;
            requestedDescriptor.msaaSamples = samples;
            int supportedSamples = SystemInfo.GetRenderTextureSupportedMSAASampleCount(requestedDescriptor);
            Debug.Log($"[IMM_URP_SMOKE] MSAA requested={samples} supported={supportedSamples} colour={requestedDescriptor.graphicsFormat} depth={requestedDescriptor.depthStencilFormat}");
            if ((metal || vulkan) && supportedSamples != samples)
            {
                Debug.Log($"[IMM_URP_SMOKE] UNSUPPORTED samples={samples} supported={supportedSamples}");
                continue;
            }
            target.Release();
            target.antiAliasing = samples;
            Require(target.Create(), $"Could not recreate the {samples}-sample target.");
            pipeline.msaaSampleCount = samples;
            Require(target.antiAliasing == samples, $"Requested {samples} samples were not retained.");
            for (int frame = 0; frame < 3; ++frame)
            {
                yield return null;
                RenderPipeline.SubmitRenderRequest(documentCamera, request);
            }
            Require(renderError == null, $"Unity reported: {renderError}");
            Require(ReadVisiblePixels(true, samples) > 100, $"No visible IMM content at {samples} samples.");
            var depthProbe = VerifyDepthComposition(samples, sample.Document.GetBoundingBox());
            try
            {
                while (depthProbe.MoveNext()) yield return depthProbe.Current;
            }
            finally { (depthProbe as IDisposable)?.Dispose(); }
            yield return null;
            RenderPipeline.SubmitRenderRequest(documentCamera, request);
            Require(ReadVisiblePixels(false) > 100, "IMM scene did not return after depth probe removal.");
            Debug.Log($"[IMM_URP_SMOKE] PASS rendered at {samples} samples.");
        }
        ReadVisiblePixels(true);
        var attribution = VerifySubmissionAttribution();
        try { while (attribution.MoveNext()) yield return attribution.Current; }
        finally { (attribution as IDisposable)?.Dispose(); }
        var allocations = VerifyManagedAllocations();
        try { while (allocations.MoveNext()) yield return allocations.Current; }
        finally { (allocations as IDisposable)?.Dispose(); }
        if (vulkan) VerifyQueuedCameras();
        optIn.enabled = false;
        for (int frame = 0; frame < 3; ++frame) yield return null;
        RenderPipeline.SubmitRenderRequest(documentCamera, request);
        Require(ReadVisiblePixels(false) == 0, "IMM content remained after camera opt-out.");
        var manager = FindFirstObjectByType<ImmPlayerManager>();
        Require(manager != null && manager.IsInitialized, "Native session was not initialized.");
        Require(sample.Document != null, "Sample document is missing.");
        if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D12)
        {
            foreach (int stereoSamples in new[] { 1, 2, 4, 8 })
            {
                var stereoProbe = ImmRenderGraphValidation.VerifyStereoPacket(documentCamera,
                    Mathf.Max(sample.Document.GetBoundingBox().extents.magnitude, 0.1f), stereoSamples);
                // Advance explicitly so the outer runner catches validation failures.
                try
                {
                    while (stereoProbe.MoveNext()) yield return stereoProbe.Current;
                }
                finally { (stereoProbe as IDisposable)?.Dispose(); }
            }
            Debug.Log("[IMM_URP_SMOKE] PASS managed stereo packet renders distinct eye slices.");
        }
        int documentId = sample.Document.DocumentId;
        Require(ImmNativePlugin.IsDocumentActive(documentId), "Sample document was already inactive before unload.");
        manager.UnloadDocument(sample.Document);
        deadline = Time.realtimeSinceStartup + 30;
        while (ImmNativePlugin.IsDocumentActive(documentId) && Time.realtimeSinceStartup < deadline)
            yield return null;
        Require(!ImmNativePlugin.IsDocumentActive(documentId), "Document unload stalled after camera opt-out.");
        Debug.Log("[IMM_URP_SMOKE] PASS camera-free document unload completed.");
        var loadingDocument = manager.LoadDocument(sample.DocumentPath);
        Require(loadingDocument != null, "Could not queue document for deferred unload.");
        documentId = loadingDocument.DocumentId;
        Require(ImmNativePlugin.IsDocumentActive(documentId), "Queued document is inactive.");
        Require(loadingDocument.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded,
            "Deferred-unload probe did not begin during loading.");
        manager.UnloadDocument(loadingDocument);
        deadline = Time.realtimeSinceStartup + 30;
        while (ImmNativePlugin.IsDocumentActive(documentId) && Time.realtimeSinceStartup < deadline)
            yield return null;
        Require(!ImmNativePlugin.IsDocumentActive(documentId), "Deferred unload stalled without a camera.");
        Debug.Log("[IMM_URP_SMOKE] PASS deferred camera-free unload completed during loading.");
        var content = VerifyContentFixtures(manager, optIn);
        try { while (content.MoveNext()) yield return content.Current; }
        finally { (content as IDisposable)?.Dispose(); }
        Destroy(sample.gameObject);
        for (int frame = 0; frame < 3; ++frame) yield return null;
        Require(manager == null, "The sample left its persistent manager alive after destruction.");
        Require(renderError == null, $"Unity reported: {renderError}");
    }

    private IEnumerator VerifyContentFixtures(ImmPlayerManager manager, ImmCamera optIn)
    {
        var originalPosition = documentCamera.transform.position;
        var originalRotation = documentCamera.transform.rotation;
        bool originalEnabled = documentCamera.enabled;
        bool originalOptIn = optIn.enabled;
        bool originalOrthographic = documentCamera.orthographic;
        float originalFov = documentCamera.fieldOfView;
        float originalNear = documentCamera.nearClipPlane, originalFar = documentCamera.farClipPlane;
        var originalClear = documentCamera.clearFlags;
        var originalBackground = documentCamera.backgroundColor;
        var pipeline = (UniversalRenderPipelineAsset)QualitySettings.renderPipeline;
        int originalSamples = pipeline.msaaSampleCount;
        int originalTargetSamples = target.antiAliasing;
        try
        {
            documentCamera.enabled = false;
            optIn.enabled = true;
            documentCamera.orthographic = false;
            documentCamera.fieldOfView = 60;
            documentCamera.clearFlags = CameraClearFlags.SolidColor;
            documentCamera.backgroundColor = Color.black;
            pipeline.msaaSampleCount = 1;
            target.Release(); target.antiAliasing = 1;
            Require(target.Create(), "Could not create the content fixture target.");
            foreach (string name in new[] { "paint-segment", "paint-circle", "paint-ellipse", "paint-square",
                "picture-flat", "picture-equirect-mono", "picture-equirect-stereo", "picture-cube-cross", "picture-cube-strip" })
            {
                string path = Path.Combine(Application.streamingAssetsPath, "urp-content", $"{name}.imm");
#if UNITY_ANDROID && !UNITY_EDITOR
                using (var download = UnityWebRequest.Get(path))
                {
                    yield return download.SendWebRequest();
                    Require(download.result == UnityWebRequest.Result.Success,
                        $"Could not extract content fixture {name}: {download.error}");
                    string directory = Path.Combine(Application.persistentDataPath, "urp-content");
                    Directory.CreateDirectory(directory);
                    path = Path.Combine(directory, $"{name}.imm");
                    File.WriteAllBytes(path, download.downloadHandler.data);
                }
#endif
                Require(File.Exists(path), $"Content fixture is missing: {name}.");
                var document = manager.LoadDocument(path);
                Require(document != null, $"Could not queue content fixture {name}.");
                int id = document.DocumentId;
                try
                {
                    float deadline = Time.realtimeSinceStartup + 30;
                    while (document.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded &&
                           Time.realtimeSinceStartup < deadline)
                    {
                        yield return null;
                        RenderPipeline.SubmitRenderRequest(documentCamera, request);
                    }
                    Require(document.GetStateInfo().Loading == ImmDocument.LoadingState.Loaded,
                        $"Content fixture loading timed out: {name}.");
                    bool surface = name.StartsWith("paint-", StringComparison.Ordinal) || name == "picture-flat";
                    var bounds = surface ? document.GetBoundingBox() : new Bounds(Vector3.zero, Vector3.one * 2);
                    float radius = Mathf.Max(bounds.extents.magnitude, 0.1f);
                    Require(!float.IsNaN(radius) && !float.IsInfinity(radius), $"Content fixture bounds are invalid: {name}.");
                    documentCamera.transform.position = surface ? bounds.center + Vector3.back * radius * 3 : Vector3.back * 3;
                    documentCamera.transform.LookAt(surface ? bounds.center : Vector3.zero);
                    documentCamera.nearClipPlane = 0.01f;
                    documentCamera.farClipPlane = Mathf.Max(radius * 8, 100);
                    document.SetTime(0, 0);
                    document.Show();
                    for (int frame = 0; frame < 3; ++frame)
                    {
                        yield return null;
                        RenderPipeline.SubmitRenderRequest(documentCamera, request);
                    }
                    int redPixels = 0;
                    foreach (var pixel in ReadTargetPixels(target, $"content-{name}"))
                        if (pixel.r > pixel.g + 10 && pixel.r > pixel.b + 10) ++redPixels;
                    Require(redPixels > 100, $"Content fixture has no expected red geometry: {name}, pixels={redPixels}.");
                    if (surface)
                    {
                        var depth = VerifyDepthComposition(1, bounds);
                        try { while (depth.MoveNext()) yield return depth.Current; }
                        finally { (depth as IDisposable)?.Dispose(); }
                    }
                    Require(renderError == null, $"Unity reported for {name}: {renderError}");
                    Debug.Log($"[IMM_URP_CONTENT] PASS {name} loaded document, mono colour and {(surface ? "bidirectional depth" : "backdrop")}.");
                }
                finally { manager.UnloadDocument(document); }
                float unloadDeadline = Time.realtimeSinceStartup + 30;
                while (ImmNativePlugin.IsDocumentActive(id) && Time.realtimeSinceStartup < unloadDeadline)
                    yield return null;
                Require(!ImmNativePlugin.IsDocumentActive(id), $"Content fixture unload stalled: {name}.");
            }
            Debug.Log("[IMM_URP_CONTENT] PASS nine paint and picture documents through Unity RenderGraph.");
        }
        finally
        {
            documentCamera.transform.SetPositionAndRotation(originalPosition, originalRotation);
            documentCamera.enabled = originalEnabled;
            optIn.enabled = originalOptIn;
            documentCamera.orthographic = originalOrthographic;
            documentCamera.fieldOfView = originalFov;
            documentCamera.nearClipPlane = originalNear; documentCamera.farClipPlane = originalFar;
            documentCamera.clearFlags = originalClear; documentCamera.backgroundColor = originalBackground;
            pipeline.msaaSampleCount = originalSamples;
            target.Release(); target.antiAliasing = originalTargetSamples; target.Create();
        }
    }

    private IEnumerator VerifyDepthComposition(int samples, Bounds bounds)
    {
        var shader = Resources.Load<Shader>("ImmUrpDepthProbe");
        Require(shader != null && shader.isSupported, "URP depth probe shader is unavailable.");
        var material = new Material(shader);
        // CreatePrimitive adds a collider, which stripped iOS builds need not retain.
        // The depth probe only needs a rendered quad.
        var mesh = new Mesh
        {
            vertices = new[]
            {
                new Vector3(-0.5f, -0.5f, 0), new Vector3(0.5f, -0.5f, 0),
                new Vector3(-0.5f, 0.5f, 0), new Vector3(0.5f, 0.5f, 0)
            },
            triangles = new[] { 0, 2, 1, 2, 3, 1 }
        };
        mesh.RecalculateBounds();
        var quad = new GameObject("IMM URP CI depth probe", typeof(MeshFilter), typeof(MeshRenderer));
        quad.GetComponent<MeshFilter>().sharedMesh = mesh;
        quad.GetComponent<MeshRenderer>().sharedMaterial = material;
        float radius = Mathf.Max(bounds.extents.magnitude, 0.1f);
        quad.transform.localScale = Vector3.one * radius * 4;
        try
        {
            for (int phase = 0; phase < 4; ++phase)
            {
                bool transparent = phase >= 2;
                bool near = phase == 0 || phase == 3;
                material.renderQueue = (int)(transparent ? RenderQueue.Transparent : RenderQueue.Geometry);
                material.SetFloat("_ZWrite", transparent ? 0 : 1);
                quad.transform.position = bounds.center + (near ? Vector3.back : Vector3.forward) * radius * 2;
                for (int frame = 0; frame < 3; ++frame)
                {
                    yield return null;
                    RenderPipeline.SubmitRenderRequest(documentCamera, request);
                }
                Require(renderError == null, $"Unity reported: {renderError}");
                int visible = ReadVisiblePixels(false);
                Require(near ? visible == 0 : visible > 100,
                    $"Depth composition failed: samples={samples} transparent={transparent} near={near} visible={visible}.");
            }
            Debug.Log($"[IMM_URP_SMOKE] PASS bidirectional depth composition at {samples} samples.");
        }
        finally
        {
            quad.SetActive(false);
            Destroy(quad);
            Destroy(mesh);
            Destroy(material);
        }
    }

    private IEnumerator VerifyManagedAllocations()
    {
        Require(ImmRenderingDiagnostics.CalibrateManagedAllocationMeasurement(),
            "Unity GC.Alloc recorder failed calibration.");
        // Warm the RenderGraph pools and bounded in-flight transport slots first.
        for (int frame = 0; frame < 16; ++frame)
        {
            yield return null;
            RenderPipeline.SubmitRenderRequest(documentCamera, request);
        }
        Array.Clear(allocationSamples, 0, allocationSamples.Length);
        Array.Clear(allocatingCallbacks, 0, allocatingCallbacks.Length);
        ImmRenderingDiagnostics.ManagedAllocationMeasured += OnManagedAllocation;
        try
        {
            for (int frame = 0; frame < 32; ++frame)
            {
                yield return null;
                RenderPipeline.SubmitRenderRequest(documentCamera, request);
            }
        }
        finally { ImmRenderingDiagnostics.ManagedAllocationMeasured -= OnManagedAllocation; }
        for (int callback = 0; callback < allocationSamples.Length; ++callback)
            Require(allocationSamples[callback] >= 32 && allocatingCallbacks[callback] == 0,
                $"Managed allocation check failed: callback={(ImmRenderCallback)callback} samples={allocationSamples[callback]} allocatingCallbacks={allocatingCallbacks[callback]}.");
        Debug.Log("[IMM_URP_SMOKE] PASS zero managed allocations in warmed IMM pass callbacks (32 frames).");
    }

    private void OnManagedAllocation(ImmManagedAllocation measurement)
    {
        int index = (int)measurement.Callback;
        ++allocationSamples[index];
        if (measurement.AllocationDetected) ++allocatingCallbacks[index];
    }

    private IEnumerator VerifySubmissionAttribution()
    {
        var descriptor = target.descriptor;
        descriptor.msaaSamples = 1;
        var otherTarget = new RenderTexture(descriptor);
        var cameraObject = new GameObject("IMM URP attributed camera", typeof(Camera), typeof(ImmCamera));
        attributionCamera = cameraObject.GetComponent<Camera>();
        attributionCamera.CopyFrom(documentCamera);
        attributionCamera.enabled = false;
        attributionCamera.transform.SetPositionAndRotation(documentCamera.transform.position +
            documentCamera.transform.right * 0.25f, documentCamera.transform.rotation);
        attributionCamera.targetTexture = otherTarget;
        attributionCamera.GetUniversalAdditionalCameraData();
        var otherRequest = new UniversalRenderPipeline.SingleCameraRequest { destination = otherTarget };
        var optIn = documentCamera.GetComponent<ImmCamera>();
        bool previousOptIn = optIn.enabled;
        bool previousCameraEnabled = documentCamera.enabled;
        // Both cameras are driven by explicit requests in this fixture. Leaving
        // the main camera enabled also schedules an automatic URP render on
        // players with an active display, producing two valid submissions.
        documentCamera.enabled = false;
        try
        {
            Require(otherTarget.Create(), "Could not create attributed camera target.");
            for (int phase = 0; phase < 2; ++phase)
            {
                optIn.enabled = phase == 0;
                ImmRenderingDiagnostics.PollCompletions();
                sceneSubmissions.Clear();
                submissionOverflow = false;
                captureSubmissions = true;
                int firstFrame = -1, lastFrame = -1;
                for (int frame = 0; frame < 4; ++frame)
                {
                    yield return null;
                    if (firstFrame < 0) firstFrame = Time.frameCount;
                    lastFrame = Time.frameCount;
                    RenderPipeline.SubmitRenderRequest(documentCamera, request);
                    RenderPipeline.SubmitRenderRequest(attributionCamera, otherRequest);
                }
                // Readback drains submitted GPU work. Idle cameras need not invoke
                // another pass, so explicitly poll acknowledgements afterwards.
                var mainPixels = ReadTargetPixels(target, $"attributed-main-{phase}");
                var otherPixels = ReadTargetPixels(otherTarget, $"attributed-other-{phase}");
                ImmRenderingDiagnostics.PollCompletions();
                captureSubmissions = false;
                Require(!submissionOverflow, "Submission attribution fixture overflowed.");
                int mainVisible = 0, otherVisible = 0, changed = 0;
                for (int pixel = 0; pixel < otherPixels.Length; ++pixel)
                {
                    if (mainPixels[pixel].r > 8 || mainPixels[pixel].g > 8 || mainPixels[pixel].b > 8) ++mainVisible;
                    if (otherPixels[pixel].r > 8 || otherPixels[pixel].g > 8 || otherPixels[pixel].b > 8) ++otherVisible;
                    if (ColorDistance(mainPixels[pixel], otherPixels[pixel]) > 15) ++changed;
                }
                Require(otherVisible > 100 && changed > 100, "Attributed camera views are missing or indistinguishable.");
                Require(phase == 0 ? mainVisible > 100 : mainVisible == 0,
                    $"Attributed main camera visibility differs: optIn={phase == 0} visible={mainVisible}.");
                for (int frame = firstFrame; frame <= lastFrame; ++frame)
                {
                    int mainEvents = CountSceneEvents(documentCamera, frame);
                    int otherEvents = CountSceneEvents(attributionCamera, frame);
                    Require(mainEvents == (phase == 0 ? 1 : 0),
                        $"Main camera submission count differs at frame {frame}, optIn={phase == 0}, actual={mainEvents}, observations={sceneSubmissions.Count}.");
                    Require(otherEvents == 1,
                        $"Second camera submission count differs at frame {frame}, actual={otherEvents}, observations={sceneSubmissions.Count}.");
                }
            }
            Debug.Log("[IMM_URP_SMOKE] PASS attributed native events for two cameras and camera opt-out.");
        }
        finally
        {
            captureSubmissions = false;
            optIn.enabled = previousOptIn;
            documentCamera.enabled = previousCameraEnabled;
            attributionCamera.targetTexture = null;
            otherTarget.Release();
            Destroy(otherTarget);
            Destroy(cameraObject);
            attributionCamera = null;
        }
    }

    private int CountSceneEvents(Camera camera, int frame)
    {
        int count = 0;
        foreach (var submission in sceneSubmissions)
        {
            if (!ReferenceEquals(submission.Camera, camera) || submission.FrameIndex != frame) continue;
            Require(submission.Result == 0 && submission.NativeSceneEvents == 1 &&
                submission.ViewCount == 1 && submission.XrPassIndex == -1,
                $"Invalid native scene acknowledgement: camera={submission.NativeCameraId} frame={frame} result={submission.Result} events={submission.NativeSceneEvents}.");
            count += submission.NativeSceneEvents;
        }
        return count;
    }

    private void OnSubmission(ImmSceneSubmission submission)
    {
        if (!captureSubmissions || (!ReferenceEquals(submission.Camera, documentCamera) &&
            !ReferenceEquals(submission.Camera, attributionCamera))) return;
        if (sceneSubmissions.Count == sceneSubmissions.Capacity) { submissionOverflow = true; return; }
        sceneSubmissions.Add(submission);
    }

    private void VerifyQueuedCameras()
    {
        var originalPosition = documentCamera.transform.position;
        var originalTarget = documentCamera.targetTexture;
        var first = new RenderTexture(target.descriptor);
        var second = new RenderTexture(target.descriptor);
        var control = new RenderTexture(target.descriptor);
        try
        {
            Require(first.Create() && second.Create() && control.Create(), "Could not create queued camera targets.");
            documentCamera.targetTexture = first;
            RenderPipeline.SubmitRenderRequest(documentCamera, new UniversalRenderPipeline.SingleCameraRequest { destination = first });
            documentCamera.transform.position += documentCamera.transform.right * 0.25f;
            documentCamera.targetTexture = second;
            RenderPipeline.SubmitRenderRequest(documentCamera, new UniversalRenderPipeline.SingleCameraRequest { destination = second });
            // No readback separates these two submissions. Earlier GPU draws must
            // retain the first camera's projection and uniforms.
            Color32[] firstPixels = ReadTargetPixels(first, "queued-first");
            Color32[] secondPixels = ReadTargetPixels(second, "queued-second");
            documentCamera.transform.position = originalPosition;
            documentCamera.targetTexture = control;
            RenderPipeline.SubmitRenderRequest(documentCamera, new UniversalRenderPipeline.SingleCameraRequest { destination = control });
            Color32[] controlPixels = ReadTargetPixels(control, "queued-control");
            int shifted = 0, mismatched = 0;
            for (int i = 0; i < firstPixels.Length; ++i)
            {
                if (ColorDistance(firstPixels[i], secondPixels[i]) > 15) ++shifted;
                if (ColorDistance(firstPixels[i], controlPixels[i]) > 15) ++mismatched;
            }
            Require(shifted > 100, $"Queued cameras did not produce distinct views: changed={shifted}.");
            Require(mismatched < 20, $"The earlier queued camera lost its own data: mismatched={mismatched}.");
            Debug.Log("[IMM_URP_SMOKE] PASS queued camera isolation.");
        }
        finally
        {
            documentCamera.transform.position = originalPosition;
            documentCamera.targetTexture = originalTarget;
            foreach (var texture in new[] { first, second, control }) { texture.Release(); Destroy(texture); }
        }
    }

    private static int ColorDistance(Color32 a, Color32 b)
    {
        return Mathf.Abs(a.r - b.r) + Mathf.Abs(a.g - b.g) + Mathf.Abs(a.b - b.b);
    }

    private static Color32[] ReadTargetPixels(RenderTexture texture, string label)
    {
        var previous = RenderTexture.active;
        var resolved = RenderTexture.GetTemporary(texture.width, texture.height, 0, RenderTextureFormat.ARGB32);
        var pixels = new Texture2D(texture.width, texture.height, TextureFormat.RGBA32, false);
        try
        {
            Graphics.Blit(texture, resolved);
            RenderTexture.active = resolved;
            pixels.ReadPixels(new Rect(0, 0, texture.width, texture.height), 0, 0);
            pixels.Apply();
            string capture = Environment.GetEnvironmentVariable("IMM_UNITY_URP_CAPTURE");
            string path = Path.Combine(Path.GetDirectoryName(capture), $"{Path.GetFileNameWithoutExtension(capture)}-{label}.png");
            File.WriteAllBytes(path, pixels.EncodeToPNG());
            return pixels.GetPixels32();
        }
        finally
        {
            RenderTexture.active = previous;
            RenderTexture.ReleaseTemporary(resolved);
            Destroy(pixels);
        }
    }

    private int ReadVisiblePixels(bool capture, int samples = 0)
    {
        var previous = RenderTexture.active;
        var resolved = RenderTexture.GetTemporary(target.width, target.height, 0, RenderTextureFormat.ARGB32);
        var pixels = new Texture2D(target.width, target.height, TextureFormat.RGBA32, false);
        try
        {
            Graphics.Blit(target, resolved);
            RenderTexture.active = resolved;
            pixels.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0);
            pixels.Apply();
            if (capture)
            {
                string path = Environment.GetEnvironmentVariable("IMM_UNITY_URP_CAPTURE");
                if (samples > 0 && !string.IsNullOrEmpty(path))
                    path = Path.Combine(Path.GetDirectoryName(path), $"{Path.GetFileNameWithoutExtension(path)}-{samples}x{Path.GetExtension(path)}");
                Require(!string.IsNullOrEmpty(path), "CI capture path is missing.");
                Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path)));
                File.WriteAllBytes(path, pixels.EncodeToPNG());
            }
            int visible = 0;
            foreach (var color in pixels.GetPixels32())
                if (color.r > 5 || color.g > 5 || color.b > 5) ++visible;
            return visible;
        }
        finally
        {
            RenderTexture.active = previous;
            Destroy(pixels);
            RenderTexture.ReleaseTemporary(resolved);
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private void OnLog(string message, string stack, LogType type)
    {
        if (type == LogType.Error || type == LogType.Exception || type == LogType.Assert)
            renderError = message;
    }

    private void OnCamera(ScriptableRenderContext context, Camera camera)
    {
        if (camera == documentCamera) ++cameraFrames;
    }

    private void OnDestroy()
    {
        Application.logMessageReceived -= OnLog;
        RenderPipelineManager.beginCameraRendering -= OnCamera;
        ImmRenderingDiagnostics.SubmissionCompleted -= OnSubmission;
        ImmRenderingDiagnostics.ManagedAllocationMeasured -= OnManagedAllocation;
        if (documentCamera != null) documentCamera.targetTexture = null;
        if (target != null) { target.Release(); Destroy(target); }
    }
}
