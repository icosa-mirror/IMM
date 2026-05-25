using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

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

        public static void CaptureDeterministicParityDiagnostics()
        {
            string projectPath = Directory.GetCurrentDirectory();
            string documentPath = Environment.GetEnvironmentVariable("IMM_UNITY_PARITY_DOCUMENT");
            if (string.IsNullOrWhiteSpace(documentPath))
            {
                documentPath = Path.Combine(projectPath, "Assets", "StreamingAssets", "sample1.imm");
            }
            if (!Path.IsPathRooted(documentPath))
            {
                documentPath = Path.GetFullPath(Path.Combine(projectPath, documentPath));
            }
            if (!File.Exists(documentPath))
            {
                throw new FileNotFoundException("IMM parity document was not found.", documentPath);
            }

            GameObject managerObject = new GameObject("IMM Parity Diagnostics Manager");
            ImmPlayerManager manager = managerObject.AddComponent<ImmPlayerManager>();
            ImmDocument document = null;
            try
            {
                if (!manager.Initialize())
                {
                    throw new InvalidOperationException("ImmPlayerManager failed to initialize for parity diagnostics.");
                }

                document = manager.LoadDocument(documentPath);
                if (document == null || !document.IsLoaded)
                {
                    throw new InvalidOperationException($"Failed to load parity document: {documentPath}");
                }

                ImmNativePlugin.GlobalWork(1);
                manager.LogDeterministicMatrixDiagnostics(1, true, document, documentPath);
                UnityEngine.Debug.Log($"[IMM_PARITY] Captured deterministic parity diagnostics for {documentPath}");
            }
            finally
            {
                if (document != null)
                {
                    manager.UnloadDocument(document);
                }
                manager.Shutdown();
                UnityEngine.Object.DestroyImmediate(managerObject);
            }
        }
    }
}
