using System;
using System.Collections.Generic;
using System.IO;
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

        #endregion

        #region State

        private bool _isInitialized = false;
        private Dictionary<int, ImmDocument> _loadedDocuments = new Dictionary<int, ImmDocument>();
        private Dictionary<int, IntPtr> _documentMemoryPtrs = new Dictionary<int, IntPtr>(); // Track memory for async loading
        private IntPtr _renderEventFunc = IntPtr.Zero;
        private readonly Dictionary<Camera, PerCameraInfo> _cameras = new Dictionary<Camera, PerCameraInfo>();
        private readonly Dictionary<Camera, float> _lastNearClipLogged = new Dictionary<Camera, float>();
        private const string NearDiagPrefix = "[IMMDBG_NEAR_20260208A] ";
        private bool _useCommandBufferRendering = false;
        private bool _useCameraCallbackRendering = false;
        private int _appleMetalEventLogCount = 0;

        private static bool IsEnvFlagEnabled(string name)
        {
            string value = Environment.GetEnvironmentVariable(name);
            return !string.IsNullOrEmpty(value) && value != "0";
        }

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
            bool builtInPipeline = GraphicsSettings.currentRenderPipeline == null;
            bool forceCameraCallback = IsEnvFlagEnabled("IMM_UNITY_FORCE_CAMERA_CALLBACK");
            _useCommandBufferRendering = builtInPipeline && !forceCameraCallback;
            _useCameraCallbackRendering = builtInPipeline && forceCameraCallback;
            if (_useCommandBufferRendering || _useCameraCallbackRendering)
            {
                Camera.onPreCull += OnCameraPreCull;
            }
            if (_useCameraCallbackRendering)
            {
                Camera.onPostRender += OnCameraPostRender;
            }
        }

        private void OnDisable()
        {
            if (_useCommandBufferRendering || _useCameraCallbackRendering)
            {
                Camera.onPreCull -= OnCameraPreCull;
            }
            if (_useCameraCallbackRendering)
            {
                Camera.onPostRender -= OnCameraPostRender;
            }
            CleanupCommandBuffers();
        }

        private void LateUpdate()
        {
            if (_isInitialized)
            {
                ImmNativePlugin.GlobalWork(1);
                ReleaseCompletedMemoryBuffers();
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

#if UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX || UNITY_IOS
            if (SystemInfo.graphicsDeviceType != UnityEngine.Rendering.GraphicsDeviceType.Metal)
            {
                LogError("IMM Player requires Metal on Apple platforms.");
                LogError($"Current Graphics API: {SystemInfo.graphicsDeviceType}");
                LogError("Switch the Apple platform Graphics API to Metal and restart Unity or rebuild the player.");
                return false;
            }
#endif

            int colorSpace = useLinearColorSpace ? 0 : 1;

            // Use Unity's temporary cache path for temporary files
            string tempFolder = Application.temporaryCachePath;
            string nativeLogPath = Path.Combine(tempFolder, logFileName);

            try
            {
#if UNITY_IOS && !UNITY_EDITOR
                ImmNativePlugin.ImmUnityRegisterRenderingPlugin();
#endif

                int result = ImmNativePlugin.Init(colorSpace, antialiasingLevel, nativeLogPath, tempFolder);

                if (result < 0)
                {
                    LogError($"Failed to initialize IMM Player. Error code: {result}");
                    LogError("Possible causes:");
                    LogError("  1. Missing DLL dependencies in Assets/Plugins/x86_64/");
                    LogError("  2. DLL platform settings incorrect (must be x86_64, Standalone + Editor)");
                    LogError("  3. Graphics API not supported (requires DirectX 11 on Windows, GLES on Android, or Metal on Apple platforms)");
                    LogError($"  4. Check native log file: {nativeLogPath}");
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

            // Native shutdown synchronously stops document loading before input buffers are released.
            ImmNativePlugin.End();

            foreach (var memPtr in _documentMemoryPtrs.Values)
            {
                Marshal.FreeHGlobal(memPtr);
            }
            _documentMemoryPtrs.Clear();
            _isInitialized = false;
            CleanupCommandBuffers();

            Log("IMM Player shut down");
        }

        #endregion

        /// <summary>Number of native player documents currently owned by this manager.</summary>
        public int LoadedDocumentCount => _loadedDocuments.Count;

        /// <summary>Whether the native player has been initialized successfully.</summary>
        public bool IsInitialized => _isInitialized;

        /// <summary>Number of unmanaged input buffers retained for asynchronous memory loads.</summary>
        public int OwnedInputBufferCount => _documentMemoryPtrs.Count;

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

            document.Unload();
            _loadedDocuments.Remove(docId);
            ReleaseCompletedMemoryBuffers();
        }

        private void ReleaseCompletedMemoryBuffers()
        {
            if (_documentMemoryPtrs.Count == 0)
                return;

            List<int> completedDocumentIds = null;
            foreach (KeyValuePair<int, IntPtr> entry in _documentMemoryPtrs)
            {
                if (_loadedDocuments.ContainsKey(entry.Key) || ImmNativePlugin.IsDocumentActive(entry.Key))
                    continue;

                if (completedDocumentIds == null)
                    completedDocumentIds = new List<int>();
                completedDocumentIds.Add(entry.Key);
            }

            if (completedDocumentIds == null)
                return;

            foreach (int documentId in completedDocumentIds)
            {
                IntPtr memPtr = _documentMemoryPtrs[documentId];
                _documentMemoryPtrs.Remove(documentId);
                Marshal.FreeHGlobal(memPtr);
                Log($"Freed memory buffer for completed document {documentId}");
            }
        }
        #endregion

        #region Rendering
        private class PerCameraInfo
        {
            public readonly CommandBuffer CommandBuffer = new CommandBuffer();
            public int CameraId = -1;
            public readonly float[] WorldToHead = new float[16];
            public readonly float[] HeadProj = new float[16];
            public readonly float[] WorldToLeft = new float[16];
            public readonly float[] LeftProj = new float[16];
            public readonly float[] WorldToRight = new float[16];
            public readonly float[] RightProj = new float[16];
        }

        public bool UsesCommandBufferRendering => _useCommandBufferRendering;

        private static bool IsAppleMetalRuntime()
        {
#if UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX || UNITY_IOS
            return SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Metal;
#else
            return false;
#endif
        }

        private static bool UseRenderIntoTextureProjection(Camera cam)
        {
            if (IsEnvFlagEnabled("IMM_UNITY_FORCE_BACKBUFFER_PROJECTION"))
                return false;
            if (IsEnvFlagEnabled("IMM_UNITY_FORCE_TEXTURE_PROJECTION"))
                return true;
            return cam != null && cam.cameraType == CameraType.SceneView;
        }

        private void CleanupCommandBuffers()
        {
            foreach (var kvp in _cameras)
            {
                if (kvp.Key)
                {
                    kvp.Key.RemoveCommandBuffer(CameraEvent.AfterImageEffectsOpaque, kvp.Value.CommandBuffer);
                }
            }
            _cameras.Clear();
        }

        private void OnCameraPreCull(Camera cam)
        {
            if (!_isInitialized || _renderEventFunc == IntPtr.Zero || cam == null)
                return;
            if (!HasRenderableDocument())
                return;

            PerCameraInfo info = GetOrCreateCameraInfo(cam, _useCommandBufferRendering);

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

            ConvertMatrixToArray(info.WorldToHead, cam.worldToCameraMatrix);
            bool renderIntoTexture = UseRenderIntoTextureProjection(cam);
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
            ImmNativePlugin.SetCameraViewport(info.CameraId, cam.pixelWidth, cam.pixelHeight);

            if (_useCameraCallbackRendering)
                return;

            int eyeIndex = 0;
            if (stereoMode == (int)StereoMode.TwoPass && cam.stereoEnabled)
            {
                eyeIndex = cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right ? 1 : 0;
            }

            int eventId = (info.CameraId << 8) | (eyeIndex & 0x1);
            info.CommandBuffer.Clear();
            info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId);
        }

        private PerCameraInfo GetOrCreateCameraInfo(Camera cam, bool attachCommandBuffer)
        {
            if (!_cameras.TryGetValue(cam, out PerCameraInfo info))
            {
                info = new PerCameraInfo();
                info.CameraId = _cameras.Count;
                info.CommandBuffer.name = "Render IMM Content";
                _cameras[cam] = info;
                if (attachCommandBuffer)
                {
                    cam.AddCommandBuffer(CameraEvent.AfterImageEffectsOpaque, info.CommandBuffer);
                }
            }
            return info;
        }

        private void OnCameraPostRender(Camera cam)
        {
            if (!_useCameraCallbackRendering || !_isInitialized || _renderEventFunc == IntPtr.Zero || cam == null)
                return;
            if (!HasRenderableDocument())
                return;

            if (!_cameras.TryGetValue(cam, out PerCameraInfo info))
                return;

            int eventId = info.CameraId << 8;
            if (_appleMetalEventLogCount < 8)
            {
                Debug.Log($"[IMM_UNITY_METAL_EVENT] camera={info.CameraId} viewport={cam.pixelWidth}x{cam.pixelHeight} eventId={eventId}");
                _appleMetalEventLogCount++;
            }
            GL.IssuePluginEvent(_renderEventFunc, eventId);
        }

        private bool HasRenderableDocument()
        {
            foreach (ImmDocument doc in _loadedDocuments.Values)
            {
                if (doc != null && doc.IsSequenceReady())
                    return true;
            }
            return false;
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
            bool renderIntoTexture = UseRenderIntoTextureProjection(camera);
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
            ImmNativePlugin.SetCameraViewport(cameraId, camera.pixelWidth, camera.pixelHeight);
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
