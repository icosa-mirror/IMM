using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace ImmPlayer
{
    /// <summary>
    /// Native P/Invoke declarations for the ImmStrokeReader DLL.
    /// This plugin reads IMM files and exposes raw stroke data (positions, colors, widths, etc.)
    /// without rendering. Independent from the ImmUnityPlugin rendering plugin.
    /// </summary>
    public static class ImmStrokeReader
    {
        private const string DllName = "ImmStrokeReader";

        [DllImport(DllName)]
        private static extern IntPtr StrokeReader_GetBuildId();

        public static string GetBuildId()
        {
            IntPtr p = StrokeReader_GetBuildId();
            return p == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(p);
        }

        #region Lifecycle API

        /// <summary>
        /// Initialize the stroke reader plugin.
        /// </summary>
        /// <param name="logFileName">Path to log file (null for default "imm_stroke_reader_log.txt")</param>
        /// <returns>0 on success, negative on error</returns>
        [DllImport(DllName)]
        public static extern int StrokeReader_Init(string logFileName);

        /// <summary>
        /// Shut down the stroke reader plugin and release all resources.
        /// </summary>
        [DllImport(DllName)]
        public static extern void StrokeReader_End();

        /// <summary>
        /// Check if the stroke reader plugin is initialized.
        /// </summary>
        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_IsInitialized();

        #endregion

        #region Loading API

        /// <summary>
        /// Load an IMM document from a file path.
        /// </summary>
        /// <param name="fileName">Path to the IMM file</param>
        /// <returns>Document ID (positive) on success, negative on error:
        /// -1 = not initialized, -2 = invalid filename, -3 = import failed</returns>
        [DllImport(DllName)]
        public static extern int StrokeReader_LoadFromFile(string fileName);

        /// <summary>
        /// Load an IMM document from memory.
        /// </summary>
        /// <param name="data">Pointer to IMM file data in memory</param>
        /// <param name="size">Size of data in bytes</param>
        /// <returns>Document ID (positive) on success, negative on error</returns>
        [DllImport(DllName)]
        public static extern int StrokeReader_LoadFromMemory(IntPtr data, int size);

        /// <summary>
        /// Unload a document and release its resources.
        /// </summary>
        /// <param name="docId">Document ID returned from LoadFromFile/LoadFromMemory</param>
        [DllImport(DllName)]
        public static extern void StrokeReader_Unload(int docId);

        /// <summary>
        /// Get the number of currently loaded documents.
        /// </summary>
        [DllImport(DllName)]
        public static extern int StrokeReader_GetDocumentCount();

        [DllImport(DllName)]
        public static extern int StrokeReader_GetChapterCountFromFile(string fileName);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetChapterCount(int docId);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetCurrentChapter(int docId);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_SetChapter(int docId, int chapterIndex);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetDrawingIndexForChapter(int docId, int layerIdx, int chapterIndex);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetLayerAnimationInfo(int docId, int layerIdx, out int frameRate, out int numFrames, out int maxRepeatCount);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetFrameBuffer(int docId, int layerIdx, [Out] int[] frames, int maxFrames);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetAuthoringLayerAnimationInfo(int docId, int layerIdx, out int frameRate, out int numFrames, out int maxRepeatCount);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetAuthoringFrameBuffer(int docId, int layerIdx, [Out] int[] frames, int maxFrames);

        #endregion

        #region Query API

        /// <summary>
        /// Get the number of layers in a document.
        /// </summary>
        /// <param name="docId">Document ID</param>
        /// <returns>Number of layers, or 0 if docId is invalid</returns>
        [DllImport(DllName)]
        public static extern int StrokeReader_GetLayerCount(int docId);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetAuthoringLayerCount(int docId);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetDocumentInfo(int docId, out StrokeDocumentInfo info);

        /// <summary>
        /// Get information about a layer.
        /// </summary>
        /// <param name="docId">Document ID</param>
        /// <param name="layerIdx">Layer index (0-based)</param>
        /// <param name="info">Output layer info struct</param>
        /// <returns>True on success, false if indices are invalid</returns>
        [DllImport(DllName, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetLayerInfo(int docId, int layerIdx, out StrokeLayerInfo info);

        [DllImport(DllName, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetAuthoringLayerInfo(int docId, int layerIdx, out StrokeAuthoringLayerInfo info);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetLayerTransform(int docId, int layerIdx, out StrokeLayerTransform local, out StrokeLayerTransform world);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetAuthoringLayerTransform(int docId, int layerIdx, out StrokeLayerTransform local, out StrokeLayerTransform world);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetLayerAnimationKeyCount(int docId, int layerIdx);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetLayerAnimationKey(
            int docId,
            int layerIdx,
            int keyIdx,
            out StrokeAnimationKey key);

        /// <summary>
        /// Get the number of drawings in a layer.
        /// </summary>
        /// <param name="docId">Document ID</param>
        /// <param name="layerIdx">Layer index</param>
        /// <returns>Number of drawings, or 0 if indices are invalid</returns>
        [DllImport(DllName)]
        public static extern int StrokeReader_GetDrawingCount(int docId, int layerIdx);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetAuthoringDrawingCount(int docId, int layerIdx);

        /// <summary>
        /// Get the number of strokes in a drawing.
        /// </summary>
        /// <param name="docId">Document ID</param>
        /// <param name="layerIdx">Layer index</param>
        /// <param name="drawingIdx">Drawing index</param>
        /// <returns>Number of strokes, or 0 if indices are invalid</returns>
        [DllImport(DllName)]
        public static extern int StrokeReader_GetStrokeCount(int docId, int layerIdx, int drawingIdx);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetAuthoringStrokeCount(int docId, int layerIdx, int drawingIdx);

        /// <summary>
        /// Get information about a stroke (brush type, visibility, point count, bounding box).
        /// </summary>
        /// <param name="docId">Document ID</param>
        /// <param name="layerIdx">Layer index</param>
        /// <param name="drawingIdx">Drawing index</param>
        /// <param name="strokeIdx">Stroke index</param>
        /// <param name="info">Output stroke info struct</param>
        /// <returns>True on success, false if indices are invalid</returns>
        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetStrokeInfo(
            int docId, int layerIdx, int drawingIdx, int strokeIdx,
            out StrokeInfo info);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetAuthoringStrokeInfo(
            int docId, int layerIdx, int drawingIdx, int strokeIdx,
            out StrokeInfo info);

        /// <summary>
        /// Get the point data for a stroke.
        /// </summary>
        /// <param name="docId">Document ID</param>
        /// <param name="layerIdx">Layer index</param>
        /// <param name="drawingIdx">Drawing index</param>
        /// <param name="strokeIdx">Stroke index</param>
        /// <param name="points">Pre-allocated array to receive point data</param>
        /// <param name="maxPoints">Maximum number of points to copy (size of points array)</param>
        /// <returns>True on success, false if indices are invalid or points is null</returns>
        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetStrokePoints(
            int docId, int layerIdx, int drawingIdx, int strokeIdx,
            [Out] StrokePoint[] points, int maxPoints);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetAuthoringStrokePoints(
            int docId, int layerIdx, int drawingIdx, int strokeIdx,
            [Out] StrokeAuthoringPoint[] points, int maxPoints);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool StrokeReader_GetPictureInfo(
            int docId, int layerIdx, out StrokePictureInfo info);

        [DllImport(DllName)]
        public static extern int StrokeReader_GetPicturePixelData(
            int docId, int layerIdx, IntPtr pixels, int maxBytes);

        #endregion
    }

    #region Data Structures

    /// <summary>
    /// Information about a layer in an IMM document.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct StrokeLayerInfo
    {
        /// <summary>Layer ID</summary>
        public int id;

        /// <summary>Layer type (0 = paint, 1 = group, etc.)</summary>
        public int type;

        /// <summary>Number of drawings in this layer</summary>
        public int numDrawings;

        /// <summary>Layer name (up to 128 characters)</summary>
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string name;

        /// <summary>Layer visible flag (0/1)</summary>
        public int visible;

        /// <summary>Layer opacity (0-1)</summary>
        public float opacity;

        /// <summary>Whether this spawn-area layer is the document default.</summary>
        public int isDefaultSpawn;

        /// <summary>Pivot rotation (x,y,z,w)</summary>
        public float pivotRotX;
        public float pivotRotY;
        public float pivotRotZ;
        public float pivotRotW;

        /// <summary>Pivot scale</summary>
        public float pivotScale;

        /// <summary>Pivot flip</summary>
        public int pivotFlip;

        /// <summary>Pivot translation</summary>
        public float pivotTransX;
        public float pivotTransY;
        public float pivotTransZ;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct StrokeAuthoringLayerInfo
    {
        public StrokeLayerInfo legacy;
        public int parentId;
        public int childIndex;
        public int isTimeline;
        public long durationTicks;
        public uint maxRepeatCount;

        public int id => legacy.id;
        public int type => legacy.type;
        public int numDrawings => legacy.numDrawings;
        public string name => legacy.name;
        public int visible => legacy.visible;
        public float opacity => legacy.opacity;
        public int isDefaultSpawn => legacy.isDefaultSpawn;
        public float pivotRotX => legacy.pivotRotX;
        public float pivotRotY => legacy.pivotRotY;
        public float pivotRotZ => legacy.pivotRotZ;
        public float pivotRotW => legacy.pivotRotW;
        public float pivotScale => legacy.pivotScale;
        public int pivotFlip => legacy.pivotFlip;
        public float pivotTransX => legacy.pivotTransX;
        public float pivotTransY => legacy.pivotTransY;
        public float pivotTransZ => legacy.pivotTransZ;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StrokeLayerTransform
    {
        public float rotX;
        public float rotY;
        public float rotZ;
        public float rotW;
        public float scale;
        public int flip;
        public float transX;
        public float transY;
        public float transZ;
    }

    /// <summary>
    /// Information about a stroke (brush type, point count, bounding box).
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct StrokeInfo
    {
        /// <summary>Brush type ID</summary>
        public int brushType;

        /// <summary>Visibility mode</summary>
        public int visibilityMode;

        /// <summary>Number of points in the stroke</summary>
        public int numPoints;

        /// <summary>Bounding box minimum X</summary>
        public float bboxMinX;
        /// <summary>Bounding box minimum Y</summary>
        public float bboxMinY;
        /// <summary>Bounding box minimum Z</summary>
        public float bboxMinZ;

        /// <summary>Bounding box maximum X</summary>
        public float bboxMaxX;
        /// <summary>Bounding box maximum Y</summary>
        public float bboxMaxY;
        /// <summary>Bounding box maximum Z</summary>
        public float bboxMaxZ;

        /// <summary>
        /// Get the bounding box as a Unity Bounds object.
        /// </summary>
        public Bounds GetBounds()
        {
            Vector3 min = new Vector3(bboxMinX, bboxMinY, bboxMinZ);
            Vector3 max = new Vector3(bboxMaxX, bboxMaxY, bboxMaxZ);
            Vector3 center = (min + max) * 0.5f;
            Vector3 size = max - min;
            return new Bounds(center, size);
        }
    }

    /// <summary>
    /// Point data for a stroke vertex.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct StrokePoint
    {
        /// <summary>Position X</summary>
        public float px;
        /// <summary>Position Y</summary>
        public float py;
        /// <summary>Position Z</summary>
        public float pz;

        /// <summary>Normal X</summary>
        public float nx;
        /// <summary>Normal Y</summary>
        public float ny;
        /// <summary>Normal Z</summary>
        public float nz;

        /// <summary>View direction X</summary>
        public float dx;
        /// <summary>View direction Y</summary>
        public float dy;
        /// <summary>View direction Z</summary>
        public float dz;

        /// <summary>Color red (0-1, gamma space)</summary>
        public float r;
        /// <summary>Color green (0-1, gamma space)</summary>
        public float g;
        /// <summary>Color blue (0-1, gamma space)</summary>
        public float b;

        /// <summary>Alpha/transparency (0-1)</summary>
        public float alpha;

        /// <summary>Stroke width (quantized, may need conversion)</summary>
        public float width;

        /// <summary>Get position as Vector3</summary>
        public Vector3 Position => new Vector3(px, py, pz);

        /// <summary>Get normal as Vector3</summary>
        public Vector3 Normal => new Vector3(nx, ny, nz);

        /// <summary>Get view direction as Vector3</summary>
        public Vector3 ViewDirection => new Vector3(dx, dy, dz);

        /// <summary>Get color as Color (with alpha)</summary>
        public Color Color => new Color(r, g, b, alpha);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StrokeAuthoringPoint
    {
        public StrokePoint legacy;
        public float length;
        public float time;

        public float r => legacy.r;
        public float g => legacy.g;
        public float b => legacy.b;
        public float alpha => legacy.alpha;
        public float width => legacy.width;
        public Vector3 Position => legacy.Position;
        public Vector3 Normal => legacy.Normal;
        public Vector3 ViewDirection => legacy.ViewDirection;
        public Color Color => legacy.Color;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StrokeDocumentInfo
    {
        public int sequenceType;
        public uint frameRate;
        public float backgroundR;
        public float backgroundG;
        public float backgroundB;
        public uint capabilities;
        public int rootAnimationKeyCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StrokeAnimationKey
    {
        public int property;
        public long timeTicks;
        public int interpolation;
        public int boolValue;
        public uint intValue;
        public float floatValue;
        public double doubleValue;
        public StrokeLayerTransform transformValue;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StrokePictureInfo
    {
        public int layerId;
        public int contentType;
        public int isViewerLocked;
        public int width;
        public int height;
        public int hasAlpha;
        public int dataSize;
    }

    #endregion

    /// <summary>
    /// High-level wrapper for ImmStrokeReader with automatic initialization and cleanup.
    /// </summary>
    public class StrokeReaderDocument : IDisposable
    {
        private int _docId = -1;
        private bool _disposed = false;

        /// <summary>
        /// Get the document ID (negative if not loaded).
        /// </summary>
        public int DocId => _docId;

        /// <summary>
        /// Check if a document is loaded.
        /// </summary>
        public bool IsLoaded => _docId > 0;

        /// <summary>
        /// Load an IMM file and return the stroke data.
        /// Automatically initializes the plugin if needed.
        /// </summary>
        /// <param name="filePath">Path to the IMM file</param>
        /// <param name="logPath">Optional log file path</param>
        /// <returns>True on success</returns>
        public bool Load(string filePath, string logPath = null, int chapterIndex = -1)
        {
            if (_docId > 0)
            {
                Debug.LogWarning("StrokeReaderDocument: Already loaded. Call Unload() first.");
                return false;
            }

            // Initialize plugin if needed
            if (!ImmStrokeReader.StrokeReader_IsInitialized())
            {
                int initResult = ImmStrokeReader.StrokeReader_Init(logPath);
                if (initResult != 0)
                {
                    Debug.LogError($"StrokeReaderDocument: Failed to initialize plugin (error {initResult})");
                    return false;
                }
            }

            _docId = ImmStrokeReader.StrokeReader_LoadFromFile(filePath);
            if (_docId < 0)
            {
                Debug.LogError($"StrokeReaderDocument: Failed to load '{filePath}' (error {_docId})");
                return false;
            }

            if (chapterIndex >= 0)
            {
                int chapterCount = ImmStrokeReader.StrokeReader_GetChapterCount(_docId);
                if (chapterIndex >= chapterCount)
                {
                    Debug.LogWarning($"StrokeReaderDocument: chapterIndex {chapterIndex} is out of range (chapterCount={chapterCount}).");
                }
                else
                {
                    ImmStrokeReader.StrokeReader_SetChapter(_docId, chapterIndex);
                }
            }

            return true;
        }

        public bool Load(byte[] immBytes, string logPath = null, int chapterIndex = -1)
        {
            if (_docId > 0 || immBytes == null || immBytes.Length == 0)
                return false;
            if (!ImmStrokeReader.StrokeReader_IsInitialized() && ImmStrokeReader.StrokeReader_Init(logPath) != 0)
                return false;

            GCHandle pinned = GCHandle.Alloc(immBytes, GCHandleType.Pinned);
            try
            {
                _docId = ImmStrokeReader.StrokeReader_LoadFromMemory(pinned.AddrOfPinnedObject(), immBytes.Length);
            }
            finally
            {
                pinned.Free();
            }
            if (_docId < 0)
                return false;

            if (chapterIndex >= 0)
            {
                int chapterCount = ImmStrokeReader.StrokeReader_GetChapterCount(_docId);
                if (chapterIndex >= chapterCount || !ImmStrokeReader.StrokeReader_SetChapter(_docId, chapterIndex))
                {
                    Unload();
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Unload the document.
        /// </summary>
        public void Unload()
        {
            if (_docId > 0)
            {
                ImmStrokeReader.StrokeReader_Unload(_docId);
                _docId = -1;
            }
        }

        /// <summary>
        /// Get layer count.
        /// </summary>
        public int LayerCount => _docId > 0 ? ImmStrokeReader.StrokeReader_GetLayerCount(_docId) : 0;

        public int AuthoringLayerCount => _docId > 0 ? ImmStrokeReader.StrokeReader_GetAuthoringLayerCount(_docId) : 0;

        public bool GetDocumentInfo(out StrokeDocumentInfo info)
        {
            info = default;
            return _docId > 0 && ImmStrokeReader.StrokeReader_GetDocumentInfo(_docId, out info);
        }

        public int ChapterCount => _docId > 0 ? ImmStrokeReader.StrokeReader_GetChapterCount(_docId) : 0;

        public int CurrentChapter => _docId > 0 ? ImmStrokeReader.StrokeReader_GetCurrentChapter(_docId) : 0;

        public bool SetChapter(int chapterIndex)
        {
            if (_docId <= 0)
                return false;
            if (chapterIndex < 0)
                return false;

            return ImmStrokeReader.StrokeReader_SetChapter(_docId, chapterIndex);
        }

        /// <summary>
        /// Get layer info by index.
        /// </summary>
        public bool GetLayerInfo(int layerIdx, out StrokeLayerInfo info)
        {
            info = default;
            if (_docId <= 0) return false;
            return ImmStrokeReader.StrokeReader_GetLayerInfo(_docId, layerIdx, out info);
        }

        public bool GetAuthoringLayerInfo(int layerIdx, out StrokeAuthoringLayerInfo info)
        {
            info = default;
            return _docId > 0 && ImmStrokeReader.StrokeReader_GetAuthoringLayerInfo(_docId, layerIdx, out info);
        }

        public bool GetAuthoringLayerTransform(int layerIdx, out StrokeLayerTransform local, out StrokeLayerTransform world)
        {
            local = default;
            world = default;
            return _docId > 0 &&
                ImmStrokeReader.StrokeReader_GetAuthoringLayerTransform(_docId, layerIdx, out local, out world);
        }

        public StrokeAnimationKey[] GetLayerAnimationKeys(int layerIdx)
        {
            if (_docId <= 0)
                return Array.Empty<StrokeAnimationKey>();
            int count = ImmStrokeReader.StrokeReader_GetLayerAnimationKeyCount(_docId, layerIdx);
            if (count <= 0)
                return Array.Empty<StrokeAnimationKey>();
            StrokeAnimationKey[] keys = new StrokeAnimationKey[count];
            for (int keyIndex = 0; keyIndex < count; keyIndex++)
            {
                if (!ImmStrokeReader.StrokeReader_GetLayerAnimationKey(_docId, layerIdx, keyIndex, out keys[keyIndex]))
                    return null;
            }
            return keys;
        }

        public int GetAuthoringDrawingCount(int layerIdx)
        {
            return _docId > 0 ? ImmStrokeReader.StrokeReader_GetAuthoringDrawingCount(_docId, layerIdx) : 0;
        }

        public int GetAuthoringStrokeCount(int layerIdx, int drawingIdx)
        {
            return _docId > 0 ? ImmStrokeReader.StrokeReader_GetAuthoringStrokeCount(_docId, layerIdx, drawingIdx) : 0;
        }

        public bool GetAuthoringStrokeInfo(int layerIdx, int drawingIdx, int strokeIdx, out StrokeInfo info)
        {
            info = default;
            return _docId > 0 &&
                ImmStrokeReader.StrokeReader_GetAuthoringStrokeInfo(_docId, layerIdx, drawingIdx, strokeIdx, out info);
        }

        public StrokeAuthoringPoint[] GetAuthoringStrokePoints(int layerIdx, int drawingIdx, int strokeIdx)
        {
            if (!GetAuthoringStrokeInfo(layerIdx, drawingIdx, strokeIdx, out StrokeInfo info))
                return null;
            if (info.numPoints <= 0)
                return Array.Empty<StrokeAuthoringPoint>();
            StrokeAuthoringPoint[] points = new StrokeAuthoringPoint[info.numPoints];
            return ImmStrokeReader.StrokeReader_GetAuthoringStrokePoints(
                _docId,
                layerIdx,
                drawingIdx,
                strokeIdx,
                points,
                points.Length)
                ? points
                : null;
        }

        /// <summary>
        /// Get drawing count for a layer.
        /// </summary>
        public int GetDrawingCount(int layerIdx)
        {
            return _docId > 0 ? ImmStrokeReader.StrokeReader_GetDrawingCount(_docId, layerIdx) : 0;
        }

        /// <summary>
        /// Get stroke count for a drawing.
        /// </summary>
        public int GetStrokeCount(int layerIdx, int drawingIdx)
        {
            return _docId > 0 ? ImmStrokeReader.StrokeReader_GetStrokeCount(_docId, layerIdx, drawingIdx) : 0;
        }

        /// <summary>
        /// Get stroke info.
        /// </summary>
        public bool GetStrokeInfo(int layerIdx, int drawingIdx, int strokeIdx, out StrokeInfo info)
        {
            info = default;
            if (_docId <= 0) return false;
            return ImmStrokeReader.StrokeReader_GetStrokeInfo(_docId, layerIdx, drawingIdx, strokeIdx, out info);
        }

        /// <summary>
        /// Get stroke points.
        /// </summary>
        /// <param name="layerIdx">Layer index</param>
        /// <param name="drawingIdx">Drawing index</param>
        /// <param name="strokeIdx">Stroke index</param>
        /// <returns>Array of points, or null on failure</returns>
        public StrokePoint[] GetStrokePoints(int layerIdx, int drawingIdx, int strokeIdx)
        {
            if (_docId <= 0) return null;

            if (!ImmStrokeReader.StrokeReader_GetStrokeInfo(_docId, layerIdx, drawingIdx, strokeIdx, out StrokeInfo info))
                return null;

            if (info.numPoints <= 0) return Array.Empty<StrokePoint>();

            StrokePoint[] points = new StrokePoint[info.numPoints];
            if (!ImmStrokeReader.StrokeReader_GetStrokePoints(_docId, layerIdx, drawingIdx, strokeIdx, points, info.numPoints))
                return null;

            return points;
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                Unload();
                _disposed = true;
            }
        }

        ~StrokeReaderDocument()
        {
            Dispose(false);
        }
    }
}
