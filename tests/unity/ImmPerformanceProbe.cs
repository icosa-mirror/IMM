using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using ImmPlayer;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Debug = UnityEngine.Debug;

// Installed only in temporary validation projects. Observe the normal scene;
// Keep the authored workload/camera. Hidden CI windows can opt into one full-size
// render request per engine frame; label that mode separately from window FPS.
public sealed class ImmPerformanceProbe : MonoBehaviour
{
    [Serializable]
    private sealed class Configuration
    {
        public double warmup_seconds = 2;
        public double requested_seconds = 5;
        public bool quit_after_sample = true;
    }

    [Serializable]
    private sealed class Sample
    {
        public string schema = "imm-frame-sample-v1";
        public string product = "unity";
        public string platform;
        public string graphics_api;
        public string gpu;
        public string engine_version;
        public string sampling_method;
        public int msaa_samples;
        public int requested_msaa_samples;
        public int width, height;
        public int vsync_count, target_frame_rate;
        public double warmup_seconds, requested_seconds, observed_seconds;
        public int engine_frame_count, imm_rendered_frame_count;
        public double[] intervals_ms;
        public string[] errors;
    }

    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly List<double> intervals = new List<double>();
    private readonly List<string> errors = new List<string>();
    private Configuration config;
    private Sample sample;
    private string output;
    private Camera camera;
    private double readyAt = -1, sampleAt = -1, previousRender;
    private int firstFrame, lastSubmittedFrame = -1;
    private bool finished;
    private RenderTexture renderTarget, originalTarget;
    private bool originalEnabled;
    private readonly bool renderRequests = Environment.GetEnvironmentVariable("IMM_PERFORMANCE_RENDER_REQUESTS") == "1";
    private UniversalRenderPipeline.SingleCameraRequest renderRequest;

#if IMM_UNITY_ANDROID_URP_CI
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void EnableAndroidCiSample()
    {
        Environment.SetEnvironmentVariable("IMM_PERFORMANCE_OUTPUT",
            Path.Combine(Application.persistentDataPath, "imm-ci", "performance-measurements-raw.json"));
        Environment.SetEnvironmentVariable("IMM_PERFORMANCE_RENDER_REQUESTS", "1");
        Environment.SetEnvironmentVariable("IMM_UNITY_PERFORMANCE_PENDING", "1");
    }
#endif

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Install()
    {
        var asset = Resources.Load<TextAsset>("ImmPerformanceProbe");
        string path = Environment.GetEnvironmentVariable("IMM_PERFORMANCE_OUTPUT");
        if (asset == null && string.IsNullOrEmpty(path)) return;
        var probe = new GameObject("IMM CI Performance Probe").AddComponent<ImmPerformanceProbe>();
        DontDestroyOnLoad(probe.gameObject);
        probe.config = asset != null ? JsonUtility.FromJson<Configuration>(asset.text) : new Configuration();
#if IMM_UNITY_ANDROID_URP_CI
        // Resume the existing Android correctness probe after sampling.
        probe.config.quit_after_sample = false;
#endif
        probe.output = string.IsNullOrEmpty(path)
            ? Path.Combine(Application.persistentDataPath, "imm-ci", "performance-measurements-raw.json") : path;
        probe.config.warmup_seconds = Duration("IMM_PERFORMANCE_WARMUP_SECONDS", probe.config.warmup_seconds);
        probe.config.requested_seconds = Duration("IMM_PERFORMANCE_SAMPLE_SECONDS", probe.config.requested_seconds);
        probe.sample = new Sample
        {
            platform = Application.platform.ToString(), graphics_api = SystemInfo.graphicsDeviceType.ToString(),
            gpu = SystemInfo.graphicsDeviceName, engine_version = Application.unityVersion,
            sampling_method = probe.renderRequests ? "paced-render-requests" : "normal-rendered-workload-frames",
            vsync_count = QualitySettings.vSyncCount, target_frame_rate = Application.targetFrameRate,
            warmup_seconds = probe.config.warmup_seconds, requested_seconds = probe.config.requested_seconds
        };
        ImmRenderingDiagnostics.SubmissionCompleted += probe.OnSubmission;
        Application.logMessageReceived += probe.OnLog;
        if (!ValidDuration(probe.config.warmup_seconds) || !ValidDuration(probe.config.requested_seconds))
            probe.Finish("Invalid sampling duration; expected a number in (0, 30].");
    }

    private static bool ValidDuration(double value) => value > 0 && value <= 30;
    private static double Duration(string name, double fallback)
    {
        string text = Environment.GetEnvironmentVariable(name);
        return text == null ? fallback : double.TryParse(text, NumberStyles.Float,
            CultureInfo.InvariantCulture, out double value) ? value : -1;
    }

    private void Update()
    {
        if (finished) return;
        double now = clock.Elapsed.TotalSeconds;
        if (sampleAt < 0)
        {
            var player = FindFirstObjectByType<ImmUrpSample>();
            var examples = FindFirstObjectByType<ImmFeatureExamples>();
            bool ready = player != null ? player.IsReady : examples != null && examples.IsDocumentRenderReady;
            camera = player != null ? player.DocumentCamera : Camera.main;
            if (renderRequests && camera != null && renderTarget == null)
            {
                originalTarget = camera.targetTexture;
                originalEnabled = camera.enabled;
                int msaa = (GraphicsSettings.currentRenderPipeline as UniversalRenderPipelineAsset)?.msaaSampleCount ?? 1;
                var descriptor = new RenderTextureDescriptor(1280, 720)
                {
                    graphicsFormat = SystemInfo.GetGraphicsFormat(DefaultFormat.LDR),
                    depthStencilFormat = SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil),
                    msaaSamples = msaa
                };
                descriptor.msaaSamples = SystemInfo.GetRenderTextureSupportedMSAASampleCount(descriptor);
                renderTarget = new RenderTexture(descriptor);
                if (!renderTarget.Create()) { Finish("Could not create the CI rendering surface."); return; }
                camera.targetTexture = renderTarget;
                camera.enabled = false;
                sample.requested_msaa_samples = msaa;
                sample.msaa_samples = descriptor.msaaSamples;
                renderRequest = new UniversalRenderPipeline.SingleCameraRequest { destination = renderTarget };
            }
            if (ready && camera != null)
            {
                if (readyAt < 0) readyAt = now;
                if (now - readyAt >= config.warmup_seconds)
                {
                    sampleAt = previousRender = now;
                    firstFrame = Time.frameCount;
                    sample.width = camera.scaledPixelWidth;
                    sample.height = camera.scaledPixelHeight;
                }
            }
            else readyAt = -1;
            if (sampleAt < 0 && now >= 90) Finish("Normal sample did not become ready within 90 seconds.");
            return;
        }
        sample.observed_seconds = now - sampleAt;
        ++sample.engine_frame_count;
        if (camera == null || sample.width != camera.scaledPixelWidth || sample.height != camera.scaledPixelHeight)
            Finish("Rendering resolution or camera changed during sampling.");
        else if (sample.observed_seconds >= config.requested_seconds) Finish(null);
    }

    private void LateUpdate()
    {
        // Readiness can require native GPU loading. Drive it before warm-up too.
        // One request per real engine tick; no per-frame readback or GPU wait.
        if (!finished && renderRequests && camera != null && renderTarget != null)
            RenderPipeline.SubmitRenderRequest(camera, renderRequest);
    }

    private void OnSubmission(ImmSceneSubmission submission)
    {
        if (finished || sampleAt < 0 || submission.Camera != camera ||
            submission.FrameIndex < firstFrame || submission.FrameIndex <= lastSubmittedFrame) return;
        if (submission.Result != 0 || submission.NativeSceneEvents != 1)
        {
            errors.Add($"Native submission failed: result={submission.Result}, events={submission.NativeSceneEvents}.");
            return;
        }
        if (ImmPlayerManager.Instance == null || ImmPlayerManager.Instance.GetPerformanceInfo().numDrawCalls <= 0) return;
        lastSubmittedFrame = submission.FrameIndex;
        double now = clock.Elapsed.TotalSeconds;
        intervals.Add(1000 * (now - previousRender));
        previousRender = now;
        ++sample.imm_rendered_frame_count;
    }

    private void OnLog(string message, string stack, LogType type)
    {
        if (!finished && (type == LogType.Error || type == LogType.Exception || type == LogType.Assert))
            errors.Add(message);
    }

    private void Finish(string error)
    {
        if (finished) return;
        finished = true;
        ImmRenderingDiagnostics.SubmissionCompleted -= OnSubmission;
        Application.logMessageReceived -= OnLog;
        if (error != null) errors.Add(error);
        sample.intervals_ms = intervals.ToArray();
        sample.errors = errors.ToArray();
        string directory = Path.GetDirectoryName(Path.GetFullPath(output));
        Directory.CreateDirectory(directory);
        File.WriteAllText(output, JsonUtility.ToJson(sample, true));
        if (renderTarget != null && camera != null)
        {
            camera.targetTexture = originalTarget;
            camera.enabled = originalEnabled;
            Destroy(renderTarget);
        }
        Debug.Log($"[IMM-FPS-UNITY] Wrote {output}; IMM frames={sample.imm_rendered_frame_count}, seconds={sample.observed_seconds:F3}.");
        Environment.SetEnvironmentVariable("IMM_UNITY_PERFORMANCE_PENDING", "0");
        if (config.quit_after_sample && !Application.isEditor) Application.Quit(errors.Count == 0 ? 0 : 1);
    }

    private void OnApplicationQuit()
    {
        if (!finished) Finish("Player stopped before sampling completed.");
    }
}
