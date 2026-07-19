using System.Collections;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace ImmPlayer.Tests
{
    public sealed class ImmAuthoringPreviewCoordinatorTests
    {
        [Test]
        public void RequestRequiresExactRevisionAndValidSettings()
        {
            GameObject owner = new GameObject("IMM Phase 4 Preview Test");
            ImmAuthoringPreviewCoordinator coordinator = owner.AddComponent<ImmAuthoringPreviewCoordinator>();
            using (ImmAuthoringDocument document = CreatePlaybackDocument(1))
            {
                ImmAuthoringResult<ImmAuthoringPreviewRequest> stale =
                    coordinator.RequestPreview(document, document.Revision - 1);
                Assert.That(stale.Succeeded, Is.False);
                Assert.That(stale.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.RevisionConflict));
                Assert.That(coordinator.ActiveRequest, Is.Null);

                ImmAuthoringPreviewSettings invalid = ImmAuthoringPreviewSettings.Default;
                invalid.DocumentToWorld[0] = float.NaN;
                ImmAuthoringResult<ImmAuthoringPreviewRequest> badSettings =
                    coordinator.RequestPreview(document, document.Revision, invalid);
                Assert.That(badSettings.Succeeded, Is.False);
                Assert.That(badSettings.ErrorCode, Is.EqualTo(ImmAuthoringErrorCode.InvalidArgument));
                Assert.That(coordinator.ActiveRequest, Is.Null);
            }
            Object.DestroyImmediate(owner);
        }

        [Test]
        public void QueuedRequestsCanBeCancelledOrSupersededBeforeNativeWork()
        {
            GameObject owner = new GameObject("IMM Phase 4 Preview Test");
            ImmAuthoringPreviewCoordinator coordinator = owner.AddComponent<ImmAuthoringPreviewCoordinator>();
            using (ImmAuthoringDocument document = CreatePlaybackDocument(1))
            {
                ImmAuthoringPreviewRequest cancelled = Require(
                    coordinator.RequestPreview(document, document.Revision));
                Assert.That(coordinator.CancelPreview(cancelled.RequestId), Is.True);
                Assert.That(cancelled.State, Is.EqualTo(ImmAuthoringPreviewState.Cancelled));
                Assert.That(cancelled.ErrorCode, Is.EqualTo(ImmAuthoringPreviewErrorCode.Cancelled));

                ImmAuthoringPreviewRequest obsolete = Require(
                    coordinator.RequestPreview(document, document.Revision));
                ImmAuthoringPreviewRequest latest = Require(
                    coordinator.RequestPreview(document, document.Revision));
                Assert.That(obsolete.State, Is.EqualTo(ImmAuthoringPreviewState.Superseded));
                Assert.That(obsolete.ErrorCode, Is.EqualTo(ImmAuthoringPreviewErrorCode.Superseded));
                Assert.That(latest.State, Is.EqualTo(ImmAuthoringPreviewState.Queued));
                Assert.That(coordinator.ActiveRequest, Is.SameAs(latest));
                coordinator.CancelPreview(latest.RequestId);
            }
            Object.DestroyImmediate(owner);
        }

#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
        [UnityTest]
        public IEnumerator ReplacementPreservesRequestedStateAndFailureKeepsLastValidPreview()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            Assert.That(manager.Initialize(), Is.True);
            int baselineDocuments = manager.LoadedDocumentCount;
            int baselineBuffers = manager.OwnedInputBufferCount;
            GameObject owner = new GameObject("IMM Phase 4 Preview Test");
            ImmAuthoringPreviewCoordinator coordinator = owner.AddComponent<ImmAuthoringPreviewCoordinator>();
            coordinator.SetPlayerManager(manager);

            using (ImmAuthoringDocument document = CreatePlaybackDocument(3))
            using (ImmAuthoringDocument unsupported = Require(
                       ImmAuthoringDocument.Create(ExportSequenceType.Comic, 30, Color.black)))
            {
                long requestedTime = ExportLayerTiming.FromFrames(1, 30).DurationTicks;
                Matrix4x4 requestedTransform = Matrix4x4.TRS(
                    new Vector3(1f, 2f, 3f),
                    Quaternion.Euler(5f, 15f, 25f),
                    Vector3.one * 1.5f);
                ImmAuthoringPreviewSettings settings = ImmAuthoringPreviewSettings.Default;
                settings.PlaybackState = ImmAuthoringPreviewPlaybackState.Paused;
                settings.TimeSinceStart = requestedTime;
                settings.DocumentToWorld = requestedTransform;

                ImmAuthoringPreviewRequest first = Require(
                    coordinator.RequestPreview(document, document.Revision, settings));
                yield return WaitForTerminal(first);
                AssertInstalled(coordinator, first, document.Revision);
                ImmDocument firstNativeDocument = coordinator.InstalledDocument;

                ImmAuthoringLayerSnapshot paint = FindPaintLayer(Require(document.CreateSnapshot()));
                ImmAuthoringLayerProperties changed = paint.Properties;
                changed.Opacity = 0.75f;
                Require(document.SetLayerProperties(paint.Id, changed));
                long replacementRevision = document.Revision;

                ImmAuthoringPreviewRequest second = Require(
                    coordinator.RequestPreview(document, replacementRevision));
                yield return WaitForTerminal(second);
                AssertInstalled(coordinator, second, replacementRevision);
                Assert.That(coordinator.InstalledDocument, Is.Not.SameAs(firstNativeDocument));
                Assert.That(second.Settings.PlaybackState, Is.EqualTo(ImmAuthoringPreviewPlaybackState.Paused));
                Assert.That(second.Settings.DocumentToWorld, Is.EqualTo(requestedTransform));
                Assert.That(second.Settings.TimeSinceStart, Is.EqualTo(requestedTime).Within(FrameTicks(1)));

                yield return null;
                ImmDocument.DocumentStateInfo nativeState = coordinator.InstalledDocument.GetStateInfo();
                coordinator.InstalledDocument.GetTime(out long actualTime, out _);
                Assert.That(nativeState.Playback, Is.EqualTo(ImmDocument.PlaybackState.Paused));
                Assert.That(actualTime, Is.EqualTo(requestedTime).Within(FrameTicks(1)));

                ImmDocument lastValidDocument = coordinator.InstalledDocument;
                long lastValidRevision = coordinator.InstalledRevision;
                ImmAuthoringPreviewRequest failed = Require(
                    coordinator.RequestPreview(unsupported, unsupported.Revision));
                yield return WaitForTerminal(failed);
                Assert.That(failed.State, Is.EqualTo(ImmAuthoringPreviewState.Failed));
                Assert.That(failed.ErrorCode, Is.EqualTo(ImmAuthoringPreviewErrorCode.CompilationFailed));
                Assert.That(failed.CompilationErrorCode, Is.EqualTo(ImmAuthoringErrorCode.Unsupported));
                Assert.That(coordinator.InstalledDocument, Is.SameAs(lastValidDocument));
                Assert.That(coordinator.InstalledRevision, Is.EqualTo(lastValidRevision));
                Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments + 1));
            }

            coordinator.ClearPreview();
            long clearedRevision = coordinator.InstalledRevision;
            Object.Destroy(owner);
            yield return WaitForOwnership(manager, baselineDocuments, baselineBuffers);
            Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments));
            Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers));
            Debug.Log($"[IMM_AUTHOR_PREVIEW_TEST] replacement-and-failure passed clearedRevision={clearedRevision}");
        }

        [UnityTest]
        public IEnumerator LoadingRequestCanBeSupersededWithoutInstallingStaleRevision()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            Assert.That(manager.Initialize(), Is.True);
            int baselineDocuments = manager.LoadedDocumentCount;
            int baselineBuffers = manager.OwnedInputBufferCount;
            GameObject owner = new GameObject("IMM Phase 4 Supersession Test");
            ImmAuthoringPreviewCoordinator coordinator = owner.AddComponent<ImmAuthoringPreviewCoordinator>();
            coordinator.SetPlayerManager(manager);

            using (ImmAuthoringDocument document = CreatePlaybackDocument(3))
            {
                ImmAuthoringPreviewRequest obsolete = Require(
                    coordinator.RequestPreview(document, document.Revision));
                float loadingDeadline = Time.realtimeSinceStartup + 30f;
                while (obsolete.State == ImmAuthoringPreviewState.Queued ||
                       obsolete.State == ImmAuthoringPreviewState.Compiling)
                {
                    Assert.That(Time.realtimeSinceStartup, Is.LessThan(loadingDeadline));
                    yield return null;
                }
                Assert.That(obsolete.State, Is.EqualTo(ImmAuthoringPreviewState.Loading));

                ImmAuthoringLayerSnapshot paint = FindPaintLayer(Require(document.CreateSnapshot()));
                ImmAuthoringLayerProperties changed = paint.Properties;
                changed.Opacity = 0.5f;
                Require(document.SetLayerProperties(paint.Id, changed));
                long latestRevision = document.Revision;
                ImmAuthoringPreviewRequest latest = Require(
                    coordinator.RequestPreview(document, latestRevision));
                Assert.That(obsolete.State, Is.EqualTo(ImmAuthoringPreviewState.Superseded));

                yield return WaitForTerminal(latest);
                AssertInstalled(coordinator, latest, latestRevision);
                Assert.That(coordinator.InstalledRevision, Is.Not.EqualTo(obsolete.SourceRevision));
            }

            coordinator.ClearPreview();
            Object.Destroy(owner);
            yield return WaitForOwnership(manager, baselineDocuments, baselineBuffers);
            Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments));
            Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers));
            Debug.Log($"[IMM_AUTHOR_PREVIEW_TEST] supersession passed documents={baselineDocuments} buffers={baselineBuffers}");
        }

        [UnityTest]
        [Category("ImmLifecycleGate")]
        public IEnumerator RepeatedPreviewReplacementKeepsOneInstalledDocumentAndReleasesBuffers()
        {
            const int replacements = 25;
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            Assert.That(manager.Initialize(), Is.True);
            int baselineDocuments = manager.LoadedDocumentCount;
            int baselineBuffers = manager.OwnedInputBufferCount;
            GameObject owner = new GameObject("IMM Phase 4 Lifecycle Test");
            ImmAuthoringPreviewCoordinator coordinator = owner.AddComponent<ImmAuthoringPreviewCoordinator>();
            coordinator.SetPlayerManager(manager);

            using (ImmAuthoringDocument document = CreatePlaybackDocument(2))
            {
                ImmAuthoringLayerSnapshot paint = FindPaintLayer(Require(document.CreateSnapshot()));
                for (int replacement = 0; replacement < replacements; replacement++)
                {
                    ImmAuthoringLayerProperties changed = paint.Properties;
                    changed.Opacity = 1f - (replacement * 0.01f);
                    Require(document.SetLayerProperties(paint.Id, changed));
                    paint = FindPaintLayer(Require(document.CreateSnapshot()));
                    ImmAuthoringPreviewRequest request = Require(
                        coordinator.RequestPreview(document, document.Revision));
                    yield return WaitForTerminal(request);
                    AssertInstalled(coordinator, request, document.Revision);

                    float releaseDeadline = Time.realtimeSinceStartup + 30f;
                    while (manager.LoadedDocumentCount != baselineDocuments + 1 ||
                           manager.OwnedInputBufferCount != baselineBuffers + 1)
                    {
                        Assert.That(Time.realtimeSinceStartup, Is.LessThan(releaseDeadline));
                        yield return null;
                    }
                }
            }

            coordinator.ClearPreview();
            Object.Destroy(owner);
            yield return WaitForOwnership(manager, baselineDocuments, baselineBuffers);
            Assert.That(manager.LoadedDocumentCount, Is.EqualTo(baselineDocuments));
            Assert.That(manager.OwnedInputBufferCount, Is.EqualTo(baselineBuffers));
            Debug.Log(
                $"[IMM_AUTHOR_PREVIEW_LIFECYCLE] passed replacements={replacements} " +
                $"documents={manager.LoadedDocumentCount} buffers={manager.OwnedInputBufferCount}");
        }
#endif

        private static IEnumerator WaitForTerminal(ImmAuthoringPreviewRequest request)
        {
            float deadline = Time.realtimeSinceStartup + 30f;
            while (!request.IsTerminal && Time.realtimeSinceStartup < deadline)
                yield return null;
            Assert.That(request.IsTerminal, Is.True, $"Preview request {request.RequestId} timed out in {request.State}.");
        }

        private static IEnumerator WaitForOwnership(
            ImmPlayerManager manager,
            int expectedDocuments,
            int expectedBuffers)
        {
            float deadline = Time.realtimeSinceStartup + 30f;
            while ((manager.LoadedDocumentCount != expectedDocuments ||
                    manager.OwnedInputBufferCount != expectedBuffers) &&
                   Time.realtimeSinceStartup < deadline)
            {
                yield return null;
            }
        }

        private static void AssertInstalled(
            ImmAuthoringPreviewCoordinator coordinator,
            ImmAuthoringPreviewRequest request,
            long expectedRevision)
        {
            Assert.That(request.State, Is.EqualTo(ImmAuthoringPreviewState.Installed), request.Message);
            Assert.That(request.ErrorCode, Is.EqualTo(ImmAuthoringPreviewErrorCode.None));
            Assert.That(request.SourceRevision, Is.EqualTo(expectedRevision));
            Assert.That(coordinator.InstalledRevision, Is.EqualTo(expectedRevision));
            Assert.That(coordinator.InstalledAuthoringDocumentId, Is.EqualTo(request.DocumentId));
            Assert.That(coordinator.InstalledDocument, Is.Not.Null);
            Assert.That(request.Statistics.BytesCompiled, Is.GreaterThan(0));
            Assert.That(request.Statistics.CompilationTime, Is.GreaterThan(System.TimeSpan.Zero));
            Assert.That(request.Statistics.PlayerLoadTime, Is.GreaterThan(System.TimeSpan.Zero));
            Assert.That(request.Transitions[0].State, Is.EqualTo(ImmAuthoringPreviewState.Queued));
            Assert.That(request.Transitions[request.Transitions.Count - 1].State,
                Is.EqualTo(ImmAuthoringPreviewState.Installed));
        }

        private static ImmAuthoringDocument CreatePlaybackDocument(int frameCount)
        {
            ImmAuthoringDocument document = Require(
                ImmAuthoringDocument.Create(ExportSequenceType.Animated, 30, Color.black));
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
                for (int index = 0; index < points.Length; index++)
                {
                    points[index] = Point(index);
                    points[index].Position.y = frame * 0.01f;
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

        private static ImmAuthoringLayerSnapshot FindPaintLayer(ImmAuthoringSnapshot snapshot)
        {
            foreach (ImmAuthoringLayerSnapshot layer in snapshot.Layers)
            {
                if (layer.Type == ImmAuthoringLayerType.Paint)
                    return layer;
            }
            Assert.Fail("Expected a paint layer.");
            return null;
        }

        private static long FrameTicks(int frames) =>
            ExportLayerTiming.FromFrames(frames, 30).DurationTicks;

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

        private static void Require(ImmAuthoringResult result) =>
            Assert.That(result.Succeeded, Is.True, result.Message);
    }
}
