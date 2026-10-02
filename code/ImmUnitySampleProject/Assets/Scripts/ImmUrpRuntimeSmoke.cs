using System;
using System.Collections;
using System.IO;
using ImmPlayer;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

/// <summary>Opt-in CI probe for the configured URP scene.</summary>
public sealed class ImmUrpRuntimeSmoke : MonoBehaviour
{
    private RenderTexture target;
    private Camera documentCamera;
    private string renderError;
    private int cameraFrames;
    private UniversalRenderPipeline.SingleCameraRequest request;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Install()
    {
        if (Environment.GetEnvironmentVariable("IMM_UNITY_URP_SMOKE") == "1")
            new GameObject("IMM URP CI probe").AddComponent<ImmUrpRuntimeSmoke>();
    }

    private IEnumerator Start()
    {
        Application.logMessageReceived += OnLog;
        Application.runInBackground = true;
        RenderPipelineManager.beginCameraRendering += OnCamera;
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
        Application.Quit(0);
    }

    private IEnumerator Run()
    {
        bool metal = SystemInfo.graphicsDeviceType == GraphicsDeviceType.Metal;
        Require(metal || SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D12, "D3D12 or Metal is required.");
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
            if (metal && supportedSamples != samples)
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
        optIn.enabled = false;
        for (int frame = 0; frame < 3; ++frame) yield return null;
        RenderPipeline.SubmitRenderRequest(documentCamera, request);
        Require(ReadVisiblePixels(false) == 0, "IMM content remained after camera opt-out.");
        var manager = FindFirstObjectByType<ImmPlayerManager>();
        Require(manager != null && manager.IsInitialized, "Native session was not initialized.");
        Require(sample.Document != null, "Sample document is missing.");
        if (!metal)
        {
            var stereoProbe = ImmRenderGraphValidation.VerifyStereoPacket(documentCamera,
                Mathf.Max(sample.Document.GetBoundingBox().extents.magnitude, 0.1f));
            // Advance explicitly so the outer runner catches validation failures.
            try
            {
                while (stereoProbe.MoveNext()) yield return stereoProbe.Current;
            }
            finally { (stereoProbe as IDisposable)?.Dispose(); }
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
        var loadingDocument = manager.LoadDocument(Path.Combine(Application.streamingAssetsPath, "sample1.imm"));
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
        Destroy(sample.gameObject);
        for (int frame = 0; frame < 3; ++frame) yield return null;
        Require(manager == null, "The sample left its persistent manager alive after destruction.");
        Require(renderError == null, $"Unity reported: {renderError}");
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
        if (documentCamera != null) documentCamera.targetTexture = null;
        if (target != null) { target.Release(); Destroy(target); }
    }
}
