using System;
using System.Runtime.InteropServices;
using System.Text;

namespace ImmPlayer
{
    // Sample bindings for layer, key, and timeline controls.
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
            public double doubleValue;
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct TimelineClock
        {
            public int timeline, parentTimeline;
            public long rootReported, rootEvaluated, elapsed, local, duration;
            public ulong cycle;
            public uint canonicalRepeat, effectiveRepeat, flags, reserved;
            public ulong evaluationRevision, presentedRevision, requestedRevision;
        }
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetProperties(int doc, int layer, ref Properties value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerSetProperties(int doc, int layer, ref Properties value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetPivot(int doc, int layer, ref Pivot value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerSetPivot(int doc, int layer, ref Pivot value);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerSetName(int doc, int layer, byte[] name, uint count);
        [DllImport(Library)] private static extern int ImmAuthoring_LayerGetName(int doc, int layer, [Out] byte[] name, uint capacity, out uint required);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeySet(int doc, int layer, ref Key key);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyGetEditability(int doc, int layer, int property);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyGetCount(int doc, int layer, int property, out uint count);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyGet(int doc, int layer, int property, uint index, ref Key key);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetTimeline(int doc, int layer, out int isTimeline,
            out double duration, out double offset, out uint frameRate, out uint frames, out uint maxRepeat);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerGetWorldTransform(int doc, int layer, ref Pivot transform);
        [DllImport(Library)] internal static extern int ImmAuthoring_TimelineGetClock(int doc, int layer, ref TimelineClock clock, uint capacity);
        [DllImport(Library)] internal static extern int ImmAuthoring_TimelinePreviewControl(int doc, int layer, int action, long ticks);
        [DllImport(Library)] internal static extern int ImmAuthoring_KeyRemove(int doc, int layer, int property, double seconds);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerCreate(int doc, int parent, int type, byte[] name, uint count, out int layer);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerDestroy(int doc, int layer);
        [DllImport(Library)] internal static extern int ImmAuthoring_LayerReparent(int doc, int layer, int parent, int index);
        [DllImport(Library)] internal static extern int ImmAuthoring_PaintSetMaxRepeatCount(int doc, int layer, uint count);

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

        internal static void ValidateLayouts()
        {
            if (Marshal.SizeOf<Properties>() != 64 || Marshal.SizeOf<Pivot>() != 48 ||
                Marshal.SizeOf<Key>() != 80 ||
                Marshal.OffsetOf<Key>(nameof(Key.doubleValue)).ToInt32() != 72 ||
                Marshal.SizeOf<LayerDiagnosticsNative>() != 44 || Marshal.SizeOf<TimelineClock>() != 96 ||
                Marshal.OffsetOf<TimelineClock>(nameof(TimelineClock.local)).ToInt32() != 32 ||
                Marshal.OffsetOf<TimelineClock>(nameof(TimelineClock.flags)).ToInt32() != 64 ||
                Marshal.OffsetOf<TimelineClock>(nameof(TimelineClock.evaluationRevision)).ToInt32() != 72)
                throw new InvalidOperationException("Live-edit sample native layouts do not match imm_authoring.h");
        }
    }
}
