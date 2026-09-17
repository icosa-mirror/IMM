using System;
using System.Collections.Generic;
using ImmPlayer.Exporter;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    public sealed class ImmAuthoringStructuralDifference
    {
        public string Path { get; }
        public string Message { get; }

        internal ImmAuthoringStructuralDifference(string path, string message)
        {
            Path = path ?? string.Empty;
            Message = message ?? string.Empty;
        }

        public override string ToString() => $"{Path}: {Message}";
    }

    public sealed class ImmAuthoringStructuralComparison
    {
        private readonly ImmAuthoringStructuralDifference[] _differences;

        public bool Equivalent => _differences.Length == 0;
        public IReadOnlyList<ImmAuthoringStructuralDifference> Differences => Array.AsReadOnly(_differences);

        internal ImmAuthoringStructuralComparison(List<ImmAuthoringStructuralDifference> differences)
        {
            _differences = differences?.ToArray() ?? Array.Empty<ImmAuthoringStructuralDifference>();
        }
    }

    public static class ImmAuthoringStructuralComparer
    {
        public static ImmAuthoringStructuralComparison Compare(
            ImmAuthoringSnapshot expected,
            ImmAuthoringSnapshot actual,
            float tolerance = 0.005f)
        {
            if (tolerance < 0f || float.IsNaN(tolerance))
                throw new ArgumentOutOfRangeException(nameof(tolerance));
            List<ImmAuthoringStructuralDifference> differences = new List<ImmAuthoringStructuralDifference>();
            if (expected == null || actual == null)
            {
                Difference(differences, "document", "Both snapshots must be non-null.");
                return new ImmAuthoringStructuralComparison(differences);
            }

            Equal(differences, "document.sequenceType", expected.SequenceType, actual.SequenceType);
            Equal(differences, "document.frameRate", expected.FrameRate, actual.FrameRate);
            Near(differences, "document.backgroundColor", expected.BackgroundColor, actual.BackgroundColor, tolerance);
            if (expected.Layers.Count != actual.Layers.Count)
            {
                Difference(differences, "document.layers", $"Expected {expected.Layers.Count}, actual {actual.Layers.Count}.");
                return new ImmAuthoringStructuralComparison(differences);
            }

            Dictionary<long, int> expectedLayerIndices = IndexLayers(expected);
            Dictionary<long, int> actualLayerIndices = IndexLayers(actual);
            for (int layerIndex = 0; layerIndex < expected.Layers.Count; layerIndex++)
            {
                ImmAuthoringLayerSnapshot expectedLayer = expected.Layers[layerIndex];
                ImmAuthoringLayerSnapshot actualLayer = actual.Layers[layerIndex];
                string path = $"layers[{layerIndex}]";
                Equal(differences, $"{path}.type", expectedLayer.Type, actualLayer.Type);
                Equal(differences, $"{path}.parent", ParentIndex(expectedLayer, expectedLayerIndices), ParentIndex(actualLayer, actualLayerIndices));
                Equal(differences, $"{path}.order", expectedLayer.Order, actualLayer.Order);
                CompareLayerProperties(differences, path, expectedLayer.Properties, actualLayer.Properties, tolerance);
                CompareAnimationKeys(differences, path, expectedLayer.AnimationKeys, actualLayer.AnimationKeys, tolerance);
                CompareDrawings(differences, path, expectedLayer, actualLayer, tolerance);
            }
            return new ImmAuthoringStructuralComparison(differences);
        }

        private static void CompareLayerProperties(
            List<ImmAuthoringStructuralDifference> differences,
            string path,
            ImmAuthoringLayerProperties expected,
            ImmAuthoringLayerProperties actual,
            float tolerance)
        {
            Equal(differences, $"{path}.name", expected.Name, actual.Name);
            Equal(differences, $"{path}.visible", expected.Visible, actual.Visible);
            Near(differences, $"{path}.opacity", expected.Opacity, actual.Opacity, tolerance);
            CompareTransform(differences, $"{path}.transform", expected.Transform, actual.Transform, tolerance);
            CompareTransform(differences, $"{path}.pivot", expected.Pivot, actual.Pivot, tolerance);
            Equal(differences, $"{path}.isTimeline", expected.IsTimeline, actual.IsTimeline);
            Equal(differences, $"{path}.durationTicks", expected.DurationTicks, actual.DurationTicks);
            Equal(differences, $"{path}.maxRepeatCount", expected.MaxRepeatCount, actual.MaxRepeatCount);
            Equal(differences, $"{path}.paintMaxRepeatCount", expected.PaintMaxRepeatCount, actual.PaintMaxRepeatCount);
        }

        private static void CompareAnimationKeys(
            List<ImmAuthoringStructuralDifference> differences,
            string path,
            IReadOnlyList<ImmAuthoringAnimationKeySnapshot> expected,
            IReadOnlyList<ImmAuthoringAnimationKeySnapshot> actual,
            float tolerance)
        {
            if (expected.Count != actual.Count)
            {
                Difference(differences, $"{path}.animationKeys", $"Expected {expected.Count}, actual {actual.Count}.");
                return;
            }
            for (int keyIndex = 0; keyIndex < expected.Count; keyIndex++)
            {
                ImmAuthoringAnimationKeySnapshot expectedKey = expected[keyIndex];
                ImmAuthoringAnimationKeySnapshot actualKey = actual[keyIndex];
                string keyPath = $"{path}.animationKeys[{keyIndex}]";
                Equal(differences, $"{keyPath}.property", expectedKey.Property, actualKey.Property);
                Equal(differences, $"{keyPath}.timeTicks", expectedKey.TimeTicks, actualKey.TimeTicks);
                Equal(differences, $"{keyPath}.interpolation", expectedKey.Interpolation, actualKey.Interpolation);
                switch (expectedKey.Property)
                {
                    case ImmAuthoringAnimationProperty.Visibility:
                    case ImmAuthoringAnimationProperty.Loop:
                        Equal(differences, $"{keyPath}.value", expectedKey.Value.BoolValue, actualKey.Value.BoolValue);
                        break;
                    case ImmAuthoringAnimationProperty.Opacity:
                        Near(differences, $"{keyPath}.value", expectedKey.Value.FloatValue, actualKey.Value.FloatValue, tolerance);
                        break;
                    case ImmAuthoringAnimationProperty.DrawInTime:
                        Near(differences, $"{keyPath}.value", expectedKey.Value.DoubleValue, actualKey.Value.DoubleValue, tolerance);
                        break;
                    case ImmAuthoringAnimationProperty.Action:
                    case ImmAuthoringAnimationProperty.Offset:
                        Equal(differences, $"{keyPath}.value", expectedKey.Value.UIntValue, actualKey.Value.UIntValue);
                        break;
                    default:
                        CompareTransform(differences, $"{keyPath}.value", expectedKey.Value.TransformValue, actualKey.Value.TransformValue, tolerance);
                        break;
                }
            }
        }

        private static void CompareDrawings(
            List<ImmAuthoringStructuralDifference> differences,
            string path,
            ImmAuthoringLayerSnapshot expected,
            ImmAuthoringLayerSnapshot actual,
            float tolerance)
        {
            if (expected.Drawings.Count != actual.Drawings.Count)
            {
                Difference(differences, $"{path}.drawings", $"Expected {expected.Drawings.Count}, actual {actual.Drawings.Count}.");
                return;
            }
            Dictionary<long, int> expectedDrawingIndices = IndexDrawings(expected.Drawings);
            Dictionary<long, int> actualDrawingIndices = IndexDrawings(actual.Drawings);
            if (expected.FrameDrawingIds.Count != actual.FrameDrawingIds.Count)
            {
                Difference(differences, $"{path}.frames", $"Expected {expected.FrameDrawingIds.Count}, actual {actual.FrameDrawingIds.Count}.");
            }
            else
            {
                for (int frameIndex = 0; frameIndex < expected.FrameDrawingIds.Count; frameIndex++)
                {
                    Equal(
                        differences,
                        $"{path}.frames[{frameIndex}]",
                        expectedDrawingIndices[expected.FrameDrawingIds[frameIndex]],
                        actualDrawingIndices[actual.FrameDrawingIds[frameIndex]]);
                }
            }

            for (int drawingIndex = 0; drawingIndex < expected.Drawings.Count; drawingIndex++)
            {
                ImmAuthoringDrawingSnapshot expectedDrawing = expected.Drawings[drawingIndex];
                ImmAuthoringDrawingSnapshot actualDrawing = actual.Drawings[drawingIndex];
                string drawingPath = $"{path}.drawings[{drawingIndex}]";
                if (expectedDrawing.Strokes.Count != actualDrawing.Strokes.Count)
                {
                    Difference(differences, $"{drawingPath}.strokes", $"Expected {expectedDrawing.Strokes.Count}, actual {actualDrawing.Strokes.Count}.");
                    continue;
                }
                for (int strokeIndex = 0; strokeIndex < expectedDrawing.Strokes.Count; strokeIndex++)
                    CompareStroke(differences, $"{drawingPath}.strokes[{strokeIndex}]", expectedDrawing.Strokes[strokeIndex], actualDrawing.Strokes[strokeIndex], tolerance);
            }
        }

        private static void CompareStroke(
            List<ImmAuthoringStructuralDifference> differences,
            string path,
            ImmAuthoringStrokeSnapshot expected,
            ImmAuthoringStrokeSnapshot actual,
            float tolerance)
        {
            Equal(differences, $"{path}.brush", expected.BrushSection, actual.BrushSection);
            Equal(differences, $"{path}.visibility", expected.Visibility, actual.Visibility);
            if (expected.Points.Count != actual.Points.Count)
            {
                Difference(differences, $"{path}.points", $"Expected {expected.Points.Count}, actual {actual.Points.Count}.");
                return;
            }
            for (int pointIndex = 0; pointIndex < expected.Points.Count; pointIndex++)
            {
                PaintPoint expectedPoint = expected.Points[pointIndex];
                PaintPoint actualPoint = actual.Points[pointIndex];
                string pointPath = $"{path}.points[{pointIndex}]";
                Near(differences, $"{pointPath}.position", expectedPoint.Position, actualPoint.Position, tolerance);
                Near(differences, $"{pointPath}.normal", expectedPoint.Normal, actualPoint.Normal, tolerance);
                if (expected.Visibility != VisibilityType.Always && actual.Visibility != VisibilityType.Always)
                    Near(differences, $"{pointPath}.direction", expectedPoint.Direction, actualPoint.Direction, tolerance);
                Near(differences, $"{pointPath}.color", expectedPoint.Color, actualPoint.Color, tolerance);
                Near(differences, $"{pointPath}.alpha", expectedPoint.Alpha, actualPoint.Alpha, tolerance);
                Near(differences, $"{pointPath}.width", expectedPoint.Width, actualPoint.Width, tolerance);
                Near(differences, $"{pointPath}.length", expectedPoint.Length, actualPoint.Length, tolerance);
                Near(differences, $"{pointPath}.time", expectedPoint.Time, actualPoint.Time, tolerance);
            }
        }

        private static void CompareTransform(
            List<ImmAuthoringStructuralDifference> differences,
            string path,
            ImmAuthoringTransform expected,
            ImmAuthoringTransform actual,
            float tolerance)
        {
            Near(differences, $"{path}.position", expected.Position, actual.Position, tolerance);
            float rotationDot = Mathf.Abs(Quaternion.Dot(expected.Rotation, actual.Rotation));
            if (1f - rotationDot > tolerance)
                Difference(differences, $"{path}.rotation", $"Quaternion dot was {rotationDot}.");
            Near(differences, $"{path}.scale", expected.Scale, actual.Scale, tolerance);
        }

        private static Dictionary<long, int> IndexLayers(ImmAuthoringSnapshot snapshot)
        {
            Dictionary<long, int> result = new Dictionary<long, int>();
            for (int index = 0; index < snapshot.Layers.Count; index++)
                result[snapshot.Layers[index].Id] = index;
            return result;
        }

        private static Dictionary<long, int> IndexDrawings(IReadOnlyList<ImmAuthoringDrawingSnapshot> drawings)
        {
            Dictionary<long, int> result = new Dictionary<long, int>();
            for (int index = 0; index < drawings.Count; index++)
                result[drawings[index].Id] = index;
            return result;
        }

        private static int ParentIndex(ImmAuthoringLayerSnapshot layer, Dictionary<long, int> indices) =>
            layer.ParentId == 0 ? -1 : indices[layer.ParentId];

        private static void Equal<T>(List<ImmAuthoringStructuralDifference> differences, string path, T expected, T actual)
        {
            if (!EqualityComparer<T>.Default.Equals(expected, actual))
                Difference(differences, path, $"Expected {expected}, actual {actual}.");
        }

        private static void Near(List<ImmAuthoringStructuralDifference> differences, string path, float expected, float actual, float tolerance)
        {
            if (Mathf.Abs(expected - actual) > tolerance)
                Difference(differences, path, $"Expected {expected}, actual {actual}.");
        }

        private static void Near(List<ImmAuthoringStructuralDifference> differences, string path, double expected, double actual, float tolerance)
        {
            if (Math.Abs(expected - actual) > tolerance)
                Difference(differences, path, $"Expected {expected}, actual {actual}.");
        }

        private static void Near(List<ImmAuthoringStructuralDifference> differences, string path, Vector3 expected, Vector3 actual, float tolerance)
        {
            if ((expected - actual).magnitude > tolerance)
                Difference(differences, path, $"Expected {expected}, actual {actual}.");
        }

        private static void Near(List<ImmAuthoringStructuralDifference> differences, string path, Color expected, Color actual, float tolerance)
        {
            if (Mathf.Abs(expected.r - actual.r) > tolerance || Mathf.Abs(expected.g - actual.g) > tolerance ||
                Mathf.Abs(expected.b - actual.b) > tolerance || Mathf.Abs(expected.a - actual.a) > tolerance)
            {
                Difference(differences, path, $"Expected {expected}, actual {actual}.");
            }
        }

        private static void Difference(List<ImmAuthoringStructuralDifference> differences, string path, string message) =>
            differences.Add(new ImmAuthoringStructuralDifference(path, message));
    }
}
