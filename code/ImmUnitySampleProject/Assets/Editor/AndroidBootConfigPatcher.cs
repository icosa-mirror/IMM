using System.IO;
using UnityEditor.Android;
using UnityEngine;

namespace ImmPlayer.Editor
{
    /// <summary>
    /// Patches the Android boot.config so Unity creates a second Vulkan graphics queue.
    /// The IMM native plugin submits its Vulkan work on that queue (family 0, index 1),
    /// which keeps it off Unity's queue mid-frame - touching Unity's queue during an XR
    /// frame makes Unity finalize the frame for external present and breaks compositor
    /// pacing (observed on Quest 3). Adreno 740 exposes 4 graphics queues.
    /// </summary>
    public class AndroidBootConfigPatcher : IPostGenerateGradleAndroidProject
    {
        private const string Key = "xr-request-additional-vulkan-graphics-queue";

        public int callbackOrder => 999;

        public void OnPostGenerateGradleAndroidProject(string basePath)
        {
            string bootConfig = Path.Combine(basePath, "src", "main", "assets", "bin", "Data", "boot.config");
            if (!File.Exists(bootConfig))
            {
                Debug.LogWarning($"[IMM_BOOTCFG] boot.config not found at {bootConfig}; additional Vulkan queue not requested");
                return;
            }

            string[] lines = File.ReadAllLines(bootConfig);
            bool patched = false;
            for (int i = 0; i < lines.Length; i++)
            {
                if (lines[i].StartsWith(Key + "="))
                {
                    lines[i] = Key + "=1";
                    patched = true;
                    break;
                }
            }

            if (patched)
            {
                File.WriteAllLines(bootConfig, lines);
            }
            else
            {
                File.AppendAllText(bootConfig, Key + "=1\n");
            }
            Debug.Log($"[IMM_BOOTCFG] {Key}=1 written to {bootConfig}");
        }
    }
}
