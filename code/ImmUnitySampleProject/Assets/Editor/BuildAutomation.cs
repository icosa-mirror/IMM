using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEditor.XR.Management;
using UnityEngine.XR.Management;

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
        private const string RuntimeSmokeOverlayProbeEnv = "IMM_UNITY_SMOKE_OVERLAY_PROBE";
        private const string RuntimeSmokeXrProbeEnv = "IMM_UNITY_SMOKE_XR_PROBE";
        private const string EditorSmokeCaptureDelaySecondsEnv = "IMM_UNITY_EDITOR_SMOKE_CAPTURE_DELAY_SECONDS";
        private const string EditorOverlayFixtureEnv = "IMM_UNITY_EDITOR_OVERLAY_FIXTURE";
        private const string EditorOverlayFixtureSolidClearEnv = "IMM_UNITY_EDITOR_OVERLAY_FIXTURE_SOLID_CLEAR";
        private const string EditorOverlayFixtureSecondCameraEnv = "IMM_UNITY_EDITOR_OVERLAY_FIXTURE_SECOND_CAMERA";
        private const string EditorSmokeActiveKey = "IMM_EDITOR_SMOKE_ACTIVE";
        private const string EditorSmokeCapturePathKey = "IMM_EDITOR_SMOKE_CAPTURE_PATH";
        private const string EditorSmokeCaptureRequestedKey = "IMM_EDITOR_SMOKE_CAPTURE_REQUESTED";
        private const string EditorSmokeNativeLogPathKey = "IMM_EDITOR_SMOKE_NATIVE_LOG_PATH";
        private const string EditorSmokeStartTicksKey = "IMM_EDITOR_SMOKE_START_TICKS";
        private const string EditorSmokeCapturePathArg = "-immSmokeCapturePath";
        private const string EditorSmokePlayerPathArg = "-immSmokePlayerPath";

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

        public static void BuildAndroidVulkanSmokePlayer()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Android, BuildTarget.Android, "Android");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.Android, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.Android, new[] { GraphicsDeviceType.Vulkan });

            string outputPath = GetCommandLineValue(EditorSmokePlayerPathArg);
            if (string.IsNullOrEmpty(outputPath))
            {
                outputPath = Path.Combine("..", "build", "unity-smoke", "android-vulkan-smoke-player", "ImmUnitySmoke.apk");
            }
            outputPath = Path.GetFullPath(outputPath);

            string outputDir = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(outputDir))
            {
                Directory.CreateDirectory(outputDir);
            }

            AndroidArchitecture previousArchitectures = PlayerSettings.Android.targetArchitectures;
            XRGeneralSettings androidXrSettings =
                XRGeneralSettingsPerBuildTarget.XRGeneralSettingsForBuildTarget(BuildTargetGroup.Android);
            bool previousInitManagerOnStart = androidXrSettings != null && androidXrSettings.InitManagerOnStart;
            try
            {
                PlayerSettings.Android.targetArchitectures = AndroidArchitecture.ARM64;
                if (androidXrSettings != null)
                {
                    androidXrSettings.InitManagerOnStart = false;
                }

                string[] androidVulkanDefines = new[] { "IMM_UNITY_ANDROID_VULKAN_CI" }
                    .Concat(new[] { "IMM_UNITY_ANDROID_VULKAN_SURFACE_CONTROL" })
                    .ToArray();
                BuildPlayer(
                    BuildTarget.Android,
                    outputPath,
                    BuildOptions.Development,
                    "Android Vulkan non-XR smoke player",
                    androidVulkanDefines);
            }
            finally
            {
                PlayerSettings.Android.targetArchitectures = previousArchitectures;
                if (androidXrSettings != null)
                {
                    androidXrSettings.InitManagerOnStart = previousInitManagerOnStart;
                }
            }
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

        public static void BuildMacOSMetalSmokePlayer()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneOSX, "macOS");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneOSX, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneOSX, new[] { GraphicsDeviceType.Metal });

            string outputPath = GetCommandLineValue(EditorSmokePlayerPathArg);
            if (string.IsNullOrEmpty(outputPath))
            {
                outputPath = Path.Combine("..", "build", "unity-smoke", "macos-metal-smoke-player", "ImmUnitySmoke.app");
            }
            outputPath = Path.GetFullPath(outputPath);

            string outputDir = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(outputDir))
            {
                Directory.CreateDirectory(outputDir);
            }

            BuildPlayer(BuildTarget.StandaloneOSX, outputPath, BuildOptions.Development, "macOS Metal smoke player");
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

        public static void RunWindowsD3D11EditorPlayModeSmoke()
        {
            RunWindowsDirectXEditorPlayModeSmoke();
        }

        public static void RunWindowsVulkanEditorPlayModeSmoke()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64, "Windows");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Vulkan });

            RunEditorPlayModeSmoke("Windows Vulkan", SmokeScenes[0], Path.Combine("..", "build", "unity-smoke", "windows-vulkan-editor-playmode.png"), true, false);
        }

        public static void BuildWindowsDirectXSmokePlayer()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64, "Windows");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Direct3D11 });

            string outputPath = GetCommandLineValue(EditorSmokePlayerPathArg);
            if (string.IsNullOrEmpty(outputPath))
            {
                outputPath = Path.Combine("..", "build", "unity-smoke", "windows-directx-smoke-player", "ImmUnitySmoke.exe");
            }
            outputPath = Path.GetFullPath(outputPath);

            string outputDir = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(outputDir))
            {
                Directory.CreateDirectory(outputDir);
            }

            BuildPlayer(BuildTarget.StandaloneWindows64, outputPath, BuildOptions.Development, "Windows DirectX smoke player");
        }

        public static void BuildWindowsVulkanSmokePlayer()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64, "Windows");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Vulkan });

            string outputPath = GetCommandLineValue(EditorSmokePlayerPathArg);
            if (string.IsNullOrEmpty(outputPath))
            {
                outputPath = Path.Combine("..", "build", "unity-smoke", "windows-vulkan-smoke-player", "ImmUnitySmoke.exe");
            }
            outputPath = Path.GetFullPath(outputPath);

            string outputDir = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(outputDir))
            {
                Directory.CreateDirectory(outputDir);
            }

            BuildPlayer(BuildTarget.StandaloneWindows64, outputPath, BuildOptions.Development, "Windows Vulkan smoke player");
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
            string capturePath = GetCommandLineValue(EditorSmokeCapturePathArg);
            if (string.IsNullOrEmpty(capturePath))
            {
                capturePath = Environment.GetEnvironmentVariable(EditorSmokeCapturePathEnv);
            }
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
                Environment.SetEnvironmentVariable(RuntimeSmokeFramesEnv, "90");
            }
            Environment.SetEnvironmentVariable(RuntimeSmokeQuitEnv, "0");
            Environment.SetEnvironmentVariable(RuntimeSmokeCompositionProbeEnv, enableCompositionProbe ? "1" : string.Empty);
            Environment.SetEnvironmentVariable(RuntimeSmokeOverlayProbeEnv, string.Empty);
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
            ConfigureSmokeMsaaIfRequested();
            ConfigureEditorOverlayFixtureIfRequested();

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

        private static string GetCommandLineValue(string key)
        {
            string[] args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; ++i)
            {
                if (string.Equals(args[i], key, StringComparison.OrdinalIgnoreCase))
                {
                    return args[i + 1];
                }
            }

            return string.Empty;
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

        private static void ConfigureEditorOverlayFixtureIfRequested()
        {
            string enabled = Environment.GetEnvironmentVariable(EditorOverlayFixtureEnv);
            if (string.IsNullOrEmpty(enabled) || enabled == "0")
                return;

            Camera cam = Camera.main;
            if (cam == null)
            {
                cam = UnityEngine.Object.FindObjectOfType<Camera>();
            }
            if (cam == null)
            {
                UnityEngine.Debug.LogError("[IMM_EDITOR_OVERLAY_FIXTURE_20260612] failed: no camera found");
                return;
            }

            string solidClear = Environment.GetEnvironmentVariable(EditorOverlayFixtureSolidClearEnv);
            if (!string.IsNullOrEmpty(solidClear) && solidClear != "0")
            {
                cam.clearFlags = CameraClearFlags.SolidColor;
                cam.backgroundColor = new Color(0.02f, 0.025f, 0.03f, 1.0f);
            }

            GameObject cube = GameObject.CreatePrimitive(PrimitiveType.Cube);
            cube.name = "IMM Editor Overlay Fixture Cube";
            cube.transform.position = cam.ViewportToWorldPoint(new Vector3(0.95f, 0.45f, 4.0f));
            cube.transform.rotation = Quaternion.identity;
            cube.transform.localScale = new Vector3(1.0f, 1.0f, 1.0f);

            Shader shader = Shader.Find("Unlit/Color");
            Material mat = shader != null ? new Material(shader) : new Material(Shader.Find("Standard"));
            mat.color = new Color(1.0f, 0.05f, 0.02f, 1.0f);
            Renderer renderer = cube.GetComponent<Renderer>();
            if (renderer != null)
            {
                renderer.sharedMaterial = mat;
            }

            string secondCamera = Environment.GetEnvironmentVariable(EditorOverlayFixtureSecondCameraEnv);
            if (!string.IsNullOrEmpty(secondCamera) && secondCamera != "0")
            {
                const int overlayLayer = 30;
                cube.layer = overlayLayer;
                cam.cullingMask = 0;

                GameObject overlayCameraObject = new GameObject("IMM Editor Overlay Fixture Camera");
                Camera overlayCamera = overlayCameraObject.AddComponent<Camera>();
                overlayCamera.CopyFrom(cam);
                overlayCamera.transform.SetPositionAndRotation(cam.transform.position, cam.transform.rotation);
                overlayCamera.clearFlags = CameraClearFlags.Nothing;
                overlayCamera.cullingMask = 1 << overlayLayer;
                overlayCamera.depth = cam.depth + 1.0f;
                overlayCamera.name = "IMM Overlay Fixture Camera";
                Environment.SetEnvironmentVariable(RuntimeSmokeOverlayProbeEnv, "1");
                ImmPlayerManager manager = UnityEngine.Object.FindObjectOfType<ImmPlayerManager>();
                if (manager != null)
                {
                    manager.SetRenderCamera(cam);
                }
                UnityEngine.Debug.Log($"[IMM_EDITOR_OVERLAY_FIXTURE_20260612] overlayCamera={overlayCamera.name} depth={overlayCamera.depth} layer={overlayLayer}");
            }

            UnityEngine.Debug.Log($"[IMM_EDITOR_OVERLAY_FIXTURE_20260612] camera={cam.name} cubePos={cube.transform.position} cubeScale={cube.transform.localScale}");
        }

        private static void ConfigureSmokeMsaaIfRequested()
        {
            string noMsaa = Environment.GetEnvironmentVariable("IMM_UNITY_FORCE_NO_MSAA");
            string forceMsaa = Environment.GetEnvironmentVariable("IMM_UNITY_FORCE_MSAA");
            if ((string.IsNullOrEmpty(noMsaa) || noMsaa == "0") && string.IsNullOrEmpty(forceMsaa))
            {
                return;
            }

            int antiAliasing = 0;
            if (!string.IsNullOrEmpty(forceMsaa))
            {
                int.TryParse(forceMsaa, out antiAliasing);
                antiAliasing = Mathf.Max(0, antiAliasing);
            }

            QualitySettings.antiAliasing = antiAliasing;
            Camera[] cameras = UnityEngine.Object.FindObjectsOfType<Camera>();
            foreach (Camera camera in cameras)
            {
                camera.allowMSAA = antiAliasing > 1;
            }

            UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE_MSAA_20260612] set camera MSAA for diagnostic smoke antiAliasing={antiAliasing}");
        }

        private static void BuildPlayer(
            BuildTarget target,
            string outputPath,
            BuildOptions options,
            string label,
            string[] extraScriptingDefines = null)
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
                options = options,
                extraScriptingDefines = extraScriptingDefines ?? Array.Empty<string>()
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
            if (string.IsNullOrEmpty(s_EditorSmokeCapturePath) &&
                !string.IsNullOrEmpty(nativeLogPath) &&
                File.Exists(nativeLogPath))
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

            TimeSpan elapsed = DateTime.UtcNow - s_EditorSmokeStartTimeUtc;
            double captureDelaySeconds = 8.0;
            string captureDelayText = Environment.GetEnvironmentVariable(EditorSmokeCaptureDelaySecondsEnv);
            if (!string.IsNullOrEmpty(captureDelayText) &&
                double.TryParse(captureDelayText, out double configuredCaptureDelaySeconds) &&
                configuredCaptureDelaySeconds >= 0.0)
            {
                captureDelaySeconds = configuredCaptureDelaySeconds;
            }

            if (!string.IsNullOrEmpty(s_EditorSmokeCapturePath) &&
                !SessionState.GetBool(EditorSmokeCaptureRequestedKey, false) &&
                elapsed.TotalSeconds > captureDelaySeconds)
            {
                SessionState.SetBool(EditorSmokeCaptureRequestedKey, true);
                UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] requesting editor fallback capture: {s_EditorSmokeCapturePath}");
                UnityEngine.ScreenCapture.CaptureScreenshot(s_EditorSmokeCapturePath);
            }

            if (!string.IsNullOrEmpty(s_EditorSmokeCapturePath) && File.Exists(s_EditorSmokeCapturePath))
            {
                s_EditorSmokeRequestedExit = true;
                UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] capture complete: {s_EditorSmokeCapturePath}");
                EditorApplication.ExitPlaymode();
                return;
            }

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
            SessionState.EraseBool(EditorSmokeCaptureRequestedKey);
            SessionState.EraseString(EditorSmokeNativeLogPathKey);
            SessionState.EraseString(EditorSmokeStartTicksKey);
        }
    }
}
