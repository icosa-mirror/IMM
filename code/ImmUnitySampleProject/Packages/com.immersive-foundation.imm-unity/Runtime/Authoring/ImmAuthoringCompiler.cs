using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using ImmPlayer.Exporter;

namespace ImmPlayer.Authoring
{
    public sealed class ImmAuthoringExportStatistics
    {
        public int LayerCount { get; internal set; }
        public int DrawingCount { get; internal set; }
        public int StrokeCount { get; internal set; }
        public long PointCount { get; internal set; }
        public TimeSpan GraphCompilationTime { get; internal set; }
        public TimeSpan SerializationTime { get; internal set; }
        public TimeSpan TotalTime { get; internal set; }
    }

    public sealed class ImmAuthoringExportResult
    {
        private readonly string[] _warnings;

        public bool Succeeded => ErrorCode == ImmAuthoringErrorCode.None;
        public ImmAuthoringErrorCode ErrorCode { get; }
        public string Message { get; }
        public long ObjectId { get; }
        public long SourceRevision { get; }
        public byte[] Data { get; }
        public string FilePath { get; }
        public long BytesWritten { get; }
        public IReadOnlyList<string> Warnings => Array.AsReadOnly(_warnings);
        public ImmAuthoringExportStatistics Statistics { get; }

        internal ImmAuthoringExportResult(
            ImmAuthoringErrorCode errorCode,
            string message,
            long objectId,
            long sourceRevision,
            byte[] data,
            string filePath,
            long bytesWritten,
            string[] warnings,
            ImmAuthoringExportStatistics statistics)
        {
            ErrorCode = errorCode;
            Message = message ?? string.Empty;
            ObjectId = objectId;
            SourceRevision = sourceRevision;
            Data = data;
            FilePath = filePath;
            BytesWritten = bytesWritten;
            _warnings = warnings ?? Array.Empty<string>();
            Statistics = statistics ?? new ImmAuthoringExportStatistics();
        }
    }

    public static class ImmAuthoringCompiler
    {
        public static ImmAuthoringExportResult ExportToMemory(
            ImmAuthoringDocument document,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus,
            CancellationToken cancellationToken = default)
        {
            return ExportToMemory(
                document,
                new ImmAuthoringOperationOptions(cancellationToken),
                opusBitrate,
                audioType);
        }

        public static ImmAuthoringExportResult ExportToMemory(
            ImmAuthoringDocument document,
            ImmAuthoringOperationOptions options,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus)
        {
            if (document == null)
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "Document cannot be null.", 0, 0);
            options = options ?? ImmAuthoringOperationOptions.Default;
            options.Report(ImmAuthoringProgressStage.Validating, 0, 1, "Capturing immutable document snapshot.");
            if (options.CancellationToken.IsCancellationRequested)
                return Failure(ImmAuthoringErrorCode.Cancelled, "Export was cancelled.", document.DocumentId, document.Revision);
            ImmAuthoringResult<ImmAuthoringSnapshot> snapshot = document.CreateSnapshot();
            return snapshot.Succeeded
                ? ExportToMemory(snapshot.Value, options, opusBitrate, audioType)
                : Failure(snapshot.ErrorCode, snapshot.Message, snapshot.ObjectId, document.Revision);
        }

        public static ImmAuthoringExportResult ExportToMemory(
            ImmAuthoringSnapshot snapshot,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus,
            CancellationToken cancellationToken = default)
        {
            return ExportToMemory(
                snapshot,
                new ImmAuthoringOperationOptions(cancellationToken),
                opusBitrate,
                audioType);
        }

        public static ImmAuthoringExportResult ExportToMemory(
            ImmAuthoringSnapshot snapshot,
            ImmAuthoringOperationOptions options,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus)
        {
            return ExportToMemoryCore(snapshot, options, opusBitrate, audioType, true);
        }

        private static ImmAuthoringExportResult ExportToMemoryCore(
            ImmAuthoringSnapshot snapshot,
            ImmAuthoringOperationOptions options,
            int opusBitrate,
            ExportAudioType audioType,
            bool reportOperationCompleted)
        {
            Stopwatch total = Stopwatch.StartNew();
            ImmAuthoringExportStatistics statistics = new ImmAuthoringExportStatistics();
            options = options ?? ImmAuthoringOperationOptions.Default;
            try
            {
                options.Report(ImmAuthoringProgressStage.Validating, 0, 1, "Validating content and resource limits.");
                ImmAuthoringExportResult preflight = Preflight(snapshot, opusBitrate, audioType, statistics, options.Limits);
                if (preflight != null)
                    return preflight;
                options.CancellationToken.ThrowIfCancellationRequested();
                options.Report(ImmAuthoringProgressStage.Validating, 1, 1, "Document is valid.");

                Stopwatch graph = Stopwatch.StartNew();
                ImmAuthoringResult<ExportSequence> compiled = Compile(snapshot, statistics, options);
                graph.Stop();
                statistics.GraphCompilationTime = graph.Elapsed;
                if (!compiled.Succeeded)
                    return Failure(compiled.ErrorCode, compiled.Message, compiled.ObjectId, snapshot.Revision, statistics, total);

                using (ExportSequence sequence = compiled.Value)
                {
                    options.CancellationToken.ThrowIfCancellationRequested();
                    options.Report(ImmAuthoringProgressStage.Serializing, 0, 1, "Serializing IMM to owned memory.");
                    Stopwatch serialization = Stopwatch.StartNew();
                    byte[] data = sequence.ExportToMemory(opusBitrate, audioType);
                    serialization.Stop();
                    statistics.SerializationTime = serialization.Elapsed;
                    options.CancellationToken.ThrowIfCancellationRequested();
                    if (data == null)
                        return Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native memory export failed.", 0, snapshot.Revision, statistics, total);
                    if (data.LongLength > options.Limits.MaxOutputBytes)
                    {
                        return Failure(
                            ImmAuthoringErrorCode.ResourceLimitExceeded,
                            $"Serialized IMM contains {data.LongLength:N0} bytes; the configured output limit is {options.Limits.MaxOutputBytes:N0}.",
                            0,
                            snapshot.Revision,
                            statistics,
                            total);
                    }
                    total.Stop();
                    statistics.TotalTime = total.Elapsed;
                    options.Report(ImmAuthoringProgressStage.Serializing, 1, 1, "Serialization completed.");
                    if (reportOperationCompleted)
                        options.Report(ImmAuthoringProgressStage.Completed, 1, 1, "Export completed.");
                    return new ImmAuthoringExportResult(
                        ImmAuthoringErrorCode.None,
                        string.Empty,
                        0,
                        snapshot.Revision,
                        data,
                        null,
                        data.LongLength,
                        Array.Empty<string>(),
                        statistics);
                }
            }
            catch (OperationCanceledException)
            {
                return Failure(ImmAuthoringErrorCode.Cancelled, "Export was cancelled.", 0, snapshot?.Revision ?? 0, statistics, total);
            }
            catch (Exception exception)
            {
                return Failure(ImmAuthoringErrorCode.NativeExportFailed, exception.Message, 0, snapshot?.Revision ?? 0, statistics, total);
            }
        }

        public static ImmAuthoringExportResult ExportToFile(
            ImmAuthoringDocument document,
            string filePath,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus,
            CancellationToken cancellationToken = default)
        {
            return ExportToFile(
                document,
                filePath,
                new ImmAuthoringOperationOptions(cancellationToken),
                opusBitrate,
                audioType);
        }

        public static ImmAuthoringExportResult ExportToFile(
            ImmAuthoringDocument document,
            string filePath,
            ImmAuthoringOperationOptions options,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus)
        {
            if (document == null)
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "Document cannot be null.", 0, 0);
            ImmAuthoringResult<ImmAuthoringSnapshot> snapshot = document.CreateSnapshot();
            return snapshot.Succeeded
                ? ExportToFile(snapshot.Value, filePath, options, opusBitrate, audioType)
                : Failure(snapshot.ErrorCode, snapshot.Message, snapshot.ObjectId, document.Revision);
        }

        public static ImmAuthoringExportResult ExportToFile(
            ImmAuthoringSnapshot snapshot,
            string filePath,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus,
            CancellationToken cancellationToken = default)
        {
            return ExportToFile(
                snapshot,
                filePath,
                new ImmAuthoringOperationOptions(cancellationToken),
                opusBitrate,
                audioType);
        }

        public static ImmAuthoringExportResult ExportToFile(
            ImmAuthoringSnapshot snapshot,
            string filePath,
            ImmAuthoringOperationOptions options,
            int opusBitrate = 96000,
            ExportAudioType audioType = ExportAudioType.Opus)
        {
            Stopwatch total = Stopwatch.StartNew();
            ImmAuthoringExportStatistics statistics = new ImmAuthoringExportStatistics();
            string temporaryPath = null;
            try
            {
                if (string.IsNullOrWhiteSpace(filePath))
                    return Failure(ImmAuthoringErrorCode.InvalidArgument, "File path cannot be empty.", 0, snapshot?.Revision ?? 0);
                options = options ?? ImmAuthoringOperationOptions.Default;
                string fullPath = Path.GetFullPath(filePath);
                string directory = Path.GetDirectoryName(fullPath);
                if (string.IsNullOrEmpty(directory) || !Directory.Exists(directory))
                    return Failure(ImmAuthoringErrorCode.InvalidArgument, $"Export directory does not exist: {directory}", 0, snapshot?.Revision ?? 0);

                ImmAuthoringExportResult memory = ExportToMemoryCore(snapshot, options, opusBitrate, audioType, false);
                statistics = memory.Statistics;
                if (!memory.Succeeded)
                    return new ImmAuthoringExportResult(memory.ErrorCode, memory.Message, memory.ObjectId, memory.SourceRevision, null, null, 0, Array.Empty<string>(), statistics);

                options.CancellationToken.ThrowIfCancellationRequested();
                temporaryPath = Path.Combine(directory, $".{Path.GetFileName(fullPath)}.{Guid.NewGuid():N}.tmp");
                options.Report(ImmAuthoringProgressStage.WritingOutput, 0, 1, "Writing atomic IMM output file.");
                File.WriteAllBytes(temporaryPath, memory.Data);
                options.CancellationToken.ThrowIfCancellationRequested();
                if (File.Exists(fullPath))
                    File.Replace(temporaryPath, fullPath, null);
                else
                    File.Move(temporaryPath, fullPath);
                temporaryPath = null;
                total.Stop();
                statistics.TotalTime = total.Elapsed;
                options.Report(ImmAuthoringProgressStage.WritingOutput, 1, 1, "IMM output file replaced atomically.");
                options.Report(ImmAuthoringProgressStage.Completed, 1, 1, "File export completed.");
                return new ImmAuthoringExportResult(
                    ImmAuthoringErrorCode.None,
                    string.Empty,
                    0,
                    snapshot.Revision,
                    null,
                    fullPath,
                    memory.BytesWritten,
                    Array.Empty<string>(),
                    statistics);
            }
            catch (OperationCanceledException)
            {
                return Failure(ImmAuthoringErrorCode.Cancelled, "Export was cancelled.", 0, snapshot?.Revision ?? 0, statistics, total);
            }
            catch (Exception exception)
            {
                return Failure(ImmAuthoringErrorCode.NativeExportFailed, exception.Message, 0, snapshot?.Revision ?? 0, statistics, total);
            }
            finally
            {
                if (!string.IsNullOrEmpty(temporaryPath) && File.Exists(temporaryPath))
                {
                    try { File.Delete(temporaryPath); }
                    catch { }
                }
            }
        }

        private static ImmAuthoringResult<ExportSequence> Compile(
            ImmAuthoringSnapshot snapshot,
            ImmAuthoringExportStatistics statistics,
            ImmAuthoringOperationOptions options)
        {
            long totalUnits = snapshot.Layers.Count;
            foreach (ImmAuthoringLayerSnapshot item in snapshot.Layers)
                foreach (ImmAuthoringDrawingSnapshot drawing in item.Drawings)
                    totalUnits += drawing.Strokes.Count;
            long completedUnits = 0;
            options.Report(ImmAuthoringProgressStage.CompilingGraph, 0, totalUnits, "Creating native authoring graph.");
            ExportSequence sequence = ExportSequence.Create(
                snapshot.SequenceType,
                snapshot.FrameRate,
                snapshot.BackgroundColor,
                snapshot.Requirements);
            if (sequence == null)
                return ImmAuthoringResult<ExportSequence>.Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native sequence creation failed.");

            Dictionary<long, IntPtr> layerHandles = new Dictionary<long, IntPtr>();
            try
            {
                foreach (ImmAuthoringLayerSnapshot layer in snapshot.Layers)
                {
                    options.CancellationToken.ThrowIfCancellationRequested();
                    IntPtr parent = layer.ParentId == 0 ? IntPtr.Zero : layerHandles[layer.ParentId];
                    TransformNative transform = ToNative(layer.Properties.Transform);
                    TransformNative pivot = ToNative(layer.Properties.Pivot);
                    IntPtr handle = layer.Type == ImmAuthoringLayerType.Group
                        ? Native.ImmExporter_CreateGroupLayer(
                            sequence.Handle, parent, layer.Properties.Name, layer.Properties.Visible ? 1 : 0,
                            layer.Properties.Opacity, ref transform, ref pivot, layer.Properties.IsTimeline ? 1 : 0,
                            layer.Properties.DurationTicks, layer.Properties.MaxRepeatCount)
                        : Native.ImmExporter_CreatePaintLayer(
                            sequence.Handle, parent, layer.Properties.Name, layer.Properties.Visible ? 1 : 0,
                            layer.Properties.Opacity, ref transform, ref pivot, layer.Properties.IsTimeline ? 1 : 0,
                            layer.Properties.DurationTicks, layer.Properties.MaxRepeatCount);
                    if (handle == IntPtr.Zero)
                        return CompileFailure(sequence, "Native layer creation failed.", layer.Id);
                    layerHandles.Add(layer.Id, handle);
                    statistics.LayerCount++;
                    options.Report(ImmAuthoringProgressStage.CompilingGraph, ++completedUnits, totalUnits, $"Compiled layer '{layer.Properties.Name}'.");

                    foreach (ImmAuthoringAnimationKeySnapshot key in layer.AnimationKeys)
                    {
                        TransformNative keyTransform = ToNative(key.Value.TransformValue);
                        if (!Native.ImmExporter_LayerAddAnimationKey(
                                handle,
                                (int)key.Property,
                                key.TimeTicks,
                                (int)key.Interpolation,
                                key.Value.BoolValue ? 1 : 0,
                                key.Value.UIntValue,
                                key.Value.FloatValue,
                                key.Value.DoubleValue,
                                ref keyTransform))
                        {
                            return CompileFailure(sequence, "Native animation key creation failed.", key.Id);
                        }
                    }

                    if (layer.Type == ImmAuthoringLayerType.Paint)
                    {
                        ImmAuthoringResult paintResult = CompilePaintLayer(layer, handle, statistics, options, ref completedUnits, totalUnits);
                        if (!paintResult.Succeeded)
                            return CompileFailure(sequence, paintResult.Message, paintResult.ObjectId);
                    }
                }
                return ImmAuthoringResult<ExportSequence>.Success(sequence);
            }
            catch
            {
                sequence.Dispose();
                throw;
            }
        }

        private static ImmAuthoringResult CompilePaintLayer(
            ImmAuthoringLayerSnapshot layer,
            IntPtr layerHandle,
            ImmAuthoringExportStatistics statistics,
            ImmAuthoringOperationOptions options,
            ref long completedUnits,
            long totalUnits)
        {
            Dictionary<long, uint> drawingIndices = new Dictionary<long, uint>();
            if (!Native.ImmExporter_PaintSetMaxRepeatCount(layerHandle, layer.Properties.PaintMaxRepeatCount))
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native paint repeat-count setup failed.", layer.Id);
            foreach (ImmAuthoringDrawingSnapshot drawing in layer.Drawings)
            {
                options.CancellationToken.ThrowIfCancellationRequested();
                IntPtr drawingHandle = Native.ImmExporter_CreateDrawing(layerHandle);
                if (drawingHandle == IntPtr.Zero)
                    return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native drawing creation failed.", drawing.Id);
                try
                {
                    drawingIndices.Add(drawing.Id, Native.ImmExporter_GetDrawingIndex(drawingHandle));
                    if (!Native.ImmExporter_DrawingInit(drawingHandle, (uint)drawing.Strokes.Count, 0))
                        return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native drawing initialization failed.", drawing.Id);
                    statistics.DrawingCount++;

                    for (int strokeIndex = 0; strokeIndex < drawing.Strokes.Count; strokeIndex++)
                    {
                        options.CancellationToken.ThrowIfCancellationRequested();
                        ImmAuthoringStrokeSnapshot stroke = drawing.Strokes[strokeIndex];
                        IntPtr elementHandle = Native.ImmExporter_DrawingGetElement(drawingHandle, (uint)strokeIndex);
                        if (elementHandle == IntPtr.Zero ||
                            !Native.ImmExporter_ElementInit(
                                elementHandle,
                                (uint)stroke.Points.Count,
                                (int)stroke.BrushSection,
                                (int)stroke.Visibility))
                        {
                            return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native stroke initialization failed.", stroke.Id);
                        }
                        PointNative[] points = ToNative(stroke.Points);
                        if (!Native.ImmExporter_ElementSetPoints(elementHandle, 0, points, (uint)points.Length))
                            return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.NativeExportFailed, "Native batch point transfer failed.", stroke.Id);
                        Native.ImmExporter_ComputeElementBounds(elementHandle);
                        statistics.StrokeCount++;
                        statistics.PointCount += points.LongLength;
                        options.Report(ImmAuthoringProgressStage.CompilingGraph, ++completedUnits, totalUnits, $"Compiled stroke {stroke.Id}.");
                    }
                    Native.ImmExporter_ComputeDrawingBounds(drawingHandle);
                }
                finally
                {
                    Native.ImmExporter_DestroyDrawing(drawingHandle);
                }
            }

            foreach (long drawingId in layer.FrameDrawingIds)
                Native.ImmExporter_PaintAddFrame(layerHandle, drawingIndices[drawingId]);
            return ImmAuthoringResult.Success();
        }

        private static ImmAuthoringExportResult Preflight(
            ImmAuthoringSnapshot snapshot,
            int opusBitrate,
            ExportAudioType audioType,
            ImmAuthoringExportStatistics statistics,
            ImmAuthoringLimits limits = null)
        {
            if (snapshot == null)
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "Snapshot cannot be null.", 0, 0, statistics);
            if (snapshot.SequenceType == ExportSequenceType.Comic)
                return Failure(ImmAuthoringErrorCode.Unsupported, "Comic authoring is not supported by the phase 3 compiler.", 0, snapshot.Revision, statistics);
            if (opusBitrate <= 0 || !Enum.IsDefined(typeof(ExportAudioType), audioType))
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "Audio export settings are invalid.", 0, snapshot.Revision, statistics);
            ImmAuthoringResult limitsResult = ImmAuthoringLimitValidator.Validate(snapshot, limits);
            if (!limitsResult.Succeeded)
                return Failure(limitsResult.ErrorCode, limitsResult.Message, limitsResult.ObjectId, snapshot.Revision, statistics);
            return null;
        }

        private static ImmAuthoringResult<ExportSequence> CompileFailure(ExportSequence sequence, string message, long objectId)
        {
            sequence.Dispose();
            return ImmAuthoringResult<ExportSequence>.Failure(ImmAuthoringErrorCode.NativeExportFailed, message, objectId);
        }

        private static TransformNative ToNative(ImmAuthoringTransform transform) => new TransformNative
        {
            Tx = transform.Position.x,
            Ty = transform.Position.y,
            Tz = transform.Position.z,
            Qx = transform.Rotation.x,
            Qy = transform.Rotation.y,
            Qz = transform.Rotation.z,
            Qw = transform.Rotation.w,
            Scale = transform.Scale
        };

        private static PointNative[] ToNative(IReadOnlyList<PaintPoint> points)
        {
            PointNative[] result = new PointNative[points.Count];
            for (int i = 0; i < points.Count; i++)
            {
                PaintPoint point = points[i];
                result[i] = new PointNative
                {
                    Px = point.Position.x, Py = point.Position.y, Pz = point.Position.z,
                    Nx = point.Normal.x, Ny = point.Normal.y, Nz = point.Normal.z,
                    Dx = point.Direction.x, Dy = point.Direction.y, Dz = point.Direction.z,
                    R = point.Color.r, G = point.Color.g, B = point.Color.b, A = point.Alpha,
                    Width = point.Width, Length = point.Length, Time = point.Time
                };
            }
            return result;
        }

        private static ImmAuthoringExportResult Failure(
            ImmAuthoringErrorCode code,
            string message,
            long objectId,
            long revision,
            ImmAuthoringExportStatistics statistics = null,
            Stopwatch total = null)
        {
            if (total != null)
            {
                total.Stop();
                statistics.TotalTime = total.Elapsed;
            }
            return new ImmAuthoringExportResult(
                code, message, objectId, revision, null, null, 0, Array.Empty<string>(), statistics);
        }
    }
}
