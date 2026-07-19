using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    public enum ImmAuthoringPreviewState
    {
        Queued,
        Compiling,
        Loading,
        Installed,
        Failed,
        Cancelled,
        Superseded
    }

    public enum ImmAuthoringPreviewErrorCode
    {
        None,
        InvalidArgument,
        RevisionConflict,
        CompilationFailed,
        PlayerLoadFailed,
        PlayerLoadTimedOut,
        Cancelled,
        Superseded,
        CoordinatorDisposed
    }

    public enum ImmAuthoringPreviewPlaybackState
    {
        Playing,
        Paused,
        PausedAndHidden
    }

    [Serializable]
    public struct ImmAuthoringPreviewSettings
    {
        public ImmAuthoringPreviewPlaybackState PlaybackState;
        public long TimeSinceStart;
        public long TimeSinceStop;
        public Matrix4x4 DocumentToWorld;

        public static ImmAuthoringPreviewSettings Default => new ImmAuthoringPreviewSettings
        {
            PlaybackState = ImmAuthoringPreviewPlaybackState.Playing,
            TimeSinceStart = 0,
            TimeSinceStop = 0,
            DocumentToWorld = Matrix4x4.identity
        };
    }

    public sealed class ImmAuthoringPreviewStatistics
    {
        public TimeSpan GraphCompilationTime { get; internal set; }
        public TimeSpan SerializationTime { get; internal set; }
        public TimeSpan CompilationTime { get; internal set; }
        public TimeSpan PlayerLoadTime { get; internal set; }
        public TimeSpan TotalTime { get; internal set; }
        public long BytesCompiled { get; internal set; }
    }

    public readonly struct ImmAuthoringPreviewTransition
    {
        public ImmAuthoringPreviewState State { get; }
        public double RealtimeSinceStartup { get; }

        internal ImmAuthoringPreviewTransition(ImmAuthoringPreviewState state, double realtimeSinceStartup)
        {
            State = state;
            RealtimeSinceStartup = realtimeSinceStartup;
        }
    }

    public sealed class ImmAuthoringPreviewRequest
    {
        private readonly List<ImmAuthoringPreviewTransition> _transitions =
            new List<ImmAuthoringPreviewTransition>();

        public long RequestId { get; }
        public long DocumentId { get; }
        public long SourceRevision { get; }
        public ImmAuthoringPreviewSettings Settings { get; internal set; }
        public ImmAuthoringPreviewState State { get; internal set; }
        public ImmAuthoringPreviewErrorCode ErrorCode { get; internal set; }
        public ImmAuthoringErrorCode CompilationErrorCode { get; internal set; }
        public string Message { get; internal set; }
        public long ObjectId { get; internal set; }
        public ImmAuthoringPreviewStatistics Statistics { get; } = new ImmAuthoringPreviewStatistics();
        public IReadOnlyList<ImmAuthoringPreviewTransition> Transitions => _transitions.AsReadOnly();
        public bool IsTerminal => State == ImmAuthoringPreviewState.Installed ||
                                  State == ImmAuthoringPreviewState.Failed ||
                                  State == ImmAuthoringPreviewState.Cancelled ||
                                  State == ImmAuthoringPreviewState.Superseded;

        internal ImmAuthoringSnapshot Snapshot { get; }
        internal CancellationTokenSource Cancellation { get; }
        internal Stopwatch TotalStopwatch { get; } = Stopwatch.StartNew();
        internal Stopwatch LoadStopwatch { get; set; }

        internal ImmAuthoringPreviewRequest(
            long requestId,
            ImmAuthoringSnapshot snapshot,
            ImmAuthoringPreviewSettings settings,
            CancellationToken cancellationToken)
        {
            RequestId = requestId;
            Snapshot = snapshot;
            DocumentId = snapshot.DocumentId;
            SourceRevision = snapshot.Revision;
            Settings = settings;
            State = ImmAuthoringPreviewState.Queued;
            ErrorCode = ImmAuthoringPreviewErrorCode.None;
            CompilationErrorCode = ImmAuthoringErrorCode.None;
            Message = string.Empty;
            Cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        }

        internal void AddTransition(ImmAuthoringPreviewState state) =>
            _transitions.Add(new ImmAuthoringPreviewTransition(state, Time.realtimeSinceStartupAsDouble));

        internal void FinishTiming()
        {
            TotalStopwatch.Stop();
            Statistics.TotalTime = TotalStopwatch.Elapsed;
        }

        internal void DisposeCancellation() => Cancellation.Dispose();
    }

    /// <summary>
    /// Compiles immutable authoring snapshots and atomically replaces the native player preview.
    /// The application remains responsible for deciding when a preview should be requested.
    /// </summary>
    public sealed class ImmAuthoringPreviewCoordinator : MonoBehaviour
    {
        private const string LogPrefix = "[IMM_AUTHOR_PREVIEW]";

        [SerializeField, Min(0.1f)] private float playerLoadTimeoutSeconds = 30f;

        private ImmPlayerManager _playerManager;
        private ImmAuthoringPreviewRequest _activeRequest;
        private ImmAuthoringPreviewRequest _installedRequest;
        private ImmDocument _candidateDocument;
        private ImmDocument _installedDocument;
        private long _nextRequestId;
        private bool _disposed;

        public event Action<ImmAuthoringPreviewRequest> StateChanged;

        public ImmAuthoringPreviewRequest ActiveRequest => _activeRequest;
        public ImmAuthoringPreviewRequest InstalledRequest => _installedRequest;
        public ImmDocument InstalledDocument => _installedDocument;
        public long InstalledRevision => _installedRequest?.SourceRevision ?? 0;
        public long InstalledAuthoringDocumentId => _installedRequest?.DocumentId ?? 0;
        public float PlayerLoadTimeoutSeconds
        {
            get => playerLoadTimeoutSeconds;
            set => playerLoadTimeoutSeconds = Mathf.Max(0.1f, value);
        }

        public void SetPlayerManager(ImmPlayerManager playerManager) => _playerManager = playerManager;

        public ImmAuthoringResult<ImmAuthoringPreviewRequest> RequestPreview(
            ImmAuthoringDocument document,
            long expectedRevision,
            CancellationToken cancellationToken = default)
        {
            return RequestPreview(document, expectedRevision, CaptureReplacementSettings(), cancellationToken);
        }

        public ImmAuthoringResult<ImmAuthoringPreviewRequest> RequestPreview(
            ImmAuthoringDocument document,
            long expectedRevision,
            ImmAuthoringPreviewSettings settings,
            CancellationToken cancellationToken = default)
        {
            if (_disposed)
                return Failure(ImmAuthoringErrorCode.Disposed, "Preview coordinator is disposed.");
            if (document == null)
                return Failure(ImmAuthoringErrorCode.InvalidArgument, "Document cannot be null.");
            if (!ValidateSettings(settings, out string settingsError))
                return Failure(ImmAuthoringErrorCode.InvalidArgument, settingsError);

            ImmAuthoringResult<ImmAuthoringSnapshot> snapshotResult = document.CreateSnapshot();
            if (!snapshotResult.Succeeded)
                return ImmAuthoringResult<ImmAuthoringPreviewRequest>.Failure(
                    snapshotResult.ErrorCode, snapshotResult.Message, snapshotResult.ObjectId);
            if (snapshotResult.Value.Revision != expectedRevision)
            {
                return ImmAuthoringResult<ImmAuthoringPreviewRequest>.Failure(
                    ImmAuthoringErrorCode.RevisionConflict,
                    $"Expected revision {expectedRevision}, but the document is at revision {snapshotResult.Value.Revision}.",
                    document.DocumentId);
            }

            ImmAuthoringPreviewRequest request = new ImmAuthoringPreviewRequest(
                ++_nextRequestId, snapshotResult.Value, settings, cancellationToken);
            ImmAuthoringPreviewRequest obsolete = _activeRequest;
            _activeRequest = request;
            if (obsolete != null)
            {
                obsolete.Cancellation.Cancel();
                ReleaseCandidate();
                Complete(
                    obsolete,
                    ImmAuthoringPreviewState.Superseded,
                    ImmAuthoringPreviewErrorCode.Superseded,
                    "Preview request was superseded by a newer request.");
            }
            if (ReferenceEquals(_activeRequest, request))
                Transition(request, ImmAuthoringPreviewState.Queued);
            return ImmAuthoringResult<ImmAuthoringPreviewRequest>.Success(request);
        }

        public bool CancelPreview(long requestId)
        {
            if (_activeRequest == null || _activeRequest.RequestId != requestId)
                return false;

            _activeRequest.Cancellation.Cancel();
            ReleaseCandidate();
            Complete(
                _activeRequest,
                ImmAuthoringPreviewState.Cancelled,
                ImmAuthoringPreviewErrorCode.Cancelled,
                "Preview request was cancelled.");
            return true;
        }

        public void ClearPreview()
        {
            if (_activeRequest != null)
            {
                _activeRequest.Cancellation.Cancel();
                ReleaseCandidate();
                Complete(
                    _activeRequest,
                    ImmAuthoringPreviewState.Cancelled,
                    ImmAuthoringPreviewErrorCode.Cancelled,
                    "Preview was cleared.");
            }

            if (_installedDocument != null)
                PlayerManager.UnloadDocument(_installedDocument);
            _installedDocument = null;
            _installedRequest = null;
        }

        private ImmPlayerManager PlayerManager => _playerManager != null ? _playerManager : ImmPlayerManager.Instance;

        private void Update()
        {
            if (_activeRequest == null)
                return;

            if (_activeRequest.Cancellation.IsCancellationRequested)
            {
                CancelPreview(_activeRequest.RequestId);
                return;
            }

            switch (_activeRequest.State)
            {
                case ImmAuthoringPreviewState.Queued:
                    CompileActiveRequest();
                    break;
                case ImmAuthoringPreviewState.Loading:
                    PollActiveLoad();
                    break;
            }
        }

        private void CompileActiveRequest()
        {
            ImmAuthoringPreviewRequest request = _activeRequest;
            CancellationToken cancellationToken = request.Cancellation.Token;
            Transition(request, ImmAuthoringPreviewState.Compiling);
            if (request != _activeRequest)
                return;
            Stopwatch compilation = Stopwatch.StartNew();
            ImmAuthoringExportResult export = ImmAuthoringCompiler.ExportToMemory(
                request.Snapshot,
                cancellationToken: cancellationToken);
            compilation.Stop();
            request.Statistics.CompilationTime = compilation.Elapsed;
            request.Statistics.GraphCompilationTime = export.Statistics.GraphCompilationTime;
            request.Statistics.SerializationTime = export.Statistics.SerializationTime;
            request.Statistics.BytesCompiled = export.BytesWritten;

            if (request != _activeRequest)
                return;
            if (!export.Succeeded)
            {
                ImmAuthoringPreviewState terminalState = export.ErrorCode == ImmAuthoringErrorCode.Cancelled
                    ? ImmAuthoringPreviewState.Cancelled
                    : ImmAuthoringPreviewState.Failed;
                ImmAuthoringPreviewErrorCode errorCode = export.ErrorCode == ImmAuthoringErrorCode.Cancelled
                    ? ImmAuthoringPreviewErrorCode.Cancelled
                    : ImmAuthoringPreviewErrorCode.CompilationFailed;
                request.CompilationErrorCode = export.ErrorCode;
                Complete(request, terminalState, errorCode, export.Message, export.ObjectId);
                return;
            }

            ImmPlayerManager manager = PlayerManager;
            if (!manager.IsInitialized && !manager.Initialize())
            {
                Complete(
                    request,
                    ImmAuthoringPreviewState.Failed,
                    ImmAuthoringPreviewErrorCode.PlayerLoadFailed,
                    "IMM player initialization failed.");
                return;
            }

            _candidateDocument = manager.LoadDocumentFromMemory(
                export.Data,
                $"authoring-preview-{request.DocumentId}-r{request.SourceRevision}.imm");
            if (_candidateDocument == null)
            {
                Complete(
                    request,
                    ImmAuthoringPreviewState.Failed,
                    ImmAuthoringPreviewErrorCode.PlayerLoadFailed,
                    "Native player rejected the compiled preview.");
                return;
            }

            request.LoadStopwatch = Stopwatch.StartNew();
            Transition(request, ImmAuthoringPreviewState.Loading);
        }

        private void PollActiveLoad()
        {
            ImmAuthoringPreviewRequest request = _activeRequest;
            ImmDocument.DocumentStateInfo state = _candidateDocument.GetStateInfo();
            if (state.Loading == ImmDocument.LoadingState.Failed ||
                state.Loading == ImmDocument.LoadingState.Unloaded)
            {
                ReleaseCandidate();
                Complete(
                    request,
                    ImmAuthoringPreviewState.Failed,
                    ImmAuthoringPreviewErrorCode.PlayerLoadFailed,
                    $"Native player load ended in state {state.Loading}.");
                return;
            }

            if (request.LoadStopwatch.Elapsed.TotalSeconds > playerLoadTimeoutSeconds)
            {
                ReleaseCandidate();
                Complete(
                    request,
                    ImmAuthoringPreviewState.Failed,
                    ImmAuthoringPreviewErrorCode.PlayerLoadTimedOut,
                    $"Native player load exceeded {playerLoadTimeoutSeconds:F1} seconds.");
                return;
            }

            if (state.Loading != ImmDocument.LoadingState.Loaded || !_candidateDocument.IsSequenceReady())
                return;

            ApplySettings(_candidateDocument, request.Settings);
            ImmDocument replacedDocument = _installedDocument;
            _installedDocument = _candidateDocument;
            _candidateDocument = null;
            _installedRequest = request;
            Complete(request, ImmAuthoringPreviewState.Installed, ImmAuthoringPreviewErrorCode.None, string.Empty);

            if (replacedDocument != null)
                PlayerManager.UnloadDocument(replacedDocument);
        }

        private ImmAuthoringPreviewSettings CaptureReplacementSettings()
        {
            if (_installedDocument == null || _installedRequest == null)
                return ImmAuthoringPreviewSettings.Default;

            ImmAuthoringPreviewSettings settings = _installedRequest.Settings;
            _installedDocument.GetTime(out long timeSinceStart, out long timeSinceStop);
            settings.TimeSinceStart = timeSinceStart;
            settings.TimeSinceStop = timeSinceStop;
            settings.PlaybackState = ToRequestedPlaybackState(_installedDocument.GetStateInfo().Playback);
            return settings;
        }

        private static ImmAuthoringPreviewPlaybackState ToRequestedPlaybackState(ImmDocument.PlaybackState state)
        {
            switch (state)
            {
                case ImmDocument.PlaybackState.PausedAndHidden:
                    return ImmAuthoringPreviewPlaybackState.PausedAndHidden;
                case ImmDocument.PlaybackState.Playing:
                    return ImmAuthoringPreviewPlaybackState.Playing;
                default:
                    return ImmAuthoringPreviewPlaybackState.Paused;
            }
        }

        private static void ApplySettings(ImmDocument document, ImmAuthoringPreviewSettings settings)
        {
            document.SetTransform(settings.DocumentToWorld);
            document.SetTime(settings.TimeSinceStart, settings.TimeSinceStop);
            switch (settings.PlaybackState)
            {
                case ImmAuthoringPreviewPlaybackState.Playing:
                    document.Show();
                    document.Resume();
                    break;
                case ImmAuthoringPreviewPlaybackState.Paused:
                    document.Show();
                    document.Pause();
                    break;
                case ImmAuthoringPreviewPlaybackState.PausedAndHidden:
                    document.Hide();
                    break;
            }
        }

        private void ReleaseCandidate()
        {
            if (_candidateDocument == null)
                return;
            PlayerManager.UnloadDocument(_candidateDocument);
            _candidateDocument = null;
        }

        private void Complete(
            ImmAuthoringPreviewRequest request,
            ImmAuthoringPreviewState state,
            ImmAuthoringPreviewErrorCode errorCode,
            string message,
            long objectId = 0)
        {
            if (request.LoadStopwatch != null && request.LoadStopwatch.IsRunning)
            {
                request.LoadStopwatch.Stop();
                request.Statistics.PlayerLoadTime = request.LoadStopwatch.Elapsed;
            }
            request.ErrorCode = errorCode;
            request.Message = message ?? string.Empty;
            request.ObjectId = objectId;
            request.FinishTiming();
            if (ReferenceEquals(_activeRequest, request))
                _activeRequest = null;
            Transition(request, state);
            request.DisposeCancellation();
        }

        private void Transition(ImmAuthoringPreviewRequest request, ImmAuthoringPreviewState state)
        {
            request.State = state;
            request.AddTransition(state);
            UnityEngine.Debug.Log(
                $"{LogPrefix} request={request.RequestId} document={request.DocumentId} " +
                $"sourceRevision={request.SourceRevision} installedRevision={InstalledRevision} " +
                $"state={state} elapsedMs={request.TotalStopwatch.Elapsed.TotalMilliseconds:F3} " +
                $"bytes={request.Statistics.BytesCompiled} result={request.ErrorCode}");
            Action<ImmAuthoringPreviewRequest> handlers = StateChanged;
            if (handlers == null)
                return;
            foreach (Action<ImmAuthoringPreviewRequest> handler in handlers.GetInvocationList())
            {
                try
                {
                    handler(request);
                }
                catch (Exception exception)
                {
                    UnityEngine.Debug.LogError(
                        $"{LogPrefix} request={request.RequestId} state={state} " +
                        $"subscriberException={exception.GetType().Name} message={exception.Message}");
                }
            }
        }

        private static bool ValidateSettings(ImmAuthoringPreviewSettings settings, out string error)
        {
            if (!Enum.IsDefined(typeof(ImmAuthoringPreviewPlaybackState), settings.PlaybackState))
            {
                error = "Preview playback state is invalid.";
                return false;
            }
            for (int index = 0; index < 16; index++)
            {
                float value = settings.DocumentToWorld[index];
                if (float.IsNaN(value) || float.IsInfinity(value))
                {
                    error = "Document-to-world transform must contain only finite values.";
                    return false;
                }
            }
            error = string.Empty;
            return true;
        }

        private static ImmAuthoringResult<ImmAuthoringPreviewRequest> Failure(
            ImmAuthoringErrorCode errorCode,
            string message)
        {
            return ImmAuthoringResult<ImmAuthoringPreviewRequest>.Failure(errorCode, message);
        }

        private void OnDestroy()
        {
            if (_disposed)
                return;
            _disposed = true;
            ClearPreview();
            StateChanged = null;
        }
    }
}
