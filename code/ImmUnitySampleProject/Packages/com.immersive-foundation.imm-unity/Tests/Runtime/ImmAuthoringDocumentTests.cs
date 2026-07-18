using System.Collections.Generic;
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
                PaintPoint[] source = { Point(0f), Point(1f) };
                long stroke = Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, source));
                source[0].Position = Vector3.one * 99f;

                ImmAuthoringSnapshot first = Require(document.CreateSnapshot());
                ImmAuthoringStrokeSnapshot firstStroke = first.Layers[0].Drawings[0].Strokes[0];
                Assert.That(firstStroke.Id, Is.EqualTo(stroke));
                Assert.That(firstStroke.Points[0].Position, Is.EqualTo(Vector3.zero));

                Require(document.ReplaceStroke(stroke, BrushSectionType.Square, VisibilityType.FadePow2, new[] { Point(2f) }));
                ImmAuthoringSnapshot second = Require(document.CreateSnapshot());
                Assert.That(firstStroke.Points.Count, Is.EqualTo(2));
                Assert.That(second.Layers[0].Drawings[0].Strokes[0].Points.Count, Is.EqualTo(1));
            }
        }

        [Test]
        public void SuccessfulMutationPublishesOneRevisionedChange()
        {
            using (ImmAuthoringDocument document = CreateDocument())
            {
                List<ImmAuthoringChange> changes = new List<ImmAuthoringChange>();
                document.Changed += changes.Add;
                long layer = Require(document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));

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
