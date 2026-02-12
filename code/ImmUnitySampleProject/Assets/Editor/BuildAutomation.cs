using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;

namespace ImmPlayer.Editor
{
    public static class BuildAutomation
    {
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
    }
}
