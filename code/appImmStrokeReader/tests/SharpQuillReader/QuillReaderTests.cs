using System;
using System.IO;
using Newtonsoft.Json.Linq;
using SharpQuill;

internal static class QuillReaderTests
{
    public static void Run()
    {
        PictureTypes();
        ViewpointDefaults();
        SpatialSoundSettings();
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

    private static void ViewpointDefaults()
    {
        var minimal = ReadLayer<LayerViewpoint>("Viewpoint", new JObject
        {
            ["Color"] = new JArray(0.6, 0.1, 0.5)
        });
        Require(minimal.Version == 1 && minimal.Sphere.W == 2 && minimal.Exporting,
            "Minimal viewpoint did not receive usable spawn defaults.");
        Require(minimal.AllowTranslationX && minimal.AllowTranslationY && minimal.AllowTranslationZ,
            "Minimal viewpoint unexpectedly restricts movement.");
        Require(Math.Abs(minimal.Color.R - 0.6f) < 0.00001f, "Viewpoint colour was lost.");
        var explicitValues = ReadLayer<LayerViewpoint>("Viewpoint", new JObject
        {
            ["Version"] = 0,
            ["Sphere"] = new JArray(1, 2, 3, 4),
            ["Exporting"] = false,
            ["AllowTranslationX"] = false,
            ["ShowingVolume"] = true,
            ["TypeStr"] = "EyeLevel"
        });
        Require(explicitValues.Version == 0 && explicitValues.Sphere.W == 4 && !explicitValues.Exporting
            && !explicitValues.AllowTranslationX && explicitValues.ShowingVolume
            && explicitValues.TypeStr == "EyeLevel", "Explicit viewpoint settings were replaced by defaults.");
    }

    private static void SpatialSoundSettings()
    {
        foreach (var mode in new[] { SoundAttenuationMode.None, SoundAttenuationMode.Linear, SoundAttenuationMode.Logarithmic })
        {
            var sound = ReadLayer<LayerSound>("Sound", new JObject
            {
                ["DataFileOffset"] = "ffffffffffffffff",
                ["Type"] = "Positional",
                ["Gain"] = 0.4,
                ["Loop"] = true,
                ["Attenuation"] = new JObject { ["Mode"] = mode.ToString(), ["Minimum"] = 0.2, ["Maximum"] = 3.3 },
                ["Modifiers"] = new JArray(new JObject
                {
                    ["Type"] = "Cone", ["AngleIn"] = 0.46, ["AngleBand"] = 0.45, ["AttenOut"] = 0.1
                })
            });
            Require(sound.SoundType == SoundType.Positional && sound.Loop && sound.Gain == 0.4f,
                "Positional sound properties were lost.");
            Require(sound.Attenuation.Mode == mode && sound.Attenuation.Minimum == 0.2f && sound.Attenuation.Maximum == 3.3f,
                $"Sound attenuation {mode} was lost.");
            Require(sound.Modifiers.Count == 1 && sound.Modifiers[0].Type == SoundModifierType.Cone
                && sound.Modifiers[0].AngleIn == 0.46f && sound.Modifiers[0].AngleBand == 0.45f
                && sound.Modifiers[0].AttenOut == 0.1f, "Sound cone parameters were lost.");
        }
        var flat = ReadLayer<LayerSound>("Sound", new JObject { ["DataFileOffset"] = "ffffffffffffffff" });
        Require(flat.SoundType == SoundType.Flat && flat.Attenuation.Mode == SoundAttenuationMode.None
            && flat.Modifiers.Count == 0 && flat.Gain == 1, "Absent sound settings do not retain safe defaults.");
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
