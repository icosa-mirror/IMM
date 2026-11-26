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
            internal int[] RecoloredStrokes { get; }
            internal long RevisionBeforeRecolor { get; }

            private Drawing(Drawing previous, long revision, int[] indices, ImmLiveDrawingEdits.Stroke[] strokes)
                : this(previous.Id, revision, strokes, previous.BiggestStroke)
            {
                RecoloredStrokes = indices;
                RevisionBeforeRecolor = previous.Revision;
            }

            /// <summary>Replace one stroke. When its geometry matches, the player can update only its colors.</summary>
            public static Drawing WithRecoloredStroke(Drawing previous, long revision, int index,
                ImmLiveDrawingEdits.Stroke replacement)
            {
                if (previous == null) throw new ArgumentNullException(nameof(previous));
                if (index < 0 || index >= previous.Strokes.Length) throw new ArgumentOutOfRangeException(nameof(index));
                var strokes = (ImmLiveDrawingEdits.Stroke[])previous.Strokes.Clone();
                var old = strokes[index];
                strokes[index] = replacement;
                return SameGeometry(old, replacement)
                    ? new Drawing(previous, revision, new[] { index }, strokes)
                    : new Drawing(previous.Id, revision, strokes, previous.BiggestStroke);
            }

            /// <summary>Replace several strokes. Replacements with matching geometry can be presented as one color edit.</summary>
            public static Drawing WithRecoloredStrokes(Drawing previous, long revision,
                IReadOnlyDictionary<int, ImmLiveDrawingEdits.Stroke> replacements)
            {
                if (previous == null) throw new ArgumentNullException(nameof(previous));
                if (replacements == null) throw new ArgumentNullException(nameof(replacements));
                if (replacements.Count == 0) throw new ArgumentException("Specify at least one stroke.", nameof(replacements));
                var strokes = (ImmLiveDrawingEdits.Stroke[])previous.Strokes.Clone();
                var indices = new int[replacements.Count];
                int count = 0;
                bool colorOnly = true;
                foreach (var replacement in replacements)
                {
                    int index = replacement.Key;
                    if (index < 0 || index >= strokes.Length) throw new ArgumentOutOfRangeException(nameof(replacements));
                    colorOnly &= SameGeometry(strokes[index], replacement.Value);
                    strokes[index] = replacement.Value;
                    indices[count++] = index;
                }
                return colorOnly
                    ? new Drawing(previous, revision, indices, strokes)
                    : new Drawing(previous.Id, revision, strokes, previous.BiggestStroke);
            }

            private static bool SameGeometry(ImmLiveDrawingEdits.Stroke old, ImmLiveDrawingEdits.Stroke replacement)
            {
                if (old.Brush != replacement.Brush || old.Visibility != replacement.Visibility ||
                    old.Points == null || replacement.Points == null || old.Points.Length != replacement.Points.Length)
                    return false;
                for (int i = 0; i < old.Points.Length; ++i)
                {
                    var a = old.Points[i];
                    var b = replacement.Points[i];
                    if (!a.Position.Equals(b.Position) || !a.Normal.Equals(b.Normal) ||
                        !a.Direction.Equals(b.Direction) || a.Width != b.Width || a.Length != b.Length || a.Time != b.Time)
                        return false;
                }
                return true;
            }

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
        private ulong _commit, _seek, _nativePresented;
        private bool _committingColors, _disableStrokeColors;
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
                    {
                        if (!_committingColors)
                            throw new InvalidOperationException($"IMM rejected paint edit {_commit} ({status.Result}).");
                        _disableStrokeColors = true;
                    }
                    else
                    {
                        if (status.State != ImmLiveDrawingEdits.CommitState.Presented) return;
                        _presented = _committing;
                        _nativePresented = _commit;
                    }
                    _committing = null;
                    _commit = 0;
                    _committingColors = false;
                }
                if (_presented != _target) UpdateTrack();
            }
            catch (Exception ex)
            {
                Fail(ex);
            }
        }

        /// <summary>Request the host's current frame before the player's LateUpdate processes seeks.
        /// Returns true once its drawing is evaluated at the current track revision.</summary>
        public bool PrepareFrame(int frame)
        {
            Pump();
            if (_disposed || _target == null || _presented != _target || Error != null ||
                frame < 0 || frame >= _target.Frames.Length || _seek != 0)
                return false;

            // Held exposures in this single static paint layer share the evaluated drawing.
            if (_evaluatedFrame >= 0 && _presented.Frames[_evaluatedFrame] == _presented.Frames[frame])
                return true;

            try
            {
                // Seek inside the frame so native tick rounding cannot select its predecessor.
                long ticks = (2L * frame + 1) * ExportLayerTiming.TicksPerSecond / (2L * _fps);
                _seek = _document.RequestSetTime(ticks, 0);
                _seekFrame = frame;
                _evaluatedFrame = -1;
            }
            catch (Exception ex)
            {
                Fail(ex);
            }
            return false;
        }

        /// <summary>Show the evaluated drawing, or keep the layer hidden while the host renders its fallback.
        /// A host that only calls this method can still start a seek here.</summary>
        public bool TryPresent(int frame, Matrix4x4 documentToWorld)
        {
            if (!PrepareFrame(frame))
            {
                Hide();
                return false;
            }
            try
            {
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
                ulong handleToEdit = _handles[drawing.Id];
                int[] strokes = drawing.RecoloredStrokes;
                _committingColors = !_disableStrokeColors && !create && !remap && strokes != null &&
                    _presented.Drawings[drawing.Id].Revision == drawing.RevisionBeforeRecolor &&
                    (strokes.Length == 1
                        ? ImmLiveDrawingEdits.TrySetStrokeColors(doc, _layer, handleToEdit, _nativePresented,
                            strokes[0], drawing.Strokes[strokes[0]].Points)
                        : ImmLiveDrawingEdits.TrySetStrokeColorBatch(doc, _layer, handleToEdit, _nativePresented,
                            strokes, drawing.Strokes));
                if (!_committingColors)
                    Check(ImmLiveDrawingEdits.SetDrawingGeometry(doc, _layer, handleToEdit,
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
            // Replacing geometry preserves the selected drawing handle. Structural edits
            // can change what the last seek selected and must be evaluated again.
            if (remap || create || removed.Length != 0) _evaluatedFrame = -1;
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
            _commit = _seek = _nativePresented = 0;
            _committingColors = _disableStrokeColors = false;
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
