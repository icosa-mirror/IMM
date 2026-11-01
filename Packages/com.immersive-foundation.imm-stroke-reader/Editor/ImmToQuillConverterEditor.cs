using System.IO;
using UnityEditor;
using UnityEngine;
using System;
using System.Linq;
using System.Text;

namespace ImmStrokeReader
{
    public static class ImmToQuillConverterEditor
    {
        private const string LogPrefix = "[IMM2QUILL_EDITOR_20261021] ";

        [MenuItem("IMM/Convert IMM To Quill...")]
        public static void ConvertImmToQuill()
        {
            string defaultInputDir = Path.Combine(Application.dataPath, "StreamingAssets");
            if (!Directory.Exists(defaultInputDir))
            {
                defaultInputDir = Application.dataPath;
            }

            string immPath = EditorUtility.OpenFilePanel("Select IMM file", defaultInputDir, "imm");
            if (string.IsNullOrEmpty(immPath))
            {
                return;
            }

            ConvertImmPathWithDefaultOutput(immPath);
        }

        [MenuItem("IMM/Convert Selected IMM To Quill")]
        public static void ConvertSelectedImmToQuill()
        {
            string immPath = GetSelectedImmAbsolutePath();
            if (string.IsNullOrEmpty(immPath))
            {
                return;
            }

            ConvertImmPathWithDefaultOutput(immPath);
        }

        [MenuItem("IMM/Convert All Selected IMM To Quill")]
        public static void ConvertAllSelectedImmToQuill()
        {
            string[] immPaths = GetSelectedImmAbsolutePaths();
            if (immPaths.Length == 0)
            {
                return;
            }

            bool includePictures = EditorUtility.DisplayDialog(
                "Include picture layers?",
                "Include picture layers in conversion for all selected files?\n\nGroups, paint, supported sound and animation are included. Unsupported content is reported before writing.",
                "Include Pictures",
                "Exclude Pictures");

            int okCount = 0;
            int failCount = 0;
            int skippedCount = 0;
            string lastOutputFolder = null;

            for (int i = 0; i < immPaths.Length; i++)
            {
                string immPath = immPaths[i];
                string outputFolder = GetDefaultOutputFolderForImm(immPath);


                Debug.Log($"{LogPrefix}Batch converting '{immPath}' to '{outputFolder}' includePictures={includePictures}");

                bool ok = false;
                string error = null;
                bool cancelled = false;
                try
                {
                    ok = WriteReviewedProject(immPath, outputFolder, includePictures, out cancelled, out error);
                }
                catch (Exception ex)
                {
                    error = ex.Message;
                }

                if (ok)
                {
                    okCount++;
                    lastOutputFolder = outputFolder;
                }
                else if (cancelled) skippedCount++;
                else
                {
                    failCount++;
                    string details = string.IsNullOrEmpty(error) ? "Conversion returned false." : error;
                    Debug.LogError($"{LogPrefix}Batch conversion failed for '{immPath}': {details}");
                }
            }

            Debug.Log($"{LogPrefix}Batch conversion completed ok={okCount} skipped={skippedCount} failed={failCount}");
            EditorUtility.DisplayDialog("IMM to Quill", $"Batch conversion complete.\n\nWritten: {okCount}\nSkipped: {skippedCount}\nFailed: {failCount}", "OK");
            if (!string.IsNullOrEmpty(lastOutputFolder))
            {
                EditorUtility.RevealInFinder(lastOutputFolder);
            }
        }

        [MenuItem("IMM/Convert Selected IMM To Quill", true)]
        public static bool ValidateConvertSelectedImmToQuill()
        {
            return !string.IsNullOrEmpty(GetSelectedImmAbsolutePath());
        }

        [MenuItem("IMM/Convert All Selected IMM To Quill", true)]
        public static bool ValidateConvertAllSelectedImmToQuill()
        {
            return GetSelectedImmAbsolutePaths().Length > 0;
        }

        private static void ConvertImmPathWithDefaultOutput(string immPath)
        {
            if (string.IsNullOrEmpty(immPath))
            {
                return;
            }

            string outputFolder = GetDefaultOutputFolderForImm(immPath);

            bool includePictures = EditorUtility.DisplayDialog(
                "Include picture layers?",
                "Include picture layers in conversion?\n\nGroups, paint, supported sound and animation are included. Unsupported content is reported before writing.",
                "Include Pictures",
                "Exclude Pictures");

            Debug.Log($"{LogPrefix}Converting '{immPath}' to '{outputFolder}' includePictures={includePictures}");

            bool ok = false;
            string error = null;
            bool cancelled = false;
            try
            {
                ok = WriteReviewedProject(immPath, outputFolder, includePictures, out cancelled, out error);
            }
            catch (System.Exception ex)
            {
                error = ex.Message;
            }

            if (cancelled) return;
            if (!ok)
            {
                string details = string.IsNullOrEmpty(error) ? "Conversion returned false." : error;
                Debug.LogError($"{LogPrefix}Conversion failed: {details}");
                EditorUtility.DisplayDialog("IMM to Quill", $"Conversion failed.\n\n{details}", "OK");
                return;
            }

            Debug.Log($"{LogPrefix}Conversion completed");
            EditorUtility.DisplayDialog("IMM to Quill", "Project written. See IMM-conversion-report.txt in the output folder for conversion limitations.", "OK");
            EditorUtility.RevealInFinder(outputFolder);
        }

        // Kept separate so the write boundary can be exercised without opening the Editor.
        internal static bool WriteReviewedProject(string immPath, string outputFolder, bool includePictures, out bool cancelled, out string error)
        {
            cancelled = false;
            var result = SharpQuillCompat.ReadImmAsHierarchyWithReport(immPath, includePictures);
            var report = new StringBuilder();
            report.AppendLine($"Source: {immPath}");
            report.AppendLine($"Destination: {outputFolder}");
            report.AppendLine($"Conversion limitations: {result.Issues.Count}");
            foreach (var issue in result.Issues)
            {
                string line = $"{issue.Code}: layer {issue.SourceLayerId} '{issue.SourceLayerName}': {issue.Message}";
                report.AppendLine(line);
                Debug.LogWarning($"{LogPrefix}{line}");
            }
            error = null;
            if (!result.Succeeded)
            {
                error = $"Content could not be read. {string.Join("\n", result.Issues.Take(6).Select(x => x.Message))}";
                return false;
            }
            bool hasExistingFiles = Directory.Exists(outputFolder) && Directory.EnumerateFileSystemEntries(outputFolder).Any();
            if (result.IsPartial || hasExistingFiles)
            {
                string summary = string.Join("\n", result.Issues.Take(6).Select(x => $"- {x.Message}"));
                string replacement = hasExistingFiles ? "\n\nExisting project files at this destination will be replaced." : "";
                if (!EditorUtility.DisplayDialog("Review Quill conversion",
                    $"Destination: {outputFolder}\n\n{result.Issues.Count} conversion limitations. Full details are in the Console and will be saved as IMM-conversion-report.txt.\n\n{summary}{replacement}",
                    "Write Project", "Skip"))
                {
                    cancelled = true;
                    return false;
                }
            }
            SharpQuill.QuillSequenceWriter.Write(result.Sequence, outputFolder);
            File.WriteAllText(Path.Combine(outputFolder, "IMM-conversion-report.txt"), report.ToString());
            return true;
        }

        private static string GetDefaultOutputFolderForImm(string immPath)
        {
            string documentsPath = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
            string quillRoot = Path.Combine(documentsPath, "Quill");
            string projectName = Path.GetFileNameWithoutExtension(immPath);
            return Path.Combine(quillRoot, projectName);
        }

        private static string GetSelectedImmAbsolutePath()
        {
            string[] paths = GetSelectedImmAbsolutePaths();
            if (paths.Length != 1)
            {
                return null;
            }

            return paths[0];
        }

        private static string[] GetSelectedImmAbsolutePaths()
        {
            if (Selection.objects == null || Selection.objects.Length == 0)
            {
                return Array.Empty<string>();
            }

            string projectRoot = Path.GetDirectoryName(Application.dataPath);
            if (string.IsNullOrEmpty(projectRoot))
            {
                return Array.Empty<string>();
            }

            var result = new System.Collections.Generic.List<string>();

            for (int i = 0; i < Selection.objects.Length; i++)
            {
                string assetPath = AssetDatabase.GetAssetPath(Selection.objects[i]);
                if (string.IsNullOrEmpty(assetPath) || !assetPath.EndsWith(".imm", StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                string absolutePath = Path.GetFullPath(Path.Combine(projectRoot, assetPath));
                result.Add(absolutePath);
            }

            return result.ToArray();
        }
    }
}
