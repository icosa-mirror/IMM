using System;
using System.Collections.Generic;
using ImmPlayer.Exporter;

namespace ImmPlayer.Authoring
{
    internal sealed class ImmAuthoringState
    {
        internal long NextObjectId = 1;
        internal readonly List<long> RootLayerIds = new List<long>();
        internal readonly Dictionary<long, LayerNode> Layers = new Dictionary<long, LayerNode>();
        internal readonly Dictionary<long, DrawingNode> Drawings = new Dictionary<long, DrawingNode>();
        internal readonly Dictionary<long, StrokeNode> Strokes = new Dictionary<long, StrokeNode>();

        internal long AllocateId() => NextObjectId++;

        internal ImmAuthoringState Clone()
        {
            ImmAuthoringState clone = new ImmAuthoringState { NextObjectId = NextObjectId };
            clone.RootLayerIds.AddRange(RootLayerIds);

            foreach (KeyValuePair<long, LayerNode> item in Layers)
            {
                LayerNode layer = item.Value.Clone();
                clone.Layers.Add(item.Key, layer);
            }

            foreach (KeyValuePair<long, DrawingNode> item in Drawings)
            {
                DrawingNode drawing = item.Value.CloneWithoutStrokes();
                clone.Drawings.Add(item.Key, drawing);
            }

            foreach (KeyValuePair<long, StrokeNode> item in Strokes)
            {
                StrokeNode stroke = item.Value.Clone();
                clone.Strokes.Add(item.Key, stroke);
            }

            return clone;
        }

        internal ImmAuthoringSnapshot CreateSnapshot(
            long documentId,
            long revision,
            ExportSequenceType sequenceType,
            uint frameRate,
            UnityEngine.Color backgroundColor,
            ExportRequirements requirements)
        {
            List<ImmAuthoringLayerSnapshot> layers = new List<ImmAuthoringLayerSnapshot>(Layers.Count);
            for (int order = 0; order < RootLayerIds.Count; order++)
                AppendLayerSnapshot(RootLayerIds[order], order, layers);

            return new ImmAuthoringSnapshot(
                documentId,
                revision,
                sequenceType,
                frameRate,
                backgroundColor,
                requirements,
                RootLayerIds.ToArray(),
                layers.ToArray());
        }

        private void AppendLayerSnapshot(long layerId, int order, List<ImmAuthoringLayerSnapshot> output)
        {
            LayerNode layer = Layers[layerId];
            ImmAuthoringDrawingSnapshot[] drawings = new ImmAuthoringDrawingSnapshot[layer.DrawingIds.Count];
            for (int drawingIndex = 0; drawingIndex < layer.DrawingIds.Count; drawingIndex++)
            {
                DrawingNode drawing = Drawings[layer.DrawingIds[drawingIndex]];
                ImmAuthoringStrokeSnapshot[] strokes = new ImmAuthoringStrokeSnapshot[drawing.StrokeIds.Count];
                for (int strokeIndex = 0; strokeIndex < drawing.StrokeIds.Count; strokeIndex++)
                {
                    StrokeNode stroke = Strokes[drawing.StrokeIds[strokeIndex]];
                    strokes[strokeIndex] = new ImmAuthoringStrokeSnapshot(
                        stroke.Id,
                        stroke.BrushSection,
                        stroke.Visibility,
                        (PaintPoint[])stroke.Points.Clone());
                }
                drawings[drawingIndex] = new ImmAuthoringDrawingSnapshot(drawing.Id, strokes);
            }

            output.Add(new ImmAuthoringLayerSnapshot(
                layer.Id,
                layer.ParentId,
                order,
                layer.Type,
                layer.Properties,
                layer.ChildIds.ToArray(),
                drawings,
                layer.FrameDrawingIds.ToArray()));

            for (int childOrder = 0; childOrder < layer.ChildIds.Count; childOrder++)
                AppendLayerSnapshot(layer.ChildIds[childOrder], childOrder, output);
        }
    }

    internal sealed class LayerNode
    {
        internal long Id;
        internal long ParentId;
        internal ImmAuthoringLayerType Type;
        internal ImmAuthoringLayerProperties Properties;
        internal readonly List<long> ChildIds = new List<long>();
        internal readonly List<long> DrawingIds = new List<long>();
        internal readonly List<long> FrameDrawingIds = new List<long>();

        internal LayerNode Clone()
        {
            LayerNode clone = new LayerNode
            {
                Id = Id,
                ParentId = ParentId,
                Type = Type,
                Properties = Properties
            };
            clone.ChildIds.AddRange(ChildIds);
            clone.DrawingIds.AddRange(DrawingIds);
            clone.FrameDrawingIds.AddRange(FrameDrawingIds);
            return clone;
        }
    }

    internal sealed class DrawingNode
    {
        internal long Id;
        internal long PaintLayerId;
        internal readonly List<long> StrokeIds = new List<long>();

        internal DrawingNode CloneWithoutStrokes()
        {
            DrawingNode clone = new DrawingNode { Id = Id, PaintLayerId = PaintLayerId };
            clone.StrokeIds.AddRange(StrokeIds);
            return clone;
        }
    }

    internal sealed class StrokeNode
    {
        internal long Id;
        internal long DrawingId;
        internal BrushSectionType BrushSection;
        internal VisibilityType Visibility;
        internal PaintPoint[] Points = Array.Empty<PaintPoint>();

        internal StrokeNode Clone() => new StrokeNode
        {
            Id = Id,
            DrawingId = DrawingId,
            BrushSection = BrushSection,
            Visibility = Visibility,
            Points = (PaintPoint[])Points.Clone()
        };
    }
}
