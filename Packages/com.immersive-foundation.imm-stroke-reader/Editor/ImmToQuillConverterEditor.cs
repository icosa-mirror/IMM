using System.IO;
using UnityEditor;
using UnityEngine;
using System;

namespace ImmStrokeReader
{
    public static class ImmToQuillConverterEditor
    {
        private const string LogPrefix = "[IMM2QUILL_20260209A] ";

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
                "Include picture layers in conversion for all selected files?\n\nPaint layers are always included.",
                "Include Pictures",
                "Paint Only");

            int okCount = 0;
            int failCount = 0;
            string lastOutputFolder = null;

            for (int i = 0; i < immPaths.Length; i++)
            {
                string immPath = immPaths[i];
                string outputFolder = GetDefaultOutputFolderForImm(immPath);
                Directory.CreateDirectory(outputFolder);
                lastOutputFolder = outputFolder;

                Debug.Log($"{LogPrefix}Batch converting '{immPath}' to '{outputFolder}' includePictures={includePictures}");

                bool ok = false;
                string error = null;
                try
                {
                    ok = SharpQuillCompat.WriteImmAsQuillProject(immPath, outputFolder, includePictures);
                }
                catch (Exception ex)
                {
                    error = ex.Message;
                }

                if (ok)
                {
                    okCount++;
                }
                else
                {
                    failCount++;
                    string details = string.IsNullOrEmpty(error) ? "Conversion returned false." : error;
                    Debug.LogError($"{LogPrefix}Batch conversion failed for '{immPath}': {details}");
                }
            }

            Debug.Log($"{LogPrefix}Batch conversion completed ok={okCount} failed={failCount}");
            EditorUtility.DisplayDialog("IMM to Quill", $"Batch conversion complete.\n\nSuccess: {okCount}\nFailed: {failCount}", "OK");
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
            Directory.CreateDirectory(outputFolder);

            bool includePictures = EditorUtility.DisplayDialog(
                "Include picture layers?",
                "Include picture layers in conversion?\n\nPaint layers are always included.",
                "Include Pictures",
                "Paint Only");

            Debug.Log($"{LogPrefix}Converting '{immPath}' to '{outputFolder}' includePictures={includePictures}");

            bool ok = false;
            string error = null;
            try
            {
                ok = SharpQuillCompat.WriteImmAsQuillProject(immPath, outputFolder, includePictures);
            }
            catch (System.Exception ex)
            {
                error = ex.Message;
            }

            if (!ok)
            {
                string details = string.IsNullOrEmpty(error) ? "Conversion returned false." : error;
                Debug.LogError($"{LogPrefix}Conversion failed: {details}");
                EditorUtility.DisplayDialog("IMM to Quill", $"Conversion failed.\n\n{details}", "OK");
                return;
            }

            Debug.Log($"{LogPrefix}Conversion completed");
            EditorUtility.DisplayDialog("IMM to Quill", "Conversion completed.", "OK");
            EditorUtility.RevealInFinder(outputFolder);
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
