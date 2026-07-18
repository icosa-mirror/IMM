using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using ImmPlayer.Exporter;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace ImmPlayer.Samples
{
    /// <summary>API-focused Phase 0 runtime authoring benchmark.</summary>
    public sealed class ImmAuthoringBenchmark : MonoBehaviour
    {
        private const string LogPrefix = "[IMM_AUTHOR_BENCH_P0]";

        [Serializable]
        public struct BenchmarkCase
        {
            public string Name;
            public int Layers;
            public int StrokesPerLayer;
            public int PointsPerStroke;
            public int Frames;
        }

        [SerializeField] private bool runOnStart;
        [SerializeField] private bool loadWithPlayer = true;
        [SerializeField] private float loadTimeoutSeconds = 30f;
        [SerializeField] private List<BenchmarkCase> cases = new List<BenchmarkCase>
        {
            new BenchmarkCase { Name = "Small", Layers = 1, StrokesPerLayer = 10, PointsPerStroke = 16, Frames = 12 },
            new BenchmarkCase { Name = "Medium", Layers = 4, StrokesPerLayer = 100, PointsPerStroke = 32, Frames = 120 },
            new BenchmarkCase { Name = "Large", Layers = 8, StrokesPerLayer = 500, PointsPerStroke = 64, Frames = 300 }
        };

        private IEnumerator Start()
        {
            if (runOnStart)
                yield return RunBenchmarks();
        }

        [ContextMenu("Run Runtime Authoring Benchmarks")]
        public void RunFromContextMenu() => StartCoroutine(RunBenchmarks());

        public IEnumerator RunBenchmarks()
        {
            foreach (BenchmarkCase benchmarkCase in cases)
                yield return RunCase(benchmarkCase);
        }

        private IEnumerator RunCase(BenchmarkCase benchmarkCase)
        {
            string outputPath = Path.Combine(Application.temporaryCachePath, $"imm-authoring-{benchmarkCase.Name}.imm");
            long managedBefore = GC.GetTotalMemory(true);
            long workingSetBefore = Process.GetCurrentProcess().WorkingSet64;
            Stopwatch constructionTimer = Stopwatch.StartNew();
            ExportSequence sequence = BuildSequence(benchmarkCase);
            constructionTimer.Stop();
            if (sequence == null)
            {
                Debug.LogError($"{LogPrefix} case={benchmarkCase.Name} failed=construction");
                yield break;
            }

            Stopwatch exportTimer = Stopwatch.StartNew();
            bool exported = sequence.ExportToFile(outputPath);
            exportTimer.Stop();
            sequence.Dispose();
            if (!exported)
            {
                Debug.LogError($"{LogPrefix} case={benchmarkCase.Name} failed=export");
                yield break;
            }

            long outputBytes = new FileInfo(outputPath).Length;
            long managedAfterExport = GC.GetTotalMemory(false);
            long workingSetAfterExport = Process.GetCurrentProcess().WorkingSet64;
            double loadMilliseconds = -1;
            double firstFrameMilliseconds = -1;

            if (loadWithPlayer)
            {
                ImmPlayerManager manager = ImmPlayerManager.Instance;
                manager.Initialize();
                Stopwatch loadTimer = Stopwatch.StartNew();
                ImmDocument document = manager.LoadDocument(outputPath);
                if (document == null)
                {
                    Debug.LogError($"{LogPrefix} case={benchmarkCase.Name} failed=player-load-start");
                    yield break;
                }

                float deadline = Time.realtimeSinceStartup + loadTimeoutSeconds;
                while (!document.IsSequenceReady() && Time.realtimeSinceStartup < deadline)
                    yield return null;
                loadTimer.Stop();

                if (!document.IsSequenceReady())
                {
                    manager.UnloadDocument(document);
                    Debug.LogError($"{LogPrefix} case={benchmarkCase.Name} failed=player-load-timeout timeoutSec={loadTimeoutSeconds:F1}");
                    yield break;
                }

                loadMilliseconds = loadTimer.Elapsed.TotalMilliseconds;
                Stopwatch firstFrameTimer = Stopwatch.StartNew();
                yield return new WaitForEndOfFrame();
                firstFrameTimer.Stop();
                firstFrameMilliseconds = firstFrameTimer.Elapsed.TotalMilliseconds;
                manager.UnloadDocument(document);
            }

            Debug.Log(
                $"{LogPrefix} case={benchmarkCase.Name} layers={benchmarkCase.Layers} " +
                $"strokesPerLayer={benchmarkCase.StrokesPerLayer} pointsPerStroke={benchmarkCase.PointsPerStroke} " +
                $"frames={benchmarkCase.Frames} constructionMs={constructionTimer.Elapsed.TotalMilliseconds:F3} " +
                $"exportMs={exportTimer.Elapsed.TotalMilliseconds:F3} loadReadyMs={loadMilliseconds:F3} " +
                $"firstFrameMs={firstFrameMilliseconds:F3} outputBytes={outputBytes} " +
                $"managedDeltaBytes={managedAfterExport - managedBefore} " +
                $"workingSetDeltaBytes={workingSetAfterExport - workingSetBefore}");
        }

        private static ExportSequence BuildSequence(BenchmarkCase benchmarkCase)
        {
            ExportSequence sequence = ExportSequence.Create(ExportSequenceType.Animated, 30, Color.black, new ExportRequirements());
            if (sequence == null)
                return null;

            for (int layerIndex = 0; layerIndex < benchmarkCase.Layers; layerIndex++)
            {
                ExportPaintLayer layer = sequence.CreatePaintLayer($"Paint {layerIndex}");
                if (layer == null)
                {
                    sequence.Dispose();
                    return null;
                }

                uint[] drawingIndices = new uint[benchmarkCase.StrokesPerLayer];
                for (int strokeIndex = 0; strokeIndex < benchmarkCase.StrokesPerLayer; strokeIndex++)
                {
                    using (ExportDrawing drawing = layer.CreateDrawing())
                    {
                        if (drawing == null || !drawing.Init(1))
                        {
                            sequence.Dispose();
                            return null;
                        }

                        ExportElement element = drawing.GetElement(0);
                        if (element == null || !element.Init((uint)benchmarkCase.PointsPerStroke, BrushSectionType.Circle, VisibilityType.Always))
                        {
                            sequence.Dispose();
                            return null;
                        }

                        for (uint pointIndex = 0; pointIndex < (uint)benchmarkCase.PointsPerStroke; pointIndex++)
                        {
                            float t = benchmarkCase.PointsPerStroke <= 1 ? 0f : pointIndex / (benchmarkCase.PointsPerStroke - 1f);
                            if (!element.SetPoint(pointIndex, MakePoint(layerIndex, strokeIndex, t)))
                            {
                                sequence.Dispose();
                                return null;
                            }
                        }

                        element.ComputeBounds();
                        drawing.ComputeBounds();
                        drawingIndices[strokeIndex] = drawing.DrawingIndex;
                    }
                }

                for (int frame = 0; frame < benchmarkCase.Frames; frame++)
                    layer.AddFrame(drawingIndices[frame % drawingIndices.Length]);
            }

            return sequence;
        }

        private static PaintPoint MakePoint(int layerIndex, int strokeIndex, float t)
        {
            return new PaintPoint
            {
                Position = new Vector3(t, layerIndex * 0.05f, strokeIndex * 0.002f),
                Normal = Vector3.up,
                Direction = Vector3.forward,
                Color = Color.HSVToRGB((strokeIndex % 32) / 32f, 0.8f, 1f),
                Alpha = 1f,
                Width = 0.01f,
                Length = t,
                Time = t
            };
        }
    }
}
