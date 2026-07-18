using System;
using System.Collections;
using System.Diagnostics;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace ImmPlayer.Samples
{
    /// <summary>
    /// Phase 1 vertical slice: construct, compile to memory, load, seek, play,
    /// render, and unload a runtime-generated multi-frame IMM paint animation.
    /// </summary>
    public sealed class ImmRuntimeAnimationSpike : MonoBehaviour
    {
        private const string LogPrefix = "[IMM_AUTHOR_SPIKE_P1]";

        [SerializeField] private bool runOnStart;
        [SerializeField] private uint frameRate = 30;
        [SerializeField] private int frameCount = 24;
        [SerializeField] private float loadTimeoutSeconds = 30f;

        private IEnumerator Start()
        {
            if (runOnStart)
                yield return RunCycles(1);
        }

        [ContextMenu("Run Animation Spike")]
        public void RunOnce() => StartCoroutine(RunCycles(1));

        [ContextMenu("Run 100-cycle Lifecycle Gate")]
        public void RunLifecycleGate() => StartCoroutine(RunCycles(100));

        public IEnumerator RunCycles(int cycles)
        {
            if (cycles <= 0 || frameCount <= 0)
            {
                Debug.LogError($"{LogPrefix} invalid cycles={cycles} frameCount={frameCount}");
                yield break;
            }

            ImmPlayerManager manager = ImmPlayerManager.Instance;
            if (!manager.Initialize())
            {
                Debug.LogError($"{LogPrefix} failed=player-initialize");
                yield break;
            }

            int baselineDocuments = manager.LoadedDocumentCount;
            int baselineBuffers = manager.OwnedInputBufferCount;
            long initialManagedBytes = GC.GetTotalMemory(true);
            long initialWorkingSet = Process.GetCurrentProcess().WorkingSet64;
            Stopwatch totalTimer = Stopwatch.StartNew();
            int completed = 0;

            for (int cycle = 0; cycle < cycles; cycle++)
            {
                Stopwatch constructionTimer = Stopwatch.StartNew();
                ImmAuthoringResult<ImmAuthoringDocument> documentResult = BuildAnimation();
                constructionTimer.Stop();
                if (!documentResult.Succeeded)
                {
                    Debug.LogError(
                        $"{LogPrefix} cycle={cycle} failed=construction code={documentResult.ErrorCode} " +
                        $"objectId={documentResult.ObjectId} message={documentResult.Message}");
                    yield break;
                }

                ImmAuthoringExportResult export;
                using (ImmAuthoringDocument authoringDocument = documentResult.Value)
                    export = ImmAuthoringCompiler.ExportToMemory(authoringDocument);
                if (!export.Succeeded)
                {
                    Debug.LogError(
                        $"{LogPrefix} cycle={cycle} failed=compile code={export.ErrorCode} " +
                        $"revision={export.SourceRevision} objectId={export.ObjectId} message={export.Message}");
                    yield break;
                }

                Stopwatch loadTimer = Stopwatch.StartNew();
                ImmDocument playbackDocument = manager.LoadDocumentFromMemory(
                    export.Data,
                    $"imm-runtime-animation-spike-{cycle}.imm");
                if (playbackDocument == null)
                {
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=load-start");
                    yield break;
                }

                float deadline = Time.realtimeSinceStartup + loadTimeoutSeconds;
                while (!playbackDocument.IsSequenceReady() && Time.realtimeSinceStartup < deadline)
                    yield return null;
                loadTimer.Stop();

                if (!playbackDocument.IsSequenceReady())
                {
                    manager.UnloadDocument(playbackDocument);
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=load-timeout");
                    yield break;
                }

                bool foundExpectedFrames = false;
                foreach (ImmDocument.LayerInfo layer in playbackDocument.GetLayersManaged())
                {
                    if (layer.Type == ImmDocument.LayerType.Paint && layer.PaintNumFrames == frameCount)
                    {
                        foundExpectedFrames = true;
                        break;
                    }
                }

                if (!foundExpectedFrames)
                {
                    manager.UnloadDocument(playbackDocument);
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=frame-count expected={frameCount}");
                    yield break;
                }

                long seekTicks = ExportLayerTiming.FromFrames(frameCount / 2, frameRate).DurationTicks;
                playbackDocument.Pause();
                playbackDocument.SetTime(seekTicks, 0);
                playbackDocument.Resume();
                Stopwatch firstRenderTimer = Stopwatch.StartNew();
                yield return new WaitForEndOfFrame();
                firstRenderTimer.Stop();
                manager.UnloadDocument(playbackDocument);

                if (manager.LoadedDocumentCount != baselineDocuments ||
                    manager.OwnedInputBufferCount != baselineBuffers)
                {
                    Debug.LogError(
                        $"{LogPrefix} cycle={cycle} failed=lifecycle-balance " +
                        $"documents={manager.LoadedDocumentCount} expectedDocuments={baselineDocuments} " +
                        $"buffers={manager.OwnedInputBufferCount} expectedBuffers={baselineBuffers}");
                    yield break;
                }

                completed++;
                Debug.Log(
                    $"{LogPrefix} cycle={cycle} revision={export.SourceRevision} " +
                    $"constructionMs={constructionTimer.Elapsed.TotalMilliseconds:F3} " +
                    $"graphCompileMs={export.Statistics.GraphCompilationTime.TotalMilliseconds:F3} " +
                    $"serializationMs={export.Statistics.SerializationTime.TotalMilliseconds:F3} " +
                    $"loadReadyMs={loadTimer.Elapsed.TotalMilliseconds:F3} " +
                    $"firstRenderMs={firstRenderTimer.Elapsed.TotalMilliseconds:F3} " +
                    $"bytes={export.BytesWritten} documents={manager.LoadedDocumentCount} " +
                    $"buffers={manager.OwnedInputBufferCount}");
            }

            totalTimer.Stop();
            long finalManagedBytes = GC.GetTotalMemory(true);
            long finalWorkingSet = Process.GetCurrentProcess().WorkingSet64;
            Debug.Log(
                $"{LogPrefix} passed cycles={completed} frames={frameCount} frameRate={frameRate} " +
                $"elapsedMs={totalTimer.Elapsed.TotalMilliseconds:F3} " +
                $"managedDeltaBytes={finalManagedBytes - initialManagedBytes} " +
                $"workingSetDeltaBytes={finalWorkingSet - initialWorkingSet} " +
                $"documents={manager.LoadedDocumentCount} buffers={manager.OwnedInputBufferCount}");
        }

        private ImmAuthoringResult<ImmAuthoringDocument> BuildAnimation()
        {
            ImmAuthoringResult<ImmAuthoringDocument> create = ImmAuthoringDocument.Create(
                ExportSequenceType.Animated,
                frameRate,
                Color.black);
            if (!create.Succeeded)
                return create;

            ImmAuthoringDocument document = create.Value;
            ImmAuthoringLayerProperties properties = ImmAuthoringLayerProperties.Default("Animated Paint");
            ExportLayerTiming timing = ExportLayerTiming.FromFrames(frameCount, frameRate);
            properties.IsTimeline = timing.IsTimeline;
            properties.DurationTicks = timing.DurationTicks;
            properties.MaxRepeatCount = timing.MaxRepeatCount;
            ImmAuthoringResult<long> layer = document.CreatePaintLayer(0, properties);
            if (!layer.Succeeded)
                return DisposeFailure(document, layer);

            for (int frame = 0; frame < frameCount; frame++)
            {
                ImmAuthoringResult<long> drawing = document.CreateDrawing(layer.Value);
                if (!drawing.Succeeded)
                    return DisposeFailure(document, drawing);

                PaintPoint[] points = new PaintPoint[8];
                for (int pointIndex = 0; pointIndex < points.Length; pointIndex++)
                {
                    float t = pointIndex / (points.Length - 1f);
                    points[pointIndex] = new PaintPoint
                    {
                        Position = new Vector3(
                            t - 0.5f,
                            frame * 0.01f,
                            Mathf.Sin(t * Mathf.PI * 2f + frame * 0.2f) * 0.1f),
                        Normal = Vector3.up,
                        Direction = Vector3.forward,
                        Color = Color.HSVToRGB(frame / (float)frameCount, 0.8f, 1f),
                        Alpha = 1f,
                        Width = 0.02f,
                        Length = t,
                        Time = t
                    };
                }

                ImmAuthoringResult<long> stroke = document.CreateStroke(
                    drawing.Value,
                    BrushSectionType.Circle,
                    VisibilityType.Always,
                    points);
                if (!stroke.Succeeded)
                    return DisposeFailure(document, stroke);

                ImmAuthoringResult frameResult = document.AppendFrame(layer.Value, drawing.Value);
                if (!frameResult.Succeeded)
                    return DisposeFailure(document, frameResult);
            }

            return ImmAuthoringResult<ImmAuthoringDocument>.Success(document);
        }

        private static ImmAuthoringResult<ImmAuthoringDocument> DisposeFailure<T>(
            ImmAuthoringDocument document,
            ImmAuthoringResult<T> failure)
        {
            document.Dispose();
            return ImmAuthoringResult<ImmAuthoringDocument>.Failure(
                failure.ErrorCode,
                failure.Message,
                failure.ObjectId);
        }

        private static ImmAuthoringResult<ImmAuthoringDocument> DisposeFailure(
            ImmAuthoringDocument document,
            ImmAuthoringResult failure)
        {
            document.Dispose();
            return ImmAuthoringResult<ImmAuthoringDocument>.Failure(
                failure.ErrorCode,
                failure.Message,
                failure.ObjectId);
        }
    }
}
