using System.Collections;
using System.Diagnostics;
using System.IO;
using ImmPlayer.Exporter;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace ImmPlayer.Samples
{
    /// <summary>
    /// Phase 1 vertical slice: construct, serialize, load, seek, play, and unload
    /// a runtime-generated multi-frame IMM paint animation.
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

            string outputPath = Path.Combine(Application.temporaryCachePath, "imm-runtime-animation-spike.imm");
            long initialWorkingSet = Process.GetCurrentProcess().WorkingSet64;
            Stopwatch totalTimer = Stopwatch.StartNew();
            int completed = 0;

            for (int cycle = 0; cycle < cycles; cycle++)
            {
                ExportSequence sequence = BuildAnimation();
                if (sequence == null)
                {
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=construction");
                    yield break;
                }

                Stopwatch exportTimer = Stopwatch.StartNew();
                bool exported = sequence.ExportToFile(outputPath);
                exportTimer.Stop();
                sequence.Dispose();
                if (!exported)
                {
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=export");
                    yield break;
                }

                ImmPlayerManager manager = ImmPlayerManager.Instance;
                manager.Initialize();
                Stopwatch loadTimer = Stopwatch.StartNew();
                ImmDocument document = manager.LoadDocument(outputPath);
                if (document == null)
                {
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=load-start");
                    yield break;
                }

                float deadline = Time.realtimeSinceStartup + loadTimeoutSeconds;
                while (!document.IsSequenceReady() && Time.realtimeSinceStartup < deadline)
                    yield return null;
                loadTimer.Stop();

                if (!document.IsSequenceReady())
                {
                    manager.UnloadDocument(document);
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=load-timeout");
                    yield break;
                }

                ImmDocument.LayerInfo[] layers = document.GetLayersManaged();
                bool foundExpectedFrames = false;
                foreach (ImmDocument.LayerInfo layer in layers)
                {
                    if (layer.Type == ImmDocument.LayerType.Paint && layer.PaintNumFrames == frameCount)
                    {
                        foundExpectedFrames = true;
                        break;
                    }
                }

                if (!foundExpectedFrames)
                {
                    manager.UnloadDocument(document);
                    Debug.LogError($"{LogPrefix} cycle={cycle} failed=frame-count expected={frameCount}");
                    yield break;
                }

                long seekTicks = ExportLayerTiming.FromFrames(frameCount / 2, frameRate).DurationTicks;
                document.Pause();
                document.SetTime(seekTicks, 0);
                document.Resume();
                yield return new WaitForEndOfFrame();
                manager.UnloadDocument(document);
                completed++;

                Debug.Log(
                    $"{LogPrefix} cycle={cycle} exportMs={exportTimer.Elapsed.TotalMilliseconds:F3} " +
                    $"loadReadyMs={loadTimer.Elapsed.TotalMilliseconds:F3} bytes={new FileInfo(outputPath).Length}");
            }

            totalTimer.Stop();
            long finalWorkingSet = Process.GetCurrentProcess().WorkingSet64;
            Debug.Log(
                $"{LogPrefix} passed cycles={completed} frames={frameCount} frameRate={frameRate} " +
                $"elapsedMs={totalTimer.Elapsed.TotalMilliseconds:F3} " +
                $"workingSetDeltaBytes={finalWorkingSet - initialWorkingSet}");
        }

        private ExportSequence BuildAnimation()
        {
            ExportSequence sequence = ExportSequence.Create(
                ExportSequenceType.Animated,
                frameRate,
                Color.black,
                new ExportRequirements());
            if (sequence == null)
                return null;

            ExportPaintLayer paintLayer = sequence.CreatePaintLayer(
                "Animated Paint",
                timing: ExportLayerTiming.FromFrames(frameCount, frameRate, 0));
            if (paintLayer == null)
            {
                sequence.Dispose();
                return null;
            }

            for (int frame = 0; frame < frameCount; frame++)
            {
                using (ExportDrawing drawing = paintLayer.CreateDrawing())
                {
                    if (drawing == null || !drawing.Init(1))
                    {
                        sequence.Dispose();
                        return null;
                    }

                    ExportElement element = drawing.GetElement(0);
                    if (element == null || !element.Init(8, BrushSectionType.Circle, VisibilityType.Always))
                    {
                        sequence.Dispose();
                        return null;
                    }

                    for (uint pointIndex = 0; pointIndex < 8; pointIndex++)
                    {
                        float t = pointIndex / 7f;
                        PaintPoint point = new PaintPoint
                        {
                            Position = new Vector3(t - 0.5f, frame * 0.01f, Mathf.Sin(t * Mathf.PI * 2f + frame * 0.2f) * 0.1f),
                            Normal = Vector3.up,
                            Direction = Vector3.forward,
                            Color = Color.HSVToRGB(frame / (float)frameCount, 0.8f, 1f),
                            Alpha = 1f,
                            Width = 0.02f,
                            Length = t,
                            Time = t
                        };
                        if (!element.SetPoint(pointIndex, point))
                        {
                            sequence.Dispose();
                            return null;
                        }
                    }

                    element.ComputeBounds();
                    drawing.ComputeBounds();
                    paintLayer.AddFrame(drawing.DrawingIndex);
                }
            }

            return sequence;
        }
    }
}
