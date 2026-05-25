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
        private const string Prefix = "[IMM_UNITY_SMOKE] ";

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

        private IEnumerator Start()
        {
            int frameCount = 180;
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

            string fullPath = Path.GetFullPath(_capturePath);
            string dir = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);

            File.WriteAllBytes(fullPath, tex.EncodeToPNG());
            Destroy(tex);

            Debug.Log($"{Prefix}capture={fullPath} width={width} height={height} pixels={pixels.Length} nonZero={nonZero} colorBuckets={colorBuckets} hash={hash}");
            QuitIfRequested(0);
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
