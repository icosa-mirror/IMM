using System;
using System.IO;
using System.Linq;
using SharpQuill;

static class QuillSoundWriterChecks
{
    public static void Run()
    {
        string tempRoot = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
        string directory = Path.Combine(tempRoot, $"imm-quill-sound-writer-{Guid.NewGuid():N}");
        try
        {
            var sequence = Sequence.CreateDefault(); sequence.RootLayer.Children.Clear();
            var group = new LayerGroup("Media"); sequence.RootLayer.Children.Add(group);
            group.Children.Add(new LayerPicture("MixedPicture") { PictureType = PictureType.TwoD,
                Data = new PictureData { Width = 1, Height = 1, Pixels = new byte[] { 1, 2, 3 } } });
            for (int channels = 1; channels <= 2; channels++)
            {
                var sound = new LayerSound($"Sound{channels}")
                {
                    SoundType = channels == 1 ? SoundType.Flat : SoundType.Positional, Gain = 0.52f,
                    Loop = channels == 2, Visible = channels == 1,
                    Attenuation = new SoundAttenuation { Mode = channels == 1 ? SoundAttenuationMode.None : SoundAttenuationMode.Logarithmic, Minimum = 0.1f, Maximum = 1.9f },
                    Data = new SoundData { AudioBytes = Wave(channels) }
                };
                sound.Modifiers.Add(new SoundModifier { Type = channels == 1 ? SoundModifierType.None : SoundModifierType.Cone,
                    AngleIn = channels == 1 ? 0 : 0.46f, AngleBand = channels == 1 ? 0 : 0.45f, AttenOut = channels == 1 ? 0 : 0.1f });
                group.Children.Add(sound);
            }
            QuillSequenceWriter.Write(sequence, directory);
            var read = QuillSequenceReader.Read(directory);
            var children = ((LayerGroup)read.RootLayer.Children.Single()).Children;
            var sounds = children.OfType<LayerSound>().ToArray();
            Require(children.Count == 3 && sounds.Length == 2, "Mixed media or sound children were omitted.");
            using (var binary = new BinaryReader(File.OpenRead(Path.Combine(directory, "Quill.qbin"))))
                for (int index = 0; index < 2; index++)
                {
                    var actual = sounds[index]; var expected = (LayerSound)group.Children[index + 1];
                    Require(actual.SoundType == expected.SoundType && actual.Gain == expected.Gain && actual.Loop == expected.Loop && actual.Visible == expected.Visible &&
                        actual.Attenuation.Mode == expected.Attenuation.Mode && actual.Attenuation.Minimum == expected.Attenuation.Minimum && actual.Attenuation.Maximum == expected.Attenuation.Maximum &&
                        actual.Modifiers.Count == 1 && actual.Modifiers[0].Type == expected.Modifiers[0].Type && actual.Modifiers[0].AngleIn == expected.Modifiers[0].AngleIn &&
                        actual.Modifiers[0].AngleBand == expected.Modifiers[0].AngleBand && actual.Modifiers[0].AttenOut == expected.Modifiers[0].AttenOut &&
                        actual.Data.AudioBytes.SequenceEqual(expected.Data.AudioBytes), "Sound settings or PCM bytes changed during project read-back.");
                    binary.BaseStream.Position = actual.DataFileOffset;
                    Require(binary.ReadBytes(8).SequenceEqual(new byte[] { 0, 0, 0, 0, 0, 0, (byte)(index + 1), 16 }) &&
                        binary.ReadUInt32() == 44100 && binary.ReadUInt32() == 8 && binary.ReadUInt32() == 0 &&
                        binary.ReadBytes((index + 1) * 16).SequenceEqual(expected.Data.AudioBytes.Skip(44)), "Sound qbin header/payload changed.");
                }
            // Ancillary WAV chunks are not part of Quill's PCM payload.
            byte[] canonical = Wave(1), withJunk;
            using (var stream = new MemoryStream())
            using (var writer = new BinaryWriter(stream))
            {
                writer.Write(canonical, 0, 4); writer.Write(canonical.Length + 10 - 8); writer.Write(canonical, 8, 4);
                writer.Write(new byte[] { 74, 85, 78, 75 }); writer.Write(1); writer.Write((byte)99); writer.Write((byte)0);
                writer.Write(canonical, 12, canonical.Length - 12); writer.Flush(); withJunk = stream.ToArray();
            }
            using (var stream = new MemoryStream())
            using (var writer = new QBinWriter(stream))
            {
                writer.Write(new SoundData { AudioBytes = withJunk }); writer.Flush();
                using var reader = new QBinReader(new MemoryStream(stream.ToArray()));
                Require(reader.ReadSoundData().AudioBytes.SequenceEqual(canonical), "WAV ancillary chunks changed PCM extraction.");
            }
            var files = new[] { "Quill.json", "Quill.qbin", "State.json" }.ToDictionary(name => name, name => File.ReadAllBytes(Path.Combine(directory, name)));
            foreach (Action<LayerSound> corrupt in new Action<LayerSound>[]
            {
                sound => sound.Data = null,
                sound => sound.Data.AudioBytes = Wave(1).Take(59).ToArray(),
                sound => sound.Data.AudioBytes[20] = 3,
                sound => sound.Data.AudioBytes[22] = 4,
                sound => sound.Data.AudioBytes[32] = 7,
                sound => sound.SoundType = (SoundType)99,
                sound => sound.Gain = float.NaN,
                sound => sound.Attenuation.Maximum = -1,
                sound => sound.Modifiers.Add(new SoundModifier()),
                sound => sound.Modifiers[0].Type = SoundModifierType.Cone,
                sound => sound.Modifiers[0].AngleIn = 1
            })
            {
                var original = (LayerSound)group.Children[1]; var valid = new LayerSound("Invalid")
                {
                    SoundType = SoundType.Flat, Gain = 1, Data = new SoundData { AudioBytes = Wave(1) },
                    Attenuation = new SoundAttenuation { Mode = SoundAttenuationMode.Linear, Minimum = 0.1f, Maximum = 1 }
                };
                valid.Modifiers.Add(new SoundModifier()); group.Children[1] = valid; corrupt(valid);
                bool rejected = false;
                try { QuillSequenceWriter.Write(sequence, directory); }
                catch (Exception exception) when (exception is InvalidDataException || exception is NotSupportedException) { rejected = true; }
                Require(rejected && files.All(file => file.Value.SequenceEqual(File.ReadAllBytes(Path.Combine(directory, file.Key)))), "Invalid sound overwrote an existing project.");
                group.Children[1] = original;
            }
            using (var stream = new MemoryStream())
            using (var writer = new QBinWriter(stream))
            {
                bool rejected = false;
                try { writer.Write(new SoundData { AudioBytes = new byte[12] }); }
                catch (InvalidDataException) { rejected = true; }
                Require(rejected && stream.Length == 0, "Invalid sound emitted a partial qbin header.");
            }
            Console.WriteLine("[IMM_QUILL_SOUND_WRITER] Passed mixed picture/mono/stereo sound read-back, exact PCM/settings, observed headers, WAV chunk handling and invalid-output guards.");
        }
        finally
        {
            Require(Path.GetDirectoryName(Path.GetFullPath(directory)) == tempRoot, "Unexpected test cleanup target.");
            if (Directory.Exists(directory)) Directory.Delete(directory, true);
        }
    }
    private static byte[] Wave(int channels)
    {
        using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream);
        writer.Write(new byte[] { 82, 73, 70, 70 }); writer.Write(36 + channels * 16); writer.Write(new byte[] { 87, 65, 86, 69, 102, 109, 116, 32 });
        writer.Write(16); writer.Write((ushort)1); writer.Write((ushort)channels); writer.Write(44100); writer.Write(44100 * channels * 2);
        writer.Write((ushort)(channels * 2)); writer.Write((ushort)16); writer.Write(new byte[] { 100, 97, 116, 97 }); writer.Write(channels * 16);
        for (int index = 0; index < channels * 8; index++) writer.Write((short)(index * 123 - 1000));
        writer.Flush(); return stream.ToArray();
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
