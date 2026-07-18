using System;
using System.Collections.Generic;
using ImmPlayer.Exporter;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    public enum ImmAuthoringErrorCode
    {
        None = 0,
        InvalidArgument,
        NotFound,
        InvalidOwner,
        HierarchyCycle,
        DanglingReference,
        RevisionConflict,
        Disposed,
        Unsupported,
        ValidationFailed,
        NativeExportFailed,
        Cancelled
    }

    public readonly struct ImmAuthoringResult
    {
        public bool Succeeded { get; }
        public ImmAuthoringErrorCode ErrorCode { get; }
        public string Message { get; }
        public long ObjectId { get; }

        private ImmAuthoringResult(bool succeeded, ImmAuthoringErrorCode errorCode, string message, long objectId)
        {
            Succeeded = succeeded;
            ErrorCode = errorCode;
            Message = message ?? string.Empty;
            ObjectId = objectId;
        }

        public static ImmAuthoringResult Success() => new ImmAuthoringResult(true, ImmAuthoringErrorCode.None, string.Empty, 0);

        public static ImmAuthoringResult Failure(ImmAuthoringErrorCode code, string message, long objectId = 0)
        {
            if (code == ImmAuthoringErrorCode.None)
                throw new ArgumentException("A failure must have a non-success error code.", nameof(code));
            return new ImmAuthoringResult(false, code, message, objectId);
        }

        public override string ToString() => Succeeded ? "Success" : $"{ErrorCode}: {Message} (object {ObjectId})";
    }

    public readonly struct ImmAuthoringResult<T>
    {
        public bool Succeeded { get; }
        public T Value { get; }
        public ImmAuthoringErrorCode ErrorCode { get; }
        public string Message { get; }
        public long ObjectId { get; }

        private ImmAuthoringResult(bool succeeded, T value, ImmAuthoringErrorCode errorCode, string message, long objectId)
        {
            Succeeded = succeeded;
            Value = value;
            ErrorCode = errorCode;
            Message = message ?? string.Empty;
            ObjectId = objectId;
        }

        public static ImmAuthoringResult<T> Success(T value) =>
            new ImmAuthoringResult<T>(true, value, ImmAuthoringErrorCode.None, string.Empty, 0);

        public static ImmAuthoringResult<T> Failure(ImmAuthoringErrorCode code, string message, long objectId = 0) =>
            new ImmAuthoringResult<T>(false, default, code, message, objectId);

        public ImmAuthoringResult WithoutValue() => Succeeded
            ? ImmAuthoringResult.Success()
            : ImmAuthoringResult.Failure(ErrorCode, Message, ObjectId);
    }

    public enum ImmAuthoringLayerType
    {
        Group = 0,
        Paint = 1
    }

    [Serializable]
    public struct ImmAuthoringTransform
    {
        public Vector3 Position;
        public Quaternion Rotation;
        public float Scale;

        public static ImmAuthoringTransform Identity => new ImmAuthoringTransform
        {
            Position = Vector3.zero,
            Rotation = Quaternion.identity,
            Scale = 1f
        };

        public bool IsFinite()
        {
            return IsFinite(Position.x) && IsFinite(Position.y) && IsFinite(Position.z) &&
                   IsFinite(Rotation.x) && IsFinite(Rotation.y) && IsFinite(Rotation.z) && IsFinite(Rotation.w) &&
                   IsFinite(Scale) && Scale > 0f;
        }

        private static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }

    [Serializable]
    public struct ImmAuthoringLayerProperties
    {
        public string Name;
        public bool Visible;
        public float Opacity;
        public ImmAuthoringTransform Transform;
        public ImmAuthoringTransform Pivot;
        public bool IsTimeline;
        public long DurationTicks;
        public uint MaxRepeatCount;

        public static ImmAuthoringLayerProperties Default(string name) => new ImmAuthoringLayerProperties
        {
            Name = name,
            Visible = true,
            Opacity = 1f,
            Transform = ImmAuthoringTransform.Identity,
            Pivot = ImmAuthoringTransform.Identity,
            IsTimeline = false,
            DurationTicks = 0,
            MaxRepeatCount = 0
        };
    }

    public sealed class ImmAuthoringChange
    {
        public long DocumentId { get; }
        public long Revision { get; }
        public IReadOnlyList<long> AffectedObjectIds { get; }

        internal ImmAuthoringChange(long documentId, long revision, long[] affectedObjectIds)
        {
            DocumentId = documentId;
            Revision = revision;
            AffectedObjectIds = Array.AsReadOnly(affectedObjectIds ?? Array.Empty<long>());
        }
    }

    public sealed class ImmAuthoringStrokeSnapshot
    {
        private readonly PaintPoint[] _points;

        public long Id { get; }
        public long DrawingId { get; }
        public BrushSectionType BrushSection { get; }
        public VisibilityType Visibility { get; }
        public IReadOnlyList<PaintPoint> Points => Array.AsReadOnly(_points);

        internal ImmAuthoringStrokeSnapshot(
            long id,
            long drawingId,
            BrushSectionType brushSection,
            VisibilityType visibility,
            PaintPoint[] points)
        {
            Id = id;
            DrawingId = drawingId;
            BrushSection = brushSection;
            Visibility = visibility;
            _points = points ?? Array.Empty<PaintPoint>();
        }

        internal PaintPoint[] CopyPoints() => (PaintPoint[])_points.Clone();
    }

    public sealed class ImmAuthoringDrawingSnapshot
    {
        private readonly ImmAuthoringStrokeSnapshot[] _strokes;

        public long Id { get; }
        public long PaintLayerId { get; }
        public IReadOnlyList<ImmAuthoringStrokeSnapshot> Strokes => Array.AsReadOnly(_strokes);

        internal ImmAuthoringDrawingSnapshot(long id, long paintLayerId, ImmAuthoringStrokeSnapshot[] strokes)
        {
            Id = id;
            PaintLayerId = paintLayerId;
            _strokes = strokes ?? Array.Empty<ImmAuthoringStrokeSnapshot>();
        }
    }

    public sealed class ImmAuthoringLayerSnapshot
    {
        private readonly long[] _childIds;
        private readonly ImmAuthoringDrawingSnapshot[] _drawings;
        private readonly long[] _frameDrawingIds;

        public long Id { get; }
        public long ParentId { get; }
        public int Order { get; }
        public ImmAuthoringLayerType Type { get; }
        public ImmAuthoringLayerProperties Properties { get; }
        public IReadOnlyList<long> ChildIds => Array.AsReadOnly(_childIds);
        public IReadOnlyList<ImmAuthoringDrawingSnapshot> Drawings => Array.AsReadOnly(_drawings);
        public IReadOnlyList<long> FrameDrawingIds => Array.AsReadOnly(_frameDrawingIds);

        internal ImmAuthoringLayerSnapshot(
            long id,
            long parentId,
            int order,
            ImmAuthoringLayerType type,
            ImmAuthoringLayerProperties properties,
            long[] childIds,
            ImmAuthoringDrawingSnapshot[] drawings,
            long[] frameDrawingIds)
        {
            Id = id;
            ParentId = parentId;
            Order = order;
            Type = type;
            Properties = properties;
            _childIds = childIds ?? Array.Empty<long>();
            _drawings = drawings ?? Array.Empty<ImmAuthoringDrawingSnapshot>();
            _frameDrawingIds = frameDrawingIds ?? Array.Empty<long>();
        }
    }

    public sealed class ImmAuthoringSnapshot
    {
        private readonly ImmAuthoringLayerSnapshot[] _layers;
        private readonly long[] _rootLayerIds;
        private readonly Dictionary<long, ImmAuthoringLayerSnapshot> _layersById;
        private readonly Dictionary<long, ImmAuthoringDrawingSnapshot> _drawingsById;
        private readonly Dictionary<long, ImmAuthoringStrokeSnapshot> _strokesById;

        public long DocumentId { get; }
        public long Revision { get; }
        public ExportSequenceType SequenceType { get; }
        public uint FrameRate { get; }
        public Color BackgroundColor { get; }
        public ExportRequirements Requirements { get; }
        public IReadOnlyList<long> RootLayerIds => Array.AsReadOnly(_rootLayerIds);
        public IReadOnlyList<ImmAuthoringLayerSnapshot> Layers => Array.AsReadOnly(_layers);

        internal ImmAuthoringSnapshot(
            long documentId,
            long revision,
            ExportSequenceType sequenceType,
            uint frameRate,
            Color backgroundColor,
            ExportRequirements requirements,
            long[] rootLayerIds,
            ImmAuthoringLayerSnapshot[] layers)
        {
            DocumentId = documentId;
            Revision = revision;
            SequenceType = sequenceType;
            FrameRate = frameRate;
            BackgroundColor = backgroundColor;
            Requirements = requirements;
            _rootLayerIds = rootLayerIds ?? Array.Empty<long>();
            _layers = layers ?? Array.Empty<ImmAuthoringLayerSnapshot>();
            _layersById = new Dictionary<long, ImmAuthoringLayerSnapshot>(_layers.Length);
            _drawingsById = new Dictionary<long, ImmAuthoringDrawingSnapshot>();
            _strokesById = new Dictionary<long, ImmAuthoringStrokeSnapshot>();
            foreach (ImmAuthoringLayerSnapshot layer in _layers)
            {
                _layersById.Add(layer.Id, layer);
                foreach (ImmAuthoringDrawingSnapshot drawing in layer.Drawings)
                {
                    _drawingsById.Add(drawing.Id, drawing);
                    foreach (ImmAuthoringStrokeSnapshot stroke in drawing.Strokes)
                        _strokesById.Add(stroke.Id, stroke);
                }
            }
        }

        public bool TryGetLayer(long layerId, out ImmAuthoringLayerSnapshot layer) =>
            _layersById.TryGetValue(layerId, out layer);

        public bool TryGetDrawing(long drawingId, out ImmAuthoringDrawingSnapshot drawing) =>
            _drawingsById.TryGetValue(drawingId, out drawing);

        public bool TryGetStroke(long strokeId, out ImmAuthoringStrokeSnapshot stroke) =>
            _strokesById.TryGetValue(strokeId, out stroke);
    }
}
