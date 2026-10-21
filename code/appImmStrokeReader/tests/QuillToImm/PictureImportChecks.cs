using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImmPlayer;
using ImmPlayer.Authoring;
using Reader = ImmPlayer.ImmStrokeReader;

static class PictureImportChecks
{
    public static void Run(string path, string directory)
    {
        byte[] sourceBytes = File.ReadAllBytes(path);
        var expected = new List<(StrokePictureAssetInfo Info, byte[] Bytes)>();
        var sourcePictureIds = new HashSet<int>();
        Require(Marshal.SizeOf<StrokePictureAssetInfo>() == 32 && !Reader.StrokeReader_GetPictureAssetInfo(-1, 0, out _), "Picture asset ABI/init guard failed.");
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "picture-import-query.log")) == 0, "Reader init failed.");
        int disk = Reader.StrokeReader_LoadFromFile(path), memory = 0;
        var pinned = GCHandle.Alloc(sourceBytes, GCHandleType.Pinned);
        try
        {
            memory = Reader.StrokeReader_LoadFromMemory(pinned.AddrOfPinnedObject(), sourceBytes.Length);
            Require(disk > 0 && memory > 0, "Picture file/memory read failed.");
            int count = Reader.StrokeReader_GetAuthoringLayerCount(disk);
            for (int index = 0; index < count; index++)
            {
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(disk, index, out var layer), "Picture hierarchy query failed.");
                bool found = Reader.StrokeReader_GetPictureAssetInfo(disk, index, out var info);
                Require(found == (layer.type == 4), "Picture asset used a legacy-layer index or wrong layer type.");
                if (!found) continue;
                sourcePictureIds.Add(layer.id);
                Require(info.image.sourceVersion == 0 && Reader.StrokeReader_GetPictureAssetInfo(memory, index, out var memoryInfo) && info.Equals(memoryInfo),
                    "Picture asset file/memory metadata differ.");
                byte[] bytes = new byte[info.image.byteCount], memoryBytes = new byte[info.image.byteCount];
                Require(Reader.StrokeReader_GetPictureAssetData(disk, index, bytes, bytes.Length) == bytes.Length &&
                    Reader.StrokeReader_GetPictureAssetData(memory, index, memoryBytes, memoryBytes.Length) == memoryBytes.Length && bytes.SequenceEqual(memoryBytes),
                    "Picture asset file/memory bytes differ.");
                Require(Reader.StrokeReader_GetPictureAssetData(disk, index, memoryBytes, memoryBytes.Length - 1) == 0 &&
                    Reader.StrokeReader_GetPictureAssetData(disk, index, null, 0) == 0, "Picture asset buffer guard failed.");
                expected.Add((info, bytes));
            }
            Require(expected.Count > 0 && !Reader.StrokeReader_GetPictureAssetInfo(disk, -1, out _) &&
                !Reader.StrokeReader_GetPictureAssetInfo(disk, count, out _), "Picture asset bounds guard failed.");
        }
        finally
        {
            if (disk > 0) Reader.StrokeReader_Unload(disk);
            if (memory > 0) Reader.StrokeReader_Unload(memory);
            pinned.Free(); Reader.StrokeReader_End();
        }
        Require(!Reader.StrokeReader_GetPictureAssetInfo(disk, 0, out _) &&
            Reader.StrokeReader_GetPictureAssetData(disk, 0, new byte[1], 1) == 0, "Picture asset query survived shutdown.");
        var imported = ImmAuthoringImporter.ImportFromFile(path);
        var fromMemory = ImmAuthoringImporter.ImportFromMemory(sourceBytes);
        Require(imported.Succeeded && fromMemory.Succeeded, $"Picture import failed: {imported.Message}; {fromMemory.Message}");
        using (imported.Document)
        using (fromMemory.Document)
        {
            var snapshot = imported.Document.CreateSnapshot().Value;
            var pictures = snapshot.Layers.Where(x => x.Type == ImmAuthoringLayerType.Picture).ToArray();
            Require(pictures.Length == expected.Count && !imported.Issues.Any(x =>
                x.Code == ImmAuthoringImportIssueCode.UnsupportedLayerType && sourcePictureIds.Contains(x.SourceLayerId)),
                "Picture import omitted picture content.");
            for (int i = 0; i < pictures.Length; i++)
            {
                var properties = pictures[i].Properties;
                var image = properties.PictureImage;
                var item = expected[i];
                Require(image != null && image.Width == item.Info.image.width && image.Height == item.Info.image.height &&
                    image.Channels == item.Info.image.channels && (int)image.Encoding == item.Info.image.encoding && !image.EncodedBgr &&
                    image.CopyData().SequenceEqual(item.Bytes) && (int)properties.PictureType == item.Info.contentType &&
                    properties.PictureViewerLocked == (item.Info.isViewerLocked != 0), "Imported picture metadata or encoded bytes changed.");
            }
            Require(ImmAuthoringStructuralComparer.Compare(snapshot, fromMemory.Document.CreateSnapshot().Value).Equivalent,
                "Picture file/memory graph imports differ.");
            string output = Path.Combine(directory, $"{Path.GetFileNameWithoutExtension(path)}-import-roundtrip.imm");
            var exported = ImmAuthoringCompiler.ExportToFile(imported.Document, output);
            Require(exported.Succeeded, exported.Message);
            var again = ImmAuthoringImporter.ImportFromFile(output);
            Require(again.Succeeded, again.Message);
            using (again.Document)
                Require(ImmAuthoringStructuralComparer.Compare(snapshot, again.Document.CreateSnapshot().Value).Equivalent,
                    "Picture import/export/import changed the graph or encoded image bytes.");
        }
        Console.WriteLine($"[IMM_PICTURE_IMPORT] Passed {expected.Count} encoded picture file/memory queries and graph round trips; other unsupported layers reported={imported.Issues.Count(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedLayerType)}.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
