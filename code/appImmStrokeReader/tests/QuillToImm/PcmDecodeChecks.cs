using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;

static class PcmDecodeChecks
{
    private static bool _opusHeaders, _vorbisPacket;
    [DllImport("ImmStrokeReader", EntryPoint = "StrokeReader_DecodeSoundPcm16")]
    private static extern long NullInfo(int doc, int layer, int limit, IntPtr info);
    [DllImport("ImmStrokeReader", EntryPoint = "StrokeReader_CopyDecodedSoundPcm16")]
    private static extern int CopyPointer(long lease, IntPtr bytes, int capacity);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr GetModuleHandle(string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] private static extern uint GetModuleFileName(IntPtr module, StringBuilder name, int size);

    public static void Check(int doc, int layer, StrokeSoundAssetInfo encoded, byte[] bytes, string directory, string name)
    {
        Require(Marshal.SizeOf<StrokeSoundPcmInfo>() == 16 && NullInfo(doc, layer, 1048576, IntPtr.Zero) == -1 &&
            Reader.StrokeReader_DecodeSoundPcm16(-1, layer, 1048576, out _) == -1 && Reader.StrokeReader_DecodeSoundPcm16(doc, int.MaxValue, 1048576, out _) == -1 &&
            Reader.StrokeReader_DecodeSoundPcm16(doc, layer, 0, out _) == -1 && Reader.StrokeReader_DecodeSoundPcm16(doc, layer, 1, out _) == -2,
            "PCM decode ABI/argument/output-limit guard failed.");
        byte[] pcm = Decode(doc, layer, out var info);
        Require(info.sampleRate == encoded.sampleRate && info.channels == encoded.channels && info.frameCount > 0 &&
            info.byteCount == (long)info.frameCount * info.channels * 2, "Decoded PCM metadata changed.");
        if (encoded.encoding == 3 && bytes.Length == 44 + info.byteCount && BitConverter.ToUInt16(bytes, 34) == 16)
            Require(pcm.SequenceEqual(bytes.Skip(44)), "PCM16 WAV samples changed.");
        File.WriteAllBytes(Path.Combine(directory, $"{name}-pcm{layer}.s16"), pcm);
        if (encoded.encoding == 5 && !_opusHeaders)
        {
            _opusHeaders = true;
            int header = 27 + bytes[26], skip = BitConverter.ToUInt16(bytes, header + 10);
            foreach (int gain in new[] { 1536, -1536 })
            {
                byte[] changed = (byte[])bytes.Clone(); BitConverter.GetBytes((short)gain).CopyTo(changed, header + 16); FixCrc(changed);
                byte[] gained = DecodeAsset(changed, encoded, directory, $"opus-gain-{gain}", out var gainInfo);
                Require(gainInfo.frameCount == info.frameCount, "Opus gain changed decoded timing.");
                double factor = Math.Pow(10, gain / (256.0 * 20)); int maxError = 0;
                for (int offset = 0; offset < pcm.Length; offset += 2)
                {
                    int expected = (int)Math.Round(BitConverter.ToInt16(pcm, offset) * factor, MidpointRounding.AwayFromZero);
                    expected = Math.Clamp(expected, short.MinValue, short.MaxValue);
                    maxError = Math.Max(maxError, Math.Abs(expected - BitConverter.ToInt16(gained, offset)));
                }
                Require(maxError <= 3, $"Opus Q8 header gain was not applied correctly (max error {maxError}).");
            }
            byte[] invalid = (byte[])bytes.Clone();
            foreach (int page in Pages(invalid)) if ((invalid[page + 5] & 4) != 0) BitConverter.GetBytes((long)(skip - 1)).CopyTo(invalid, page + 6);
            FixCrc(invalid); RejectAsset(invalid, encoded, directory, "opus-bad-trim");
        }
        if (encoded.encoding == 4 && !_vorbisPacket)
        {
            _vorbisPacket = true; byte[] invalid = (byte[])bytes.Clone();
            int setup = invalid.AsSpan().IndexOf(new byte[] { 5, 118, 111, 114, 98, 105, 115 });
            Require(setup >= 0, "Vorbis setup fixture missing."); invalid[setup + 1] ^= 128; FixCrc(invalid);
            RejectAsset(invalid, encoded, directory, "vorbis-bad-setup");
        }
    }
    private static byte[] Decode(int doc, int layer, out StrokeSoundPcmInfo info)
    {
        long lease = Reader.StrokeReader_DecodeSoundPcm16(doc, layer, 256 * 1024 * 1024, out info);
        Require(lease > 0, $"Native PCM decode failed: {lease}.");
        try
        {
            byte[] result = new byte[info.byteCount]; result[0] = 91;
            Require(Reader.StrokeReader_CopyDecodedSoundPcm16(lease, null, info.byteCount) == 0 &&
                Reader.StrokeReader_CopyDecodedSoundPcm16(lease, result, result.Length - 1) == 0 && result[0] == 91 &&
                Reader.StrokeReader_CopyDecodedSoundPcm16(lease, result, result.Length) == result.Length, "PCM copy buffer guard failed.");
            if (OperatingSystem.IsWindows()) GuardedMemoryChecks.Run(result, new[] { result.Length, 0, 1, result.Length - 1 },
                (input, length) => CopyPointer(lease, input, length) == result.Length);
            return result;
        }
        finally
        {
            Require(Reader.StrokeReader_ReleaseDecodedSoundPcm16(lease) && !Reader.StrokeReader_ReleaseDecodedSoundPcm16(lease) &&
                Reader.StrokeReader_CopyDecodedSoundPcm16(lease, new byte[1], 1) == 0, "PCM release/stale lease guard failed.");
        }
    }
    private static string ExportAsset(byte[] bytes, StrokeSoundAssetInfo info, string directory, string name)
    {
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value;
        var properties = ImmAuthoringLayerProperties.Default("Encoded");
        properties.SoundAudio = ImmAuthoringAudio.CreateEncoded(info.sampleRate, info.channels, (ImmAuthoringAudioEncoding)info.encoding, bytes).Value;
        properties.Sound.SpatialType = info.channels == 4 ? ExportSoundSpatialType.Ambisonic : ExportSoundSpatialType.Flat;
        Require(document.CreateSoundLayer(0, properties).Succeeded, "Encoded PCM fixture creation failed.");
        string path = Path.Combine(directory, $"{name}.imm"); var exported = ImmAuthoringCompiler.ExportToFile(document, path);
        Require(exported.Succeeded, exported.Message); return path;
    }
    private static byte[] DecodeAsset(byte[] bytes, StrokeSoundAssetInfo info, string directory, string name, out StrokeSoundPcmInfo decoded)
    {
        string path = ExportAsset(bytes, info, directory, name); int doc = Reader.StrokeReader_LoadFromFile(path);
        Require(doc > 0, "Encoded PCM fixture load failed.");
        try { return Decode(doc, 0, out decoded); }
        finally { Reader.StrokeReader_Unload(doc); }
    }
    private static void RejectAsset(byte[] bytes, StrokeSoundAssetInfo info, string directory, string name)
    {
        string path = ExportAsset(bytes, info, directory, name); int doc = Reader.StrokeReader_LoadFromFile(path);
        Require(doc > 0 && Reader.StrokeReader_GetSoundAssetInfo(doc, 0, out var encoded) && encoded.sampleRate > 0, "Bad-codec fixture must retain valid container headers.");
        try { Require(Reader.StrokeReader_DecodeSoundPcm16(doc, 0, 1048576, out var decoded) == -1 && decoded.byteCount == 0, "Malformed codec/trim produced an audio lease."); }
        finally { Reader.StrokeReader_Unload(doc); }
    }
    private static System.Collections.Generic.IEnumerable<int> Pages(byte[] bytes)
    {
        for (int page = 0; page < bytes.Length;)
        {
            yield return page; int segments = bytes[page + 26], length = 27 + segments;
            for (int index = 0; index < segments; index++) length += bytes[page + 27 + index];
            page += length;
        }
    }
    private static void FixCrc(byte[] bytes)
    {
        foreach (int page in Pages(bytes))
        {
            int length = 27 + bytes[page + 26]; for (int i = 27; i < 27 + bytes[page + 26]; i++) length += bytes[page + i];
            Array.Clear(bytes, page + 22, 4); uint crc = 0;
            for (int index = page; index < page + length; index++)
            {
                crc ^= (uint)bytes[index] << 24;
                for (int bit = 0; bit < 8; bit++) crc = (crc << 1) ^ ((crc & 0x80000000u) != 0 ? 0x04c11db7u : 0);
            }
            BitConverter.GetBytes(crc).CopyTo(bytes, page + 22);
        }
    }
    public static void LifetimeAndOffset(string directory)
    {
        string source = Path.Combine(directory, "pcm-long-opus.imm");
        using (var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value)
        {
            byte[] samples = new byte[48000 * 3 * 2];
            for (int frame = 0; frame < 48000 * 3; frame++) BitConverter.GetBytes((short)(4000 * Math.Sin(2 * Math.PI * 440 * frame / 48000))).CopyTo(samples, frame * 2);
            var properties = ImmAuthoringLayerProperties.Default("Long"); properties.SoundAudio = ImmAuthoringAudio.CreatePcm16(48000, 1, samples).Value;
            Require(document.CreateSoundLayer(0, properties).Succeeded && ImmAuthoringCompiler.ExportToFile(document, source).Succeeded, "Long Opus fixture failed.");
        }
        Require(Reader.StrokeReader_Init(null) == 0, "PCM lifetime init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(source); Require(doc > 0, "PCM lifetime load failed.");
        Require(Reader.StrokeReader_GetSoundAssetInfo(doc, 0, out var encoded), "Long Opus metadata missing.");
        byte[] bytes = new byte[encoded.byteCount]; Require(Reader.StrokeReader_GetSoundAssetData(doc, 0, bytes, bytes.Length) == bytes.Length, "Long Opus bytes missing.");
        byte[] original = Decode(doc, 0, out var originalInfo); byte[] shifted = (byte[])bytes.Clone(); int pages = 0;
        foreach (int page in Pages(shifted))
        {
            long granule = BitConverter.ToInt64(shifted, page + 6);
            if (granule > 0) { BitConverter.GetBytes(granule + 48000).CopyTo(shifted, page + 6); pages++; }
        }
        Require(pages > 1, "Initial-granule fixture needs multiple audio pages."); FixCrc(shifted);
        byte[] changed = DecodeAsset(shifted, encoded, directory, "opus-granule-offset", out var changedInfo);
        Require(changedInfo.frameCount == originalInfo.frameCount && changed.SequenceEqual(original), "Initial granule offset changed pre-skip or PCM samples.");
        long lease = Reader.StrokeReader_DecodeSoundPcm16(doc, 0, 1048576, out var info); Require(lease > 0, "PCM owned lease failed.");
        Reader.StrokeReader_Unload(doc);
        Require(Reader.StrokeReader_CopyDecodedSoundPcm16(lease, new byte[info.byteCount], info.byteCount) == info.byteCount, "Document unload invalidated independent PCM.");
        Reader.StrokeReader_End(); Require(Reader.StrokeReader_Init(null) == 0, "PCM reinit failed.");
        int reopened = Reader.StrokeReader_LoadFromFile(source);
        long fresh = Reader.StrokeReader_DecodeSoundPcm16(reopened, 0, 1048576, out _);
        Require(fresh > 0 && fresh != lease && !Reader.StrokeReader_ReleaseDecodedSoundPcm16(lease) && Reader.StrokeReader_CopyDecodedSoundPcm16(lease, new byte[1], 1) == 0,
            "Reader shutdown retained or reused a stale PCM lease.");
        Require(Reader.StrokeReader_ReleaseDecodedSoundPcm16(fresh), "New PCM lease release failed.");
        Reader.StrokeReader_Unload(reopened); Reader.StrokeReader_End();
        Console.WriteLine("[IMM_SOUND_PCM] Passed WAV/Vorbis/Opus decode, byte budgets, PCM copy/lifetime guards, codec corruption, Opus gain/pre-skip/end trim and initial granule offsets.");
    }
    public static void WavFormats(string directory)
    {
        Require(Reader.StrokeReader_Init(null) == 0, "WAV profile init failed.");
        try
        {
            var cases = new[]
            {
                (Name: "8", Bits: 8, Codec: 1, Data: new byte[] { 0, 128, 255 }, Expected: new short[] { -32768, 0, 32512 }),
                (Name: "16", Bits: 16, Codec: 1, Data: new short[] { short.MinValue, -1, 0, 1, short.MaxValue }.SelectMany(value => BitConverter.GetBytes(value)).ToArray(), Expected: new short[] { short.MinValue, -1, 0, 1, short.MaxValue }),
                (Name: "24", Bits: 24, Codec: 1, Data: new byte[] { 0, 0, 128, 0, 0, 0, 255, 255, 127 }, Expected: new short[] { short.MinValue, 0, short.MaxValue }),
                (Name: "32", Bits: 32, Codec: 1, Data: new int[] { int.MinValue, 0, int.MaxValue }.SelectMany(value => BitConverter.GetBytes(value)).ToArray(), Expected: new short[] { short.MinValue, 0, short.MaxValue }),
                (Name: "float", Bits: 32, Codec: 3, Data: new float[] { -2, -1, -0.25f, 0, 0.25f, 1, 2 }.SelectMany(value => BitConverter.GetBytes(value)).ToArray(), Expected: new short[] { short.MinValue, short.MinValue, -8192, 0, 8192, short.MaxValue, short.MaxValue })
            };
            var info = new StrokeSoundAssetInfo { encoding = 3, sampleRate = 48000, channels = 1 };
            foreach (var item in cases)
            {
                byte[] pcm = DecodeAsset(Wave(item.Bits, item.Codec, item.Data), info, directory, $"pcm-wave-{item.Name}", out var decoded);
                Require(decoded.frameCount == item.Expected.Length && pcm.Length == item.Expected.Length * 2 &&
                    item.Expected.Select((value, index) => BitConverter.ToInt16(pcm, index * 2) == value).All(value => value), "WAV sample conversion/clipping failed.");
            }
            RejectAsset(Wave(32, 3, BitConverter.GetBytes(float.NaN)), info, directory, "pcm-wave-nonfinite");
        }
        finally { Reader.StrokeReader_End(); }
        Console.WriteLine("[IMM_SOUND_PCM_WAV] Passed integer 8/16/24/32-bit and float32 clipping, exact PCM16 and nonfinite-sample rejection.");
    }
    private static byte[] Wave(int bits, int codec, byte[] data)
    {
        using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream);
        writer.Write(Encoding.ASCII.GetBytes("RIFF")); writer.Write(36 + data.Length + (data.Length & 1)); writer.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
        writer.Write(16); writer.Write((ushort)codec); writer.Write((ushort)1); writer.Write(48000); writer.Write(48000 * bits / 8);
        writer.Write((ushort)(bits / 8)); writer.Write((ushort)bits); writer.Write(Encoding.ASCII.GetBytes("data")); writer.Write(data.Length); writer.Write(data);
        if ((data.Length & 1) != 0) writer.Write((byte)0); writer.Flush(); return stream.ToArray();
    }
    public static void ReaderOnly(string library, string path)
    {
        NativeLibrary.SetDllImportResolver(typeof(Reader).Assembly, (name, assembly, searchPath) => name == "ImmStrokeReader" ? NativeLibrary.Load(library) : IntPtr.Zero);
        Require(Reader.StrokeReader_Init(null) == 0, "Standalone reader init failed."); int doc = Reader.StrokeReader_LoadFromFile(path); Require(doc > 0, "Standalone reader load failed.");
        try
        {
            int sound = -1;
            for (int index = 0; index < Reader.StrokeReader_GetAuthoringLayerCount(doc); index++)
                if (Reader.StrokeReader_GetSoundAssetInfo(doc, index, out _)) { sound = index; break; }
            Require(sound >= 0 && Decode(doc, sound, out _).Length > 0, "Standalone reader PCM decode failed.");
            if (OperatingSystem.IsWindows())
            {
                var name = new StringBuilder(1024); Require(GetModuleFileName(GetModuleHandle("opus.dll"), name, name.Capacity) > 0 &&
                    string.Equals(Path.GetDirectoryName(name.ToString()), Path.GetDirectoryName(library), StringComparison.OrdinalIgnoreCase), "Standalone reader resolved Opus outside its own package.");
            }
        }
        finally { Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        Console.WriteLine("[IMM_READER_PCM_STANDALONE] Reader-only folder decoded PCM with its own shared runtime dependencies.");
    }
    public static void Standalone(string reader, string directory)
    {
        if (!OperatingSystem.IsWindows()) return;
        string folder = Path.GetFullPath(Path.Combine(directory, "reader-only")); Directory.CreateDirectory(folder);
        foreach (string file in Directory.GetFiles(Path.GetDirectoryName(reader), "*.dll")) File.Copy(file, Path.Combine(folder, Path.GetFileName(file)), true);
        string binary = Path.Combine(folder, Path.GetFileName(reader));
        var start = new ProcessStartInfo(Environment.ProcessPath) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true };
        if (Path.GetFileNameWithoutExtension(Environment.ProcessPath).Equals("dotnet", StringComparison.OrdinalIgnoreCase)) start.ArgumentList.Add(System.Reflection.Assembly.GetExecutingAssembly().Location);
        foreach (string argument in new[] { "--reader-only-pcm", binary, Path.GetFullPath(Path.Combine(directory, "sound-opus.imm")) }) start.ArgumentList.Add(argument);
        using var process = Process.Start(start); var stdout = process.StandardOutput.ReadToEndAsync(); var stderr = process.StandardError.ReadToEndAsync();
        if (!process.WaitForExit(60000)) { process.Kill(true); throw new TimeoutException("Owned reader-only test process timed out."); }
        string output = stdout.GetAwaiter().GetResult() + stderr.GetAwaiter().GetResult(); File.WriteAllText(Path.Combine(directory, "reader-only.log"), output);
        Require(process.ExitCode == 0 && output.Contains("[IMM_READER_PCM_STANDALONE]"), "Reader-only child failed; inspect reader-only.log.");
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
