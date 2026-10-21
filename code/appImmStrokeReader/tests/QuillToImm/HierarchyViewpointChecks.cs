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
        using var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0, 0, 0, 1)).Value;
        long group = document.CreateGroupLayer(0, ImmAuthoringLayerProperties.Default("Views")).Value;
        var floor = ImmAuthoringLayerProperties.Default("Floor");
        floor.SpawnAreaFloorLevel = true;
        floor.SpawnAreaVolumeOffset = new Vector3(1, 2, 3); floor.SpawnAreaVolumeExtent = new Vector3(4, 4, 4);
        floor.SpawnAreaAllowTranslationX = false; floor.SpawnAreaAllowTranslationY = true; floor.SpawnAreaAllowTranslationZ = false;
        floor.Transform.Position = new Vector3(6, 7, 8); floor.Transform.Flip = ImmAuthoringFlip.Y;
        floor.Pivot.Position = new Vector3(2, 3, 4); floor.Pivot.Flip = ImmAuthoringFlip.X;
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
            // The current native spawn constructor defaults pivot translation;
            // the reader must retain the native value. Compiler preservation is a separate open gap.
            actual.Transform.Translation.X == 6 && actual.Transform.Flip == "Y" && actual.Pivot.Translation.X == 0 && actual.Pivot.Flip == "X" &&
            actual.Animation.Keys.Visibility.Single().Time == 525,
            $"Viewpoint changed: tracking={actual.TypeStr}/{views[1].TypeStr}, sphere={actual.Sphere.X},{actual.Sphere.Y},{actual.Sphere.Z},{actual.Sphere.W}, movement={actual.AllowTranslationX},{actual.AllowTranslationY},{actual.AllowTranslationZ}, local={actual.Transform.Translation.X}/{actual.Transform.Flip}, pivot={actual.Pivot.Translation.X}/{actual.Pivot.Flip}, key={actual.Animation.Keys.Visibility.Single().Time}.");
        Require(result.Issues.Any(x => x.Code == Issue.LayerOmitted && x.SourceLayerName == "Box") &&
            result.Issues.Any(x => x.Code == Issue.ViewpointMetadataNotPreserved && x.SourceLayerName == "Floor" && x.Message.Contains("preview")),
            "Box or preview limitation was silent.");
        string project = Path.Combine(directory, "hierarchy-viewpoint-project");
        SharpQuill.QuillSequenceWriter.Write(result.Sequence, project);
        var copy = SharpQuill.QuillSequenceReader.Read(project);
        var restored = ((SharpQuill.LayerGroup)copy.RootLayer.Children.Single()).Children.OfType<SharpQuill.LayerViewpoint>().ToArray();
        Require(copy.DefaultViewpoint == result.Sequence.DefaultViewpoint && restored.Length == 2 && restored[0].Sphere.W == 4 &&
            restored[1].TypeStr == "EyeLevel" && restored[0].Animation.Keys.Visibility.Single().Time == 525,
            "Viewpoint project read-back changed.");
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
    static void Require(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
}
