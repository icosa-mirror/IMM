using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;

static class PictureAuthoringChecks
{
    public static void Run(string directory)
    {
        byte[] pixels = { 220, 40, 90, 255, 10, 60, 200, 100, 90, 180, 30, 0, 10, 20, 30, 255 };
        var image = ImmAuthoringImage.Create(2, 2, 4, ImmAuthoringImageEncoding.Rgba8, pixels).Value;
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0, 0, 0, 1)).Value;
        var missing = ImmAuthoringLayerProperties.Default("Missing");
        Require(!document.CreatePictureLayer(0, missing).Succeeded, "Picture without an image accepted.");
        long parent = document.CreateGroupLayer(0, ImmAuthoringLayerProperties.Default("Pictures")).Value;
        long first = 0;
        for (int type = 0; type <= 4; type++)
        {
            var properties = ImmAuthoringLayerProperties.Default($"Picture {type}");
            properties.PictureType = (ExportPictureType)type;
            properties.PictureViewerLocked = type % 2 != 0;
            properties.PictureImage = image;
            properties.Transform.Position = new Vector3(1, 2, 3);
            properties.Transform.Flip = ImmAuthoringFlip.X;
            properties.Pivot.Position = new Vector3(4, 5, 6);
            properties.Pivot.Flip = ImmAuthoringFlip.Z;
            properties.Opacity = 0.5f;
            properties.IsTimeline = true; properties.DurationTicks = 126000; properties.MaxRepeatCount = 3;
            var created = document.CreatePictureLayer(parent, properties);
            Require(created.Succeeded, created.Message);
            if (type == 0) first = created.Value;
            Require(!document.CreateDrawing(created.Value).Succeeded && !document.CreateGroupLayer(0, properties).Succeeded,
                "Picture/paint/image ownership escaped.");
        }
        var before = document.CreateSnapshot().Value;
        using (var edit = document.BeginEdit(document.Revision).Value)
        {
            var changed = before.Layers.Single(x => x.Id == first).Properties;
            changed.PictureViewerLocked = true;
            Require(edit.EditableDocument.SetLayerProperties(first, changed).Succeeded && edit.Commit().Succeeded, "Picture transaction failed.");
        }
        Require(!before.Layers.Single(x => x.Id == first).Properties.PictureViewerLocked &&
            !ImmAuthoringStructuralComparer.Compare(before, document.CreateSnapshot().Value).Equivalent,
            "Picture snapshot/transaction/comparison failed.");
        var reset = before.Layers.Single(x => x.Id == first).Properties;
        Require(document.SetLayerProperties(first, reset).Succeeded, "Picture reset failed.");
        var invalid = reset; invalid.PictureType = (ExportPictureType)99;
        Require(!document.SetLayerProperties(first, invalid).Succeeded, "Invalid picture projection accepted.");
        string output = Path.Combine(directory, "picture-graph.imm");
        var exported = ImmAuthoringCompiler.ExportToFile(document, output);
        Require(exported.Succeeded, exported.Message);
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "picture-graph.log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetLayerCount(doc) == 5 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 6,
                "Picture layer count changed.");
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var group), "Picture parent missing.");
            for (int type = 0; type <= 4; type++)
            {
                Require(Reader.StrokeReader_GetPictureInfo(doc, type, out var info) && info.contentType == type &&
                    info.isViewerLocked == type % 2 && info.width == 2 && info.height == 2 && info.hasAlpha == 1 && info.dataSize == pixels.Length,
                    "Picture projection/viewer lock/image metadata changed.");
                byte[] actual = new byte[pixels.Length];
                var pinned = GCHandle.Alloc(actual, GCHandleType.Pinned);
                try
                {
                    Require(Reader.StrokeReader_GetPicturePixelData(doc, type, pinned.AddrOfPinnedObject(), actual.Length) == actual.Length && actual.SequenceEqual(pixels),
                        "Picture RGBA colors/alpha changed.");
                }
                finally { pinned.Free(); }
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, type + 1, out var layer) && layer.opacity == 0.5f &&
                    layer.parentId == group.id && layer.childIndex == type && layer.pivotFlip == 3 && layer.pivotTransX == 4 &&
                    layer.isTimeline == 1 && layer.durationTicks == 126000 && layer.maxRepeatCount == 3 &&
                    Reader.StrokeReader_GetAuthoringLayerTransform(doc, type + 1, out var local, out _) &&
                    local.flip == 1 && local.transX == 1 && local.transY == 2 && local.transZ == 3,
                    "Picture common properties changed.");
            }
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        PictureImportChecks.Run(output, directory);
        byte[] saved = File.ReadAllBytes(output);
        Require(!ImmAuthoringCompiler.ExportToFile(document, output,
                new ImmAuthoringOperationOptions(limits: new ImmAuthoringLimits(maxInputBytes: 1))).Succeeded &&
            File.ReadAllBytes(output).SequenceEqual(saved), "Picture limits replaced existing output.");
        Require(!Native.ImmExporter_PictureSetImage(IntPtr.Zero, 2, 2, 4, 1, pixels, (ulong)pixels.Length), "Null native picture accepted.");
        CheckEncoded(directory);
        PictureImportChecks.Run(Path.GetFullPath("exampleImmFiles/sample1.imm"), directory);
        Console.WriteLine("[IMM_PICTURE_AUTHORING] Passed picture projections/viewer lock, exact RGBA/alpha, common properties, transactions, encoded assets and output guards.");
    }

    private static void CheckEncoded(string directory)
    {
        byte[] png = Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGO4oxH1HwAFoAJeZn+M9QAAAABJRU5ErkJggg==");
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value;
        var properties = ImmAuthoringLayerProperties.Default("Encoded");
        properties.PictureImage = ImmAuthoringImage.Create(1, 1, 4, ImmAuthoringImageEncoding.Png, png).Value;
        long id = document.CreatePictureLayer(0, properties).Value;
        string output = Path.Combine(directory, "picture-encoded.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, output).Succeeded && PreviewAssetChecks.ReadAsset(output, 1).SequenceEqual(png),
            "Encoded picture PNG was changed.");
        PictureImportChecks.Run(output, directory);
        byte[] jpeg = PreviewAssetChecks.ReadAsset(Path.Combine(directory, "preview-encoded.imm"), 2);
        properties.PictureImage = ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Jpeg, jpeg).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded && ImmAuthoringCompiler.ExportToFile(document, output).Succeeded &&
            PreviewAssetChecks.ReadAsset(output, 2).SequenceEqual(jpeg), "Encoded picture JPEG was recompressed or changed.");
        PictureImportChecks.Run(output, directory);
        properties.PictureImage = ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Jpeg, jpeg, true).Value;
        Require(!document.SetLayerProperties(id, properties).Succeeded, "BGR picture payload accepted.");
        byte[] gray = Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVR4nGOoBgAAfQB8EB20bAAAAABJRU5ErkJggg==");
        properties.PictureImage = ImmAuthoringImage.Create(1, 1, 1, ImmAuthoringImageEncoding.Png, gray).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded && ImmAuthoringCompiler.ExportToFile(document, output).Succeeded &&
            PreviewAssetChecks.ReadAsset(output, 1).SequenceEqual(gray), "Grayscale picture asset changed.");
        PictureImportChecks.Run(output, directory);
        CheckGrayPixels(output, new byte[] { 123, 123, 123, 255 });
        byte[] packedGray = Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAIAAAABAQAAAADcWUInAAAACklEQVR4nGNoAAAAggCBd81ytgAAAABJRU5ErkJggg==");
        properties.PictureImage = ImmAuthoringImage.Create(2, 1, 1, ImmAuthoringImageEncoding.Png, packedGray).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded && ImmAuthoringCompiler.ExportToFile(document, output).Succeeded,
            "Packed grayscale picture export failed.");
        CheckGrayPixels(output, new byte[] { 255, 255, 255, 255, 0, 0, 0, 255 });
        PictureImportChecks.Run(output, directory);
        properties.Visible = false;
        Require(document.SetLayerProperties(id, properties).Succeeded && ImmAuthoringCompiler.ExportToFile(document, output).Succeeded,
            "Hidden picture export failed.");
        PictureImportChecks.Run(output, directory);
        properties.PictureImage = ImmAuthoringImage.Create(1, 1, 4, ImmAuthoringImageEncoding.Png,
            new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 }).Value;
        Require(document.SetLayerProperties(id, properties).Succeeded, "Malformed picture fixture setup failed.");
        byte[] saved = File.ReadAllBytes(output);
        Require(!ImmAuthoringCompiler.ExportToFile(document, output).Succeeded && saved.SequenceEqual(File.ReadAllBytes(output)),
            "Malformed encoded picture replaced existing output.");
    }

    private static void CheckGrayPixels(string path, byte[] expected)
    {
        Require(Reader.StrokeReader_Init(null) == 0, "Gray reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        byte[] actual = new byte[expected.Length];
        var pinned = GCHandle.Alloc(actual, GCHandleType.Pinned);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetPicturePixelData(doc, 0, pinned.AddrOfPinnedObject(), actual.Length) == actual.Length &&
                actual.SequenceEqual(expected), "Decoded grayscale values or alpha changed.");
        }
        finally { pinned.Free(); if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
