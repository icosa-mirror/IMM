using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Newtonsoft.Json.Linq;
using SharpQuill;

internal static class QuillReaderTests
{
    public static void Run(string corpusPath = null)
    {
        PictureTypes();
        ViewpointDefaults();
        SpatialSoundSettings();
        StartupSettings();
        EmbeddedSound();
        MetadataDoesNotReadPayloads();
        if (corpusPath != null) CheckCorpus(corpusPath);
        Console.WriteLine("[IMM_QUILL_READER] All reader regressions passed.");
    }

    private static void MetadataDoesNotReadPayloads()
    {
        string directory = Path.Combine(Path.GetTempPath(), $"imm-quill-metadata-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        string jsonPath = Path.Combine(directory, "Quill.json");
        string binaryPath = Path.Combine(directory, "Quill.qbin");
        var document = new JObject
        {
            ["Version"] = 1,
            ["Sequence"] = new JObject
            {
                ["RootLayer"] = new JObject
                {
                    ["Name"] = "Root", ["Type"] = "Group",
                    ["Implementation"] = new JObject
                    {
                        ["Children"] = new JArray(new JObject
                        {
                            ["Name"] = "Paint", ["Type"] = "Paint",
                            ["Implementation"] = new JObject
                            {
                                ["Framerate"] = 12,
                                ["Drawings"] = new JArray(new JObject
                                {
                                    ["BoundingBox"] = new JArray(0, 1, 0, 1, 0, 1),
                                    ["DataFileOffset"] = "1234"
                                }),
                                ["Frames"] = new JArray(0, 0)
                            }
                        })
                    }
                }
            }
        };
        try
        {
            File.WriteAllText(jsonPath, document.ToString());
            File.WriteAllBytes(binaryPath, Array.Empty<byte>());
            // An exclusive lock proves metadata access does not open qbin at all.
            using (var locked = new FileStream(binaryPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
            {
                var sequence = QuillSequenceReader.ReadMetadata(directory);
                var paint = sequence?.RootLayer.Children.Single() as LayerPaint;
                Require(paint != null && paint.Framerate == 12 && paint.Drawings.Count == 1
                    && paint.Drawings[0].DataFileOffset == 0x1234 && paint.Frames.SequenceEqual(new[] { 0, 0 }),
                    "Metadata read lost drawing declarations, held frames or layer rate.");
                Require(paint.Drawings[0].Data.Strokes.Count == 0, "Metadata read loaded paint data.");
            }
            try { QuillSequenceReader.Read(directory); }
            catch (EndOfStreamException) { return; }
            throw new InvalidOperationException("Full read must still detect the truncated qbin.");
        }
        finally
        {
            File.Delete(jsonPath);
            File.Delete(binaryPath);
            Directory.Delete(directory);
        }
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

    private static void StartupSettings()
    {
        foreach (bool animate in new[] { false, true })
        {
            var sequence = ReadProject(new JObject { ["TrackingOrigin"] = 1, ["AnimateOnStart"] = animate });
            Require(sequence.TrackingOrigin == 1 && sequence.AnimateOnStart == animate,
                "Explicit startup metadata was lost.");
            RoundTripStartupSettings(sequence);
        }
        var legacy = ReadProject(new JObject());
        Require(legacy.TrackingOrigin == null && legacy.AnimateOnStart == null,
            "Unspecified startup metadata acquired an invented default.");
        RoundTripStartupSettings(legacy);
    }

    private static void RoundTripStartupSettings(Sequence sequence)
    {
        string directory = Path.Combine(Path.GetTempPath(), $"imm-quill-startup-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            QuillSequenceWriter.Write(sequence, directory);
            var json = JObject.Parse(File.ReadAllText(Path.Combine(directory, "Quill.json")))["Sequence"];
            Require((json["AnimateOnStart"] != null) == sequence.AnimateOnStart.HasValue
                && (json["TrackingOrigin"] != null) == sequence.TrackingOrigin.HasValue,
                "Writer did not preserve optional metadata presence.");
            var reread = QuillSequenceReader.Read(directory);
            Require(reread != null && reread.AnimateOnStart == sequence.AnimateOnStart
                && reread.TrackingOrigin == sequence.TrackingOrigin, "Startup settings failed to round-trip.");
        }
        finally
        {
            foreach (string name in new[] { "Quill.json", "Quill.qbin", "State.json" })
                File.Delete(Path.Combine(directory, name));
            Directory.Delete(directory);
        }
    }

    private static void EmbeddedSound()
    {
        byte[] pcm = { 0x00, 0x80, 0xff, 0x7f, 0x34, 0x12, 0xcc, 0xed };
        foreach (byte channels in new byte[] { 1, 2 })
        {
            byte[] payload = SoundPayload(channels, pcm);
            using var reader = new QBinReader(new MemoryStream(payload));
            SoundData sound = reader.ReadSoundData();
            byte[] wav = sound.AudioBytes;
            Require(sound.Length == 44 + pcm.Length && wav.Length == sound.Length,
                "Embedded PCM did not produce a complete WAV file.");
            Require(Encoding.ASCII.GetString(wav, 0, 4) == "RIFF"
                && Encoding.ASCII.GetString(wav, 8, 4) == "WAVE"
                && BitConverter.ToUInt16(wav, 20) == 1
                && BitConverter.ToUInt16(wav, 22) == channels
                && BitConverter.ToUInt32(wav, 24) == 44100
                && BitConverter.ToUInt16(wav, 34) == 16
                && BitConverter.ToUInt32(wav, 40) == pcm.Length, "WAV metadata is incorrect.");
            Require(wav.Skip(44).SequenceEqual(pcm), "PCM sample bytes changed.");
            Require(reader.BaseStream.Position == 20 + pcm.Length && reader.ReadUInt32() == 0x12345678,
                "Sound reader consumed data from the next asset.");
        }
        byte[] truncated = SoundPayload(2, pcm).Take(20 + pcm.Length - 1).ToArray();
        ExpectSoundError<EndOfStreamException>(truncated);
        byte[] tooLarge = SoundPayload(2, pcm);
        Array.Copy(BitConverter.GetBytes(uint.MaxValue), 0, tooLarge, 12, 4);
        ExpectSoundError<InvalidDataException>(tooLarge);
        byte[] unsupported = SoundPayload(2, pcm);
        unsupported[7] = 32;
        ExpectSoundError<NotSupportedException>(unsupported);
        byte[] invalidChannels = SoundPayload(2, pcm);
        invalidChannels[6] = 0;
        ExpectSoundError<InvalidDataException>(invalidChannels);
    }

    private static byte[] SoundPayload(byte channels, byte[] pcm)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        writer.Write(0u);
        writer.Write((byte)0);
        writer.Write((byte)0);
        writer.Write(channels);
        writer.Write((byte)16);
        writer.Write(44100u);
        writer.Write((uint)(pcm.Length / (channels * 2)));
        writer.Write(0u);
        writer.Write(pcm);
        writer.Write(0x12345678u);
        return stream.ToArray();
    }

    private static void ExpectSoundError<T>(byte[] payload) where T : Exception
    {
        using var reader = new QBinReader(new MemoryStream(payload));
        try { reader.ReadSoundData(); }
        catch (T) { return; }
        throw new InvalidOperationException($"Expected {typeof(T).Name} for malformed sound.");
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
        return ReadDocument(new JObject { ["Version"] = 1, ["Sequence"] = sequence });
    }

    private static Sequence ReadDocument(JObject document)
    {
        string directory = Path.Combine(Path.GetTempPath(), $"imm-quill-reader-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        string scenePath = Path.Combine(directory, "Quill.json");
        string binaryPath = Path.Combine(directory, "Quill.qbin");
        try
        {
            File.WriteAllText(scenePath, document.ToString());
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

    private static IEnumerable<JObject> SourceLayers(JObject root)
    {
        yield return root;
        if (root["Implementation"]?["Children"] is JArray children)
            foreach (JObject child in children)
                foreach (var layer in SourceLayers(child)) yield return layer;
    }

    private static IEnumerable<Layer> ParsedLayers(Layer root)
    {
        yield return root;
        if (root is LayerGroup group)
            foreach (var child in group.Children)
                foreach (var layer in ParsedLayers(child)) yield return layer;
    }

    private static void CheckCorpus(string corpusPath)
    {
        int projects = 0, sounds = 0, pictures = 0;
        foreach (string path in Directory.EnumerateFiles(corpusPath, "Quill.json", SearchOption.AllDirectories))
        {
            var original = JObject.Parse(File.ReadAllText(path));
            // Parse real hierarchy/metadata without allocating every paint point or image.
            var sequence = QuillSequenceReader.ReadMetadata(Path.GetDirectoryName(path));
            Require(sequence?.RootLayer != null, $"Metadata read failed for {Path.GetDirectoryName(path)}.");
            var expected = SourceLayers((JObject)original["Sequence"]["RootLayer"]).ToArray();
            var parsed = ParsedLayers(sequence.RootLayer).ToArray();
            Require(expected.Length == parsed.Length, $"Layer count changed for {Path.GetDirectoryName(path)}.");
            Require(sequence.AnimateOnStart == original["Sequence"]["AnimateOnStart"]?.ToObject<bool?>()
                && sequence.TrackingOrigin == original["Sequence"]["TrackingOrigin"]?.ToObject<int?>(),
                "Corpus startup settings changed.");
            using var binary = new QBinReader(File.OpenRead(Path.Combine(Path.GetDirectoryName(path), "Quill.qbin")));
            for (int i = 0; i < parsed.Length; ++i)
            {
                var impl = expected[i]["Implementation"];
                if (parsed[i] is LayerPicture picture)
                {
                    string type = (string)impl["Type"];
                    Require((type == "2D" && picture.PictureType == PictureType.TwoD)
                        || (type == "360_Equirect_Stereo" && picture.PictureType == PictureType.ThreeSixty_Equirect_Stereo),
                        $"Unrecognized corpus picture {type}.");
                    Require(picture.ViewerLocked == impl["ViewerLocked"].ToObject<bool>(), "Picture viewer lock changed.");
                    ++pictures;
                }
                if (!(parsed[i] is LayerSound sound)) continue;
                Require(sound.SoundType.ToString() == (string)impl["Type"]
                    && sound.Attenuation.Mode.ToString() == (string)impl["Attenuation"]["Mode"]
                    && sound.Attenuation.Minimum == impl["Attenuation"]["Minimum"].ToObject<float>()
                    && sound.Attenuation.Maximum == impl["Attenuation"]["Maximum"].ToObject<float>(),
                    "Corpus sound spatial properties changed.");
                var modifiers = (JArray)impl["Modifiers"];
                Require(sound.Modifiers.Count == modifiers.Count, "Sound modifiers were lost.");
                for (int m = 0; m < modifiers.Count; ++m)
                {
                    var modifier = sound.Modifiers[m];
                    Require(modifier.Type.ToString() == (string)modifiers[m]["Type"], "Sound modifier type changed.");
                    if (modifier.Type == SoundModifierType.Cone)
                        Require(modifier.AngleIn == modifiers[m]["AngleIn"].ToObject<float>()
                            && modifier.AngleBand == modifiers[m]["AngleBand"].ToObject<float>()
                            && modifier.AttenOut == modifiers[m]["AttenOut"].ToObject<float>(), "Sound cone changed.");
                }
                long offset = Convert.ToInt64((string)impl["DataFileOffset"], 16);
                binary.BaseStream.Position = offset;
                var audio = binary.ReadSoundData();
                Require(audio.Length > 44 && binary.BaseStream.Position == offset + 20 + audio.Length - 44,
                    "Corpus audio payload boundary is incorrect.");
                binary.BaseStream.Position = offset + 20;
                byte[] buffer = new byte[65536];
                int copied = 0;
                while (copied < audio.Length - 44)
                {
                    int count = binary.Read(buffer, 0, Math.Min(buffer.Length, audio.Length - 44 - copied));
                    Require(count > 0 && audio.AudioBytes.AsSpan(44 + copied, count).SequenceEqual(buffer.AsSpan(0, count)),
                        "Corpus PCM sample bytes changed.");
                    copied += count;
                }
                ++sounds;
            }
            ++projects;
        }
        Require(projects > 0, "No Quill projects found in corpus directory.");
        Console.WriteLine($"[IMM_QUILL_CORPUS] Passed {projects} project metadata trees, {sounds} PCM sounds and {pictures} picture settings.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
