using System.Collections;
using System.IO;
using ImmPlayer;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

/// <summary>Minimal document setup for the configured RenderGraph sample scene.</summary>
public sealed class ImmUrpSample : MonoBehaviour
{
    [SerializeField] private UniversalRenderPipelineAsset pipeline;
    [SerializeField] private Camera documentCamera;
    private RenderPipelineAsset previousPipeline;
    private ImmPlayerManager manager;
    private bool pipelineAssigned;
    public ImmDocument Document { get; private set; }
    public string DocumentPath { get; private set; }

    private IEnumerator Start()
    {
        if (pipeline == null || documentCamera == null ||
            documentCamera.GetComponent<ImmCamera>() == null)
        {
            Debug.LogError("[IMM_URP_SAMPLE] Assign the URP asset and an ImmCamera camera.");
            yield break;
        }
        previousPipeline = QualitySettings.renderPipeline;
        QualitySettings.renderPipeline = pipeline;
        pipelineAssigned = true;
        // The manager marks its GameObject DontDestroyOnLoad. Keep the sample
        // controller in this scene so unloading it still restores the pipeline.
        manager = new GameObject("IMM URP Sample Manager").AddComponent<ImmPlayerManager>();
        if (!manager.IsInitialized)
        {
            Debug.LogError("[IMM_URP_SAMPLE] Native session initialization failed.");
            yield break;
        }
        DocumentPath = Path.Combine(Application.streamingAssetsPath, "sample1.imm");
#if UNITY_ANDROID && !UNITY_EDITOR
        // Android StreamingAssets live inside the APK, beyond native file access.
        using (var download = UnityWebRequest.Get(DocumentPath))
        {
            yield return download.SendWebRequest();
            if (download.result != UnityWebRequest.Result.Success)
            {
                Debug.LogError($"[IMM_URP_SAMPLE] Could not extract sample1.imm: {download.error}");
                yield break;
            }
            DocumentPath = Path.Combine(Application.persistentDataPath, "sample1.imm");
            File.WriteAllBytes(DocumentPath, download.downloadHandler.data);
        }
#endif
        var document = Document = manager.LoadDocument(DocumentPath);
        if (document == null)
        {
            Debug.LogError("[IMM_URP_SAMPLE] Could not load sample1.imm.");
            yield break;
        }
        float deadline = Time.realtimeSinceStartup + 30;
        while (document.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded &&
               Time.realtimeSinceStartup < deadline)
            yield return null;
        if (document.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded)
        {
            Debug.LogError("[IMM_URP_SAMPLE] Document loading timed out.");
            yield break;
        }
        var bounds = document.GetBoundingBox();
        float radius = Mathf.Max(bounds.extents.magnitude, 0.1f);
        documentCamera.nearClipPlane = radius * 0.01f;
        documentCamera.farClipPlane = radius * 8;
        documentCamera.transform.position = bounds.center + Vector3.back * radius * 3;
        documentCamera.transform.LookAt(bounds.center);
        document.SetTime(3 * 12600, 0);
        document.Show();
        Debug.Log("[IMM_URP_SAMPLE] Document ready on the configured RenderGraph camera.");
    }

    private void OnDestroy()
    {
        if (manager != null)
        {
            manager.Shutdown();
            Destroy(manager.gameObject);
        }
        if (pipelineAssigned) QualitySettings.renderPipeline = previousPipeline;
    }
}
