using System;
using System.Collections.Generic;
using System.IO;
using ImmPlayer.Exporter;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    public enum ImmAuthoringImportLossiness
    {
        Lossless = 0,
        Lossy = 1
    }

    public enum ImmAuthoringImportIssueCode
    {
        UnsupportedSequenceType,
        UnsupportedLayerType,
        UnsupportedRootAnimation,
        UnsupportedCapabilities,
        UnsupportedFlip,
        UnsupportedBrush,
        UnsupportedAnimationProperty,
        InvalidSourceValue,
        ReparentedLayer,
        FrameRateChanged
    }

    public sealed class ImmAuthoringImportIssue
    {
        public ImmAuthoringImportIssueCode Code { get; }
        public string Message { get; }
        public int SourceLayerId { get; }

        internal ImmAuthoringImportIssue(ImmAuthoringImportIssueCode code, string message, int sourceLayerId = 0)
        {
            Code = code;
            Message = message ?? string.Empty;
            SourceLayerId = sourceLayerId;
        }
    }

    public sealed class ImmAuthoringImportStatistics
    {
        public int SourceLayerCount { get; internal set; }
        public int ImportedLayerCount { get; internal set; }
        public int ImportedDrawingCount { get; internal set; }
        public int ImportedStrokeCount { get; internal set; }
        public long ImportedPointCount { get; internal set; }
        public int ImportedFrameCount { get; internal set; }
        public int ImportedAnimationKeyCount { get; internal set; }
    }

    public sealed class ImmAuthoringImportResult
    {
        private readonly ImmAuthoringImportIssue[] _issues;

        public bool Succeeded => ErrorCode == ImmAuthoringErrorCode.None && Document != null;
        public ImmAuthoringErrorCode ErrorCode { get; }
        public string Message { get; }
        public ImmAuthoringDocument Document { get; }
        public ImmAuthoringImportLossiness Lossiness { get; }
        public bool CanOverwriteSource => Succeeded && Lossiness == ImmAuthoringImportLossiness.Lossless;
        public IReadOnlyList<ImmAuthoringImportIssue> Issues => Array.AsReadOnly(_issues);
        public ImmAuthoringImportStatistics Statistics { get; }

        internal ImmAuthoringImportResult(
            ImmAuthoringErrorCode errorCode,
            string message,
            ImmAuthoringDocument document,
            ImmAuthoringImportIssue[] issues,
            ImmAuthoringImportStatistics statistics)
        {
            ErrorCode = errorCode;
            Message = message ?? string.Empty;
            Document = document;
            _issues = issues ?? Array.Empty<ImmAuthoringImportIssue>();
            Lossiness = _issues.Length == 0
                ? ImmAuthoringImportLossiness.Lossless
                : ImmAuthoringImportLossiness.Lossy;
            Statistics = statistics ?? new ImmAuthoringImportStatistics();
        }
    }

    public static class ImmAuthoringImporter
    {
        private const int SourceGroupLayerType = 0;
        private const int SourcePaintLayerType = 1;

        public static ImmAuthoringImportResult ImportFromFile(string filePath, string nativeLogPath = null)
        {
            if (string.IsNullOrWhiteSpace(filePath))
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "File path cannot be empty.");
            if (!File.Exists(filePath))
                return Failure(ImmAuthoringErrorCode.NotFound, $"IMM file was not found: {filePath}");

            using (StrokeReaderDocument source = new StrokeReaderDocument())
            {
                return source.Load(filePath, nativeLogPath)
                    ? ImportLoadedDocument(source)
                    : Failure(ImmAuthoringErrorCode.ValidationFailed, $"The IMM importer could not read '{filePath}'.");
            }
        }

        public static ImmAuthoringImportResult ImportFromMemory(byte[] immBytes, string nativeLogPath = null)
        {
            if (immBytes == null || immBytes.Length == 0)
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "IMM data cannot be null or empty.");

            using (StrokeReaderDocument source = new StrokeReaderDocument())
            {
                return source.Load(immBytes, nativeLogPath)
                    ? ImportLoadedDocument(source)
                    : Failure(ImmAuthoringErrorCode.ValidationFailed, "The IMM importer could not read the memory buffer.");
            }
        }

        private static ImmAuthoringImportResult ImportLoadedDocument(StrokeReaderDocument source)
        {
            ImmAuthoringImportStatistics statistics = new ImmAuthoringImportStatistics
            {
                SourceLayerCount = source.LayerCount
            };
            List<ImmAuthoringImportIssue> issues = new List<ImmAuthoringImportIssue>();
            if (!source.GetDocumentInfo(out StrokeDocumentInfo documentInfo))
                return Failure(ImmAuthoringErrorCode.ValidationFailed, "The source document metadata could not be read.", statistics);

            SourceLayer[] sourceLayers = new SourceLayer[source.LayerCount];
            for (int layerIndex = 0; layerIndex < sourceLayers.Length; layerIndex++)
            {
                if (!source.GetLayerInfo(layerIndex, out StrokeLayerInfo info) ||
                    !ImmStrokeReader.StrokeReader_GetLayerTransform(source.DocId, layerIndex, out StrokeLayerTransform local, out _))
                {
                    return Failure(
                        ImmAuthoringErrorCode.ValidationFailed,
                        $"Source layer {layerIndex} metadata could not be read.",
                        statistics);
                }
                sourceLayers[layerIndex] = new SourceLayer(layerIndex, info, local);
            }

            ExportSequenceType sequenceType = ConvertSequenceType(documentInfo.sequenceType, issues);
            uint frameRate = ResolveFrameRate(source.DocId, documentInfo.frameRate, sourceLayers, issues);
            ImmAuthoringResult<ImmAuthoringDocument> create = ImmAuthoringDocument.Create(
                sequenceType,
                frameRate,
                new Color(documentInfo.backgroundR, documentInfo.backgroundG, documentInfo.backgroundB, 1f));
            if (!create.Succeeded)
                return Failure(create.ErrorCode, create.Message, statistics);

            ImmAuthoringDocument document = create.Value;
            if (documentInfo.capabilities != 0)
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.UnsupportedCapabilities,
                    $"Source capability bits 0x{documentInfo.capabilities:X} are not represented by the paint authoring model."));
            }
            if (documentInfo.rootAnimationKeyCount != 0)
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.UnsupportedRootAnimation,
                    $"The source root contains {documentInfo.rootAnimationKeyCount} animation keys; root/comic animation is outside the supported paint subset."));
            }

            try
            {
                ImmAuthoringResult<ImmAuthoringTransaction> begin = document.BeginEdit(document.Revision);
                if (!begin.Succeeded)
                    return DisposeAndFail(document, begin.ErrorCode, begin.Message, statistics);
                using (ImmAuthoringTransaction transaction = begin.Value)
                {
                    ImmAuthoringDocument editable = transaction.EditableDocument;
                    Dictionary<int, long> importedLayers = new Dictionary<int, long>();
                    Dictionary<int, SourceLayer> layersBySourceId = new Dictionary<int, SourceLayer>();
                    foreach (SourceLayer layer in sourceLayers)
                        layersBySourceId[layer.Info.id] = layer;

                    foreach (SourceLayer sourceLayer in sourceLayers)
                    {
                        if (sourceLayer.Info.type != SourceGroupLayerType && sourceLayer.Info.type != SourcePaintLayerType)
                        {
                            issues.Add(new ImmAuthoringImportIssue(
                                ImmAuthoringImportIssueCode.UnsupportedLayerType,
                                $"Layer '{sourceLayer.Info.name}' uses unsupported IMM layer type {sourceLayer.Info.type} and was omitted.",
                                sourceLayer.Info.id));
                            continue;
                        }

                        long parentId = ResolveImportedParent(
                            sourceLayer,
                            layersBySourceId,
                            importedLayers,
                            issues);
                        ImmAuthoringLayerProperties properties = ConvertLayerProperties(sourceLayer, issues);
                        ImmAuthoringResult<long> addLayer = sourceLayer.Info.type == SourceGroupLayerType
                            ? editable.CreateGroupLayer(parentId, properties, sourceLayer.Info.childIndex)
                            : editable.CreatePaintLayer(parentId, properties, sourceLayer.Info.childIndex);
                        if (!addLayer.Succeeded)
                            return DisposeAndFail(document, addLayer.ErrorCode, addLayer.Message, statistics, transaction);

                        long importedLayerId = addLayer.Value;
                        importedLayers[sourceLayer.Info.id] = importedLayerId;
                        statistics.ImportedLayerCount++;

                        ImmAuthoringResult keys = ImportAnimationKeys(
                            source,
                            sourceLayer,
                            editable,
                            importedLayerId,
                            issues,
                            statistics);
                        if (!keys.Succeeded)
                            return DisposeAndFail(document, keys.ErrorCode, keys.Message, statistics, transaction);

                        if (sourceLayer.Info.type == SourcePaintLayerType)
                        {
                            ImmAuthoringResult paint = ImportPaintLayer(
                                source,
                                sourceLayer,
                                editable,
                                importedLayerId,
                                frameRate,
                                issues,
                                statistics);
                            if (!paint.Succeeded)
                                return DisposeAndFail(document, paint.ErrorCode, paint.Message, statistics, transaction);
                        }
                    }

                    ImmAuthoringResult<long> commit = transaction.Commit();
                    if (!commit.Succeeded)
                        return DisposeAndFail(document, commit.ErrorCode, commit.Message, statistics);
                }

                return new ImmAuthoringImportResult(
                    ImmAuthoringErrorCode.None,
                    string.Empty,
                    document,
                    issues.ToArray(),
                    statistics);
            }
            catch (Exception exception)
            {
                document.Dispose();
                return Failure(ImmAuthoringErrorCode.ValidationFailed, exception.Message, statistics);
            }
        }

        private static ImmAuthoringResult ImportPaintLayer(
            StrokeReaderDocument source,
            SourceLayer sourceLayer,
            ImmAuthoringDocument editable,
            long importedLayerId,
            uint documentFrameRate,
            List<ImmAuthoringImportIssue> issues,
            ImmAuthoringImportStatistics statistics)
        {
            int drawingCount = source.GetDrawingCount(sourceLayer.Index);
            long[] drawingIds = new long[drawingCount];
            for (int drawingIndex = 0; drawingIndex < drawingCount; drawingIndex++)
            {
                ImmAuthoringResult<long> drawing = editable.CreateDrawing(importedLayerId);
                if (!drawing.Succeeded)
                    return drawing.WithoutValue();
                drawingIds[drawingIndex] = drawing.Value;
                statistics.ImportedDrawingCount++;

                int strokeCount = source.GetStrokeCount(sourceLayer.Index, drawingIndex);
                for (int strokeIndex = 0; strokeIndex < strokeCount; strokeIndex++)
                {
                    if (!source.GetStrokeInfo(sourceLayer.Index, drawingIndex, strokeIndex, out StrokeInfo strokeInfo))
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Source stroke metadata could not be read.", importedLayerId);
                    if (strokeInfo.brushType <= (int)BrushSectionType.Point ||
                        strokeInfo.brushType > (int)BrushSectionType.Square ||
                        !Enum.IsDefined(typeof(VisibilityType), strokeInfo.visibilityMode))
                    {
                        issues.Add(new ImmAuthoringImportIssue(
                            ImmAuthoringImportIssueCode.UnsupportedBrush,
                            $"Layer '{sourceLayer.Info.name}' contains unsupported brush/visibility values ({strokeInfo.brushType}/{strokeInfo.visibilityMode}); that stroke was omitted.",
                            sourceLayer.Info.id));
                        continue;
                    }

                    StrokePoint[] sourcePoints = source.GetStrokePoints(sourceLayer.Index, drawingIndex, strokeIndex);
                    if (sourcePoints == null || sourcePoints.Length < 2)
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Source stroke points could not be read.", importedLayerId);
                    PaintPoint[] points = new PaintPoint[sourcePoints.Length];
                    bool invalidPosition = false;
                    PointAdjustment adjustedPointMetadata = PointAdjustment.None;
                    for (int pointIndex = 0; pointIndex < points.Length; pointIndex++)
                    {
                        StrokePoint point = sourcePoints[pointIndex];
                        Vector3 position = point.Position;
                        if (!IsFinite(position))
                        {
                            invalidPosition = true;
                            break;
                        }

                        Vector3 normal = point.Normal;
                        Vector3 direction = point.ViewDirection;
                        float red = point.r;
                        float green = point.g;
                        float blue = point.b;
                        float alpha = point.alpha;
                        float width = point.width;
                        float length = point.length;
                        float time = point.time;

                        if (SanitizeVector(ref normal, Vector3.up)) adjustedPointMetadata |= PointAdjustment.Normal;
                        if (SanitizeVector(ref direction, Vector3.zero)) adjustedPointMetadata |= PointAdjustment.Direction;
                        if (SanitizeUnitValue(ref red, 1f) |
                            SanitizeUnitValue(ref green, 1f) |
                            SanitizeUnitValue(ref blue, 1f)) adjustedPointMetadata |= PointAdjustment.Color;
                        if (SanitizeUnitValue(ref alpha, 1f)) adjustedPointMetadata |= PointAdjustment.Alpha;
                        if (SanitizePositive(ref width, 0.000001f)) adjustedPointMetadata |= PointAdjustment.Width;
                        if (SanitizeFinite(ref length, 0f)) adjustedPointMetadata |= PointAdjustment.Length;
                        if (SanitizeFinite(ref time, 0f)) adjustedPointMetadata |= PointAdjustment.Time;

                        points[pointIndex] = new PaintPoint
                        {
                            Position = position,
                            Normal = normal,
                            Direction = direction,
                            Color = new Color(red, green, blue, 1f),
                            Alpha = alpha,
                            Width = width,
                            Length = length,
                            Time = time
                        };
                    }
                    if (invalidPosition)
                    {
                        issues.Add(new ImmAuthoringImportIssue(
                            ImmAuthoringImportIssueCode.InvalidSourceValue,
                            $"Layer '{sourceLayer.Info.name}' contains a stroke with a non-finite position; that stroke was omitted.",
                            sourceLayer.Info.id));
                        continue;
                    }
                    if (adjustedPointMetadata != PointAdjustment.None)
                    {
                        issues.Add(new ImmAuthoringImportIssue(
                            ImmAuthoringImportIssueCode.InvalidSourceValue,
                            $"Layer '{sourceLayer.Info.name}' contained invalid point metadata ({adjustedPointMetadata}); affected values were clamped or replaced.",
                            sourceLayer.Info.id));
                    }

                    ImmAuthoringResult<long> stroke = editable.CreateStroke(
                        drawing.Value,
                        (BrushSectionType)strokeInfo.brushType,
                        (VisibilityType)strokeInfo.visibilityMode,
                        points);
                    if (!stroke.Succeeded)
                        return stroke.WithoutValue();
                    statistics.ImportedStrokeCount++;
                    statistics.ImportedPointCount += points.LongLength;
                }
            }

            if (!ImmStrokeReader.StrokeReader_GetLayerAnimationInfo(
                    source.DocId,
                    sourceLayer.Index,
                    out int layerFrameRate,
                    out int frameCount,
                    out int paintMaxRepeatCount))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Paint frame metadata could not be read.", importedLayerId);
            ImmAuthoringResult<ImmAuthoringSnapshot> currentSnapshot = editable.CreateSnapshot();
            if (!currentSnapshot.Succeeded || !currentSnapshot.Value.TryGetLayer(importedLayerId, out ImmAuthoringLayerSnapshot importedLayer))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Imported paint layer could not be queried.", importedLayerId);
            ImmAuthoringLayerProperties paintProperties = importedLayer.Properties;
            paintProperties.PaintMaxRepeatCount = (uint)Math.Max(0, paintMaxRepeatCount);
            ImmAuthoringResult setPaintProperties = editable.SetLayerProperties(importedLayerId, paintProperties);
            if (!setPaintProperties.Succeeded)
                return setPaintProperties;
            if (layerFrameRate > 0 && layerFrameRate != documentFrameRate)
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.FrameRateChanged,
                    $"Layer '{sourceLayer.Info.name}' uses {layerFrameRate} fps while the document uses {documentFrameRate} fps.",
                    sourceLayer.Info.id));
            }
            if (frameCount > 0)
            {
                int[] frameBuffer = new int[frameCount];
                int read = ImmStrokeReader.StrokeReader_GetFrameBuffer(source.DocId, sourceLayer.Index, frameBuffer, frameBuffer.Length);
                if (read != frameCount)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Paint frame mapping could not be read.", importedLayerId);
                for (int frameIndex = 0; frameIndex < frameCount; frameIndex++)
                {
                    int drawingIndex = frameBuffer[frameIndex];
                    if (drawingIndex < 0 || drawingIndex >= drawingIds.Length)
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.DanglingReference, "A source frame refers to a missing drawing.", importedLayerId);
                    ImmAuthoringResult frame = editable.AppendFrame(importedLayerId, drawingIds[drawingIndex]);
                    if (!frame.Succeeded)
                        return frame;
                    statistics.ImportedFrameCount++;
                }
            }
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringResult ImportAnimationKeys(
            StrokeReaderDocument source,
            SourceLayer sourceLayer,
            ImmAuthoringDocument editable,
            long importedLayerId,
            List<ImmAuthoringImportIssue> issues,
            ImmAuthoringImportStatistics statistics)
        {
            StrokeAnimationKey[] keys = source.GetLayerAnimationKeys(sourceLayer.Index);
            if (keys == null)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.ValidationFailed, "Layer animation keys could not be read.", importedLayerId);
            int visibilityKeyCount = 0;
            foreach (StrokeAnimationKey candidate in keys)
            {
                if (candidate.property == (int)ImmAuthoringAnimationProperty.Visibility)
                    visibilityKeyCount++;
            }
            foreach (StrokeAnimationKey sourceKey in keys)
            {
                if (visibilityKeyCount == 1 &&
                    sourceKey.property == (int)ImmAuthoringAnimationProperty.Visibility &&
                    sourceKey.timeTicks == 0 &&
                    sourceKey.interpolation == (int)ImmAuthoringInterpolation.None &&
                    sourceKey.boolValue != 0)
                {
                    continue;
                }
                if (!Enum.IsDefined(typeof(ImmAuthoringAnimationProperty), sourceKey.property) ||
                    !Enum.IsDefined(typeof(ImmAuthoringInterpolation), sourceKey.interpolation))
                {
                    issues.Add(new ImmAuthoringImportIssue(
                        ImmAuthoringImportIssueCode.InvalidSourceValue,
                        $"Layer '{sourceLayer.Info.name}' contains an unsupported animation property or interpolation; that key was omitted.",
                        sourceLayer.Info.id));
                    continue;
                }

                ImmAuthoringAnimationProperty property = (ImmAuthoringAnimationProperty)sourceKey.property;
                if (property == ImmAuthoringAnimationProperty.Position ||
                    property == ImmAuthoringAnimationProperty.Rotation ||
                    property == ImmAuthoringAnimationProperty.Scale)
                {
                    issues.Add(new ImmAuthoringImportIssue(
                        ImmAuthoringImportIssueCode.UnsupportedAnimationProperty,
                        $"Layer '{sourceLayer.Info.name}' contains an obsolete {property} animation key; it was omitted in favour of IMM v2 Transform keys.",
                        sourceLayer.Info.id));
                    continue;
                }
                ImmAuthoringTransform transform = ConvertTransform(
                    sourceKey.transformValue,
                    sourceLayer.Info.id,
                    "animation key",
                    issues);
                ImmAuthoringAnimationValue value = new ImmAuthoringAnimationValue
                {
                    BoolValue = sourceKey.boolValue != 0,
                    UIntValue = sourceKey.intValue,
                    FloatValue = sourceKey.floatValue,
                    DoubleValue = sourceKey.doubleValue,
                    TransformValue = transform
                };
                ImmAuthoringResult<long> add = editable.CreateAnimationKey(
                    importedLayerId,
                    property,
                    sourceKey.timeTicks,
                    value,
                    (ImmAuthoringInterpolation)sourceKey.interpolation);
                if (!add.Succeeded)
                    return add.WithoutValue();
                statistics.ImportedAnimationKeyCount++;
            }
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringLayerProperties ConvertLayerProperties(
            SourceLayer source,
            List<ImmAuthoringImportIssue> issues)
        {
            ImmAuthoringLayerProperties properties = ImmAuthoringLayerProperties.Default(
                string.IsNullOrWhiteSpace(source.Info.name) ? $"Layer {source.Info.id}" : source.Info.name);
            properties.Visible = source.Info.visible != 0;
            properties.Opacity = Mathf.Clamp01(source.Info.opacity);
            if (!Mathf.Approximately(properties.Opacity, source.Info.opacity))
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.InvalidSourceValue,
                    $"Layer '{properties.Name}' opacity was outside 0-1 and was clamped.",
                    source.Info.id));
            }
            properties.Transform = ConvertTransform(source.LocalTransform, source.Info.id, "transform", issues);
            properties.Pivot = ConvertTransform(
                new StrokeLayerTransform
                {
                    rotX = source.Info.pivotRotX,
                    rotY = source.Info.pivotRotY,
                    rotZ = source.Info.pivotRotZ,
                    rotW = source.Info.pivotRotW,
                    scale = source.Info.pivotScale,
                    flip = source.Info.pivotFlip,
                    transX = source.Info.pivotTransX,
                    transY = source.Info.pivotTransY,
                    transZ = source.Info.pivotTransZ
                },
                source.Info.id,
                "pivot",
                issues);
            properties.IsTimeline = source.Info.isTimeline != 0;
            properties.DurationTicks = Math.Max(0, source.Info.durationTicks);
            properties.MaxRepeatCount = source.Info.maxRepeatCount;
            return properties;
        }

        private static ImmAuthoringTransform ConvertTransform(
            StrokeLayerTransform source,
            int sourceLayerId,
            string description,
            List<ImmAuthoringImportIssue> issues)
        {
            if (source.flip != 0)
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.UnsupportedFlip,
                    $"Layer {sourceLayerId} uses a flipped {description}; flip state is not represented by the current authoring transform.",
                    sourceLayerId));
            }
            Quaternion rotation = new Quaternion(source.rotX, source.rotY, source.rotZ, source.rotW);
            float rotationMagnitudeSquared =
                rotation.x * rotation.x + rotation.y * rotation.y +
                rotation.z * rotation.z + rotation.w * rotation.w;
            if (!IsFinite(rotation) || rotationMagnitudeSquared < 0.000001f)
            {
                rotation = Quaternion.identity;
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.InvalidSourceValue,
                    $"Layer {sourceLayerId} contains an invalid {description} rotation; identity was used.",
                    sourceLayerId));
            }
            else
            {
                float inverseMagnitude = 1f / Mathf.Sqrt(rotationMagnitudeSquared);
                rotation = new Quaternion(
                    rotation.x * inverseMagnitude,
                    rotation.y * inverseMagnitude,
                    rotation.z * inverseMagnitude,
                    rotation.w * inverseMagnitude);
            }
            float scale = source.scale;
            if (!IsFinite(scale) || scale <= 0f)
            {
                scale = 1f;
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.InvalidSourceValue,
                    $"Layer {sourceLayerId} contains an invalid {description} scale; 1 was used.",
                    sourceLayerId));
            }
            return new ImmAuthoringTransform
            {
                Position = new Vector3(source.transX, source.transY, source.transZ),
                Rotation = rotation,
                Scale = scale
            };
        }

        private static long ResolveImportedParent(
            SourceLayer source,
            Dictionary<int, SourceLayer> layersBySourceId,
            Dictionary<int, long> importedLayers,
            List<ImmAuthoringImportIssue> issues)
        {
            int parentSourceId = source.Info.parentId;
            bool skippedParent = false;
            while (parentSourceId != 0 && !importedLayers.TryGetValue(parentSourceId, out _))
            {
                skippedParent = true;
                if (!layersBySourceId.TryGetValue(parentSourceId, out SourceLayer parent))
                    break;
                parentSourceId = parent.Info.parentId;
            }
            if (skippedParent)
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.ReparentedLayer,
                    $"Layer '{source.Info.name}' was reparented because its source parent is outside the supported paint subset.",
                    source.Info.id));
            }
            return parentSourceId != 0 && importedLayers.TryGetValue(parentSourceId, out long parentId)
                ? parentId
                : 0;
        }

        private static ExportSequenceType ConvertSequenceType(
            int sourceType,
            List<ImmAuthoringImportIssue> issues)
        {
            if (sourceType == (int)ExportSequenceType.Still)
                return ExportSequenceType.Still;
            if (sourceType == (int)ExportSequenceType.Animated)
                return ExportSequenceType.Animated;
            issues.Add(new ImmAuthoringImportIssue(
                ImmAuthoringImportIssueCode.UnsupportedSequenceType,
                $"Sequence type {sourceType} is outside the supported still/animated paint subset and was converted to Animated."));
            return ExportSequenceType.Animated;
        }

        private static uint ResolveFrameRate(
            int sourceDocumentId,
            uint documentFrameRate,
            SourceLayer[] layers,
            List<ImmAuthoringImportIssue> issues)
        {
            uint frameRate = documentFrameRate;
            if (frameRate == 0)
            {
                foreach (SourceLayer layer in layers)
                {
                    if (layer.Info.type == SourcePaintLayerType &&
                        ImmStrokeReader.StrokeReader_GetLayerAnimationInfo(sourceDocumentId, layer.Index, out int paintRate, out _, out _) &&
                        paintRate > 0)
                    {
                        frameRate = (uint)paintRate;
                        break;
                    }
                }
            }
            if (frameRate == 0 || ExportLayerTiming.TicksPerSecond % frameRate != 0)
            {
                issues.Add(new ImmAuthoringImportIssue(
                    ImmAuthoringImportIssueCode.FrameRateChanged,
                    $"Source frame rate {frameRate} is not supported by the exact-tick authoring model; 24 fps was used."));
                frameRate = 24;
            }
            return frameRate;
        }

        private static ImmAuthoringImportResult DisposeAndFail(
            ImmAuthoringDocument document,
            ImmAuthoringErrorCode errorCode,
            string message,
            ImmAuthoringImportStatistics statistics,
            ImmAuthoringTransaction transaction = null)
        {
            transaction?.Abort();
            document.Dispose();
            return Failure(errorCode, message, statistics);
        }

        private static ImmAuthoringImportResult Failure(
            ImmAuthoringErrorCode errorCode,
            string message,
            ImmAuthoringImportStatistics statistics = null) =>
            new ImmAuthoringImportResult(errorCode, message, null, null, statistics);

        private static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private static bool IsFinite(Vector3 value) =>
            IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);

        private static bool IsFinite(Quaternion value) =>
            IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z) && IsFinite(value.w);

        private static bool SanitizeVector(ref Vector3 value, Vector3 fallback)
        {
            if (IsFinite(value))
                return false;
            value = fallback;
            return true;
        }

        private static bool SanitizeUnitValue(ref float value, float fallback)
        {
            float sanitized = IsFinite(value) ? Mathf.Clamp01(value) : fallback;
            if (sanitized.Equals(value))
                return false;
            value = sanitized;
            return true;
        }

        private static bool SanitizePositive(ref float value, float fallback)
        {
            if (IsFinite(value) && value > 0f)
                return false;
            value = fallback;
            return true;
        }

        private static bool SanitizeFinite(ref float value, float fallback)
        {
            if (IsFinite(value))
                return false;
            value = fallback;
            return true;
        }

        [Flags]
        private enum PointAdjustment
        {
            None = 0,
            Normal = 1 << 0,
            Direction = 1 << 1,
            Color = 1 << 2,
            Alpha = 1 << 3,
            Width = 1 << 4,
            Length = 1 << 5,
            Time = 1 << 6
        }

        private sealed class SourceLayer
        {
            internal int Index { get; }
            internal StrokeLayerInfo Info { get; }
            internal StrokeLayerTransform LocalTransform { get; }

            internal SourceLayer(int index, StrokeLayerInfo info, StrokeLayerTransform localTransform)
            {
                Index = index;
                Info = info;
                LocalTransform = localTransform;
            }
        }
    }
}
