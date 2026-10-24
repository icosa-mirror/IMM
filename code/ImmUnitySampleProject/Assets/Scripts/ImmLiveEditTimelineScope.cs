using System;
using System.Collections.Generic;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    public sealed partial class ImmLiveEditPanel
    {
        private const uint ClockRoot = 1, ClockPlaying = 2, ClockVisible = 4, ClockHeld = 8,
            ClockActionHeld = 16, ClockClamped = 32, ClockActive = 64, ClockInactive = 128;
        private int timelineScope = -1;
        private N.TimelineClock timelineClock;
        private bool timelineClockValid, timelineClockMissing;
        private string timelineClockError = "";
        private sealed class TimelineView
        {
            internal double Start, Range;
            internal Vector2 Scroll;
            internal string Filter;
        }
        private readonly Dictionary<int, TimelineView> timelineViews = new Dictionary<int, TimelineView>();

        private LayerNode ScopeNode => treeNodes.TryGetValue(timelineScope, out LayerNode node) ? node : null;
        private bool IsRootScope => ScopeNode != null && ScopeNode.Parent == null;

        private void EnsureTimelineScope()
        {
            LayerNode current = ScopeNode;
            if (current != null && (current.Parent == null || current.Info.IsTimeline)) return;
            timelineScope = treeRoots.Count > 0 ? treeRoots[0].Info.Id : -1;
            timelineClockValid = false;
        }

        private void UpdateTimelineClock()
        {
            if (!attached || loading || document == null || timelineClockMissing)
            { timelineClockValid = false; return; }
            EnsureTimelineScope();
            if (timelineScope < 0) return;
            try
            {
                var value = new N.TimelineClock();
                int result = N.ImmAuthoring_TimelineGetClock(Doc, timelineScope, ref value, 96);
                if (result != 0)
                {
                    timelineClockError = $"Clock: {N.Result(result)}";
                    timelineClockValid = false;
                    return;
                }
                timelineClock = value; timelineClockValid = true; timelineClockError = "";
                timelinePlayTime = value.local / 12600.0;
                if (revealTimelinePlayhead)
                {
                    if (followPlayhead && (value.flags & ClockActive) != 0 &&
                        (timelinePlayTime < timelineStart || timelinePlayTime > timelineStart + timelineRange))
                        timelineStart = Math.Floor(timelinePlayTime / timelineRange) * timelineRange;
                    revealTimelinePlayhead = false;
                }
            }
            catch (EntryPointNotFoundException)
            {
                timelineClockMissing = true; timelineClockValid = false;
                timelineClockError = "Reload the matching native plugin to use timeline clocks.";
                Log(timelineClockError);
            }
        }

        private bool InTimelineScope(LayerNode node)
        {
            if (node.Info.Id == timelineScope) return IsRootScope;
            for (var ancestor = node.Parent; ancestor != null; ancestor = ancestor.Parent)
            {
                if (ancestor.Info.Id == timelineScope) return true;
                if (ancestor.Info.IsTimeline) return false;
            }
            return false;
        }

        private static long RootOccurrenceTick(N.TimelineClock clock, long localTick)
        {
            if (clock.duration <= 0) return localTick;
            ulong cycle = clock.cycle;
            // A finite end has a raw elapsed cycle beyond its last occurrence.
            // Rewind within the last valid cycle, not from the clamped pose delta.
            if ((clock.flags & ClockClamped) != 0 && clock.effectiveRepeat > 0) cycle = clock.effectiveRepeat - 1;
            return checked(checked((long)cycle * clock.duration) + localTick);
        }

        private void OpenTimelineScope(int layer)
        {
            if (pending != 0 || awaitingCommit || !treeNodes.TryGetValue(layer, out LayerNode node) ||
                (node.Parent != null && !node.Info.IsTimeline)) return;
            if (layer == timelineScope) return;
            if (ScopeNode != null) timelineViews[timelineScope] = new TimelineView
                { Start = timelineStart, Range = timelineRange, Scroll = layerScroll, Filter = filter };
            ReleaseTimelineCapture();
            timelineScope = layer; timelineClockValid = false; timelineClock = default; timelinePlayTime = 0; queuedTimelineSeekTicks = null;
            timelineSeekRequest = 0; revealTimelinePlayhead = false;
            selected = layer; selectedTimelineKey = null;
            if (timelineViews.TryGetValue(layer, out TimelineView saved))
            { timelineStart = saved.Start; timelineRange = saved.Range; layerScroll = saved.Scroll; filter = saved.Filter; }
            else
            {
                timelineStart = 0; timelineRange = 5; layerScroll = Vector2.zero; filter = "";
                if (timelineData.TryGetValue(layer, out TimelineData data) && data.Duration > 0 && node.Parent != null)
                    timelineRange = Math.Max(0.125, data.Duration * 1.05);
            }
            FilterTree();
            Run(ReadSelected);
            UpdateTimelineClock();
        }

        private void RevealTimelineLayer(int layer)
        {
            if (!treeNodes.TryGetValue(layer, out LayerNode node)) return;
            LayerNode owner = node.Parent == null ? node : node.Parent;
            while (owner != null && owner.Parent != null && !owner.Info.IsTimeline) owner = owner.Parent;
            if (owner != null && owner.Info.Id != timelineScope) OpenTimelineScope(owner.Info.Id);
            for (LayerNode ancestor = node.Parent; ancestor != null; ancestor = ancestor.Parent) collapsedNodes.Remove(ancestor.Info.Id);
            expandedKeyLayers.Add(layer);
        }

        private void DrawTimelineScopeToolbar()
        {
            GUILayout.BeginHorizontal();
            GUI.enabled = attached && pending == 0 && !awaitingCommit;
            if (GUILayout.Button("Root timeline", GUILayout.Width(100)) && treeRoots.Count > 0)
                OpenTimelineScope(treeRoots[0].Info.Id);
            GUI.enabled &= !IsRootScope;
            if (GUILayout.Button("Up", GUILayout.Width(45)))
            {
                LayerNode ancestor = ScopeNode?.Parent;
                while (ancestor != null && ancestor.Parent != null && !ancestor.Info.IsTimeline) ancestor = ancestor.Parent;
                if (ancestor != null) OpenTimelineScope(ancestor.Info.Id);
            }
            GUI.enabled = attached && pending == 0 && !awaitingCommit &&
                treeNodes.TryGetValue(selected, out LayerNode selectedNode) && selectedNode.Info.IsTimeline && selected != timelineScope;
            if (GUILayout.Button("Open selected timeline", GUILayout.Width(155))) OpenTimelineScope(selected);
            GUI.enabled = true;
            var names = new List<string>();
            for (LayerNode node = ScopeNode; node != null; node = node.Parent) names.Add(node.Info.Name);
            names.Reverse();
            GUILayout.Label(string.Join(" / ", names), GUILayout.MinWidth(100));
            string clockState = !timelineClockValid ? timelineClockError : (timelineClock.flags & ClockInactive) != 0 ? "Inactive" :
                (timelineClock.flags & ClockActionHeld) != 0 ? "Action hold" : (timelineClock.flags & ClockClamped) != 0 ? "Finite end" :
                (timelineClock.flags & ClockHeld) != 0 ? "Paused" : "Playing";
            string iteration = !timelineClockValid || (timelineClock.flags & ClockInactive) != 0 ? clockState :
                (timelineClock.flags & ClockClamped) != 0 ? $"End ({timelineClock.effectiveRepeat} cycles)" : $"Cycle {timelineClock.cycle + 1} · {clockState}";
            GUILayout.Label(iteration, GUILayout.Width(180));
            if (!IsRootScope) GUILayout.Label("Local timeline view · playback preview pending", GUILayout.Width(320));
            GUILayout.EndHorizontal();
        }
    }
}
