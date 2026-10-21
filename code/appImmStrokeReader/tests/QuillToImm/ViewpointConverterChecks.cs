using System;
using System.IO;
using System.Linq;
using ImmPlayer.Authoring;
using Newtonsoft.Json.Linq;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

static class ViewpointConverterChecks
{
    public static void Run(string directory)
    {
        string project = Path.Combine(directory, "viewpoint-source");
        Directory.CreateDirectory(project);
        var sequence = Sequence.CreateDefault();
        sequence.RootLayer.Children.Clear();
        var group = new LayerGroup("Spawns");
        foreach (string name in new[] { "HiddenGizmo", "Floor", "Excluded", "Target" })
        {
            var viewpoint = LayerViewpoint.CreateDefault();
            viewpoint.Name = name;
            viewpoint.TypeStr = name == "Floor" ? "FloorLevel" : "EyeLevel";
            viewpoint.Visible = false;
            viewpoint.Exporting = name != "Excluded";
            viewpoint.Sphere = new SharpQuill.Vector4(1, 2, 3, 4);
            viewpoint.AllowTranslationX = false;
            viewpoint.AllowTranslationY = true;
            viewpoint.AllowTranslationZ = false;
            group.Children.Add(viewpoint);
        }
        sequence.RootLayer.Children.Add(group);
        sequence.DefaultViewpoint = "Root/Spawns/Target";
        QuillSequenceWriter.Write(sequence, project);
        string jsonPath = Path.Combine(project, "Quill.json");
        var source = JObject.Parse(File.ReadAllText(jsonPath));
        JArray Children(JObject document) => (JArray)document["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!["Implementation"]!["Children"]!;
        Children(source)[2]!["Implementation"]!["TypeStr"] = "ExcludedUnknownType";
        Children(source)[2]!["Implementation"]!["DataFileOffset"] = "not-a-preview-offset";
        File.WriteAllText(jsonPath, source.ToString());
        string original = File.ReadAllText(jsonPath);
        string output = Path.Combine(directory, "viewpoints.imm");
        var converted = QuillToImmConverter.ConvertProject(project, output);
        Require(converted.Succeeded && !converted.IsPartial, Describe(converted));
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "viewpoint-converter.log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 4, "Export flags or viewpoint-only output changed.");
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var parent), "Spawn group missing.");
            for (int index = 1; index < 4; index++)
            {
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, index, out var info) && info.type == 8 && info.parentId == parent.id && info.visible == 1,
                    "Viewpoint inclusion followed gizmo visibility or lost hierarchy.");
                Require(info.isDefaultSpawn == (info.name == "Target" ? 1 : 0), "Non-first default viewpoint was lost.");
                Require(Reader.StrokeReader_GetLayerSpawnAreaInfo(doc, index, out var spawn) &&
                    spawn.isFloorLevel == (info.name == "Floor" ? 1 : 0) && spawn.volumeType == 0 &&
                    spawn.volumeOffsetX == 1 && spawn.volumeOffsetY == 2 && spawn.volumeOffsetZ == 3 &&
                    spawn.volumeExtentX == 4 && spawn.locomotion == 2, "Viewpoint tracking/sphere/locomotion mapping changed.");
            }
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }

        var preview = JObject.Parse(original);
        Children(preview)[0]!["Implementation"]!["DataFileOffset"] = "8";
        File.WriteAllText(jsonPath, preview.ToString());
        Reject("OmittedViewpointPreview");
        var partial = QuillToImmConverter.ConvertProject(project, output, allowOmittedContent: true);
        Require(partial.Succeeded && partial.IsPartial && partial.Issues.Any(x => x.Code == "OmittedViewpointPreview"), "Preview omission was not explicit.");
        var missingDefault = JObject.Parse(original);
        missingDefault["Sequence"]!["DefaultViewpoint"] = "Root/Spawns/Excluded";
        File.WriteAllText(jsonPath, missingDefault.ToString());
        Reject("OmittedDefaultViewpoint");
        var fallback = QuillToImmConverter.ConvertProject(project, output, allowOmittedContent: true);
        Require(fallback.Succeeded && fallback.IsPartial && fallback.Issues.Any(x => x.Code == "OmittedDefaultViewpoint"), "Default fallback was not explicit.");
        var unknown = JObject.Parse(original);
        Children(unknown)[0]!["Implementation"]!["TypeStr"] = "UnknownTracking";
        File.WriteAllText(jsonPath, unknown.ToString());
        Reject("ConversionFailed");
        File.WriteAllText(jsonPath, original);
        Console.WriteLine("[IMM_QUILL_VIEWPOINT_TEST] Passed native export flags/gizmo visibility/tracking/sphere/mask/hierarchy/non-first default checks and explicit preview/default fallback diagnostics.");

        void Reject(string code)
        {
            byte[] marker = { 7, 8, 9 };
            File.WriteAllBytes(output, marker);
            var result = QuillToImmConverter.ConvertProject(project, output);
            Require(!result.Succeeded && result.Issues.Any(x => x.Code == code) && File.ReadAllBytes(output).SequenceEqual(marker), Describe(result));
        }
    }

    private static string Describe(QuillToImmConversionResult result) => string.Join("; ", result.Issues.Select(x => $"{x.Code}: {x.Message}"));
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
