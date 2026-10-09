using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEditor.XR.Management;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;
using UnityEngine.SpatialTracking;
using UnityEngine.XR.OpenXR;
using UnityEngine.XR.OpenXR.Features.MetaQuestSupport;

namespace ImmPlayer.Editor
{
    // Kept out of the dedicated flat CI copies, which omit OpenXR packages.
    public sealed class ImmUrpXrBuild : IProcessSceneWithReport
    {
        public int callbackOrder => 0;

        public static void BuildAndroidOpenXRQuestPlayer() => Build(BuildTarget.Android, GraphicsDeviceType.Vulkan,
            "-immQuestPlayerPath", "../build/unity-smoke/android-openxr-quest-player/ImmUnityQuestVulkan.apk");

        public static void BuildWindowsOpenXRPlayer() => Build(BuildTarget.StandaloneWindows64, GraphicsDeviceType.Direct3D12,
            "-immUrpXrPlayerPath", "../build/unity-smoke/windows-openxr-urp/ImmUnityOpenXR.exe");

        public static void BuildWindowsDirectXAndOpenXRPlayers()
        {
            BuildAutomation.BuildWindowsDirectXSmokePlayer();
            BuildWindowsOpenXRPlayer();
        }

        private static void Build(BuildTarget target, GraphicsDeviceType api, string argument, string fallback)
        {
            var group = BuildPipeline.GetBuildTargetGroup(target);
            var general = XRGeneralSettingsPerBuildTarget.XRGeneralSettingsForBuildTarget(group);
            var manager = general != null ? general.AssignedSettings : null;
            if (manager == null || manager.activeLoaders.Count != 1 || !(manager.activeLoaders[0] is OpenXRLoader))
                throw new InvalidOperationException("The URP XR sample requires exactly one configured OpenXR loader.");
            var settings = OpenXRSettings.GetSettingsForBuildTargetGroup(group);
            if (settings == null) throw new InvalidOperationException("OpenXR settings are missing.");
            var quest = target == BuildTarget.Android ? settings.GetFeature<MetaQuestFeature>() : null;
            if (target == BuildTarget.Android && quest == null)
                throw new InvalidOperationException("The Quest OpenXR feature is missing.");
            var previousMode = settings.renderMode;
            bool previousQuest = quest != null && quest.enabled;
            var previousArchitectures = PlayerSettings.Android.targetArchitectures;
            try
            {
                settings.renderMode = OpenXRSettings.RenderMode.SinglePassInstanced;
                EditorUtility.SetDirty(settings);
                if (quest != null) { quest.enabled = true; EditorUtility.SetDirty(quest); }
                if (target == BuildTarget.Android) PlayerSettings.Android.targetArchitectures = AndroidArchitecture.ARM64;
                var arguments = Environment.GetCommandLineArgs();
                int index = Array.IndexOf(arguments, argument);
                string output = index >= 0 && index + 1 < arguments.Length ? arguments[index + 1] : fallback;
                AssetDatabase.SaveAssets();
                Debug.Log($"[IMM_URP_XR_BUILD] api={api} scene={ImmUrpSampleSetup.ScenePath} stereo=SinglePassInstanced xrStartup=scene");
                BuildAutomation.BuildUrpPlayer(target, api, Path.GetFullPath(output), useXr: true);
            }
            finally
            {
                settings.renderMode = previousMode;
                EditorUtility.SetDirty(settings);
                if (quest != null) { quest.enabled = previousQuest; EditorUtility.SetDirty(quest); }
                if (target == BuildTarget.Android) PlayerSettings.Android.targetArchitectures = previousArchitectures;
                AssetDatabase.SaveAssets();
            }
        }

        public void OnProcessScene(Scene scene, BuildReport report)
        {
            if (!BuildAutomation.BuildingUrpXrSample) return;
            var sample = scene.GetRootGameObjects().SelectMany(root => root.GetComponentsInChildren<ImmUrpSample>(true)).Single();
            var camera = sample.DocumentCamera;
            if (camera == null) throw new InvalidOperationException("The URP XR sample camera is missing.");
            var origin = new GameObject("IMM OpenXR Origin");
            SceneManager.MoveGameObjectToScene(origin, scene);
            camera.transform.SetParent(origin.transform, false);
            camera.transform.localPosition = Vector3.zero;
            camera.transform.localRotation = Quaternion.identity;
            var tracker = camera.gameObject.AddComponent<TrackedPoseDriver>();
            if (!tracker.SetPoseSource(TrackedPoseDriver.DeviceType.GenericXRDevice, TrackedPoseDriver.TrackedPose.Center))
                throw new InvalidOperationException("Could not configure the OpenXR head pose source.");
            tracker.trackingType = TrackedPoseDriver.TrackingType.RotationAndPosition;
            tracker.updateType = TrackedPoseDriver.UpdateType.UpdateAndBeforeRender;
            tracker.UseRelativeTransform = false;
            sample.SetViewingOrigin(origin.transform);
            origin.AddComponent<XrSceneBootstrap>();
            sample.gameObject.AddComponent<ImmUrpXrRuntimeSmoke>();
            BuildAutomation.UrpXrSceneProcessed = true;
            Debug.Log("[IMM_URP_XR_BUILD] trackedRig=CenterEye viewingOrigin=separate startup=scene");
        }
    }
}
