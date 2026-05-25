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
        private MatrixDiagnostics _lastMatrixDiagnostics = new MatrixDiagnostics();

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
                    kvp.Key.RemoveCommandBuffer(CameraEvent.AfterImageEffectsOpaque, kvp.Value.CommandBuffer);
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
                cam.AddCommandBuffer(CameraEvent.AfterImageEffectsOpaque, info.CommandBuffer);
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

            ConvertMatrixToArray(info.WorldToHead, cam.worldToCameraMatrix);
            bool renderIntoTexture = true; // TEST: was cam.cameraType == CameraType.SceneView
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
            CaptureMatrixDiagnostics(info.CameraId, stereoMode, info.WorldToHead, info.HeadProj);

            int eyeIndex = 0;
            if (stereoMode == (int)StereoMode.TwoPass && cam.stereoEnabled)
            {
                eyeIndex = cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right ? 1 : 0;
            }

            int eventId = (info.CameraId << 8) | (eyeIndex & 0x1);
            info.CommandBuffer.Clear();
            info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId);
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
            bool renderIntoTexture = true; // TEST: was camera.cameraType == CameraType.SceneView
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
            CaptureMatrixDiagnostics(cameraId, stereoType, world2head, prjHead);
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

            float[] worldToHeadArray = MatrixToFloatArray(world2head);
            float[] projectionHeadArray = MatrixToFloatArray(projectionHead);
            float[] worldToLeftArray = MatrixToFloatArray(world2leftEye);
            float[] projectionLeftArray = MatrixToFloatArray(projectionLeft);
            float[] worldToRightArray = MatrixToFloatArray(world2rightEye);
            float[] projectionRightArray = MatrixToFloatArray(projectionRight);

            ImmNativePlugin.SetMatrices(
                cameraId,
                (int)stereoMode,
                worldToHeadArray,
                projectionHeadArray,
                worldToLeftArray,
                projectionLeftArray,
                worldToRightArray,
                projectionRightArray);
            CaptureMatrixDiagnostics(cameraId, (int)stereoMode, worldToHeadArray, projectionHeadArray);
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

        public string GetLastMatrixDiagnosticsJson()
        {
            return JsonUtility.ToJson(_lastMatrixDiagnostics);
        }

        public void LogLastMatrixDiagnostics()
        {
            Debug.Log("IMM_UNITY_MATRIX_DIAGNOSTICS_JSON " + GetLastMatrixDiagnosticsJson());
        }

        public void SetDeterministicMatrixDiagnostics(int cameraId = 1, bool submitToNative = false, ImmDocument document = null, string documentPath = null)
        {
            float[] worldToHead =
            {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.25f, 1.6f, 6.0f, 1.0f,
            };
            float[] projection =
            {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, -1.0f, -1.0f,
                0.0f, 0.0f, -0.1f, 0.0f,
            };
            float[] documentToWorld =
            {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.5f, 0.0f, -0.25f, 1.0f,
            };
            int stereoType = (int)StereoMode.Mono;
            if (submitToNative && _isInitialized)
            {
                ImmNativePlugin.SetMatrices(
                    cameraId,
                    stereoType,
                    worldToHead,
                    projection,
                    null, null, null, null);
                if (document != null && document.IsLoaded)
                {
                    document.SetTransform(MatrixFromFloatArray(documentToWorld));
                }
            }
            CaptureMatrixDiagnostics(cameraId, stereoType, worldToHead, projection);
            _lastMatrixDiagnostics.document_to_world = CopyMatrix(documentToWorld);
            CaptureDocumentIdentityDiagnostics(document, documentPath);
            CapturePlayerParityDiagnostics(document);
        }

        public void LogDeterministicMatrixDiagnostics(int cameraId = 1, bool submitToNative = false, ImmDocument document = null, string documentPath = null)
        {
            SetDeterministicMatrixDiagnostics(cameraId, submitToNative, document, documentPath);
            LogLastMatrixDiagnostics();
        }

        private void CaptureMatrixDiagnostics(int cameraId, int stereoMode, float[] worldToHead, float[] projection)
        {
            _lastMatrixDiagnostics.schema = "imm_unity_matrix_diagnostics_v1";
            _lastMatrixDiagnostics.unity_version = Application.unityVersion;
            _lastMatrixDiagnostics.camera_id = cameraId;
            _lastMatrixDiagnostics.stereo_mode = stereoMode;
            _lastMatrixDiagnostics.world_to_head = CopyMatrix(worldToHead);
            _lastMatrixDiagnostics.projection = CopyMatrix(projection);
        }

        private void CapturePlayerParityDiagnostics(ImmDocument document)
        {
            PlayerInfo info;
            ImmNativePlugin.GetPlayerInfo(out info);
            _lastMatrixDiagnostics.background_color = new[]
            {
                info.backgroundColor.red,
                info.backgroundColor.green,
                info.backgroundColor.blue,
                1.0f
            };

            if (document == null || !document.IsLoaded)
            {
                _lastMatrixDiagnostics.document_loading_state = -1;
                _lastMatrixDiagnostics.document_playback_state = -1;
                _lastMatrixDiagnostics.bounding_box_valid = false;
                _lastMatrixDiagnostics.bounding_box_min = new float[3];
                _lastMatrixDiagnostics.bounding_box_max = new float[3];
                _lastMatrixDiagnostics.spawn_area_count = 0;
                _lastMatrixDiagnostics.active_spawn_area_index = -1;
                _lastMatrixDiagnostics.active_spawn_area_id = -1;
                return;
            }

            DocumentState state = document.GetState();
            _lastMatrixDiagnostics.document_loading_state = state.loadingState;
            _lastMatrixDiagnostics.document_playback_state = state.playbackState;

            Bounds bounds = document.GetBoundingBox();
            _lastMatrixDiagnostics.bounding_box_valid = IsFinite(bounds.min) && IsFinite(bounds.max);
            _lastMatrixDiagnostics.bounding_box_min = VectorToArray(bounds.min);
            _lastMatrixDiagnostics.bounding_box_max = VectorToArray(bounds.max);

            int[] spawnAreaIds = document.GetSpawnAreaList();
            int activeSpawnAreaId = document.GetActiveSpawnAreaId();
            _lastMatrixDiagnostics.spawn_area_count = spawnAreaIds.Length;
            _lastMatrixDiagnostics.active_spawn_area_id = activeSpawnAreaId;
            _lastMatrixDiagnostics.active_spawn_area_index = -1;
            for (int i = 0; i < spawnAreaIds.Length; i++)
            {
                if (spawnAreaIds[i] == activeSpawnAreaId)
                {
                    _lastMatrixDiagnostics.active_spawn_area_index = i;
                    break;
                }
            }
        }

        private void CaptureDocumentIdentityDiagnostics(ImmDocument document, string documentPath)
        {
            string path = !string.IsNullOrWhiteSpace(documentPath)
                ? documentPath
                : document?.FileName;
            _lastMatrixDiagnostics.document_path = path ?? string.Empty;
            _lastMatrixDiagnostics.document_name = string.IsNullOrEmpty(path)
                ? string.Empty
                : System.IO.Path.GetFileName(path);
            long fileSize = System.IO.File.Exists(path)
                ? new System.IO.FileInfo(path).Length
                : -1L;
            _lastMatrixDiagnostics.document_size_bytes = fileSize > int.MaxValue
                ? int.MaxValue
                : (int)fileSize;
        }

        private static float[] CopyMatrix(float[] matrix)
        {
            float[] result = new float[16];
            if (matrix == null)
            {
                return result;
            }
            Array.Copy(matrix, result, Math.Min(matrix.Length, result.Length));
            return result;
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

        private static Matrix4x4 MatrixFromFloatArray(float[] values)
        {
            Matrix4x4 result = Matrix4x4.identity;
            if (values == null)
            {
                return result;
            }
            for (int i = 0; i < Math.Min(values.Length, 16); i++)
            {
                result[i] = values[i];
            }
            return result;
        }

        private static float[] VectorToArray(Vector3 vector)
        {
            return new[] { vector.x, vector.y, vector.z };
        }

        private static bool IsFinite(Vector3 vector)
        {
            return !float.IsNaN(vector.x) && !float.IsInfinity(vector.x)
                && !float.IsNaN(vector.y) && !float.IsInfinity(vector.y)
                && !float.IsNaN(vector.z) && !float.IsInfinity(vector.z);
        }

        private static void ConvertMatrixToArray(float[] dst, Matrix4x4 matrix)
        {
            for (int i = 0; i < 16; i++)
            {
                dst[i] = matrix[i];
            }
        }

        #endregion

        [Serializable]
        private class MatrixDiagnostics
        {
            public string schema = "imm_unity_matrix_diagnostics_v1";
            public string unity_version = "";
            public int camera_id = -1;
            public int stereo_mode = 0;
            public float[] world_to_head = new float[16];
            public float[] projection = new float[16];
            public float[] document_to_world = new float[16];
            public string document_path = "";
            public string document_name = "";
            public int document_size_bytes = -1;
            public int document_loading_state = -1;
            public int document_playback_state = -1;
            public float[] background_color = new float[4];
            public bool bounding_box_valid = false;
            public float[] bounding_box_min = new float[3];
            public float[] bounding_box_max = new float[3];
            public int spawn_area_count = 0;
            public int active_spawn_area_index = -1;
            public int active_spawn_area_id = -1;
        }

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
