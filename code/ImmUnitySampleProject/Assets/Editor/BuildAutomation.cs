using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine.Rendering;

namespace ImmPlayer.Editor
{
    public static class BuildAutomation
    {
        private static readonly string[] SmokeScenes =
        {
            "Assets/Scenes/SampleScene.unity"
        };
        private const string VrSmokeScene = "Assets/Scenes/SampleSceneVR.unity";
        private const string EditorSmokeCapturePathEnv = "IMM_UNITY_EDITOR_SMOKE_CAPTURE_PATH";
        private const string RuntimeSmokeCapturePathEnv = "IMM_UNITY_SMOKE_CAPTURE_PATH";
        private const string RuntimeSmokeFramesEnv = "IMM_UNITY_SMOKE_FRAMES";
        private const string RuntimeSmokeQuitEnv = "IMM_UNITY_SMOKE_QUIT";
        private const string RuntimeSmokeCompositionProbeEnv = "IMM_UNITY_SMOKE_COMPOSITION_PROBE";
        private const string RuntimeSmokeXrProbeEnv = "IMM_UNITY_SMOKE_XR_PROBE";
        private const string EditorSmokeActiveKey = "IMM_EDITOR_SMOKE_ACTIVE";
        private const string EditorSmokeCapturePathKey = "IMM_EDITOR_SMOKE_CAPTURE_PATH";
        private const string EditorSmokeNativeLogPathKey = "IMM_EDITOR_SMOKE_NATIVE_LOG_PATH";
        private const string EditorSmokeStartTicksKey = "IMM_EDITOR_SMOKE_START_TICKS";

        private static string s_EditorSmokeCapturePath;
        private static DateTime s_EditorSmokeStartTimeUtc;
        private static bool s_EditorSmokeRequestedExit;

        public static void BuildAndroidDebug()
        {
            string projectPath = Directory.GetCurrentDirectory();
            string outputDir = Path.Combine(projectPath, "Builds", "Android");
            Directory.CreateDirectory(outputDir);

            string outputApk = Path.Combine(outputDir, "IMMUnityTest.apk");
            string[] scenes = EditorBuildSettings.scenes
                .Where(s => s.enabled)
                .Select(s => s.path)
                .ToArray();

            if (scenes.Length == 0)
            {
                throw new InvalidOperationException("No enabled scenes in EditorBuildSettings.");
            }

            var options = new BuildPlayerOptions
            {
                scenes = scenes,
                locationPathName = outputApk,
                target = BuildTarget.Android,
                options = BuildOptions.Development | BuildOptions.AllowDebugging
            };

            BuildReport report = BuildPipeline.BuildPlayer(options);
            if (report.summary.result != BuildResult.Succeeded)
            {
                throw new InvalidOperationException($"Android build failed: {report.summary.result}");
            }

            UnityEngine.Debug.Log($"[IMM_AUTOBUILD] Android build succeeded: {outputApk}");
        }

        public static void BuildMacOSDevelopment()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneOSX, "macOS");

            string projectPath = Directory.GetCurrentDirectory();
            string outputDir = Path.Combine(projectPath, "Builds", "macOS");
            Directory.CreateDirectory(outputDir);

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneOSX, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneOSX, new[] { GraphicsDeviceType.Metal });

            string outputApp = Path.Combine(outputDir, "IMMUnityTest.app");
            BuildPlayer(BuildTarget.StandaloneOSX, outputApp, BuildOptions.Development, "macOS");
        }

        public static void BuildIOSDevelopment()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.iOS, BuildTarget.iOS, "iOS");

            string projectPath = Directory.GetCurrentDirectory();
            string outputDir = Path.Combine(projectPath, "Builds", "iOS");
            Directory.CreateDirectory(outputDir);

            PlayerSettings.iOS.sdkVersion = iOSSdkVersion.DeviceSDK;
            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.iOS, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.iOS, new[] { GraphicsDeviceType.Metal });

            BuildPlayer(BuildTarget.iOS, outputDir, BuildOptions.Development, "iOS");
        }

        public static void BuildIOSSimulatorDevelopment()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.iOS, BuildTarget.iOS, "iOS Simulator");

            string projectPath = Directory.GetCurrentDirectory();
            string outputDir = Path.Combine(projectPath, "Builds", "iOSSimulator");
            Directory.CreateDirectory(outputDir);

            PlayerSettings.iOS.sdkVersion = iOSSdkVersion.SimulatorSDK;
            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.iOS, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.iOS, new[] { GraphicsDeviceType.Metal });

            BuildPlayer(BuildTarget.iOS, outputDir, BuildOptions.Development, "iOS Simulator");
        }

        public static void RunMacOSEditorPlayModeSmoke()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneOSX, "macOS");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneOSX, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneOSX, new[] { GraphicsDeviceType.Metal });

            RunEditorPlayModeSmoke("macOS", SmokeScenes[0], Path.Combine("..", "build", "unity-smoke", "macos-editor-playmode.png"), false, false);
        }

        public static void RunWindowsDirectXEditorPlayModeSmoke()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64, "Windows");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Direct3D11 });

            RunEditorPlayModeSmoke("Windows DirectX", SmokeScenes[0], Path.Combine("..", "build", "unity-smoke", "windows-directx-editor-playmode.png"), true, false);
        }

        public static void RunWindowsOpenXREditorPlayModeSmoke()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64, "Windows");
            if (!File.Exists(VrSmokeScene))
            {
                throw new FileNotFoundException($"Required VR smoke scene is missing: {VrSmokeScene}", VrSmokeScene);
            }

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Direct3D11 });

            RunEditorPlayModeSmoke("Windows OpenXR VR", VrSmokeScene, Path.Combine("..", "build", "unity-smoke", "windows-openxr-vr-editor-playmode.png"), false, true);
        }

        private static void RunEditorPlayModeSmoke(string label, string scenePath, string defaultCapturePath, bool enableCompositionProbe, bool enableXrProbe)
        {
            string capturePath = Environment.GetEnvironmentVariable(EditorSmokeCapturePathEnv);
            if (string.IsNullOrEmpty(capturePath))
            {
                capturePath = Path.GetFullPath(defaultCapturePath);
            }
            else
            {
                capturePath = Path.GetFullPath(capturePath);
            }

            string captureDir = Path.GetDirectoryName(capturePath);
            if (!string.IsNullOrEmpty(captureDir))
            {
                Directory.CreateDirectory(captureDir);
            }

            if (File.Exists(capturePath))
            {
                File.Delete(capturePath);
            }

            Environment.SetEnvironmentVariable(RuntimeSmokeCapturePathEnv, capturePath);
            if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable(RuntimeSmokeFramesEnv)))
            {
                Environment.SetEnvironmentVariable(RuntimeSmokeFramesEnv, "360");
            }
            Environment.SetEnvironmentVariable(RuntimeSmokeQuitEnv, "0");
            Environment.SetEnvironmentVariable(RuntimeSmokeCompositionProbeEnv, enableCompositionProbe ? "1" : string.Empty);
            Environment.SetEnvironmentVariable(RuntimeSmokeXrProbeEnv, enableXrProbe ? "1" : string.Empty);

            string nativeLogPath = Path.GetFullPath("imm_player_log.txt");
            if (File.Exists(nativeLogPath))
            {
                File.Delete(nativeLogPath);
            }

            SessionState.SetBool(EditorSmokeActiveKey, true);
            SessionState.SetString(EditorSmokeCapturePathKey, capturePath);
            SessionState.SetString(EditorSmokeNativeLogPathKey, nativeLogPath);
            SessionState.SetString(EditorSmokeStartTicksKey, DateTime.UtcNow.Ticks.ToString());

            EditorSceneManager.OpenScene(scenePath);

            s_EditorSmokeCapturePath = capturePath;
            s_EditorSmokeStartTimeUtc = DateTime.UtcNow;
            s_EditorSmokeRequestedExit = false;

            EditorApplication.update -= UpdateEditorPlayModeSmoke;
            EditorApplication.update += UpdateEditorPlayModeSmoke;
            EditorApplication.playModeStateChanged -= OnEditorPlayModeSmokeStateChanged;
            EditorApplication.playModeStateChanged += OnEditorPlayModeSmokeStateChanged;

            UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] entering {label} play mode capture={capturePath}");
            EditorApplication.EnterPlaymode();
        }

        [InitializeOnLoadMethod]
        private static void ResumeEditorPlayModeSmokeAfterReload()
        {
            if (!SessionState.GetBool(EditorSmokeActiveKey, false))
                return;

            s_EditorSmokeCapturePath = SessionState.GetString(EditorSmokeCapturePathKey, string.Empty);
            string ticksText = SessionState.GetString(EditorSmokeStartTicksKey, string.Empty);
            if (!long.TryParse(ticksText, out long ticks))
            {
                ticks = DateTime.UtcNow.Ticks;
            }

            s_EditorSmokeStartTimeUtc = new DateTime(ticks, DateTimeKind.Utc);
            s_EditorSmokeRequestedExit = false;

            EditorApplication.update -= UpdateEditorPlayModeSmoke;
            EditorApplication.update += UpdateEditorPlayModeSmoke;
            EditorApplication.playModeStateChanged -= OnEditorPlayModeSmokeStateChanged;
            EditorApplication.playModeStateChanged += OnEditorPlayModeSmokeStateChanged;
        }

        private static void EnsureBuildTargetSupported(BuildTargetGroup group, BuildTarget target, string label)
        {
            if (!BuildPipeline.IsBuildTargetSupported(group, target))
            {
                throw new InvalidOperationException($"{label} build target is not supported by this Unity editor install. Install the matching platform Build Support module and rerun this build method.");
            }
        }

        private static void BuildPlayer(BuildTarget target, string outputPath, BuildOptions options, string label)
        {
            foreach (string scene in SmokeScenes)
            {
                if (!File.Exists(scene))
                {
                    throw new FileNotFoundException($"Required smoke scene is missing: {scene}", scene);
                }
            }

            var buildOptions = new BuildPlayerOptions
            {
                scenes = SmokeScenes,
                locationPathName = outputPath,
                target = target,
                options = options
            };

            BuildReport report = BuildPipeline.BuildPlayer(buildOptions);
            if (report.summary.result != BuildResult.Succeeded)
            {
                throw new InvalidOperationException($"{label} build failed: {report.summary.result}");
            }

            UnityEngine.Debug.Log($"[IMM_AUTOBUILD] {label} build succeeded: {outputPath}");
        }

        private static void UpdateEditorPlayModeSmoke()
        {
            if (s_EditorSmokeRequestedExit)
                return;

            string nativeLogPath = SessionState.GetString(EditorSmokeNativeLogPathKey, string.Empty);
            if (!string.IsNullOrEmpty(nativeLogPath) && File.Exists(nativeLogPath))
            {
                string nativeLog = File.ReadAllText(nativeLogPath);
                if (nativeLog.Contains("Loaded in SPU!") &&
                    nativeLog.Contains("Decoded Ogg Opus sound to PCM temp WAV for AVFoundation"))
                {
                    s_EditorSmokeRequestedExit = true;
                    UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] native load/audio complete: {nativeLogPath}");
                    EditorApplication.ExitPlaymode();
                    return;
                }
            }

            if (!string.IsNullOrEmpty(s_EditorSmokeCapturePath) && File.Exists(s_EditorSmokeCapturePath))
            {
                s_EditorSmokeRequestedExit = true;
                UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] capture complete: {s_EditorSmokeCapturePath}");
                EditorApplication.ExitPlaymode();
                return;
            }

            TimeSpan elapsed = DateTime.UtcNow - s_EditorSmokeStartTimeUtc;
            if (elapsed.TotalSeconds > 90.0)
            {
                s_EditorSmokeRequestedExit = true;
                UnityEngine.Debug.LogError($"[IMM_EDITOR_SMOKE] timed out waiting for capture: {s_EditorSmokeCapturePath}");
                EditorApplication.Exit(2);
            }
        }

        private static void OnEditorPlayModeSmokeStateChanged(PlayModeStateChange state)
        {
            if (state != PlayModeStateChange.EnteredEditMode || !s_EditorSmokeRequestedExit)
                return;

            EditorApplication.update -= UpdateEditorPlayModeSmoke;
            EditorApplication.playModeStateChanged -= OnEditorPlayModeSmokeStateChanged;

            if (!string.IsNullOrEmpty(s_EditorSmokeCapturePath) && File.Exists(s_EditorSmokeCapturePath))
            {
                ClearEditorPlayModeSmokeSession();
                UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] passed: {s_EditorSmokeCapturePath}");
                EditorApplication.Exit(0);
            }
            else if (EditorSmokeNativeLogPassed())
            {
                ClearEditorPlayModeSmokeSession();
                UnityEngine.Debug.Log("[IMM_EDITOR_SMOKE] passed: native Editor play-mode load/audio smoke completed");
                EditorApplication.Exit(0);
            }
            else
            {
                ClearEditorPlayModeSmokeSession();
                UnityEngine.Debug.LogError($"[IMM_EDITOR_SMOKE] failed: capture missing after play mode exit: {s_EditorSmokeCapturePath}");
                EditorApplication.Exit(2);
            }
        }

        private static bool EditorSmokeNativeLogPassed()
        {
            string nativeLogPath = SessionState.GetString(EditorSmokeNativeLogPathKey, string.Empty);
            if (string.IsNullOrEmpty(nativeLogPath) || !File.Exists(nativeLogPath))
                return false;

            string nativeLog = File.ReadAllText(nativeLogPath);
            return nativeLog.Contains("Loaded in SPU!") &&
                   nativeLog.Contains("Decoded Ogg Opus sound to PCM temp WAV for AVFoundation");
        }

        private static void ClearEditorPlayModeSmokeSession()
        {
            SessionState.EraseBool(EditorSmokeActiveKey);
            SessionState.EraseString(EditorSmokeCapturePathKey);
            SessionState.EraseString(EditorSmokeNativeLogPathKey);
            SessionState.EraseString(EditorSmokeStartTicksKey);
        }
    }
}
