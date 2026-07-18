using System.Collections.Generic;
using System.Threading;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using NUnit.Framework;
using UnityEngine;

namespace ImmPlayer.Tests
{
    public sealed class ImmAuthoringDocumentTests
    {
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
#endif

        private static ImmAuthoringDocument CreateDocument()
        {
            return Require(ImmAuthoringDocument.Create(ExportSequenceType.Animated, 30, Color.black));
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
