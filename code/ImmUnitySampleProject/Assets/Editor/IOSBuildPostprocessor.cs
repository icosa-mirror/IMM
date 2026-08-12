#if UNITY_IOS
using System.IO;
using UnityEditor;
using UnityEditor.Callbacks;
using UnityEditor.iOS.Xcode;
using UnityEngine;

namespace ImmPlayer.Editor
{
    public static class IOSBuildPostprocessor
    {
        [PostProcessBuild(100)]
        public static void AddNativeLinkDependencies(BuildTarget target, string buildPath)
        {
            if (target != BuildTarget.iOS)
            {
                return;
            }

            string projectPath = PBXProject.GetPBXProjectPath(buildPath);
            if (!File.Exists(projectPath))
            {
                throw new FileNotFoundException($"Generated iOS Xcode project is missing: {projectPath}", projectPath);
            }

            var project = new PBXProject();
            project.ReadFromFile(projectPath);
            string frameworkTarget = project.GetUnityFrameworkTargetGuid();
            string zlibGuid = project.AddFile(
                "usr/lib/libz.tbd",
                "Frameworks/libz.tbd",
                PBXSourceTree.Sdk);
            project.AddFileToBuild(frameworkTarget, zlibGuid);
            project.WriteToFile(projectPath);

            Debug.Log($"[IMM_UNITY_IOS_LINK_20260812] target=UnityFramework dependency=libz.tbd project={projectPath}");
        }
    }
}
#endif
