using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;

// Build the delivered scene with its saved pipeline/camera/settings. No smoke
// runtime defines or alternate scene setup participate in this measurement.
public static class ImmPerformanceBuild
{
    public static void BuildWindows()
    {
        string output = Environment.GetEnvironmentVariable("IMM_PERFORMANCE_PLAYER_PATH");
        if (string.IsNullOrEmpty(output)) throw new ArgumentException("IMM_PERFORMANCE_PLAYER_PATH is required.");
        output = Path.GetFullPath(output);
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        BuildReport report = BuildPipeline.BuildPlayer(new BuildPlayerOptions
        {
            scenes = new[] { "Assets/Scenes/SampleScene.unity" },
            target = BuildTarget.StandaloneWindows64,
            locationPathName = output,
            options = BuildOptions.None
        });
        if (report.summary.result != BuildResult.Succeeded)
            throw new InvalidOperationException($"Normal sample build failed: {report.summary.result}");
    }
}
