using System;
using System.Collections.Generic;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    public sealed partial class ImmLiveEditPanel
    {
        private const long SequenceTicksPerSecond = 12600;

        private static long SequenceTick(double seconds)
        {
            if (double.IsNaN(seconds) || seconds <= 0) return 0;
            double ticks = Math.Round(seconds * SequenceTicksPerSecond, MidpointRounding.AwayFromZero);
            // double(long.MaxValue) is 2^63: reject that cast by saturating first.
            return double.IsInfinity(ticks) || ticks >= (double)long.MaxValue ? long.MaxValue : (long)ticks;
        }

        // Native tracks are ordered by time. Return the last key at/before the
        // sample and the next strictly later key, including when no key is active.
        private static bool SequenceKeyAt(TimelineData data, int property, long tick,
            out N.Key key, out long next)
        {
            key = default;
            next = long.MaxValue;
            if (!data.Keys.TryGetValue(property, out N.Key[] keys) || keys.Length == 0) return false;
            int low = 0, high = keys.Length;
            while (low < high)
            {
                int middle = low + (high - low) / 2;
                if (SequenceTick(keys[middle].seconds) <= tick) low = middle + 1;
                else high = middle;
            }
            if (low < keys.Length) next = SequenceTick(keys[low].seconds);
            if (low == 0) return false;
            key = keys[low - 1];
            return true;
        }

        private bool TrySampleSequence(LayerNode node, TimelineData data, long tick,
            out int slot, out bool held, out long nextChange)
        {
            slot = -1;
            held = false;
            nextChange = long.MaxValue;
            if (node == null || data == null || tick < 0 || data.Drawings.Length == 0 || !InTimelineScope(node)) return false;
            if (!timelineData.TryGetValue(timelineScope, out TimelineData scope)) return false;
            if (scope.Duration > 0)
            {
                long duration = SequenceTick(scope.Duration);
                if (tick > duration) return false;
                if (duration < long.MaxValue) nextChange = duration + 1;
            }

            bool visible = true, foundScope = false;
            for (LayerNode ancestor = node; ancestor != null; ancestor = ancestor.Parent)
            {
                bool boundary = ancestor.Info.Id == timelineScope;
                // A nested container's keys belong to its parent composition.
                // Ordinary document-root visibility still masks the root scope.
                if (boundary && !IsRootScope) { foundScope = true; break; }
                TimelineData state = data;
                if (ancestor != node && !timelineData.TryGetValue(ancestor.Info.Id, out state)) return false;
                bool hasVisibility = state.Keys.TryGetValue(0, out N.Key[] visibility) && visibility.Length > 0;
                bool active = SequenceKeyAt(state, 0, tick, out N.Key key, out long next);
                nextChange = Math.Min(nextChange, next);
                visible &= hasVisibility ? active && key.boolean != 0 : state.CanonicalVisible;
                if (boundary) { foundScope = true; break; }
            }
            if (!foundScope) return false;

            bool activeVisibility = SequenceKeyAt(data, 0, tick, out N.Key visibleKey, out long visibilityNext);
            bool hasPaintVisibility = data.Keys.TryGetValue(0, out N.Key[] paintVisibility) && paintVisibility.Length > 0;
            bool activeOffset = SequenceKeyAt(data, 8, tick, out N.Key offsetKey, out long offsetNext);
            bool activeLoop = SequenceKeyAt(data, 7, tick, out N.Key loopKey, out long loopNext);
            nextChange = Math.Min(nextChange, Math.Min(visibilityNext, Math.Min(offsetNext, loopNext)));
            if (!visible) return false;
            if (!hasPaintVisibility)
            {
                // Native keyless paint retains local time zero; Offset cannot
                // invent a visibility span or an advancing drawing clock.
                slot = 0; held = true;
                return true;
            }
            if (!activeVisibility || visibleKey.boolean == 0) return false;
            if (data.Drawings.Length == 1) { slot = 0; held = true; return true; }
            if (data.FrameRate == 0 || SequenceTicksPerSecond % data.FrameRate != 0) return false;

            long start = SequenceTick(visibleKey.seconds);
            long offset = activeOffset ? offsetKey.ticks : 0;
            long elapsed = tick - start;
            if (elapsed > long.MaxValue - offset) return false;
            long local = elapsed + offset;
            long ticksPerFrame = SequenceTicksPerSecond / data.FrameRate;
            ulong frameNumber = (ulong)(local / ticksPerFrame);
            ulong count = (ulong)data.Drawings.Length;
            uint repeat = activeLoop ? (loopKey.boolean != 0 ? 0u : 1u) : data.Repeat;
            bool clamped = repeat != 0 && frameNumber / count >= repeat;
            slot = clamped ? data.Drawings.Length - 1 : (int)(frameNumber % count);
            held = clamped || (frameNumber > 0 && data.Drawings[slot] == data.Drawings[(int)((frameNumber - 1) % count)]);
            if (!clamped)
            {
                long step = ticksPerFrame - local % ticksPerFrame;
                long frameBoundary = tick > long.MaxValue - step ? long.MaxValue : tick + step;
                nextChange = Math.Min(nextChange, frameBoundary);
            }
            return true;
        }

        private struct ProjectedSequenceSegment
        {
            internal long Start, End;
            internal int Slot;
            internal bool Visible, Held;
        }

        private void DrawProjectedSequence(LayerNode node, TimelineData data, float y, float namesWidth, float tracksWidth)
        {
            if (Event.current.type != EventType.Repaint || data.Drawings.Length == 0 ||
                timelineRange <= 0 || tracksWidth <= 0) return;
            long start = SequenceTick(timelineStart), end = SequenceTick(timelineStart + timelineRange);
            if (timelineData.TryGetValue(timelineScope, out TimelineData scope) && scope.Duration > 0)
                end = Math.Min(end, SequenceTick(scope.Duration));
            if (end <= start) return;

            // Collect only a viewport-sized number of exact intervals. No work
            // is proportional to total document frames or repeat count.
            int budget = Math.Max(16, Math.Min(4096, (int)Math.Ceiling(tracksWidth / 3)));
            var segments = new List<ProjectedSequenceSegment>(Math.Min(budget, 128));
            long cursor = start;
            while (cursor < end && segments.Count < budget)
            {
                bool visible = TrySampleSequence(node, data, cursor, out int slot, out bool held, out long next);
                long right = Math.Min(end, next);
                if (right <= cursor) break;
                segments.Add(new ProjectedSequenceSegment { Start = cursor, End = right, Slot = slot, Visible = visible, Held = held });
                cursor = right;
            }
            if (cursor < end)
            {
                DrawDenseSequence(node, data, start, end, y, namesWidth, tracksWidth);
                return;
            }
            foreach (var segment in segments)
            {
                if (!segment.Visible) continue;
                float left = SequenceX(segment.Start, namesWidth, tracksWidth);
                float right = SequenceX(segment.End, namesWidth, tracksWidth);
                Fill(new Rect(left, y + 4, right - left, RowHeight - 8),
                    new Color(0.25f, segment.Held ? 0.44f : 0.57f, 0.65f));
                int previousSlot = -1;
                bool previousVisible = segment.Start > 0 && TrySampleSequence(node, data, segment.Start - 1,
                    out previousSlot, out _, out _);
                bool changesDrawing = !previousVisible || data.Drawings[previousSlot] != data.Drawings[segment.Slot];
                if (changesDrawing && right - left >= 3) Fill(new Rect(left + 1, y + 8, 3, 6), Color.white);
            }
        }

        private float SequenceX(long tick, float namesWidth, float tracksWidth) =>
            namesWidth + (float)((tick / (double)SequenceTicksPerSecond - timelineStart) / timelineRange) * tracksWidth;

        private static long SequenceBinTick(long start, long end, int index, int count)
        {
            long span = end - start;
            // Quotient/remainder avoids overflow from multiplying the full span.
            return start + (span / count) * index + ((span % count) * index) / count;
        }

        private void DrawDenseSequence(LayerNode node, TimelineData data, long start, long end,
            float y, float namesWidth, float tracksWidth)
        {
            int bins = Math.Max(1, Math.Min(4096, (int)Math.Ceiling(tracksWidth)));
            for (int index = 0; index < bins; ++index)
            {
                long a = SequenceBinTick(start, end, index, bins), b = SequenceBinTick(start, end, index + 1, bins);
                if (b <= a) continue;
                bool visible = TrySampleSequence(node, data, a, out _, out _, out long next);
                bool mixed = next < b;
                if (!visible && !mixed) continue;
                float left = SequenceX(a, namesWidth, tracksWidth), right = SequenceX(b, namesWidth, tracksWidth);
                // A hidden sample with an interior event is explicitly unknown
                // at this zoom, rather than a claim of a visible drawing.
                Color color = visible ? new Color(0.25f, mixed ? 0.43f : 0.57f, 0.65f) : new Color(0.35f, 0.35f, 0.39f);
                Fill(new Rect(left, y + 4, right - left, RowHeight - 8), color);
            }
            GUI.Label(new Rect(namesWidth + 6, y + 2, tracksWidth - 12, RowHeight - 4),
                new GUIContent("Aggregate · zoom for frames", "Dense frame or key changes are summarized. Selecting still samples the exact table slot."));
        }
    }
}
