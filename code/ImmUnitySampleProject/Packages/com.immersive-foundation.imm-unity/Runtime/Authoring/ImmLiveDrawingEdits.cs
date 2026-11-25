using System;
using System.Runtime.InteropServices;
using ImmPlayer.Exporter;

namespace ImmPlayer.Authoring
{
    /// <summary>
    /// Edits the live player document identified by ImmDocument.DocumentId. Methods return
    /// native ImmAuthoringResult codes (zero on success). One writer owns the pending batch.
    /// Queue edits, then Commit once. On a queueing failure, DiscardPending before retrying.
    /// Poll GetCommitStatus until Presented or Rejected before recording undo or issuing more edits.
    /// Drawing handles belong to a paint layer and expire when editing detaches or the document unloads.
    /// </summary>
    public static class ImmLiveDrawingEdits
    {
#if UNITY_IOS && !UNITY_EDITOR
        private const string Library = "__Internal";
#else
        private const string Library = "ImmUnityPlugin";
#endif
        private const int InvalidArgument = -2;
        // Limits from imm_authoring.h, checked before allocating point buffers.
        private const int MaxStrokes = 65536;
        private const int MaxStrokePoints = 8192;
        private const int MaxDrawingPoints = 1048576;

        public enum CommitState { Unknown, Queued, Preparing, Prepared, Presented, Rejected }

        [StructLayout(LayoutKind.Sequential)]
        public struct Revisions
        {
            internal uint Size, Version;
            public ulong Requested, Prepared, Presented;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CommitStatus
        {
            internal uint Size, Version;
            public ulong Revision;
            public CommitState State;
            public int Result;
            public uint Command;
            private uint reserved;
            public ulong Target;
        }

        public readonly struct Stroke
        {
            public readonly BrushSectionType Brush;
            public readonly VisibilityType Visibility;
            public readonly PaintPoint[] Points;

            public Stroke(BrushSectionType brush, VisibilityType visibility, PaintPoint[] points)
            {
                Brush = brush;
                Visibility = visibility;
                Points = points;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Element
        {
            public uint Size, Version;
            public BrushSectionType Brush;
            public VisibilityType Visibility;
            public uint Count, Reserved;
            public IntPtr Points;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Geometry
        {
            public uint Size, Version, Count, Reserved;
            public IntPtr Elements;
            public float BiggestStroke;
            public int ColorSpace, Flipped;
            public uint Reserved1;
        }

        [DllImport(Library, EntryPoint = "ImmAuthoring_Attach")]
        public static extern int Attach(int document);

        [DllImport(Library, EntryPoint = "ImmAuthoring_Detach")]
        public static extern int Detach(int document);

        [DllImport(Library, EntryPoint = "ImmAuthoring_DiscardPending")]
        public static extern int DiscardPending(int document);

        [DllImport(Library, EntryPoint = "ImmAuthoring_Commit")]
        public static extern int Commit(int document, out ulong revision);

        public static int GetCommitStatus(int document, ulong revision, out CommitStatus status)
        {
            status = new CommitStatus { Size = (uint)Marshal.SizeOf<CommitStatus>(), Version = 1 };
            return ImmAuthoring_GetCommitStatus(document, revision, ref status);
        }

        public static int GetRevisions(int document, out Revisions revisions)
        {
            revisions = new Revisions { Size = (uint)Marshal.SizeOf<Revisions>(), Version = 1 };
            return ImmAuthoring_GetRevisions(document, ref revisions);
        }

        [DllImport(Library, EntryPoint = "ImmAuthoring_DrawingGetHandle")]
        public static extern int GetDrawing(int document, int layer, int drawingIndex, out ulong drawing);

        [DllImport(Library, EntryPoint = "ImmAuthoring_DrawingCreate")]
        public static extern int CreateDrawing(int document, int layer, out ulong drawing);

        [DllImport(Library, EntryPoint = "ImmAuthoring_DrawingDestroy")]
        public static extern int DestroyDrawing(int document, int layer, ulong drawing);

        [DllImport(Library, EntryPoint = "ImmAuthoring_FrameGetDrawingHandle")]
        public static extern int GetFrameDrawing(int document, int layer, int frame, out ulong drawing);

        [DllImport(Library, EntryPoint = "ImmAuthoring_FrameSetHandle")]
        public static extern int SetFrameDrawing(int document, int layer, int frame, ulong drawing);

        /// <summary>Repeated handles hold a drawing. An empty array clears the sequence.</summary>
        public static int SetFrameSequence(int document, int layer, ulong[] drawings)
        {
            if (drawings == null) return InvalidArgument;
            return ImmAuthoring_FrameSequenceSetHandles(document, layer, drawings, (uint)drawings.Length);
        }

        /// <summary>Replace the entire drawing. Native code copies all stroke data before returning.</summary>
        public static int SetDrawingGeometry(int document, int layer, ulong drawing, Stroke[] strokes,
            float biggestStroke, bool gamma = false, bool flipped = false)
        {
            if (strokes == null || strokes.Length == 0 || strokes.Length > MaxStrokes)
                return InvalidArgument;

            int totalPoints = 0;
            foreach (Stroke stroke in strokes)
            {
                int count = stroke.Points?.Length ?? 0;
                totalPoints += count;
                if (count < 2 || count > MaxStrokePoints || totalPoints > MaxDrawingPoints)
                    return InvalidArgument;
            }

            var elements = new Element[strokes.Length];
            var pins = new GCHandle[strokes.Length];
            GCHandle elementPin = default;
            try
            {
                for (int i = 0; i < strokes.Length; i++)
                {
                    Stroke stroke = strokes[i];
                    var points = ImmAuthoringCompiler.ToNative(stroke.Points);
                    pins[i] = GCHandle.Alloc(points, GCHandleType.Pinned);
                    elements[i] = new Element
                    {
                        Size = (uint)Marshal.SizeOf<Element>(), Version = 1,
                        Brush = stroke.Brush, Visibility = stroke.Visibility,
                        Count = (uint)points.Length, Points = pins[i].AddrOfPinnedObject()
                    };
                }
                elementPin = GCHandle.Alloc(elements, GCHandleType.Pinned);
                var geometry = new Geometry
                {
                    Size = (uint)Marshal.SizeOf<Geometry>(), Version = 1,
                    Count = (uint)elements.Length, Elements = elementPin.AddrOfPinnedObject(),
                    BiggestStroke = biggestStroke, ColorSpace = gamma ? 1 : 0, Flipped = flipped ? 1 : 0
                };
                return ImmAuthoring_DrawingSetGeometry(document, layer, drawing, ref geometry);
            }
            finally
            {
                if (elementPin.IsAllocated) elementPin.Free();
                foreach (GCHandle pin in pins)
                    if (pin.IsAllocated) pin.Free();
            }
        }

        [DllImport(Library)]
        private static extern int ImmAuthoring_GetCommitStatus(int document, ulong revision, ref CommitStatus status);
        [DllImport(Library)]
        private static extern int ImmAuthoring_GetRevisions(int document, ref Revisions revisions);
        [DllImport(Library)]
        private static extern int ImmAuthoring_FrameSequenceSetHandles(int document, int layer, [In] ulong[] drawings, uint count);
        [DllImport(Library)]
        private static extern int ImmAuthoring_DrawingSetGeometry(int document, int layer, ulong drawing, ref Geometry geometry);
    }
}
