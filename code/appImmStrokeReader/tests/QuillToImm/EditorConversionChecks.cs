using System;
using System.IO;
using System.Linq;
using SharpQuill;
using Editor = ImmStrokeReader.ImmToQuillConverterEditor;

// The production menu code is compiled unchanged. Only Editor interaction is
// simulated; this is not a live Unity Editor acceptance test.
namespace UnityEditor
{
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class MenuItem : Attribute
    {
        public MenuItem(string path, bool validate = false) { }
    }
    public static class Selection { public static object[] objects => Array.Empty<object>(); }
    public static class AssetDatabase { public static string GetAssetPath(object value) => ""; }
    public static class EditorUtility
    {
        public static bool Answer;
        public static int Dialogs;
        public static string Message;
        public static bool DisplayDialog(string title, string message, string ok, string cancel = null)
        { Dialogs++; Message = message; return Answer; }
        public static string OpenFilePanel(string title, string directory, string extension) => "";
        public static void RevealInFinder(string path) { }
    }
}

static class EditorConversionChecks
{
    public static void Run(string directory)
    {
        string source = Path.Combine(directory, "sound-opus.imm");
        string output = Path.Combine(directory, "editor-reviewed-project");
        Directory.CreateDirectory(output);
        File.WriteAllText(Path.Combine(output, "Quill.json"), "preserve JSON");
        File.WriteAllText(Path.Combine(output, "Quill.qbin"), "preserve QBIN");
        UnityEditor.EditorUtility.Answer = false;
        UnityEditor.EditorUtility.Dialogs = 0;
        Require(!Editor.WriteReviewedProject(source, output, true, out bool cancelled, out string error) && cancelled && error == null &&
            UnityEditor.EditorUtility.Dialogs == 1 && UnityEditor.EditorUtility.Message.Contains(output) &&
            UnityEditor.EditorUtility.Message.Contains("replaced") &&
            File.ReadAllText(Path.Combine(output, "Quill.json")) == "preserve JSON" &&
            File.ReadAllText(Path.Combine(output, "Quill.qbin")) == "preserve QBIN", "Skipping review changed existing output.");
        string skippedOutput = Path.Combine(directory, "editor-skipped-new-project");
        Require(!Editor.WriteReviewedProject(source, skippedOutput, true, out cancelled, out error) && cancelled &&
            !Directory.Exists(skippedOutput), "Skipping review created an output directory.");
        UnityEditor.EditorUtility.Answer = true;
        Require(Editor.WriteReviewedProject(source, output, true, out cancelled, out error) && !cancelled && error == null,
            "Reviewed write failed.");
        var sequence = QuillSequenceReader.Read(output);
        Require(sequence.RootLayer.Children.OfType<LayerGroup>().Single().Children.OfType<LayerSound>().Count() == 3,
            "Editor used the flat reader or omitted supported sounds.");
        string report = File.ReadAllText(Path.Combine(output, "IMM-conversion-report.txt"));
        Require(report.Contains("LayerOmitted") && report.Contains("SoundMetadataNotPreserved") && report.Contains(source),
            "Written conversion report lost omissions or provenance.");
        byte[] before = File.ReadAllBytes(Path.Combine(output, "Quill.qbin"));
        int dialogs = UnityEditor.EditorUtility.Dialogs;
        Require(!Editor.WriteReviewedProject(Path.Combine(directory, "sound-malformed.imm"), output, true, out cancelled, out error) &&
            !cancelled && !string.IsNullOrEmpty(error) && UnityEditor.EditorUtility.Dialogs == dialogs &&
            File.ReadAllBytes(Path.Combine(output, "Quill.qbin")).SequenceEqual(before), "Failed read replaced project or requested write approval.");
        ImmPlayer.ImmStrokeReader.StrokeReader_End();
        Console.WriteLine("[IMM_EDITOR_QUILL_CONVERSION] Passed reviewed hierarchy/sound write, complete report, skip preservation and failed-read guards; live Editor not exercised.");
    }
    static void Require(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
}
