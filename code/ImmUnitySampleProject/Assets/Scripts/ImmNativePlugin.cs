using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer
{
    /// <summary>
    /// Native P/Invoke declarations for the ImmUnityPlugin DLL
    /// </summary>
    public static class ImmNativePlugin
    {
        private const string DllName = "ImmUnityPlugin";

        #region Unity Plugin Callbacks

        [DllImport(DllName)]
        public static extern IntPtr GetRenderEventFunc();

        [DllImport(DllName)]
        public static extern void Debug();

        #endregion

        #region Initialization and Cleanup

        /// <summary>
        /// Initialize the IMM player
        /// </summary>
        /// <param name="colorSpace">0 = linear, 1 = gamma</param>
        /// <param name="antialiasing">Anti-aliasing level (e.g., 8)</param>
        /// <param name="logFileName">Path to log file</param>
        /// <param name="tmpFolderName">Path to temporary folder (use Application.temporaryCachePath)</param>
        /// <returns>0 on success, negative on error</returns>
        [DllImport(DllName)]
        public static extern int Init(int colorSpace, int antialiasing, string logFileName, string tmpFolderName);

        [DllImport(DllName)]
        public static extern void End();

        #endregion

        #region Frame Updates

        /// <summary>
        /// Global work to be done once per frame
        /// </summary>
        /// <param name="enabled">1 to enable, 0 to disable</param>
        [DllImport(DllName)]
        public static extern void GlobalWork(int enabled);

        /// <summary>
        /// Set camera matrices for rendering
        /// </summary>
        [DllImport(DllName)]
        public static extern void SetMatrices(
            int cameraID,
            int stereoType,
            float[] world2head,
            float[] prjHead,
            float[] world2leye,
            float[] prjLeft,
            float[] world2reye,
            float[] prjRight);

        #endregion

        #region Document Loading and Unloading

        /// <summary>
        /// Load an IMM document from a file
        /// </summary>
        /// <param name="fileName">Path to the IMM file</param>
        /// <returns>Document ID, or negative on error</returns>
        [DllImport(DllName)]
        public static extern int LoadFromFile(string fileName);

        /// <summary>
        /// Load an IMM document from memory
        /// </summary>
        /// <param name="fileName">File name for reference</param>
        /// <param name="size">Size of data in bytes</param>
        /// <param name="data">Pointer to data</param>
        /// <returns>Document ID, or negative on error</returns>
        [DllImport(DllName)]
        public static extern int LoadFromMemory(string fileName, int size, IntPtr data);

        /// <summary>
        /// Unload a document
        /// </summary>
        /// <param name="id">Document ID</param>
        [DllImport(DllName)]
        public static extern void Unload(int id);

        #endregion

        #region Document Transform

        /// <summary>
        /// Set the document-to-world transformation matrix
        /// </summary>
        [DllImport(DllName)]
        public static extern void SetDocumentToWorld(int id, float[] doc2world);

        #endregion

        #region Layer Editing

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool SetLayerVisible(int docId, int layerId, int visible);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool ClearLayerVisibilityOverride(int docId, int layerId);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool SetLayerOpacity(int docId, int layerId, float opacity);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool SetLayerTransform(int docId, int layerId, float[] layerToWorld);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool ClearLayerTransformOverride(int docId, int layerId);

        [DllImport(DllName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool GetLayerDiagnostics(int docId, int layerId, out LayerDiagnosticsNative diag);

        #endregion

        #region Playback Control

        [DllImport(DllName)]
        public static extern void Pause(int id);

        [DllImport(DllName)]
        public static extern void Resume(int id);

        [DllImport(DllName)]
        public static extern void Hide(int id);

        [DllImport(DllName)]
        public static extern void Show(int id);

        [DllImport(DllName)]
        public static extern void Continue(int id);

        [DllImport(DllName)]
        public static extern void SkipForward(int id);

        [DllImport(DllName)]
        public static extern void SkipBack(int id);

        [DllImport(DllName)]
        public static extern void Restart(int id);

        #endregion

        #region Chapter Navigation

        [DllImport(DllName)]
        public static extern int GetChapterCount(int id);

        [DllImport(DllName)]
        public static extern int GetCurrentChapter(int id);

        #endregion

        #region Time Control

        [DllImport(DllName)]
        public static extern void SetTime(int id, long timeSinceStart, long timeSinceStop);

        [DllImport(DllName)]
        public static extern void GetTime(int id, out long timeSinceStart, out long timeSinceStop);

        [DllImport(DllName)]
        public static extern long GetPlayTime(int id);

        #endregion

        #region Document Info and State

        [DllImport(DllName)]
        public static extern void GetPlayerInfo(out PlayerInfo info);

        [DllImport(DllName)]
        public static extern void GetDocumentState(out DocumentState state, int id);

        [DllImport(DllName)]
        public static extern uint GetDocumentInfoEx(int id);

        #endregion

        #region Audio Control

        [DllImport(DllName)]
        public static extern float GetSound(int id);

        [DllImport(DllName)]
        public static extern void SetSound(int id, float volume);

        #endregion

        #region Bounding Box

        [DllImport(DllName)]
        public static extern void GetBoundingBox(int id, out Bounds3 bound);

        [DllImport(DllName)]
        public static extern bool IsSequenceReady(int docId);

        [DllImport(DllName)]
        public static extern int GetLayerCount(int docId);

        [DllImport(DllName, CharSet = CharSet.Unicode)]
        public static extern bool GetLayerInfoByIndex(int docId, int index, out LayerInfoNative info);

        #endregion

        #region Spawn Areas

        [DllImport(DllName)]
        public static extern int GetSpawnAreaCount(int docId);

        [DllImport(DllName)]
        public static extern int GetSpawnAreaList(int docId, int spawnAreaIdsSize, int[] pSpawnAreaIds);

        [DllImport(DllName)]
        public static extern int GetActiveSpawnAreaId(int docId);

        [DllImport(DllName)]
        public static extern void SetActiveSpawnAreaId(int docId, int activeSpawnAreaId);

        [DllImport(DllName)]
        public static extern bool GetSpawnAreaInfo(int docId, int spawnareaId, out SerializedSpawnArea serializedSpawnArea);

        #endregion
    }

    #region Data Structures

    [StructLayout(LayoutKind.Sequential)]
    public struct PlayerInfo
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct BackgroundColor
        {
            public float red;
            public float green;
            public float blue;
        }

        public BackgroundColor backgroundColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DocumentState
    {
        public int loadingState;
        public int playbackState;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Bounds3
    {
        public float minX, minY, minZ;
        public float maxX, maxY, maxZ;

        public UnityEngine.Bounds ToUnityBounds()
        {
            Vector3 min = new Vector3(minX, minY, minZ);
            Vector3 max = new Vector3(maxX, maxY, maxZ);
            Vector3 center = (min + max) * 0.5f;
            Vector3 size = max - min;
            return new UnityEngine.Bounds(center, size);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct LayerBounds3
    {
        public float minX, maxX;
        public float minY, maxY;
        public float minZ, maxZ;

        public UnityEngine.Bounds ToUnityBounds()
        {
            Vector3 min = new Vector3(minX, minY, minZ);
            Vector3 max = new Vector3(maxX, maxY, maxZ);
            Vector3 center = (min + max) * 0.5f;
            Vector3 size = max - min;
            return new UnityEngine.Bounds(center, size);
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct LayerInfoNative
    {
        public int id;
        public int type;
        public int parentId;
        public int isTimeline;
        public int isLoaded;
        public int isVisible;
        public float opacity;
        public int hasBBox;
        public LayerBounds3 bbox;
        public int numChildren;
        public int assetId;
        public int paintNumDrawings;
        public int paintNumFrames;
        public int paintNumStrokes;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string name;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string fullName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct LayerDiagnosticsNative
    {
        public int hasVisibilityKeys;
        public int hasOpacityKeys;
        public int isVisible;
        public float opacity;
        public int isWorldVisible;
        public float worldOpacity;
        public int parentId;
        public int visibilityOverrideEnabled;
        public int visibilityOverrideValue;
        public int hasTransformKeys;
        public int transformOverrideEnabled;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SerializedSpawnArea
    {
        public enum Type : uint
        {
            EyeLevel = 0,
            FloorLevel = 1,
        }

        public enum VolumeType
        {
            Sphere = 0,
            Box = 1,
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Volume
        {
            public VolumeType type;

            [StructLayout(LayoutKind.Sequential)]
            public struct SphereExtent
            {
                public float r;
            }

            [StructLayout(LayoutKind.Sequential)]
            public struct BoxExtent
            {
                public float x, y, z;
            }

            [StructLayout(LayoutKind.Sequential)]
            public struct Offset
            {
                public float x, y, z;
            }

            public SphereExtent sphereExtent;
            public BoxExtent boxExtent;
            public Offset offset;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Transform
        {
            public float posx;
            public float posy;
            public float posz;
            public float rotx;
            public float roty;
            public float rotz;
            public float rotw;
            public float sca;

            public UnityEngine.Vector3 GetPosition()
            {
                return new UnityEngine.Vector3(posx, posy, posz);
            }

            public UnityEngine.Quaternion GetRotation()
            {
                return new UnityEngine.Quaternion(rotx, roty, rotz, rotw);
            }

            public float GetScale()
            {
                return sca;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Screenshot
        {
            public uint format;
            public int width;
            public int height;
            public IntPtr pData;
        }

        public IntPtr mName;
        public int mVersion;
        public Type mType;
        [MarshalAs(UnmanagedType.I1)]
        public bool mAnimated;
        public Volume volume;
        public Transform transform;
        public int locomotion;
        public Screenshot screenshot;

        public string GetName()
        {
            return Marshal.PtrToStringUni(mName);
        }
    }

    #endregion
}
