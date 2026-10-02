using System;
using ImmPlayer;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering.Universal;

public static class ImmUrpSampleSetup
{
    public const string ScenePath = "Assets/Scenes/SampleSceneURP.unity";
    public const string PipelinePath = "Assets/Settings/ImmURP.asset";
    private const string RendererPath = "Assets/Settings/ImmURPRenderer.asset";

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
        var serialized = new SerializedObject(sample);
        serialized.FindProperty("pipeline").objectReferenceValue = pipeline;
        serialized.FindProperty("documentCamera").objectReferenceValue = camera;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        if (serialized.FindProperty("pipeline").objectReferenceValue == null)
            throw new InvalidOperationException("Could not assign the sample pipeline asset.");
        AssetDatabase.SaveAssets();
        if (!EditorSceneManager.SaveScene(scene, ScenePath))
            throw new InvalidOperationException("Could not save the IMM URP sample scene.");
        Debug.Log($"[IMM_URP_SAMPLE_SETUP] Created {ScenePath} with its renderer feature and pipeline.");
    }
}
