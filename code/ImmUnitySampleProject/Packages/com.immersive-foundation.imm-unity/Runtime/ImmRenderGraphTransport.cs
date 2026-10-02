using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer
{
    // Single managed caller. Native packet completion is read through an acquire
    // operation in the plugin. The owner must poll and queue shutdown before disposal.
    internal sealed class ImmRenderGraphTransport : IDisposable
    {
        internal const int PacketSize = 224;
        internal const int EventId = 0x494d4d;
        private enum Phase { Created, Initializing, Ready, Stopping, Stopped, Faulted }
        private sealed class Slot
        {
            internal IntPtr Memory;
            internal int Operation;
            internal bool Pending;
        }
        private readonly List<Slot> slots = new List<Slot>();
        private readonly float[] matrices = new float[32];
        private readonly IntPtr callback;
        private Phase phase;
        private ulong sequence;
        private bool disposed;
        internal bool IsReady => phase == Phase.Ready;
        internal bool IsStopped => phase == Phase.Stopped;
        internal bool IsDisposed => disposed;
        internal int LastError { get; private set; }
        internal ulong RenderSubmissions { get; private set; }

        [DllImport("ImmUnityPlugin", CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr GetRenderGraphEventFunc();
        [DllImport("ImmUnityPlugin", CallingConvention = CallingConvention.StdCall)]
        private static extern int GetRenderGraphPacketSize();
        [DllImport("ImmUnityPlugin", CallingConvention = CallingConvention.StdCall)]
        private static extern int GetRenderGraphPacketResult(IntPtr packet, out int result, out ulong gpuCompletion);

        internal ImmRenderGraphTransport()
        {
            if (IntPtr.Size != 8 || SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D12)
                throw new NotSupportedException("IMM RenderGraph transport currently requires Windows D3D12.");
            if (GetRenderGraphPacketSize() != PacketSize)
                throw new InvalidOperationException("IMM managed and native RenderGraph packet layouts differ.");
            callback = GetRenderGraphEventFunc();
            if (callback == IntPtr.Zero)
                throw new InvalidOperationException("IMM D3D12 rendering event is unavailable.");
        }

        internal void Poll()
        {
            ThrowIfDisposed();
            foreach (Slot slot in slots)
            {
                if (!slot.Pending) continue;
                int completed = GetRenderGraphPacketResult(slot.Memory, out int result, out _);
                if (completed < 0) throw new InvalidOperationException("IMM rejected a queued packet's ABI.");
                if (completed == 0) continue;
                slot.Pending = false;
                if (result < 0)
                {
                    LastError = result;
                    if (phase != Phase.Stopping && phase != Phase.Stopped) phase = Phase.Faulted;
                }
                else if (slot.Operation == 0 && phase == Phase.Initializing) phase = Phase.Ready;
                else if (slot.Operation == 2) phase = Phase.Stopped;
            }
        }

        internal void QueueInitialize(CommandBuffer commands, bool linearColor, bool enableSound)
        {
            Poll();
            if ((phase != Phase.Created && phase != Phase.Stopped) || HasPending())
                throw new InvalidOperationException("IMM RenderGraph session is already active.");
            Slot slot = Acquire(0);
            Marshal.WriteInt32(slot.Memory, 28, linearColor ? 0 : 1);
            Marshal.WriteInt32(slot.Memory, 32, 8); // Initial renderer configuration; borrowed attachments determine draw sample counts.
            Marshal.WriteInt32(slot.Memory, 36, enableSound ? 1 : 0);
            Enqueue(commands, slot);
            LastError = 0;
            phase = Phase.Initializing;
        }

        internal void QueueRender(CommandBuffer commands, int camera, IntPtr color, IntPtr depth,
            uint colorFormat, uint depthFormat, RectInt viewport, Matrix4x4 worldToView, Matrix4x4 projection)
        {
            Poll();
            if (!IsReady) throw new InvalidOperationException("IMM RenderGraph session is not ready.");
            if (camera < 0 || camera >= 256 || color == IntPtr.Zero || depth == IntPtr.Zero ||
                viewport.x < 0 || viewport.y < 0 || viewport.width <= 0 || viewport.height <= 0)
                throw new ArgumentException("Invalid IMM camera or render attachments.");
            Slot slot = Acquire(1);
            Marshal.WriteInt32(slot.Memory, 24, camera);
            Marshal.WriteInt64(slot.Memory, 40, color.ToInt64());
            Marshal.WriteInt64(slot.Memory, 48, depth.ToInt64());
            Marshal.WriteInt32(slot.Memory, 56, unchecked((int)colorFormat));
            Marshal.WriteInt32(slot.Memory, 60, unchecked((int)depthFormat));
            Marshal.WriteInt32(slot.Memory, 64, viewport.x);
            Marshal.WriteInt32(slot.Memory, 68, viewport.y);
            Marshal.WriteInt32(slot.Memory, 72, viewport.width);
            Marshal.WriteInt32(slot.Memory, 76, viewport.height);
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 4; ++column)
                {
                    matrices[row * 4 + column] = worldToView[row, column];
                    matrices[16 + row * 4 + column] = projection[row, column];
                }
            Marshal.Copy(matrices, 0, IntPtr.Add(slot.Memory, 80), matrices.Length);
            Enqueue(commands, slot);
            ++RenderSubmissions;
        }

        internal bool QueueMaintenance(CommandBuffer commands)
        {
            Poll();
            if (!IsReady) return false;
            // One outstanding maintenance event is enough to advance unloads.
            foreach (Slot slot in slots)
                if (slot.Pending && slot.Operation == 3) return false;
            Enqueue(commands, Acquire(3));
            return true;
        }

        internal void QueueShutdown(CommandBuffer commands)
        {
            Poll();
            if (phase == Phase.Stopping || phase == Phase.Stopped) return;
            Enqueue(commands, Acquire(2));
            phase = Phase.Stopping;
        }

        private Slot Acquire(int operation)
        {
            Slot available = null;
            foreach (Slot slot in slots) if (!slot.Pending) { available = slot; break; }
            if (available == null)
            {
                available = new Slot { Memory = Marshal.AllocHGlobal(PacketSize) };
                slots.Add(available);
            }
            // Clear stale results and unused fields before publishing another request.
            for (int offset = 0; offset < PacketSize; offset += 8) Marshal.WriteInt64(available.Memory, offset, 0);
            Marshal.WriteInt32(available.Memory, 0, 1);
            Marshal.WriteInt32(available.Memory, 4, PacketSize);
            Marshal.WriteInt32(available.Memory, 8, operation);
            Marshal.WriteInt64(available.Memory, 16, unchecked((long)++sequence));
            available.Operation = operation;
            return available;
        }

        private void Enqueue(CommandBuffer commands, Slot slot)
        {
            if (commands == null) throw new ArgumentNullException(nameof(commands));
            // Conservatively retain the packet even if command recording throws.
            slot.Pending = true;
            commands.IssuePluginEventAndData(callback, EventId, slot.Memory);
        }
        private bool HasPending()
        {
            foreach (Slot slot in slots) if (slot.Pending) return true;
            return false;
        }
        private void ThrowIfDisposed()
        {
            if (disposed) throw new ObjectDisposedException(nameof(ImmRenderGraphTransport));
        }
        public void Dispose()
        {
            if (disposed) return;
            Poll();
            if (HasPending() || (phase != Phase.Created && phase != Phase.Stopped))
                throw new InvalidOperationException("Queue IMM shutdown and wait for all packet acknowledgements before disposal.");
            foreach (Slot slot in slots) Marshal.FreeHGlobal(slot.Memory);
            slots.Clear();
            disposed = true;
        }
    }
}
