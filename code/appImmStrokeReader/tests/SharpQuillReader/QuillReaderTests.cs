using System;
using System.IO;
using Newtonsoft.Json.Linq;
using SharpQuill;

internal static class QuillReaderTests
{
    public static void Run()
    {
        PictureTypes();
        Console.WriteLine("[IMM_QUILL_READER] All reader regressions passed.");
    }

    private static void PictureTypes()
    {
        foreach (var entry in new[]
        {
            ("2D", PictureType.TwoD),
            ("360_Equirect_Mono", PictureType.ThreeSixty_Equirect_Mono),
            ("360_Equirect_Stereo", PictureType.ThreeSixty_Equirect_Stereo),
            ("360 Equirectangular Stereo", PictureType.ThreeSixty_Equirect_Stereo),
            ("ThreeSixty_Equirect_Mono", PictureType.ThreeSixty_Equirect_Mono)
        })
        {
            var layer = ReadLayer<LayerPicture>("Picture", new JObject
            {
                ["Type"] = entry.Item1,
                ["ViewerLocked"] = true,
                ["DataFileOffset"] = "ffffffffffffffff"
            });
            Require(layer.PictureType == entry.Item2 && layer.ViewerLocked,
                $"Picture type {entry.Item1} was not preserved.");
        }
    }

    private static T ReadLayer<T>(string type, JObject implementation) where T : Layer
    {
        var layer = new JObject
        {
            ["Name"] = "Fixture",
            ["Type"] = type,
            ["Implementation"] = implementation
        };
        return (T)ReadProject(new JObject(), layer).RootLayer.Children[0];
    }

    private static Sequence ReadProject(JObject settings, params JObject[] children)
    {
        string directory = Path.Combine(Path.GetTempPath(), $"imm-quill-reader-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        string scenePath = Path.Combine(directory, "Quill.json");
        string binaryPath = Path.Combine(directory, "Quill.qbin");
        try
        {
            var sequence = new JObject
            {
                ["RootLayer"] = new JObject
                {
                    ["Name"] = "Root",
                    ["Type"] = "Group",
                    ["Implementation"] = new JObject { ["Children"] = new JArray(children) }
                }
            };
            foreach (var setting in settings.Properties())
                sequence[setting.Name] = setting.Value.DeepClone();
            File.WriteAllText(scenePath, new JObject { ["Version"] = 1, ["Sequence"] = sequence }.ToString());
            File.WriteAllBytes(binaryPath, new byte[4]);
            Sequence result = QuillSequenceReader.Read(directory);
            Require(result?.RootLayer != null, "Quill project failed to read.");
            return result;
        }
        finally
        {
            File.Delete(scenePath);
            File.Delete(binaryPath);
            Directory.Delete(directory);
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
