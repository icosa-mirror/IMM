using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using ImmPlayer.Exporter;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    /// <summary>A single paint track whose frame clock and editable stroke data belong to the host.
    /// Call all instance methods on the Unity main thread. The host owns camera registration.</summary>
    public sealed class ImmLivePaintTrack : IDisposable
    {
        public sealed class Drawing
        {
            public long Id { get; }
            public long Revision { get; }
            public ImmLiveDrawingEdits.Stroke[] Strokes { get; }
            public float BiggestStroke { get; }

            public Drawing(long id, long revision, ImmLiveDrawingEdits.Stroke[] strokes, float biggestStroke)
            {
                Id = id;
                Revision = revision;
                Strokes = strokes ?? throw new ArgumentNullException(nameof(strokes));
                BiggestStroke = biggestStroke;
            }
        }

        private sealed class Track
        {
            internal readonly Dictionary<long, Drawing> Drawings;
            internal readonly long[] Frames;

            internal Track(Drawing[] drawings, long[] frames)
            {
                Drawings = drawings.ToDictionary(d => d.Id);
                Frames = frames;
                if (frames.Length == 0 || frames.Any(id => !Drawings.ContainsKey(id)))
                    throw new ArgumentException("A paint track requires frames referencing its drawings.");
            }
        }

        private readonly ImmPlayerManager _manager;
        private readonly uint _fps;
        private readonly CancellationTokenSource _lifetime = new CancellationTokenSource();
        private readonly Dictionary<long, ulong> _handles = new Dictionary<long, ulong>();
        private Track _target, _installing, _presented, _committing;
        private Task<byte[]> _compile;
        private ImmDocument _document;
        private int _layer = -1;
        private ulong _commit, _seek;
        private int _seekFrame = -1, _evaluatedFrame = -1;
        private Matrix4x4 _matrix;
        private bool _visible, _disposed;
        public string Error { get; private set; }

        public ImmLivePaintTrack(ImmPlayerManager manager, int framesPerSecond)
        {
            _manager = manager != null ? manager : throw new ArgumentNullException(nameof(manager));
            if (framesPerSecond <= 0 || ExportLayerTiming.TicksPerSecond % framesPerSecond != 0)
                throw new ArgumentOutOfRangeException(nameof(framesPerSecond));
            _fps = (uint)framesPerSecond;
        }

        /// <summary>The caller must leave all supplied arrays unmodified and change a drawing's
        /// revision whenever its contents change. Multiple frames can use the same drawing ID.</summary>
        public void SetTrack(Drawing[] drawings, long[] frames)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(ImmLivePaintTrack));
            var target = new Track(drawings, frames ?? throw new ArgumentNullException(nameof(frames)));
            Hide();
            _target = target;
            Error = null;
        }

        /// <summary>Progress native loading and pending edits, including when the editor owns the canvas.</summary>
        public void Pump()
        {
            if (_disposed || Error != null || _target == null) return;
            try
            {
                if (_presented == null)
                {
                    Install();
                    return;
                }
                if (_seek != 0)
                {
                    if (!_document.TryGetSpawnAreaSnapshot(null, out var snapshot)) return;
                    if (snapshot.IsComplete(_seek))
                        _evaluatedFrame = _seekFrame;
                    else if (snapshot.requestedCommand == _seek &&
                             snapshot.commandStatus == PlaybackRequestStatus.Pending)
                        return;
                    else if (snapshot.completedCommand != _seek && snapshot.requestedCommand == 0)
                        return;
                    _seek = 0;
                }
                if (_commit != 0)
                {
                    Check(ImmLiveDrawingEdits.GetCommitStatus(_document.DocumentId, _commit, out var status));
                    if (status.State == ImmLiveDrawingEdits.CommitState.Rejected)
                        throw new InvalidOperationException($"IMM rejected paint edit {_commit} ({status.Result}).");
                    if (status.State != ImmLiveDrawingEdits.CommitState.Presented) return;
                    _presented = _committing;
                    _committing = null;
                    _commit = 0;
                    _evaluatedFrame = -1;
                }
                if (_presented != _target) UpdateTrack();
            }
            catch (Exception ex)
            {
                Fail(ex);
            }
        }

        /// <summary>Show the evaluated drawing, or keep the layer hidden while the host renders its fallback.</summary>
        public bool TryPresent(int frame, Matrix4x4 documentToWorld)
        {
            Pump();
            if (_disposed || _target == null || _presented != _target || Error != null ||
                frame < 0 || frame >= _target.Frames.Length || _seek != 0)
            {
                Hide();
                return false;
            }
            try
            {
                if (_evaluatedFrame != frame)
                {
                    Hide();
                    // Seek inside the frame so native tick rounding cannot select its predecessor.
                    long ticks = (2L * frame + 1) * ExportLayerTiming.TicksPerSecond / (2L * _fps);
                    _seek = _document.RequestSetTime(ticks, 0);
                    _seekFrame = frame;
                    return false;
                }
                if (!_visible || _matrix != documentToWorld)
                {
                    _document.SetTransform(documentToWorld);
                    _matrix = documentToWorld;
                }
                if (!_visible)
                    _visible = _document.SetLayerOpacity(_layer, 1f);
                return _visible;
            }
            catch (Exception ex)
            {
                Fail(ex);
                return false;
            }
        }

        public void Hide()
        {
            if (!_visible) return;
            _document.SetLayerOpacity(_layer, 0f);
            _visible = false;
        }

        private void Install()
        {
            if (_document == null)
            {
                if (_compile == null)
                {
                    _installing = _target;
                    Track source = _installing;
                    var cancellation = _lifetime.Token;
                    _compile = Task.Run(() => Compile(source, _fps, cancellation), cancellation);
                }
                if (!_compile.IsCompleted) return;
                var task = _compile;
                _compile = null;
                byte[] bytes = task.GetAwaiter().GetResult();
                _document = _manager.LoadDocumentFromMemory(bytes, "paint-track.imm",
                    new ImmDocumentLoadOptions { InitiallyHidden = true });
                if (_document == null) throw new InvalidOperationException("IMM could not load the paint track.");
                return;
            }

            var state = _document.GetStateInfo();
            if (state.Loading == ImmDocument.LoadingState.Failed)
                throw new InvalidOperationException("IMM paint track loading failed.");
            if (state.Loading != ImmDocument.LoadingState.Loaded || !_document.IsSequenceReady()) return;
            if (_layer < 0)
            {
                _layer = _document.GetLayersManaged().First(l => l.Type == ImmDocument.LayerType.Paint).Id;
                if (!_document.SetLayerOpacity(_layer, 0f))
                    throw new InvalidOperationException("IMM could not hide the pending paint layer.");
            }
            // Show and Pause each occupy the player's document command slot. Wait for each
            // state change; use opacity thereafter so hiding paint does not stop frame evaluation.
            if (state.Playback == ImmDocument.PlaybackState.PausedAndHidden)
            {
                _document.Show();
                return;
            }
            if (state.Playback != ImmDocument.PlaybackState.Paused)
            {
                _document.Pause();
                return;
            }
            int doc = _document.DocumentId;
            Check(ImmLiveDrawingEdits.Attach(doc));
            for (int i = 0; i < _installing.Frames.Length; i++)
            {
                long id = _installing.Frames[i];
                if (_handles.ContainsKey(id)) continue;
                Check(ImmLiveDrawingEdits.GetFrameDrawing(doc, _layer, i, out ulong handle));
                _handles.Add(id, handle);
            }
            _presented = _installing;
            _installing = null;
        }

        private static byte[] Compile(Track track, uint fps, CancellationToken token)
        {
            var create = ImmAuthoringDocument.Create(ExportSequenceType.Animated, fps, Color.black);
            using (var graph = Value(create))
            {
                long layer = Value(graph.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Paint")));
                var ids = new Dictionary<long, long>();
                foreach (long id in track.Frames)
                {
                    token.ThrowIfCancellationRequested();
                    if (!ids.TryGetValue(id, out long drawing))
                    {
                        drawing = Value(graph.CreateDrawing(layer));
                        ids.Add(id, drawing);
                        foreach (var stroke in track.Drawings[id].Strokes)
                        {
                            token.ThrowIfCancellationRequested();
                            Value(graph.CreateStroke(drawing, stroke.Brush, stroke.Visibility, stroke.Points));
                        }
                    }
                    var frame = graph.AppendFrame(layer, drawing);
                    if (!frame.Succeeded) throw new InvalidOperationException(frame.Message);
                }
                var result = ImmAuthoringCompiler.ExportToMemory(graph, cancellationToken: token);
                if (!result.Succeeded) throw new InvalidOperationException(result.Message);
                return result.Data;
            }
        }

        private static T Value<T>(ImmAuthoringResult<T> result)
        {
            if (!result.Succeeded) throw new InvalidOperationException(result.Message);
            return result.Value;
        }

        private void UpdateTrack()
        {
            // The native batch supports one edited/new drawing or one deletion. A new drawing
            // can be published together with its frame references. Recompile other combinations.
            Drawing[] changed = _target.Drawings.Values.Where(d => !_handles.ContainsKey(d.Id) ||
                !_presented.Drawings.TryGetValue(d.Id, out var old) || old.Revision != d.Revision).ToArray();
            long[] removed = _handles.Keys.Where(id => !_target.Drawings.ContainsKey(id)).ToArray();
            bool remap = !_presented.Frames.SequenceEqual(_target.Frames);
            bool create = changed.Length == 1 && !_handles.ContainsKey(changed[0].Id);
            if (changed.Length + removed.Length > 1 || changed.Length == 1 &&
                (changed[0].Strokes.Length == 0 || remap && !create))
            {
                Unload();
                return;
            }
            if (changed.Length == 0 && removed.Length == 0 && !remap)
            {
                _presented = _target;
                return;
            }

            int doc = _document.DocumentId;
            if (changed.Length != 0)
            {
                var drawing = changed[0];
                if (create)
                {
                    Check(ImmLiveDrawingEdits.CreateDrawing(doc, _layer, out ulong handle));
                    _handles.Add(drawing.Id, handle);
                }
                Check(ImmLiveDrawingEdits.SetDrawingGeometry(doc, _layer, _handles[drawing.Id],
                    drawing.Strokes, drawing.BiggestStroke, _manager.UsesGammaPaint));
            }
            if (remap)
                Check(ImmLiveDrawingEdits.SetFrameSequence(doc, _layer, _target.Frames.Select(id => _handles[id]).ToArray()));
            if (removed.Length != 0)
            {
                Check(ImmLiveDrawingEdits.DestroyDrawing(doc, _layer, _handles[removed[0]]));
                _handles.Remove(removed[0]);
            }
            Check(ImmLiveDrawingEdits.Commit(doc, out _commit));
            _committing = _target;
        }

        private static void Check(int result)
        {
            if (result != 0) throw new InvalidOperationException($"IMM live paint edit failed ({result}).");
        }

        private void Fail(Exception ex)
        {
            Error = ex.Message;
            Debug.LogWarning($"[IMM Live Paint Track] {Error}");
            Unload();
        }

        private void Unload()
        {
            Hide();
            if (_document != null && _manager != null)
                _manager.UnloadDocument(_document);
            _document = null;
            _presented = _installing = _committing = null;
            _handles.Clear();
            _layer = _evaluatedFrame = -1;
            _commit = _seek = 0;
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            _lifetime.Cancel();
            _lifetime.Dispose();
            Unload();
            _target = null;
        }
    }
}
