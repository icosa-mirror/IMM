using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace ImmPlayer.Exporter
{
    public enum ExportSequenceType
    {
        Still = 0,
        Animated = 1,
        Comic = 2
    }

    public enum ExportAudioType
    {
        Opus = 0,
        Ogg = 1
    }

    public enum BrushSectionType
    {
        Point = 0,
        Segment = 1,
        Circle = 2,
        Ellipse = 3,
        Square = 4
    }

    public enum VisibilityType
    {
        FadePow2 = 0,
        Always = 1
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ExportRequirements
    {
        public long MaxMemory;
        public long MaxRenderCalls;
        public long MaxTriangles;
        public long MaxSoundChannels;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct TransformNative
    {
        public float Tx;
        public float Ty;
        public float Tz;
        public float Qx;
        public float Qy;
        public float Qz;
        public float Qw;
        public float Scale;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct PointNative
    {
        public float Px;
        public float Py;
        public float Pz;
        public float Nx;
        public float Ny;
        public float Nz;
        public float Dx;
        public float Dy;
        public float Dz;
        public float R;
        public float G;
        public float B;
        public float A;
        public float Width;
        public float Length;
        public float Time;
    }

    public struct PaintPoint
    {
        public Vector3 Position;
        public Vector3 Normal;
        public Vector3 Direction;
        public Color Color;
        public float Alpha;
        public float Width;
        public float Length;
        public float Time;
    }

    public sealed class ExportSequence : IDisposable
    {
        internal IntPtr Handle { get; private set; }
        public bool IsValid => Handle != IntPtr.Zero;

        private ExportSequence(IntPtr handle)
        {
            Handle = handle;
        }

        public static ExportSequence Create(
            ExportSequenceType type,
            uint frameRate,
            Color backgroundColor,
            ExportRequirements requirements,
            byte caps = 0)
        {
            IntPtr handle = Native.ImmExporter_CreateSequence(
                (int)type,
                caps,
                backgroundColor.r,
                backgroundColor.g,
                backgroundColor.b,
                frameRate,
                requirements.MaxMemory,
                requirements.MaxRenderCalls,
                requirements.MaxTriangles,
                requirements.MaxSoundChannels);

            if (handle == IntPtr.Zero)
            {
                return null;
            }
            return new ExportSequence(handle);
        }

        public ExportPaintLayer CreatePaintLayer(
            string name,
            bool visible = true,
            float opacity = 1.0f,
            Transform transform = null)
        {
            if (!IsValid)
                return null;

            TransformNative t = TransformUtils.ToNativeTransform(transform);
            TransformNative pivot = TransformUtils.IdentityTransform();

            IntPtr layerHandle = Native.ImmExporter_CreatePaintLayer(
                Handle,
                IntPtr.Zero,
                name ?? "Paint",
                visible ? 1 : 0,
                opacity,
                ref t,
                ref pivot,
                0,
                0,
                0);

            if (layerHandle == IntPtr.Zero)
                return null;

            return new ExportPaintLayer(layerHandle);
        }

        public ExportGroupLayer CreateGroupLayer(
            string name,
            bool visible = true,
            float opacity = 1.0f,
            Transform transform = null)
        {
            if (!IsValid)
                return null;

            TransformNative t = TransformUtils.ToNativeTransform(transform);
            TransformNative pivot = TransformUtils.IdentityTransform();

            IntPtr layerHandle = Native.ImmExporter_CreateGroupLayer(
                Handle,
                IntPtr.Zero,
                name ?? "Group",
                visible ? 1 : 0,
                opacity,
                ref t,
                ref pivot,
                0,
                0,
                0);

            if (layerHandle == IntPtr.Zero)
                return null;

            return new ExportGroupLayer(Handle, layerHandle);
        }

        public ExportPaintLayer CreatePaintLayer(
            ExportGroupLayer parent,
            string name,
            bool visible = true,
            float opacity = 1.0f,
            Transform transform = null)
        {
            if (!IsValid || parent == null || !parent.IsValid)
                return null;

            TransformNative t = TransformUtils.ToNativeTransform(transform);
            TransformNative pivot = TransformUtils.IdentityTransform();

            IntPtr layerHandle = Native.ImmExporter_CreatePaintLayer(
                Handle,
                parent.Handle,
                name ?? "Paint",
                visible ? 1 : 0,
                opacity,
                ref t,
                ref pivot,
                0,
                0,
                0);

            if (layerHandle == IntPtr.Zero)
                return null;

            return new ExportPaintLayer(layerHandle);
        }

        public bool ExportToFile(string filePath, int opusBitrate = 96000, ExportAudioType audioType = ExportAudioType.Opus)
        {
            if (!IsValid || string.IsNullOrEmpty(filePath))
                return false;
            return Native.ImmExporter_ExportToFile(Handle, filePath, opusBitrate, (int)audioType);
        }

        public void Dispose()
        {
            if (Handle != IntPtr.Zero)
            {
                Native.ImmExporter_DestroySequence(Handle);
                Handle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

    }

    public sealed class ExportGroupLayer
    {
        internal IntPtr SequenceHandle { get; }
        internal IntPtr Handle { get; }
        public bool IsValid => Handle != IntPtr.Zero;

        internal ExportGroupLayer(IntPtr sequenceHandle, IntPtr layerHandle)
        {
            SequenceHandle = sequenceHandle;
            Handle = layerHandle;
        }

        public ExportGroupLayer CreateGroupLayer(
            string name,
            bool visible = true,
            float opacity = 1.0f,
            Transform transform = null)
        {
            if (!IsValid)
                return null;

            TransformNative t = TransformUtils.ToNativeTransform(transform);
            TransformNative pivot = TransformUtils.IdentityTransform();

            IntPtr layerHandle = Native.ImmExporter_CreateGroupLayer(
                SequenceHandle,
                Handle,
                name ?? "Group",
                visible ? 1 : 0,
                opacity,
                ref t,
                ref pivot,
                0,
                0,
                0);

            if (layerHandle == IntPtr.Zero)
                return null;

            return new ExportGroupLayer(SequenceHandle, layerHandle);
        }

        public ExportPaintLayer CreatePaintLayer(
            string name,
            bool visible = true,
            float opacity = 1.0f,
            Transform transform = null)
        {
            if (!IsValid)
                return null;

            TransformNative t = TransformUtils.ToNativeTransform(transform);
            TransformNative pivot = TransformUtils.IdentityTransform();

            IntPtr layerHandle = Native.ImmExporter_CreatePaintLayer(
                SequenceHandle,
                Handle,
                name ?? "Paint",
                visible ? 1 : 0,
                opacity,
                ref t,
                ref pivot,
                0,
                0,
                0);

            if (layerHandle == IntPtr.Zero)
                return null;

            return new ExportPaintLayer(layerHandle);
        }
    }

    public sealed class ExportPaintLayer
    {
        internal IntPtr Handle { get; }

        internal ExportPaintLayer(IntPtr handle)
        {
            Handle = handle;
        }

        public ExportDrawing CreateDrawing()
        {
            IntPtr drawingHandle = Native.ImmExporter_CreateDrawing(Handle);
            if (drawingHandle == IntPtr.Zero)
                return null;

            uint drawingIndex = Native.ImmExporter_GetDrawingIndex(drawingHandle);
            return new ExportDrawing(drawingHandle, Handle, drawingIndex);
        }
    }

    public sealed class ExportDrawing : IDisposable
    {
        private IntPtr _drawingHandle;
        private readonly IntPtr _paintLayerHandle;
        private readonly uint _drawingIndex;

        internal ExportDrawing(IntPtr drawingHandle, IntPtr paintLayerHandle, uint drawingIndex)
        {
            _drawingHandle = drawingHandle;
            _paintLayerHandle = paintLayerHandle;
            _drawingIndex = drawingIndex;
        }

        public bool Init(uint numElements, bool flipped = false)
        {
            return Native.ImmExporter_DrawingInit(_drawingHandle, numElements, flipped ? 1 : 0);
        }

        public ExportElement GetElement(uint elementIndex)
        {
            IntPtr elementHandle = Native.ImmExporter_DrawingGetElement(_drawingHandle, elementIndex);
            if (elementHandle == IntPtr.Zero)
                return null;
            return new ExportElement(elementHandle);
        }

        public void ComputeBounds()
        {
            Native.ImmExporter_ComputeDrawingBounds(_drawingHandle);
        }

        public void AddFrame()
        {
            Native.ImmExporter_PaintAddFrame(_paintLayerHandle, _drawingIndex);
        }

        public void Dispose()
        {
            if (_drawingHandle != IntPtr.Zero)
            {
                Native.ImmExporter_DestroyDrawing(_drawingHandle);
                _drawingHandle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }
    }

    public sealed class ExportElement
    {
        private readonly IntPtr _elementHandle;

        internal ExportElement(IntPtr elementHandle)
        {
            _elementHandle = elementHandle;
        }

        public bool Init(uint numPoints, BrushSectionType brushType, VisibilityType visibilityType)
        {
            return Native.ImmExporter_ElementInit(_elementHandle, numPoints, (int)brushType, (int)visibilityType);
        }

        public bool SetPoint(uint pointIndex, PaintPoint point)
        {
            PointNative native = new PointNative
            {
                Px = point.Position.x,
                Py = point.Position.y,
                Pz = point.Position.z,
                Nx = point.Normal.x,
                Ny = point.Normal.y,
                Nz = point.Normal.z,
                Dx = point.Direction.x,
                Dy = point.Direction.y,
                Dz = point.Direction.z,
                R = point.Color.r,
                G = point.Color.g,
                B = point.Color.b,
                A = point.Alpha,
                Width = point.Width,
                Length = point.Length,
                Time = point.Time
            };
            return Native.ImmExporter_ElementSetPoint(_elementHandle, pointIndex, ref native);
        }

        public void ComputeBounds()
        {
            Native.ImmExporter_ComputeElementBounds(_elementHandle);
        }
    }

    internal static class Native
    {
        private const string DllName = "ImmUnityPlugin";

        [DllImport(DllName)]
        public static extern IntPtr ImmExporter_CreateSequence(
            int type,
            int caps,
            float bgR,
            float bgG,
            float bgB,
            uint frameRate,
            long maxMemory,
            long maxRenderCalls,
            long maxTriangles,
            long maxSoundChannels);

        [DllImport(DllName)]
        public static extern void ImmExporter_DestroySequence(IntPtr sequenceHandle);

        [DllImport(DllName, CharSet = CharSet.Ansi)]
        public static extern IntPtr ImmExporter_CreatePaintLayer(
            IntPtr sequenceHandle,
            IntPtr parentLayerHandle,
            string name,
            int visible,
            float opacity,
            ref TransformNative transform,
            ref TransformNative pivot,
            int isTimeline,
            long durationTicks,
            uint maxRepeatCount);

        [DllImport(DllName, CharSet = CharSet.Ansi)]
        public static extern IntPtr ImmExporter_CreateGroupLayer(
            IntPtr sequenceHandle,
            IntPtr parentLayerHandle,
            string name,
            int visible,
            float opacity,
            ref TransformNative transform,
            ref TransformNative pivot,
            int isTimeline,
            long durationTicks,
            uint maxRepeatCount);

        [DllImport(DllName)]
        public static extern IntPtr ImmExporter_CreateDrawing(IntPtr paintLayerHandle);

        [DllImport(DllName)]
        public static extern void ImmExporter_DestroyDrawing(IntPtr drawingHandle);

        [DllImport(DllName)]
        public static extern uint ImmExporter_GetDrawingIndex(IntPtr drawingHandle);

        [DllImport(DllName)]
        public static extern bool ImmExporter_DrawingInit(IntPtr drawingHandle, uint numElements, int flipped);

        [DllImport(DllName)]
        public static extern IntPtr ImmExporter_DrawingGetElement(IntPtr drawingHandle, uint elementIndex);

        [DllImport(DllName)]
        public static extern bool ImmExporter_ElementInit(IntPtr elementHandle, uint numPoints, int brushSectionType, int visibilityType);

        [DllImport(DllName)]
        public static extern bool ImmExporter_ElementSetPoint(IntPtr elementHandle, uint pointIndex, ref PointNative point);

        [DllImport(DllName)]
        public static extern void ImmExporter_ComputeElementBounds(IntPtr elementHandle);

        [DllImport(DllName)]
        public static extern void ImmExporter_ComputeDrawingBounds(IntPtr drawingHandle);

        [DllImport(DllName)]
        public static extern void ImmExporter_PaintAddFrame(IntPtr paintLayerHandle, uint drawingIndex);

        [DllImport(DllName, CharSet = CharSet.Ansi)]
        public static extern bool ImmExporter_ExportToFile(IntPtr sequenceHandle, string fileName, int opusBitrate, int audioType);
    }

    internal static class TransformUtils
    {
        public static TransformNative IdentityTransform()
        {
            return new TransformNative
            {
                Tx = 0f,
                Ty = 0f,
                Tz = 0f,
                Qx = 0f,
                Qy = 0f,
                Qz = 0f,
                Qw = 1f,
                Scale = 1f
            };
        }

        public static TransformNative ToNativeTransform(Transform transform)
        {
            if (transform == null)
                return IdentityTransform();

            Quaternion q = transform.rotation;
            return new TransformNative
            {
                Tx = transform.position.x,
                Ty = transform.position.y,
                Tz = transform.position.z,
                Qx = q.x,
                Qy = q.y,
                Qz = q.z,
                Qw = q.w,
                Scale = transform.lossyScale.x
            };
        }
    }
}
