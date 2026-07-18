using System;
using System.Collections.Generic;
using System.Threading;
using ImmPlayer.Exporter;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    public sealed class ImmAuthoringDocument : IDisposable
    {
        private static long sNextDocumentId;

        private readonly object _gate = new object();
        private ImmAuthoringState _state;
        private bool _disposed;
        private long _revision;
        private ExportSequenceType _sequenceType;
        private uint _frameRate;
        private Color _backgroundColor;
        private ExportRequirements _requirements;

        public long DocumentId { get; }
        public event Action<ImmAuthoringChange> Changed;

        private ImmAuthoringDocument(
            ExportSequenceType sequenceType,
            uint frameRate,
            Color backgroundColor,
            ExportRequirements requirements,
            ImmAuthoringState state = null,
            long revision = 0)
        {
            DocumentId = Interlocked.Increment(ref sNextDocumentId);
            _sequenceType = sequenceType;
            _frameRate = frameRate;
            _backgroundColor = backgroundColor;
            _requirements = requirements;
            _state = state ?? new ImmAuthoringState();
            _revision = revision;
        }

        public static ImmAuthoringResult<ImmAuthoringDocument> Create(
            ExportSequenceType sequenceType,
            uint frameRate,
            Color backgroundColor,
            ExportRequirements requirements = default)
        {
            ImmAuthoringResult validation = ValidateDocumentSettings(sequenceType, frameRate, backgroundColor, requirements);
            return validation.Succeeded
                ? ImmAuthoringResult<ImmAuthoringDocument>.Success(new ImmAuthoringDocument(sequenceType, frameRate, backgroundColor, requirements))
                : ImmAuthoringResult<ImmAuthoringDocument>.Failure(validation.ErrorCode, validation.Message, validation.ObjectId);
        }

        public long Revision
        {
            get { lock (_gate) return _revision; }
        }

        public ImmAuthoringResult<ImmAuthoringTransaction> BeginEdit(long expectedRevision)
        {
            lock (_gate)
            {
                if (_disposed)
                    return ImmAuthoringResult<ImmAuthoringTransaction>.Failure(
                        ImmAuthoringErrorCode.Disposed,
                        "Document is disposed.",
                        DocumentId);
                if (expectedRevision != _revision)
                    return ImmAuthoringResult<ImmAuthoringTransaction>.Failure(
                        ImmAuthoringErrorCode.RevisionConflict,
                        $"Expected revision {expectedRevision}, but the document is at revision {_revision}.",
                        DocumentId);

                ImmAuthoringDocument editable = new ImmAuthoringDocument(
                    _sequenceType,
                    _frameRate,
                    _backgroundColor,
                    _requirements,
                    _state.Clone(),
                    _revision);
                return ImmAuthoringResult<ImmAuthoringTransaction>.Success(
                    new ImmAuthoringTransaction(this, editable, _revision));
            }
        }

        public ImmAuthoringResult SetDocumentSettings(
            ExportSequenceType sequenceType,
            uint frameRate,
            Color backgroundColor,
            ExportRequirements requirements)
        {
            ImmAuthoringResult validation = ValidateDocumentSettings(sequenceType, frameRate, backgroundColor, requirements);
            if (!validation.Succeeded)
                return validation;

            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                _sequenceType = sequenceType;
                _frameRate = frameRate;
                _backgroundColor = backgroundColor;
                _requirements = requirements;
                change = AdvanceRevision(Array.Empty<long>());
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult<long> CreateGroupLayer(
            long parentId,
            ImmAuthoringLayerProperties properties,
            int siblingIndex = -1) => CreateLayer(ImmAuthoringLayerType.Group, parentId, properties, siblingIndex);

        public ImmAuthoringResult<long> CreatePaintLayer(
            long parentId,
            ImmAuthoringLayerProperties properties,
            int siblingIndex = -1) => CreateLayer(ImmAuthoringLayerType.Paint, parentId, properties, siblingIndex);

        public ImmAuthoringResult SetLayerProperties(long layerId, ImmAuthoringLayerProperties properties)
        {
            ImmAuthoringResult validation = ValidateLayerProperties(properties, layerId);
            if (!validation.Succeeded)
                return validation;

            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Layers.TryGetValue(layerId, out LayerNode layer))
                    return NotFound("Layer", layerId);
                layer.Properties = properties;
                change = AdvanceRevision(new[] { layerId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult ReparentLayer(long layerId, long newParentId, int siblingIndex = -1)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Layers.TryGetValue(layerId, out LayerNode layer))
                    return NotFound("Layer", layerId);
                if (newParentId == layerId)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.HierarchyCycle, "A layer cannot parent itself.", layerId);
                if (newParentId != 0)
                {
                    if (!_state.Layers.TryGetValue(newParentId, out LayerNode newParent))
                        return NotFound("Parent layer", newParentId);
                    if (newParent.Type != ImmAuthoringLayerType.Group)
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidOwner, "Only group layers can own child layers.", newParentId);
                    if (IsDescendantOf(newParentId, layerId))
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.HierarchyCycle, "Reparenting would create a hierarchy cycle.", layerId);
                }

                List<long> oldSiblings = GetChildList(layer.ParentId);
                List<long> newSiblings = GetChildList(newParentId);
                long oldParentId = layer.ParentId;
                int oldIndex = oldSiblings.IndexOf(layerId);
                if (oldIndex < 0)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Layer is missing from its parent's child list.", layerId);

                if (ReferenceEquals(oldSiblings, newSiblings))
                {
                    int target = NormalizeInsertIndex(siblingIndex, newSiblings.Count);
                    oldSiblings.RemoveAt(oldIndex);
                    if (target > oldIndex)
                        target--;
                    newSiblings.Insert(Math.Min(target, newSiblings.Count), layerId);
                }
                else
                {
                    int target = NormalizeInsertIndex(siblingIndex, newSiblings.Count);
                    oldSiblings.RemoveAt(oldIndex);
                    newSiblings.Insert(target, layerId);
                    layer.ParentId = newParentId;
                }

                change = AdvanceRevision(new[] { layerId, oldParentId, newParentId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult RemoveLayer(long layerId)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Layers.TryGetValue(layerId, out LayerNode layer))
                    return NotFound("Layer", layerId);

                List<long> removedIds = new List<long>();
                RemoveLayerRecursive(layerId, removedIds);
                GetChildList(layer.ParentId).Remove(layerId);
                removedIds.Add(layer.ParentId);
                change = AdvanceRevision(removedIds.ToArray());
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult<long> CreateDrawing(long paintLayerId)
        {
            return ApplyMutation(
                state =>
                {
                    if (!state.Layers.TryGetValue(paintLayerId, out LayerNode layer))
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.NotFound, "Paint layer was not found.", paintLayerId);
                    if (layer.Type != ImmAuthoringLayerType.Paint)
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.InvalidOwner, "Drawings can only be created in paint layers.", paintLayerId);
                    long id = state.AllocateId();
                    state.Drawings.Add(id, new DrawingNode { Id = id, PaintLayerId = paintLayerId });
                    layer.DrawingIds.Add(id);
                    return ImmAuthoringResult<long>.Success(id);
                },
                id => new[] { paintLayerId, id });
        }

        public ImmAuthoringResult<long> CloneDrawing(long drawingId)
        {
            return ApplyMutation(
                state =>
                {
                    if (!state.Drawings.TryGetValue(drawingId, out DrawingNode source))
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.NotFound, "Drawing was not found.", drawingId);
                    LayerNode layer = state.Layers[source.PaintLayerId];
                    long cloneId = state.AllocateId();
                    DrawingNode clone = new DrawingNode { Id = cloneId, PaintLayerId = source.PaintLayerId };
                    state.Drawings.Add(cloneId, clone);
                    layer.DrawingIds.Add(cloneId);
                    foreach (long strokeId in source.StrokeIds)
                    {
                        StrokeNode sourceStroke = state.Strokes[strokeId];
                        long cloneStrokeId = state.AllocateId();
                        StrokeNode cloneStroke = sourceStroke.Clone();
                        cloneStroke.Id = cloneStrokeId;
                        cloneStroke.DrawingId = cloneId;
                        state.Strokes.Add(cloneStrokeId, cloneStroke);
                        clone.StrokeIds.Add(cloneStrokeId);
                    }
                    return ImmAuthoringResult<long>.Success(cloneId);
                },
                id => new[] { drawingId, id });
        }

        public ImmAuthoringResult RemoveDrawing(long drawingId)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Drawings.TryGetValue(drawingId, out DrawingNode drawing))
                    return NotFound("Drawing", drawingId);
                LayerNode layer = _state.Layers[drawing.PaintLayerId];
                if (layer.FrameDrawingIds.Contains(drawingId))
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.DanglingReference, "Drawing is referenced by one or more frames.", drawingId);

                long[] strokes = drawing.StrokeIds.ToArray();
                foreach (long strokeId in strokes)
                    _state.Strokes.Remove(strokeId);
                layer.DrawingIds.Remove(drawingId);
                _state.Drawings.Remove(drawingId);
                List<long> affected = new List<long>(strokes) { drawingId, drawing.PaintLayerId };
                change = AdvanceRevision(affected.ToArray());
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult<long> CreateStroke(
            long drawingId,
            BrushSectionType brushSection,
            VisibilityType visibility,
            IReadOnlyList<PaintPoint> points,
            int strokeIndex = -1)
        {
            PaintPoint[] pointCopy = CopyPoints(points);
            ImmAuthoringResult validation = ValidateStroke(brushSection, visibility, pointCopy, drawingId);
            if (!validation.Succeeded)
                return ImmAuthoringResult<long>.Failure(validation.ErrorCode, validation.Message, validation.ObjectId);

            return ApplyMutation(
                state =>
                {
                    if (!state.Drawings.TryGetValue(drawingId, out DrawingNode drawing))
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.NotFound, "Drawing was not found.", drawingId);
                    int insertIndex = NormalizeInsertIndex(strokeIndex, drawing.StrokeIds.Count);
                    long id = state.AllocateId();
                    state.Strokes.Add(id, new StrokeNode
                    {
                        Id = id,
                        DrawingId = drawingId,
                        BrushSection = brushSection,
                        Visibility = visibility,
                        Points = pointCopy
                    });
                    drawing.StrokeIds.Insert(insertIndex, id);
                    return ImmAuthoringResult<long>.Success(id);
                },
                id => new[] { drawingId, id });
        }

        public ImmAuthoringResult ReplaceStroke(
            long strokeId,
            BrushSectionType brushSection,
            VisibilityType visibility,
            IReadOnlyList<PaintPoint> points)
        {
            PaintPoint[] pointCopy = CopyPoints(points);
            ImmAuthoringResult validation = ValidateStroke(brushSection, visibility, pointCopy, strokeId);
            if (!validation.Succeeded)
                return validation;

            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Strokes.TryGetValue(strokeId, out StrokeNode stroke))
                    return NotFound("Stroke", strokeId);
                stroke.BrushSection = brushSection;
                stroke.Visibility = visibility;
                stroke.Points = pointCopy;
                change = AdvanceRevision(new[] { strokeId, stroke.DrawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult<long> CloneStroke(long strokeId, long targetDrawingId, int strokeIndex = -1)
        {
            return ApplyMutation(
                state =>
                {
                    if (!state.Strokes.TryGetValue(strokeId, out StrokeNode source))
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.NotFound, "Stroke was not found.", strokeId);
                    if (!state.Drawings.TryGetValue(targetDrawingId, out DrawingNode target))
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.NotFound, "Target drawing was not found.", targetDrawingId);
                    int insertIndex = NormalizeInsertIndex(strokeIndex, target.StrokeIds.Count);
                    long cloneId = state.AllocateId();
                    StrokeNode clone = source.Clone();
                    clone.Id = cloneId;
                    clone.DrawingId = targetDrawingId;
                    state.Strokes.Add(cloneId, clone);
                    target.StrokeIds.Insert(insertIndex, cloneId);
                    return ImmAuthoringResult<long>.Success(cloneId);
                },
                id => new[] { strokeId, targetDrawingId, id });
        }

        public ImmAuthoringResult RemoveStroke(long strokeId)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Strokes.TryGetValue(strokeId, out StrokeNode stroke))
                    return NotFound("Stroke", strokeId);
                _state.Drawings[stroke.DrawingId].StrokeIds.Remove(strokeId);
                _state.Strokes.Remove(strokeId);
                change = AdvanceRevision(new[] { strokeId, stroke.DrawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult MoveStroke(long strokeId, int newIndex)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Strokes.TryGetValue(strokeId, out StrokeNode stroke))
                    return NotFound("Stroke", strokeId);
                List<long> strokes = _state.Drawings[stroke.DrawingId].StrokeIds;
                int oldIndex = strokes.IndexOf(strokeId);
                if (newIndex < 0 || newIndex >= strokes.Count)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Stroke index is outside the drawing.", strokeId);
                strokes.RemoveAt(oldIndex);
                strokes.Insert(newIndex, strokeId);
                change = AdvanceRevision(new[] { strokeId, stroke.DrawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult AppendFrame(long paintLayerId, long drawingId) => InsertFrame(paintLayerId, int.MaxValue, drawingId);

        public ImmAuthoringResult InsertFrame(long paintLayerId, int frameIndex, long drawingId)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                ImmAuthoringResult ownership = ValidateFrameOwnership(paintLayerId, drawingId);
                if (!ownership.Succeeded)
                    return ownership;
                LayerNode layer = _state.Layers[paintLayerId];
                int insertIndex = frameIndex == int.MaxValue ? layer.FrameDrawingIds.Count : frameIndex;
                if (insertIndex < 0 || insertIndex > layer.FrameDrawingIds.Count)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Frame index is outside the paint layer.", paintLayerId);
                layer.FrameDrawingIds.Insert(insertIndex, drawingId);
                change = AdvanceRevision(new[] { paintLayerId, drawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult SetFrame(long paintLayerId, int frameIndex, long drawingId)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                ImmAuthoringResult ownership = ValidateFrameOwnership(paintLayerId, drawingId);
                if (!ownership.Succeeded)
                    return ownership;
                LayerNode layer = _state.Layers[paintLayerId];
                if (frameIndex < 0 || frameIndex >= layer.FrameDrawingIds.Count)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Frame index is outside the paint layer.", paintLayerId);
                long previousDrawingId = layer.FrameDrawingIds[frameIndex];
                layer.FrameDrawingIds[frameIndex] = drawingId;
                change = AdvanceRevision(new[] { paintLayerId, previousDrawingId, drawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult MoveFrame(long paintLayerId, int oldIndex, int newIndex)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Layers.TryGetValue(paintLayerId, out LayerNode layer))
                    return NotFound("Paint layer", paintLayerId);
                if (layer.Type != ImmAuthoringLayerType.Paint)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidOwner, "Frames belong to paint layers.", paintLayerId);
                if (oldIndex < 0 || oldIndex >= layer.FrameDrawingIds.Count || newIndex < 0 || newIndex >= layer.FrameDrawingIds.Count)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Frame index is outside the paint layer.", paintLayerId);
                long drawingId = layer.FrameDrawingIds[oldIndex];
                layer.FrameDrawingIds.RemoveAt(oldIndex);
                layer.FrameDrawingIds.Insert(newIndex, drawingId);
                change = AdvanceRevision(new[] { paintLayerId, drawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult RemoveFrame(long paintLayerId, int frameIndex)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (!_state.Layers.TryGetValue(paintLayerId, out LayerNode layer))
                    return NotFound("Paint layer", paintLayerId);
                if (layer.Type != ImmAuthoringLayerType.Paint)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidOwner, "Frames belong to paint layers.", paintLayerId);
                if (frameIndex < 0 || frameIndex >= layer.FrameDrawingIds.Count)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Frame index is outside the paint layer.", paintLayerId);
                long drawingId = layer.FrameDrawingIds[frameIndex];
                layer.FrameDrawingIds.RemoveAt(frameIndex);
                change = AdvanceRevision(new[] { paintLayerId, drawingId });
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }

        public ImmAuthoringResult ResizeFrameSequence(long paintLayerId, int frameCount, long fillDrawingId = 0)
        {
            ImmAuthoringChange change;
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                if (frameCount < 0)
                    return ImmAuthoringResult.Failure(
                        ImmAuthoringErrorCode.InvalidArgument,
                        "Frame count cannot be negative.",
                        paintLayerId);
                if (!_state.Layers.TryGetValue(paintLayerId, out LayerNode layer))
                    return NotFound("Paint layer", paintLayerId);
                if (layer.Type != ImmAuthoringLayerType.Paint)
                    return ImmAuthoringResult.Failure(
                        ImmAuthoringErrorCode.InvalidOwner,
                        "Frames belong to paint layers.",
                        paintLayerId);
                if (frameCount == layer.FrameDrawingIds.Count)
                    return ImmAuthoringResult.Success();

                List<long> affectedIds = new List<long> { paintLayerId };
                if (frameCount < layer.FrameDrawingIds.Count)
                {
                    for (int index = frameCount; index < layer.FrameDrawingIds.Count; index++)
                        affectedIds.Add(layer.FrameDrawingIds[index]);
                    layer.FrameDrawingIds.RemoveRange(
                        frameCount,
                        layer.FrameDrawingIds.Count - frameCount);
                }
                else
                {
                    ImmAuthoringResult ownership = ValidateFrameOwnership(paintLayerId, fillDrawingId);
                    if (!ownership.Succeeded)
                        return ownership;
                    affectedIds.Add(fillDrawingId);
                    while (layer.FrameDrawingIds.Count < frameCount)
                        layer.FrameDrawingIds.Add(fillDrawingId);
                }

                change = AdvanceRevision(affectedIds.ToArray());
            }
            Publish(change);
            return ImmAuthoringResult.Success();
        }
        public ImmAuthoringResult<ImmAuthoringSnapshot> CreateSnapshot()
        {
            lock (_gate)
            {
                if (_disposed)
                    return ImmAuthoringResult<ImmAuthoringSnapshot>.Failure(ImmAuthoringErrorCode.Disposed, "Document is disposed.", DocumentId);
                return ImmAuthoringResult<ImmAuthoringSnapshot>.Success(CreateSnapshotUnsafe());
            }
        }

        public ImmAuthoringResult Validate()
        {
            lock (_gate)
            {
                if (_disposed)
                    return Disposed();
                return ValidateState(_state);
            }
        }

        public void Dispose()
        {
            lock (_gate)
            {
                if (_disposed)
                    return;
                _disposed = true;
                _state = null;
                Changed = null;
            }
        }

        private ImmAuthoringResult<long> CreateLayer(
            ImmAuthoringLayerType type,
            long parentId,
            ImmAuthoringLayerProperties properties,
            int siblingIndex)
        {
            ImmAuthoringResult validation = ValidateLayerProperties(properties);
            if (!validation.Succeeded)
                return ImmAuthoringResult<long>.Failure(validation.ErrorCode, validation.Message, validation.ObjectId);

            return ApplyMutation(
                state =>
                {
                    if (parentId != 0)
                    {
                        if (!state.Layers.TryGetValue(parentId, out LayerNode parent))
                            return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.NotFound, "Parent layer was not found.", parentId);
                        if (parent.Type != ImmAuthoringLayerType.Group)
                            return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.InvalidOwner, "Only group layers can own child layers.", parentId);
                    }
                    List<long> siblings = parentId == 0 ? state.RootLayerIds : state.Layers[parentId].ChildIds;
                    int insertIndex = NormalizeInsertIndex(siblingIndex, siblings.Count);
                    long id = state.AllocateId();
                    state.Layers.Add(id, new LayerNode { Id = id, ParentId = parentId, Type = type, Properties = properties });
                    siblings.Insert(insertIndex, id);
                    return ImmAuthoringResult<long>.Success(id);
                },
                id => new[] { parentId, id });
        }

        private ImmAuthoringResult<T> ApplyMutation<T>(
            Func<ImmAuthoringState, ImmAuthoringResult<T>> mutation,
            Func<T, long[]> affectedIds)
        {
            ImmAuthoringChange change;
            ImmAuthoringResult<T> result;
            lock (_gate)
            {
                if (_disposed)
                    return ImmAuthoringResult<T>.Failure(ImmAuthoringErrorCode.Disposed, "Document is disposed.", DocumentId);
                result = mutation(_state);
                if (!result.Succeeded)
                    return result;
                change = AdvanceRevision(affectedIds(result.Value));
            }
            Publish(change);
            return result;
        }

        private ImmAuthoringResult ValidateFrameOwnership(long paintLayerId, long drawingId)
        {
            if (_disposed)
                return Disposed();
            if (!_state.Layers.TryGetValue(paintLayerId, out LayerNode layer))
                return NotFound("Paint layer", paintLayerId);
            if (layer.Type != ImmAuthoringLayerType.Paint)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidOwner, "Frames belong to paint layers.", paintLayerId);
            if (!_state.Drawings.TryGetValue(drawingId, out DrawingNode drawing))
                return NotFound("Drawing", drawingId);
            if (drawing.PaintLayerId != paintLayerId)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidOwner, "Frame drawing must belong to the same paint layer.", drawingId);
            return ImmAuthoringResult.Success();
        }

        private bool IsDescendantOf(long candidateId, long ancestorId)
        {
            long current = candidateId;
            while (current != 0)
            {
                if (current == ancestorId)
                    return true;
                current = _state.Layers[current].ParentId;
            }
            return false;
        }

        private List<long> GetChildList(long parentId) => parentId == 0 ? _state.RootLayerIds : _state.Layers[parentId].ChildIds;

        private void RemoveLayerRecursive(long layerId, List<long> removedIds)
        {
            LayerNode layer = _state.Layers[layerId];
            foreach (long childId in layer.ChildIds.ToArray())
                RemoveLayerRecursive(childId, removedIds);
            foreach (long drawingId in layer.DrawingIds.ToArray())
            {
                DrawingNode drawing = _state.Drawings[drawingId];
                foreach (long strokeId in drawing.StrokeIds)
                {
                    _state.Strokes.Remove(strokeId);
                    removedIds.Add(strokeId);
                }
                _state.Drawings.Remove(drawingId);
                removedIds.Add(drawingId);
            }
            _state.Layers.Remove(layerId);
            removedIds.Add(layerId);
        }

        private ImmAuthoringSnapshot CreateSnapshotUnsafe() => _state.CreateSnapshot(
            DocumentId,
            _revision,
            _sequenceType,
            _frameRate,
            _backgroundColor,
            _requirements);

        private ImmAuthoringChange AdvanceRevision(long[] affectedIds)
        {
            _revision++;
            return new ImmAuthoringChange(DocumentId, _revision, RemoveDuplicateIds(affectedIds));
        }

        private void Publish(ImmAuthoringChange change) => Changed?.Invoke(change);

        private static long[] RemoveDuplicateIds(long[] ids)
        {
            if (ids == null || ids.Length == 0)
                return Array.Empty<long>();
            HashSet<long> unique = new HashSet<long>();
            foreach (long id in ids)
            {
                if (id > 0)
                    unique.Add(id);
            }
            long[] result = new long[unique.Count];
            unique.CopyTo(result);
            Array.Sort(result);
            return result;
        }

        private static int NormalizeInsertIndex(int requestedIndex, int count)
        {
            if (requestedIndex < 0 || requestedIndex > count)
                return count;
            return requestedIndex;
        }

        private static PaintPoint[] CopyPoints(IReadOnlyList<PaintPoint> points)
        {
            if (points == null)
                return null;
            PaintPoint[] copy = new PaintPoint[points.Count];
            for (int i = 0; i < points.Count; i++)
                copy[i] = points[i];
            return copy;
        }

        private static ImmAuthoringResult ValidateDocumentSettings(
            ExportSequenceType sequenceType,
            uint frameRate,
            Color backgroundColor,
            ExportRequirements requirements)
        {
            if (!Enum.IsDefined(typeof(ExportSequenceType), sequenceType))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Sequence type is invalid.");
            if (frameRate == 0 || ExportLayerTiming.TicksPerSecond % frameRate != 0)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, $"Frame rate must be a positive divisor of {ExportLayerTiming.TicksPerSecond}.");
            if (!IsFinite(backgroundColor.r) || !IsFinite(backgroundColor.g) || !IsFinite(backgroundColor.b) || !IsFinite(backgroundColor.a))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Background color must be finite.");
            if (requirements.MaxMemory < 0 || requirements.MaxRenderCalls < 0 || requirements.MaxTriangles < 0 || requirements.MaxSoundChannels < 0)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Requirements cannot be negative.");
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringResult ValidateLayerProperties(ImmAuthoringLayerProperties properties, long objectId = 0)
        {
            if (string.IsNullOrWhiteSpace(properties.Name))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Layer name cannot be empty.", objectId);
            if (!IsFinite(properties.Opacity) || properties.Opacity < 0f || properties.Opacity > 1f)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Layer opacity must be in the range 0-1.", objectId);
            if (!properties.Transform.IsFinite() || !properties.Pivot.IsFinite())
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Layer transform and pivot must be finite with positive scale.", objectId);
            if (properties.DurationTicks < 0)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Layer duration cannot be negative.", objectId);
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringResult ValidateStroke(
            BrushSectionType brushSection,
            VisibilityType visibility,
            PaintPoint[] points,
            long objectId)
        {
            if (!Enum.IsDefined(typeof(BrushSectionType), brushSection) || !Enum.IsDefined(typeof(VisibilityType), visibility))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Stroke brush or visibility type is invalid.", objectId);
            if (points == null || points.Length < 3)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Stroke must contain at least three points.", objectId);
            for (int i = 0; i < points.Length; i++)
            {
                PaintPoint point = points[i];
                if (!IsFinite(point.Position) || !IsFinite(point.Normal) || !IsFinite(point.Direction) ||
                    !IsFinite(point.Color.r) || !IsFinite(point.Color.g) || !IsFinite(point.Color.b) ||
                    !IsFinite(point.Alpha) || !IsFinite(point.Width) || !IsFinite(point.Length) || !IsFinite(point.Time) ||
                    point.Alpha < 0f || point.Alpha > 1f || point.Width <= 0f)
                {
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, $"Stroke point {i} contains invalid values.", objectId);
                }
            }
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringResult ValidateState(ImmAuthoringState state)
        {
            HashSet<long> visited = new HashSet<long>();
            foreach (long rootId in state.RootLayerIds)
            {
                ImmAuthoringResult result = ValidateLayerRecursive(state, rootId, 0, visited);
                if (!result.Succeeded)
                    return result;
            }
            if (visited.Count != state.Layers.Count)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "One or more layers are unreachable from the document root.");

            HashSet<long> reachableDrawings = new HashSet<long>();
            HashSet<long> reachableStrokes = new HashSet<long>();
            foreach (LayerNode layer in state.Layers.Values)
            {
                foreach (long drawingId in layer.DrawingIds)
                {
                    if (!reachableDrawings.Add(drawingId))
                        return ImmAuthoringResult.Failure(
                            ImmAuthoringErrorCode.ValidationFailed,
                            "Drawing appears more than once in the document.",
                            drawingId);
                }
            }
            if (reachableDrawings.Count != state.Drawings.Count)
                return ImmAuthoringResult.Failure(
                    ImmAuthoringErrorCode.ValidationFailed,
                    "One or more drawings are unreachable from their paint layer.");

            foreach (DrawingNode drawing in state.Drawings.Values)
            {
                foreach (long strokeId in drawing.StrokeIds)
                {
                    if (!reachableStrokes.Add(strokeId))
                        return ImmAuthoringResult.Failure(
                            ImmAuthoringErrorCode.ValidationFailed,
                            "Stroke appears more than once in the document.",
                            strokeId);
                }
            }
            if (reachableStrokes.Count != state.Strokes.Count)
                return ImmAuthoringResult.Failure(
                    ImmAuthoringErrorCode.ValidationFailed,
                    "One or more strokes are unreachable from their drawing.");
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringResult ValidateLayerRecursive(ImmAuthoringState state, long layerId, long expectedParentId, HashSet<long> visited)
        {
            if (!state.Layers.TryGetValue(layerId, out LayerNode layer))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Child list refers to a missing layer.", layerId);
            if (!visited.Add(layerId))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.HierarchyCycle, "Layer appears more than once in the hierarchy.", layerId);
            if (layer.ParentId != expectedParentId)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Layer parent does not match its owner's child list.", layerId);
            ImmAuthoringResult properties = ValidateLayerProperties(layer.Properties, layerId);
            if (!properties.Succeeded)
                return properties;
            if (layer.Type == ImmAuthoringLayerType.Group && (layer.DrawingIds.Count != 0 || layer.FrameDrawingIds.Count != 0))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidOwner, "Group layer contains paint data.", layerId);
            if (layer.Type == ImmAuthoringLayerType.Paint)
            {
                foreach (long drawingId in layer.DrawingIds)
                {
                    if (!state.Drawings.TryGetValue(drawingId, out DrawingNode drawing) || drawing.PaintLayerId != layerId)
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Paint layer contains an invalid drawing.", drawingId);
                    foreach (long strokeId in drawing.StrokeIds)
                    {
                        if (!state.Strokes.TryGetValue(strokeId, out StrokeNode stroke) || stroke.DrawingId != drawingId)
                            return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Drawing contains an invalid stroke.", strokeId);
                        ImmAuthoringResult strokeResult = ValidateStroke(stroke.BrushSection, stroke.Visibility, stroke.Points, strokeId);
                        if (!strokeResult.Succeeded)
                            return strokeResult;
                    }
                }
                foreach (long drawingId in layer.FrameDrawingIds)
                {
                    if (!state.Drawings.TryGetValue(drawingId, out DrawingNode drawing) || drawing.PaintLayerId != layerId)
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.DanglingReference, "Frame refers to a drawing outside its paint layer.", drawingId);
                }
            }
            foreach (long childId in layer.ChildIds)
            {
                ImmAuthoringResult childResult = ValidateLayerRecursive(state, childId, layerId, visited);
                if (!childResult.Succeeded)
                    return childResult;
            }
            return ImmAuthoringResult.Success();
        }

        private static bool IsFinite(Vector3 value) => IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
        private static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private static ImmAuthoringResult NotFound(string type, long id) =>
            ImmAuthoringResult.Failure(ImmAuthoringErrorCode.NotFound, $"{type} was not found.", id);

        private ImmAuthoringResult Disposed() =>
            ImmAuthoringResult.Failure(ImmAuthoringErrorCode.Disposed, "Document is disposed.", DocumentId);

        internal object SyncRoot => _gate;
        internal bool IsDisposedUnsafe => _disposed;
        internal long RevisionUnsafe => _revision;
        internal ImmAuthoringState CloneStateUnsafe() => _state.Clone();
        internal void ReplaceStateUnsafe(ImmAuthoringState state) => _state = state;
        internal ImmAuthoringChange AdvanceRevisionUnsafe(long[] affectedIds) => AdvanceRevision(affectedIds);
        internal void PublishTransactionChange(ImmAuthoringChange change) => Publish(change);
        internal ImmAuthoringSnapshot CreateSnapshotInternalUnsafe() => CreateSnapshotUnsafe();

        internal ImmAuthoringResult<long> CommitTransaction(
            ImmAuthoringDocument editable,
            long expectedRevision,
            long[] affectedObjectIds)
        {
            ImmAuthoringChange change = null;
            long committedRevision;
            lock (_gate)
            {
                if (_disposed)
                    return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.Disposed, "Document is disposed.", DocumentId);
                if (_revision != expectedRevision)
                    return ImmAuthoringResult<long>.Failure(
                        ImmAuthoringErrorCode.RevisionConflict,
                        $"Expected revision {expectedRevision}, but the document is at revision {_revision}.",
                        DocumentId);

                lock (editable._gate)
                {
                    if (editable._disposed)
                        return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.Disposed, "Transaction edit document is disposed.");
                    ImmAuthoringResult validation = ValidateState(editable._state);
                    if (!validation.Succeeded)
                        return ImmAuthoringResult<long>.Failure(validation.ErrorCode, validation.Message, validation.ObjectId);
                    if (editable._revision == expectedRevision)
                        return ImmAuthoringResult<long>.Success(_revision);

                    _state = editable._state.Clone();
                    _sequenceType = editable._sequenceType;
                    _frameRate = editable._frameRate;
                    _backgroundColor = editable._backgroundColor;
                    _requirements = editable._requirements;
                    change = AdvanceRevision(affectedObjectIds);
                    committedRevision = change.Revision;
                }
            }
            Publish(change);
            return ImmAuthoringResult<long>.Success(committedRevision);
        }
    }
}
