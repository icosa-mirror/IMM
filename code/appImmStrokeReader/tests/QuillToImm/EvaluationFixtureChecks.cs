using System;
using System.IO;
using System.Linq;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;

static class EvaluationFixtureChecks
{
    public static void Run(string directory)
    {
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0.1f, 0.2f, 0.3f, 1)).Value;
        var root = ImmAuthoringLayerProperties.Default("Root");
        root.IsTimeline = true; root.DurationTicks = 12600; root.MaxRepeatCount = 2;
        Require(document.SetRootProperties(root).Succeeded, "Evaluation root fixture failed.");
        var parent = ImmAuthoringLayerProperties.Default("Timeline");
        parent.IsTimeline = true; parent.DurationTicks = 6300; parent.MaxRepeatCount = 2;
        parent.Transform.Position = new Vector3(1, 2, 3); parent.Transform.Flip = ImmAuthoringFlip.Y;
        parent.Pivot.Position = new Vector3(0.5f, 0, 0); parent.Opacity = 0.8f;
        long group = document.CreateGroupLayer(0, parent).Value;
        Require(document.CreateAnimationKey(group, ImmAuthoringAnimationProperty.Offset, 0, ImmAuthoringAnimationValue.FromUInt(525)).Succeeded,
            "Evaluation nested offset fixture failed.");
        var properties = ImmAuthoringLayerProperties.Default("Paint");
        properties.Pivot.Position = new Vector3(1, 0, 0); properties.Pivot.Flip = ImmAuthoringFlip.X;
        properties.PaintMaxRepeatCount = 3;
        long paint = document.CreatePaintLayer(group, properties).Value;
        long first = document.CreateDrawing(paint).Value, second = document.CreateDrawing(paint).Value;
        var point = new PaintPoint { Position = Vector3.zero, Normal = Vector3.up, Direction = new Vector3(1, 0, 0),
            Color = new Color(1, 0, 0, 1), Alpha = 1, Width = 0.1f };
        var end = point; end.Position = new Vector3(1, 0, 0); end.Length = 1;
        Require(document.CreateStroke(first, BrushSectionType.Circle, VisibilityType.Always, new[] { point, end }).Succeeded,
            "Evaluation first drawing failed.");
        end.Position = new Vector3(2, 0, 0); end.Length = 2;
        Require(document.CreateStroke(second, BrushSectionType.Segment, VisibilityType.Always, new[] { point, end }).Succeeded,
            "Evaluation second drawing failed.");
        foreach (long drawing in new[] { first, first, second, first })
            Require(document.AppendFrame(paint, drawing).Succeeded, "Evaluation held-frame fixture failed.");
        foreach (var key in new[] { (0L, true), (1575L, false), (3150L, true) })
            Require(document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Visibility, key.Item1,
                ImmAuthoringAnimationValue.FromBool(key.Item2)).Succeeded, "Evaluation visibility fixture failed.");
        Require(document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Opacity, 0, ImmAuthoringAnimationValue.FromFloat(0.25f), ImmAuthoringInterpolation.Linear).Succeeded &&
            document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Opacity, 3150, ImmAuthoringAnimationValue.FromFloat(0.75f), ImmAuthoringInterpolation.Smoothstep).Succeeded,
            "Evaluation opacity fixture failed.");
        var modes = new[] { ImmAuthoringInterpolation.None, ImmAuthoringInterpolation.Linear, ImmAuthoringInterpolation.Smoothstep,
            ImmAuthoringInterpolation.EaseIn, ImmAuthoringInterpolation.EaseOut, ImmAuthoringInterpolation.None };
        long changedKey = 0;
        for (int index = 0; index < modes.Length; index++)
        {
            var pose = properties.Transform; pose.Position = new Vector3(index, 0, 0); pose.Flip = ImmAuthoringFlip.Z;
            var key = document.CreateAnimationKey(paint, ImmAuthoringAnimationProperty.Transform, index * 525,
                ImmAuthoringAnimationValue.FromTransform(pose), modes[index]);
            Require(key.Succeeded, "Evaluation interpolation fixture failed.");
            if (index == 1) changedKey = key.Value;
        }
        string reference = Path.Combine(directory, "evaluation-reference.imm");
        Require(ImmAuthoringCompiler.ExportToFile(document, reference).Succeeded, "Evaluation reference export failed.");
        var converted = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(reference);
        Require(converted.Succeeded, "Evaluation reference hierarchy read failed.");
        string project = Path.Combine(directory, "evaluation-project");
        SharpQuill.QuillSequenceWriter.Write(converted.Sequence, project);
        var candidate = QuillToImmConverter.ConvertProject(project, Path.Combine(directory, "evaluation-roundtrip.imm"));
        Require(candidate.Succeeded, string.Join("; ", candidate.Issues.Select(x => x.Message)));
        var changed = properties.Transform; changed.Position = new Vector3(20, 0, 0); changed.Flip = ImmAuthoringFlip.Z;
        Require(document.ReplaceAnimationKey(changedKey, ImmAuthoringAnimationProperty.Transform, 525,
            ImmAuthoringAnimationValue.FromTransform(changed), ImmAuthoringInterpolation.Linear).Succeeded &&
            ImmAuthoringCompiler.ExportToFile(document, Path.Combine(directory, "evaluation-mismatch.imm")).Succeeded,
            "Evaluation deliberate mismatch fixture failed.");
        ImmPlayer.ImmStrokeReader.StrokeReader_End();
        Console.WriteLine("[IMM_EVALUATION_FIXTURES] Wrote native/reference, Quill round-trip and deliberate mismatch for independent evaluated-state checks; this generator does not evaluate playback.");
    }
    static void Require(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
}
