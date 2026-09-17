using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace ImmPlayer.Tests
{
    public sealed class ImmAuthoringProductionTests
    {
        private const long MaxSoakRetainedManagedBytes = 16L * 1024L * 1024L;

        [Test]
        public void CapabilitiesDescribeTheRunningPlatform()
        {
            ImmAuthoringCapabilities capabilities = ImmAuthoringRuntime.Capabilities;

            Assert.That(capabilities.Platform, Is.Not.Empty);
            Assert.That(capabilities.Architecture, Is.Not.Empty);
#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
            Assert.That(capabilities.IsAuthoringSupported, Is.EqualTo(IntPtr.Size == 8));
            Assert.That(capabilities.Supports(ImmAuthoringFeature.Playback), Is.True);
            Assert.That(capabilities.Supports(ImmAuthoringFeature.ProgressReporting), Is.EqualTo(IntPtr.Size == 8));
#endif
        }

        [Test]
        public void IndependentApplicationAssemblyConsumesRuntimeWithoutSamples()
        {
            Type consumer = Type.GetType(
                "ImmPackageConsumer.ImmPackageConsumerSmoke, ImmPhase6PackageConsumer",
                throwOnError: true);
            string description = (string)consumer.GetMethod(
                "DescribeRuntime",
                BindingFlags.Public | BindingFlags.Static)?.Invoke(null, null);
            object result = consumer.GetMethod(
                "CreateAndDisposeDocument",
                BindingFlags.Public | BindingFlags.Static)?.Invoke(null, null);

            Assert.That(description, Does.Contain(ImmAuthoringRuntime.Capabilities.Platform));
            Assert.That(result, Is.TypeOf<ImmAuthoringResult>());
            Assert.That(((ImmAuthoringResult)result).Succeeded, Is.True);
        }

#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
        [Test]
        public void ExportReportsProgressAndHonoursOutputLimit()
        {
            using (ImmAuthoringDocument document = CreateFixture())
            {
                ProgressRecorder recorder = new ProgressRecorder();
                ImmAuthoringOperationOptions options = new ImmAuthoringOperationOptions(progress: recorder);
                ImmAuthoringExportResult success = ImmAuthoringCompiler.ExportToMemory(document, options);

                Assert.That(success.Succeeded, Is.True, success.Message);
                Assert.That(recorder.Items.Any(item => item.Stage == ImmAuthoringProgressStage.Validating), Is.True);
                Assert.That(recorder.Items.Any(item => item.Stage == ImmAuthoringProgressStage.CompilingGraph), Is.True);
                Assert.That(recorder.Items.Any(item => item.Stage == ImmAuthoringProgressStage.Serializing), Is.True);
                Assert.That(recorder.Items.Last().Stage, Is.EqualTo(ImmAuthoringProgressStage.Completed));
                Assert.That(recorder.Items.All(item => item.Fraction >= 0f && item.Fraction <= 1f), Is.True);

                string filePath = Path.Combine(Application.temporaryCachePath, $"imm-phase6-progress-{Guid.NewGuid():N}.imm");
                try
                {
                    ProgressRecorder fileRecorder = new ProgressRecorder();
                    ImmAuthoringExportResult file = ImmAuthoringCompiler.ExportToFile(
                        document,
                        filePath,
                        new ImmAuthoringOperationOptions(progress: fileRecorder));
                    Assert.That(file.Succeeded, Is.True, file.Message);
                    Assert.That(fileRecorder.Items.Any(item => item.Stage == ImmAuthoringProgressStage.WritingOutput), Is.True);
                    Assert.That(fileRecorder.Items.Last().Stage, Is.EqualTo(ImmAuthoringProgressStage.Completed));
                }
                finally
                {
                    if (File.Exists(filePath))
                        File.Delete(filePath);
                }

                ImmAuthoringLimits tinyOutput = Limits(maxOutputBytes: 1);
                ImmAuthoringExportResult limited = ImmAuthoringCompiler.ExportToMemory(
                    document,
                    new ImmAuthoringOperationOptions(limits: tinyOutput));
                Assert.That(limited.Succeeded, Is.False);
                Assert.That(limited.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.ResourceLimitExceeded));
                Assert.That(limited.Data, Is.Null);

                ImmAuthoringExportResult pointLimited = ImmAuthoringCompiler.ExportToMemory(
                    document,
                    new ImmAuthoringOperationOptions(limits: Limits(maxTotalPoints: 1)));
                Assert.That(pointLimited.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.ResourceLimitExceeded));
                Assert.That(pointLimited.Message, Does.Contain("total points"));
                Assert.That(document.Validate().Succeeded, Is.True);
            }
        }

        [Test]
        public void ExportCancellationIsControlledAndAtomicFileExportPreservesDestination()
        {
            string path = Path.Combine(Application.temporaryCachePath, $"imm-phase6-atomic-{Guid.NewGuid():N}.imm");
            byte[] original = { 7, 6, 5, 4 };
            File.WriteAllBytes(path, original);
            try
            {
                using (ImmAuthoringDocument document = CreateFixture())
                using (CancellationTokenSource cancellation = new CancellationTokenSource())
                {
                    cancellation.Cancel();
                    ImmAuthoringOperationOptions options = new ImmAuthoringOperationOptions(cancellation.Token);
                    ImmAuthoringExportResult memory = ImmAuthoringCompiler.ExportToMemory(document, options);
                    ImmAuthoringExportResult file = ImmAuthoringCompiler.ExportToFile(document, path, options);

                    Assert.That(memory.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                    Assert.That(memory.Data, Is.Null);
                    Assert.That(file.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                    Assert.That(File.ReadAllBytes(path), Is.EqualTo(original));
                    Assert.That(Directory.GetFiles(Application.temporaryCachePath, $".{Path.GetFileName(path)}.*.tmp"), Is.Empty);
                }
            }
            finally
            {
                if (File.Exists(path))
                    File.Delete(path);
            }
        }

        [Test]
        public void ProgressCallbackCanCancelCompilationAndImportAtSafeCheckpoints()
        {
            using (ImmAuthoringDocument document = CreateFixture())
            using (CancellationTokenSource compileCancellation = new CancellationTokenSource())
            {
                CancellingProgress compileProgress = new CancellingProgress(
                    compileCancellation,
                    ImmAuthoringProgressStage.CompilingGraph);
                ImmAuthoringExportResult cancelledExport = ImmAuthoringCompiler.ExportToMemory(
                    document,
                    new ImmAuthoringOperationOptions(compileCancellation.Token, compileProgress));
                Assert.That(cancelledExport.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                Assert.That(cancelledExport.Data, Is.Null);

                ImmAuthoringExportResult valid = ImmAuthoringCompiler.ExportToMemory(document);
                Assert.That(valid.Succeeded, Is.True, valid.Message);
                using (CancellationTokenSource importCancellation = new CancellationTokenSource())
                {
                    CancellingProgress importProgress = new CancellingProgress(
                        importCancellation,
                        ImmAuthoringProgressStage.InspectingSource);
                    ImmAuthoringImportResult cancelledImport = ImmAuthoringImporter.ImportFromMemory(
                        valid.Data,
                        new ImmAuthoringOperationOptions(importCancellation.Token, importProgress));
                    Assert.That(cancelledImport.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                    Assert.That(cancelledImport.Document, Is.Null);
                }
            }
        }

        [Test]
        public void PlanningEnvelopeCancellationRetryAndImportReleaseManagedMemory()
        {
            const int drawingCount = 500;
            const int strokesPerDrawing = 8;
            const int pointsPerStroke = 64;
            long baseline = GC.GetTotalMemory(true);
            using (ImmAuthoringDocument document = CreateLargeFixture(drawingCount, strokesPerDrawing, pointsPerStroke))
            using (CancellationTokenSource cancellation = new CancellationTokenSource())
            {
                ThresholdCancellingProgress progress = new ThresholdCancellingProgress(cancellation, 100);
                ImmAuthoringExportResult cancelled = ImmAuthoringCompiler.ExportToMemory(
                    document,
                    new ImmAuthoringOperationOptions(cancellation.Token, progress));

                Assert.That(progress.HighestCompletedUnits, Is.GreaterThanOrEqualTo(100));
                Assert.That(cancelled.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                Assert.That(cancelled.Data, Is.Null);
                Assert.That(document.Validate().Succeeded, Is.True);

                ImmAuthoringExportResult retry = ImmAuthoringCompiler.ExportToMemory(document);
                Assert.That(retry.Succeeded, Is.True, retry.Message);
                ImmAuthoringImportResult import = ImmAuthoringImporter.ImportFromMemory(retry.Data);
                try
                {
                    Assert.That(import.Succeeded, Is.True, import.Message);
                    Assert.That(import.Statistics.ImportedDrawingCount, Is.EqualTo(drawingCount));
                    Assert.That(import.Statistics.ImportedStrokeCount, Is.EqualTo(drawingCount * strokesPerDrawing));
                    Assert.That(import.Statistics.ImportedPointCount, Is.EqualTo((long)drawingCount * strokesPerDrawing * pointsPerStroke));
                }
                finally
                {
                    import.Document?.Dispose();
                }
            }

            long retained = GC.GetTotalMemory(true) - baseline;
            Assert.That(retained, Is.LessThanOrEqualTo(64L * 1024L * 1024L));
        }

        [Test]
        public void MalformedAndLimitedImportsFailWithoutPoisoningRecovery()
        {
            byte[] malformedBytes = { 0x49, 0x4d, 0x4d, 0x00 };
            ImmAuthoringImportResult malformed = ImmAuthoringImporter.ImportFromMemory(malformedBytes);
            AssertControlledCorruptInput(malformed);

            string malformedPath = Path.Combine(Application.temporaryCachePath, $"imm-phase6-corrupt-{Guid.NewGuid():N}.imm");
            File.WriteAllBytes(malformedPath, malformedBytes);
            try
            {
                LogAssert.Expect(LogType.Error, new Regex("StrokeReaderDocument: Failed to load.*"));
                AssertControlledCorruptInput(ImmAuthoringImporter.ImportFromFile(malformedPath));
            }
            finally
            {
                if (File.Exists(malformedPath))
                    File.Delete(malformedPath);
            }

            using (ImmAuthoringDocument source = CreateFixture())
            {
                ImmAuthoringExportResult export = ImmAuthoringCompiler.ExportToMemory(source);
                Assert.That(export.Succeeded, Is.True, export.Message);

                ImmAuthoringImportResult limited = ImmAuthoringImporter.ImportFromMemory(
                    export.Data,
                    new ImmAuthoringOperationOptions(limits: Limits(maxInputBytes: export.BytesWritten - 1)));
                Assert.That(limited.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.ResourceLimitExceeded));
                Assert.That(limited.Document, Is.Null);

                using (CancellationTokenSource cancellation = new CancellationTokenSource())
                {
                    cancellation.Cancel();
                    ImmAuthoringImportResult cancelled = ImmAuthoringImporter.ImportFromMemory(
                        export.Data,
                        new ImmAuthoringOperationOptions(cancellation.Token));
                    Assert.That(cancelled.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                    Assert.That(cancelled.Document, Is.Null);
                }

                ImmAuthoringImportResult recovered = ImmAuthoringImporter.ImportFromMemory(export.Data);
                try
                {
                    Assert.That(recovered.Succeeded, Is.True, recovered.Message);
                    Assert.That(recovered.Document.Validate().Succeeded, Is.True);
                }
                finally
                {
                    recovered.Document?.Dispose();
                }
            }
        }

        [Test]
        public void HundredEditExportImportCyclesStayWithinManagedMemoryAllowance()
        {
            long baseline = GC.GetTotalMemory(true);
            using (ImmAuthoringDocument document = CreateFixture())
            {
                long strokeId = Require(document.CreateSnapshot()).Layers[0].Drawings[0].Strokes[0].Id;
                for (int iteration = 0; iteration < 100; iteration++)
                {
                    Require(document.ReplaceStroke(
                        strokeId,
                        BrushSectionType.Circle,
                        VisibilityType.Always,
                        Points(iteration * 0.001f)));
                    ImmAuthoringExportResult export = ImmAuthoringCompiler.ExportToMemory(document);
                    Assert.That(export.Succeeded, Is.True, $"Iteration {iteration}: {export.Message}");
                    ImmAuthoringImportResult import = ImmAuthoringImporter.ImportFromMemory(export.Data);
                    try
                    {
                        Assert.That(import.Succeeded, Is.True, $"Iteration {iteration}: {import.Message}");
                    }
                    finally
                    {
                        import.Document?.Dispose();
                    }
                }
            }

            long retained = GC.GetTotalMemory(true) - baseline;
            Assert.That(retained, Is.LessThanOrEqualTo(MaxSoakRetainedManagedBytes));
        }
#endif

        private static ImmAuthoringDocument CreateFixture()
        {
            ImmAuthoringDocument document = Require(ImmAuthoringDocument.Create(
                ExportSequenceType.Animated,
                30,
                Color.black));
            long paint = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Phase 6 Paint")));
            long drawing = Require(document.CreateDrawing(paint));
            Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, Points(0f)));
            Require(document.AppendFrame(paint, drawing));
            return document;
        }

        private static ImmAuthoringDocument CreateLargeFixture(int drawingCount, int strokesPerDrawing, int pointsPerStroke)
        {
            ImmAuthoringDocument document = Require(ImmAuthoringDocument.Create(
                ExportSequenceType.Animated,
                30,
                Color.black));
            long paint = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Cancellation Paint")));
            for (int drawingIndex = 0; drawingIndex < drawingCount; drawingIndex++)
            {
                long drawing = Require(document.CreateDrawing(paint));
                for (int strokeIndex = 0; strokeIndex < strokesPerDrawing; strokeIndex++)
                {
                    PaintPoint[] points = new PaintPoint[pointsPerStroke];
                    for (int pointIndex = 0; pointIndex < points.Length; pointIndex++)
                    {
                        points[pointIndex] = Point(
                            new Vector3(pointIndex * 0.01f, strokeIndex * 0.01f, drawingIndex * 0.001f),
                            Color.Lerp(Color.red, Color.blue, pointIndex / (points.Length - 1f)));
                    }
                    Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, points));
                }
                Require(document.AppendFrame(paint, drawing));
            }
            return document;
        }

        private static PaintPoint[] Points(float offset)
        {
            return new[]
            {
                Point(new Vector3(offset, 0f, 0f), Color.red),
                Point(new Vector3(offset + 0.5f, 0.25f, 0f), Color.blue)
            };
        }

        private static PaintPoint Point(Vector3 position, Color color)
        {
            return new PaintPoint
            {
                Position = position,
                Normal = Vector3.forward,
                Direction = Vector3.zero,
                Color = color,
                Alpha = 1f,
                Width = 0.05f
            };
        }

        private static ImmAuthoringLimits Limits(
            long maxInputBytes = 256L * ImmAuthoringLimits.MiB,
            long maxOutputBytes = 256L * ImmAuthoringLimits.MiB,
            long maxTotalPoints = 16000000)
        {
            return new ImmAuthoringLimits(
                maxInputBytes: maxInputBytes,
                maxOutputBytes: maxOutputBytes,
                maxTotalPoints: maxTotalPoints);
        }

        private static void AssertControlledCorruptInput(ImmAuthoringImportResult result)
        {
            Assert.That(result.Succeeded, Is.False);
            Assert.That(result.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.CorruptInput));
            Assert.That(result.Document, Is.Null);
        }

        private static T Require<T>(ImmAuthoringResult<T> result)
        {
            Assert.That(result.Succeeded, Is.True, result.Message);
            return result.Value;
        }

        private static void Require(ImmAuthoringResult result)
        {
            Assert.That(result.Succeeded, Is.True, result.Message);
        }

        private sealed class ProgressRecorder : IProgress<ImmAuthoringProgress>
        {
            internal List<ImmAuthoringProgress> Items { get; } = new List<ImmAuthoringProgress>();
            public void Report(ImmAuthoringProgress value) => Items.Add(value);
        }

        private sealed class CancellingProgress : IProgress<ImmAuthoringProgress>
        {
            private readonly CancellationTokenSource _cancellation;
            private readonly ImmAuthoringProgressStage _stage;

            internal CancellingProgress(CancellationTokenSource cancellation, ImmAuthoringProgressStage stage)
            {
                _cancellation = cancellation;
                _stage = stage;
            }

            public void Report(ImmAuthoringProgress value)
            {
                if (value.Stage == _stage)
                    _cancellation.Cancel();
            }
        }

        private sealed class ThresholdCancellingProgress : IProgress<ImmAuthoringProgress>
        {
            private readonly CancellationTokenSource _cancellation;
            private readonly long _threshold;

            internal long HighestCompletedUnits { get; private set; }

            internal ThresholdCancellingProgress(CancellationTokenSource cancellation, long threshold)
            {
                _cancellation = cancellation;
                _threshold = threshold;
            }

            public void Report(ImmAuthoringProgress value)
            {
                if (value.Stage != ImmAuthoringProgressStage.CompilingGraph)
                    return;
                HighestCompletedUnits = Math.Max(HighestCompletedUnits, value.CompletedUnits);
                if (HighestCompletedUnits >= _threshold)
                    _cancellation.Cancel();
            }
        }
    }
}
