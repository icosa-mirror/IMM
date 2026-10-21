using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Native = ImmPlayer.Exporter.Native;
using Reader = ImmPlayer.ImmStrokeReader;

static class RootAnimationChecks
{
    public static void Run(string directory)
    {
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Comic, 24, new Color(0, 0, 0, 1)).Value;
        var root = ImmAuthoringLayerProperties.Default("Root");
        root.IsTimeline = true;
        root.Visible = false; // Explicit visibility keys take precedence over this unkeyed fallback.
        root.DurationTicks = 45360000;
        Require(document.SetRootProperties(root).Succeeded, "Root setup failed.");
        Require(Native.ImmExporter_GetRootLayer(IntPtr.Zero) == IntPtr.Zero, "Null sequence exposed a root handle.");
        long play = Add(ImmAuthoringAnimationProperty.Action, 524, ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.Play));
        Add(ImmAuthoringAnimationProperty.Action, 525, ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.Stop));
        Add(ImmAuthoringAnimationProperty.Action, 1575, ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.Play));
        Add(ImmAuthoringAnimationProperty.Action, 2100, ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.MakeDefault));
        Add(ImmAuthoringAnimationProperty.Action, 3150, ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.Loop));
        Add(ImmAuthoringAnimationProperty.Visibility, 100, ImmAuthoringAnimationValue.FromBool(true));
        Add(ImmAuthoringAnimationProperty.Visibility, 5000, ImmAuthoringAnimationValue.FromBool(false));
        Add(ImmAuthoringAnimationProperty.Opacity, 0, ImmAuthoringAnimationValue.FromFloat(0.25f));
        Add(ImmAuthoringAnimationProperty.DrawInTime, 0, ImmAuthoringAnimationValue.FromDouble(0.5));
        Add(ImmAuthoringAnimationProperty.Offset, 0, ImmAuthoringAnimationValue.FromUInt(7));
        Add(ImmAuthoringAnimationProperty.Loop, 0, ImmAuthoringAnimationValue.FromBool(true));
        var transform = ImmAuthoringTransform.Identity; transform.Position = new Vector3(1, 2, 3); transform.Flip = ImmAuthoringFlip.Z;
        Add(ImmAuthoringAnimationProperty.Transform, 0, ImmAuthoringAnimationValue.FromTransform(transform), ImmAuthoringInterpolation.Smoothstep);
        Require(!document.CreateAnimationKey(0, ImmAuthoringAnimationProperty.Action, 524, ImmAuthoringAnimationValue.FromUInt(0)).Succeeded,
            "Duplicate root property/tick accepted.");
        var snapshot = document.CreateSnapshot().Value;
        Require(snapshot.Layers.Count == 0 && snapshot.RootLayerIds.Count == 0 && snapshot.RootAnimationKeys.Count == 12 &&
            snapshot.TryGetAnimationKey(play, out var found) && found.LayerId == 0 && !snapshot.TryGetLayer(0, out _),
            "Root keys changed child-layer identity or snapshot lookup.");
        using (var transaction = document.BeginEdit(document.Revision).Value)
        {
            Require(transaction.EditableDocument.ReplaceAnimationKey(play, ImmAuthoringAnimationProperty.Action, 523,
                ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.Play), ImmAuthoringInterpolation.None).Succeeded,
                "Root key replacement failed.");
            Require(document.CreateSnapshot().Value.TryGetAnimationKey(play, out var original) && original.TimeTicks == 524,
                "Root key transaction changed its source early.");
            Require(transaction.Commit().Succeeded && document.CreateSnapshot().Value.TryGetAnimationKey(play, out var edited) && edited.TimeTicks == 523,
                "Root key transaction commit failed.");
        }
        Require(snapshot.TryGetAnimationKey(play, out var immutable) && immutable.TimeTicks == 524, "Root snapshot changed after edit.");
        Require(document.ReplaceAnimationKey(play, ImmAuthoringAnimationProperty.Action, 524,
            ImmAuthoringAnimationValue.FromUInt((uint)ImmAuthoringAction.Play), ImmAuthoringInterpolation.None).Succeeded, "Root key reset failed.");
        long removable = Add(ImmAuthoringAnimationProperty.Offset, 6000, ImmAuthoringAnimationValue.FromUInt(10));
        Require(document.RemoveAnimationKey(removable).Succeeded && document.Validate().Succeeded, "Root key removal left invalid ownership.");
        string path = Path.Combine(directory, "root-animation.imm");
        byte[] marker = { 1, 2, 3 };
        File.WriteAllBytes(path, marker);
        var limited = ImmAuthoringCompiler.ExportToFile(document, path,
            new ImmAuthoringOperationOptions(limits: new ImmAuthoringLimits(maxAnimationKeys: 11)));
        Require(!limited.Succeeded && limited.ErrorCode == ImmAuthoringErrorCode.ResourceLimitExceeded &&
            File.ReadAllBytes(path).SequenceEqual(marker), "Root keys bypassed limits or changed failed output.");
        var exported = ImmAuthoringCompiler.ExportToFile(document, path);
        Require(exported.Succeeded, exported.Message);
        Require(Reader.StrokeReader_Init(Path.Combine(directory, "root-animation.log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        int chapters = 0;
        var actual = new List<StrokeAnimationKey>();
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetDocumentInfo(doc, out var info) && info.sequenceType == 2 && info.rootAnimationKeyCount == 12,
                "Comic category or root key count changed.");
            for (int i = 0; i < 12; i++)
            {
                Require(Reader.StrokeReader_GetRootAnimationKey(doc, i, out var key), "Root key query failed.");
                actual.Add(key);
            }
            Require(actual.Any(x => x.property == 6 && x.timeTicks == 524 && x.intValue == 1) &&
                actual.Any(x => x.property == 6 && x.timeTicks == 525 && x.intValue == 0), "One-tick action separation changed.");
            Require(actual.Where(x => x.property == 0).Select(x => x.timeTicks).SequenceEqual(new long[] { 100, 5000 }),
                "Static root defaults inserted an earlier visibility key.");
            Require(actual.Any(x => x.property == 9 && x.transformValue.flip == 3 && x.interpolation == 2 && x.transformValue.transY == 2),
                "Root transform reflection/interpolation changed.");
            chapters = Reader.StrokeReader_GetChapterCount(doc);
            Require(chapters == 3, $"Expected three chapters from two Play markers, got {chapters}.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        var imported = ImmAuthoringImporter.ImportFromFile(path, Path.Combine(directory, "root-animation-import.log"));
        Require(imported.Succeeded, imported.Message);
        using (var roundTripDocument = imported.Document)
        {
            Require(!imported.Issues.Any(x => x.Code == ImmAuthoringImportIssueCode.UnsupportedRootAnimation ||
                x.Code == ImmAuthoringImportIssueCode.UnsupportedSequenceType), "Supported root actions/comic category were reported unsupported.");
            var comparison = ImmAuthoringStructuralComparer.Compare(document.CreateSnapshot().Value, roundTripDocument.CreateSnapshot().Value);
            Require(comparison.Equivalent, $"Root animation import changed: {string.Join("; ", comparison.Differences.Take(4).Select(x => $"{x.Path}: {x.Message}"))}");
            string roundTripPath = Path.Combine(directory, "root-animation-roundtrip.imm");
            var again = ImmAuthoringCompiler.ExportToFile(roundTripDocument, roundTripPath);
            Require(again.Succeeded, again.Message);
            Require(Reader.StrokeReader_Init(Path.Combine(directory, "root-animation-roundtrip.log")) == 0, "Round-trip reader init failed.");
            int restored = Reader.StrokeReader_LoadFromFile(roundTripPath);
            try
            {
                Require(restored > 0 && Reader.StrokeReader_GetDocumentInfo(restored, out var restoredInfo) &&
                    restoredInfo.sequenceType == 2 && restoredInfo.rootAnimationKeyCount == actual.Count &&
                    Reader.StrokeReader_GetChapterCount(restored) == chapters, "Re-export changed comic category, keys or chapters.");
                for (int index = 0; index < actual.Count; index++)
                    Require(Reader.StrokeReader_GetRootAnimationKey(restored, index, out var key) && key.Equals(actual[index]),
                        $"Re-export changed stored root key {index}.");
            }
            finally { if (restored > 0) Reader.StrokeReader_Unload(restored); Reader.StrokeReader_End(); }
        }
        Console.WriteLine($"[IMM_ROOT_ANIMATION_TEST] Passed root ownership/transaction/keys/comic/tick/chapter export-import checks; chapters={chapters}. Playback controls remain separate.");

        long Add(ImmAuthoringAnimationProperty property, long time, ImmAuthoringAnimationValue value,
            ImmAuthoringInterpolation interpolation = ImmAuthoringInterpolation.None)
        {
            var result = document.CreateAnimationKey(0, property, time, value, interpolation);
            Require(result.Succeeded, result.Message);
            return result.Value;
        }
    }

    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
