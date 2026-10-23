using System;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace ImmPlayer
{
    // Sample-only bindings to imm_authoring.h. This does not change the managed SDK.
    internal static class ImmLiveEditNative
    {
#if UNITY_IOS && !UNITY_EDITOR
        private const string Library = "__Internal";
#else
        private const string Library = "ImmUnityPlugin";
#endif
        [StructLayout(LayoutKind.Sequential)]
        internal struct Properties
        {
            public uint size, version, mask, reserved;
            public int visible;
            public float opacity, tx, ty, tz, qx, qy, qz, qw, scale;
            public int flip;
            public uint reserved1;
            public static Properties Empty => new Properties { size = 64, version = 1 };
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct Pivot
        {
            public uint size, version;
            public float tx, ty, tz, qx, qy, qz, qw, scale;
            public int flip;
            public uint reserved;
            public static Pivot Empty => new Pivot { size = 48, version = 1 };
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct Key
        {
            public uint size, version;
            public int property, interpolation;
            public double seconds;
            public int boolean;
            public float value, tx, ty, tz, qx, qy, qz, qw, scale;
            public uint ticks;
            public int flip;
            public uint reserved0, reserved1;
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct Status
        {
            public uint size, version;
            public ulong revision;
            public int state, result;
            public uint command, reserved;
            public ulong target;
            public static Status Empty => new Status { size = 40, version = 1 };
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct Revisions
        {
            public uint size, version;
            public ulong requested, prepared, presented;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct Point
        {
            public Vector3 position, normal, direction, color;
            public float alpha, width, length, time;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct Element
        {
            public uint size, version;
            public int brush, visibility;
            public uint count, reserved;
            public IntPtr points;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct Geometry
        {
            public uint size, version, count, reserved;
            public IntPtr elements;
            public float biggestStroke;
            public int colorSpace, flipped;
            public uint reserved1;
        }

        [DllImport(Library)] internal static extern int ImmAuthoring_Attach(int doc);
        [DllImport(Library)] internal static extern int ImmAuthoring_Detach(int doc);
        [DllImport(Library)] internal static extern int ImmAuthoring_DiscardPending(int doc);
        [DllImport(Library)] internal static extern int ImmAuthoring_Commit(int doc, out ulong revision);
        [DllImport(Library)] internal static extern int ImmAuthoring_GetCommitStatus(int doc, ulong revision, ref Status status);
        [DllImport(Library)] internal static extern int ImmAuthoring_GetRevisions(int doc, ref Revisions revisions);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetProperties(int doc, int layer, ref Properties value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerSetProperties(int doc, int layer, ref Properties value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetPivot(int doc, int layer, ref Pivot value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerSetPivot(int doc, int layer, ref Pivot value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerSetName(int doc, int layer, byte[] name, uint count);
        [DllImport(Library)] private static extern int ImmAuthoring_LayerGetName(int doc, int layer, [Out] byte[] name, uint capacity, out uint required);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeySet(int doc, int layer, ref Key key);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyGetCount(int doc, int layer, int property, out uint count);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyGet(int doc, int layer, int property, uint index, ref Key key);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetTimeline(int doc, int layer, out int isTimeline,
            out double duration, out double offset, out uint frameRate, out uint frames, out uint maxRepeat);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetWorldTransform(int doc, int layer, ref Pivot transform);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyRemove(int doc, int layer, int property, double seconds);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerCreate(int doc, int parent, int type, byte[] name, uint count, out int layer);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerDestroy(int doc, int layer);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerReparent(int doc, int layer, int parent, int index);
        [DllImport(Library)] internal static extern int ImmAuthoring_PaintSetMaxRepeatCount(int doc, int layer, uint count);
        [DllImport(Library)] internal static extern int ImmAuthoring_FrameGetDrawingHandle(int doc, int layer, int frame, out ulong drawing);
        [DllImport(Library)] internal static extern int ImmAuthoring_FrameSetHandle(int doc, int layer, int frame, ulong drawing);
        [DllImport(Library)] internal static extern int ImmAuthoring_FrameSequenceSetHandles(int doc, int layer, ulong[] drawings, uint count);
        [DllImport(Library)] internal static extern int ImmAuthoring_DrawingGetHandle(int doc, int layer, int index, out ulong drawing);
        [DllImport(Library)] private static extern int ImmAuthoring_DrawingSetGeometry(int doc, int layer, ulong drawing, ref Geometry geometry);

        internal static string ReadName(int doc, int layer)
        {
            Check(ImmAuthoring_LayerGetName(doc, layer, null, 0, out uint required));
            var bytes = new byte[required];
            Check(ImmAuthoring_LayerGetName(doc, layer, bytes, required, out required));
            return Encoding.UTF8.GetString(bytes, 0, checked((int)required - 1));
        }

        internal static void Check(int result)
        {
            if (result != 0) throw new InvalidOperationException($"{Result(result)} ({result})");
        }

        internal static string Result(int result)
        {
            switch (result)
            {
                case 0: return "OK";
                case -1: return "Not found";
                case -2: return "Invalid argument";
                case -3: return "Unsupported operation/content";
                case -4: return "Invalid session/batch state";
                case -5: return "Out of memory";
                case -6: return "Validation failed";
                case -7: return "Busy / pending command or data ownership";
                case -9: return "Cancelled";
                case -10: return "Renderer preparation/publication failed";
                default: return $"Native result {result}";
            }
        }

        internal static int ReplaceWithMarker(int doc, int layer, ulong drawing, float size)
        {
            var points = new Point[4];
            for (int i = 0; i < points.Length; ++i)
                points[i] = new Point { position = new Vector3((i - 1.5f) * size / 3f, (i % 2) * size / 3f, 0),
                    normal = Vector3.forward, direction = Vector3.right, color = new Vector3(1, 0.1f, 0.8f),
                    alpha = 1, width = size * 0.04f, length = i * size / 3f };
            var pointPin = GCHandle.Alloc(points, GCHandleType.Pinned);
            try
            {
                var elements = new[] { new Element { size = (uint)Marshal.SizeOf<Element>(), version = 1,
                    brush = 2, visibility = 1, count = (uint)points.Length, points = pointPin.AddrOfPinnedObject() } };
                var elementPin = GCHandle.Alloc(elements, GCHandleType.Pinned);
                try
                {
                    var geometry = new Geometry { size = (uint)Marshal.SizeOf<Geometry>(), version = 1,
                        count = 1, elements = elementPin.AddrOfPinnedObject(), biggestStroke = size * 0.04f };
                    return ImmAuthoring_DrawingSetGeometry(doc, layer, drawing, ref geometry);
                }
                finally { elementPin.Free(); }
            }
            finally { pointPin.Free(); }
        }

        internal static void ValidateLayouts()
        {
            if (Marshal.SizeOf<Properties>() != 64 || Marshal.SizeOf<Pivot>() != 48 ||
                Marshal.SizeOf<Key>() != 80 || Marshal.SizeOf<Status>() != 40 || Marshal.SizeOf<Point>() != 64 ||
                Marshal.SizeOf<LayerDiagnosticsNative>() != 44)
                throw new InvalidOperationException("Live-edit sample native layouts do not match imm_authoring.h");
        }
    }
}
