using System;
using System.IO;
using System.Linq;
using System.Text;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;

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
        Console.WriteLine("[IMM_PREVIEW_ASSETS] immutable bytes, native encoding, exact encoded export, comparison and output guards passed.");
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
