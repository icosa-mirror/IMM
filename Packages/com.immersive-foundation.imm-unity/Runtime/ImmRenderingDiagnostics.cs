using System;
using Unity.Profiling;
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

    /// <summary>Whether any managed allocation occurred inside one IMM pass callback.</summary>
    public readonly struct ImmManagedAllocation
    {
        public ImmRenderCallback Callback { get; }
        public bool AllocationDetected { get; }
        internal ImmManagedAllocation(ImmRenderCallback callback, bool allocationDetected)
        {
            Callback = callback;
            AllocationDetected = allocationDetected;
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
        /// Poll native acknowledgements on the main thread. Rendering normally polls
        /// automatically; an idle diagnostic fixture must poll explicitly after its
        /// GPU readback completes. This does not wait for GPU work or submit a scene.
        /// </summary>
        public static void PollCompletions()
        {
            var transport = ImmRenderGraphSession.Current?.Transport;
            if (transport != null && !transport.IsDisposed) transport.Poll();
        }

        /// <summary>
        /// Opt-in measurement of each IMM pass callback, including transport work and
        /// any submission listeners it invokes. Measurement listeners run after the
        /// recorder is stopped; they must not allocate or change rendering state.
        /// These scopes exclude surrounding Unity frame work and GPU/native allocations.
        /// </summary>
        public static event Action<ImmManagedAllocation> ManagedAllocationMeasured;

        internal struct AllocationScope : IDisposable
        {
            private readonly Action<ImmManagedAllocation> listener;
            private readonly ImmRenderCallback callback;
            private ProfilerRecorder recorder;
            internal AllocationScope(ImmRenderCallback callback)
            {
                this.callback = callback;
                listener = ManagedAllocationMeasured;
                recorder = listener != null ? CreateAllocationRecorder() : default;
                if (listener != null && !recorder.Valid)
                    throw new NotSupportedException("Unity GC.Alloc recording is unavailable.");
            }
            public void Dispose()
            {
                if (listener == null) return;
                recorder.Stop();
                bool allocated = recorder.Count != 0;
                recorder.Dispose();
                listener(new ImmManagedAllocation(callback, allocated));
            }
        }

        internal static AllocationScope Measure(ImmRenderCallback callback) => new AllocationScope(callback);

        // One sample is sufficient: any allocation fails the zero-allocation gate.
        private static ProfilerRecorder CreateAllocationRecorder() => ProfilerRecorder.StartNew(
            ProfilerCategory.Internal, "GC.Alloc", 1, ProfilerRecorderOptions.CollectOnlyOnCurrentThread);

        /// <summary>Verify an empty scope is clean and a known allocation is detected.</summary>
        public static bool CalibrateManagedAllocationMeasurement()
        {
            using (var empty = CreateAllocationRecorder())
            {
                if (!empty.Valid) return false;
                empty.Stop();
                if (empty.Count != 0) return false;
            }
            using var recorder = CreateAllocationRecorder();
            if (!recorder.Valid) return false;
            var calibration = new byte[256];
            recorder.Stop();
            GC.KeepAlive(calibration);
            return recorder.Count != 0;
        }

        internal static bool HasSubscribers => SubmissionCompleted != null;
        internal static void Publish(ImmSceneSubmission submission) => SubmissionCompleted?.Invoke(submission);
    }
}
