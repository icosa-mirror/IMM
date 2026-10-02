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
        Require(SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D12, "D3D12 is required.");
        var sample = FindFirstObjectByType<ImmUrpSample>();
        Require(sample != null, "Configured URP sample is missing.");
        var optIn = FindFirstObjectByType<ImmCamera>();
        Require(optIn != null, "Opted-in camera is missing.");
        documentCamera = optIn.GetComponent<Camera>();
        target = new RenderTexture(new RenderTextureDescriptor(256, 256)
        {
            graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
            depthStencilFormat = GraphicsFormat.D32_SFloat,
            msaaSamples = 8
        });
        target.Create();
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
