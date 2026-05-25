using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine.Rendering;

namespace ImmPlayer.Editor
{
    public static class BuildAutomation
    {
        private static readonly string[] SmokeScenes =
        {
            "Assets/Scenes/SampleScene.unity"
        };

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

            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.iOS, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.iOS, new[] { GraphicsDeviceType.Metal });

            BuildPlayer(BuildTarget.iOS, outputDir, BuildOptions.Development, "iOS");
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
    }
}
