using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR;

namespace ImmPlayer
{
    /// <summary>
    /// High-level manager for the IMM Player plugin
    /// Handles initialization, rendering, and document management
    /// </summary>
    public class ImmPlayerManager : MonoBehaviour
    {
        
        private const string LogPrefix = "[IMM] ";
        private static void Log(string message) => Debug.Log(LogPrefix + message);
        private static void LogWarning(string message) => Debug.LogWarning(LogPrefix + message);
        private static void LogError(string message) => Debug.LogError(LogPrefix + message);
#region Singleton

        private static ImmPlayerManager _instance;
        public static ImmPlayerManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = FindObjectOfType<ImmPlayerManager>();
                    if (_instance == null)
                    {
                        GameObject go = new GameObject("ImmPlayerManager");
                        _instance = go.AddComponent<ImmPlayerManager>();
                        DontDestroyOnLoad(go);
                    }
                }
                return _instance;
            }
        }

        #endregion

        #region Settings

        [Header("Player Settings")]
        [SerializeField] private bool useLinearColorSpace = true;
        [SerializeField] private int antialiasingLevel = 8;
        [SerializeField] private string logFileName = "imm_player_log.txt";
        [SerializeField] private bool forceImmediateIssueInXR = false;

        #endregion

        #region State

        private bool _isInitialized = false;
        private Dictionary<int, ImmDocument> _loadedDocuments = new Dictionary<int, ImmDocument>();
        private Dictionary<int, IntPtr> _documentMemoryPtrs = new Dictionary<int, IntPtr>(); // Track memory for async loading
        private IntPtr _renderEventFunc = IntPtr.Zero;
        private readonly Dictionary<Camera, PerCameraInfo> _cameras = new Dictionary<Camera, PerCameraInfo>();
        private readonly Dictionary<Camera, float> _lastNearClipLogged = new Dictionary<Camera, float>();
        private const string NearDiagPrefix = "[IMMDBG_NEAR_20260208A] ";
        private const string XrStereoDiagPrefix = "[IMMDBG_XRSTEREO_20260212A] ";
        private bool _useCommandBufferRendering = false;

        #endregion

        #region Unity Lifecycle

        private void Awake()
        {
            if (_instance != null && _instance != this)
            {
                Destroy(gameObject);
                return;
            }
            _instance = this;
            DontDestroyOnLoad(gameObject);
        }

        private void Start()
        {
            Initialize();
        }

        private void OnEnable()
        {
            _useCommandBufferRendering = GraphicsSettings.currentRenderPipeline == null;
            if (_useCommandBufferRendering)
            {
                Camera.onPreCull += OnCameraPreCull;
            }
        }

        private void OnDisable()
        {
            if (_useCommandBufferRendering)
            {
                Camera.onPreCull -= OnCameraPreCull;
            }
            CleanupCommandBuffers();
        }

        private void LateUpdate()
        {
            if (_isInitialized)
            {
                ImmNativePlugin.GlobalWork(1);
            }
        }

        private void OnDestroy()
        {
            Shutdown();
        }

        private void OnApplicationQuit()
        {
            Shutdown();
        }

        #endregion

        #region Initialization

        public bool Initialize()
        {
            if (_isInitialized)
            {
                LogWarning("ImmPlayerManager is already initialized");
                return true;
            }

            Log("=== IMM Player Initialization Started ===");

#if UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
            if (SystemInfo.graphicsDeviceType != UnityEngine.Rendering.GraphicsDeviceType.OpenGLCore)
            {
                LogError("IMM Player requires OpenGL Core on macOS.");
                LogError($"Current Graphics API: {SystemInfo.graphicsDeviceType}");
                LogError("Switch the macOS Graphics API to OpenGLCore and restart Unity (or build a macOS player with OpenGLCore). ");
                return false;
            }
#endif

            int colorSpace = useLinearColorSpace ? 0 : 1;

            // Use Unity's temporary cache path for temporary files
            string tempFolder = Application.temporaryCachePath;

            try
            {
                int result = ImmNativePlugin.Init(colorSpace, antialiasingLevel, logFileName, tempFolder);

                if (result < 0)
                {
                    LogError($"Failed to initialize IMM Player. Error code: {result}");
                    LogError("Possible causes:");
                    LogError("  1. Missing DLL dependencies in Assets/Plugins/x86_64/");
                    LogError("  2. DLL platform settings incorrect (must be x86_64, Standalone + Editor)");
                    LogError("  3. Graphics API not supported (requires DirectX 11 or OpenGL Core; Metal is not supported)");
                    LogError($"  4. Check native log file: {logFileName}");
                    return false;
                }

                _renderEventFunc = ImmNativePlugin.GetRenderEventFunc();
                _isInitialized = true;

                Log("=== IMM Player Initialized Successfully ===");
                return true;
            }
            catch (System.DllNotFoundException ex)
            {
                LogError("=== DLL NOT FOUND ERROR ===");
                LogError($"Could not load ImmUnityPlugin.dll or one of its dependencies: {ex.Message}");
                LogError("Required DLLs in Assets/Plugins/x86_64/:");
                LogError("  - ImmUnityPlugin.dll");
                LogError("  - Audio360.dll");
                LogError("  - jpeg62.dll, libpng16.dll");
                LogError("  - ogg.dll, opus.dll, opusenc.dll");
                LogError("  - vorbis.dll, vorbisenc.dll");
                LogError("  - zlib1.dll");
                LogError("Make sure all DLLs have correct import settings!");
                return false;
            }
            catch (System.Exception ex)
            {
                LogError($"=== UNEXPECTED ERROR ===");
                LogError($"Exception during initialization: {ex.GetType().Name}");
                LogError($"Message: {ex.Message}");
                LogError($"Stack: {ex.StackTrace}");
                return false;
            }
        }

        public void Shutdown()
        {
            if (!_isInitialized)
                return;

            // Unload all documents
            foreach (var doc in _loadedDocuments.Values)
            {
                doc.Unload();
            }
            _loadedDocuments.Clear();

            // Free any remaining memory allocated for documents loaded from memory
            foreach (var memPtr in _documentMemoryPtrs.Values)
            {
                Marshal.FreeHGlobal(memPtr);
            }
            _documentMemoryPtrs.Clear();

            ImmNativePlugin.End();
            _isInitialized = false;
            CleanupCommandBuffers();

            Log("IMM Player shut down");
        }

        #endregion

        #region Document Management

        /// <summary>
        /// Load an IMM document from a file
        /// </summary>
        public ImmDocument LoadDocument(string filePath)
        {
            if (!_isInitialized)
            {
                LogError("IMM Player is not initialized");
                return null;
            }

            int docId = ImmNativePlugin.LoadFromFile(filePath);
            if (docId < 0)
            {
                LogError($"Failed to load document from: {filePath}");
                return null;
            }

            ImmDocument doc = new ImmDocument(docId, filePath);
            _loadedDocuments[docId] = doc;

            Log($"Loaded document: {filePath} (ID: {docId})");

            // Kick the native state machine once so the load command is processed promptly.
            ImmNativePlugin.GlobalWork(1);
            return doc;
        }

        /// <summary>
        /// Load an IMM document from memory
        /// </summary>
        public ImmDocument LoadDocumentFromMemory(byte[] data, string fileName)
        {
            if (!_isInitialized)
            {
                LogError("IMM Player is not initialized");
                return null;
            }

            IntPtr dataPtr = Marshal.AllocHGlobal(data.Length);
            Marshal.Copy(data, 0, dataPtr, data.Length);

            int docId = ImmNativePlugin.LoadFromMemory(fileName, data.Length, dataPtr);

            if (docId < 0)
            {
                // Free memory immediately on failure
                Marshal.FreeHGlobal(dataPtr);
                LogError($"Failed to load document from memory: {fileName}");
                return null;
            }

            // Track the memory pointer - native code loads asynchronously in a background thread,
            // so we must keep the memory alive until the document is unloaded
            _documentMemoryPtrs[docId] = dataPtr;

            ImmDocument doc = new ImmDocument(docId, fileName);
            _loadedDocuments[docId] = doc;

            Log($"Loaded document from memory: {fileName} (ID: {docId})");

            // Kick the native state machine once so the load command is processed promptly.
            ImmNativePlugin.GlobalWork(1);
            return doc;
        }

        /// <summary>
        /// Unload a document
        /// </summary>
        public void UnloadDocument(ImmDocument document)
        {
            if (document == null)
                return;

            int docId = document.DocumentId;

            if (_loadedDocuments.ContainsKey(docId))
            {
                _loadedDocuments.Remove(docId);
            }

            // Free any memory that was allocated for loading from memory
            if (_documentMemoryPtrs.TryGetValue(docId, out IntPtr memPtr))
            {
                _documentMemoryPtrs.Remove(docId);
                Marshal.FreeHGlobal(memPtr);
                Log($"Freed memory buffer for document {docId}");
            }

            document.Unload();
        }

        #endregion

        #region Rendering
        private class PerCameraInfo
        {
            public readonly CommandBuffer CommandBuffer = new CommandBuffer();
            public int CameraId = -1;
            public CameraEvent HookEvent = CameraEvent.AfterImageEffectsOpaque;
            public bool IsAttached = false;
            public readonly float[] WorldToHead = new float[16];
            public readonly float[] HeadProj = new float[16];
            public readonly float[] WorldToLeft = new float[16];
            public readonly float[] LeftProj = new float[16];
            public readonly float[] WorldToRight = new float[16];
            public readonly float[] RightProj = new float[16];
        }

        public bool UsesCommandBufferRendering => _useCommandBufferRendering;

        private void CleanupCommandBuffers()
        {
            foreach (var kvp in _cameras)
            {
                if (kvp.Key)
                {
                    kvp.Key.RemoveCommandBuffer(kvp.Value.HookEvent, kvp.Value.CommandBuffer);
                    kvp.Key.RemoveCommandBuffer(CameraEvent.AfterImageEffectsOpaque, kvp.Value.CommandBuffer);
                    kvp.Key.RemoveCommandBuffer(CameraEvent.AfterEverything, kvp.Value.CommandBuffer);
                }
            }
            _cameras.Clear();
        }

        private void OnCameraPreCull(Camera cam)
        {
            if (!_isInitialized || _renderEventFunc == IntPtr.Zero || cam == null)
                return;

            PerCameraInfo info;
            if (!_cameras.TryGetValue(cam, out info))
            {
                info = new PerCameraInfo();
                info.CameraId = _cameras.Count;
                info.CommandBuffer.name = "Render IMM Content";
                _cameras[cam] = info;
            }

            CameraEvent desiredEvent = XRSettings.enabled ? CameraEvent.AfterEverything : CameraEvent.AfterImageEffectsOpaque;
            if (!info.IsAttached || info.HookEvent != desiredEvent)
            {
                if (info.IsAttached)
                {
                    cam.RemoveCommandBuffer(info.HookEvent, info.CommandBuffer);
                }
                info.HookEvent = desiredEvent;
                cam.AddCommandBuffer(info.HookEvent, info.CommandBuffer);
                info.IsAttached = true;
            }

            int stereoMode = (int)StereoMode.Mono;
            if (cam.stereoEnabled)
            {
                if (XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.MultiPass)
                {
                    stereoMode = (int)StereoMode.TwoPass;
                }
                else if (XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.SinglePass)
                {
                    stereoMode = (int)StereoMode.SinglePass;
                }
                else if (XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.SinglePassInstanced)
                {
                    // The native plugin doesn't support instanced single-pass; force two-pass.
                    stereoMode = (int)StereoMode.TwoPass;
                }
            }

            bool isAndroidXr = XRSettings.enabled && Application.platform == RuntimePlatform.Android;

            if (isAndroidXr && stereoMode == (int)StereoMode.Mono)
            {
                stereoMode = XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.MultiPass
                    ? (int)StereoMode.TwoPass
                    : (int)StereoMode.SinglePass;
            }

            if (XRSettings.enabled)
            {
                Debug.Log($"{XrStereoDiagPrefix}cam={cam.name} stereoEnabled={cam.stereoEnabled} xrEnabled={XRSettings.enabled} xrMode={XRSettings.stereoRenderingMode} chosenStereoMode={stereoMode}");
            }

            ConvertMatrixToArray(info.WorldToHead, cam.worldToCameraMatrix);
            bool renderIntoTexture = cam.cameraType == CameraType.SceneView;
            Matrix4x4 headProjection = cam.nonJitteredProjectionMatrix;
            ConvertMatrixToArray(info.HeadProj, GL.GetGPUProjectionMatrix(headProjection, renderIntoTexture));

            if (!_lastNearClipLogged.TryGetValue(cam, out float lastNear) || !Mathf.Approximately(lastNear, cam.nearClipPlane))
            {
                _lastNearClipLogged[cam] = cam.nearClipPlane;
                Matrix4x4 gpuProjection = GL.GetGPUProjectionMatrix(headProjection, renderIntoTexture);
                Debug.Log($"{NearDiagPrefix}cam={cam.name} type={cam.cameraType} near={cam.nearClipPlane:F5} far={cam.farClipPlane:F2} renderIntoTex={renderIntoTexture} nonJit(m22={headProjection[10]:F6},m23={headProjection[14]:F6},m32={headProjection[11]:F6}) gpu(m22={gpuProjection[10]:F6},m23={gpuProjection[14]:F6},m32={gpuProjection[11]:F6})");
            }

            if (cam.stereoEnabled)
            {
                ConvertMatrixToArray(info.WorldToLeft, cam.GetStereoViewMatrix(Camera.StereoscopicEye.Left));
                ConvertMatrixToArray(info.LeftProj, GL.GetGPUProjectionMatrix(cam.GetStereoProjectionMatrix(Camera.StereoscopicEye.Left), renderIntoTexture));
                ConvertMatrixToArray(info.WorldToRight, cam.GetStereoViewMatrix(Camera.StereoscopicEye.Right));
                ConvertMatrixToArray(info.RightProj, GL.GetGPUProjectionMatrix(cam.GetStereoProjectionMatrix(Camera.StereoscopicEye.Right), renderIntoTexture));
            }

            ImmNativePlugin.SetMatrices(
                info.CameraId,
                stereoMode,
                info.WorldToHead,
                info.HeadProj,
                cam.stereoEnabled ? info.WorldToLeft : null,
                cam.stereoEnabled ? info.LeftProj : null,
                cam.stereoEnabled ? info.WorldToRight : null,
                cam.stereoEnabled ? info.RightProj : null);

            info.CommandBuffer.Clear();

            if (stereoMode == (int)StereoMode.TwoPass && isAndroidXr)
            {
                int leftEventId = (info.CameraId << 8) | 0;
                int rightEventId = (info.CameraId << 8) | 1;
                info.CommandBuffer.IssuePluginEvent(_renderEventFunc, leftEventId);
                info.CommandBuffer.IssuePluginEvent(_renderEventFunc, rightEventId);

                if (forceImmediateIssueInXR)
                {
                    GL.IssuePluginEvent(_renderEventFunc, leftEventId);
                    GL.IssuePluginEvent(_renderEventFunc, rightEventId);
                }
            }
            else
            {
                int eyeIndex = 0;
                if (stereoMode == (int)StereoMode.TwoPass && cam.stereoEnabled)
                {
                    eyeIndex = cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right ? 1 : 0;
                }

                int eventId = (info.CameraId << 8) | (eyeIndex & 0x1);
                info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId);
            }
        }

        /// <summary>
        /// Set camera matrices for rendering
        /// </summary>
        public void SetCameraMatrices(
            int cameraId,
            Camera camera,
            StereoMode stereoMode = StereoMode.Mono)
        {
            if (!_isInitialized)
                return;

            Matrix4x4 worldToCamera = camera.worldToCameraMatrix;
            bool renderIntoTexture = camera.cameraType == CameraType.SceneView;
            Matrix4x4 projection = GL.GetGPUProjectionMatrix(camera.nonJitteredProjectionMatrix, renderIntoTexture);

            float[] world2head = MatrixToFloatArray(worldToCamera);
            float[] prjHead = MatrixToFloatArray(projection);

            int stereoType = (int)stereoMode;

            ImmNativePlugin.SetMatrices(
                cameraId,
                stereoType,
                world2head,
                prjHead,
                null, null, null, null);
        }

        /// <summary>
        /// Set camera matrices for stereo rendering
        /// </summary>
        public void SetStereoCameraMatrices(
            int cameraId,
            Matrix4x4 world2head,
            Matrix4x4 projectionHead,
            Matrix4x4 world2leftEye,
            Matrix4x4 projectionLeft,
            Matrix4x4 world2rightEye,
            Matrix4x4 projectionRight,
            StereoMode stereoMode)
        {
            if (!_isInitialized)
                return;

            ImmNativePlugin.SetMatrices(
                cameraId,
                (int)stereoMode,
                MatrixToFloatArray(world2head),
                MatrixToFloatArray(projectionHead),
                MatrixToFloatArray(world2leftEye),
                MatrixToFloatArray(projectionLeft),
                MatrixToFloatArray(world2rightEye),
                MatrixToFloatArray(projectionRight));
        }

        /// <summary>
        /// Issue a render event for a camera
        /// </summary>
        public void IssueRenderEvent(int cameraId)
        {
            if (_renderEventFunc != IntPtr.Zero)
            {
                int eventId = cameraId << 8;
                GL.IssuePluginEvent(_renderEventFunc, eventId);
            }
        }

        #endregion

        #region Utility

        public struct PlayerInfoManaged
        {
            public Color BackgroundColor;
        }

        public PlayerInfoManaged GetPlayerInfo()
        {
            PlayerInfo info;
            ImmNativePlugin.GetPlayerInfo(out info);
            return new PlayerInfoManaged
            {
                BackgroundColor = new Color(
                    info.backgroundColor.red,
                    info.backgroundColor.green,
                    info.backgroundColor.blue)
            };
        }

        private static float[] MatrixToFloatArray(Matrix4x4 matrix)
        {
            float[] result = new float[16];
            for (int i = 0; i < 16; i++)
            {
                result[i] = matrix[i];
            }
            return result;
        }

        private static void ConvertMatrixToArray(float[] dst, Matrix4x4 matrix)
        {
            for (int i = 0; i < 16; i++)
            {
                dst[i] = matrix[i];
            }
        }

        #endregion

        #region Enums

        public enum StereoMode
        {
            Mono = 0,
            TwoPass = 1,
            SinglePass = 2
        }

        #endregion
    }
}
