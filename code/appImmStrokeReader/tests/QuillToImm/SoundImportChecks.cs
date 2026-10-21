using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;

static class SoundImportChecks
{
    [DllImport("ImmUnityPlugin", EntryPoint = "ImmExporter_SoundSetEncoded")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool SetEncodedPointer(IntPtr layer, int format, int rate, int channels, IntPtr bytes, ulong size);
    public static void Run(string path, string directory)
    {
        byte[] file = File.ReadAllBytes(path);
        var expected = new List<(StrokeSoundAssetInfo Info, byte[] Bytes)>();
        Require(Marshal.SizeOf<StrokeSoundProperties>() == 44 && Marshal.SizeOf<StrokeSoundAssetInfo>() == 68 &&
            !Reader.StrokeReader_GetSoundAssetInfo(-1, 0, out _), "Sound ABI/init guard failed.");
        Require(Reader.StrokeReader_Init(null) == 0, "Reader init failed.");
        int disk = Reader.StrokeReader_LoadFromFile(path), memory = 0;
        var pinned = GCHandle.Alloc(file, GCHandleType.Pinned);
        try
        {
            memory = Reader.StrokeReader_LoadFromMemory(pinned.AddrOfPinnedObject(), file.Length);
            Require(disk > 0 && memory > 0, "Sound file/memory read failed.");
            int count = Reader.StrokeReader_GetAuthoringLayerCount(disk);
            for (int index = 0; index < count; index++)
            {
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(disk, index, out var layer), "Sound hierarchy missing.");
                bool found = Reader.StrokeReader_GetSoundAssetInfo(disk, index, out var info);
                Require(found == (layer.type == 5), "Sound asset query used the legacy array or wrong type.");
                if (!found) continue;
                Require(info.sampleRate > 0 && info.byteCount > 0 && Reader.StrokeReader_GetSoundAssetInfo(memory, index, out var memoryInfo) &&
                    info.Equals(memoryInfo), "Sound file/memory metadata mismatch or unsupported fixture.");
                byte[] bytes = new byte[info.byteCount], memoryBytes = new byte[info.byteCount];
                Require(Reader.StrokeReader_GetSoundAssetData(disk, index, bytes, bytes.Length) == bytes.Length &&
                    Reader.StrokeReader_GetSoundAssetData(memory, index, memoryBytes, memoryBytes.Length) == memoryBytes.Length &&
                    bytes.SequenceEqual(memoryBytes), "Sound file/memory payload differs.");
                memoryBytes[0] = 42;
                Require(Reader.StrokeReader_GetSoundAssetData(disk, index, memoryBytes, memoryBytes.Length - 1) == 0 && memoryBytes[0] == 42 &&
                    Reader.StrokeReader_GetSoundAssetData(disk, index, null, 0) == 0, "Sound buffer guard failed.");
                expected.Add((info, bytes));
            }
            Require(expected.Count > 0 && !Reader.StrokeReader_GetSoundAssetInfo(disk, -1, out _) &&
                !Reader.StrokeReader_GetSoundAssetInfo(disk, count, out _), "Sound bounds guard failed.");
        }
        finally
        {
            if (disk > 0) Reader.StrokeReader_Unload(disk);
            if (memory > 0) Reader.StrokeReader_Unload(memory);
            pinned.Free(); Reader.StrokeReader_End();
        }
        Require(!Reader.StrokeReader_GetSoundAssetInfo(disk, 0, out _) &&
            Reader.StrokeReader_GetSoundAssetData(disk, 0, new byte[1], 1) == 0, "Sound query survived shutdown.");
        var imported = ImmAuthoringImporter.ImportFromFile(path);
        var memoryImport = ImmAuthoringImporter.ImportFromMemory(file);
        Require(imported.Succeeded && memoryImport.Succeeded, $"Sound import failed: {imported.Message}; {memoryImport.Message}");
        using (imported.Document)
        using (memoryImport.Document)
        {
            var snapshot = imported.Document.CreateSnapshot().Value;
            var sounds = snapshot.Layers.Where(x => x.Type == ImmAuthoringLayerType.Sound).ToArray();
            Require(sounds.Length == expected.Count && !imported.Issues.Any(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedSoundAsset),
                "Encoded sound import omitted content.");
            for (int i = 0; i < sounds.Length; i++)
            {
                var properties = sounds[i].Properties;
                var item = expected[i];
                Require(properties.SoundAudio != null && properties.SoundAudio.FrameCount == -1 &&
                    properties.SoundAudio.SampleRate == item.Info.sampleRate && properties.SoundAudio.Channels == item.Info.channels &&
                    (int)properties.SoundAudio.Encoding == item.Info.encoding && properties.SoundAudio.CopyData().SequenceEqual(item.Bytes) &&
                    properties.SoundSourceDuration == item.Info.sourceDuration && properties.SoundInitiallyPlaying == (item.Info.initiallyPlaying != 0) &&
                    SameSettings(properties.Sound, item.Info.properties),
                    "Sound metadata or encoded payload changed.");
                if (i == 0 && OperatingSystem.IsWindows()) CheckBounds(item.Info, item.Bytes);
            }
            Require(ImmAuthoringStructuralComparer.Compare(snapshot, memoryImport.Document.CreateSnapshot().Value).Equivalent,
                "File/memory sound graphs differ.");
            string output = Path.Combine(directory, $"{Path.GetFileNameWithoutExtension(path)}-sound-roundtrip.imm");
            // Global codec selection applies to PCM. Imported encoded assets must remain unchanged.
            var exported = ImmAuthoringCompiler.ExportToFile(imported.Document, output, audioType: ExportAudioType.Ogg);
            Require(exported.Succeeded, exported.Message);
            var again = ImmAuthoringImporter.ImportFromFile(output);
            Require(again.Succeeded, again.Message);
            using (again.Document)
                Require(ImmAuthoringStructuralComparer.Compare(snapshot, again.Document.CreateSnapshot().Value).Equivalent,
                    "Sound graph/settings/payload changed during import/export/import.");
        }
        Console.WriteLine($"[IMM_SOUND_IMPORT] Passed {expected.Count} encoded sounds through file/memory queries and exact graph/payload round trips.");
    }

    private static bool SameSettings(ExportSoundProperties a, StrokeSoundProperties b) =>
        (int)a.SpatialType == b.spatialType && a.Gain == b.gain && a.Loop == b.loop && (int)a.Attenuation == b.attenuation &&
        a.DistanceMin == b.distanceMin && a.DistanceMax == b.distanceMax && (int)a.Modifier == b.modifier &&
        a.AngleInX == b.angleInX && a.AngleInY == b.angleInY && a.AngleBand == b.angleBand && a.AttenuationOutside == b.attenuationOutside;

    private static void CheckBounds(StrokeSoundAssetInfo info, byte[] bytes)
    {
        using var sequence = ExportSequence.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1), default);
        var transform = new TransformNative { Qw = 1, Scale = 1 };
        var settings = new ExportSoundProperties { Gain = 1, SpatialType = info.channels == 4 ? ExportSoundSpatialType.Ambisonic : ExportSoundSpatialType.Flat };
        IntPtr layer = Native.ImmExporter_CreateSoundLayer(sequence.Handle, IntPtr.Zero, "Bounds", 1, 1, ref transform, ref transform, 0, 0, 0, ref settings);
        Require(layer != IntPtr.Zero, "Sound bounds fixture failed.");
        GuardedMemoryChecks.Run(bytes, new[] { bytes.Length, 0, 1, 7, 8, 12, 16, 26, 27, 33, bytes.Length - 1 },
            (input, length) => SetEncodedPointer(layer, info.encoding, info.sampleRate, info.channels, input, (ulong)length));
        if (info.encoding == 4 || info.encoding == 5)
        {
            byte[] corrupted = (byte[])bytes.Clone();
            corrupted[22] ^= 1; // Change the stored page checksum without changing packet framing.
            var pinned = GCHandle.Alloc(corrupted, GCHandleType.Pinned);
            try
            {
                Require(!SetEncodedPointer(layer, info.encoding, info.sampleRate, info.channels,
                    pinned.AddrOfPinnedObject(), (ulong)corrupted.Length), "Ogg checksum corruption was accepted.");
            }
            finally { pinned.Free(); }
        }
    }

    public static void AdditionalFixtures(string directory)
    {
        byte[] wave;
        using (var stream = new MemoryStream())
        {
            using var writer = new BinaryWriter(stream, Encoding.ASCII, true);
            writer.Write(Encoding.ASCII.GetBytes("RIFF")); writer.Write(36 + 400); writer.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
            writer.Write(16); writer.Write((ushort)1); writer.Write((ushort)2); writer.Write(44100); writer.Write(44100 * 4);
            writer.Write((ushort)4); writer.Write((ushort)16); writer.Write(Encoding.ASCII.GetBytes("data")); writer.Write(400); writer.Write(new byte[400]);
            writer.Flush(); wave = stream.ToArray();
        }
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value;
        var properties = ImmAuthoringLayerProperties.Default("Wave");
        properties.SoundAudio = ImmAuthoringAudio.CreateEncoded(44100, 2, ImmAuthoringAudioEncoding.Wav, wave).Value;
        properties.SoundSourceDuration = 0.75f; properties.SoundInitiallyPlaying = true;
        long id = document.CreateSoundLayer(0, properties).Value;
        string path = Path.Combine(directory, "sound-wave.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, path).Succeeded, "WAV passthrough export failed.");
        Run(path, directory);
        properties.Visible = false;
        Require(document.SetLayerProperties(id, properties).Succeeded && ImmAuthoringCompiler.ExportToFile(document, path).Succeeded, "Hidden sound export failed.");
        Run(path, directory);
        byte[] saved = File.ReadAllBytes(path);
        byte[] broken = (byte[])saved.Clone();
        int table = broken.AsSpan().IndexOf(Encoding.ASCII.GetBytes("Restable"));
        int asset = checked((int)BitConverter.ToUInt64(broken, table + 24));
        broken[asset + 12] = 0; // Corrupt the known codec stream, retaining valid IMM framing.
        string bad = Path.Combine(directory, "sound-malformed.imm"); File.WriteAllBytes(bad, broken);
        var rejected = ImmAuthoringImporter.ImportFromFile(bad);
        Require(rejected.Succeeded, rejected.Message);
        using (rejected.Document)
            Require(rejected.Issues.Any(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedSoundAsset) &&
                !rejected.Document.CreateSnapshot().Value.Layers.Any(x => x.Type == ImmAuthoringLayerType.Sound), "Malformed sound was silently accepted or omitted without a diagnostic.");
        byte[] fake = new byte[27]; Encoding.ASCII.GetBytes("OggS").CopyTo(fake, 0);
        properties.SoundAudio = ImmAuthoringAudio.CreateEncoded(48000, 2, ImmAuthoringAudioEncoding.Opus, fake).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded && !ImmAuthoringCompiler.ExportToFile(document, path).Succeeded &&
            saved.SequenceEqual(File.ReadAllBytes(path)), "Malformed encoded sound replaced output.");
        Run(Path.GetFullPath("exampleImmFiles/sample1.imm"), directory);
    }

    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
