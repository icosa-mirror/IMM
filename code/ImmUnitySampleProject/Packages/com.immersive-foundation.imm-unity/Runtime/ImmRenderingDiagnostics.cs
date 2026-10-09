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

    public static class ImmRenderingDiagnostics
    {
        /// <summary>
        /// Invoked on the main thread after native acknowledgement. Camera is null for
        /// direct transport probes. XR pass is -1 for mono, 0 for supported two-view XR.
        /// Diagnostic subscribers should avoid allocating during performance measurements.
        /// Callbacks run during polling; do not queue or dispose sessions from a callback.
        /// </summary>
        public static event Action<ImmSceneSubmission> SubmissionCompleted;

        internal static bool HasSubscribers => SubmissionCompleted != null;
        internal static void Publish(ImmSceneSubmission submission) => SubmissionCompleted?.Invoke(submission);
    }
}
