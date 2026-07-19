using System;
using System.Collections;
using System.IO;
using System.Linq;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace ImmPlayer.Tests
{
    public sealed class ImmAuthoringImportTests
    {
        [Test]
        public void AnimationKeysHaveStableIdsAndTransactionalMutation()
        {
            using (ImmAuthoringDocument document = CreateSupportedFixture())
            {
                ImmAuthoringSnapshot initial = Require(document.CreateSnapshot());
                ImmAuthoringLayerSnapshot paint = initial.Layers.Single(layer => layer.Type == ImmAuthoringLayerType.Paint);
                Assert.That(paint.AnimationKeys, Has.Count.EqualTo(3));
                long keyId = paint.AnimationKeys[0].Id;
                Assert.That(initial.TryGetAnimationKey(keyId, out ImmAuthoringAnimationKeySnapshot key), Is.True);

                using (ImmAuthoringTransaction transaction = Require(document.BeginEdit(document.Revision)))
                {
                    Require(transaction.EditableDocument.ReplaceAnimationKey(
                        keyId,
                        key.Property,
                        key.TimeTicks + 100,
                        ImmAuthoringAnimationValue.FromFloat(0.35f),
                        key.Interpolation));
                    Require(transaction.Commit());
                }

                ImmAuthoringSnapshot changed = Require(document.CreateSnapshot());
                Assert.That(changed.TryGetAnimationKey(keyId, out key), Is.True);
                Assert.That(key.TimeTicks, Is.EqualTo(100));
                Assert.That(key.Value.FloatValue, Is.EqualTo(0.35f));
            }
        }

#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
        [UnityTest]
        public IEnumerator SupportedPaintImportsMutatesRoundTripsAndPlays()
        {
            string filePath = Path.Combine(Application.temporaryCachePath, $"imm-phase5-{Guid.NewGuid():N}.imm");
            ImmDocument playback = null;
            ImmAuthoringImportResult memoryImport = null;
            ImmAuthoringImportResult fileImport = null;
            ImmAuthoringImportResult modifiedImport = null;
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            Assert.That(manager.Initialize(), Is.True);

            try
            {
                using (ImmAuthoringDocument source = CreateSupportedFixture())
                {
                    ImmAuthoringSnapshot sourceSnapshot = Require(source.CreateSnapshot());
                    ImmAuthoringExportResult memory = ImmAuthoringCompiler.ExportToMemory(source);
                    ImmAuthoringExportResult file = ImmAuthoringCompiler.ExportToFile(source, filePath);
                    Assert.That(memory.Succeeded, Is.True, memory.Message);
                    Assert.That(file.Succeeded, Is.True, file.Message);

                    memoryImport = ImmAuthoringImporter.ImportFromMemory(memory.Data);
                    fileImport = ImmAuthoringImporter.ImportFromFile(filePath);
                    AssertLossless(memoryImport);
                    AssertLossless(fileImport);
                    AssertEquivalent(sourceSnapshot, Require(memoryImport.Document.CreateSnapshot()));
                    AssertEquivalent(sourceSnapshot, Require(fileImport.Document.CreateSnapshot()));

                    ImmAuthoringSnapshot importedSnapshot = Require(memoryImport.Document.CreateSnapshot());
                    ImmAuthoringStrokeSnapshot importedStroke = importedSnapshot.Layers
                        .SelectMany(layer => layer.Drawings)
                        .SelectMany(drawing => drawing.Strokes)
                        .First();
                    PaintPoint[] changedPoints = importedStroke.Points.ToArray();
                    for (int pointIndex = 0; pointIndex < changedPoints.Length; pointIndex++)
                    {
                        PaintPoint point = changedPoints[pointIndex];
                        point.Position.y += 0.2f;
                        point.Color = Color.cyan;
                        changedPoints[pointIndex] = point;
                    }
                    Require(memoryImport.Document.ReplaceStroke(
                        importedStroke.Id,
                        importedStroke.BrushSection,
                        importedStroke.Visibility,
                        changedPoints));

                    ImmAuthoringSnapshot modifiedSnapshot = Require(memoryImport.Document.CreateSnapshot());
                    ImmAuthoringExportResult modified = ImmAuthoringCompiler.ExportToMemory(memoryImport.Document);
                    Assert.That(modified.Succeeded, Is.True, modified.Message);
                    modifiedImport = ImmAuthoringImporter.ImportFromMemory(modified.Data);
                    AssertLossless(modifiedImport);
                    AssertEquivalent(modifiedSnapshot, Require(modifiedImport.Document.CreateSnapshot()));

                    playback = manager.LoadDocumentFromMemory(modified.Data, "imm-phase5-round-trip.imm");
                    Assert.That(playback, Is.Not.Null);
                    float deadline = Time.realtimeSinceStartup + 30f;
                    while ((!playback.IsSequenceReady() || playback.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded) &&
                           Time.realtimeSinceStartup < deadline)
                        yield return null;
                    Assert.That(playback.IsSequenceReady(), Is.True, "Modified re-export did not become playable.");
                    Assert.That(
                        playback.GetStateInfo().Loading,
                        Is.EqualTo(ImmDocument.LoadingState.Loaded),
                        "Modified re-export did not finish loading.");
                    ImmDocument.LayerInfo[] playbackLayers = playback.GetLayersManaged();
                    Assert.That(
                        playbackLayers.Any(layer => layer.Type == ImmDocument.LayerType.Paint),
                        Is.True,
                        $"Playback exposed {playbackLayers.Length} layers: {string.Join(", ", playbackLayers.Select(layer => $"{layer.Id}:{layer.Type}:{layer.Name}"))}");
                }
            }
            finally
            {
                if (playback != null)
                    manager.UnloadDocument(playback);
                memoryImport?.Document?.Dispose();
                fileImport?.Document?.Dispose();
                modifiedImport?.Document?.Dispose();
                if (File.Exists(filePath))
                    File.Delete(filePath);
            }
        }

        [Test]
        public void UnsupportedExistingContentPreventsSafeOverwrite()
        {
            string samplePath = Path.Combine(Application.streamingAssetsPath, "sample1.imm");
            ImmAuthoringImportResult import = ImmAuthoringImporter.ImportFromFile(samplePath);
            try
            {
                Assert.That(import.Succeeded, Is.True, import.Message);
                Assert.That(import.Lossiness, Is.EqualTo(ImmAuthoringImportLossiness.Lossy));
                Assert.That(import.CanOverwriteSource, Is.False);
                Assert.That(import.Issues, Is.Not.Empty);
            }
            finally
            {
                import.Document?.Dispose();
            }
        }
#endif

        private static ImmAuthoringDocument CreateSupportedFixture()
        {
            ImmAuthoringDocument document = Require(ImmAuthoringDocument.Create(
                ExportSequenceType.Animated,
                24,
                new Color(0.08f, 0.12f, 0.18f, 1f)));
            using (ImmAuthoringTransaction transaction = Require(document.BeginEdit(0)))
            {
                ImmAuthoringDocument editable = transaction.EditableDocument;
                ImmAuthoringLayerProperties groupProperties = ImmAuthoringLayerProperties.Default("Imported Group");
                groupProperties.Transform = new ImmAuthoringTransform
                {
                    Position = new Vector3(0.1f, 0.2f, 0.3f),
                    Rotation = Quaternion.Euler(0f, 15f, 0f),
                    Scale = 1.1f
                };
                long group = Require(editable.CreateGroupLayer(0, groupProperties));

                ImmAuthoringLayerProperties paintProperties = ImmAuthoringLayerProperties.Default("Editable Paint");
                ExportLayerTiming timing = ExportLayerTiming.FromFrames(3, 24, 2);
                paintProperties.IsTimeline = timing.IsTimeline;
                paintProperties.DurationTicks = timing.DurationTicks;
                paintProperties.MaxRepeatCount = timing.MaxRepeatCount;
                paintProperties.PaintMaxRepeatCount = 3;
                long paint = Require(editable.CreatePaintLayer(group, paintProperties));
                long firstDrawing = Require(editable.CreateDrawing(paint));
                long secondDrawing = Require(editable.CreateDrawing(paint));
                Require(editable.CreateStroke(firstDrawing, BrushSectionType.Circle, VisibilityType.Always, StrokePoints(0f, Color.red)));
                Require(editable.CreateStroke(secondDrawing, BrushSectionType.Ellipse, VisibilityType.FadePow2, StrokePoints(0.4f, Color.blue)));
                Require(editable.AppendFrame(paint, firstDrawing));
                Require(editable.AppendFrame(paint, secondDrawing));
                Require(editable.AppendFrame(paint, firstDrawing));

                Require(editable.CreateAnimationKey(
                    paint,
                    ImmAuthoringAnimationProperty.Opacity,
                    0,
                    ImmAuthoringAnimationValue.FromFloat(0.25f),
                    ImmAuthoringInterpolation.Linear));
                Require(editable.CreateAnimationKey(
                    paint,
                    ImmAuthoringAnimationProperty.Opacity,
                    ExportLayerTiming.TicksPerSecond,
                    ImmAuthoringAnimationValue.FromFloat(0.8f),
                    ImmAuthoringInterpolation.Smoothstep));
                Require(editable.CreateAnimationKey(
                    paint,
                    ImmAuthoringAnimationProperty.Transform,
                    0,
                    ImmAuthoringAnimationValue.FromTransform(new ImmAuthoringTransform
                    {
                        Position = new Vector3(0f, 0.25f, 0f),
                        Rotation = Quaternion.Euler(0f, 0f, 10f),
                        Scale = 1f
                    }),
                    ImmAuthoringInterpolation.EaseIn));
                Require(transaction.Commit());
            }
            return document;
        }

        private static PaintPoint[] StrokePoints(float yOffset, Color color)
        {
            PaintPoint[] points = new PaintPoint[3];
            for (int pointIndex = 0; pointIndex < points.Length; pointIndex++)
            {
                points[pointIndex] = new PaintPoint
                {
                    Position = new Vector3(pointIndex * 0.5f, yOffset, 0f),
                    Normal = Vector3.up,
                    Direction = Vector3.forward,
                    Color = color,
                    Alpha = 1f,
                    Width = 0.04f,
                    Length = pointIndex * 0.5f,
                    Time = 0.5f * pointIndex / points.Length
                };
            }
            return points;
        }

        private static void AssertLossless(ImmAuthoringImportResult result)
        {
            Assert.That(result.Succeeded, Is.True, result.Message);
            Assert.That(result.Lossiness, Is.EqualTo(ImmAuthoringImportLossiness.Lossless),
                string.Join("\n", result.Issues.Select(issue => issue.Message)));
            Assert.That(result.CanOverwriteSource, Is.True);
        }

        private static void AssertEquivalent(ImmAuthoringSnapshot expected, ImmAuthoringSnapshot actual)
        {
            ImmAuthoringStructuralComparison comparison = ImmAuthoringStructuralComparer.Compare(expected, actual, 0.02f);
            Assert.That(comparison.Equivalent, Is.True,
                string.Join("\n", comparison.Differences.Take(20).Select(difference => difference.ToString())));
        }

        private static void Require(ImmAuthoringResult result)
        {
            Assert.That(result.Succeeded, Is.True, result.ToString());
        }

        private static T Require<T>(ImmAuthoringResult<T> result)
        {
            Assert.That(result.Succeeded, Is.True, result.Message);
            return result.Value;
        }
    }
}
