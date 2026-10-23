using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using ImmPlayer.Authoring;
using UnityEditor;
using UnityEngine;

namespace ImmPlayer.Editor
{
    public sealed class QuillToImmConverterWindow : EditorWindow
    {
        private const string LogPrefix = "[IMM_QUILL_EDITOR_20261023] ";
        [SerializeField] private string projectFolder = "";
        [SerializeField] private string outputAsset = "Assets/Converted.imm";
        [SerializeField] private bool allowOmissions;
        [SerializeField] private bool overwrite;
        [SerializeField] private int inputMiB = 256;
        private QuillToImmAssetConversionResult result;
        private Vector2 scroll;

        [MenuItem("IMM/Convert Quill Project...")]
        public static void Open()
        {
            var window = GetWindow<QuillToImmConverterWindow>("Quill to IMM");
            window.minSize = new Vector2(480, 330);
            window.Show();
        }

        private void OnGUI()
        {
            EditorGUILayout.LabelField("Quill to IMM", EditorStyles.boldLabel);
            EditorGUILayout.HelpBox("Choose the folder containing Quill.json and Quill.qbin. The resulting IMM will be imported into Assets.", MessageType.Info);
            EditorGUI.BeginChangeCheck();
            EditorGUILayout.BeginHorizontal();
            projectFolder = EditorGUILayout.TextField("Quill project folder", projectFolder);
            if (GUILayout.Button("Browse...", GUILayout.Width(85)))
            {
                string chosen = EditorUtility.OpenFolderPanel("Choose Quill project", projectFolder, "");
                if (!string.IsNullOrEmpty(chosen)) projectFolder = chosen;
            }
            EditorGUILayout.EndHorizontal();
            EditorGUILayout.BeginHorizontal();
            outputAsset = EditorGUILayout.TextField("IMM output asset", outputAsset);
            if (GUILayout.Button("Browse...", GUILayout.Width(85)))
            {
                string chosen = EditorUtility.SaveFilePanelInProject("Choose IMM output", "Converted", "imm", "Save the converted IMM inside Assets.");
                if (!string.IsNullOrEmpty(chosen)) outputAsset = chosen;
            }
            EditorGUILayout.EndHorizontal();
            allowOmissions = EditorGUILayout.ToggleLeft("Allow partial conversion (omit unsupported content)", allowOmissions);
            overwrite = EditorGUILayout.ToggleLeft("Replace existing IMM output", overwrite);
            inputMiB = Math.Max(1, EditorGUILayout.IntField("Input size limit (MiB)", inputMiB));
            if (EditorGUI.EndChangeCheck()) result = null;
            using (new EditorGUI.DisabledScope(EditorApplication.isPlayingOrWillChangePlaymode))
            {
                if (GUILayout.Button("Convert and import", GUILayout.Height(30))) Convert();
            }
            if (EditorApplication.isPlayingOrWillChangePlaymode)
                EditorGUILayout.HelpBox("Stop Play mode before converting.", MessageType.Info);
            if (result == null) return;
            string status = result.Cancelled ? "Conversion cancelled. Existing output was kept." :
                result.Succeeded ? result.Conversion.IsPartial ? "Converted with omissions." : "Conversion imported successfully." :
                result.OutputPublished ? "IMM saved, but import failed." : "Conversion failed. Existing output was kept.";
            EditorGUILayout.HelpBox(status, result.Cancelled ? MessageType.Info : result.Succeeded && !result.Conversion.IsPartial ? MessageType.Info : MessageType.Warning);
            scroll = EditorGUILayout.BeginScrollView(scroll);
            if (result.Error != null) EditorGUILayout.HelpBox(result.Error, MessageType.Error);
            if (result.Conversion != null)
                foreach (var issue in result.Conversion.Issues)
                    EditorGUILayout.HelpBox($"{issue.SourcePath}: {issue.Message}", issue.Severity == QuillToImmIssueSeverity.Error ? MessageType.Error : MessageType.Info);
            EditorGUILayout.EndScrollView();
            if (result.AssetImported && GUILayout.Button("Select converted asset"))
            {
                Selection.activeObject = AssetDatabase.LoadMainAssetAtPath(result.AssetPath);
                EditorGUIUtility.PingObject(Selection.activeObject);
            }
        }

        private void Convert()
        {
            using (var cancellation = new CancellationTokenSource())
            {
                try
                {
                    var options = new ImmAuthoringOperationOptions(cancellation.Token, new EditorProgress(cancellation),
                        new ImmAuthoringLimits(maxInputBytes: checked((long)inputMiB * ImmAuthoringLimits.MiB)));
                    result = QuillToImmAssetConversion.ConvertAndImport(projectFolder, outputAsset, allowOmissions, overwrite, options);
                    if (result.Conversion != null)
                        foreach (var issue in result.Conversion.Issues)
                            UnityEngine.Debug.Log($"{LogPrefix}{issue.Severity} {issue.Code} {issue.SourcePath}: {issue.Message}");
                    if (result.Error != null) UnityEngine.Debug.LogError($"{LogPrefix}{result.Error}");
                }
                finally { EditorUtility.ClearProgressBar(); }
            }
        }

        private sealed class EditorProgress : IProgress<ImmAuthoringProgress>
        {
            private readonly CancellationTokenSource cancellation;
            private readonly Stopwatch clock = Stopwatch.StartNew();
            private long lastUpdate = -100;
            public EditorProgress(CancellationTokenSource cancellation) { this.cancellation = cancellation; }
            public void Report(ImmAuthoringProgress value)
            {
                if (Application.isBatchMode || clock.ElapsedMilliseconds - lastUpdate < 100) return;
                lastUpdate = clock.ElapsedMilliseconds;
                if (EditorUtility.DisplayCancelableProgressBar("Quill to IMM", value.Message, value.Fraction)) cancellation.Cancel();
            }
        }
    }
}
