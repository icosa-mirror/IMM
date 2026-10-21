using System;
using System.IO;
using System.Linq;
using ImmPlayer.Authoring;
using Reader = ImmPlayer.ImmStrokeReader;
using ImmPlayer.Exporter;
using UnityEngine;

static class HierarchyReadChecks
{
    public static void Run(string directory)
    {
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0.1f, 0.2f, 0.3f, 1)).Value;
        var root = ImmAuthoringLayerProperties.Default("Root");
        root.IsTimeline = true; root.DurationTicks = 25200; root.MaxRepeatCount = 2; root.Transform.Position = new Vector3(1, 0, 0);
        Require(document.SetRootProperties(root).Succeeded, "Hierarchy root creation failed.");
        var outer = ImmAuthoringLayerProperties.Default("Outer"); outer.Transform.Position = new Vector3(3, 2, 1); outer.Transform.Flip = ImmAuthoringFlip.X;
        long parent = document.CreateGroupLayer(0, outer).Value;
        var inner = ImmAuthoringLayerProperties.Default("Inner"); inner.Transform.Position = new Vector3(2, 0, 0); inner.Pivot.Flip = ImmAuthoringFlip.Y;
        inner.IsTimeline = true; inner.DurationTicks = 12600; inner.MaxRepeatCount = 3;
        long group = document.CreateGroupLayer(parent, inner).Value;
        Require(document.CreateAnimationKey(group, ImmAuthoringAnimationProperty.Offset, 0, ImmAuthoringAnimationValue.FromUInt(525)).Succeeded, "Hierarchy offset failed.");
        var properties = ImmAuthoringLayerProperties.Default("Paint"); properties.Transform.Flip = ImmAuthoringFlip.Z; properties.Pivot.Position = new Vector3(1, 0, 0); properties.PaintMaxRepeatCount = 3;
        long paint = document.CreatePaintLayer(group, properties).Value;
        long first = document.CreateDrawing(paint).Value, second = document.CreateDrawing(paint).Value;
        var point = new PaintPoint { Position = Vector3.zero, Normal = Vector3.up, Direction = new Vector3(1, 0, 0), Color = new Color(1, 0, 0, 1), Alpha = 1, Width = 1 };
        var end = point; end.Position = new Vector3(1, 0, 0); end.Length = 1;
        Require(document.CreateStroke(first, BrushSectionType.Circle, VisibilityType.Always, new[] { point, end }).Succeeded &&
            document.CreateStroke(second, BrushSectionType.Segment, VisibilityType.FadePow2, new[] { point, end }).Succeeded, "Hierarchy paint failed.");
        foreach (long drawing in new[] { first, first, second, first }) Require(document.AppendFrame(paint, drawing).Succeeded, "Hierarchy frame failed.");
        Require(document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Visibility, 1575, ImmAuthoringAnimationValue.FromBool(false)).Succeeded &&
            document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Opacity, 0, ImmAuthoringAnimationValue.FromFloat(0.25f), ImmAuthoringInterpolation.Linear).Succeeded,
            "Hierarchy visibility/opacity failed.");
        var modes = new[] { ImmAuthoringInterpolation.None, ImmAuthoringInterpolation.Linear, ImmAuthoringInterpolation.Smoothstep, ImmAuthoringInterpolation.EaseIn, ImmAuthoringInterpolation.EaseOut };
        for (int index = 0; index < modes.Length; index++)
        {
            var pose = properties.Transform; pose.Position = new Vector3(index, 0, 0); pose.Flip = (ImmAuthoringFlip)(index % 4);
            Require(document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Transform, index * 525, ImmAuthoringAnimationValue.FromTransform(pose), modes[index]).Succeeded, "Hierarchy transform key failed.");
        }
        var picture = ImmAuthoringLayerProperties.Default("Picture"); picture.PictureViewerLocked = true;
        picture.PictureImage = ImmAuthoringImage.Create(1, 1, 3, ImmAuthoringImageEncoding.Rgb8, new byte[] { 1, 2, 3 }).Value;
        Require(document.CreatePictureLayer(group, picture).Succeeded, "Hierarchy picture failed.");
        string source = Path.Combine(directory, "hierarchy-source.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, source).Succeeded, "Hierarchy source export failed.");
        var result = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(source);
        Require(result.Succeeded, string.Join("; ", result.Issues.Select(x => x.Message)));
        Require(!result.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.FlattenedHierarchy ||
            x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.AnimationNotPreserved), "Supported hierarchy/key data was lost.");
        var a = (SharpQuill.LayerGroup)result.Sequence.RootLayer.Children.Single(); var b = (SharpQuill.LayerGroup)a.Children.Single();
        var importedPaint = (SharpQuill.LayerPaint)b.Children[0];
        Require(a.Name == "Outer" && b.Name == "Inner" && b.Children[1] is SharpQuill.LayerPicture && a.Transform.Translation.X == 3 && b.Transform.Translation.X == 2 &&
            a.Transform.Flip == "X" && b.Pivot.Flip == "Y" && importedPaint.Pivot.Translation.X == 1 && importedPaint.Transform.Flip == "Z" &&
            importedPaint.Frames.SequenceEqual(new[] { 0, 0, 1, 0 }) && importedPaint.Drawings.Count == 2 && importedPaint.Animation.Keys.Transform.Count == 5 &&
            importedPaint.Animation.Keys.Transform[2].Interpolation == SharpQuill.Interpolation.Smoothstep &&
            importedPaint.Animation.Keys.Transform[3].Interpolation == SharpQuill.Interpolation.EaseIn &&
            importedPaint.Animation.Keys.Transform[4].Interpolation == SharpQuill.Interpolation.EaseOut, "Hierarchy/local pose/reflection/hold/interpolation changed.");
        string project = Path.Combine(directory, "hierarchy-project"), output = Path.Combine(directory, "hierarchy-roundtrip.imm");
        SharpQuill.QuillSequenceWriter.Write(result.Sequence, project);
        var converted = QuillToImmConverter.ConvertProject(project, output);
        Require(converted.Succeeded && !converted.IsPartial, string.Join("; ", converted.Issues.Select(x => x.Message)));
        var readback = SharpQuill.QuillSequenceReader.Read(project);
        var readbackPaint = (SharpQuill.LayerPaint)((SharpQuill.LayerGroup)((SharpQuill.LayerGroup)readback.RootLayer.Children.Single()).Children.Single()).Children[0];
        Require(readbackPaint.Frames.SequenceEqual(importedPaint.Frames) && readbackPaint.Animation.Keys.Transform.Count == 5 &&
            readbackPaint.Animation.Keys.Transform.Select(x => x.Interpolation).SequenceEqual(importedPaint.Animation.Keys.Transform.Select(x => x.Interpolation)) &&
            readbackPaint.Drawings[0].Data.Strokes[0].Vertices.Select(x => x.Width).SequenceEqual(importedPaint.Drawings[0].Data.Strokes[0].Vertices.Select(x => x.Width)),
            "Project read-back changed held frames, interpolation or decoded widths.");
        string flatProject = Path.Combine(directory, "hierarchy-legacy-project"), flatOutput = Path.Combine(directory, "hierarchy-legacy-roundtrip.imm");
        SharpQuill.QuillSequenceWriter.Write(global::ImmStrokeReader.SharpQuillCompat.ReadImmAsSequence(source, true), flatProject);
        Require(QuillToImmConverter.ConvertProject(flatProject, flatOutput).Succeeded, "Legacy baseline conversion failed.");
        float originalScale = Scale(source), legacyScale = Scale(flatOutput), hierarchyScale = Scale(output);
        Require(legacyScale == hierarchyScale, "Hierarchy reconstruction introduced additional drawing-scale drift versus the legacy adapter.");
        Console.WriteLine($"[IMM_NATIVE_QUANTIZATION_BASELINE] Original drawing scale {originalScale:R}; legacy and hierarchy re-export {hierarchyScale:R}. Native codec identity remains unaccepted.");
        Require(document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.DrawInTime, 0, ImmAuthoringAnimationValue.FromDouble(1)).Succeeded &&
            document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Opacity, 525, ImmAuthoringAnimationValue.FromFloat(0.75f), ImmAuthoringInterpolation.Spline).Succeeded,
            "Unsupported-channel fixture failed.");
        Require(ImmAuthoringCompiler.ExportToFile(document, source).Succeeded, "Unmapped key export failed.");
        var partial = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(source);
        Require(partial.Succeeded && partial.Issues.Count(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.AnimationNotPreserved) == 2,
            "Unmapped channel/interpolation was not reported.");
        root.DurationTicks = 16777217;
        var rootUpdate = document.SetRootProperties(root);
        Require(rootUpdate.Succeeded, $"Float-boundary root update failed: {rootUpdate.Message}");
        var boundaryExport = ImmAuthoringCompiler.ExportToFile(document, source);
        Require(boundaryExport.Succeeded, $"Float-boundary fixture export failed: {boundaryExport.Message}");
        var rejected = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(source);
        Require(!rejected.Succeeded && rejected.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.ValueNotRepresentable), "Duration rounded silently.");
        root.DurationTicks = 2147483648;
        Require(document.SetRootProperties(root).Succeeded && document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Transform, 2147483648,
            ImmAuthoringAnimationValue.FromTransform(properties.Transform)).Succeeded && ImmAuthoringCompiler.ExportToFile(document, source).Succeeded, "Integer-time fixture failed.");
        rejected = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(source);
        Require(!rejected.Succeeded && rejected.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.ValueNotRepresentable), "Key time truncated silently.");
        Console.WriteLine("[IMM_SHARPQUILL_HIERARCHY_PROJECT] Passed nested groups, holds, pivots/reflections, five interpolations, pictures, project read-back, legacy quantization baseline, unmapped-key reports and precision guards.");
    }
    private static float Scale(string path)
    {
        Require(Reader.StrokeReader_Init(null) == 0, "Scale init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        Require(doc > 0, "Scale file load failed.");
        try
        {
            for (int index = 0; index < Reader.StrokeReader_GetLayerCount(doc); index++)
                if (Reader.StrokeReader_GetLayerInfo(doc, index, out var info) && info.type == 1)
                    return Reader.StrokeReader_GetDrawingBiggestStroke(doc, index, 0);
            throw new InvalidOperationException("Scale fixture has no paint.");
        }
        finally { Reader.StrokeReader_Unload(doc); }
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
