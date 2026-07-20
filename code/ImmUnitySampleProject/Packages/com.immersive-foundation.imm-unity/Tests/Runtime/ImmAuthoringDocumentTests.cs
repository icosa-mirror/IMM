using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace ImmPlayer.Tests
{
    public sealed class ImmAuthoringDocumentTests
    {
        private const long MaxRetainedManagedBytes = 8L * 1024L * 1024L;
        private const long MaxRetainedWorkingSetBytes = 64L * 1024L * 1024L;
        [Test]
        public void LayerHierarchyUsesStableIdsAndRejectsCycles()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long group = Require(document.CreateGroupLayer(0, ImmAuthoringLayerProperties.Default("Group")));
                long child = Require(document.CreateGroupLayer(group, ImmAuthoringLayerProperties.Default("Child")));
                long paint = Require(document.CreatePaintLayer(group, ImmAuthoringLayerProperties.Default("Paint")));

                Assert.That(group, Is.Not.EqualTo(child));
                Assert.That(child, Is.Not.EqualTo(paint));
                Assert.That(document.Revision, Is.EqualTo(3));

                ImmAuthoringResult cycle = document.ReparentLayer(group, child);
                Assert.That(cycle.Succeeded, Is.False);
                Assert.That(cycle.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.HierarchyCycle));
                Assert.That(document.Revision, Is.EqualTo(3));

                Require(document.ReparentLayer(paint, 0, 0));
                ImmAuthoringSnapshot snapshot = Require(document.CreateSnapshot());
                Assert.That(snapshot.RootLayerIds, Is.EqualTo(new[] { paint, group }));
                Assert.That(snapshot.TryGetLayer(paint, out ImmAuthoringLayerSnapshot paintLayer), Is.True);
                Assert.That(paintLayer.ParentId, Is.Zero);
            }
        }

        [Test]
        public void FramesOnlyReferenceDrawingsFromTheirPaintLayer()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long firstLayer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("First")));
                long secondLayer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Second")));
                long firstDrawing = Require(document.CreateDrawing(firstLayer));
                long secondDrawing = Require(document.CreateDrawing(secondLayer));

                Require(document.AppendFrame(firstLayer, firstDrawing));
                ImmAuthoringResult wrongOwner = document.AppendFrame(firstLayer, secondDrawing);
                Assert.That(wrongOwner.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.InvalidOwner));

                ImmAuthoringResult referencedRemoval = document.RemoveDrawing(firstDrawing);
                Assert.That(referencedRemoval.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.DanglingReference));
                Require(document.RemoveFrame(firstLayer, 0));
                Require(document.RemoveDrawing(firstDrawing));
            }
        }

        [Test]
        public void StrokeBuffersAreCopiedAndSnapshotsAreImmutable()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long layer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                long drawing = Require(document.CreateDrawing(layer));
                PaintPoint[] source = { Point(0f), Point(1f), Point(2f) };
                long stroke = Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, source));
                source[0].Position = Vector3.one * 99f;

                ImmAuthoringSnapshot first = Require(document.CreateSnapshot());
                ImmAuthoringStrokeSnapshot firstStroke = first.Layers[0].Drawings[0].Strokes[0];
                Assert.That(firstStroke.Id, Is.EqualTo(stroke));
                Assert.That(firstStroke.Points[0].Position, Is.EqualTo(Vector3.zero));

                Require(document.ReplaceStroke(
                    stroke,
                    BrushSectionType.Square,
                    VisibilityType.FadePow2,
                    new[] { Point(2f), Point(3f), Point(4f) }));
                ImmAuthoringSnapshot second = Require(document.CreateSnapshot());
                Assert.That(firstStroke.Points.Count, Is.EqualTo(3));
                Assert.That(second.Layers[0].Drawings[0].Strokes[0].Points.Count, Is.EqualTo(3));
            }
        }

        [Test]
        public void SuccessfulMutationPublishesOneRevisionedChange()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                List<ImmAuthoringChange> changes = new List<ImmAuthoringChange>();
                document.Changed += changes.Add;
                ImmAuthoringLayerProperties properties = ImmAuthoringLayerProperties.Default("Paint");
                properties.IsTimeline = true;
                properties.DurationTicks = ExportLayerTiming.FromFrames(1, 30).DurationTicks;
                long layer = Require(document.CreatePaintLayer(0, properties));

                Assert.That(changes, Has.Count.EqualTo(1));
                Assert.That(changes[0].DocumentId, Is.EqualTo(document.DocumentId));
                Assert.That(changes[0].Revision, Is.EqualTo(1));
                Assert.That(changes[0].AffectedObjectIds, Does.Contain(layer));

                ImmAuthoringResult failure = document.SetLayerProperties(99999, ImmAuthoringLayerProperties.Default("Missing"));
                Assert.That(failure.Succeeded, Is.False);
                Assert.That(document.Revision, Is.EqualTo(1));
                Assert.That(changes, Has.Count.EqualTo(1));
            }
        }

        [Test]
        public void ValidationRejectsInvalidDataWithoutAdvancingRevision()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                ImmAuthoringLayerProperties invalid = ImmAuthoringLayerProperties.Default("Invalid");
                invalid.Opacity = 2f;
                ImmAuthoringResult<long> layer = document.CreatePaintLayer(0, invalid);
                Assert.That(layer.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.InvalidArgument));
                Assert.That(document.Revision, Is.Zero);

                long validLayer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                long drawing = Require(document.CreateDrawing(validLayer));
                PaintPoint invalidPoint = Point(0f);
                invalidPoint.Width = 0f;
                ImmAuthoringResult<long> stroke = document.CreateStroke(
                    drawing,
                    BrushSectionType.Circle,
                    VisibilityType.Always,
                    new[] { invalidPoint });
                Assert.That(stroke.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.InvalidArgument));
                Assert.That(document.Validate().Succeeded, Is.True);
            }
        }

        [Test]
        public void TransactionCommitsAtomicallyWithOneRevisionAndNotification()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            using (ImmAuthoringTransaction transaction = Require(document.BeginEdit(document.Revision)))
            {
                List<ImmAuthoringChange> changes = new List<ImmAuthoringChange>();
                document.Changed += changes.Add;
                long group = Require(transaction.EditableDocument.CreateGroupLayer(
                    0,
                    ImmAuthoringLayerProperties.Default("Group")));
                Require(transaction.EditableDocument.CreatePaintLayer(
                    group,
                    ImmAuthoringLayerProperties.Default("Paint")));

                Assert.That(Require(document.CreateSnapshot()).Layers, Is.Empty);
                long committedRevision = Require(transaction.Commit());
                Assert.That(committedRevision, Is.EqualTo(1));
                Assert.That(document.Revision, Is.EqualTo(1));
                Assert.That(Require(document.CreateSnapshot()).Layers, Has.Count.EqualTo(2));
                Assert.That(changes, Has.Count.EqualTo(1));
                Assert.That(changes[0].AffectedObjectIds, Does.Contain(group));
            }
        }

        [Test]
        public void StaleOrAbortedTransactionDoesNotChangeDocument()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                using (ImmAuthoringTransaction first = Require(document.BeginEdit(0)))
                using (ImmAuthoringTransaction stale = Require(document.BeginEdit(0)))
                {
                    Require(first.EditableDocument.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("First")));
                    Require(stale.EditableDocument.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Stale")));
                    Require(first.Commit());

                    ImmAuthoringResult<long> staleResult = stale.Commit();
                    Assert.That(staleResult.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.RevisionConflict));
                    Assert.That(Require(document.CreateSnapshot()).Layers, Has.Count.EqualTo(1));
                }

                using (ImmAuthoringTransaction aborted = Require(document.BeginEdit(document.Revision)))
                {
                    Require(aborted.EditableDocument.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Aborted")));
                    aborted.Abort();
                }
                Assert.That(document.Revision, Is.EqualTo(1));
                Assert.That(Require(document.CreateSnapshot()).Layers, Has.Count.EqualTo(1));
            }
        }

        [Test]
        public void SnapshotLooksUpDrawingsAndStrokesByStableIdWithOwnership()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long layer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                long drawing = Require(document.CreateDrawing(layer));
                long stroke = Require(document.CreateStroke(
                    drawing,
                    BrushSectionType.Circle,
                    VisibilityType.Always,
                    new[] { Point(0f), Point(1f), Point(2f) }));

                ImmAuthoringSnapshot beforeReorder = Require(document.CreateSnapshot());
                Assert.That(beforeReorder.TryGetDrawing(drawing, out ImmAuthoringDrawingSnapshot drawingSnapshot), Is.True);
                Assert.That(drawingSnapshot.PaintLayerId, Is.EqualTo(layer));
                Assert.That(beforeReorder.TryGetStroke(stroke, out ImmAuthoringStrokeSnapshot strokeSnapshot), Is.True);
                Assert.That(strokeSnapshot.DrawingId, Is.EqualTo(drawing));

                long unrelated = Require(document.CreateGroupLayer(
                    0,
                    ImmAuthoringLayerProperties.Default("Unrelated"),
                    0));
                Require(document.ReparentLayer(layer, unrelated));
                ImmAuthoringSnapshot afterReorder = Require(document.CreateSnapshot());
                Assert.That(afterReorder.TryGetDrawing(drawing, out drawingSnapshot), Is.True);
                Assert.That(afterReorder.TryGetStroke(stroke, out strokeSnapshot), Is.True);
                Assert.That(drawingSnapshot.Id, Is.EqualTo(drawing));
                Assert.That(strokeSnapshot.Id, Is.EqualTo(stroke));
            }
        }

        [Test]
        public void FrameSequenceResizeGrowsAndShrinksWithoutDanglingReferences()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long layer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                long first = Require(document.CreateDrawing(layer));
                long fill = Require(document.CreateDrawing(layer));
                Require(document.AppendFrame(layer, first));

                Require(document.ResizeFrameSequence(layer, 4, fill));
                ImmAuthoringLayerSnapshot grown = Require(document.CreateSnapshot()).Layers[0];
                Assert.That(grown.FrameDrawingIds, Is.EqualTo(new[] { first, fill, fill, fill }));

                long revisionBeforeNoOp = document.Revision;
                Require(document.ResizeFrameSequence(layer, 4, 0));
                Assert.That(document.Revision, Is.EqualTo(revisionBeforeNoOp));

                Require(document.ResizeFrameSequence(layer, 2));
                ImmAuthoringLayerSnapshot shrunk = Require(document.CreateSnapshot()).Layers[0];
                Assert.That(shrunk.FrameDrawingIds, Is.EqualTo(new[] { first, fill }));
                Assert.That(document.Validate().Succeeded, Is.True);

                long otherLayer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Other")));
                long wrongOwner = Require(document.CreateDrawing(otherLayer));
                long revisionBeforeFailure = document.Revision;
                ImmAuthoringResult failure = document.ResizeFrameSequence(layer, 3, wrongOwner);
                Assert.That(failure.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.InvalidOwner));
                Assert.That(document.Revision, Is.EqualTo(revisionBeforeFailure));
            }
        }

        [Test]
        public void FrameMappingsKeepStableIdsAcrossInsertionMoveReplacementAndResize()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long layer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                long firstDrawing = Require(document.CreateDrawing(layer));
                long secondDrawing = Require(document.CreateDrawing(layer));
                long insertedDrawing = Require(document.CreateDrawing(layer));
                long firstFrame = Require(document.AppendFrameWithId(layer, firstDrawing));
                long secondFrame = Require(document.AppendFrameWithId(layer, secondDrawing));
                long insertedFrame = Require(document.InsertFrameWithId(layer, 1, insertedDrawing));

                Require(document.MoveFrame(firstFrame, 2));
                ImmAuthoringChange lastChange = null;
                document.Changed += change => lastChange = change;
                Require(document.SetFrameDrawing(firstFrame, secondDrawing));

                ImmAuthoringSnapshot snapshot = Require(document.CreateSnapshot());
                ImmAuthoringLayerSnapshot paint = snapshot.Layers[0];
                Assert.That(
                    paint.Frames,
                    Has.Count.EqualTo(3));
                Assert.That(
                    new[] { paint.Frames[0].Id, paint.Frames[1].Id, paint.Frames[2].Id },
                    Is.EqualTo(new[] { insertedFrame, secondFrame, firstFrame }));
                Assert.That(
                    new[] { paint.Frames[0].DrawingId, paint.Frames[1].DrawingId, paint.Frames[2].DrawingId },
                    Is.EqualTo(new[] { insertedDrawing, secondDrawing, secondDrawing }));
                Assert.That(paint.Frames[2].Index, Is.EqualTo(2));
                Assert.That(snapshot.TryGetFrame(firstFrame, out ImmAuthoringFrameSnapshot firstFrameSnapshot), Is.True);
                Assert.That(firstFrameSnapshot.PaintLayerId, Is.EqualTo(layer));
                Assert.That(lastChange.AffectedObjectIds, Does.Contain(firstFrame));

                Require(document.RemoveFrame(insertedFrame));
                snapshot = Require(document.CreateSnapshot());
                Assert.That(snapshot.TryGetFrame(insertedFrame, out _), Is.False);
                Assert.That(
                    new[] { snapshot.Layers[0].Frames[0].Id, snapshot.Layers[0].Frames[1].Id },
                    Is.EqualTo(new[] { secondFrame, firstFrame }));

                Require(document.ResizeFrameSequence(layer, 4, firstDrawing));
                paint = Require(document.CreateSnapshot()).Layers[0];
                Assert.That(paint.Frames[0].Id, Is.EqualTo(secondFrame));
                Assert.That(paint.Frames[1].Id, Is.EqualTo(firstFrame));
                HashSet<long> frameIds = new HashSet<long>();
                foreach (ImmAuthoringFrameSnapshot frame in paint.Frames)
                    Assert.That(frameIds.Add(frame.Id), Is.True, $"Duplicate frame ID {frame.Id}");

                Require(document.ResizeFrameSequence(layer, 1));
                snapshot = Require(document.CreateSnapshot());
                Assert.That(snapshot.TryGetFrame(secondFrame, out _), Is.True);
                Assert.That(snapshot.TryGetFrame(firstFrame, out _), Is.False);
                Assert.That(document.Validate().Succeeded, Is.True);
            }
        }

        [Test]
        public void RepeatedManagedLifecycleRejectsUseAfterDispose()
        {
            for (int iteration = 0; iteration < 100; iteration++)
            {
                ImmAuthoringDocument document = CreateDocument();
                Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default($"Paint {iteration}")));
                document.Dispose();

                Assert.That(document.CreateSnapshot().ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Disposed));
                Assert.That(
                    document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Disposed")).ErrorCode,
                    Is.EqualTo(ImmAuthoringErrorCode.Disposed));
                document.Dispose();
            }
        }
        [Test]
        public void CancelledExportReturnsStructuredResultWithoutNativeWork()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            using (CancellationTokenSource cancellation = new CancellationTokenSource())
            {
                cancellation.Cancel();
                ImmAuthoringExportResult result = ImmAuthoringCompiler.ExportToMemory(
                    document,
                    cancellationToken: cancellation.Token);
                Assert.That(result.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Cancelled));
                Assert.That(result.Data, Is.Null);
                Assert.That(result.SourceRevision, Is.EqualTo(document.Revision));
            }
        }

#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
        [Test]
        public void ExportFailureForDisposedDocumentIdentifiesStableDocumentId()
        {
            ImmAuthoringDocument document = CreateDocument();
            long documentId = document.DocumentId;
            document.Dispose();

            ImmAuthoringExportResult result = ImmAuthoringCompiler.ExportToMemory(document);

            Assert.That(result.Succeeded, Is.False);
            Assert.That(result.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Disposed));
            Assert.That(result.ObjectId, Is.EqualTo(documentId));
            Assert.That(result.Data, Is.Null);
        }
        [Test]
        public void ValidSnapshotExportsDeterministicallyToOwnedMemory()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                long layer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                long drawing = Require(document.CreateDrawing(layer));
                Require(document.CreateStroke(
                    drawing,
                    BrushSectionType.Circle,
                    VisibilityType.Always,
                    new[]
                    {
                        Point(0f), Point(1f), Point(2f), Point(3f),
                        Point(4f), Point(5f), Point(6f), Point(7f)
                    }));
                Require(document.AppendFrame(layer, drawing));

                ImmAuthoringExportResult first = ImmAuthoringCompiler.ExportToMemory(document);
                ImmAuthoringExportResult second = ImmAuthoringCompiler.ExportToMemory(document);
                Assert.That(first.Succeeded, Is.True, first.Message);
                Assert.That(second.Succeeded, Is.True, second.Message);
                Assert.That(first.SourceRevision, Is.EqualTo(document.Revision));
                Assert.That(first.BytesWritten, Is.GreaterThan(0));
                Assert.That(second.Data, Is.EqualTo(first.Data));
                Assert.That(first.Statistics.PointCount, Is.EqualTo(8));
            }
        }

        [UnityTest]
        public IEnumerator ExportedMemoryAndFileLoadThroughPlayerAndReleaseOwnership()
        {
            const int frameCount = 3;
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            Assert.That(manager.Initialize(), Is.True);
            int baselineDocuments = manager.LoadedDocumentCount;
            int baselineBuffers = manager.OwnedInputBufferCount;
            string filePath = Path.Combine(
                Application.temporaryCachePath,
                $"imm-phase3-load-{Guid.NewGuid():N}.imm");
            ImmDocument memoryPlayback = null;
            ImmDocument filePlayback = null;

            try
            {
                using (ImmAuthoringDocument document = CreatePlaybackDocument(frameCount))
                {
                    ImmAuthoringExportResult memory = ImmAuthoringCompiler.ExportToMemory(document);
                    ImmAuthoringExportResult file = ImmAuthoringCompiler.ExportToFile(document, filePath);
                    Assert.That(memory.Succeeded, Is.True, memory.Message);
                    Assert.That(file.Succeeded, Is.True, file.Message);
                    Assert.That(memory.SourceRevision, Is.EqualTo(document.Revision));
                    Assert.That(file.SourceRevision, Is.EqualTo(document.Revision));
                    Assert.That(file.BytesWritten, Is.EqualTo(new FileInfo(filePath).Length));
                    Assert.That(File.ReadAllBytes(filePath), Is.EqualTo(memory.Data));

                    memoryPlayback = manager.LoadDocumentFromMemory(memory.Data, "imm-phase3-memory.imm");
                    Assert.That(memoryPlayback, Is.Not.Null);
                    Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments + 1));
                    Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers + 1));
                    float deadline = Time.realtimeSinceStartup + 30f;
                    while (!IsPlaybackReady(memoryPlayback) && Time.realtimeSinceStartup < deadline)
                        yield return null;
                    Assert.That(IsPlaybackReady(memoryPlayback), Is.True, "Memory export did not become fully loaded.");
                    Assert.That(HasPaintFrameCount(memoryPlayback, frameCount), Is.True);
                    manager.UnloadDocument(memoryPlayback);
                    memoryPlayback = null;
                    deadline = Time.realtimeSinceStartup + 30f;
                    while (manager.OwnedInputBufferCount != baselineBuffers && Time.realtimeSinceStartup < deadline)
                        yield return null;
                    Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments));
                    Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers));

                    filePlayback = manager.LoadDocument(filePath);
                    Assert.That(filePlayback, Is.Not.Null);
                    deadline = Time.realtimeSinceStartup + 30f;
                    while (!IsPlaybackReady(filePlayback) && Time.realtimeSinceStartup < deadline)
                        yield return null;
                    Assert.That(IsPlaybackReady(filePlayback), Is.True, "File export did not become fully loaded.");
                    Assert.That(HasPaintFrameCount(filePlayback, frameCount), Is.True);
                    manager.UnloadDocument(filePlayback);
                    filePlayback = null;
                }
            }
            finally
            {
                if (memoryPlayback != null)
                    manager.UnloadDocument(memoryPlayback);
                if (filePlayback != null)
                    manager.UnloadDocument(filePlayback);
                if (File.Exists(filePath))
                    File.Delete(filePath);
            }

            Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments));
            Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers));
            Debug.Log(
                $"[IMM_PHASE3_LOAD_TEST] passed documents={manager.LoadedDocumentCount} " +
                $"buffers={manager.OwnedInputBufferCount}");
        }

        [UnityTest]
        [Category("ImmLifecycleGate")]
        public IEnumerator RebuildLoadUnloadOneHundredCyclesKeepsNativeOwnershipBalanced()
        {
            const int cycles = 100;
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            Assert.That(manager.Initialize(), Is.True);
            int baselineDocuments = manager.LoadedDocumentCount;
            int baselineBuffers = manager.OwnedInputBufferCount;
            long initialManagedBytes = GC.GetTotalMemory(true);
            long initialWorkingSetBytes = System.Diagnostics.Process.GetCurrentProcess().WorkingSet64;

            for (int cycle = 0; cycle < cycles; cycle++)
            {
                ImmDocument playback = null;
                try
                {
                    ImmAuthoringExportResult export;
                    using (ImmAuthoringDocument document = CreatePlaybackDocument(1))
                        export = ImmAuthoringCompiler.ExportToMemory(document);
                    Assert.That(export.Succeeded, Is.True, $"Cycle {cycle}: {export.Message}");

                    playback = manager.LoadDocumentFromMemory(
                        export.Data,
                        $"imm-lifecycle-gate-{cycle}.imm");
                    Assert.That(playback, Is.Not.Null, $"Cycle {cycle} failed to start loading.");
                    float deadline = Time.realtimeSinceStartup + 30f;
                    while (!IsPlaybackReady(playback) && Time.realtimeSinceStartup < deadline)
                        yield return null;
                    Assert.That(IsPlaybackReady(playback), Is.True, $"Cycle {cycle} timed out.");
                }
                finally
                {
                    if (playback != null)
                        manager.UnloadDocument(playback);
                }

                float releaseDeadline = Time.realtimeSinceStartup + 30f;
                while (manager.OwnedInputBufferCount != baselineBuffers && Time.realtimeSinceStartup < releaseDeadline)
                    yield return null;
                Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments), $"Cycle {cycle}");
                Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers), $"Cycle {cycle}");
            }

            long managedDeltaBytes = GC.GetTotalMemory(true) - initialManagedBytes;
            long workingSetDeltaBytes =
                System.Diagnostics.Process.GetCurrentProcess().WorkingSet64 - initialWorkingSetBytes;
            Assert.That(managedDeltaBytes, Is.LessThanOrEqualTo(MaxRetainedManagedBytes));
            Assert.That(workingSetDeltaBytes, Is.LessThanOrEqualTo(MaxRetainedWorkingSetBytes));
            Debug.Log(
                $"[IMM_P1_LIFECYCLE_GATE] passed cycles={cycles} " +
                $"managedDeltaBytes={managedDeltaBytes} workingSetDeltaBytes={workingSetDeltaBytes} " +
                $"documents={manager.LoadedDocumentCount} buffers={manager.OwnedInputBufferCount}");
        }
#endif

        private static ImmAuthoringDocument CreateDocument()
        {
            return Require(ImmAuthoringDocument.Create(ExportSequenceType.Animated, 30, Color.black));
        }

        private static ImmAuthoringDocument CreatePlaybackDocument(int frameCount)
        {
            ImmAuthoringDocument document = CreateDocument();
            ImmAuthoringLayerProperties properties = ImmAuthoringLayerProperties.Default("Paint");
            ExportLayerTiming timing = ExportLayerTiming.FromFrames(frameCount, 30);
            properties.IsTimeline = timing.IsTimeline;
            properties.DurationTicks = timing.DurationTicks;
            properties.MaxRepeatCount = timing.MaxRepeatCount;
            long layer = Require(document.CreatePaintLayer(0, properties));

            for (int frame = 0; frame < frameCount; frame++)
            {
                long drawing = Require(document.CreateDrawing(layer));
                PaintPoint[] points = new PaintPoint[8];
                for (int pointIndex = 0; pointIndex < points.Length; pointIndex++)
                {
                    points[pointIndex] = Point(pointIndex);
                    points[pointIndex].Position.y = frame * 0.01f;
                }
                Require(document.CreateStroke(
                    drawing,
                    BrushSectionType.Circle,
                    VisibilityType.Always,
                    points));
                Require(document.AppendFrame(layer, drawing));
            }

            return document;
        }

        private static bool IsPlaybackReady(ImmDocument document)
        {
            return document.IsSequenceReady() &&
                   document.GetStateInfo().Loading == ImmDocument.LoadingState.Loaded;
        }

        private static bool HasPaintFrameCount(ImmDocument document, int expectedFrameCount)
        {
            foreach (ImmDocument.LayerInfo layer in document.GetLayersManaged())
            {
                if (layer.Type == ImmDocument.LayerType.Paint && layer.PaintNumFrames == expectedFrameCount)
                    return true;
            }
            return false;
        }

        private static PaintPoint Point(float x)
        {
            return new PaintPoint
            {
                Position = new Vector3(x, 0f, 0f),
                Normal = Vector3.up,
                Direction = Vector3.forward,
                Color = Color.white,
                Alpha = 1f,
                Width = 0.01f,
                Length = x,
                Time = x
            };
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
    }
}
