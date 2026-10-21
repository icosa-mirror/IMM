using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;

static class SoundAuthoringChecks
{
    public static void Run(string directory)
    {
        Require(Marshal.SizeOf<ExportSoundProperties>() == 44, "Sound ABI layout changed.");
        Require(!ImmAuthoringAudio.CreatePcm16(0, 2, new byte[4]).Succeeded &&
            !ImmAuthoringAudio.CreatePcm16(7999, 1, new byte[2]).Succeeded &&
            !ImmAuthoringAudio.CreatePcm16(44100, 3, new byte[6]).Succeeded &&
            !ImmAuthoringAudio.CreatePcm16(44100, 2, new byte[3]).Succeeded, "Invalid PCM accepted.");
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0, 0, 0, 1)).Value;
        long parent = document.CreateGroupLayer(0, ImmAuthoringLayerProperties.Default("Sounds")).Value;
        var expected = new Dictionary<string, (ExportSoundProperties Sound, int Channels)>();
        long first = 0;
        for (int index = 0; index < 5; index++)
        {
            int channels = index == 0 ? 1 : index == 4 ? 4 : 2;
            byte[] pcm = Samples(channels);
            var audio = ImmAuthoringAudio.CreatePcm16(44100, channels, pcm).Value;
            byte value = audio.CopyData()[10]; pcm[10] = (byte)(value ^ 255);
            byte[] copy = audio.CopyData(); copy[10] = (byte)(value ^ 255);
            Require(audio.CopyData()[10] == value && audio.FrameCount == 4410, "PCM byte ownership or frame count changed.");
            var properties = ImmAuthoringLayerProperties.Default($"Clip{index}");
            properties.SoundAudio = audio;
            properties.Sound.Gain = 0.75f;
            properties.Sound.Loop = index % 2;
            properties.Sound.SpatialType = index == 4 ? ExportSoundSpatialType.Ambisonic : index >= 2 ?
                ExportSoundSpatialType.Positional : ExportSoundSpatialType.Flat;
            if (index == 2 || index == 3)
            {
                properties.Sound.Attenuation = (ExportSoundAttenuationType)(index - 1);
                properties.Sound.DistanceMin = 2; properties.Sound.DistanceMax = 12;
                properties.Sound.Modifier = (ExportSoundModifierType)(index - 1);
                properties.Sound.AngleInX = 0.3f;
                properties.Sound.AngleInY = index == 3 ? 0.4f : 0;
                properties.Sound.AngleBand = 0.5f; properties.Sound.AttenuationOutside = 0.2f;
            }
            properties.Transform.Position = new Vector3(1, 2, 3); properties.Transform.Flip = ImmAuthoringFlip.Y;
            properties.Pivot.Position = new Vector3(4, 5, 6); properties.Pivot.Flip = ImmAuthoringFlip.X;
            properties.IsTimeline = true; properties.DurationTicks = 126000; properties.MaxRepeatCount = 3;
            var created = document.CreateSoundLayer(parent, properties);
            Require(created.Succeeded, created.Message);
            if (index == 0) first = created.Value;
            Require(!document.CreateDrawing(created.Value).Succeeded && !document.CreateGroupLayer(0, properties).Succeeded,
                "Sound ownership rules failed.");
            expected.Add(properties.Name, (properties.Sound, channels));
        }
        var before = document.CreateSnapshot().Value;
        var original = before.Layers.Single(x => x.Id == first).Properties;
        using (var edit = document.BeginEdit(document.Revision).Value)
        {
            var changed = original; changed.Sound.Loop = 1;
            Require(edit.EditableDocument.SetLayerProperties(first, changed).Succeeded && edit.Commit().Succeeded, "Sound transaction failed.");
        }
        Require(before.Layers.Single(x => x.Id == first).Properties.Sound.Loop == 0 &&
            !ImmAuthoringStructuralComparer.Compare(before, document.CreateSnapshot().Value).Equivalent, "Sound snapshot/comparison failed.");
        Require(document.SetLayerProperties(first, original).Succeeded, "Sound reset failed.");
        var invalid = original; invalid.Sound.Gain = float.NaN;
        Require(!document.SetLayerProperties(first, invalid).Succeeded, "Nonfinite sound gain accepted.");
        invalid = original; invalid.SoundAudio = null;
        Require(!document.SetLayerProperties(first, invalid).Succeeded, "Missing PCM accepted.");
        invalid = original; invalid.Sound.SpatialType = ExportSoundSpatialType.Ambisonic;
        Require(!document.SetLayerProperties(first, invalid).Succeeded, "Invalid ambisonic channel layout accepted.");
        string output = Path.Combine(directory, "sound-opus.imm");
        var result = ImmAuthoringCompiler.ExportToFile(document, output);
        Require(result.Succeeded, result.Message);
        Inspect(output, directory, expected, 5);
        string ogg = Path.Combine(directory, "sound-ogg.imm");
        result = ImmAuthoringCompiler.ExportToFile(document, ogg, audioType: ExportAudioType.Ogg);
        Require(result.Succeeded, result.Message);
        Inspect(ogg, directory, expected, 4);
        Require(Reader.StrokeReader_Init(null) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 6, "Sound hierarchy unavailable.");
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var group), "Sound parent unavailable.");
            for (int index = 1; index <= 5; index++)
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, index, out var layer) && layer.type == 5 && layer.parentId == group.id &&
                    layer.childIndex == index - 1 && layer.pivotFlip == 1 && layer.pivotTransX == 4 && layer.durationTicks == 126000 && layer.maxRepeatCount == 3 &&
                    Reader.StrokeReader_GetAuthoringLayerTransform(doc, index, out var local, out _) && local.flip == 2 && local.transX == 1,
                    "Sound hierarchy/common properties changed.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        byte[] saved = File.ReadAllBytes(output);
        Require(!ImmAuthoringCompiler.ExportToFile(document, output,
            new ImmAuthoringOperationOptions(limits: new ImmAuthoringLimits(maxInputBytes: 1))).Succeeded &&
            saved.SequenceEqual(File.ReadAllBytes(output)), "Sound asset budget replaced output.");
        Require(!Native.ImmExporter_SoundSetPcm16(IntPtr.Zero, 44100, 2, new byte[4], 4), "Null sound target accepted.");
        using (var empty = ExportSequence.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1), default))
        {
            var transform = new TransformNative { Qw = 1, Scale = 1 };
            var sound = original.Sound;
            IntPtr handle = Native.ImmExporter_CreateSoundLayer(empty.Handle, IntPtr.Zero, "Empty", 1, 1, ref transform, ref transform, 0, 0, 0, ref sound);
            Require(handle != IntPtr.Zero,
                "Empty sound fixture failed.");
            Require(!Native.ImmExporter_SoundSetPcm16(handle, 7999, 1, new byte[2], 2) &&
                !Native.ImmExporter_SoundSetPcm16(handle, 44100, 1, null, 2) &&
                !Native.ImmExporter_SoundSetPcm16(handle, 44100, 1, new byte[3], 3), "Native PCM validation failed.");
            Require(empty.ExportToMemory() == null, "Native sound without an asset exported successfully.");
        }
        Console.WriteLine("[IMM_SOUND_AUTHORING] Passed immutable PCM, graph ownership/transactions, spatial metadata, mono/stereo/ambisonic Opus/Ogg export and resource/output guards; playback not checked.");
    }

    private static byte[] Samples(int channels)
    {
        byte[] bytes = new byte[4410 * channels * 2];
        for (int frame = 0; frame < 4410; frame++)
            for (int channel = 0; channel < channels; channel++)
            {
                short value = (short)(Math.Sin(2 * Math.PI * (220 + channel * 70) * frame / 44100) * 8000);
                int offset = (frame * channels + channel) * 2;
                bytes[offset] = (byte)value; bytes[offset + 1] = (byte)(value >> 8);
            }
        return bytes;
    }

    private static void Inspect(string path, string directory, Dictionary<string, (ExportSoundProperties Sound, int Channels)> expected, int format)
    {
        byte[] file = File.ReadAllBytes(path);
        int table = file.AsSpan().IndexOf(Encoding.ASCII.GetBytes("Restable"));
        Require(table >= 0 && BitConverter.ToUInt32(file, table + 16) == expected.Count, "Sound asset table changed.");
        using var reader = new BinaryReader(new MemoryStream(file));
        int offset = 0, found = 0;
        while (offset < file.Length)
        {
            int relative = file.AsSpan(offset).IndexOf(Encoding.ASCII.GetBytes("Layerdes"));
            if (relative < 0) break;
            offset += relative;
            reader.BaseStream.Position = offset + 16;
            int version = reader.ReadInt32();
            reader.BaseStream.Position += 65 * (version >= 1 ? 2 : 1) + 17;
            string name = Encoding.UTF8.GetString(reader.ReadBytes(reader.ReadInt32()));
            string type = Encoding.UTF8.GetString(reader.ReadBytes(reader.ReadInt32()));
            int asset = reader.ReadInt32(); offset += 8;
            if (type != "Quill.Sound") continue;
            Require(expected.TryGetValue(name, out var item), "Unexpected sound name.");
            Require(Encoding.ASCII.GetString(reader.ReadBytes(8)) == "Timeline", "Sound timeline missing.");
            long timelineSize = reader.ReadInt64(); reader.BaseStream.Position += timelineSize;
            Require(reader.ReadUInt32() == 1, "Sound version changed.");
            var properties = new ExportSoundProperties { SpatialType = (ExportSoundSpatialType)reader.ReadByte() };
            Require(reader.ReadSingle() == 0, "Unexpected legacy sound duration.");
            properties.Gain = reader.ReadSingle(); properties.Loop = reader.ReadByte(); reader.ReadByte();
            properties.Attenuation = (ExportSoundAttenuationType)reader.ReadByte();
            properties.DistanceMin = reader.ReadSingle(); properties.DistanceMax = reader.ReadSingle();
            properties.Modifier = (ExportSoundModifierType)reader.ReadByte();
            if (properties.Modifier != ExportSoundModifierType.None)
            {
                properties.AngleInX = reader.ReadSingle();
                if (properties.Modifier == ExportSoundModifierType.DirectionalFrustum) properties.AngleInY = reader.ReadSingle();
                properties.AngleBand = reader.ReadSingle(); properties.AttenuationOutside = reader.ReadSingle();
            }
            Require(properties.Equals(item.Sound), "Serialized spatial sound settings changed.");
            int assetOffset = checked((int)BitConverter.ToUInt64(file, table + 24 + asset * 20));
            int assetSize = checked((int)BitConverter.ToUInt64(file, table + 32 + asset * 20));
            Require(BitConverter.ToUInt32(file, assetOffset) == format, "Sound codec changed.");
            int headerSize = format == 5 ? 16 : 12;
            if (format == 5) Require(BitConverter.ToUInt32(file, assetOffset + 4) == item.Channels, "Opus channel header changed.");
            Require(BitConverter.ToUInt64(file, assetOffset + headerSize - 8) == (ulong)(assetSize - headerSize), "Sound payload framing changed.");
            byte[] encoded = file.AsSpan(assetOffset + headerSize, assetSize - headerSize).ToArray();
            Require(encoded.AsSpan(0, 4).SequenceEqual(Encoding.ASCII.GetBytes("OggS")), "Ogg container missing.");
            int identification = encoded.AsSpan().IndexOf(Encoding.ASCII.GetBytes(format == 5 ? "OpusHead" : "vorbis"));
            Require(identification >= 0 && encoded[identification + (format == 5 ? 9 : 10)] == item.Channels &&
                BitConverter.ToUInt32(encoded, identification + (format == 5 ? 12 : 11)) == 44100, "Encoded audio channels/input rate changed.");
            File.WriteAllBytes(Path.Combine(directory, $"{name}-{format}.{(format == 5 ? "opus" : "ogg")}"), encoded);
            found++;
        }
        Require(found == expected.Count, "Sound metadata count changed.");
    }

    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
