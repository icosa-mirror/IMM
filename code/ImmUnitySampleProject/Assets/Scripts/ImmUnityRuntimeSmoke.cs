using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;

namespace ImmPlayer
{
    public sealed class ImmUnityRuntimeSmoke : MonoBehaviour
    {
        private const string CapturePathEnv = "IMM_UNITY_SMOKE_CAPTURE_PATH";
        private const string RenderCapturePathEnv = "IMM_UNITY_SMOKE_RENDER_CAPTURE_PATH";
        private const string FramesEnv = "IMM_UNITY_SMOKE_FRAMES";
        private const string QuitEnv = "IMM_UNITY_SMOKE_QUIT";
        private const string CompositionProbeEnv = "IMM_UNITY_SMOKE_COMPOSITION_PROBE";
        private const string OverlayProbeEnv = "IMM_UNITY_SMOKE_OVERLAY_PROBE";
        private const string OverlayFixtureEnv = "IMM_UNITY_SMOKE_OVERLAY_FIXTURE";
        private const string FreezePlaybackEnv = "IMM_UNITY_SMOKE_FREEZE_PLAYBACK";
        private const string FreezeTimeTicksEnv = "IMM_UNITY_SMOKE_FREEZE_TIME_TICKS";
        private const string XrProbeEnv = "IMM_UNITY_SMOKE_XR_PROBE";
        private const string ExpectedGraphicsApiEnv = "IMM_UNITY_EXPECT_GRAPHICS_API";
        private const string DisableMsaaEnv = "IMM_UNITY_SMOKE_DISABLE_MSAA";
        private const string CaptureCameraTextureEnv = "IMM_UNITY_SMOKE_CAPTURE_CAMERA_TEXTURE";
        private const string CapturePathArg = "-immSmokeCapturePath";
        private const string RenderCapturePathArg = "-immSmokeRenderCapturePath";
        private const string FramesArg = "-immSmokeFrames";
        private const string QuitArg = "-immSmokeQuit";
        private const string CompositionProbeArg = "-immSmokeCompositionProbe";
        private const string OverlayProbeArg = "-immSmokeOverlayProbe";
        private const string XrProbeArg = "-immSmokeXrProbe";
        private const string ExpectedGraphicsApiArg = "-immSmokeExpectedGraphicsApi";
        private const string MinOrderedOverlayImmUniqueColorsArg = "-immSmokeMinOrderedOverlayImmUniqueColors";
        private const string Prefix = "[IMM_UNITY_SMOKE] ";
        private const int MinRegionPixels = 24;
        private const float MinDominantShare = 0.80f;
        private const float MaxOccludedShare = 0.08f;
        private const float MinOrderedOverlayImmShare = 0.02f;
        private const int MinOrderedOverlayImmUniqueColors = 5000;
        private const float MinOrderedOverlayTopRightBrightToBottomRightRatio = 2.0f;
        private const float MinOrderedOverlayBottomLeftPaintToTopLeftRatio = 2.0f;
        private const int CaptureWidth = 1280;
        private const int CaptureHeight = 720;
        private const long DefaultCompositionFreezeTimeTicks = 37800;
        private static readonly Color FrontProbeColor = new Color(1.0f, 0.0f, 1.0f, 1.0f);
        private static readonly Color RearOccludedProbeColor = new Color(0.0f, 1.0f, 1.0f, 1.0f);
        private static readonly Color RearVisibleProbeColor = new Color(1.0f, 1.0f, 0.0f, 1.0f);

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void Install()
        {
            string capturePath = GetCommandLineValue(CapturePathArg);
            if (string.IsNullOrEmpty(capturePath))
            {
                capturePath = Environment.GetEnvironmentVariable(CapturePathEnv);
            }
#if IMM_UNITY_ANDROID_VULKAN_CI
            capturePath = Path.Combine(Application.persistentDataPath, "imm-ci", "unity-android-vulkan.png");
#endif
            if (string.IsNullOrEmpty(capturePath))
                return;

            var go = new GameObject("IMM Unity Runtime Smoke");
            DontDestroyOnLoad(go);
            ImmUnityRuntimeSmoke smoke = go.AddComponent<ImmUnityRuntimeSmoke>();
            smoke._capturePath = capturePath;
            smoke._renderCapturePath = GetCommandLineValue(RenderCapturePathArg);
            if (string.IsNullOrEmpty(smoke._renderCapturePath))
            {
                smoke._renderCapturePath = Environment.GetEnvironmentVariable(RenderCapturePathEnv);
            }
#if IMM_UNITY_ANDROID_VULKAN_CI
            smoke._renderCapturePath = Path.Combine(Application.persistentDataPath, "imm-ci", "unity-android-vulkan-render.png");
            smoke._presentationCapturePath = Path.Combine(
                Application.persistentDataPath,
                "imm-ci",
                "unity-android-vulkan-presented.png");
            Debug.Log($"{Prefix}Android Vulkan CI smoke installed capture={capturePath}");
#endif
        }

        private string _capturePath;
        private string _renderCapturePath;
        private string _presentationCapturePath;
        private bool _compositionProbeEnabled;
        private bool _overlayProbeEnabled;
        private bool _xrProbeEnabled;
        private Camera _compositionCamera;
        private GameObject _frontProbe;
        private GameObject _rearOccludedProbe;
        private GameObject _rearVisibleProbe;
        private RenderTexture _diagnosticCameraTargetTexture;
        private readonly List<string> _compositionFailures = new List<string>();

        private IEnumerator Start()
        {
            int frameCount = 180;
            _compositionProbeEnabled = IsEnabled(CompositionProbeArg, CompositionProbeEnv);
            _overlayProbeEnabled = IsEnabled(OverlayProbeArg, OverlayProbeEnv);
            _xrProbeEnabled = IsEnabled(XrProbeArg, XrProbeEnv);
#if IMM_UNITY_ANDROID_VULKAN_CI
            frameCount = 90;
            _compositionProbeEnabled = true;
            _overlayProbeEnabled = false;
            _xrProbeEnabled = false;
            Debug.Log($"[IMM_UNITY_ANDROID_VK_SMOKE_FRAMES_20260729] warmupFrames={frameCount}");
#endif
            if (!ValidateExpectedGraphicsApi())
            {
                QuitIfRequested(6);
                yield break;
            }
            bool resizeDisplayForCapture = true;
#if IMM_UNITY_ANDROID_VULKAN_CI
            // Keep Android's native swapchain extent. Resizing the display after
            // Vulkan initialization invalidates the presentation surface used by
            // the native render callback. CaptureScreenTexture resamples the
            // resulting framebuffer to the fixed validation dimensions.
            resizeDisplayForCapture = false;
            Debug.Log($"[IMM_UNITY_ANDROID_VK_NATIVE_SURFACE_20260730] capture source={Screen.width}x{Screen.height}");
#endif
            if (resizeDisplayForCapture && (Screen.width != CaptureWidth || Screen.height != CaptureHeight))
            {
                Debug.Log($"{Prefix}setting capture resolution {CaptureWidth}x{CaptureHeight} from {Screen.width}x{Screen.height}");
                Screen.SetResolution(CaptureWidth, CaptureHeight, false);
            }
            if (IsTruthyValue(Environment.GetEnvironmentVariable(DisableMsaaEnv)))
            {
                QualitySettings.antiAliasing = 0;
                foreach (Camera camera in FindObjectsOfType<Camera>())
                {
                    camera.allowMSAA = false;
                }
                Debug.Log($"{Prefix}runtime diagnostic MSAA disabled");
            }
            bool enableDiagnosticCameraTarget =
                IsTruthyValue(Environment.GetEnvironmentVariable(CaptureCameraTextureEnv));
            if (enableDiagnosticCameraTarget)
            {
                ConfigureDiagnosticCameraTargetTexture();
            }

            string framesText = GetCommandLineValue(FramesArg);
            if (string.IsNullOrEmpty(framesText))
            {
                framesText = Environment.GetEnvironmentVariable(FramesEnv);
            }
            if (!string.IsNullOrEmpty(framesText) && int.TryParse(framesText, out int parsedFrames))
            {
                frameCount = Mathf.Max(1, parsedFrames);
            }

            for (int i = 0; i < frameCount; ++i)
            {
                yield return null;
            }

            yield return StabilizeSampleViewpoint();

            if (_compositionProbeEnabled)
            {
                FreezeCompositionPlaybackIfRequested();
                for (int settleFrame = 0; settleFrame < 5; ++settleFrame)
                {
                    yield return null;
                }
            }

            if (!string.IsNullOrEmpty(_renderCapturePath))
            {
                yield return new WaitForEndOfFrame();
#if IMM_UNITY_ANDROID_VULKAN_CI
                ImmPlayerManager playerManager = FindObjectOfType<ImmPlayerManager>();
                Camera renderCaptureCamera = playerManager != null
                    ? playerManager.GetAndroidVulkanSourceCameraForValidation()
                    : null;
                if (renderCaptureCamera == null)
                    renderCaptureCamera = Camera.main != null ? Camera.main : FindObjectOfType<Camera>();
#else
                Camera renderCaptureCamera = Camera.main != null ? Camera.main : FindObjectOfType<Camera>();
#endif
                if (renderCaptureCamera == null)
                {
                    Debug.LogError($"{Prefix}missing render candidate capture camera");
                    QuitIfRequested(2);
                    yield break;
                }
#if IMM_UNITY_ANDROID_VULKAN_CI
                RenderTexture vulkanPresentationTarget = playerManager != null
                    ? playerManager.GetAndroidVulkanPresentationTargetForValidation(renderCaptureCamera)
                    : null;
                Texture2D renderCapture;
                if (vulkanPresentationTarget != null)
                {
                    Debug.Log(
                        $"{Prefix}render source=unity-vulkan-presentation-texture " +
                        $"size={vulkanPresentationTarget.width}x{vulkanPresentationTarget.height}");
                    renderCapture = CaptureRenderTexture(vulkanPresentationTarget);
                }
                else
                {
                    Debug.Log(
                        $"{Prefix}render source=unity-vulkan-active-render-pass " +
                        $"size={Screen.width}x{Screen.height}");
                    renderCapture = CaptureScreenTexture();
                }
#else
                Texture2D renderCapture = _diagnosticCameraTargetTexture != null
                    ? CaptureRenderTexture(_diagnosticCameraTargetTexture)
                    : (SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan
                        ? CaptureScreenTexture()
                        : CaptureCameraTexture(renderCaptureCamera));
#endif
                WriteCapture(renderCapture, _renderCapturePath, "render candidate");
#if IMM_UNITY_ANDROID_VULKAN_CI
                RenderTexture unityPresentationTarget = playerManager != null
                    ? playerManager.GetAndroidVulkanUnityPresentationTargetForValidation(renderCaptureCamera)
                    : null;
                if (unityPresentationTarget != null && !string.IsNullOrEmpty(_presentationCapturePath))
                {
                    Texture2D presentationCapture = CaptureRenderTexture(unityPresentationTarget);
                    WriteCapture(
                        presentationCapture,
                        _presentationCapturePath,
                        "Unity presentation target");
                }
#endif
            }

            if (_compositionProbeEnabled)
            {
                if (_overlayProbeEnabled)
                {
                    ConfigureRuntimeOverlayFixtureIfRequested();
                }
                if (!CreateCompositionProbes())
                {
                    RecordCompositionFailure("failed to create scene composition probes");
                }
                for (int i = 0; i < 5; ++i)
                {
                    yield return null;
                }
            }

            yield return new WaitForEndOfFrame();

            Camera captureCamera = _compositionCamera != null ? _compositionCamera : Camera.main;
            if (captureCamera == null)
            {
                Debug.LogError($"{Prefix}missing capture camera");
                QuitIfRequested(2);
                yield break;
            }

            bool usePresentedFrameCapture = !IsTruthyValue(Environment.GetEnvironmentVariable(CaptureCameraTextureEnv)) &&
                (_overlayProbeEnabled ||
                (_compositionProbeEnabled && SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan));
            Texture2D tex = _diagnosticCameraTargetTexture != null
                ? CaptureRenderTexture(_diagnosticCameraTargetTexture)
                : (_overlayProbeEnabled && SystemInfo.graphicsDeviceType != UnityEngine.Rendering.GraphicsDeviceType.Vulkan
                    ? CaptureOrderedCameraStackTexture(captureCamera)
                    : (usePresentedFrameCapture ? CaptureScreenTexture() : CaptureCameraTexture(captureCamera)));

            int width = tex.width;
            int height = tex.height;
            if (width <= 0 || height <= 0)
            {
                Debug.LogError($"{Prefix}invalid screen size {width}x{height}");
                QuitIfRequested(2);
                yield break;
            }

            Color32[] pixels = tex.GetPixels32();
            ulong hash = 1469598103934665603UL;
            int nonZero = 0;
            int colorBuckets = 0;
            int[] bucketSeen = new int[16];

            for (int i = 0; i < pixels.Length; ++i)
            {
                Color32 p = pixels[i];
                if (p.r != 0 || p.g != 0 || p.b != 0)
                    ++nonZero;

                hash ^= p.r;
                hash *= 1099511628211UL;
                hash ^= p.g;
                hash *= 1099511628211UL;
                hash ^= p.b;
                hash *= 1099511628211UL;

                int bucket = ((p.r >> 6) << 2) ^ ((p.g >> 6) << 1) ^ (p.b >> 6);
                if (bucketSeen[bucket] == 0)
                {
                    bucketSeen[bucket] = 1;
                    ++colorBuckets;
                }

            }

            if (_compositionProbeEnabled)
            {
                CompositionRegionResult front = AnalyzeProbeRegion(pixels, width, height, _compositionCamera, _frontProbe, FrontProbeColor);
                CompositionRegionResult rearVisible = AnalyzeProbeRegion(pixels, width, height, _compositionCamera, _rearVisibleProbe, RearVisibleProbeColor);
                CompositionRegionResult rearOccluded = AnalyzeProbeRegion(pixels, width, height, _compositionCamera, _rearOccludedProbe, RearOccludedProbeColor);
                Debug.Log($"{Prefix}composition front={front} rearVisible={rearVisible} rearOccluded={rearOccluded}");
                if (front.TotalPixels < MinRegionPixels || front.Share < MinDominantShare)
                {
                    RecordCompositionFailure($"scene composition front occluder failed: {front}");
                }
                if (rearVisible.TotalPixels < MinRegionPixels || rearVisible.Share < MinDominantShare)
                {
                    RecordCompositionFailure($"scene composition rear visible probe failed: {rearVisible}");
                }
                if (_overlayProbeEnabled)
                {
                    OrderedOverlayImmResult immResult = AnalyzeOrderedOverlayImmContent(pixels, width, height);
                    int minOrderedOverlayImmUniqueColors = GetPositiveCommandLineInt(
                        MinOrderedOverlayImmUniqueColorsArg,
                        MinOrderedOverlayImmUniqueColors);
                    Debug.Log($"{Prefix}composition orderedOverlayImm={immResult}");
                    if (immResult.Share < MinOrderedOverlayImmShare || immResult.UniqueColors < minOrderedOverlayImmUniqueColors)
                    {
                        RecordCompositionFailure($"scene composition ordered overlay IMM background failed: {immResult}");
                    }
                    if (immResult.TopRightBrightToBottomRightRatio < MinOrderedOverlayTopRightBrightToBottomRightRatio ||
                        immResult.BottomLeftPaintToTopLeftRatio < MinOrderedOverlayBottomLeftPaintToTopLeftRatio)
                    {
                        RecordCompositionFailure($"scene composition ordered overlay orientation failed: {immResult}");
                    }
                    if (rearOccluded.TotalPixels < MinRegionPixels || rearOccluded.Share < MinDominantShare)
                    {
                        RecordCompositionFailure($"scene composition overlay rear probe failed: {rearOccluded}");
                    }
                }
                else if (rearOccluded.TotalPixels < MinRegionPixels || rearOccluded.Share > MaxOccludedShare)
                {
                    RecordCompositionFailure($"scene composition rear occlusion probe failed: {rearOccluded}");
                }
                if (_compositionFailures.Count == 0)
                {
                    Debug.Log($"{Prefix}{(_overlayProbeEnabled ? "scene composition overlay probe passed" : "scene composition probe passed")}");
                }
            }

            if (_xrProbeEnabled && !ValidateXrProbe())
            {
                QuitIfRequested(5);
                yield break;
            }

            string fullPath = Path.GetFullPath(_capturePath);
            string dir = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);

            File.WriteAllBytes(fullPath, tex.EncodeToPNG());
            Destroy(tex);

            Debug.Log($"{Prefix}capture={fullPath} width={width} height={height} pixels={pixels.Length} nonZero={nonZero} colorBuckets={colorBuckets} hash={hash}");
            ReleaseDiagnosticCameraTargetTexture();
#if IMM_UNITY_ANDROID_VULKAN_CI
            // Firebase's Robo test takes its externally observed screenshot at
            // the end of the wait action. Keep the real composition foreground
            // so Firebase observes the same surface that this capture validates.
            Debug.Log("[IMM_UNITY_ANDROID_VK_COMPOSITION_HOLD_20260731] activity remains foreground for Firebase capture");
            yield break;
#else
            QuitIfRequested(0);
#endif
        }

        private static void WriteCapture(Texture2D texture, string capturePath, string label)
        {
            string fullPath = Path.GetFullPath(capturePath);
            string directory = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(directory))
            {
                Directory.CreateDirectory(directory);
            }
            File.WriteAllBytes(fullPath, texture.EncodeToPNG());
            Debug.Log($"{Prefix}{label} capture={fullPath} width={texture.width} height={texture.height}");
            UnityEngine.Object.Destroy(texture);
        }

        private void ConfigureDiagnosticCameraTargetTexture()
        {
            Camera camera = Camera.main;
            if (camera == null)
                camera = FindObjectOfType<Camera>();
            if (camera == null)
            {
                Debug.LogWarning($"{Prefix}runtime diagnostic camera target skipped: no camera");
                return;
            }

            _diagnosticCameraTargetTexture = new RenderTexture(CaptureWidth, CaptureHeight, 24, RenderTextureFormat.ARGB32)
            {
                antiAliasing = Mathf.Max(1, QualitySettings.antiAliasing),
                name = "IMM Runtime Smoke Diagnostic Target"
            };
            _diagnosticCameraTargetTexture.Create();
            camera.targetTexture = _diagnosticCameraTargetTexture;
            Debug.Log($"{Prefix}runtime diagnostic camera target enabled camera={camera.name} samples={_diagnosticCameraTargetTexture.antiAliasing}");
        }

        private void ReleaseDiagnosticCameraTargetTexture()
        {
            if (_diagnosticCameraTargetTexture == null)
                return;

            Camera[] cameras = FindObjectsOfType<Camera>();
            foreach (Camera camera in cameras)
            {
                if (camera != null && camera.targetTexture == _diagnosticCameraTargetTexture)
                    camera.targetTexture = null;
            }
            _diagnosticCameraTargetTexture.Release();
            Destroy(_diagnosticCameraTargetTexture);
            _diagnosticCameraTargetTexture = null;
        }

        private static Texture2D CaptureRenderTexture(RenderTexture renderTexture)
        {
            RenderTexture previousActive = RenderTexture.active;
            RenderTexture stagingTexture = RenderTexture.GetTemporary(
                CaptureWidth,
                CaptureHeight,
                0,
                RenderTextureFormat.ARGB32,
                RenderTextureReadWrite.Default);
            try
            {
                // Native Vulkan rendering writes directly into the camera
                // target. A Unity-owned blit establishes the resource
                // transition and submission dependency before CPU readback.
                Graphics.Blit(renderTexture, stagingTexture);
                RenderTexture.active = stagingTexture;
                var tex = new Texture2D(CaptureWidth, CaptureHeight, TextureFormat.RGB24, false);
                tex.ReadPixels(new Rect(0, 0, CaptureWidth, CaptureHeight), 0, 0, false);
                tex.Apply(false, false);
                return tex;
            }
            finally
            {
                RenderTexture.active = previousActive;
                RenderTexture.ReleaseTemporary(stagingTexture);
            }
        }

        private static Texture2D CaptureCameraTexture(Camera captureCamera)
        {
            RenderTexture renderTexture = new RenderTexture(CaptureWidth, CaptureHeight, 24, RenderTextureFormat.ARGB32);
            RenderTexture previousActive = RenderTexture.active;
            RenderTexture previousTarget = captureCamera.targetTexture;
            try
            {
                captureCamera.targetTexture = renderTexture;
                RenderTexture.active = renderTexture;
                captureCamera.Render();

                var tex = new Texture2D(CaptureWidth, CaptureHeight, TextureFormat.RGB24, false);
                tex.ReadPixels(new Rect(0, 0, CaptureWidth, CaptureHeight), 0, 0, false);
                tex.Apply(false, false);
                return tex;
            }
            finally
            {
                captureCamera.targetTexture = previousTarget;
                RenderTexture.active = previousActive;
                renderTexture.Release();
                Destroy(renderTexture);
            }
        }

        private static Texture2D CaptureScreenTexture()
        {
            Texture2D screenTexture = ScreenCapture.CaptureScreenshotAsTexture();
            if (screenTexture.width == CaptureWidth && screenTexture.height == CaptureHeight)
                return screenTexture;

            var resized = new Texture2D(CaptureWidth, CaptureHeight, TextureFormat.RGB24, false);
            Color[] sourcePixels = screenTexture.GetPixels();
            Color[] resizedPixels = new Color[CaptureWidth * CaptureHeight];
            int sourceWidth = screenTexture.width;
            int sourceHeight = screenTexture.height;
            for (int y = 0; y < CaptureHeight; ++y)
            {
                int sourceY = Mathf.Clamp(Mathf.RoundToInt((y + 0.5f) * sourceHeight / CaptureHeight - 0.5f), 0, sourceHeight - 1);
                for (int x = 0; x < CaptureWidth; ++x)
                {
                    int sourceX = Mathf.Clamp(Mathf.RoundToInt((x + 0.5f) * sourceWidth / CaptureWidth - 0.5f), 0, sourceWidth - 1);
                    resizedPixels[y * CaptureWidth + x] = sourcePixels[sourceY * sourceWidth + sourceX];
                }
            }
            resized.SetPixels(resizedPixels);
            resized.Apply(false, false);
            Destroy(screenTexture);
            return resized;
        }

        private static IEnumerator StabilizeSampleViewpoint()
        {
            ImmPlayerExample example = FindObjectOfType<ImmPlayerExample>();
            ImmFeatureExamples featureExamples = FindObjectOfType<ImmFeatureExamples>();
            if (example == null && featureExamples == null)
            {
                Debug.LogWarning($"{Prefix}smoke viewpoint target was not found");
                yield break;
            }

            const int maxAttempts = 180;
            for (int i = 0; i < maxAttempts; ++i)
            {
                bool applied = example != null
                    ? example.TrySetSmokeSpawnArea(0)
                    : featureExamples.TrySetSmokeSpawnArea(0);
                if (applied)
                {
                    Debug.Log($"{Prefix}smoke spawn area 0 applied to {(example != null ? nameof(ImmPlayerExample) : nameof(ImmFeatureExamples))} after {i + 1} attempt(s)");
                    for (int settleFrame = 0; settleFrame < 10; ++settleFrame)
                    {
                        yield return null;
                    }
                    yield break;
                }
                yield return null;
            }

            Debug.LogWarning($"{Prefix}smoke spawn area 0 was not available before capture");
        }

        private void FreezeCompositionPlaybackIfRequested()
        {
            string freezeFlag = Environment.GetEnvironmentVariable(FreezePlaybackEnv);
            if (!string.IsNullOrEmpty(freezeFlag) && !IsTruthyValue(freezeFlag))
            {
                Debug.Log($"{Prefix}composition playback freeze disabled");
                return;
            }

            long freezeTicks = DefaultCompositionFreezeTimeTicks;
            string freezeTicksText = Environment.GetEnvironmentVariable(FreezeTimeTicksEnv);
            if (!string.IsNullOrEmpty(freezeTicksText) && long.TryParse(freezeTicksText, out long parsedTicks))
                freezeTicks = Math.Max(0L, parsedTicks);

            int frozenDocuments = 0;
            var seenDocuments = new HashSet<int>();
            MonoBehaviour[] behaviours = FindObjectsOfType<MonoBehaviour>();
            for (int i = 0; i < behaviours.Length; ++i)
            {
                MonoBehaviour behaviour = behaviours[i];
                if (behaviour == null)
                    continue;

                Type type = behaviour.GetType();
                while (type != null)
                {
                    FieldInfo[] fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
                    for (int fieldIndex = 0; fieldIndex < fields.Length; ++fieldIndex)
                    {
                        FieldInfo field = fields[fieldIndex];
                        if (!typeof(ImmDocument).IsAssignableFrom(field.FieldType))
                            continue;

                        if (field.GetValue(behaviour) is ImmDocument document && document.IsLoaded)
                        {
                            if (!seenDocuments.Add(document.DocumentId))
                                continue;

                            document.Show();
                            document.SetTime(freezeTicks, 1);
                            document.Pause();
                            ++frozenDocuments;
                        }
                    }
                    type = type.BaseType;
                }
            }

            if (frozenDocuments > 0)
                ImmNativePlugin.GlobalWork(1);

            Debug.Log($"{Prefix}composition playback freeze documents={frozenDocuments} ticks={freezeTicks}");
        }

        private static Texture2D CaptureOrderedCameraStackTexture(Camera finalCamera)
        {
            Camera[] cameras = FindObjectsOfType<Camera>();
            Array.Sort(cameras, (left, right) => left.depth.CompareTo(right.depth));

            RenderTexture renderTexture = new RenderTexture(CaptureWidth, CaptureHeight, 24, RenderTextureFormat.ARGB32);
            RenderTexture previousActive = RenderTexture.active;
            var previousTargets = new Dictionary<Camera, RenderTexture>();
            try
            {
                RenderTexture.active = renderTexture;
                GL.Clear(true, true, Color.clear);

                for (int i = 0; i < cameras.Length; ++i)
                {
                    Camera camera = cameras[i];
                    if (camera == null || !camera.enabled || camera.targetTexture != null)
                        continue;
                    if (finalCamera != null && camera.targetDisplay != finalCamera.targetDisplay)
                        continue;

                    previousTargets[camera] = camera.targetTexture;
                    camera.targetTexture = renderTexture;
                    RenderTexture.active = renderTexture;
                    camera.Render();
                }

                var tex = new Texture2D(CaptureWidth, CaptureHeight, TextureFormat.RGB24, false);
                RenderTexture.active = renderTexture;
                tex.ReadPixels(new Rect(0, 0, CaptureWidth, CaptureHeight), 0, 0, false);
                tex.Apply(false, false);
                return tex;
            }
            finally
            {
                foreach (KeyValuePair<Camera, RenderTexture> entry in previousTargets)
                {
                    if (entry.Key != null)
                        entry.Key.targetTexture = entry.Value;
                }
                RenderTexture.active = previousActive;
                renderTexture.Release();
                Destroy(renderTexture);
            }
        }

        private static void ConfigureRuntimeOverlayFixtureIfRequested()
        {
            string enabled = Environment.GetEnvironmentVariable(OverlayFixtureEnv);
            if (string.IsNullOrEmpty(enabled) || enabled == "0")
                return;

            Camera cam = Camera.main;
            if (cam == null)
                cam = FindObjectOfType<Camera>();
            if (cam == null)
            {
                Debug.LogError($"{Prefix}overlay fixture failed: no camera found");
                return;
            }

            const int overlayLayer = 30;
            if (cam.clearFlags == CameraClearFlags.SolidColor)
            {
                cam.clearFlags = CameraClearFlags.Skybox;
                Debug.Log($"{Prefix}overlay fixture restored skybox clear for AfterSkybox render event");
            }
            cam.cullingMask = 0;

            GameObject cube = CreateSmokeQuad("IMM Runtime Overlay Fixture Cube");
            cube.layer = overlayLayer;
            cube.transform.position = cam.ViewportToWorldPoint(new Vector3(0.95f, 0.45f, 4.0f));
            cube.transform.rotation = Quaternion.identity;
            cube.transform.localScale = Vector3.one;

            Shader shader = Resources.Load<Shader>("ImmUnitySmokeUnlitColor");
            if (shader == null)
                shader = Shader.Find("Unlit/Color");
            if (shader == null)
                shader = Shader.Find("Standard");
            if (shader == null)
            {
                Debug.LogError($"{Prefix}overlay fixture failed: no shader found");
                return;
            }

            Material material = new Material(shader);
            material.color = new Color(1.0f, 0.05f, 0.02f, 1.0f);
            Renderer renderer = cube.GetComponent<Renderer>();
            if (renderer != null)
                renderer.sharedMaterial = material;

            GameObject overlayCameraObject = new GameObject("IMM Runtime Overlay Fixture Camera");
            Camera overlayCamera = overlayCameraObject.AddComponent<Camera>();
            overlayCamera.CopyFrom(cam);
            overlayCamera.transform.SetPositionAndRotation(cam.transform.position, cam.transform.rotation);
            overlayCamera.clearFlags = CameraClearFlags.Nothing;
            overlayCamera.cullingMask = 1 << overlayLayer;
            overlayCamera.depth = cam.depth + 1.0f;
            overlayCamera.name = "IMM Runtime Overlay Fixture Camera";

            ImmPlayerManager manager = FindObjectOfType<ImmPlayerManager>();
            if (manager != null)
                manager.SetRenderCamera(cam);

            Debug.Log($"{Prefix}overlay fixture created baseCamera={cam.name} overlayCamera={overlayCamera.name} layer={overlayLayer}");
        }

        private bool CreateCompositionProbes()
        {
#if IMM_UNITY_ANDROID_VULKAN_CI
            ImmPlayerManager playerManager = FindObjectOfType<ImmPlayerManager>();
            Camera cam = playerManager != null
                ? playerManager.GetAndroidVulkanSourceCameraForValidation()
                : null;
            if (cam == null)
                cam = Camera.main;
#else
            Camera cam = _overlayProbeEnabled ? FindOverlayCompositionCamera() : Camera.main;
#endif
            if (cam == null)
                return false;

            _compositionCamera = cam;
            int probeLayer = _overlayProbeEnabled ? FirstVisibleLayer(cam.cullingMask, 0) : 0;
            Vector3 forward = cam.transform.forward.normalized;
            Vector3 right = cam.transform.right.normalized;
            Vector3 up = cam.transform.up.normalized;
            Vector3 center = cam.transform.position + forward * 3.0f;
            _frontProbe = CreateProbe("IMM Scene Front Occluder Probe", FrontProbeColor, center - right * 0.70f - up * 0.35f - forward * 1.00f, cam.transform.rotation, new Vector3(0.50f, 0.50f, 0.06f), probeLayer);
            _rearOccludedProbe = CreateProbe("IMM Scene Rear Occlusion Probe", RearOccludedProbeColor, center + forward * 0.95f + right * 0.25f, cam.transform.rotation, new Vector3(0.75f, 0.75f, 0.06f), probeLayer);
            _rearVisibleProbe = CreateProbe("IMM Scene Rear Visible Probe", RearVisibleProbeColor, center + right * 1.30f + up * 0.85f + forward * 0.35f, cam.transform.rotation, new Vector3(0.65f, 0.65f, 0.06f), probeLayer);
            Debug.Log($"{Prefix}scene composition probes created center={center} camera={cam.name} overlay={_overlayProbeEnabled} layer={probeLayer}");
            return true;
        }

        private static Camera FindOverlayCompositionCamera()
        {
            Camera[] cameras = FindObjectsOfType<Camera>();
            Camera best = null;
            for (int i = 0; i < cameras.Length; ++i)
            {
                Camera candidate = cameras[i];
                if (candidate == null || candidate.cullingMask == 0)
                    continue;
                if (best == null || candidate.depth > best.depth)
                    best = candidate;
            }
            return best != null ? best : Camera.main;
        }

        private static int FirstVisibleLayer(int cullingMask, int fallbackLayer)
        {
            for (int layer = 0; layer < 32; ++layer)
            {
                if ((cullingMask & (1 << layer)) != 0)
                    return layer;
            }
            return fallbackLayer;
        }

        private static GameObject CreateProbe(string name, Color color, Vector3 position, Quaternion rotation, Vector3 scale, int layer)
        {
            GameObject probe = CreateSmokeQuad(name);
            probe.layer = layer;
            probe.transform.SetPositionAndRotation(position, rotation);
            probe.transform.localScale = scale;
            var renderer = probe.GetComponent<Renderer>();
            if (renderer == null)
                return probe;

            Shader shader = Resources.Load<Shader>("ImmUnitySmokeUnlitColor");
            if (shader == null)
                shader = Shader.Find("Universal Render Pipeline/Unlit");
            if (shader == null)
                shader = Shader.Find("Unlit/Color");
            if (shader == null)
                shader = Shader.Find("Standard");

            if (shader == null)
            {
                Debug.LogError($"{Prefix}scene composition probe material failed: no shader found for {name}");
                return probe;
            }

            var material = new Material(shader);
            if (material.HasProperty("_BaseColor"))
                material.SetColor("_BaseColor", color);
            if (material.HasProperty("_Color"))
                material.SetColor("_Color", color);
            renderer.sharedMaterial = material;
            Debug.Log($"{Prefix}scene composition probe material {name} shader={shader.name}");
            return probe;
        }

        private static GameObject CreateSmokeQuad(string name)
        {
            var probe = new GameObject(name);
            var meshFilter = probe.AddComponent<MeshFilter>();
            probe.AddComponent<MeshRenderer>();

            var mesh = new Mesh
            {
                name = $"{name} Mesh",
                vertices = new[]
                {
                    new Vector3(-0.5f, -0.5f, 0.0f),
                    new Vector3(-0.5f, 0.5f, 0.0f),
                    new Vector3(0.5f, 0.5f, 0.0f),
                    new Vector3(0.5f, -0.5f, 0.0f)
                },
                triangles = new[] { 0, 1, 2, 0, 2, 3 },
                uv = new[]
                {
                    new Vector2(0.0f, 0.0f),
                    new Vector2(0.0f, 1.0f),
                    new Vector2(1.0f, 1.0f),
                    new Vector2(1.0f, 0.0f)
                }
            };
            mesh.RecalculateNormals();
            mesh.RecalculateBounds();
            meshFilter.sharedMesh = mesh;
            return probe;
        }

        private static CompositionRegionResult AnalyzeProbeRegion(Color32[] pixels, int width, int height, Camera camera, GameObject probe, Color target)
        {
            if (camera == null || probe == null)
                return new CompositionRegionResult("missing", 0, 0, 0.0f, new RectInt(0, 0, 0, 0));

            Transform transform = probe.transform;
            Vector3 center = transform.position;
            Vector3 right = transform.right * (transform.localScale.x * 0.5f);
            Vector3 up = transform.up * (transform.localScale.y * 0.5f);
            Vector3[] corners =
            {
                center - right - up,
                center - right + up,
                center + right - up,
                center + right + up,
            };

            int minX = width;
            int minY = height;
            int maxX = -1;
            int maxY = -1;
            foreach (Vector3 corner in corners)
            {
                Vector3 screen = camera.WorldToScreenPoint(corner);
                if (screen.z <= 0.0f)
                    continue;
                float scaledX = screen.x * width / Mathf.Max(1, camera.pixelWidth);
                float scaledY = screen.y * height / Mathf.Max(1, camera.pixelHeight);
                minX = Mathf.Min(minX, Mathf.FloorToInt(scaledX));
                minY = Mathf.Min(minY, Mathf.FloorToInt(scaledY));
                maxX = Mathf.Max(maxX, Mathf.CeilToInt(scaledX));
                maxY = Mathf.Max(maxY, Mathf.CeilToInt(scaledY));
            }

            if (maxX < minX || maxY < minY)
                return new CompositionRegionResult(probe.name, 0, 0, 0.0f, new RectInt(0, 0, 0, 0));

            const int inset = 3;
            minX = Mathf.Clamp(minX + inset, 0, width - 1);
            maxX = Mathf.Clamp(maxX - inset, 0, width - 1);
            minY = Mathf.Clamp(minY + inset, 0, height - 1);
            maxY = Mathf.Clamp(maxY - inset, 0, height - 1);
            if (maxX < minX || maxY < minY)
                return new CompositionRegionResult(probe.name, 0, 0, 0.0f, new RectInt(0, 0, 0, 0));

            int total = 0;
            int matched = 0;
            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    ++total;
                    if (IsNear(pixels[y * width + x], target))
                        ++matched;
                }
            }

            float share = total > 0 ? (float)matched / total : 0.0f;
            return new CompositionRegionResult(probe.name, total, matched, share, new RectInt(minX, minY, maxX - minX + 1, maxY - minY + 1));
        }

        private static bool IsNear(Color32 pixel, Color target)
        {
            return Mathf.Abs(pixel.r / 255.0f - target.r) <= 0.20f
                && Mathf.Abs(pixel.g / 255.0f - target.g) <= 0.20f
                && Mathf.Abs(pixel.b / 255.0f - target.b) <= 0.20f;
        }

        private static OrderedOverlayImmResult AnalyzeOrderedOverlayImmContent(Color32[] pixels, int width, int height)
        {
            int candidate = 0;
            int topBright = 0;
            int bottomBright = 0;
            int topPaint = 0;
            int bottomPaint = 0;
            int topRightBright = 0;
            int bottomRightBright = 0;
            int topLeftPaint = 0;
            int bottomLeftPaint = 0;
            var colors = new HashSet<int>();
            int[] buckets = new int[64];
            int bucketCount = 0;
            for (int i = 0; i < pixels.Length; ++i)
            {
                Color32 pixel = pixels[i];
                if (!IsLikelyImmOrderedOverlayPixel(pixel))
                    continue;

                ++candidate;
                colors.Add((pixel.r << 16) | (pixel.g << 8) | pixel.b);
                int bucket = ((pixel.r >> 6) << 4) | ((pixel.g >> 6) << 2) | (pixel.b >> 6);
                if (buckets[bucket] == 0)
                {
                    buckets[bucket] = 1;
                    ++bucketCount;
                }

                int x = i % width;
                int y = i / width;
                int displayY = height - 1 - y;
                bool isTopHalf = displayY < height / 2;
                bool isTopRightOrientationRegion = x >= width * 3 / 5 && x < width * 49 / 50 && displayY < height * 7 / 20;
                bool isBottomRightOrientationRegion = x >= width * 3 / 5 && x < width * 49 / 50 && displayY >= height * 13 / 20;
                bool isTopLeftOrientationRegion = x < width * 2 / 5 && displayY < height * 7 / 20;
                bool isBottomLeftOrientationRegion = x < width * 2 / 5 && displayY >= height * 13 / 20;
                if (IsLikelyOrderedOverlayBrightSkyPixel(pixel))
                {
                    if (isTopHalf)
                        ++topBright;
                    else
                        ++bottomBright;
                    if (isTopRightOrientationRegion)
                        ++topRightBright;
                    if (isBottomRightOrientationRegion)
                        ++bottomRightBright;
                }
                if (IsLikelyOrderedOverlayPaintPixel(pixel))
                {
                    if (isTopHalf)
                        ++topPaint;
                    else
                        ++bottomPaint;
                    if (isTopLeftOrientationRegion)
                        ++topLeftPaint;
                    if (isBottomLeftOrientationRegion)
                        ++bottomLeftPaint;
                }
            }

            int total = Math.Max(1, width * height);
            float share = (float)candidate / total;
            return new OrderedOverlayImmResult(
                candidate,
                total,
                share,
                bucketCount,
                colors.Count,
                topBright,
                bottomBright,
                topPaint,
                bottomPaint,
                topRightBright,
                bottomRightBright,
                topLeftPaint,
                bottomLeftPaint);
        }

        private static bool IsLikelyImmOrderedOverlayPixel(Color32 pixel)
        {
            int max = Mathf.Max(pixel.r, Mathf.Max(pixel.g, pixel.b));
            if (max <= 32)
                return false;

            if (IsNear(pixel, FrontProbeColor) ||
                IsNear(pixel, RearOccludedProbeColor) ||
                IsNear(pixel, RearVisibleProbeColor) ||
                IsNear(pixel, new Color(1.0f, 0.05f, 0.02f, 1.0f)))
            {
                return false;
            }

            return true;
        }

        private static bool IsLikelyOrderedOverlayBrightSkyPixel(Color32 pixel)
        {
            int max = Mathf.Max(pixel.r, Mathf.Max(pixel.g, pixel.b));
            int min = Mathf.Min(pixel.r, Mathf.Min(pixel.g, pixel.b));
            float luma = 0.2126f * pixel.r + 0.7152f * pixel.g + 0.0722f * pixel.b;
            return luma > 145.0f && max - min < 110;
        }

        private static bool IsLikelyOrderedOverlayPaintPixel(Color32 pixel)
        {
            return pixel.r >= 45 &&
                pixel.r <= 190 &&
                pixel.r > pixel.g * 1.25f &&
                pixel.r > pixel.b * 1.25f &&
                pixel.g < 120 &&
                pixel.b < 120;
        }

        private void RecordCompositionFailure(string message)
        {
            _compositionFailures.Add(message);
            Debug.LogError($"{Prefix}{message}");
        }

        private static bool IsEnabled(string argName, string envName)
        {
            string value = GetCommandLineValue(argName);
            if (string.IsNullOrEmpty(value))
            {
                value = Environment.GetEnvironmentVariable(envName);
            }
            return IsTruthyValue(value);
        }

        private static bool IsTruthyValue(string value)
        {
            return !string.IsNullOrEmpty(value) && value != "0" && !string.Equals(value, "false", StringComparison.OrdinalIgnoreCase);
        }

        private static bool ValidateXrProbe()
        {
            bool enabled = GetXrSettingsBool("enabled");
            bool deviceActive = GetXrSettingsBool("isDeviceActive");
            string loadedDeviceName = GetXrSettingsString("loadedDeviceName");
            string stereoRenderingMode = GetXrSettingsString("stereoRenderingMode");
            int displayCount = CountXrDisplays(out int runningDisplays);

            Debug.Log($"{Prefix}xr enabled={enabled} deviceActive={deviceActive} loadedDevice={loadedDeviceName} stereoMode={stereoRenderingMode} displays={displayCount} runningDisplays={runningDisplays}");
            if (!enabled)
            {
                Debug.LogError($"{Prefix}xr probe failed: XRSettings.enabled is false");
                return false;
            }
            if (!deviceActive)
            {
                Debug.LogError($"{Prefix}xr probe failed: XRSettings.isDeviceActive is false");
                return false;
            }
            if (runningDisplays <= 0)
            {
                Debug.LogError($"{Prefix}xr probe failed: no running XR display subsystem");
                return false;
            }

            Debug.Log($"{Prefix}xr probe passed");
            return true;
        }

        private static bool ValidateExpectedGraphicsApi()
        {
            string expected = GetCommandLineValue(ExpectedGraphicsApiArg);
            if (string.IsNullOrEmpty(expected))
            {
                expected = Environment.GetEnvironmentVariable(ExpectedGraphicsApiEnv);
            }
#if IMM_UNITY_ANDROID_VULKAN_CI
            expected = "Vulkan";
#endif
            if (string.IsNullOrEmpty(expected))
            {
                return true;
            }

            string actual = SystemInfo.graphicsDeviceType.ToString();
            bool matched = string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase);
            Debug.Log($"{Prefix}graphics api expected={expected} actual={actual}");
            if (!matched)
            {
                Debug.LogError($"{Prefix}graphics api probe failed: expected {expected}, actual {actual}");
            }
            return matched;
        }

        private static bool GetXrSettingsBool(string propertyName)
        {
            object value = GetXrSettingsValue(propertyName);
            return value is bool boolValue && boolValue;
        }

        private static string GetXrSettingsString(string propertyName)
        {
            object value = GetXrSettingsValue(propertyName);
            return value != null ? value.ToString() : "<unavailable>";
        }

        private static object GetXrSettingsValue(string propertyName)
        {
            Type xrSettingsType = Type.GetType("UnityEngine.XR.XRSettings, UnityEngine.XRModule");
            return xrSettingsType?.GetProperty(propertyName, BindingFlags.Public | BindingFlags.Static)?.GetValue(null);
        }

        private static int CountXrDisplays(out int runningDisplays)
        {
            runningDisplays = 0;
            Type displayType = Type.GetType("UnityEngine.XR.XRDisplaySubsystem, UnityEngine.XRModule");
            if (displayType == null)
                return 0;

            Type listType = typeof(List<>).MakeGenericType(displayType);
            object displays = Activator.CreateInstance(listType);
            MethodInfo getInstances = null;
            foreach (MethodInfo method in typeof(SubsystemManager).GetMethods(BindingFlags.Public | BindingFlags.Static))
            {
                if (method.Name == "GetInstances" && method.IsGenericMethodDefinition && method.GetParameters().Length == 1)
                {
                    getInstances = method.MakeGenericMethod(displayType);
                    break;
                }
            }

            getInstances?.Invoke(null, new[] { displays });
            int count = 0;
            PropertyInfo runningProperty = displayType.GetProperty("running");
            foreach (object display in (IEnumerable)displays)
            {
                ++count;
                if (display != null && runningProperty?.GetValue(display) is bool running && running)
                    ++runningDisplays;
            }
            return count;
        }

        private struct CompositionRegionResult
        {
            public CompositionRegionResult(string label, int totalPixels, int matchedPixels, float share, RectInt rect)
            {
                Label = label;
                TotalPixels = totalPixels;
                MatchedPixels = matchedPixels;
                Share = share;
                Rect = rect;
            }

            public string Label { get; }
            public int TotalPixels { get; }
            public int MatchedPixels { get; }
            public float Share { get; }
            public RectInt Rect { get; }

            public override string ToString()
            {
                return $"{Label} rect={Rect.x},{Rect.y},{Rect.width},{Rect.height} total={TotalPixels} matched={MatchedPixels} share={Share:F3}";
            }
        }

        private struct OrderedOverlayImmResult
        {
            public OrderedOverlayImmResult(
                int candidatePixels,
                int totalPixels,
                float share,
                int colorBuckets,
                int uniqueColors,
                int topBrightPixels,
                int bottomBrightPixels,
                int topPaintPixels,
                int bottomPaintPixels,
                int topRightBrightPixels,
                int bottomRightBrightPixels,
                int topLeftPaintPixels,
                int bottomLeftPaintPixels)
            {
                CandidatePixels = candidatePixels;
                TotalPixels = totalPixels;
                Share = share;
                ColorBuckets = colorBuckets;
                UniqueColors = uniqueColors;
                TopBrightPixels = topBrightPixels;
                BottomBrightPixels = bottomBrightPixels;
                TopPaintPixels = topPaintPixels;
                BottomPaintPixels = bottomPaintPixels;
                TopRightBrightPixels = topRightBrightPixels;
                BottomRightBrightPixels = bottomRightBrightPixels;
                TopLeftPaintPixels = topLeftPaintPixels;
                BottomLeftPaintPixels = bottomLeftPaintPixels;
            }

            public int CandidatePixels { get; }
            public int TotalPixels { get; }
            public float Share { get; }
            public int ColorBuckets { get; }
            public int UniqueColors { get; }
            public int TopBrightPixels { get; }
            public int BottomBrightPixels { get; }
            public int TopPaintPixels { get; }
            public int BottomPaintPixels { get; }
            public int TopRightBrightPixels { get; }
            public int BottomRightBrightPixels { get; }
            public int TopLeftPaintPixels { get; }
            public int BottomLeftPaintPixels { get; }
            public float BrightTopToBottomRatio => TopBrightPixels / (float)Math.Max(1, BottomBrightPixels);
            public float PaintTopToBottomRatio => TopPaintPixels / (float)Math.Max(1, BottomPaintPixels);
            public float TopRightBrightToBottomRightRatio => TopRightBrightPixels / (float)Math.Max(1, BottomRightBrightPixels);
            public float BottomLeftPaintToTopLeftRatio => BottomLeftPaintPixels / (float)Math.Max(1, TopLeftPaintPixels);

            public override string ToString()
            {
                return $"candidate={CandidatePixels} total={TotalPixels} share={Share:F4} colorBuckets={ColorBuckets} uniqueColors={UniqueColors} brightTop={TopBrightPixels} brightBottom={BottomBrightPixels} brightTopBottom={BrightTopToBottomRatio:F3} paintTop={TopPaintPixels} paintBottom={BottomPaintPixels} paintTopBottom={PaintTopToBottomRatio:F3} topRightBright={TopRightBrightPixels} bottomRightBright={BottomRightBrightPixels} topRightBrightBottomRight={TopRightBrightToBottomRightRatio:F3} topLeftPaint={TopLeftPaintPixels} bottomLeftPaint={BottomLeftPaintPixels} bottomLeftPaintTopLeft={BottomLeftPaintToTopLeftRatio:F3}";
            }
        }

        private static void QuitIfRequested(int exitCode)
        {
#if IMM_UNITY_ANDROID_VULKAN_CI
            Application.Quit(exitCode);
            return;
#endif
            string quit = GetCommandLineValue(QuitArg);
            if (string.IsNullOrEmpty(quit))
            {
                quit = Environment.GetEnvironmentVariable(QuitEnv);
            }
            if (quit == "1" || string.Equals(quit, "true", StringComparison.OrdinalIgnoreCase))
            {
                Application.Quit(exitCode);
            }
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

        private static int GetPositiveCommandLineInt(string key, int fallback)
        {
            string value = GetCommandLineValue(key);
            return !string.IsNullOrEmpty(value) &&
                int.TryParse(value, out int parsed) &&
                parsed > 0
                ? parsed
                : fallback;
        }
    }
}
