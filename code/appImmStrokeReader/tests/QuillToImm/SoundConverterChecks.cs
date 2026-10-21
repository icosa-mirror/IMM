using System;
using System.IO;
using System.Linq;
using ImmPlayer;
using ImmPlayer.Authoring;
using Newtonsoft.Json.Linq;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

static class SoundConverterChecks
{
    public static void Run(string directory)
    {
        string project = Path.Combine(directory, "sound-source");
        var sequence = Sequence.CreateDefault();
        sequence.DefaultViewpoint = "";
        sequence.RootLayer.Children.Clear();
        foreach (string name in new[] { "Flat", "Cone", "Hidden" }) sequence.RootLayer.Children.Add(new LayerGroup(name));
        QuillSequenceWriter.Write(sequence, project);
        string jsonPath = Path.Combine(project, "Quill.json"), binaryPath = Path.Combine(project, "Quill.qbin");
        var source = JObject.Parse(File.ReadAllText(jsonPath));
        var layers = (JArray)source["Sequence"]["RootLayer"]["Implementation"]["Children"];
        using (var binary = new BinaryWriter(File.Create(binaryPath)))
        {
            binary.Write(new byte[8]);
            for (int index = 0; index < 3; index++)
            {
                var layer = (JObject)layers[index];
                layer["Type"] = "Sound";
                layer["Visible"] = index != 2;
                layer["Animation"]["Keys"]["Visibility"][0]["Value"] = index != 2;
                layer["Implementation"] = new JObject
                {
                    ["DataFileOffset"] = $"{binary.BaseStream.Position:X}", ["Type"] = index == 0 ? "Flat" : "Positional",
                    ["Gain"] = 0.52, ["Loop"] = index != 1,
                    ["Attenuation"] = new JObject { ["Mode"] = index == 0 ? "None" : index == 1 ? "Linear" : "Logarithmic", ["Minimum"] = 0.4, ["Maximum"] = 1.6 },
                    ["Modifiers"] = new JArray(index == 1 ? new JObject { ["Type"] = "Cone", ["AngleIn"] = 0.46, ["AngleBand"] = 0.459, ["AttenOut"] = 0.1 } : new JObject { ["Type"] = "None" })
                };
                layer["Opacity"] = index == 0 ? 1 : 0.75;
                layer["Animation"]["Keys"]["Opacity"] = new JArray(new JObject { ["Time"] = "0", ["Value"] = index == 0 ? 1 : index == 1 ? 0.75 : 0.5, ["Interpolation"] = "None" });
                layer["Animation"]["StartOffset"] = "42";
                layer["Animation"]["Keys"]["Offset"] = new JArray(new JObject { ["Time"] = "0", ["Value"] = 42, ["Interpolation"] = "None" });
                int channels = index == 1 ? 1 : 2;
                binary.Write(new byte[5]); binary.Write((byte)0); binary.Write((byte)channels); binary.Write((byte)16);
                binary.Write(44100u); binary.Write(4410u); binary.Write(0u);
                for (int frame = 0; frame < 4410; frame++)
                    for (int channel = 0; channel < channels; channel++)
                        binary.Write((short)(12000 * Math.Sin(2 * Math.PI * (440 + channel * 220) * frame / 44100)));
            }
        }
        File.WriteAllText(jsonPath, source.ToString());
        string validJson = source.ToString(); byte[] validBinary = File.ReadAllBytes(binaryPath);
        string output = Path.Combine(directory, "quill-sounds.imm");
        var result = QuillToImmConverter.ConvertProject(project, output);
        Require(result.Succeeded && !result.IsPartial, Describe(result));
        Require(Reader.StrokeReader_Init(null) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 3, "Sound-only or hidden sound mapping failed.");
            for (int index = 0; index < 3; index++)
            {
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, index, out var layer) && layer.type == 5, "Sound layer missing.");
                bool offsetFound = false, visibilityFound = false, opacityFound = false;
                for (int keyIndex = 0; Reader.StrokeReader_GetLayerAnimationKey(doc, index, keyIndex, out var key); keyIndex++)
                {
                    if (key.property == 1 && key.timeTicks == 0 && key.floatValue == 0.5f) opacityFound = true;
                    if (key.property == 8 && key.timeTicks == 0 && key.intValue == 42) offsetFound = true;
                    if (key.property == 0 && key.timeTicks == 0 && key.boolValue == (index == 2 ? 0 : 1)) visibilityFound = true;
                }
                // Native import initializes base visibility to true; serialized visibility lives in keys.
                Require(offsetFound && visibilityFound && opacityFound == (index == 2), "Sound visibility/offset keys changed.");
                Require(Reader.StrokeReader_GetSoundAssetInfo(doc, index, out var sound) && sound.encoding == 5 && sound.sampleRate == 48000 &&
                    sound.channels == (index == 1 ? 1 : 2) && sound.byteCount > 0 && sound.sourceDuration == 0 && sound.initiallyPlaying == 0,
                    "Converted sound codec/channel/source metadata changed.");
                Require(sound.properties.spatialType == (index == 0 ? 0 : 2) && sound.properties.gain == 0.52f &&
                    sound.properties.loop == (index == 1 ? 0 : 1) && sound.properties.attenuation == index &&
                    sound.properties.distanceMin == 0.4f && sound.properties.distanceMax == 1.6f &&
                    sound.properties.modifier == (index == 1 ? 1 : 0) && sound.properties.angleInX == (index == 1 ? 0.46f : 0) &&
                    sound.properties.angleBand == (index == 1 ? 0.459f : 0) && sound.properties.attenuationOutside == (index == 1 ? 0.1f : 0), "Sound settings changed.");
            }
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        SoundImportChecks.Run(output, directory);
        ConverterChecks.ComparePaint(output, Path.Combine(directory, "quill-sounds-sound-roundtrip.imm"), true, true);
        byte[] marker = { 7, 8, 9 };
        foreach (Action corrupt in new Action[]
        {
            () => File.WriteAllBytes(binaryPath, validBinary.Take(validBinary.Length - 1).ToArray()),
            () => { byte[] b = (byte[])validBinary.Clone(); b[13] = 1; File.WriteAllBytes(binaryPath, b); },
            () => layers[0]["Implementation"]["DataFileOffset"] = "0",
            () => layers[0]["Implementation"]["Type"] = "Ambisonic",
            () => layers[0]["Implementation"]["Attenuation"]["Mode"] = "Unknown",
            () => ((JArray)layers[0]["Implementation"]["Modifiers"]).Add(new JObject { ["Type"] = "Cone" }),
            () => layers[1]["Implementation"]["Modifiers"][0]["Type"] = "Unknown"
        })
        {
            File.WriteAllBytes(binaryPath, validBinary); source = JObject.Parse(validJson);
            layers = (JArray)source["Sequence"]["RootLayer"]["Implementation"]["Children"];
            corrupt(); File.WriteAllText(jsonPath, source.ToString()); File.WriteAllBytes(output, marker);
            result = QuillToImmConverter.ConvertProject(project, output, true);
            Require(!result.Succeeded && File.ReadAllBytes(output).SequenceEqual(marker), "Malformed sound was accepted or replaced output.");
        }
        File.WriteAllText(jsonPath, validJson); File.WriteAllBytes(binaryPath, validBinary);
        Require(!QuillToImmConverter.ConvertAnimatedPaintProject(project, output).Succeeded && File.ReadAllBytes(output).SequenceEqual(marker), "Paint-only profile accepted sound.");
        Console.WriteLine("[IMM_QUILL_SOUND] Passed sound-only mono/stereo, spatial settings, hidden retention, encoded import round trips and malformed-input/output guards.");
    }
    private static string Describe(QuillToImmConversionResult result) => string.Join("; ", result.Issues.Select(x => $"{x.Code}: {x.Message}"));
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
