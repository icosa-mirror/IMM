using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;

static class PreviewAssetChecks
{
    public static void Run(string directory)
    {
        byte[] pixels = new byte[16 * 16 * 4];
        for (int i = 0; i < pixels.Length; i += 4)
        {
            pixels[i] = (byte)(i / 4); pixels[i + 1] = 80; pixels[i + 2] = 200; pixels[i + 3] = 255;
        }
        var created = ImmAuthoringImage.Create(16, 16, 4, ImmAuthoringImageEncoding.Rgba8, pixels);
        Require(created.Succeeded, created.Message);
        var image = created.Value;
        pixels[0] = 255;
        var exposed = image.CopyData(); exposed[0] = 255;
        Require(image.CopyData()[0] == 0, "Image byte ownership escaped.");
        Require(!ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Rgb8, pixels).Succeeded &&
            !ImmAuthoringImage.Create(int.MaxValue, int.MaxValue, 4, ImmAuthoringImageEncoding.Rgba8, pixels).Succeeded,
            "Invalid dimensions or payload accepted.");
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value;
        var properties = ImmAuthoringLayerProperties.Default("Preview");
        properties.SpawnAreaPreview = image;
        Require(!document.CreateGroupLayer(0, properties).Succeeded && !document.SetRootProperties(properties).Succeeded,
            "Unsupported preview ownership accepted.");
        long id = document.CreateSpawnAreaLayer(0, properties).Value;
        var before = document.CreateSnapshot().Value;
        string rawPath = Path.Combine(directory, "preview-raw.imm");
        var exported = ImmAuthoringCompiler.ExportToFile(document, rawPath);
        Require(exported.Succeeded, exported.Message);
        byte[] jpeg = ReadAsset(rawPath, 2);
        Require(jpeg.Length > 3 && jpeg[0] == 255 && jpeg[1] == 216, "Raw preview did not encode as JPEG.");
        var encoded = ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Jpeg, jpeg);
        Require(encoded.Succeeded, encoded.Message);
        properties.SpawnAreaPreview = encoded.Value;
        Require(document.SetLayerProperties(id, properties).Succeeded, "Encoded preview setup failed.");
        Require(before.Layers[0].Properties.SpawnAreaPreview.Encoding == ImmAuthoringImageEncoding.Rgba8 &&
            !ImmAuthoringStructuralComparer.Compare(before, document.CreateSnapshot().Value).Equivalent,
            "Snapshot or structural comparison lost image changes.");
        string encodedPath = Path.Combine(directory, "preview-encoded.imm");
        exported = ImmAuthoringCompiler.ExportToFile(document, encodedPath);
        Require(exported.Succeeded && ReadAsset(encodedPath, 2).SequenceEqual(jpeg), "Encoded JPEG was recompressed or altered.");
        CheckReadBack(directory, encodedPath, jpeg, false);
        properties.SpawnAreaPreview = ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Jpeg, jpeg, true).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded, "BGR fixture setup failed.");
        string bgrPath = Path.Combine(directory, "preview-bgr.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, bgrPath).Succeeded, "BGR fixture export failed.");
        CheckReadBack(directory, bgrPath, jpeg, true);
        byte[] png = Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGO4oxH1HwAFoAJeZn+M9QAAAABJRU5ErkJggg==");
        properties.SpawnAreaPreview = ImmAuthoringImage.Create(1, 1, 4, ImmAuthoringImageEncoding.Png, png).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded, "PNG fixture setup failed.");
        string pngPath = Path.Combine(directory, "preview-png.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, pngPath).Succeeded, "PNG fixture export failed.");
        CheckReadBack(directory, pngPath, png, false, 1, 1, 4, 2);
        properties.SpawnAreaPreview = null;
        Require(document.SetLayerProperties(id, properties).Succeeded, "Absent preview fixture setup failed.");
        string absentPath = Path.Combine(directory, "preview-absent.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, absentPath).Succeeded, "Absent fixture export failed.");
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "preview-absent.log")) == 0, "Reader init failed.");
        int absentDoc = Reader.StrokeReader_LoadFromFile(absentPath);
        try { Require(absentDoc > 0 && !Reader.StrokeReader_GetSpawnAreaPreviewInfo(absentDoc, 0, out _), "Implicit blank became a declared preview."); }
        finally { if (absentDoc > 0) Reader.StrokeReader_Unload(absentDoc); Reader.StrokeReader_End(); }
        properties.SpawnAreaPreview = encoded.Value;
        Require(document.SetLayerProperties(id, properties).Succeeded, "Image budget fixture reset failed.");
        var limited = new ImmAuthoringOperationOptions(limits: new ImmAuthoringLimits(maxInputBytes: 1));
        byte[] saved = File.ReadAllBytes(encodedPath);
        Require(!ImmAuthoringCompiler.ExportToFile(document, encodedPath, limited).Succeeded &&
            saved.SequenceEqual(File.ReadAllBytes(encodedPath)), "Image budget failure replaced existing output.");
        Require(!Native.ImmExporter_SpawnAreaSetPreview(IntPtr.Zero, 16, 16, 4, 1, 2, pixels, (ulong)pixels.Length),
            "Null native preview target accepted.");
        // Valid header with invalid image data must fail at the native decoder and preserve output.
        properties.SpawnAreaPreview = ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Jpeg, new byte[] { 255, 216, 255 }).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded, "Malformed codec fixture setup failed.");
        Require(!ImmAuthoringCompiler.ExportToFile(document, encodedPath).Succeeded &&
            saved.SequenceEqual(File.ReadAllBytes(encodedPath)), "Malformed image replaced existing output.");
        Console.WriteLine("[IMM_PREVIEW_ASSETS] immutable bytes, native encoding, exact JPEG/PNG file/memory read-back and import/re-export, BGR version, absent previews and output guards passed.");
    }

    private static void CheckReadBack(string directory, string path, byte[] expected, bool bgr,
        int width = 16, int height = 16, int channels = 3, int encoding = 3)
    {
        Require(Marshal.SizeOf<StrokeImageAssetInfo>() == 24 &&
            !Reader.StrokeReader_GetSpawnAreaPreviewInfo(-1, 0, out _), "Preview ABI/init guard failed.");
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "preview-reader.log")) == 0, "Reader init failed.");
        byte[] file = File.ReadAllBytes(path);
        var pinned = GCHandle.Alloc(file, GCHandleType.Pinned);
        int disk = 0, memory = 0;
        try
        {
            disk = Reader.StrokeReader_LoadFromFile(path);
            memory = Reader.StrokeReader_LoadFromMemory(pinned.AddrOfPinnedObject(), file.Length);
            foreach (int doc in new[] { disk, memory })
            {
                Require(doc > 0 && Reader.StrokeReader_GetSpawnAreaPreviewInfo(doc, 0, out var info) &&
                    info.width == width && info.height == height && info.channels == channels && info.encoding == encoding &&
                    info.sourceVersion == (bgr ? 1 : 2) && info.byteCount == expected.Length, "Preview metadata changed.");
                byte[] bytes = new byte[expected.Length];
                Require(Reader.StrokeReader_GetSpawnAreaPreviewData(doc, 0, bytes, bytes.Length) == bytes.Length && bytes.SequenceEqual(expected),
                    "Original preview bytes changed.");
                bytes[0] = 42;
                Require(Reader.StrokeReader_GetSpawnAreaPreviewData(doc, 0, bytes, bytes.Length - 1) == 0 && bytes[0] == 42 &&
                    Reader.StrokeReader_GetSpawnAreaPreviewData(doc, 0, null, 0) == 0 &&
                    !Reader.StrokeReader_GetSpawnAreaPreviewInfo(doc, -1, out _) &&
                    !Reader.StrokeReader_GetSpawnAreaPreviewInfo(doc, 1, out _), "Preview bounds/buffer guard failed.");
            }
        }
        finally
        {
            if (disk > 0) Reader.StrokeReader_Unload(disk);
            if (memory > 0) Reader.StrokeReader_Unload(memory);
            pinned.Free(); Reader.StrokeReader_End();
        }
        Require(!Reader.StrokeReader_GetSpawnAreaPreviewInfo(disk, 0, out _), "Preview query survived shutdown.");
        var imported = ImmAuthoringImporter.ImportFromFile(path, Path.Combine(directory, "preview-import.log"));
        Require(imported.Succeeded, imported.Message);
        using (imported.Document)
        {
            var image = imported.Document.CreateSnapshot().Value.Layers.Single().Properties.SpawnAreaPreview;
            Require(image != null && image.EncodedBgr == bgr && image.CopyData().SequenceEqual(expected), "Import omitted or changed preview.");
            string roundTrip = Path.Combine(directory, $"{Path.GetFileNameWithoutExtension(path)}-roundtrip.imm");
            Require(ImmAuthoringCompiler.ExportToFile(imported.Document, roundTrip).Succeeded && ReadAsset(roundTrip, encoding == 2 ? 1u : 2u).SequenceEqual(expected),
                "Import/re-export altered encoded preview.");
        }
    }

    private static byte[] ReadAsset(string path, uint expectedFormat)
    {
        byte[] file = File.ReadAllBytes(path);
        byte[] marker = Encoding.ASCII.GetBytes("Restable");
        int table = -1;
        for (int i = 0; i <= file.Length - marker.Length; i++)
            if (file.AsSpan(i, marker.Length).SequenceEqual(marker)) { table = i; break; }
        Require(table >= 0 && BitConverter.ToUInt32(file, table + 16) == 1, "Expected one preview asset.");
        int offset = checked((int)BitConverter.ToUInt64(file, table + 24));
        int size = checked((int)BitConverter.ToUInt64(file, table + 32));
        Require(BitConverter.ToUInt32(file, offset) == expectedFormat && BitConverter.ToUInt64(file, offset + 4) == (ulong)(size - 4),
            "Preview asset framing changed.");
        return file.AsSpan(offset + 12, size - 12).ToArray();
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
