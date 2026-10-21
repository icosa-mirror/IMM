using System.Text;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Native = ImmPlayer.Exporter.Native;
using Reader = ImmPlayer.ImmStrokeReader;

static class RootPropertyChecks
{
    public static void Run(string directory)
    {
        Directory.CreateDirectory(directory);
        string output = Path.Combine(directory, "root-properties.imm");
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0, 0, 0, 1)).Value;
        Require(!document.CreateSnapshot().Value.RootProperties.HasValue, "Legacy default root became explicit.");
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
        var exported = ImmAuthoringCompiler.ExportToFile(document, output);
        Require(exported.Succeeded, exported.Message);
        AssertSerializedRoot(output, root);
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "root-properties.log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetDocumentInfo(doc, out var info) && info.rootAnimationKeyCount == 1 &&
                Reader.StrokeReader_GetRootAnimationKey(doc, 0, out var key) && key.property == 0 && key.boolValue == 0,
                "Root visibility did not survive native read-back.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        Console.WriteLine("[IMM_ROOT_PROPERTY_TEST] Passed root timing/opacity/transform/pivot/flips/visibility serialization and graph transaction checks.");
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
