using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    public sealed partial class ImmLiveEditPanel
    {
        private sealed class TimelineData
        {
            internal double Duration, Offset;
            internal uint FrameRate, Frames, Repeat;
            internal bool IsTimeline;
            internal readonly Dictionary<int, N.Key[]> Keys = new Dictionary<int, N.Key[]>();
            internal ulong[] Drawings = Array.Empty<ulong>();
        }

        private readonly Dictionary<int, TimelineData> timelineData = new Dictionary<int, TimelineData>();
        private readonly HashSet<int> expandedKeyLayers = new HashSet<int>();
        private N.Key? selectedTimelineKey;
        private int selectedKeyLayer = -1;
        private bool draggingKey, snapKeys = true, followPlayhead = true, timelineBodyHasMouse;
        private double draggedKeyTime;
        private int timelineFrameRate = 24;
        private string timelineReadError = "";
        private string drawingIndex = "0";

        private void DrawTimelineFrameTools()
        {
            bool previous = GUI.enabled;
            GUI.enabled &= timelineData.TryGetValue(selected, out TimelineData data) && data.Drawings.Length > 0;
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Insert hold after frame")) ChangeFrameTable(true);
            if (GUILayout.Button("Remove frame")) ChangeFrameTable(false);
            GUILayout.EndHorizontal();
            GUILayout.BeginHorizontal();
            GUILayout.Label("Drawing index", GUILayout.Width(90));
            drawingIndex = GUILayout.TextField(drawingIndex, GUILayout.Width(50));
            if (GUILayout.Button("Use drawing at frame")) Submit("Set frame drawing", () =>
            {
                N.Check(N.ImmAuthoring_DrawingGetHandle(Doc, selected, Integer(drawingIndex), out ulong drawing));
                return N.ImmAuthoring_FrameSetHandle(Doc, selected, Integer(frame), drawing);
            });
            GUILayout.EndHorizontal();
            GUI.enabled = previous;
        }

        private void ChangeFrameTable(bool insertHold)
        {
            Submit(insertHold ? "Insert hold" : "Remove frame", () =>
            {
                if (!timelineData.TryGetValue(selected, out TimelineData data)) throw new InvalidOperationException("Refresh the selected track first.");
                int index = Integer(frame);
                if (index < 0 || index >= data.Drawings.Length) throw new ArgumentOutOfRangeException(nameof(frame));
                var next = new List<ulong>(data.Drawings);
                if (insertHold) next.Insert(index + 1, next[index]); else next.RemoveAt(index);
                return N.ImmAuthoring_FrameSequenceSetHandles(Doc, selected, next.ToArray(), (uint)next.Count);
            });
        }

        private void ReadTimeline()
        {
            timelineData.Clear();
            timelineReadError = "";
            foreach (var layer in layers)
            {
                int result = N.ImmAuthoring_LayerGetTimeline(Doc, layer.Id, out int isTimeline,
                    out double duration, out double offset, out uint fps, out uint frames, out uint repeats);
                if (result != 0) { timelineReadError = $"Some tracks unavailable: {N.Result(result)}"; continue; }
                var data = new TimelineData { IsTimeline = isTimeline != 0, Duration = duration, Offset = offset,
                    FrameRate = fps, Frames = frames, Repeat = repeats };
                foreach (int property in channelIds)
                {
                    result = N.ImmAuthoring_KeyGetCount(Doc, layer.Id, property, out uint count);
                    if (result != 0) { timelineReadError = $"Some keys unavailable: {N.Result(result)}"; continue; }
                    var keys = new N.Key[checked((int)count)];
                    for (uint i = 0; i < count; i++)
                    {
                        keys[i] = new N.Key { size = 80, version = 1 };
                        N.Check(N.ImmAuthoring_KeyGet(Doc, layer.Id, property, i, ref keys[i]));
                    }
                    data.Keys[property] = keys;
                }
                data.Drawings = new ulong[checked((int)frames)];
                for (int i = 0; i < data.Drawings.Length; i++)
                {
                    result = N.ImmAuthoring_FrameGetDrawingHandle(Doc, layer.Id, i, out data.Drawings[i]);
                    if (result != 0) { data.Drawings = Array.Empty<ulong>(); break; }
                }
                timelineData[layer.Id] = data;
            }
            if (selectedTimelineKey.HasValue)
            {
                N.Key selectedKey = selectedTimelineKey.Value;
                selectedTimelineKey = null;
                if (timelineData.TryGetValue(selectedKeyLayer, out TimelineData selectedData) && selectedData.Keys.TryGetValue(selectedKey.property, out N.Key[] selectedKeys))
                    foreach (var key in selectedKeys)
                        if (Math.Abs(key.seconds - selectedKey.seconds) < 1.0 / 12600) selectedTimelineKey = key;
            }
        }

        private double SnapTime(double seconds) => Math.Max(0,
            snapKeys ? Math.Round(seconds * timelineFrameRate) / timelineFrameRate : seconds);

        private void SelectTimelineKey(int layerId, N.Key key)
        {
            selected = layerId;
            Run(ReadSelected);
            selectedKeyLayer = layerId;
            selectedTimelineKey = key;
            keyChannel = Array.IndexOf(channelIds, key.property);
            keyInterpolation = key.interpolation;
            keyTime = key.seconds.ToString("0.######", CultureInfo.InvariantCulture);
            if (key.property == 0) keyVisible = key.boolean != 0;
            else if (key.property == 1) properties.opacity = key.value;
            else if (key.property == 7) repeat = key.boolean != 0 ? "0" : "1";
            else if (key.property == 8) repeat = key.ticks.ToString(CultureInfo.InvariantCulture);
            else if (key.property == 9)
            {
                properties.tx = key.tx; properties.ty = key.ty; properties.tz = key.tz;
                properties.qx = key.qx; properties.qy = key.qy; properties.qz = key.qz; properties.qw = key.qw;
                properties.scale = key.scale; properties.flip = key.flip;
                positionX = key.tx.ToString("R", CultureInfo.InvariantCulture);
                positionY = key.ty.ToString("R", CultureInfo.InvariantCulture);
                positionZ = key.tz.ToString("R", CultureInfo.InvariantCulture);
            }
        }

        private void MoveTimelineKey(double seconds)
        {
            if (!selectedTimelineKey.HasValue || pending != 0 || awaitingCommit) return;
            N.Key key = selectedTimelineKey.Value;
            double original = key.seconds;
            key.seconds = SnapTime(seconds);
            if (Math.Abs(key.seconds - original) < 1.0 / 12600) return;
            if (timelineData.TryGetValue(selectedKeyLayer, out TimelineData data) && data.Keys.TryGetValue(key.property, out N.Key[] keys))
                foreach (var other in keys)
                    if (Math.Abs(other.seconds - key.seconds) < 1.0 / 12600)
                    { Log("A key already exists at that time. Choose another frame."); return; }
            selected = selectedKeyLayer;
            Submit("Move key", () =>
            {
                N.Check(N.ImmAuthoring_KeyRemove(Doc, selectedKeyLayer, key.property, original));
                return N.ImmAuthoring_KeySet(Doc, selectedKeyLayer, ref key);
            });
            selectedTimelineKey = key;
            keyTime = key.seconds.ToString("0.######", CultureInfo.InvariantCulture);
        }

        private void DeleteTimelineKey()
        {
            if (!selectedTimelineKey.HasValue || pending != 0 || awaitingCommit) return;
            N.Key key = selectedTimelineKey.Value;
            selected = selectedKeyLayer;
            Submit("Delete key", () => N.ImmAuthoring_KeyRemove(Doc, selectedKeyLayer, key.property, key.seconds));
            selectedTimelineKey = null;
        }

        private void FitTimeline()
        {
            double end = 1;
            foreach (var row in timelineRows)
                if (timelineData.TryGetValue(row.node.Info.Id, out TimelineData data))
                {
                    if (data.FrameRate > 0) end = Math.Max(end, data.Frames / (double)data.FrameRate);
                    foreach (var keys in data.Keys.Values)
                        foreach (var key in keys) end = Math.Max(end, key.seconds + 1.0 / timelineFrameRate);
                }
            timelineStart = 0; timelineRange = end * 1.05;
        }

        private static bool HasNestedTimelineParent(LayerNode node)
        {
            for (var parentNode = node.Parent; parentNode != null; parentNode = parentNode.Parent)
                if (parentNode.Parent != null && parentNode.Info.IsTimeline) return true;
            return false;
        }

        private bool CanProjectSequence(LayerNode node, TimelineData data)
        {
            if (HasNestedTimelineParent(node)) return false;
            for (var ancestor = node; ancestor != null; ancestor = ancestor.Parent)
                if (timelineData.TryGetValue(ancestor.Info.Id, out TimelineData state))
                {
                    if (state.Keys.TryGetValue(7, out N.Key[] loops) && loops.Length > 0) return false;
                    if (state.Keys.TryGetValue(8, out N.Key[] offsets)) foreach (var key in offsets) if (key.ticks != 0) return false;
                    if (state.Keys.TryGetValue(0, out N.Key[] visibility)) foreach (var key in visibility) if (key.seconds != 0 || key.boolean == 0) return false;
                }
            return true;
        }

        private void DrawSequence(TimelineData data, float y, float namesWidth, float tracksWidth)
        {
            if (data.FrameRate == 0 || data.Drawings.Length == 0) return;
            // Frame sequences repeat in their local clock. Property keys remain
            // in the parent timeline; these lanes do not invent visibility clips.
            int first = Math.Max(0, (int)Math.Min(int.MaxValue - 1, Math.Floor(timelineStart * data.FrameRate)));
            int end = (int)Math.Min(int.MaxValue - 1, Math.Ceiling((timelineStart + timelineRange) * data.FrameRate));
            long available = data.Repeat == 0 ? int.MaxValue : (long)data.Drawings.Length * data.Repeat;
            int last = (int)Math.Min(end, available);
            if (tracksWidth / (timelineRange * data.FrameRate) < 3)
            {
                double a = Math.Max(timelineStart, first / (double)data.FrameRate);
                double b = Math.Min(timelineStart + timelineRange, last / (double)data.FrameRate);
                if (b > a) Fill(new Rect(namesWidth + (float)((a - timelineStart) / timelineRange) * tracksWidth, y + 4,
                    (float)((b - a) / timelineRange) * tracksWidth, RowHeight - 8), new Color(0.25f, 0.44f, 0.65f));
                return;
            }
            for (int i = first; i < last; i++)
            {
                double a = Math.Max(timelineStart, i / (double)data.FrameRate);
                double b = Math.Min(timelineStart + timelineRange, (i + 1) / (double)data.FrameRate);
                float x = namesWidth + (float)((a - timelineStart) / timelineRange) * tracksWidth;
                float width = (float)((b - a) / timelineRange) * tracksWidth;
                int frameIndex = i % data.Drawings.Length;
                bool held = i > 0 && data.Drawings[frameIndex] == data.Drawings[(i - 1) % data.Drawings.Length];
                Fill(new Rect(x, y + 4, width, RowHeight - 8), new Color(0.25f, held ? 0.44f : 0.57f, 0.65f));
                if (!held && width >= 3) Fill(new Rect(x + 1, y + 8, 3, 6), Color.white);
            }
        }

        private void DrawKeys(LayerNode node, int property, float y, float namesWidth, float tracksWidth)
        {
            if (!timelineData.TryGetValue(node.Info.Id, out TimelineData data) || !data.Keys.TryGetValue(property, out N.Key[] keys)) return;
            foreach (var key in keys)
            {
                bool chosen = selectedKeyLayer == node.Info.Id && selectedTimelineKey.HasValue &&
                    selectedTimelineKey.Value.property == property && Math.Abs(selectedTimelineKey.Value.seconds - key.seconds) < 1.0 / 12600;
                double seconds = chosen && draggingKey ? draggedKeyTime : key.seconds;
                if (seconds < timelineStart || seconds > timelineStart + timelineRange) continue;
                float x = namesWidth + (float)((seconds - timelineStart) / timelineRange) * tracksWidth;
                var hit = new Rect(x - 7, y + 3, 14, RowHeight - 6);
                Color color = chosen ? new Color(1, 0.73f, 0.25f) : new Color(0.75f, 0.85f, 0.95f);
                Fill(new Rect(x - 3, y + 8, 7, 7), color);
                GUI.Label(hit, new GUIContent("", $"{key.seconds:0.######} s · {channels[Array.IndexOf(channelIds, property)]}"));
                Event input = Event.current;
                if (timelineBodyHasMouse && attached && pending == 0 && !awaitingCommit && input.type == EventType.MouseDown && input.button == 0 && hit.Contains(input.mousePosition))
                {
                    SelectTimelineKey(node.Info.Id, key);
                    draggingKey = true; draggedKeyTime = key.seconds;
                    scrubControl = GUIUtility.GetControlID(FocusType.Passive); GUIUtility.hotControl = scrubControl;
                    document.Pause(); input.Use();
                }
            }
        }
    }
}
