using System;
using System.Collections.Generic;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    public sealed partial class ImmLiveEditPanel
    {
        private sealed class TimelineState
        {
            internal N.Key[] Keys = Array.Empty<N.Key>();
            internal ulong[] Frames = Array.Empty<ulong>();
            internal uint Repeat;
            internal long Bytes => Keys.LongLength * 80 + Frames.LongLength * sizeof(ulong) + sizeof(uint);
        }

        private sealed class TimelineEdit
        {
            internal string Label;
            internal int Layer, Property;
            internal TimelineState Before, After;
            internal long Bytes => Before.Bytes + After.Bytes;
        }

        private readonly List<TimelineEdit> timelineUndo = new List<TimelineEdit>();
        private readonly List<TimelineEdit> timelineRedo = new List<TimelineEdit>();
        private TimelineEdit historyRecording, historyReplay;
        private bool replayIsUndo;
        private ulong historyPresented;
        private string historyNotice = "";
        // A frame table may contain one million handles. Bound retained snapshots,
        // as well as the number of user actions, rather than retaining 64 huge tables.
        private const long HistoryByteLimit = 32 * 1024 * 1024;
        private const int HistoryEntryLimit = 64;

        private N.Revisions HistoryRevisions()
        {
            var value = new N.Revisions { size = 32, version = 1 };
            N.Check(N.ImmAuthoring_GetRevisions(Doc, ref value));
            return value;
        }

        private bool HistoryReady(N.Revisions value)
        {
            if (value.requested == value.presented) return true;
            var state = N.Status.Empty;
            N.Check(N.ImmAuthoring_GetCommitStatus(Doc, value.requested, ref state));
            // A rejected request leaves the requested counter ahead permanently,
            // but has not changed the published track and must not poison undo.
            return state.state == 5;
        }

        private TimelineState CaptureTimelineState(int layer, int property)
        {
            N.Revisions before = HistoryRevisions();
            if (!HistoryReady(before))
                throw new InvalidOperationException("Wait for the current edit to finish before changing this track.");
            var state = new TimelineState();
            if (property == -1)
            {
                N.Check(N.ImmAuthoring_LayerGetTimeline(Doc, layer, out _, out _, out _, out _, out uint count, out _));
                state.Frames = new ulong[checked((int)count)];
                for (int i = 0; i < state.Frames.Length; i++)
                    N.Check(N.ImmAuthoring_FrameGetDrawingHandle(Doc, layer, i, out state.Frames[i]));
            }
            else if (property == -2)
            {
                N.Check(N.ImmAuthoring_LayerGetTimeline(Doc, layer, out _, out _, out _, out _, out _, out state.Repeat));
            }
            else
            {
                N.Check(N.ImmAuthoring_KeyGetCount(Doc, layer, property, out uint count));
                state.Keys = new N.Key[checked((int)count)];
                for (uint i = 0; i < count; i++)
                {
                    state.Keys[i] = new N.Key { size = 80, version = 1 };
                    N.Check(N.ImmAuthoring_KeyGet(Doc, layer, property, i, ref state.Keys[i]));
                }
            }
            N.Revisions after = HistoryRevisions();
            if (before.presented != after.presented || !HistoryReady(after))
                throw new InvalidOperationException("The document changed while reading this track. Refresh and retry.");
            return state;
        }

        private void RecordTimelineBefore(int layer, int property)
        {
            if (historyReplay != null || historyRecording != null) return;
            TimelineState state = CaptureTimelineState(layer, property);
            if ((timelineUndo.Count > 0 || timelineRedo.Count > 0) && HistoryRevisions().presented != historyPresented)
                ClearTimelineHistory("History cleared because the document changed elsewhere.");
            historyRecording = new TimelineEdit { Layer = layer, Property = property, Before = state };
        }

        private void ClearTimelineHistory(string reason = "")
        {
            timelineUndo.Clear(); timelineRedo.Clear();
            historyRecording = historyReplay = null;
            historyPresented = revisions.presented;
            historyNotice = reason;
        }

        private void AbandonHistoryRequest()
        {
            historyRecording = historyReplay = null;
        }

        private void FinishTimelineHistory(bool presented)
        {
            try { CompleteTimelineHistory(presented); }
            catch (Exception error) { ClearTimelineHistory($"History unavailable: {error.Message}"); }
        }

        private void CompleteTimelineHistory(bool presented)
        {
            if (!presented) { AbandonHistoryRequest(); return; }
            if (historyReplay != null)
            {
                TimelineEdit entry = historyReplay;
                TimelineState expected = replayIsUndo ? entry.Before : entry.After;
                if (!SameTimelineState(CaptureTimelineState(entry.Layer, entry.Property), expected))
                { ClearTimelineHistory("History reset: the published track differs from the requested state."); return; }
                var from = replayIsUndo ? timelineUndo : timelineRedo;
                var to = replayIsUndo ? timelineRedo : timelineUndo;
                from.RemoveAt(from.Count - 1); to.Add(entry);
                historyReplay = null; historyPresented = revisions.presented; historyNotice = "";
                return;
            }
            if (historyRecording == null)
            { ClearTimelineHistory("History cleared after another document edit."); return; }
            TimelineEdit recorded = historyRecording;
            historyRecording = null;
            recorded.Label = awaitingCommitLabel;
            recorded.After = CaptureTimelineState(recorded.Layer, recorded.Property);
            historyPresented = revisions.presented;
            if (SameTimelineState(recorded.Before, recorded.After)) return;
            timelineRedo.Clear();
            if (recorded.Bytes > HistoryByteLimit)
            { ClearTimelineHistory("This edit exceeds the undo memory limit; history was cleared."); return; }
            timelineUndo.Add(recorded); historyNotice = "";
            long bytes = 0;
            foreach (var entry in timelineUndo) bytes += entry.Bytes;
            while (timelineUndo.Count > HistoryEntryLimit || bytes > HistoryByteLimit)
            { bytes -= timelineUndo[0].Bytes; timelineUndo.RemoveAt(0); }
        }

        private static bool SameTimelineState(TimelineState a, TimelineState b)
        {
            if (a.Repeat != b.Repeat || a.Frames.Length != b.Frames.Length || a.Keys.Length != b.Keys.Length) return false;
            for (int i = 0; i < a.Frames.Length; i++) if (a.Frames[i] != b.Frames[i]) return false;
            for (int i = 0; i < a.Keys.Length; i++)
            {
                N.Key x = a.Keys[i], y = b.Keys[i];
                if (!SameTimelineKey(x, y)) return false;
            }
            return true;
        }

        private static bool SameTimelineKey(N.Key x, N.Key y) =>
            Math.Abs(x.seconds - y.seconds) <= 0.5 / 12600 && x.property == y.property && x.interpolation == y.interpolation &&
            x.boolean == y.boolean && x.value == y.value && x.doubleValue == y.doubleValue && x.ticks == y.ticks && x.flip == y.flip &&
            x.tx == y.tx && x.ty == y.ty && x.tz == y.tz && x.qx == y.qx && x.qy == y.qy && x.qz == y.qz &&
            x.qw == y.qw && x.scale == y.scale;

        private static bool ContainsTimelineKey(N.Key[] keys, N.Key value)
        {
            foreach (N.Key key in keys) if (SameTimelineKey(key, value)) return true;
            return false;
        }

        private void ReplayTimelineHistory(bool undo)
        {
            if (!attached || pending != 0 || awaitingCommit || historyReplay != null) return;
            var from = undo ? timelineUndo : timelineRedo;
            if (from.Count == 0) return;
            N.Revisions currentRevision = HistoryRevisions();
            if (currentRevision.presented != historyPresented)
            { ClearTimelineHistory("History cleared because the document changed elsewhere."); Run(Refresh); return; }
            if (!HistoryReady(currentRevision))
            { Log("Wait for the current edit to finish before undoing or redoing."); return; }
            TimelineEdit entry = from[from.Count - 1];
            TimelineState current = CaptureTimelineState(entry.Layer, entry.Property);
            if (!SameTimelineState(current, undo ? entry.After : entry.Before))
            { ClearTimelineHistory("History cleared because this track changed elsewhere."); Run(Refresh); return; }
            TimelineState target = undo ? entry.Before : entry.After;
            // Imported unsupported Action keys can remain unchanged on a track.
            // Replay only changed identities/values, never remove/reinsert those keys.
            var removeKeys = new List<N.Key>();
            var setKeys = new List<N.Key>();
            foreach (N.Key key in current.Keys)
                if (entry.Property != 6 || !ContainsTimelineKey(target.Keys, key)) removeKeys.Add(key);
            foreach (N.Key key in target.Keys)
                if (entry.Property != 6 || !ContainsTimelineKey(current.Keys, key)) setKeys.Add(key);
            if (entry.Property >= 0)
            {
                foreach (N.Key key in removeKeys)
                    if (!CanEditTimelineKey(entry.Layer, key)) { Log("This imported key is read-only on this layer."); return; }
                foreach (N.Key key in setKeys)
                    if (!CanEditTimelineKey(entry.Layer, key)) { Log("This imported key is read-only on this layer."); return; }
            }
            historyReplay = entry; replayIsUndo = undo;
            RevealTimelineLayer(entry.Layer);
            selected = entry.Layer;
            Submit($"{(undo ? "Undo" : "Redo")} {entry.Label}", () =>
            {
                if (entry.Property == -1)
                    return N.ImmAuthoring_FrameSequenceSetHandles(Doc, entry.Layer, target.Frames, (uint)target.Frames.Length);
                if (entry.Property == -2) return N.ImmAuthoring_PaintSetMaxRepeatCount(Doc, entry.Layer, target.Repeat);
                foreach (var key in removeKeys)
                    N.Check(N.ImmAuthoring_KeyRemove(Doc, entry.Layer, entry.Property, key.seconds));
                foreach (var value in setKeys)
                {
                    N.Key key = value;
                    N.Check(N.ImmAuthoring_KeySet(Doc, entry.Layer, ref key));
                }
                return 0;
            });
        }

        private void DrawTimelineHistory(float x)
        {
            bool previous = GUI.enabled;
            bool ready = attached && pending == 0 && !awaitingCommit && scrubControl == 0;
            GUI.enabled = ready && timelineUndo.Count > 0;
            string undoTip = timelineUndo.Count > 0 ? $"Undo {timelineUndo[timelineUndo.Count - 1].Label}" : historyNotice;
            if (GUI.Button(new Rect(x, 4, 80, 22), new GUIContent($"Undo ({timelineUndo.Count})", undoTip))) Run(() => ReplayTimelineHistory(true));
            GUI.enabled = ready && timelineRedo.Count > 0;
            string redoTip = timelineRedo.Count > 0 ? $"Redo {timelineRedo[timelineRedo.Count - 1].Label}" : historyNotice;
            if (GUI.Button(new Rect(x + 84, 4, 80, 22), new GUIContent($"Redo ({timelineRedo.Count})", redoTip))) Run(() => ReplayTimelineHistory(false));
            GUI.enabled = previous;
        }
    }
}
