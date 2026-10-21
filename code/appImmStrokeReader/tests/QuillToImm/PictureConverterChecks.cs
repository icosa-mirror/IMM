using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using ImmPlayer.Authoring;
using Newtonsoft.Json.Linq;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

static class PictureConverterChecks
{
    public static void Run(string directory)
    {
        string project = Path.Combine(directory, "picture-source");
        var sequence = Sequence.CreateDefault();
        sequence.DefaultViewpoint = "";
        sequence.RootLayer.Children.Clear();
        sequence.RootLayer.Children.Add(new LayerGroup("RGB"));
        sequence.RootLayer.Children.Add(new LayerGroup("Alpha"));
        sequence.RootLayer.Children.Add(new LayerGroup("Hidden"));
        QuillSequenceWriter.Write(sequence, project);
        string jsonPath = Path.Combine(project, "Quill.json"), binaryPath = Path.Combine(project, "Quill.qbin");
        var source = JObject.Parse(File.ReadAllText(jsonPath));
        var layers = (JArray)source["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]!;
        byte[][] pixels = { new byte[] { 210, 20, 60, 10, 140, 230, 50, 80, 110, 190, 160, 130 }, new byte[] { 210, 20, 60, 0, 10, 140, 230, 100, 50, 80, 110, 200, 190, 160, 130, 255 } };
        using (var binary = new BinaryWriter(File.Create(binaryPath)))
        {
            binary.Write(new byte[8]);
            for (int index = 0; index < 2; index++)
            {
                var layer = (JObject)layers[index]!;
                layer["Type"] = "Picture";
                layer["Implementation"] = new JObject { ["Type"] = index == 0 ? "2D" : "TwoD", ["ViewerLocked"] = index == 1,
                    ["DataFileOffset"] = $"0x{binary.BaseStream.Position:X}" };
                binary.Write(new byte[5]); binary.Write((byte)(index == 0 ? 6 : 7)); binary.Write((ushort)0);
                binary.Write(2u); binary.Write(2u); binary.Write(0u); binary.Write(pixels[index]);
            }
        }
        layers[2]!["Type"] = "Picture"; layers[2]!["Visible"] = false;
        layers[2]!["Implementation"] = new JObject { ["Type"] = "UnknownHidden", ["DataFileOffset"] = "invalid" };
        File.WriteAllText(jsonPath, source.ToString());
        string original = File.ReadAllText(jsonPath);
        byte[] validBinary = File.ReadAllBytes(binaryPath);
        string output = Path.Combine(directory, "quill-pictures.imm");
        var converted = QuillToImmConverter.ConvertProject(project, output);
        Require(converted.Succeeded && !converted.IsPartial && converted.Issues.Any(x => x.Code == "HiddenPictureExcluded"), Describe(converted));
        VerifyProjection(0, 0);
        PictureImportChecks.Run(output, directory);
        ConverterChecks.ComparePaint(output, Path.Combine(directory, "quill-pictures-import-roundtrip.imm"), true, true);
        foreach (bool aliases in new[] { false, true })
        {
            layers[0]!["Implementation"]!["Type"] = aliases ? "360 Equirectangular Mono" : "360_Equirect_Mono";
            layers[1]!["Implementation"]!["Type"] = aliases ? "360 Equirectangular Stereo" : "360_Equirect_Stereo";
            File.WriteAllText(jsonPath, source.ToString());
            var panorama = QuillToImmConverter.ConvertProject(project, output);
            Require(panorama.Succeeded && !panorama.IsPartial &&
                panorama.Issues.Count(x => x.Code == "PanoramaLayoutUnverified") == 2, Describe(panorama));
            VerifyProjection(1, 2);
            PictureImportChecks.Run(output, directory);
            foreach (string field in new[] { "HorizontalFOV", "VerticalFOV" })
            {
                layers[1]!["Implementation"]!["Metadata"] = new JObject { [field] = 180 };
                File.WriteAllText(jsonPath, source.ToString());
                Reject("UnsupportedPictureFov");
                Require(!QuillToImmConverter.ConvertProject(project, output, true).Succeeded,
                    "Omission permission hid unsupported panorama field-of-view metadata.");
            }
            ((JObject)layers[1]!["Implementation"]!).Remove("Metadata");
        }
        source = JObject.Parse(original);
        layers = (JArray)source["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]!;
        File.WriteAllText(jsonPath, original);
        File.WriteAllBytes(binaryPath, validBinary.Take(validBinary.Length - 1).ToArray());
        Reject("ConversionFailed");
        Require(!QuillToImmConverter.ConvertProject(project, output, true).Succeeded, "Omission permission hid truncated pixels.");
        File.WriteAllBytes(binaryPath, validBinary);
        layers[1]!["Implementation"]!["DataFileOffset"] = "0";
        File.WriteAllText(jsonPath, source.ToString()); Reject("ConversionFailed");
        layers[1]!["Implementation"]!["Type"] = "2DDepth";
        File.WriteAllText(jsonPath, source.ToString()); Reject("OmittedLayer");
        var partial = QuillToImmConverter.ConvertProject(project, output, true);
        Require(partial.Succeeded && partial.IsPartial && partial.Issues.Any(x => x.Code == "OmittedLayer"), "Unsupported projection omission was not explicit.");
        File.WriteAllText(jsonPath, original);
        byte[] marker = { 7, 8, 9 }; File.WriteAllBytes(output, marker);
        var limited = QuillToImmConverter.ConvertProject(project, output, false,
            new ImmAuthoringOperationOptions(limits: new ImmAuthoringLimits(maxInputBytes: 1)));
        Require(!limited.Succeeded && File.ReadAllBytes(output).SequenceEqual(marker), "Configured source budget replaced output.");
        var outputLimited = QuillToImmConverter.ConvertProject(project, output, false,
            new ImmAuthoringOperationOptions(limits: new ImmAuthoringLimits(maxOutputBytes: 1)));
        Require(!outputLimited.Succeeded && File.ReadAllBytes(output).SequenceEqual(marker), "Configured output budget replaced output.");
        using (var cancelled = new CancellationTokenSource())
        {
            cancelled.Cancel();
            var result = QuillToImmConverter.ConvertProject(project, output, false,
                new ImmAuthoringOperationOptions(cancellationToken: cancelled.Token));
            Require(!result.Succeeded && result.Issues.Any(x => x.Code == "Cancelled") && File.ReadAllBytes(output).SequenceEqual(marker),
                "Cancelled picture conversion replaced output.");
        }
        Require(!QuillToImmConverter.ConvertAnimatedPaintProject(project, output).Succeeded && File.ReadAllBytes(output).SequenceEqual(marker),
            "Paint-only profile accepted pictures or replaced output.");
        Console.WriteLine("[IMM_QUILL_PICTURES] Passed 2D/mono/stereo structural picture mapping and aliases, distinct upper/lower RGB/RGBA pixels, viewer-lock metadata, import round-trip and rejection/output guards; panorama visual parity unverified.");

        void VerifyProjection(params int[] projections)
        {
            Require(Reader.StrokeReader_Init(null) == 0, "Reader init failed.");
            int doc = Reader.StrokeReader_LoadFromFile(output);
            try
            {
                Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 2, "Picture-only mapping or hidden exclusion failed.");
                for (int index = 0; index < 2; index++)
                {
                    Require(Reader.StrokeReader_GetPictureAssetInfo(doc, index, out var info) && info.contentType == projections[index] &&
                        info.isViewerLocked == index && info.image.width == 2 && info.image.height == 2 && info.image.encoding == 2,
                        "Picture projection/viewer-lock/asset metadata changed.");
                    byte[] actual = new byte[16];
                    var pinned = GCHandle.Alloc(actual, GCHandleType.Pinned);
                    try
                    {
                        Require(Reader.StrokeReader_GetPicturePixelData(doc, index, pinned.AddrOfPinnedObject(), actual.Length) == actual.Length,
                            "Converted picture pixels missing.");
                    }
                    finally { pinned.Free(); }
                    byte[] expected = index == 0 ? new byte[] { 210, 20, 60, 255, 10, 140, 230, 255, 50, 80, 110, 255, 190, 160, 130, 255 } : pixels[1];
                    Require(actual.SequenceEqual(expected), "Converted picture RGB/alpha changed.");
                }
            }
            finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        }

        void Reject(string code)
        {
            byte[] saved = { 7, 8, 9 }; File.WriteAllBytes(output, saved);
            var result = QuillToImmConverter.ConvertProject(project, output);
            Require(!result.Succeeded && result.Issues.Any(x => x.Code == code) && File.ReadAllBytes(output).SequenceEqual(saved), Describe(result));
        }
    }

    private static string Describe(QuillToImmConversionResult result) => string.Join("; ", result.Issues.Select(x => $"{x.Code}: {x.Message}"));
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
