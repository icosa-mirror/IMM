using System;
using UnityEngine;

namespace ImmPlayer
{
    /// <summary>A completed native scene event, attributed to its recorded camera and frame.</summary>
    public readonly struct ImmSceneSubmission
    {
        public Camera Camera { get; }
        public ulong Sequence { get; }
        public int NativeCameraId { get; }
        public int FrameIndex { get; }
        public int XrPassIndex { get; }
        public int ViewCount { get; }
        public int NativeSceneEvents { get; }
        public int Result { get; }

        internal ImmSceneSubmission(Camera camera, ulong sequence, int nativeCameraId,
            int frameIndex, int xrPassIndex, int viewCount, int nativeSceneEvents, int result)
        {
            Camera = camera;
            Sequence = sequence;
            NativeCameraId = nativeCameraId;
            FrameIndex = frameIndex;
            XrPassIndex = xrPassIndex;
            ViewCount = viewCount;
            NativeSceneEvents = nativeSceneEvents;
            Result = result;
        }
    }

    public enum ImmRenderCallback { AddRenderPasses, RecordRenderGraph, ExecuteNative }

    /// <summary>Thread-local managed bytes allocated inside one IMM pass callback.</summary>
    public readonly struct ImmManagedAllocation
    {
        public ImmRenderCallback Callback { get; }
        public long Bytes { get; }
        internal ImmManagedAllocation(ImmRenderCallback callback, long bytes)
        {
            Callback = callback;
            Bytes = bytes;
        }
    }

    public static class ImmRenderingDiagnostics
    {
        /// <summary>
        /// Invoked on the main thread after native acknowledgement. Camera is null for
        /// direct transport probes. XR pass is -1 for mono, 0 for supported two-view XR.
        /// Diagnostic subscribers should avoid allocating during performance measurements.
        /// Callbacks run during polling; do not queue or dispose sessions from a callback.
        /// </summary>
        public static event Action<ImmSceneSubmission> SubmissionCompleted;

        /// <summary>
        /// Opt-in measurement of each IMM pass callback, including transport work and
        /// any submission listeners it invokes. Measurement listeners run after the
        /// byte count is read; they must not allocate or change rendering state.
        /// These scopes exclude surrounding Unity frame work and GPU/native allocations.
        /// </summary>
        public static event Action<ImmManagedAllocation> ManagedAllocationMeasured;

        internal readonly struct AllocationScope : IDisposable
        {
            private readonly Action<ImmManagedAllocation> listener;
            private readonly ImmRenderCallback callback;
            private readonly long start;
            internal AllocationScope(ImmRenderCallback callback)
            {
                this.callback = callback;
                listener = ManagedAllocationMeasured;
                start = listener != null ? GC.GetAllocatedBytesForCurrentThread() : 0;
            }
            public void Dispose()
            {
                if (listener != null)
                    listener(new ImmManagedAllocation(callback, GC.GetAllocatedBytesForCurrentThread() - start));
            }
        }

        internal static AllocationScope Measure(ImmRenderCallback callback) => new AllocationScope(callback);

        internal static bool HasSubscribers => SubmissionCompleted != null;
        internal static void Publish(ImmSceneSubmission submission) => SubmissionCompleted?.Invoke(submission);
    }
}
