using System;
using System.Collections.Generic;
using System.Linq;
using ImmPlayer;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

static class HierarchyAdapterChecks
{
    public static void Run(string path)
    {
        var result = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(path);
        Require(result.Succeeded, string.Join("; ", result.Issues.Select(x => x.Message)));
        int doc = Reader.StrokeReader_LoadFromFile(path);
        Require(doc > 0, "Hierarchy comparison load failed.");
        try
        {
            var records = new List<(int Index, StrokeAuthoringLayerInfo Info)>();
            for (int index = 0; index < Reader.StrokeReader_GetAuthoringLayerCount(doc); index++)
            {
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, index, out var info), "Hierarchy record unavailable.");
                records.Add((index, info));
            }
            Require(Reader.StrokeReader_GetRootLayerProperties(doc, out var root, out var transform), "Hierarchy root unavailable.");
            Require(Reader.StrokeReader_GetDocumentInfo(doc, out var metadata), "Hierarchy document unavailable.");
            CheckCommon(result.Sequence.RootLayer, root, transform);
            CheckKeys(result.Sequence.RootLayer, root, -1, metadata.rootAnimationKeyCount);
            int paints = 0, groups = 0, points = 0;
            CheckGroup(result.Sequence.RootLayer, 0);
            Require(paints > 0 && groups > 0 && points > 0, "Hierarchy fixture must exercise groups and paint geometry.");
            Require(!result.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.FlattenedHierarchy ||
                x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.ConversionFailed), "Hierarchy mode fell back or failed.");
            Console.WriteLine($"[IMM_SHARPQUILL_HIERARCHY] Compared {groups} groups, {paints} paint layers and {points} points, local transforms/pivots, keys and frame maps.");

            void CheckGroup(LayerGroup actual, int parentId)
            {
                var expected = records.Where(x => x.Info.parentId == parentId && (x.Info.type == 0 || x.Info.type == 1 || x.Info.type == 4)).OrderBy(x => x.Info.childIndex).ToArray();
                Require(actual.Children.Count == expected.Length, "Hierarchy child count/order changed.");
                for (int index = 0; index < expected.Length; index++)
                {
                    var item = expected[index]; var child = actual.Children[index];
                    Require(Reader.StrokeReader_GetAuthoringLayerTransform(doc, item.Index, out var local, out _), "Local transform unavailable.");
                    CheckCommon(child, item.Info, local);
                    CheckKeys(child, item.Info, item.Index, Reader.StrokeReader_GetLayerAnimationKeyCount(doc, item.Index));
                    if (item.Info.type == 0) { groups++; Require(child is LayerGroup, "Group was flattened."); CheckGroup((LayerGroup)child, item.Info.id); }
                    if (item.Info.type != 1) continue;
                    paints++; Require(child is LayerPaint, "Paint type changed."); var paint = (LayerPaint)child;
                    Require(Reader.StrokeReader_GetAuthoringLayerAnimationInfo(doc, item.Index, out int rate, out int count, out int repeats), "Paint animation unavailable.");
                    var frames = new int[count];
                    Require((count == 0 || Reader.StrokeReader_GetAuthoringFrameBuffer(doc, item.Index, frames, count) == count) && paint.Framerate == rate &&
                        paint.MaxRepeatCount == repeats && paint.Frames.SequenceEqual(frames) && paint.Drawings.Count == Reader.StrokeReader_GetAuthoringDrawingCount(doc, item.Index), "Paint reuse/rate/repeat changed.");
                    for (int drawing = 0; drawing < paint.Drawings.Count; drawing++)
                    {
                        var strokes = paint.Drawings[drawing].Data.Strokes;
                        Require(strokes.Count == Reader.StrokeReader_GetAuthoringStrokeCount(doc, item.Index, drawing), "Stroke count changed.");
                        for (int stroke = 0; stroke < strokes.Count; stroke++)
                        {
                            Require(Reader.StrokeReader_GetAuthoringStrokeInfo(doc, item.Index, drawing, stroke, out var info), "Stroke info missing.");
                            var source = new StrokeAuthoringPoint[info.numPoints];
                            Require(Reader.StrokeReader_GetAuthoringStrokePoints(doc, item.Index, drawing, stroke, source, source.Length), "Point read failed.");
                            Require(strokes[stroke].Vertices.Count == source.Length && strokes[stroke].DisableRotationalOpacity == (info.visibilityMode == 1), "Stroke size/visibility changed.");
                            for (int point = 0; point < source.Length; point++)
                            {
                                var a = source[point].legacy; var b = strokes[stroke].Vertices[point];
                                Require(b.Position.X == a.px && b.Position.Y == a.py && b.Position.Z == a.pz && b.Normal.X == a.nx && b.Normal.Y == a.ny && b.Normal.Z == a.nz &&
                                    b.Tangent.X == a.dx && b.Tangent.Y == a.dy && b.Tangent.Z == a.dz && b.Color.R == a.r && b.Color.G == a.g && b.Color.B == a.b && b.Opacity == a.alpha && b.Width == a.width,
                                    "Local point attributes changed."); points++;
                            }
                        }
                    }
                }
            }
            void CheckKeys(Layer layer, StrokeAuthoringLayerInfo info, int layerIndex, int count)
            {
                int visibility = 0, opacity = 0, offset = 0, transforms = 0;
                for (int index = 0; index < count; index++)
                {
                    StrokeAnimationKey key;
                    Require(layerIndex < 0 ? Reader.StrokeReader_GetRootAnimationKey(doc, index, out key) : Reader.StrokeReader_GetLayerAnimationKey(doc, layerIndex, index, out key), "Key read failed.");
                    if (key.interpolation > 4 || (key.property != 0 && key.property != 1 && key.property != 8 && key.property != 9))
                    {
                        Require(result.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.AnimationNotPreserved && x.SourceLayerId == info.id), "Unmapped key not reported."); continue;
                    }
                    var interpolation = key.interpolation == 2 ? Interpolation.Smoothstep : key.interpolation == 3 ? Interpolation.EaseIn : key.interpolation == 4 ? Interpolation.EaseOut : (Interpolation)key.interpolation;
                    if (key.property == 0) { var actual = layer.Animation.Keys.Visibility[visibility++]; Require(actual.Time == key.timeTicks && actual.Value == (key.boolValue != 0) && actual.Interpolation == interpolation, "Visibility key changed."); }
                    if (key.property == 1) { var actual = layer.Animation.Keys.Opacity[opacity++]; Require(actual.Time == key.timeTicks && actual.Value == key.floatValue && actual.Interpolation == interpolation, "Opacity key changed."); }
                    if (key.property == 8) { var actual = layer.Animation.Keys.Offset[offset++]; Require(actual.Time == key.timeTicks && actual.Value == key.intValue && actual.Interpolation == interpolation, "Offset key changed."); }
                    if (key.property == 9) { var actual = layer.Animation.Keys.Transform[transforms++]; Require(actual.Time == key.timeTicks && actual.Interpolation == interpolation, "Transform key timing changed."); CheckTransform(actual.Value, key.transformValue); }
                }
                Require(visibility == layer.Animation.Keys.Visibility.Count && opacity == layer.Animation.Keys.Opacity.Count && offset == layer.Animation.Keys.Offset.Count && transforms == layer.Animation.Keys.Transform.Count, "Extra/default keys invented.");
            }
        }
        finally { Reader.StrokeReader_Unload(doc); }
    }
    private static void CheckCommon(Layer actual, StrokeAuthoringLayerInfo expected, StrokeLayerTransform local)
    {
        Require(actual.Name == expected.name && actual.Visible == (expected.visible != 0) && actual.Opacity == expected.opacity &&
            actual.Animation.Timeline == (expected.isTimeline != 0) && actual.Animation.Duration == expected.durationTicks && actual.Animation.MaxRepeatCount == expected.maxRepeatCount, "Hierarchy static/timeline metadata changed.");
        CheckTransform(actual.Transform, local);
        CheckTransform(actual.Pivot, new StrokeLayerTransform { rotX = expected.pivotRotX, rotY = expected.pivotRotY, rotZ = expected.pivotRotZ, rotW = expected.pivotRotW,
            scale = expected.pivotScale, flip = expected.pivotFlip, transX = expected.pivotTransX, transY = expected.pivotTransY, transZ = expected.pivotTransZ });
    }
    private static void CheckTransform(Transform actual, StrokeLayerTransform expected) => Require(actual.Rotation.X == expected.rotX && actual.Rotation.Y == expected.rotY &&
        actual.Rotation.Z == expected.rotZ && actual.Rotation.W == expected.rotW && actual.Scale == expected.scale && actual.Flip == new[] { "N", "X", "Y", "Z" }[expected.flip] &&
        actual.Translation.X == expected.transX && actual.Translation.Y == expected.transY && actual.Translation.Z == expected.transZ, "Local transform/pivot/flip changed.");
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
