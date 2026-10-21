using System;
using System.IO;
using System.Linq;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Adapter = ImmStrokeReader.SharpQuillCompat;
using Issue = ImmStrokeReader.SharpQuillConversionIssueCode;

static class HierarchyViewpointChecks
{
    public static void Run(string directory)
    {
        CheckLegacyConstructor(directory);
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0, 0, 0, 1)).Value;
        long group = document.CreateGroupLayer(0, ImmAuthoringLayerProperties.Default("Views")).Value;
        var floor = ImmAuthoringLayerProperties.Default("Floor");
        floor.SpawnAreaFloorLevel = true;
        floor.SpawnAreaVolumeOffset = new Vector3(1, 2, 3); floor.SpawnAreaVolumeExtent = new Vector3(4, 4, 4);
        floor.SpawnAreaAllowTranslationX = false; floor.SpawnAreaAllowTranslationY = true; floor.SpawnAreaAllowTranslationZ = false;
        floor.Transform.Position = new Vector3(6, 7, 8); floor.Transform.Flip = ImmAuthoringFlip.Y;
        floor.Pivot.Position = new Vector3(2, 3, 4); floor.Pivot.Flip = ImmAuthoringFlip.X;
        floor.Visible = false; floor.Opacity = 0.65f;
        floor.IsTimeline = true; floor.DurationTicks = 25200; floor.MaxRepeatCount = 2;
        floor.SpawnAreaPreview = ImmAuthoringImage.Create(16, 16, 3, ImmAuthoringImageEncoding.Rgb8, new byte[16 * 16 * 3]).Value;
        long floorId = document.CreateSpawnAreaLayer(group, floor).Value;
        Require(document.CreateAnimationKey(floorId, ImmAuthoringAnimationProperty.Visibility, 525, ImmAuthoringAnimationValue.FromBool(false)).Succeeded,
            "Viewpoint key fixture failed.");
        var eye = ImmAuthoringLayerProperties.Default("Eye"); eye.SpawnAreaFloorLevel = false; eye.SpawnAreaIsDefault = true;
        long eyeId = document.CreateSpawnAreaLayer(group, eye).Value;
        var box = ImmAuthoringLayerProperties.Default("Box"); box.SpawnAreaVolume = ExportSpawnAreaVolume.Box;
        long boxId = document.CreateSpawnAreaLayer(group, box).Value;
        string input = Path.Combine(directory, "hierarchy-viewpoints.imm");
        Export();
        var result = Adapter.ReadImmAsHierarchyWithReport(input);
        Require(result.Succeeded && result.Sequence.DefaultViewpoint == "Root/Views/Eye", "Native default viewpoint path lost.");
        var views = ((SharpQuill.LayerGroup)result.Sequence.RootLayer.Children.Single()).Children.OfType<SharpQuill.LayerViewpoint>().ToArray();
        Require(views.Length == 2 && views[0].Name == "Floor" && views[1].Name == "Eye", "Viewpoint order/box omission changed.");
        var actual = views[0];
        Require(actual.TypeStr == "FloorLevel" && views[1].TypeStr == "EyeLevel" && actual.Exporting &&
            actual.Sphere.X == 1 && actual.Sphere.Y == 2 && actual.Sphere.Z == 3 && actual.Sphere.W == 4 &&
            !actual.AllowTranslationX && actual.AllowTranslationY && !actual.AllowTranslationZ &&
            actual.Transform.Translation.X == 6 && actual.Transform.Flip == "Y" && actual.Pivot.Translation.X == 2 && actual.Pivot.Flip == "X" &&
            actual.Visible && actual.Opacity == 0.65f && actual.Animation.Timeline && actual.Animation.Duration == 25200 && actual.Animation.MaxRepeatCount == 2 &&
            actual.Animation.Keys.Visibility.Count == 2 && actual.Animation.Keys.Visibility[0].Time == 0 && !actual.Animation.Keys.Visibility[0].Value && actual.Animation.Keys.Visibility[1].Time == 525,
            $"Viewpoint changed: tracking={actual.TypeStr}/{views[1].TypeStr}, sphere={actual.Sphere.X},{actual.Sphere.Y},{actual.Sphere.Z},{actual.Sphere.W}, movement={actual.AllowTranslationX},{actual.AllowTranslationY},{actual.AllowTranslationZ}, local={actual.Transform.Translation.X}/{actual.Transform.Flip}, pivot={actual.Pivot.Translation.X}/{actual.Pivot.Flip}, keys={actual.Animation.Keys.Visibility.Count}, visible={actual.Visible}, opacity={actual.Opacity:R}, timeline={actual.Animation.Timeline}/{actual.Animation.Duration}/{actual.Animation.MaxRepeatCount}.");
        Require(result.Issues.Any(x => x.Code == Issue.LayerOmitted && x.SourceLayerName == "Box") &&
            result.Issues.Any(x => x.Code == Issue.ViewpointMetadataNotPreserved && x.SourceLayerName == "Floor" && x.Message.Contains("preview")),
            "Box or preview limitation was silent.");
        string project = Path.Combine(directory, "hierarchy-viewpoint-project");
        SharpQuill.QuillSequenceWriter.Write(result.Sequence, project);
        var copy = SharpQuill.QuillSequenceReader.Read(project);
        var restored = ((SharpQuill.LayerGroup)copy.RootLayer.Children.Single()).Children.OfType<SharpQuill.LayerViewpoint>().ToArray();
        Require(copy.DefaultViewpoint == result.Sequence.DefaultViewpoint && restored.Length == 2 && restored[0].Sphere.W == 4 &&
            restored[1].TypeStr == "EyeLevel" && restored[0].Animation.Keys.Visibility.Count == 2 && !restored[0].Animation.Keys.Visibility[0].Value && restored[0].Animation.Keys.Visibility[1].Time == 525 &&
            restored[0].Pivot.Translation.X == 2 && restored[0].Opacity == 0.65f && restored[0].Animation.Duration == 25200,
            "Viewpoint project read-back changed.");
        var imported = ImmAuthoringImporter.ImportFromFile(input);
        Require(imported.Succeeded, imported.Message);
        using (imported.Document)
        {
            var importedFloor = imported.Document.CreateSnapshot().Value.Layers.Single(x => x.Properties.Name == "Floor");
            var properties = importedFloor.Properties;
            Require(importedFloor.AnimationKeys.Any(x => x.Property == ImmAuthoringAnimationProperty.Visibility && x.TimeTicks == 0 && !x.Value.BoolValue) && properties.Opacity == 0.65f && properties.Pivot.Position.x == 2 &&
                properties.Pivot.Position.y == 3 && properties.Pivot.Position.z == 4 && properties.Pivot.Flip == ImmAuthoringFlip.X &&
                properties.IsTimeline && properties.DurationTicks == 25200 && properties.MaxRepeatCount == 2,
                "Spawn common fields did not survive authoring import.");
            string roundtrip = Path.Combine(directory, "spawn-common-roundtrip.imm");
            Require(ImmAuthoringCompiler.ExportToFile(imported.Document, roundtrip).Succeeded, "Spawn common re-export failed.");
            var roundtripResult = Adapter.ReadImmAsHierarchyWithReport(roundtrip);
            Require(roundtripResult.Succeeded, "Spawn common re-export read failed.");
            var roundtripFloor = ((SharpQuill.LayerGroup)roundtripResult.Sequence.RootLayer.Children.Single()).Children.OfType<SharpQuill.LayerViewpoint>().Single(x => x.Name == "Floor");
            Require(roundtripFloor.Pivot.Translation.X == 2 && roundtripFloor.Opacity == 0.65f &&
                roundtripFloor.Animation.Duration == 25200 && roundtripFloor.Animation.MaxRepeatCount == 2 &&
                roundtripFloor.Animation.Keys.Visibility.Count == 2 && !roundtripFloor.Animation.Keys.Visibility[0].Value,
                "Spawn common import/re-export changed properties or visibility track.");
        }
        Require(document.CreateAnimationKey(floorId, ImmAuthoringAnimationProperty.Visibility, 0, ImmAuthoringAnimationValue.FromBool(true)).Succeeded,
            "Explicit time-zero visibility override failed.");
        Export(); result = Adapter.ReadImmAsHierarchyWithReport(input);
        var overridden = ((SharpQuill.LayerGroup)result.Sequence.RootLayer.Children.Single()).Children.OfType<SharpQuill.LayerViewpoint>().Single(x => x.Name == "Floor");
        Require(overridden.Animation.Keys.Visibility.Count == 2 && overridden.Animation.Keys.Visibility[0].Time == 0 &&
            overridden.Animation.Keys.Visibility[0].Value && !overridden.Animation.Keys.Visibility[1].Value,
            "Explicit time-zero key did not replace constructor visibility.");
        var duplicate = ImmAuthoringLayerProperties.Default("Eye");
        Require(document.CreateSpawnAreaLayer(group, duplicate).Succeeded, "Duplicate-name fixture failed.");
        Export(); result = Adapter.ReadImmAsHierarchyWithReport(input);
        Require(result.Succeeded && result.Sequence.DefaultViewpoint == "" && result.Issues.Any(x =>
            x.Code == Issue.ViewpointMetadataNotPreserved && x.Message.Contains("ambiguous")), "Ambiguous default silently selected a viewpoint.");
        eye.SpawnAreaIsDefault = false; box.SpawnAreaIsDefault = true;
        Require(document.SetLayerProperties(eyeId, eye).Succeeded && document.SetLayerProperties(boxId, box).Succeeded, "Box default fixture failed.");
        Export(); result = Adapter.ReadImmAsHierarchyWithReport(input);
        Require(result.Succeeded && result.Sequence.DefaultViewpoint == "" && result.Issues.Any(x =>
            x.Code == Issue.ViewpointMetadataNotPreserved && x.Message.Contains("omitted")), "Omitted box default silently selected another viewpoint.");
        ImmPlayer.ImmStrokeReader.StrokeReader_End();
        Console.WriteLine("[IMM_HIERARCHY_VIEWPOINT] Passed sphere/tracking/movement/local keys, project read-back, preview/box reports and unique/ambiguous/omitted default selection.");

        void Export() { var exported = ImmAuthoringCompiler.ExportToFile(document, input); Require(exported.Succeeded, exported.Message); }
    }
    static void CheckLegacyConstructor(string directory)
    {
        var transform = new TransformNative { Tx = 9, Qw = 1, Scale = 1 };
        var pivot = new TransformNative { Tx = 4, Qw = 1, Scale = 1 };
        Require(Native.ImmExporter_CreateSpawnAreaLayerWithProperties(IntPtr.Zero, IntPtr.Zero, "Invalid", 0, 0.5f,
            ref transform, ref pivot, 1, 525, 2, 0) == IntPtr.Zero, "Spawn constructor accepted a null sequence.");
        IntPtr sequence = Native.ImmExporter_CreateSequence(0, 0, 0, 0, 0, 24, 0, 0, 0, 0);
        Require(sequence != IntPtr.Zero, "Legacy spawn sequence creation failed.");
        string path = Path.Combine(directory, "legacy-spawn-constructor.imm");
        try
        {
            Require(Native.ImmExporter_CreateSpawnAreaLayer(sequence, IntPtr.Zero, "Legacy", ref transform, 1) != IntPtr.Zero &&
                Native.ImmExporter_ExportToFile(sequence, path, 96000, 0), "Legacy spawn construction/export failed.");
        }
        finally { Native.ImmExporter_DestroySequence(sequence); }
        Require(ImmPlayer.ImmStrokeReader.StrokeReader_Init(null) == 0, "Legacy spawn reader init failed.");
        int doc = ImmPlayer.ImmStrokeReader.StrokeReader_LoadFromFile(path);
        try
        {
            Require(doc > 0 && ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var info) &&
                info.visible == 1 && info.opacity == 1 && info.pivotTransX == 0 && info.pivotScale == 1 && info.pivotFlip == 0 &&
                info.isTimeline == 0 && info.durationTicks == 0 && info.maxRepeatCount == 0 &&
                ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerTransform(doc, 0, out var local, out _) && local.transX == 9 &&
                ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerSpawnAreaInfo(doc, 0, out var spawn) && spawn.IsFloorLevel,
                "Original spawn constructor defaults changed.");
        }
        finally { if (doc > 0) ImmPlayer.ImmStrokeReader.StrokeReader_Unload(doc); }
    }
    static void Require(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
}
