using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace ImmPlayer.EditorTools
{
    /// <summary>
    /// Creates the stroke reader sample scene, so the scene can be regenerated instead of
    /// being hand-edited, and so it can be produced from a batch-mode Unity run:
    ///
    /// <code>
    /// Unity.exe -batchmode -nographics -quit -projectPath code/ImmUnitySampleProject \
    ///   -executeMethod ImmPlayer.EditorTools.StrokeReaderSampleSceneBuilder.CreateSampleScene
    /// </code>
    /// </summary>
    public static class StrokeReaderSampleSceneBuilder
    {
        private const string ScenePath = "Assets/Scenes/StrokeReaderSampleScene.unity";
        private const string SceneName = "StrokeReaderSampleScene";

        [MenuItem("IMM/Create Stroke Reader Sample Scene")]
        public static void CreateSampleScene()
        {
            Scene scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

            // Camera looking at the origin, with the report overlay drawn on top of it.
            var cameraObject = new GameObject("Main Camera");
            cameraObject.tag = "MainCamera";
            Camera camera = cameraObject.AddComponent<Camera>();
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.08f, 0.09f, 0.11f, 1f);
            cameraObject.transform.position = new Vector3(0f, 1.6f, -3f);
            cameraObject.transform.rotation = Quaternion.identity;
            cameraObject.AddComponent<AudioListener>();

            var lightObject = new GameObject("Directional Light");
            Light light = lightObject.AddComponent<Light>();
            light.type = LightType.Directional;
            light.intensity = 1.0f;
            lightObject.transform.rotation = Quaternion.Euler(50f, -30f, 0f);

            var readerObject = new GameObject("Stroke Reader Example");
            readerObject.AddComponent<ImmStrokeReaderExample>();

            Directory.CreateDirectory(Path.GetDirectoryName(ScenePath) ?? "Assets/Scenes");
            if (!EditorSceneManager.SaveScene(scene, ScenePath))
            {
                throw new IOException($"Could not save the stroke reader sample scene to {ScenePath}");
            }

            RegisterInBuildSettings();

            AssetDatabase.SaveAssets();
            Debug.Log($"[IMM_SR_SAMPLE] Created {ScenePath} (scene name '{SceneName}')");
        }

        private static void RegisterInBuildSettings()
        {
            string guid = AssetDatabase.AssetPathToGUID(ScenePath);
            var scenes = new List<EditorBuildSettingsScene>(EditorBuildSettings.scenes);
            scenes.RemoveAll(entry => entry.path == ScenePath);

            // Disabled on purpose: the CI players build the enabled scene list, and this
            // scene is a manual/reference sample rather than a smoke target.
            scenes.Add(new EditorBuildSettingsScene(ScenePath, false));
            EditorBuildSettings.scenes = scenes.ToArray();
            Debug.Log($"[IMM_SR_SAMPLE] Registered {ScenePath} (guid {guid}) in the build settings as disabled");
        }
    }
}
