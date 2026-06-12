using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer.Editor
{
    public static class BuildAutomation
    {
        private static readonly string[] SmokeScenes =
        {
            "Assets/Scenes/SampleScene.unity"
        };
        private const string EditorSmokeCapturePathEnv = "IMM_UNITY_EDITOR_SMOKE_CAPTURE_PATH";
        private const string RuntimeSmokeCapturePathEnv = "IMM_UNITY_SMOKE_CAPTURE_PATH";
        private const string RuntimeSmokeFramesEnv = "IMM_UNITY_SMOKE_FRAMES";
        private const string RuntimeSmokeQuitEnv = "IMM_UNITY_SMOKE_QUIT";
        private const string EditorOverlayFixtureEnv = "IMM_UNITY_EDITOR_OVERLAY_FIXTURE";
        private const string EditorOverlayFixtureSecondCameraEnv = "IMM_UNITY_EDITOR_OVERLAY_FIXTURE_SECOND_CAMERA";
        private const string EditorSmokeActiveKey = "IMM_EDITOR_SMOKE_ACTIVE";
        private const string EditorSmokeCapturePathKey = "IMM_EDITOR_SMOKE_CAPTURE_PATH";
        private const string EditorSmokeCaptureRequestedKey = "IMM_EDITOR_SMOKE_CAPTURE_REQUESTED";
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

            string capturePath = Environment.GetEnvironmentVariable(EditorSmokeCapturePathEnv);
            if (string.IsNullOrEmpty(capturePath))
            {
                capturePath = Path.GetFullPath(Path.Combine("..", "build", "unity-smoke", "macos-editor-playmode.png"));
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

            string nativeLogPath = Path.GetFullPath("imm_player_log.txt");
            if (File.Exists(nativeLogPath))
            {
                File.Delete(nativeLogPath);
            }

            SessionState.SetBool(EditorSmokeActiveKey, true);
            SessionState.SetString(EditorSmokeCapturePathKey, capturePath);
            SessionState.SetString(EditorSmokeNativeLogPathKey, nativeLogPath);
            SessionState.SetString(EditorSmokeStartTicksKey, DateTime.UtcNow.Ticks.ToString());

            EditorSceneManager.OpenScene(SmokeScenes[0]);
            ConfigureEditorOverlayFixtureIfRequested();

            s_EditorSmokeCapturePath = capturePath;
            s_EditorSmokeStartTimeUtc = DateTime.UtcNow;
            s_EditorSmokeRequestedExit = false;

            EditorApplication.update -= UpdateEditorPlayModeSmoke;
            EditorApplication.update += UpdateEditorPlayModeSmoke;
            EditorApplication.playModeStateChanged -= OnEditorPlayModeSmokeStateChanged;
            EditorApplication.playModeStateChanged += OnEditorPlayModeSmokeStateChanged;

            UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] entering play mode capture={capturePath}");
            EditorApplication.EnterPlaymode();
        }

        public static void RunWindowsVulkanEditorPlayModeSmoke()
        {
            EnsureBuildTargetSupported(BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64, "Windows");

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Vulkan });

            string capturePath = Environment.GetEnvironmentVariable(EditorSmokeCapturePathEnv);
            if (string.IsNullOrEmpty(capturePath))
            {
                capturePath = Path.GetFullPath(Path.Combine("..", "build", "unity-smoke", "windows-vulkan-editor-playmode.png"));
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

            string nativeLogPath = Path.GetFullPath("imm_player_log.txt");
            if (File.Exists(nativeLogPath))
            {
                File.Delete(nativeLogPath);
            }

            SessionState.SetBool(EditorSmokeActiveKey, true);
            SessionState.SetString(EditorSmokeCapturePathKey, capturePath);
            SessionState.SetString(EditorSmokeNativeLogPathKey, nativeLogPath);
            SessionState.SetString(EditorSmokeStartTicksKey, DateTime.UtcNow.Ticks.ToString());

            EditorSceneManager.OpenScene(SmokeScenes[0]);
            ConfigureEditorOverlayFixtureIfRequested();

            s_EditorSmokeCapturePath = capturePath;
            s_EditorSmokeStartTimeUtc = DateTime.UtcNow;
            s_EditorSmokeRequestedExit = false;

            EditorApplication.update -= UpdateEditorPlayModeSmoke;
            EditorApplication.update += UpdateEditorPlayModeSmoke;
            EditorApplication.playModeStateChanged -= OnEditorPlayModeSmokeStateChanged;
            EditorApplication.playModeStateChanged += OnEditorPlayModeSmokeStateChanged;

            UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] entering Windows Vulkan play mode capture={capturePath}");
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

            cam.clearFlags = CameraClearFlags.SolidColor;
            cam.backgroundColor = new Color(0.02f, 0.025f, 0.03f, 1.0f);

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
                UnityEngine.Debug.Log($"[IMM_EDITOR_OVERLAY_FIXTURE_20260612] overlayCamera={overlayCamera.name} depth={overlayCamera.depth} layer={overlayLayer}");
            }

            UnityEngine.Debug.Log($"[IMM_EDITOR_OVERLAY_FIXTURE_20260612] camera={cam.name} cubePos={cube.transform.position} cubeScale={cube.transform.localScale}");
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
            if (!string.IsNullOrEmpty(s_EditorSmokeCapturePath) &&
                !SessionState.GetBool(EditorSmokeCaptureRequestedKey, false) &&
                elapsed.TotalSeconds > 8.0)
            {
                SessionState.SetBool(EditorSmokeCaptureRequestedKey, true);
                UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] requesting editor fallback capture: {s_EditorSmokeCapturePath}");
                UnityEngine.ScreenCapture.CaptureScreenshot(s_EditorSmokeCapturePath);
            }

            if (!string.IsNullOrEmpty(s_EditorSmokeCapturePath) && File.Exists(s_EditorSmokeCapturePath))
            {
                if (!EditorSmokeCaptureLooksRendered(s_EditorSmokeCapturePath))
                {
                    s_EditorSmokeRequestedExit = true;
                    UnityEngine.Debug.LogError($"[IMM_EDITOR_SMOKE] failed: capture is blank or single-color: {s_EditorSmokeCapturePath}");
                    EditorApplication.ExitPlaymode();
                    return;
                }
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
                bool captureLooksRendered = EditorSmokeCaptureLooksRendered(s_EditorSmokeCapturePath);
                ClearEditorPlayModeSmokeSession();
                if (captureLooksRendered)
                {
                    UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] passed: {s_EditorSmokeCapturePath}");
                    EditorApplication.Exit(0);
                }
                else
                {
                    UnityEngine.Debug.LogError($"[IMM_EDITOR_SMOKE] failed: capture is blank or single-color: {s_EditorSmokeCapturePath}");
                    EditorApplication.Exit(2);
                }
            }
            else if (string.IsNullOrEmpty(s_EditorSmokeCapturePath) && EditorSmokeNativeLogPassed())
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

        private static bool EditorSmokeCaptureLooksRendered(string capturePath)
        {
            if (string.IsNullOrEmpty(capturePath) || !File.Exists(capturePath))
                return false;

            byte[] bytes = File.ReadAllBytes(capturePath);
            var texture = new Texture2D(2, 2, TextureFormat.RGBA32, false);
            try
            {
                if (!ImageConversion.LoadImage(texture, bytes, false))
                    return false;

                Color32[] pixels = texture.GetPixels32();
                if (pixels == null || pixels.Length == 0)
                    return false;

                int nonZero = 0;
                int[] bucketSeen = new int[64];
                int colorBuckets = 0;
                for (int i = 0; i < pixels.Length; ++i)
                {
                    Color32 p = pixels[i];
                    if (p.r != 0 || p.g != 0 || p.b != 0)
                        ++nonZero;

                    int bucket = ((p.r >> 6) << 4) | ((p.g >> 6) << 2) | (p.b >> 6);
                    if (bucketSeen[bucket] == 0)
                    {
                        bucketSeen[bucket] = 1;
                        ++colorBuckets;
                    }
                }

                UnityEngine.Debug.Log($"[IMM_EDITOR_SMOKE] capture stats: pixels={pixels.Length} nonZero={nonZero} colorBuckets={colorBuckets}");
                return nonZero > pixels.Length / 100 && colorBuckets >= 2;
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(texture);
            }
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
