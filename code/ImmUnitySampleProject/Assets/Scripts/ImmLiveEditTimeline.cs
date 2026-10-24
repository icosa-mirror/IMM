using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    public sealed partial class ImmLiveEditPanel
    {
        private Rect timelineWindow, toolbarWindow;
        private Vector2 toolbarScroll, timelineToolbarScroll;
        private double timelineStart, timelineRange = 5, timelinePlayTime;
        private int scrubControl;
        private long? queuedTimelineSeekTicks;
        private ulong timelineSeekRequest;
        private bool revealTimelinePlayhead;
        private bool pauseBeforeTimelineSeek, timelinePauseRequested;
        private float timelineSeekDeadline;
        private const float RowHeight = 24;
        private readonly List<(LayerNode node, int depth, int property)> timelineRows = new List<(LayerNode, int, int)>();

        private bool OverPanel(Vector2 point) => window.Contains(point) || timelineWindow.Contains(point) || toolbarWindow.Contains(point);

        private void DrawWorkspace()
        {
            if (wrapped == null) wrapped = new GUIStyle(GUI.skin.label) { wordWrap = true };
            if (treeButton == null) treeButton = new GUIStyle(GUI.skin.button) { alignment = TextAnchor.MiddleLeft };
            float timelineHeight = Mathf.Min(Mathf.Clamp(Screen.height * 0.4f, 190, 400), Mathf.Max(90, Screen.height - 210));
            toolbarWindow = new Rect(12, 12, Screen.width - 24, Screen.width < 1050 ? 118 : 100);
            timelineWindow = new Rect(12, Screen.height - timelineHeight - 12, Screen.width - 24, timelineHeight);
            float inspectorTop = toolbarWindow.yMax + 8;
            window = new Rect(12, inspectorTop, Mathf.Min(440, Screen.width - 24), Mathf.Max(60, timelineWindow.y - inspectorTop - 8));
            Fill(toolbarWindow, new Color(0.11f, 0.12f, 0.14f));
            GUI.Box(toolbarWindow, GUIContent.none);
            GUILayout.BeginArea(new Rect(toolbarWindow.x + 8, toolbarWindow.y + 4, toolbarWindow.width - 16, toolbarWindow.height - 8));
            toolbarScroll = GUILayout.BeginScrollView(toolbarScroll, false, false);
            GUILayout.BeginVertical(GUILayout.MinWidth(1000));
            DrawToolbar();
            GUILayout.EndVertical();
            GUILayout.EndScrollView();
            GUILayout.EndArea();
            Fill(window, new Color(0.13f, 0.14f, 0.16f));
            window = GUILayout.Window(GetEntityId().GetHashCode(), window, DrawPanel, "Layer inspector — memory only");
            DrawTimeline();
        }

        private void DrawToolbar()
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label("IMM Live Edit Lab", GUILayout.Width(120));
            file = GUILayout.TextField(file, GUILayout.MinWidth(120));
#if UNITY_EDITOR
            if (GUILayout.Button("Browse", GUILayout.Width(64)))
            {
                string picked = UnityEditor.EditorUtility.OpenFilePanel("Open IMM", "", "imm");
                if (!string.IsNullOrEmpty(picked)) file = picked;
            }
#endif
            GUI.enabled = !loading && pending == 0 && !awaitingCommit;
            if (GUILayout.Button("Load / Reload", GUILayout.Width(100))) BeginLoad();
            GUI.enabled = document != null || loading;
            if (GUILayout.Button("Unload", GUILayout.Width(60))) Unload();
            GUI.enabled = document != null && !loading && pending == 0 && !awaitingCommit;
            if (GUILayout.Button("Frame scene", GUILayout.Width(95))) FrameScene();
            if (GUILayout.Button("Frame selected", GUILayout.Width(105))) FrameSelected();
            GUILayout.EndHorizontal();
            GUILayout.BeginHorizontal();
            GUI.enabled &= IsRootScope && timelineClockValid;
            if (GUILayout.Button("Play", GUILayout.Width(55))) document.Resume();
            if (GUILayout.Button("Pause", GUILayout.Width(55))) document.Pause();
            if (GUILayout.Button("Restart", GUILayout.Width(65))) document.Restart();
            if (GUILayout.Button("< frame", GUILayout.Width(65))) { Run(() => PauseAndSeekTimeline(Math.Max(0, timelinePlayTime - 1.0 / timelineFrameRate))); }
            if (GUILayout.Button("frame >", GUILayout.Width(65))) { Run(() => PauseAndSeekTimeline(timelinePlayTime + 1.0 / timelineFrameRate)); }
            seek = GUILayout.TextField(seek, GUILayout.Width(60));
            if (GUILayout.Button("Seek s", GUILayout.Width(60))) Run(() => SeekTimeline(Number(seek)));
            int index = Array.IndexOf(viewpoints, selectedViewpoint);
            GUI.enabled = document != null && viewpoints.Length > 0;
            if (GUILayout.Button("<", GUILayout.Width(26))) selectedViewpoint = viewpoints[(index - 1 + viewpoints.Length) % viewpoints.Length];
            GUILayout.Label($"View {Mathf.Max(0, index + 1)} / {viewpoints.Length}", GUILayout.Width(90));
            if (GUILayout.Button(">", GUILayout.Width(26))) selectedViewpoint = viewpoints[(index + 1) % viewpoints.Length];
            if (GUILayout.Button("Move to view", GUILayout.Width(100))) MoveToViewpoint();
            GUI.enabled = true;
            GUILayout.Label(status, GUILayout.MinWidth(100));
            GUILayout.EndHorizontal();
            DrawTimelineScopeToolbar();
        }

        private void CollectTimelineRows(LayerNode node, int depth)
        {
            if (!string.IsNullOrEmpty(filter) && !filteredNodes.Contains(node.Info.Id)) return;
            timelineRows.Add((node, depth, -1));
            if (InTimelineScope(node) && expandedKeyLayers.Contains(node.Info.Id) && timelineData.TryGetValue(node.Info.Id, out TimelineData data))
                for (int i = 0; i < channelIds.Length; i++)
                    if ((data.Keys.TryGetValue(channelIds[i], out N.Key[] keys) && keys.Length > 0) || (selected == node.Info.Id && keyChannel == i))
                        timelineRows.Add((node, depth + 1, channelIds[i]));
            if ((!node.Info.IsTimeline || node.Info.Id == timelineScope) &&
                (!string.IsNullOrEmpty(filter) || !collapsedNodes.Contains(node.Info.Id)))
                foreach (var child in node.Children) CollectTimelineRows(child, depth + 1);
        }

        private void DrawTimeline()
        {
            Fill(timelineWindow, new Color(0.11f, 0.12f, 0.14f));
            GUI.Box(timelineWindow, GUIContent.none);
            GUI.BeginGroup(timelineWindow);
            float namesWidth = Mathf.Min(320, timelineWindow.width * 0.32f);
            float tracksWidth = Mathf.Max(50, timelineWindow.width - namesWidth - 20);
            float headerExtra = timelineWindow.width < 1740 ? 20 : 0;
            timelineToolbarScroll = GUI.BeginScrollView(new Rect(0, 0, timelineWindow.width, 30 + headerExtra), timelineToolbarScroll,
                new Rect(0, 0, Math.Max(1740, timelineWindow.width - 1), 28), false, false);
            GUI.Label(new Rect(8, 4, 76, 22), "Timeline");
            GUI.enabled = attached && !loading;
            string nextFilter = GUI.TextField(new Rect(84, 4, Mathf.Max(80, namesWidth - 92), 22), filter);
            if (nextFilter != filter) { filter = nextFilter; FilterTree(); }
            if (GUI.Button(new Rect(namesWidth, 4, 70, 22), "Expand")) collapsedNodes.Clear();
            if (GUI.Button(new Rect(namesWidth + 74, 4, 70, 22), "Collapse"))
                foreach (var pair in treeNodes) if (pair.Value.Children.Count > 0) collapsedNodes.Add(pair.Key);
            if (GUI.Button(new Rect(namesWidth + 148, 4, 70, 22), "Refresh")) Run(Refresh);
            if (GUI.Button(new Rect(namesWidth + 228, 4, 28, 22), "+")) timelineRange = Math.Max(0.125, timelineRange / 2);
            if (GUI.Button(new Rect(namesWidth + 260, 4, 28, 22), "−")) timelineRange = Math.Min(3600, timelineRange * 2);
            if (GUI.Button(new Rect(namesWidth + 296, 4, 28, 22), "<")) timelineStart = Math.Max(0, timelineStart - timelineRange);
            if (GUI.Button(new Rect(namesWidth + 328, 4, 28, 22), ">")) timelineStart += timelineRange;
            if (GUI.Button(new Rect(namesWidth + 364, 4, 50, 22), "Home")) timelineStart = 0;
            GUI.enabled = true;
            double playTime = timelinePlayTime;
            if (followPlayhead && timelineClockValid && (timelineClock.flags & ClockActive) != 0 && document != null && attached && document.IsPlaying() &&
                (playTime < timelineStart || playTime > timelineStart + timelineRange))
                timelineStart = Math.Floor(playTime / timelineRange) * timelineRange;
            if (GUI.Button(new Rect(namesWidth + 420, 4, 52, 22), "Fit")) FitTimeline();
            followPlayhead = GUI.Toggle(new Rect(namesWidth + 480, 4, 75, 22), followPlayhead, "Follow");
            snapKeys = GUI.Toggle(new Rect(namesWidth + 560, 4, 70, 22), snapKeys, "Snap");
            GUI.Label(new Rect(namesWidth + 630, 4, 175, 22), timelineClockValid ? $"{playTime:F3} s / {timelineFrameRate} fps" : "Clock unavailable");
            GUI.enabled = attached && timelineClockValid && (timelineClock.flags & ClockActive) != 0 && pending == 0 && !awaitingCommit &&
                treeNodes.TryGetValue(selected, out LayerNode selectedNode) && InTimelineScope(selectedNode);
            if (GUI.Button(new Rect(namesWidth + 810, 4, 70, 22), "Set key"))
            { keyTime = SnapTime(playTime).ToString("0.######", CultureInfo.InvariantCulture); Submit("Set key", SetKey); }
            GUI.enabled &= selectedTimelineKey.HasValue;
            if (GUI.Button(new Rect(namesWidth + 884, 4, 80, 22), "Delete key")) DeleteTimelineKey();
            GUI.enabled = true;
            DrawTimelineHistory(namesWidth + 972);
            DrawTimelineClipboard(namesWidth + 1144);
            GUI.EndScrollView();
            GUI.BeginGroup(new Rect(0, headerExtra, timelineWindow.width, timelineWindow.height - headerExtra));
            GUI.Label(new Rect(8, 30, namesWidth - 12, 22), $"Layers ({layers.Length}) / drawing frames");
            Fill(new Rect(namesWidth, 30, tracksWidth, 24), new Color(0.12f, 0.13f, 0.15f));
            double step = Math.Pow(10, Math.Floor(Math.Log10(timelineRange / 8)));
            if (timelineRange / step > 16) step *= 5;
            else if (timelineRange / step > 8) step *= 2;
            for (double time = Math.Ceiling(timelineStart / step) * step; time <= timelineStart + timelineRange; time += step)
            {
                float x = namesWidth + (float)((time - timelineStart) / timelineRange) * tracksWidth;
                GUI.Label(new Rect(x + 3, 30, 82, 22), $"{time:0.###} s");
            }
            timelineRows.Clear();
            LayerNode scope = ScopeNode;
            if (scope != null && scope.Parent != null)
                foreach (var child in scope.Children) CollectTimelineRows(child, 0);
            else foreach (var root in treeRoots) CollectTimelineRows(root, 0);
            var body = new Rect(0, 54, timelineWindow.width - 4, Math.Max(1, timelineWindow.height - headerExtra - 78));
            var content = new Rect(0, 0, timelineWindow.width - 22, Math.Max(body.height - 1, timelineRows.Count * RowHeight));
            timelineBodyHasMouse = body.Contains(Event.current.mousePosition);
            layerScroll = GUI.BeginScrollView(body, layerScroll, content, false, false);
            for (int i = 0; i < timelineRows.Count; i++)
            {
                var (node, depth, property) = timelineRows[i];
                float y = i * RowHeight;
                if (y + RowHeight < layerScroll.y || y > layerScroll.y + body.height) continue;
                Fill(new Rect(0, y, content.width, RowHeight), selected == node.Info.Id ? new Color(0.19f, 0.31f, 0.43f) :
                    i % 2 == 0 ? new Color(0.19f, 0.20f, 0.22f) : new Color(0.16f, 0.17f, 0.19f));
                float indent = Math.Min(depth * 16, namesWidth - 130);
                if (property == -1 && GUI.Button(new Rect(6 + indent, y + 2, 20, 20), node.Children.Count > 0 ? (collapsedNodes.Contains(node.Info.Id) ? "▶" : "▼") : expandedKeyLayers.Contains(node.Info.Id) ? "−" : "+"))
                {
                    if (node.Children.Count > 0) { if (!collapsedNodes.Add(node.Info.Id)) collapsedNodes.Remove(node.Info.Id); }
                    else if (!expandedKeyLayers.Add(node.Info.Id)) expandedKeyLayers.Remove(node.Info.Id);
                }
                string name = property >= 0 ? channels[Array.IndexOf(channelIds, property)] : string.IsNullOrEmpty(node.Info.Name) ? "(unnamed)" : node.Info.Name;
                if (GUI.Button(new Rect(30 + indent, y + 2, namesWidth - indent - 36 - (property == -1 && node.Info.PaintNumFrames > 0 ? 60 : 0), 20), new GUIContent(name, node.Info.FullName), treeButton))
                { selected = node.Info.Id; expandedKeyLayers.Add(selected); if (property >= 0) keyChannel = Array.IndexOf(channelIds, property); selectedTimelineKey = null; Run(ReadSelected); }
                for (double time = Math.Ceiling(timelineStart / step) * step; time <= timelineStart + timelineRange; time += step)
                {
                    float x = namesWidth + (float)((time - timelineStart) / timelineRange) * tracksWidth;
                    Fill(new Rect(x, y, 1, RowHeight), new Color(0.27f, 0.28f, 0.30f));
                }
                Event rowInput = Event.current;
                if (timelineBodyHasMouse && rowInput.type == EventType.MouseDown && rowInput.button == 0 &&
                    new Rect(namesWidth, y, tracksWidth, RowHeight).Contains(rowInput.mousePosition))
                {
                    selectedTimelineKey = null;
                    selected = node.Info.Id; expandedKeyLayers.Add(selected);
                    if (property >= 0) keyChannel = Array.IndexOf(channelIds, property);
                    Run(ReadSelected);
                    if (timelineData.TryGetValue(selected, out TimelineData clickedData) && CanProjectSequence(node, clickedData) && clickedData.FrameRate > 0 && clickedData.Drawings.Length > 0)
                    {
                        timelineFrameRate = (int)clickedData.FrameRate;
                        double seconds = timelineStart + Mathf.Clamp01((rowInput.mousePosition.x - namesWidth) / tracksWidth) * timelineRange;
                        frame = ((int)(seconds * clickedData.FrameRate) % clickedData.Drawings.Length).ToString(CultureInfo.InvariantCulture);
                    }
                }
                if (property >= 0) DrawKeys(node, property, y, namesWidth, tracksWidth);
                else if (timelineData.TryGetValue(node.Info.Id, out TimelineData data))
                {
                    if (CanProjectSequence(node, data)) DrawSequence(data, y, namesWidth, tracksWidth);
                    else if (data.Frames > 0) GUI.Label(new Rect(namesWidth + 8, y + 2, tracksWidth - 12, 20), "Local sequence — clock mapping pending");
                    string detail = data.Frames > 0 ? $"{data.Frames}f @ {data.FrameRate} fps · {(data.Repeat == 0 ? "loop" : $"repeat {data.Repeat}")}" : node.Info.Type.ToString();
                    if (data.Frames == 0) GUI.Label(new Rect(namesWidth + 8, y + 2, 270, 20), detail);
                    else GUI.Label(new Rect(namesWidth - 60, y + 2, 54, 20), new GUIContent($"{data.Frames}f", detail));
                }
            }
            GUI.EndScrollView();
            if (timelineClockValid && (timelineClock.flags & ClockActive) != 0 && playTime >= timelineStart && playTime <= timelineStart + timelineRange)
            {
                float x = namesWidth + (float)((playTime - timelineStart) / timelineRange) * tracksWidth;
                Fill(new Rect(x, 30, 2, timelineWindow.height - headerExtra - 54), new Color(1, 0.36f, 0.25f));
            }
            GUI.Label(new Rect(8, timelineWindow.height - headerExtra - 22, timelineWindow.width - 16, 20),
                $"Ruler: scrub · Squares: select/drag keys · Blue: drawing/hold preview (white marks changes; complex local clocks pending) · Range {timelineStart:0.###}–{timelineStart + timelineRange:0.###} s. {timelineReadError}");
            var scrubArea = new Rect(namesWidth, 30, tracksWidth, timelineWindow.height - headerExtra - 54);
            int control = GUIUtility.GetControlID(FocusType.Passive);
            Event input = Event.current;
            if (document != null && !loading && IsRootScope && input.type == EventType.MouseDown && input.button == 0 && scrubArea.Contains(input.mousePosition))
            { GUI.FocusControl(null); scrubControl = control; GUIUtility.hotControl = control; ScrubAt(input.mousePosition.x, namesWidth, tracksWidth); input.Use(); }
            else if (scrubControl != 0 && GUIUtility.hotControl == scrubControl)
            {
                if (input.type == EventType.MouseDrag)
                {
                    if (draggingKey) draggedKeyTime = SnapTime(timelineStart + Mathf.Clamp01((input.mousePosition.x - namesWidth) / tracksWidth) * timelineRange);
                    else ScrubAt(input.mousePosition.x, namesWidth, tracksWidth);
                    input.Use();
                }
                else if (input.type == EventType.MouseUp)
                {
                    if (draggingKey) MoveTimelineKey(draggedKeyTime);
                    ReleaseTimelineCapture(); input.Use();
                }
            }
            GUI.EndGroup();
            GUI.EndGroup();
        }

        private void ReleaseTimelineCapture()
        {
            if (scrubControl != 0 && GUIUtility.hotControl == scrubControl) GUIUtility.hotControl = 0;
            scrubControl = 0; draggingKey = false;
        }

        private static void Fill(Rect rect, Color color)
        {
            Color previous = GUI.color; GUI.color = color;
            GUI.DrawTexture(rect, Texture2D.whiteTexture); GUI.color = previous;
        }

        private void ScrubAt(float x, float namesWidth, float tracksWidth) =>
            Run(() => PauseAndSeekTimeline(SnapTime(timelineStart + Mathf.Clamp01((x - namesWidth) / tracksWidth) * timelineRange)));

        private void PauseAndSeekTimeline(double seconds)
        {
            SeekTimeline(seconds);
            pauseBeforeTimelineSeek = true;
        }

        private void UpdateTimelinePlayback()
        {
            if (timelineSeekRequest != 0 && attached && !loading && document != null &&
                document.TryGetSpawnAreaSnapshot(null, out SpawnAreaSnapshot snapshot))
            {
                if (snapshot.IsComplete(timelineSeekRequest))
                { timelineSeekRequest = 0; revealTimelinePlayhead = true; }
                else if (snapshot.requestedCommand != timelineSeekRequest || Time.unscaledTime > timelineSeekDeadline)
                    timelineSeekRequest = 0;
            }
            if (!queuedTimelineSeekTicks.HasValue || !attached || loading || document == null || pending != 0 || awaitingCommit) return;
            if (Time.unscaledTime > timelineSeekDeadline)
            {
                queuedTimelineSeekTicks = null; pauseBeforeTimelineSeek = false; timelinePauseRequested = false;
                Log("Seek could not be accepted while playback was busy. Retry when ready.");
                return;
            }
            if (pauseBeforeTimelineSeek && document.IsPlaying())
            {
                if (!timelinePauseRequested) { document.Pause(); timelinePauseRequested = true; }
                return;
            }
            ulong request = document.RequestSetTime(queuedTimelineSeekTicks.Value, 0);
            if (request == 0) return;
            timelineSeekRequest = request;
            queuedTimelineSeekTicks = null; pauseBeforeTimelineSeek = false; timelinePauseRequested = false;
        }

        private void SeekTimeline(double seconds)
        {
            if (document == null || double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0)
                throw new ArgumentException("Seek time must be finite and nonnegative.");
            if (!IsRootScope) throw new InvalidOperationException("Local playback preview is not available yet. Use the main timeline for scene playback.");
            long ticks = checked((long)Math.Round(seconds * 12600, MidpointRounding.AwayFromZero));
            if (!timelineClockValid) throw new InvalidOperationException("Wait for the timeline clock to become ready.");
            queuedTimelineSeekTicks = RootOccurrenceTick(timelineClock, ticks);
            timelineSeekDeadline = Time.unscaledTime + 5;
            seek = seconds.ToString("0.###", CultureInfo.InvariantCulture);
            keyTime = (ticks / 12600.0).ToString("R", CultureInfo.InvariantCulture);
        }
    }
}
