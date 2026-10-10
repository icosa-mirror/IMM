using System;
using ImmPlayer;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering.Universal;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;

public static class ImmUrpSampleSetup
{
    public const string ScenePath = "Assets/Scenes/SampleSceneURP.unity";
    public const string PipelinePath = "Assets/Settings/ImmURP.asset";
    private const string RendererPath = "Assets/Settings/ImmURPRenderer.asset";

    [MenuItem("IMM/Save URP Sample Defaults")]
    public static void SaveSampleDefaults()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode)
            throw new InvalidOperationException("Stop Play before saving the sample defaults.");
        for (int i = 0; i < SceneManager.sceneCount; ++i)
            if (SceneManager.GetSceneAt(i).isDirty)
                throw new InvalidOperationException("Save your open scenes before saving the sample defaults.");
        var pipeline = AssetDatabase.LoadAssetAtPath<UniversalRenderPipelineAsset>(PipelinePath);
        if (pipeline == null) throw new InvalidOperationException("The saved IMM URP asset is missing.");
        GraphicsSettings.defaultRenderPipeline = pipeline;
        int quality = QualitySettings.GetQualityLevel();
        try
        {
            for (int i = 0; i < QualitySettings.names.Length; ++i)
            {
                QualitySettings.SetQualityLevel(i, false);
                QualitySettings.renderPipeline = null; // Inherit the saved project default.
            }
        }
        finally { QualitySettings.SetQualityLevel(quality, false); }
        PlayerSettings.SetUseDefaultGraphicsAPIs(UnityEditor.BuildTarget.StandaloneWindows64, false);
        PlayerSettings.SetGraphicsAPIs(UnityEditor.BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Direct3D12 });
        var setup = EditorSceneManager.GetSceneManagerSetup();
        try
        {
            foreach (string path in new[] { "Assets/Scenes/SampleScene.unity", ScenePath, "Assets/Scenes/SampleSceneVR.unity" })
            {
                var scene = EditorSceneManager.OpenScene(path, OpenSceneMode.Single);
                foreach (var root in scene.GetRootGameObjects())
                {
                    foreach (var camera in root.GetComponentsInChildren<Camera>(true))
                        if (camera.CompareTag("MainCamera"))
                        {
                            if (!camera.TryGetComponent<ImmCamera>(out _)) camera.gameObject.AddComponent<ImmCamera>();
                            camera.GetUniversalAdditionalCameraData();
                        }
                    foreach (var wrapper in root.GetComponentsInChildren<ImmUrpSample>(true))
                        if (!wrapper.TryGetComponent<ImmPlayerExample>(out _))
                            wrapper.gameObject.AddComponent<ImmPlayerExample>();
                    foreach (var player in root.GetComponentsInChildren<ImmPlayerExample>(true))
                    {
                        var serialized = new SerializedObject(player);
                        var cameraProperty = serialized.FindProperty("targetCamera");
                        var camera = cameraProperty.objectReferenceValue as Camera;
                        if (camera == null) camera = Camera.main;
                        if (camera == null) throw new InvalidOperationException($"No document camera in {path}.");
                        cameraProperty.objectReferenceValue = camera;
                        if (!camera.TryGetComponent<ImmCamera>(out _)) camera.gameObject.AddComponent<ImmCamera>();
                        camera.GetUniversalAdditionalCameraData();
                        if (player.GetComponent<ImmUrpSample>() != null)
                        {
                            serialized.FindProperty("documentPath").stringValue = "sample1.imm";
                            serialized.FindProperty("loadOnStart").boolValue = true;
                            serialized.FindProperty("autoPlay").boolValue = true;
                        }
                        serialized.ApplyModifiedPropertiesWithoutUndo();
                    }
                }
                EditorSceneManager.MarkSceneDirty(scene);
                if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException($"Could not save {path}.");
            }
        }
        finally { EditorSceneManager.RestoreSceneManagerSetup(setup); }
        AssetDatabase.SaveAssets();
        Debug.Log("[IMM_SAMPLE_DEFAULTS] Saved URP, Windows D3D12, opted-in cameras and shared sample players.");
    }

    // Intended for initial creation. Refuse to replace existing sample assets.
    [MenuItem("IMM/Create RenderGraph Sample")]
    public static void CreateSample()
    {
        if (AssetDatabase.LoadMainAssetAtPath(ScenePath) != null ||
            AssetDatabase.LoadMainAssetAtPath(PipelinePath) != null ||
            AssetDatabase.LoadMainAssetAtPath(RendererPath) != null)
            throw new InvalidOperationException("The IMM URP sample already exists.");
        if (!EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo()) return;
        if (!AssetDatabase.IsValidFolder("Assets/Settings"))
            AssetDatabase.CreateFolder("Assets", "Settings");
        var renderer = ScriptableObject.CreateInstance<UniversalRendererData>();
        AssetDatabase.CreateAsset(renderer, RendererPath);
        var feature = ScriptableObject.CreateInstance<ImmRendererFeature>();
        feature.name = "IMM RenderGraph";
        AssetDatabase.AddObjectToAsset(feature, renderer);
        renderer.rendererFeatures.Add(feature);
        renderer.SetDirty();
        EditorUtility.SetDirty(renderer);
        var pipeline = UniversalRenderPipelineAsset.Create(renderer);
        pipeline.msaaSampleCount = 8;
        pipeline.supportsHDR = false;
        AssetDatabase.CreateAsset(pipeline, PipelinePath);
        AssetDatabase.SaveAssets();
        var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        // Opening a scene can unload assets that have no scene references yet.
        pipeline = AssetDatabase.LoadAssetAtPath<UniversalRenderPipelineAsset>(PipelinePath);
        var cameraObject = new GameObject("Main Camera", typeof(Camera), typeof(ImmCamera));
        cameraObject.tag = "MainCamera";
        var camera = cameraObject.GetComponent<Camera>();
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = Color.black;
        camera.allowHDR = false;
        camera.GetUniversalAdditionalCameraData();
        var sample = new GameObject("IMM URP Sample").AddComponent<ImmUrpSample>();
        var serialized = new SerializedObject(sample.GetComponent<ImmPlayerExample>());
        serialized.FindProperty("targetCamera").objectReferenceValue = camera;
        serialized.FindProperty("documentPath").stringValue = "sample1.imm";
        serialized.FindProperty("loadOnStart").boolValue = true;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        GraphicsSettings.defaultRenderPipeline = pipeline;
        AssetDatabase.SaveAssets();
        if (!EditorSceneManager.SaveScene(scene, ScenePath))
            throw new InvalidOperationException("Could not save the IMM URP sample scene.");
        Debug.Log($"[IMM_URP_SAMPLE_SETUP] Created {ScenePath} with its renderer feature and pipeline.");
    }
}
