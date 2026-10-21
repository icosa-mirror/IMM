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
        string binaryPath = Path.Combine(project, "Quill.qbin");
        byte[] originalBinary = File.ReadAllBytes(binaryPath);
        using (var binary = new BinaryWriter(File.Create(binaryPath)))
        {
            binary.Write(originalBinary);
            while (binary.BaseStream.Position < 8) binary.Write((byte)0);
            for (int index = 0; index < 2; index++)
            {
                Children(preview)[index]!["Implementation"]!["DataFileOffset"] = $"0x{binary.BaseStream.Position:X}";
                binary.Write(new byte[5]); binary.Write((byte)(index == 0 ? 7 : 6)); binary.Write((ushort)0);
                binary.Write(8u); binary.Write(8u); binary.Write(0u);
                for (int pixel = 0; pixel < 64; pixel++)
                {
                    binary.Write((byte)220); binary.Write((byte)40); binary.Write((byte)90);
                    if (index == 0) binary.Write((byte)255);
                }
            }
        }
        File.WriteAllText(jsonPath, preview.ToString());
        var withPreview = QuillToImmConverter.ConvertProject(project, output);
        Require(withPreview.Succeeded && !withPreview.IsPartial, Describe(withPreview));
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "viewpoint-preview.log")) == 0, "Reader init failed.");
        doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            for (int index = 1; index <= 2; index++)
                Require(doc > 0 && Reader.StrokeReader_GetSpawnAreaPreviewInfo(doc, index, out var info) &&
                    info.width == 8 && info.height == 8 && info.channels == 3 && info.encoding == 3 && info.sourceVersion == 2,
                    "RGB/RGBA Quill preview did not reach native output.");
            Require(!Reader.StrokeReader_GetSpawnAreaPreviewInfo(doc, 3, out _), "Absent preview became explicit.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        byte[] validBinary = File.ReadAllBytes(binaryPath);
        File.WriteAllBytes(binaryPath, validBinary.Take(validBinary.Length - 1).ToArray());
        Reject("ConversionFailed");
        var allowedMalformed = QuillToImmConverter.ConvertProject(project, output, allowOmittedContent: true);
        Require(!allowedMalformed.Succeeded, "Omission permission hid a malformed preview.");
        validBinary[13] = 99; // Preview format byte, offset 8 + 5.
        File.WriteAllBytes(binaryPath, validBinary);
        Reject("ConversionFailed");
        File.WriteAllBytes(binaryPath, originalBinary);
        Children(preview)[0]!["Implementation"]!["DataFileOffset"] = "FFFFFFFFFFFFFFFF";
        File.WriteAllText(jsonPath, preview.ToString());
        Reject("ConversionFailed");
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
        Console.WriteLine("[IMM_QUILL_VIEWPOINT_TEST] Passed native viewpoint/default mapping, RGB/RGBA preview payloads, malformed image guards and explicit default fallback diagnostics.");

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
