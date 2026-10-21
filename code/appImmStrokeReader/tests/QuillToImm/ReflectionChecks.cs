using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;
using Native = ImmPlayer.Exporter.Native;

static class ReflectionChecks
{
    public static void Run(string root)
    {
        Require(Marshal.SizeOf<TransformNative>() == 32, "Legacy transform ABI changed.");
        string path = Path.Combine(root, "reflections.imm");
        using (var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1)).Value)
        {
            foreach (ImmAuthoringFlip axis in new[] { ImmAuthoringFlip.X, ImmAuthoringFlip.Y, ImmAuthoringFlip.Z })
            {
                var properties = ImmAuthoringLayerProperties.Default($"Flip{axis}");
                properties.Transform.Flip = axis;
                properties.Transform.Position = new Vector3((int)axis, 2, 3);
                properties.Pivot.Flip = Next(axis);
                long group = document.CreateGroupLayer(0, properties).Value;
                for (int key = 0; key < 3; key++)
                {
                    var t = properties.Transform;
                    t.Flip = key == 0 ? axis : key == 1 ? Next(axis) : ImmAuthoringFlip.None;
                    Require(document.CreateAnimationKey(group, ImmAuthoringAnimationProperty.Transform, key * 525,
                        ImmAuthoringAnimationValue.FromTransform(t), ImmAuthoringInterpolation.None).Succeeded, "Reflection key rejected.");
                }
                var paint = ImmAuthoringLayerProperties.Default("Paint");
                paint.Transform.Flip = axis;
                paint.Pivot.Flip = Next(axis);
                long layer = document.CreatePaintLayer(group, paint).Value;
                long drawing = document.CreateDrawing(layer).Value;
                var a = new PaintPoint { Position = new Vector3(0, 0, 0), Normal = Vector3.up,
                    Color = new Color(1, 0.5f, 0.2f, 1), Alpha = 1, Width = 0.01f };
                var b = a; b.Position = new Vector3(1, 0, 0); b.Length = 1;
                Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, new[] { a, b }).Succeeded, "Paint creation failed.");
                Require(document.AppendFrame(layer, drawing).Succeeded, "Frame creation failed.");
            }
            var invalid = ImmAuthoringLayerProperties.Default("Invalid");
            invalid.Transform.Flip = (ImmAuthoringFlip)4;
            Require(!document.CreateGroupLayer(0, invalid).Succeeded, "Unknown graph flip accepted.");
            invalid.Transform.Flip = (ImmAuthoringFlip)(-1);
            Require(!document.CreateGroupLayer(0, invalid).Succeeded, "Negative graph flip accepted.");
            var export = ImmAuthoringCompiler.ExportToFile(document, path);
            Require(export.Succeeded, export.Message);
        }
        Require(Reader.StrokeReader_Init(Path.ChangeExtension(path, ".log")) == 0, "Native init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 6, "Reflected layer count changed.");
            for (int index = 0; index < 6; index++)
            {
                int axis = index / 2 + 1;
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, index, out var info), "Layer info missing.");
                Require(Reader.StrokeReader_GetAuthoringLayerTransform(doc, index, out var transform, out _), "Transform missing.");
                Require(transform.flip == axis && info.pivotFlip == (int)Next((ImmAuthoringFlip)axis), "Layer or pivot reflection was lost.");
                if (index % 2 != 0) continue;
                var keys = new List<StrokeAnimationKey>();
                for (int k = 0; k < Reader.StrokeReader_GetLayerAnimationKeyCount(doc, index); k++)
                {
                    Require(Reader.StrokeReader_GetLayerAnimationKey(doc, index, k, out var key), "Key missing.");
                    if (key.property == (int)ImmAuthoringAnimationProperty.Transform) keys.Add(key);
                }
                Require(keys.Count == 3 && keys.Select(x => x.transformValue.flip).SequenceEqual(new[] { axis, (int)Next((ImmAuthoringFlip)axis), 0 }),
                    "Key reflection or legacy no-flip key path changed.");
            }
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }

        var imported = ImmAuthoringImporter.ImportFromFile(path, Path.Combine(root, "reflection-import.log"));
        Require(imported.Succeeded && !imported.Issues.Any(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedFlip), "Valid flips were treated as lossy import.");
        using (var document = imported.Document)
        {
            var before = document.CreateSnapshot().Value;
            foreach (var layer in before.Layers)
            {
                Require(layer.Properties.Transform.Flip != ImmAuthoringFlip.None && layer.Properties.Pivot.Flip != ImmAuthoringFlip.None,
                    "Importer dropped a layer/pivot reflection.");
                if (layer.Type == ImmAuthoringLayerType.Group)
                    Require(layer.AnimationKeys.Where(x => x.Property == ImmAuthoringAnimationProperty.Transform)
                        .Select(x => x.Value.TransformValue.Flip).SequenceEqual(new[] { layer.Properties.Transform.Flip, Next(layer.Properties.Transform.Flip), ImmAuthoringFlip.None }),
                        "Importer dropped transform-key flips.");
            }
            var first = before.Layers[0];
            var changed = first.Properties; changed.Transform.Flip = ImmAuthoringFlip.None;
            Require(document.SetLayerProperties(first.Id, changed).Succeeded, "Property update failed.");
            var comparison = ImmAuthoringStructuralComparer.Compare(before, document.CreateSnapshot().Value);
            Require(!comparison.Equivalent && comparison.Differences.Any(x => x.Path.EndsWith(".flip")), "Structural comparison ignored a reflection.");
        }
        using (var sequence = ExportSequence.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1), default))
        {
            var layer = sequence.CreateGroupLayer("InvalidNative");
            Require(Native.ImmExporter_LayerSetTransformFlips(layer.Handle, 1, 2), "Valid native flips rejected.");
            Require(!Native.ImmExporter_LayerSetTransformFlips(layer.Handle, 3, 4), "Invalid pivot enum accepted.");
            Require(!Native.ImmExporter_LayerSetTransformFlips(layer.Handle, -1, 0), "Negative enum accepted.");
            Require(!Native.ImmExporter_LayerSetTransformFlips(IntPtr.Zero, 0, 0), "Null native layer accepted.");
            var t = TransformUtils.IdentityTransform();
            Require(!Native.ImmExporter_LayerAddAnimationKeyWithFlip(layer.Handle, (int)ImmAuthoringAnimationProperty.Opacity, 0, 0, 0, 0, 1, 0, ref t, 1),
                "Reflection accepted on a non-transform key.");
            Require(!Native.ImmExporter_LayerAddAnimationKeyWithFlip(layer.Handle, (int)ImmAuthoringAnimationProperty.Transform, 0, 0, 0, 0, 0, 0, ref t, 4),
                "Invalid key reflection accepted.");
            string invalidPath = Path.Combine(root, "invalid-native.imm");
            Require(sequence.ExportToFile(invalidPath), "Native enum check export failed.");
            Require(Reader.StrokeReader_Init(Path.Combine(root, "invalid-native.log")) == 0, "Native init failed.");
            int id = Reader.StrokeReader_LoadFromFile(invalidPath);
            try
            {
                Require(id > 0 && Reader.StrokeReader_GetAuthoringLayerInfo(id, 0, out var info) && info.pivotFlip == 2, "Rejected setter changed pivot.");
                Require(Reader.StrokeReader_GetAuthoringLayerTransform(id, 0, out var value, out _) && value.flip == 1, "Rejected setter changed transform.");
            }
            finally { if (id > 0) Reader.StrokeReader_Unload(id); Reader.StrokeReader_End(); }
        }
        Console.WriteLine("[IMM_QUILL_REFLECTION_TEST] Passed X/Y/Z layer/pivot/key export, import/comparison, enum rejection and legacy ABI checks.");
    }
    private static ImmAuthoringFlip Next(ImmAuthoringFlip axis) => axis == ImmAuthoringFlip.Z ? ImmAuthoringFlip.X : axis + 1;
    public static void AssertFirstLayerFlip(string path, int axis)
    {
        Require(Reader.StrokeReader_Init(Path.ChangeExtension(path, ".log")) == 0, "Native init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var info) && info.pivotFlip == axis,
                "Quill converter lost a pivot flip.");
            Require(Reader.StrokeReader_GetAuthoringLayerTransform(doc, 0, out var transform, out _) && transform.flip == axis,
                "Quill converter lost a transform flip.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
