using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Native = ImmPlayer.Exporter.Native;
using Reader = ImmPlayer.ImmStrokeReader;

static class RootPropertyChecks
{
    [DllImport("ImmStrokeReader", EntryPoint = "StrokeReader_GetRootLayerProperties")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool QueryNullInfo(int doc, IntPtr info, out StrokeLayerTransform transform);
    [DllImport("ImmStrokeReader", EntryPoint = "StrokeReader_GetRootLayerProperties")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool QueryNullTransform(int doc, out StrokeAuthoringLayerInfo info, IntPtr transform);

    public static void Run(string directory)
    {
        Directory.CreateDirectory(directory);
        string output = Path.Combine(directory, "root-properties.imm");
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value;
        Require(!document.CreateSnapshot().Value.RootProperties.HasValue, "Legacy default root became explicit.");
        var implicitDefault = document.CreateSnapshot().Value;
        Require(document.SetRootProperties(ImmAuthoringLayerProperties.Default("Root")).Succeeded &&
            ImmAuthoringStructuralComparer.Compare(implicitDefault, document.CreateSnapshot().Value).Equivalent,
            "Explicit and implicit default roots were not semantically equivalent.");
        var root = ImmAuthoringLayerProperties.Default("Root");
        root.Visible = false;
        root.Opacity = 0.5f;
        root.Transform.Position = new Vector3(1, 2, 3);
        root.Transform.Scale = 2;
        root.Transform.Flip = ImmAuthoringFlip.X;
        root.Pivot.Position = new Vector3(4, 5, 6);
        root.Pivot.Scale = 0.5f;
        root.Pivot.Flip = ImmAuthoringFlip.Z;
        root.IsTimeline = true;
        root.DurationTicks = 45360000;
        root.MaxRepeatCount = 3;
        Require(document.SetRootProperties(root).Succeeded, "Valid root properties rejected.");
        var before = document.CreateSnapshot().Value;
        using (var transaction = document.BeginEdit(document.Revision).Value)
        {
            var edited = root; edited.DurationTicks++;
            Require(transaction.EditableDocument.SetRootProperties(edited).Succeeded, "Root transaction edit rejected.");
            Require(document.CreateSnapshot().Value.RootProperties.Value.DurationTicks == root.DurationTicks, "Uncommitted root escaped transaction.");
            Require(transaction.Commit().Succeeded, "Root transaction commit failed.");
        }
        Require(before.RootProperties.Value.DurationTicks == 45360000 && document.CreateSnapshot().Value.RootProperties.Value.DurationTicks == 45360001,
            "Root snapshots or transaction cloning lost their values.");
        Require(!ImmAuthoringStructuralComparer.Compare(before, document.CreateSnapshot().Value).Equivalent, "Comparer ignored root properties.");
        Require(document.SetRootProperties(root).Succeeded, "Root reset failed.");
        long revision = document.Revision;
        var invalid = root; invalid.DurationTicks = -1;
        Require(!document.SetRootProperties(invalid).Succeeded && document.Revision == revision, "Invalid root timing mutated the graph.");
        invalid = root; invalid.Transform.Flip = (ImmAuthoringFlip)99;
        Require(!document.SetRootProperties(invalid).Succeeded && document.Revision == revision, "Invalid root reflection mutated the graph.");
        var identity = new TransformNative { Qw = 1, Scale = 1 };
        Require(!Native.ImmExporter_SetRootLayerProperties(IntPtr.Zero, 1, 1, ref identity, ref identity, 0, 0, 0, 0, 0), "Null sequence accepted.");
        // IMM stores frame rate on paint, so include a child to make the document
        // rate observable rather than exercising the no-paint fallback warning.
        long paint = document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Child")).Value;
        long drawing = document.CreateDrawing(paint).Value;
        var a = new PaintPoint { Position = Vector3.zero, Normal = Vector3.up, Width = 0.01f, Alpha = 1, Color = new Color(1, 1, 1, 1) };
        var b = a; b.Position = new Vector3(1, 0, 0);
        Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, new[] { a, b }).Succeeded &&
            document.AppendFrame(paint, drawing).Succeeded, "Root child fixture failed.");
        var exported = ImmAuthoringCompiler.ExportToFile(document, output);
        Require(exported.Succeeded, exported.Message);
        AssertSerializedRoot(output, root);
        Require(!Reader.StrokeReader_GetRootLayerProperties(-1, out _, out _), "Root query succeeded before initialization.");
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "root-properties.log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetDocumentInfo(doc, out var info) && info.rootAnimationKeyCount == 1 &&
                Reader.StrokeReader_GetRootAnimationKey(doc, 0, out var key) && key.property == 0 && key.boolValue == 0,
                "Root visibility did not survive native read-back.");
            Require(Reader.StrokeReader_GetAuthoringLayerCount(doc) == 1 &&
                Reader.StrokeReader_GetRootLayerProperties(doc, out var properties, out var transform) &&
                properties.name == "Root" && properties.type == 0 && properties.parentId == -1 &&
                properties.visible == 0 && properties.opacity == 0.5f && properties.isTimeline == 1 &&
                properties.durationTicks == 45360000 && properties.maxRepeatCount == 3 &&
                properties.pivotScale == 0.5f && properties.pivotFlip == 3 && properties.pivotTransX == 4 &&
                properties.pivotTransY == 5 && properties.pivotTransZ == 6 && transform.scale == 2 && transform.flip == 1 &&
                transform.transX == 1 && transform.transY == 2 && transform.transZ == 3,
                "Root property query changed metadata or indexed child-layer counts.");
            Require(!Reader.StrokeReader_GetRootLayerProperties(-1, out _, out _) &&
                !QueryNullInfo(doc, IntPtr.Zero, out _) && !QueryNullTransform(doc, out _, IntPtr.Zero), "Invalid root query succeeded.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        Require(!Reader.StrokeReader_GetRootLayerProperties(doc, out _, out _), "Root query survived document unload/end.");
        var imported = ImmAuthoringImporter.ImportFromFile(output, Path.Combine(directory, "root-property-import.log"));
        Require(imported.Succeeded, imported.Message);
        using (var importedDocument = imported.Document)
        {
            Require(imported.Lossiness == ImmAuthoringImportLossiness.Lossless,
                $"Static root properties were reported lossy: {string.Join("; ", imported.Issues.Select(x => x.Message))}");
            // Stroke length/time are codec-derived, so compare the root settings
            // exactly rather than arbitrary pre-encoding child point metadata.
            var importedRoot = importedDocument.CreateSnapshot().Value.RootProperties;
            Require(importedRoot.HasValue && importedRoot.Value.Equals(root), "Imported static root properties changed.");
            string roundTrip = Path.Combine(directory, "root-properties-roundtrip.imm");
            var reexported = ImmAuthoringCompiler.ExportToFile(importedDocument, roundTrip);
            Require(reexported.Succeeded, reexported.Message);
            AssertSerializedRoot(roundTrip, root);
        }
        Console.WriteLine("[IMM_ROOT_PROPERTY_TEST] Passed root timing/opacity/transform/pivot/flips/visibility serialization and graph transaction checks.");
        Console.WriteLine("[IMM_ROOT_PROPERTY_IMPORT_TEST] Passed native root queries, bounds/null/lifetime checks and lossless static root import/re-export.");
        CheckRootOffsetImport(directory);
    }

    private static void CheckRootOffsetImport(string directory)
    {
        string sample = Path.GetFullPath("exampleImmFiles/sample1.imm");
        var defaults = ImmAuthoringImporter.ImportFromFile(sample, Path.Combine(directory, "root-default-offset.log"));
        Require(defaults.Succeeded, defaults.Message);
        using (defaults.Document)
            Require(!defaults.Issues.Any(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedRootAnimation) &&
                defaults.Document.CreateSnapshot().Value.RootAnimationKeys.Any(x => x.Property == ImmAuthoringAnimationProperty.Offset && x.Value.UIntValue == 0),
                "The default zero offset was omitted or misdiagnosed as unsupported root animation.");

        // Change only the tracked sample's root Offset key value in an artifact
        // copy. All channel lengths and resource offsets remain unchanged.
        byte[] bytes = File.ReadAllBytes(sample);
        int timeline = bytes.AsSpan().IndexOf(Encoding.ASCII.GetBytes("Timeline"));
        using var stream = new MemoryStream(bytes);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
        stream.Position = timeline + 16;
        Require(timeline >= 0 && reader.ReadUInt32() == 2, "Unexpected root fixture timeline layout.");
        int offsetValue = -1;
        for (int channel = 0; channel < 10; channel++)
        {
            ushort property = reader.ReadUInt16();
            ulong keys = reader.ReadUInt64();
            int valueBytes = property switch { 0 or 7 => 1, 1 or 6 or 8 => 4, 2 => 24, 3 => 32, 4 or 5 => 8, 9 => 65, _ => throw new InvalidDataException("Unexpected fixture channel.") };
            for (ulong k = 0; k < keys; k++)
            {
                long time = reader.ReadInt64();
                byte interpolation = reader.ReadByte();
                if (property == 8 && keys == 1 && time == 0 && interpolation == 0) offsetValue = checked((int)stream.Position);
                stream.Position += valueBytes;
            }
        }
        Require(offsetValue >= 0 && BitConverter.ToUInt32(bytes, offsetValue) == 0, "Default root offset fixture missing.");
        BitConverter.GetBytes(1u).CopyTo(bytes, offsetValue);
        string altered = Path.Combine(directory, "root-nondefault-offset.imm");
        File.WriteAllBytes(altered, bytes);
        var nondefault = ImmAuthoringImporter.ImportFromFile(altered, Path.Combine(directory, "root-nondefault-offset.log"));
        Require(nondefault.Succeeded, nondefault.Message);
        using (nondefault.Document)
            Require(!nondefault.Issues.Any(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedRootAnimation) &&
                nondefault.Document.CreateSnapshot().Value.RootAnimationKeys.Any(x => x.Property == ImmAuthoringAnimationProperty.Offset && x.Value.UIntValue == 1),
                "The nondefault root offset was omitted or misdiagnosed.");
        Console.WriteLine("[IMM_ROOT_OFFSET_IMPORT_TEST] Passed default and nondefault root-offset import.");
    }

    public static void AssertSerializedRoot(string path, ImmAuthoringLayerProperties expected)
    {
        // The writer emits Root as the first version-1 Layerdes chunk. Check the
        // actual binary metadata because the public reader does not expose root timing.
        byte[] bytes = File.ReadAllBytes(path);
        int offset = bytes.AsSpan().IndexOf(Encoding.ASCII.GetBytes("Layerdes"));
        Require(offset >= 0, "Root layer descriptor missing.");
        using var reader = new BinaryReader(new MemoryStream(bytes));
        reader.BaseStream.Position = offset + 16;
        Require(reader.ReadUInt32() == 1, "Unexpected root descriptor version.");
        AssertTransform(reader, expected.Transform);
        AssertTransform(reader, expected.Pivot);
        Require(reader.ReadSingle() == expected.Opacity && reader.ReadByte() == (expected.IsTimeline ? 1 : 0) &&
            reader.ReadUInt64() == (ulong)expected.DurationTicks && reader.ReadUInt32() == expected.MaxRepeatCount,
            "Serialized root opacity/timeline/duration/repeats changed.");
    }

    private static void AssertTransform(BinaryReader reader, ImmAuthoringTransform expected)
    {
        Require(reader.ReadDouble() == expected.Rotation.x && reader.ReadDouble() == expected.Rotation.y &&
            reader.ReadDouble() == expected.Rotation.z && reader.ReadDouble() == expected.Rotation.w &&
            reader.ReadDouble() == expected.Scale && reader.ReadDouble() == expected.Position.x &&
            reader.ReadDouble() == expected.Position.y && reader.ReadDouble() == expected.Position.z && reader.ReadByte() == (byte)expected.Flip,
            "Serialized root transform or pivot changed.");
    }

    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
