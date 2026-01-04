using UnityEngine;
using System.IO;
using System.Linq;

namespace ImmPlayer
{
    /// <summary>
    /// Diagnostic utility to verify IMM Player setup
    /// Add this to a GameObject and check the console output
    /// </summary>
    public class ImmDiagnostics : MonoBehaviour
    {
        
        private const string LogPrefix = "[IMM] ";
        private static void Log(string message) => Debug.Log(LogPrefix + message);
        private static void LogWarning(string message) => Debug.LogWarning(LogPrefix + message);
        private static void LogError(string message) => Debug.LogError(LogPrefix + message);
[ContextMenu("Run Diagnostics")]
        public void RunDiagnostics()
        {
            Log("========================================");
            Log("IMM Player Diagnostics");
            Log("========================================");

            // 1. Check DLL files
            Log("\n1. Checking DLL files...");
            string pluginPath = Path.Combine(Application.dataPath, "Plugins", "x86_64");

            string[] requiredDlls = new string[]
            {
                "ImmUnityPlugin.dll",
                "Audio360.dll",
                "jpeg62.dll",
                "libpng16.dll",
                "ogg.dll",
                "opus.dll",
                "opusenc.dll",
                "vorbis.dll",
                "vorbisenc.dll",
                "zlib1.dll"
            };

            bool allDllsPresent = true;
            foreach (var dll in requiredDlls)
            {
                string dllPath = Path.Combine(pluginPath, dll);
                bool exists = File.Exists(dllPath);
                string status = exists ? "✓" : "✗ MISSING";
                Log($"  {status} {dll}");
                if (!exists) allDllsPresent = false;
            }

            if (allDllsPresent)
                Log("  All DLLs present!");
            else
                LogError("  Some DLLs are missing!");

            // 2. Check Unity settings
            Log("\n2. Checking Unity settings...");
            Log($"  Graphics API: {SystemInfo.graphicsDeviceType}");
            Log($"  Graphics Device: {SystemInfo.graphicsDeviceName}");
            Log($"  Color Space: {QualitySettings.activeColorSpace}");
            Log($"  Platform: {Application.platform}");

            // 3. Check if ImmPlayerManager exists
            Log("\n3. Checking ImmPlayerManager...");
            var manager = FindObjectOfType<ImmPlayerManager>();
            if (manager != null)
            {
                Log("  ✓ ImmPlayerManager found in scene");
                Log($"    GameObject: {manager.gameObject.name}");
            }
            else
            {
                LogWarning("  ✗ ImmPlayerManager not found in scene");
                LogWarning("    Add ImmPlayerManager component to a GameObject!");
            }

            // 4. Check paths
            Log("\n4. Checking paths...");
            Log($"  Data Path: {Application.dataPath}");
            Log($"  Temp Cache: {Application.temporaryCachePath}");
            Log($"  Plugin Path: {pluginPath}");

            Log("\n========================================");
            Log("Diagnostics Complete");
            Log("========================================");
        }

        private void Start()
        {
            // Auto-run diagnostics on start in Editor
            #if UNITY_EDITOR
            RunDiagnostics();
            #endif
        }
    }
}

