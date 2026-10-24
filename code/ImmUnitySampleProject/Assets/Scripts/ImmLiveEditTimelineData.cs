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
        private N.Key? clipboardTimelineKey;
        private int selectedKeyLayer = -1;
        private bool draggingKey, snapKeys = true, followPlayhead = true, timelineBodyHasMouse;
        private double draggedKeyTime;
        private int timelineFrameRate = 24;
        private string timelineReadError = "";
        private string drawingIndex = "0";
        private string rotationX = "0", rotationY = "0", rotationZ = "0", transformScale = "1", keyOffset = "0";
        private bool rotationEdited, keyLoop = true;
        private static readonly string[] ReflectionNames = { "None", "X", "Y", "Z" };

        private void SyncTransformFields()
        {
            positionX = properties.tx.ToString("R", CultureInfo.InvariantCulture);
            positionY = properties.ty.ToString("R", CultureInfo.InvariantCulture);
            positionZ = properties.tz.ToString("R", CultureInfo.InvariantCulture);
            var angles = new Quaternion(properties.qx, properties.qy, properties.qz, properties.qw).eulerAngles;
            rotationX = angles.x.ToString("0.###", CultureInfo.InvariantCulture);
            rotationY = angles.y.ToString("0.###", CultureInfo.InvariantCulture);
            rotationZ = angles.z.ToString("0.###", CultureInfo.InvariantCulture);
            transformScale = properties.scale.ToString("R", CultureInfo.InvariantCulture);
            rotationEdited = false;
        }

        private void DrawTransformFields()
        {
            GUILayout.Label(new GUIContent("Local rotation X / Y / Z (degrees)", "Applied in ZXY order using IMM local axes."));
            GUILayout.BeginHorizontal();
            string x = GUILayout.TextField(rotationX), y = GUILayout.TextField(rotationY), z = GUILayout.TextField(rotationZ);
            rotationEdited |= x != rotationX || y != rotationY || z != rotationZ;
            rotationX = x; rotationY = y; rotationZ = z;
            GUILayout.EndHorizontal();
            GUILayout.BeginHorizontal();
            GUILayout.Label("Scale", GUILayout.Width(40));
            transformScale = GUILayout.TextField(transformScale, GUILayout.Width(90));
            GUILayout.Label("Reflection", GUILayout.Width(65));
            properties.flip = GUILayout.SelectionGrid(properties.flip, ReflectionNames, 4);
            GUILayout.EndHorizontal();
        }

        private void ReadTransformFields()
        {
            ReadPositionFields();
            properties.scale = (float)Number(transformScale);
            if (!rotationEdited) return;
            var angles = new Vector3((float)Number(rotationX), (float)Number(rotationY), (float)Number(rotationZ));
            if (!float.IsFinite(angles.x) || !float.IsFinite(angles.y) || !float.IsFinite(angles.z))
                throw new ArgumentException("Rotation must be finite.");
            Quaternion rotation = Quaternion.Euler(angles);
            properties.qx = rotation.x; properties.qy = rotation.y; properties.qz = rotation.z; properties.qw = rotation.w;
        }

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
                RecordTimelineBefore(selected, -1);
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
                RecordTimelineBefore(selected, -1);
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
            else if (key.property == 7) keyLoop = key.boolean != 0;
            else if (key.property == 8) keyOffset = key.ticks.ToString(CultureInfo.InvariantCulture);
            else if (key.property == 9)
            {
                properties.tx = key.tx; properties.ty = key.ty; properties.tz = key.tz;
                properties.qx = key.qx; properties.qy = key.qy; properties.qz = key.qz; properties.qw = key.qw;
                properties.scale = key.scale; properties.flip = key.flip;
                SyncTransformFields();
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
                RecordTimelineBefore(selectedKeyLayer, key.property);
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
            Submit("Delete key", () =>
            { RecordTimelineBefore(selectedKeyLayer, key.property); return N.ImmAuthoring_KeyRemove(Doc, selectedKeyLayer, key.property, key.seconds); });
            selectedTimelineKey = null;
        }

        private void CopyTimelineKey(bool cut)
        {
            if (!selectedTimelineKey.HasValue || pending != 0 || awaitingCommit || TimelinePreviewActive) return;
            clipboardTimelineKey = selectedTimelineKey.Value;
            if (cut) DeleteTimelineKey();
            else Log("Key copied. Select a layer and paste at the playhead.");
        }

        private bool CanPasteTimelineKey => attached && !TimelinePreviewActive && !loading && pending == 0 && !awaitingCommit &&
            clipboardTimelineKey.HasValue && timelineClockValid && (timelineClock.flags & ClockActive) != 0 &&
            treeNodes.TryGetValue(selected, out LayerNode pasteNode) && InTimelineScope(pasteNode);

        private void PasteTimelineKey()
        {
            if (!CanPasteTimelineKey) return;
            N.Key key = clipboardTimelineKey.Value;
            key.seconds = SnapTime(timelinePlayTime);
            if (timelineData.TryGetValue(selected, out TimelineData data) && data.Keys.TryGetValue(key.property, out N.Key[] keys))
                foreach (N.Key existing in keys)
                    if (Math.Abs(existing.seconds - key.seconds) < 0.5 / 12600)
                    { Log("A key already exists at the playhead. Move the playhead before pasting."); return; }
            int layer = selected;
            Submit("Paste key", () =>
            {
                RecordTimelineBefore(layer, key.property);
                return N.ImmAuthoring_KeySet(Doc, layer, ref key);
            });
            if (pending == 0 && !awaitingCommit) return;
            selectedKeyLayer = layer; selectedTimelineKey = key;
            keyChannel = Array.IndexOf(channelIds, key.property);
            keyTime = key.seconds.ToString("R", CultureInfo.InvariantCulture);
            expandedKeyLayers.Add(layer);
        }

        private void DrawTimelineClipboard(float x)
        {
            bool previous = GUI.enabled;
            GUI.enabled = attached && !TimelinePreviewActive && pending == 0 && !awaitingCommit && selectedTimelineKey.HasValue;
            if (GUI.Button(new Rect(x, 4, 60, 22), new GUIContent("Copy", "Copy selected key (Ctrl/Cmd+C)"))) Run(() => CopyTimelineKey(false));
            if (GUI.Button(new Rect(x + 64, 4, 60, 22), new GUIContent("Cut", "Cut selected key (Ctrl/Cmd+X)"))) Run(() => CopyTimelineKey(true));
            GUI.enabled = CanPasteTimelineKey;
            if (GUI.Button(new Rect(x + 128, 4, 60, 22), new GUIContent("Paste", "Paste at the playhead on the selected layer (Ctrl/Cmd+V)"))) Run(PasteTimelineKey);
            GUI.enabled = previous;
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
            if (!InTimelineScope(node)) return false;
            for (var ancestor = node; ancestor != null && ancestor.Info.Id != timelineScope; ancestor = ancestor.Parent)
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
                    GUI.FocusControl(null);
                    SelectTimelineKey(node.Info.Id, key);
                    if (TimelinePreviewActive) { input.Use(); continue; }
                    draggingKey = true; draggedKeyTime = key.seconds;
                    scrubControl = GUIUtility.GetControlID(FocusType.Passive); GUIUtility.hotControl = scrubControl;
                    document.Pause(); input.Use();
                }
            }
        }
    }
}
