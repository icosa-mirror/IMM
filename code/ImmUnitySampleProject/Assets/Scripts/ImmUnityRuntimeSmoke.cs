using System;
using System.Collections;
using System.IO;
using UnityEngine;

namespace ImmPlayer
{
    public sealed class ImmUnityRuntimeSmoke : MonoBehaviour
    {
        private const string CapturePathEnv = "IMM_UNITY_SMOKE_CAPTURE_PATH";
        private const string FramesEnv = "IMM_UNITY_SMOKE_FRAMES";
        private const string QuitEnv = "IMM_UNITY_SMOKE_QUIT";
        private const string CompositionProbeEnv = "IMM_UNITY_SMOKE_COMPOSITION_PROBE";
        private const string Prefix = "[IMM_UNITY_SMOKE] ";
        private const int MinRegionPixels = 24;
        private const float MinDominantShare = 0.35f;
        private const float MaxOccludedShare = 0.12f;
        private static readonly Color FrontProbeColor = new Color(1.0f, 0.0f, 1.0f, 1.0f);
        private static readonly Color RearOccludedProbeColor = new Color(0.0f, 1.0f, 1.0f, 1.0f);
        private static readonly Color RearVisibleProbeColor = new Color(1.0f, 1.0f, 0.0f, 1.0f);

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void Install()
        {
            string capturePath = Environment.GetEnvironmentVariable(CapturePathEnv);
            if (string.IsNullOrEmpty(capturePath))
                return;

            var go = new GameObject("IMM Unity Runtime Smoke");
            DontDestroyOnLoad(go);
            go.AddComponent<ImmUnityRuntimeSmoke>()._capturePath = capturePath;
        }

        private string _capturePath;
        private bool _compositionProbeEnabled;
        private Camera _compositionCamera;
        private GameObject _frontProbe;
        private GameObject _rearOccludedProbe;
        private GameObject _rearVisibleProbe;

        private IEnumerator Start()
        {
            int frameCount = 180;
            _compositionProbeEnabled = IsEnvEnabled(CompositionProbeEnv);
            if (_compositionProbeEnabled)
            {
                if (!CreateCompositionProbes())
                {
                    Debug.LogError($"{Prefix}failed to create scene composition probes");
                    QuitIfRequested(3);
                    yield break;
                }
            }

            string framesText = Environment.GetEnvironmentVariable(FramesEnv);
            if (!string.IsNullOrEmpty(framesText) && int.TryParse(framesText, out int parsedFrames))
            {
                frameCount = Mathf.Max(1, parsedFrames);
            }

            for (int i = 0; i < frameCount; ++i)
            {
                yield return null;
            }

            yield return new WaitForEndOfFrame();

            int width = Screen.width;
            int height = Screen.height;
            if (width <= 0 || height <= 0)
            {
                Debug.LogError($"{Prefix}invalid screen size {width}x{height}");
                QuitIfRequested(2);
                yield break;
            }

            var tex = new Texture2D(width, height, TextureFormat.RGB24, false);
            tex.ReadPixels(new Rect(0, 0, width, height), 0, 0, false);
            tex.Apply(false, false);

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
                    Debug.LogError($"{Prefix}scene composition front occluder failed: {front}");
                    QuitIfRequested(4);
                    yield break;
                }
                if (rearVisible.TotalPixels < MinRegionPixels || rearVisible.Share < MinDominantShare)
                {
                    Debug.LogError($"{Prefix}scene composition rear visible probe failed: {rearVisible}");
                    QuitIfRequested(4);
                    yield break;
                }
                if (rearOccluded.TotalPixels < MinRegionPixels || rearOccluded.Share > MaxOccludedShare)
                {
                    Debug.LogError($"{Prefix}scene composition rear occlusion probe failed: {rearOccluded}");
                    QuitIfRequested(4);
                    yield break;
                }
                Debug.Log($"{Prefix}scene composition probe passed");
            }

            string fullPath = Path.GetFullPath(_capturePath);
            string dir = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);

            File.WriteAllBytes(fullPath, tex.EncodeToPNG());
            Destroy(tex);

            Debug.Log($"{Prefix}capture={fullPath} width={width} height={height} pixels={pixels.Length} nonZero={nonZero} colorBuckets={colorBuckets} hash={hash}");
            QuitIfRequested(0);
        }

        private bool CreateCompositionProbes()
        {
            Camera cam = Camera.main;
            if (cam == null)
                return false;

            _compositionCamera = cam;
            Vector3 forward = cam.transform.forward.normalized;
            Vector3 right = cam.transform.right.normalized;
            Vector3 center = cam.transform.position + forward * 3.0f;
            _frontProbe = CreateProbe("IMM Scene Front Occluder Probe", FrontProbeColor, center - right * 0.50f - forward * 0.35f, cam.transform.rotation, new Vector3(0.55f, 0.55f, 0.06f));
            _rearOccludedProbe = CreateProbe("IMM Scene Rear Occlusion Probe", RearOccludedProbeColor, center - forward * 0.95f, cam.transform.rotation, new Vector3(0.75f, 0.75f, 0.06f));
            _rearVisibleProbe = CreateProbe("IMM Scene Rear Visible Probe", RearVisibleProbeColor, center + right * 0.75f + forward * 0.45f, cam.transform.rotation, new Vector3(0.65f, 0.65f, 0.06f));
            Debug.Log($"{Prefix}scene composition probes created center={center}");
            return true;
        }

        private static GameObject CreateProbe(string name, Color color, Vector3 position, Quaternion rotation, Vector3 scale)
        {
            GameObject probe = GameObject.CreatePrimitive(PrimitiveType.Cube);
            probe.name = name;
            probe.transform.SetPositionAndRotation(position, rotation);
            probe.transform.localScale = scale;
            var renderer = probe.GetComponent<Renderer>();
            if (renderer == null)
                return probe;

            Shader shader = Shader.Find("Unlit/Color");
            if (shader == null)
                shader = Shader.Find("Standard");
            var material = new Material(shader);
            material.color = color;
            renderer.sharedMaterial = material;
            return probe;
        }

        private static CompositionRegionResult AnalyzeProbeRegion(Color32[] pixels, int width, int height, Camera camera, GameObject probe, Color target)
        {
            if (camera == null || probe == null)
                return new CompositionRegionResult("missing", 0, 0, 0.0f, RectInt.zero);

            Renderer renderer = probe.GetComponent<Renderer>();
            Bounds bounds = renderer != null ? renderer.bounds : new Bounds(probe.transform.position, Vector3.one * 0.25f);
            Vector3 min = bounds.min;
            Vector3 max = bounds.max;
            Vector3[] corners =
            {
                new Vector3(min.x, min.y, min.z),
                new Vector3(min.x, min.y, max.z),
                new Vector3(min.x, max.y, min.z),
                new Vector3(min.x, max.y, max.z),
                new Vector3(max.x, min.y, min.z),
                new Vector3(max.x, min.y, max.z),
                new Vector3(max.x, max.y, min.z),
                new Vector3(max.x, max.y, max.z),
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
                minX = Mathf.Min(minX, Mathf.FloorToInt(screen.x));
                minY = Mathf.Min(minY, Mathf.FloorToInt(screen.y));
                maxX = Mathf.Max(maxX, Mathf.CeilToInt(screen.x));
                maxY = Mathf.Max(maxY, Mathf.CeilToInt(screen.y));
            }

            if (maxX < minX || maxY < minY)
                return new CompositionRegionResult(probe.name, 0, 0, 0.0f, RectInt.zero);

            const int inset = 3;
            minX = Mathf.Clamp(minX + inset, 0, width - 1);
            maxX = Mathf.Clamp(maxX - inset, 0, width - 1);
            minY = Mathf.Clamp(minY + inset, 0, height - 1);
            maxY = Mathf.Clamp(maxY - inset, 0, height - 1);
            if (maxX < minX || maxY < minY)
                return new CompositionRegionResult(probe.name, 0, 0, 0.0f, RectInt.zero);

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

        private static bool IsEnvEnabled(string name)
        {
            string value = Environment.GetEnvironmentVariable(name);
            return !string.IsNullOrEmpty(value) && value != "0" && !string.Equals(value, "false", StringComparison.OrdinalIgnoreCase);
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

        private static void QuitIfRequested(int exitCode)
        {
            string quit = Environment.GetEnvironmentVariable(QuitEnv);
            if (quit == "1" || string.Equals(quit, "true", StringComparison.OrdinalIgnoreCase))
            {
                Application.Quit(exitCode);
            }
        }
    }
}
