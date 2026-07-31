using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

[assembly: InternalsVisibleTo("ImmUnity.Runtime.Tests")]

namespace ImmPlayer
{
    internal sealed class VulkanPresentationCamera : MonoBehaviour
    {
        private Camera _camera;
        private CommandBuffer _presentationCommands;

        internal void Configure(RenderTexture presentationSource)
        {
            ReleasePresentationCommands();
            if (presentationSource == null)
                return;

            _camera = GetComponent<Camera>();
            _presentationCommands = new CommandBuffer { name = "IMM Vulkan Presentation" };
            _presentationCommands.Blit(
                new RenderTargetIdentifier(presentationSource),
                BuiltinRenderTextureType.CameraTarget);
            _camera.AddCommandBuffer(CameraEvent.AfterEverything, _presentationCommands);
            Debug.Log(
                $"[IMM_UNITY_VK_PRESENT_COMMANDS_20260731] source={presentationSource.width}x{presentationSource.height} " +
                $"event={CameraEvent.AfterEverything} destination=CameraTarget");
        }

        private void OnDestroy()
        {
            ReleasePresentationCommands();
        }

        private void ReleasePresentationCommands()
        {
            if (_presentationCommands == null)
                return;
            if (_camera != null)
                _camera.RemoveCommandBuffer(CameraEvent.AfterEverything, _presentationCommands);
            _presentationCommands.Release();
            _presentationCommands = null;
        }
    }

    internal enum ImmProjectionDestination
    {
        Backbuffer,
        ExplicitRenderTexture,
        EditorGameView,
        EditorSceneView,
        VulkanHostAttachment,
        XrDisplay,
        ForcedBackbuffer,
        ForcedRenderTexture,
    }

    internal static class ImmProjectionDestinationResolver
    {
        internal static ImmProjectionDestination Resolve(
            GraphicsDeviceType graphicsDeviceType,
            CameraType cameraType,
            bool stereoEnabled,
            bool hasExplicitRenderTexture,
            bool isEditor,
            bool forceBackbuffer,
            bool forceRenderTexture)
        {
            // Preserve the existing diagnostic override precedence.
            if (forceBackbuffer)
                return ImmProjectionDestination.ForcedBackbuffer;
            if (forceRenderTexture)
                return ImmProjectionDestination.ForcedRenderTexture;

            // The destination is more important than the graphics backend.
            // Unity requires texture-backed projections for explicit render
            // textures and for the Editor's internally texture-backed views.
            if (hasExplicitRenderTexture)
                return ImmProjectionDestination.ExplicitRenderTexture;
            if (cameraType == CameraType.SceneView)
                return ImmProjectionDestination.EditorSceneView;

            // XR displays render into runtime-managed swapchain textures.
            if (stereoEnabled)
                return ImmProjectionDestination.XrDisplay;

            if (isEditor && cameraType == CameraType.Game)
                return ImmProjectionDestination.EditorGameView;

            // Preserve the verified non-XR Vulkan host-attachment behavior.
            if (graphicsDeviceType == GraphicsDeviceType.Vulkan &&
                cameraType == CameraType.Game)
                return ImmProjectionDestination.VulkanHostAttachment;

            return ImmProjectionDestination.Backbuffer;
        }

        internal static bool UsesRenderTextureProjection(ImmProjectionDestination destination)
        {
            switch (destination)
            {
                case ImmProjectionDestination.ExplicitRenderTexture:
                case ImmProjectionDestination.EditorGameView:
                case ImmProjectionDestination.EditorSceneView:
                case ImmProjectionDestination.VulkanHostAttachment:
                case ImmProjectionDestination.XrDisplay:
                case ImmProjectionDestination.ForcedRenderTexture:
                    return true;
                default:
                    return false;
            }
        }
    }

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
        [SerializeField] private Camera renderCamera = null;

        #endregion

        #region State

        private bool _isInitialized = false;
        private Dictionary<int, ImmDocument> _loadedDocuments = new Dictionary<int, ImmDocument>();
        private Dictionary<int, IntPtr> _documentMemoryPtrs = new Dictionary<int, IntPtr>(); // Track memory for async loading
        private readonly Dictionary<int, ImmDocument> _pendingUnloadDocuments = new Dictionary<int, ImmDocument>();
        private readonly HashSet<int> _nativeUnloadsInFlight = new HashSet<int>();
        private IntPtr _renderEventFunc = IntPtr.Zero;
        private IntPtr _renderEventAndDataFunc = IntPtr.Zero;
        private readonly Dictionary<Camera, PerCameraInfo> _cameras = new Dictionary<Camera, PerCameraInfo>();
        private readonly Dictionary<Camera, float> _lastNearClipLogged = new Dictionary<Camera, float>();
        private readonly Dictionary<Camera, ImmProjectionDestination> _lastProjectionDestinationLogged = new Dictionary<Camera, ImmProjectionDestination>();
        private readonly HashSet<Camera> _loggedVulkanRenderTargetSource = new HashSet<Camera>();
        private readonly HashSet<Camera> _loggedVulkanPrepareWarning = new HashSet<Camera>();
        private readonly HashSet<int> _configuredVulkanRenderEvents = new HashSet<int>();
        private readonly Dictionary<Camera, RenderTexture> _vulkanPresentationTargets = new Dictionary<Camera, RenderTexture>();
        private readonly Dictionary<Camera, VulkanPresentationCamera> _vulkanPresentationCameras =
            new Dictionary<Camera, VulkanPresentationCamera>();
        private int _androidVulkanPostRenderPresentationCount;
        private const string NearDiagPrefix = "[IMMDBG_NEAR_20260208A] ";
        private const string ProjectionDestinationDiagPrefix = "[IMM_PROJECTION_TARGET_20260725] ";
        private const int VulkanCustomBlitEventId = 6;
        private const int VulkanPrepareEventFlag = 0x80;
        private bool _useCommandBufferRendering = false;
        private bool _useCameraCallbackRendering = false;
        private Coroutine _vulkanSampleEventCoroutine = null;
        private int _appleMetalEventLogCount = 0;
        private int _appleMetalQueueLogCount = 0;
        private int _androidVulkanPreCullCount = 0;
        private static Mesh _vulkanOverlayFixtureMesh;
        private static Material _vulkanOverlayFixtureMaterial;

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
#if UNITY_ANDROID
            // The Vulkan source camera targets an explicit RenderTexture. Its
            // attached command buffer records IMM inside Unity's live render
            // pass so Unity owns attachment state and synchronization.
            if (IsVulkanRuntime())
                forceCameraCallback = false;
#endif
            _useCommandBufferRendering = builtInPipeline && !forceCameraCallback;
            _useCameraCallbackRendering = builtInPipeline && forceCameraCallback;
            if (IsVulkanRuntime())
            {
                Debug.Log(
                    $"[IMM_UNITY_ANDROID_VK_CALLBACK_20260729] unity={Application.unityVersion} " +
                    $"commandBuffer={_useCommandBufferRendering} cameraCallback={_useCameraCallbackRendering}");
            }
            if (IsAppleMetalRuntime())
            {
                string pipelineName = GraphicsSettings.currentRenderPipeline == null
                    ? "BuiltIn"
                    : GraphicsSettings.currentRenderPipeline.GetType().FullName;
                Debug.Log(
                    $"[IMM_UNITY_METAL_MANAGED_SETUP] unity={Application.unityVersion} " +
                    $"pipeline={pipelineName} graphics={SystemInfo.graphicsDeviceType} " +
                    $"commandBuffer={_useCommandBufferRendering} cameraCallback={_useCameraCallbackRendering} " +
                    $"forceCameraCallback={forceCameraCallback}");
            }
            if (_useCommandBufferRendering || _useCameraCallbackRendering)
            {
                Camera.onPreCull += OnCameraPreCull;
            }
            if (_useCameraCallbackRendering)
            {
                Camera.onPostRender += OnCameraPostRender;
            }
            if (builtInPipeline && IsEnvFlagEnabled("IMM_UNITY_VK_SAMPLE_WAIT_FOR_END_OF_FRAME"))
            {
                _vulkanSampleEventCoroutine = StartCoroutine(IssueVulkanSampleEventAfterEndOfFrame());
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
            if (_vulkanSampleEventCoroutine != null)
            {
                StopCoroutine(_vulkanSampleEventCoroutine);
                _vulkanSampleEventCoroutine = null;
            }
            CleanupCommandBuffers();
        }

        private void LateUpdate()
        {
            if (_isInitialized)
            {
                ImmNativePlugin.GlobalWork(1);
                ProcessPendingDocumentUnloads();
                IssueNativeUnloadDrainEvent();
                CompleteFinishedNativeUnloads();
                ReleaseCompletedMemoryBuffers();
                if (IsVulkanRuntime() &&
                    _renderEventFunc != IntPtr.Zero &&
                    !IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_HOST_RENDER") &&
                    IsEnvFlagEnabled("IMM_UNITY_VK_USE_PREPARE_EVENT"))
                {
                    foreach (PerCameraInfo info in _cameras.Values)
                    {
                        int prepareEventId = (info.CameraId << 8) | 0x80;
                        if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") && _configuredVulkanRenderEvents.Add(prepareEventId))
                        {
                            int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(prepareEventId);
                            Debug.Log($"[IMM_UNITY_VK_EVENTCFG_20260611] eventId={prepareEventId} configured={configured}");
                        }
                        GL.IssuePluginEvent(_renderEventFunc, prepareEventId);
                    }
                }
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
                _renderEventAndDataFunc = ImmNativePlugin.GetRenderEventAndDataFunc();
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
            _pendingUnloadDocuments.Clear();
            _nativeUnloadsInFlight.Clear();

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

        /// <summary>Number of documents waiting to reach a safe native unload boundary.</summary>
        public int PendingUnloadDocumentCount => _pendingUnloadDocuments.Count;

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
            if (!_loadedDocuments.ContainsKey(docId))
                return;

            ImmDocument.LoadingState loadingState = document.GetStateInfo().Loading;
            if (loadingState == ImmDocument.LoadingState.Unloaded ||
                loadingState == ImmDocument.LoadingState.Loading)
            {
                _pendingUnloadDocuments[docId] = document;
                Log($"Deferred unload for loading document {docId} (state: {loadingState})");
                return;
            }

            CompleteDocumentUnload(document);
            ReleaseCompletedMemoryBuffers();
        }

        private void ProcessPendingDocumentUnloads()
        {
            if (_pendingUnloadDocuments.Count == 0)
                return;

            List<int> readyDocumentIds = null;
            foreach (KeyValuePair<int, ImmDocument> entry in _pendingUnloadDocuments)
            {
                ImmDocument.LoadingState loadingState = entry.Value.GetStateInfo().Loading;
                if (loadingState != ImmDocument.LoadingState.Loaded &&
                    loadingState != ImmDocument.LoadingState.Failed)
                {
                    continue;
                }

                if (readyDocumentIds == null)
                    readyDocumentIds = new List<int>();
                readyDocumentIds.Add(entry.Key);
            }

            if (readyDocumentIds == null)
                return;

            foreach (int documentId in readyDocumentIds)
            {
                ImmDocument document = _pendingUnloadDocuments[documentId];
                document.Hide();
                CompleteDocumentUnload(document);
                Log($"Completed deferred unload for document {documentId}");
            }
        }

        private void CompleteDocumentUnload(ImmDocument document)
        {
            int documentId = document.DocumentId;
            _nativeUnloadsInFlight.Add(documentId);
            document.Unload();
            _loadedDocuments.Remove(documentId);
            _pendingUnloadDocuments.Remove(documentId);
            ReleaseDocumentMemoryBuffer(documentId);
        }

        private void CompleteFinishedNativeUnloads()
        {
            if (_nativeUnloadsInFlight.Count == 0)
                return;

            List<int> completedDocumentIds = null;
            foreach (int documentId in _nativeUnloadsInFlight)
            {
                if (ImmNativePlugin.IsDocumentActive(documentId))
                    continue;

                if (completedDocumentIds == null)
                    completedDocumentIds = new List<int>();
                completedDocumentIds.Add(documentId);
            }

            if (completedDocumentIds == null)
                return;

            foreach (int documentId in completedDocumentIds)
            {
                _nativeUnloadsInFlight.Remove(documentId);
                Log($"[IMM_NATIVE_UNLOAD_DRAIN] Completed native unload for document {documentId}");
            }
        }

        private void IssueNativeUnloadDrainEvent()
        {
            if (_nativeUnloadsInFlight.Count > 0 && _renderEventFunc != IntPtr.Zero)
                GL.IssuePluginEvent(_renderEventFunc, 0);
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
                ReleaseDocumentMemoryBuffer(documentId);
        }

        private void ReleaseDocumentMemoryBuffer(int documentId)
        {
            if (!_documentMemoryPtrs.TryGetValue(documentId, out IntPtr memPtr))
                return;

            _documentMemoryPtrs.Remove(documentId);
            Marshal.FreeHGlobal(memPtr);
            Log($"Freed memory buffer for completed document {documentId}");
        }
        #endregion

        #region Rendering
        private class PerCameraInfo
        {
            public readonly CommandBuffer CommandBuffer = new CommandBuffer();
            public int CameraId = -1;
            public bool VulkanCommandBufferPopulated;
            public readonly float[] WorldToHead = new float[16];
            public readonly float[] HeadProj = new float[16];
            public readonly float[] WorldToLeft = new float[16];
            public readonly float[] LeftProj = new float[16];
            public readonly float[] WorldToRight = new float[16];
            public readonly float[] RightProj = new float[16];
        }

        public bool UsesCommandBufferRendering => _useCommandBufferRendering;

        public bool IsReadyForDocumentLoad
        {
            get
            {
                if (!_isInitialized)
                    return false;

                return RequiresNativeDocumentLoadReady() ? ImmNativePlugin.IsReadyForDocumentLoad() != 0 : true;
            }
        }

        private static bool RequiresNativeDocumentLoadReady()
        {
#if UNITY_ANDROID && !UNITY_EDITOR
            return true;
#else
            return false;
#endif
        }

        private static bool IsAppleMetalRuntime()
        {
#if UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX || UNITY_IOS
            return SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Metal;
#else
            return false;
#endif
        }

        private static bool IsVulkanRuntime()
        {
#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN || UNITY_ANDROID
            return SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan;
#else
            return false;
#endif
        }

        private static CameraEvent GetVulkanCommandBufferEvent()
        {
            string value = Environment.GetEnvironmentVariable("IMM_UNITY_VK_CAMERA_EVENT");
            if (string.Equals(value, "AfterImageEffectsOpaque", StringComparison.OrdinalIgnoreCase))
                return CameraEvent.AfterImageEffectsOpaque;
            if (string.Equals(value, "AfterForwardOpaque", StringComparison.OrdinalIgnoreCase))
                return CameraEvent.AfterForwardOpaque;
            if (string.Equals(value, "AfterSkybox", StringComparison.OrdinalIgnoreCase))
                return CameraEvent.AfterSkybox;
            if (string.Equals(value, "BeforeImageEffectsOpaque", StringComparison.OrdinalIgnoreCase))
                return CameraEvent.BeforeImageEffectsOpaque;
            if (string.Equals(value, "AfterEverything", StringComparison.OrdinalIgnoreCase))
                return CameraEvent.AfterEverything;
            if (string.Equals(value, "BeforeForwardOpaque", StringComparison.OrdinalIgnoreCase))
                return CameraEvent.BeforeForwardOpaque;

#if UNITY_ANDROID
            return CameraEvent.AfterEverything;
#else
            return CameraEvent.AfterSkybox;
#endif
        }

        public void SetRenderCamera(Camera camera)
        {
            renderCamera = camera;
        }

        public void ClearRenderCamera()
        {
            renderCamera = null;
        }

        private bool ShouldRenderCamera(Camera cam)
        {
            if (cam == null)
                return false;

            if (cam.GetComponent<VulkanPresentationCamera>() != null)
                return false;

            if (renderCamera != null && cam != renderCamera)
                return false;

            if (IsEnvFlagEnabled("IMM_UNITY_GAME_CAMERAS_ONLY") && cam.cameraType != CameraType.Game)
                return false;

            string cameraName = Environment.GetEnvironmentVariable("IMM_UNITY_CAMERA_NAME");
            if (!string.IsNullOrEmpty(cameraName) && !string.Equals(cam.name, cameraName, StringComparison.Ordinal))
                return false;

            return true;
        }

        private ImmProjectionDestination ResolveProjectionDestination(Camera cam)
        {
            ImmProjectionDestination destination = ImmProjectionDestinationResolver.Resolve(
                SystemInfo.graphicsDeviceType,
                cam != null ? cam.cameraType : CameraType.Game,
                cam != null && cam.stereoEnabled,
                cam != null && cam.targetTexture != null,
                Application.isEditor,
                IsEnvFlagEnabled("IMM_UNITY_FORCE_BACKBUFFER_PROJECTION"),
                IsEnvFlagEnabled("IMM_UNITY_FORCE_TEXTURE_PROJECTION"));

            if (cam != null &&
                (!_lastProjectionDestinationLogged.TryGetValue(cam, out ImmProjectionDestination previous) ||
                 previous != destination))
            {
                _lastProjectionDestinationLogged[cam] = destination;
                bool renderIntoTexture = ImmProjectionDestinationResolver.UsesRenderTextureProjection(destination);
                Debug.Log(
                    $"{ProjectionDestinationDiagPrefix}camera={cam.name} cameraType={cam.cameraType} " +
                    $"backend={SystemInfo.graphicsDeviceType} stereo={cam.stereoEnabled} " +
                    $"explicitTarget={cam.targetTexture != null} destination={destination} " +
                    $"renderIntoTexture={renderIntoTexture}");
            }

            return destination;
        }

        private bool UseRenderIntoTextureProjection(Camera cam)
        {
            return ImmProjectionDestinationResolver.UsesRenderTextureProjection(
                ResolveProjectionDestination(cam));
        }

        private void CleanupCommandBuffers()
        {
            CameraEvent[] events =
            {
                CameraEvent.AfterImageEffectsOpaque,
                CameraEvent.BeforeForwardOpaque,
                CameraEvent.AfterForwardOpaque,
                CameraEvent.AfterSkybox,
                CameraEvent.BeforeImageEffectsOpaque,
                CameraEvent.AfterEverything
            };
            foreach (var kvp in _cameras)
            {
                if (kvp.Key)
                {
                    foreach (CameraEvent cameraEvent in events)
                    {
                        kvp.Key.RemoveCommandBuffer(cameraEvent, kvp.Value.CommandBuffer);
                    }
                }
            }
            _cameras.Clear();
            _configuredVulkanRenderEvents.Clear();
            foreach (KeyValuePair<Camera, VulkanPresentationCamera> entry in _vulkanPresentationCameras)
            {
                if (entry.Value != null)
                    Destroy(entry.Value.gameObject);
            }
            _vulkanPresentationCameras.Clear();
            foreach (KeyValuePair<Camera, RenderTexture> entry in _vulkanPresentationTargets)
            {
                if (entry.Key != null && entry.Key.targetTexture == entry.Value)
                    entry.Key.targetTexture = null;
                if (entry.Value == null)
                    continue;
                entry.Value.Release();
                Destroy(entry.Value);
            }
            _vulkanPresentationTargets.Clear();
        }

        private RenderTexture GetOrCreateVulkanPresentationTarget(Camera cam)
        {
#if UNITY_ANDROID
            if (!IsVulkanRuntime() || cam == null || cam.stereoEnabled)
                return null;

            int width = Math.Max(1, Screen.width);
            int height = Math.Max(1, Screen.height);
            if (_vulkanPresentationTargets.TryGetValue(cam, out RenderTexture existing))
            {
                if (existing != null && existing.width == width && existing.height == height)
                {
                    if (cam.targetTexture != existing)
                        cam.targetTexture = existing;
                    return existing;
                }

                if (cam.targetTexture == existing)
                    cam.targetTexture = null;
                if (existing != null)
                {
                    existing.Release();
                    Destroy(existing);
                }
                _vulkanPresentationTargets.Remove(cam);
                if (_vulkanPresentationCameras.TryGetValue(cam, out VulkanPresentationCamera oldPresenter))
                {
                    if (oldPresenter != null)
                        Destroy(oldPresenter.gameObject);
                    _vulkanPresentationCameras.Remove(cam);
                }
            }
            else if (cam.targetTexture != null)
            {
                return null;
            }

            var target = new RenderTexture(width, height, 24, RenderTextureFormat.ARGB32)
            {
                antiAliasing = 1,
                name = $"IMM Vulkan Presentation Target ({cam.name})",
                useMipMap = false,
                autoGenerateMips = false
            };
            target.Create();
            cam.targetTexture = target;
            _vulkanPresentationTargets[cam] = target;

            var presenterObject = new GameObject($"IMM Vulkan Presenter ({cam.name})");
            presenterObject.transform.SetParent(transform, false);
            Camera presenterCamera = presenterObject.AddComponent<Camera>();
            presenterCamera.depth = cam.depth + 1000.0f;
            presenterCamera.clearFlags = CameraClearFlags.SolidColor;
            presenterCamera.backgroundColor = Color.black;
            presenterCamera.cullingMask = 1 << 31;
            presenterCamera.allowHDR = false;
            presenterCamera.allowMSAA = false;
            presenterCamera.useOcclusionCulling = false;
            presenterCamera.orthographic = true;
            presenterCamera.orthographicSize = 1.0f;
            presenterCamera.nearClipPlane = 0.01f;
            presenterCamera.farClipPlane = 10.0f;
            presenterCamera.targetDisplay = cam.targetDisplay;
            presenterCamera.rect = cam.rect;
            VulkanPresentationCamera presenter = presenterObject.AddComponent<VulkanPresentationCamera>();
            presenter.Configure(target);
            _vulkanPresentationCameras[cam] = presenter;

            Debug.Log(
                $"[IMM_UNITY_VK_PRESENT_TERMINAL_20260731] camera={cam.name} size={width}x{height} " +
                $"mainDepth={cam.depth} presenterDepth={presenterCamera.depth}");
            return target;
#else
            return null;
#endif
        }

#if IMM_UNITY_ANDROID_VULKAN_CI
        public RenderTexture GetAndroidVulkanPresentationTargetForValidation(Camera cam)
        {
            if (cam == null)
                return null;
            _vulkanPresentationTargets.TryGetValue(cam, out RenderTexture target);
            return target;
        }
#endif

        private void OnCameraPreCull(Camera cam)
        {
            if (!_isInitialized || _renderEventFunc == IntPtr.Zero || cam == null)
                return;
            if (!ShouldRenderCamera(cam))
                return;

            GetOrCreateVulkanPresentationTarget(cam);
            PerCameraInfo info = GetOrCreateCameraInfo(cam, _useCommandBufferRendering);
#if UNITY_ANDROID
            if (IsVulkanRuntime() &&
                _vulkanPresentationCameras.ContainsKey(cam))
            {
                int presentationEventId = info.CameraId << 8;
                if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") &&
                    _configuredVulkanRenderEvents.Add(presentationEventId))
                {
                    int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(presentationEventId);
                    Debug.Log(
                        $"[IMM_UNITY_VK_PRESENT_EVENT_20260730] eventId={presentationEventId} " +
                        $"camera={info.CameraId} configured={configured}");
                }
            }
#endif
#if UNITY_ANDROID
            if (IsVulkanRuntime())
            {
                int preCullCount = ++_androidVulkanPreCullCount;
                if (preCullCount <= 4 || (preCullCount & (preCullCount - 1)) == 0)
                {
                    CameraEvent renderEvent = GetVulkanCommandBufferEvent();
                    Debug.Log(
                        $"[IMM_UNITY_VK_PRECULL_20260730] count={preCullCount} frame={Time.frameCount} " +
                        $"cam={cam.name} enabled={cam.enabled} active={cam.gameObject.activeInHierarchy} " +
                        $"target={(cam.targetTexture != null ? $"{cam.targetTexture.width}x{cam.targetTexture.height}" : "display")} " +
                        $"attached={cam.GetCommandBuffers(renderEvent).Length} ready={IsReadyForDocumentLoad} " +
                        $"renderable={HasRenderableDocument()}");
                }
            }
#endif

            if (!IsReadyForDocumentLoad)
            {
                // GLES on Android needs native renderer initialization to
                // complete from Unity's render-thread plugin callback, where
                // the GL context is current. Queue this pre-load event before a
                // document exists so LoadFromFile cannot race renderer creation.
                info.CommandBuffer.Clear();
                info.CommandBuffer.IssuePluginEvent(_renderEventFunc, info.CameraId << 8);
                return;
            }

            if (!HasRenderableDocument())
                return;

            int stereoMode = ResolveStereoMode(cam);

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
            if (IsVulkanRuntime())
            {
                RenderTexture vulkanTargetTexture = cam.targetTexture != null ? cam.targetTexture : cam.activeTexture;
                RenderBuffer colorBuffer = vulkanTargetTexture != null ? vulkanTargetTexture.colorBuffer : Display.main.colorBuffer;
                RenderBuffer depthBuffer = vulkanTargetTexture != null ? vulkanTargetTexture.depthBuffer : Display.main.depthBuffer;
                int vulkanSampleCount = vulkanTargetTexture != null
                    ? Math.Max(1, vulkanTargetTexture.antiAliasing)
                    : (cam.allowMSAA ? Math.Max(1, QualitySettings.antiAliasing) : 1);
                IntPtr colorRenderBuffer = colorBuffer.GetNativeRenderBufferPtr();
                IntPtr depthRenderBuffer = depthBuffer.GetNativeRenderBufferPtr();
                if (!_loggedVulkanRenderTargetSource.Contains(cam))
                {
                    _loggedVulkanRenderTargetSource.Add(cam);
                    string source = vulkanTargetTexture != null ? $"cameraTexture {vulkanTargetTexture.width}x{vulkanTargetTexture.height}" : "display";
                    Debug.Log(
                        $"[IMM_UNITY_ANDROID_VK_TARGET_20260729] cam={cam.name} cameraId={info.CameraId} " +
                        $"source={source} pixel={cam.pixelWidth}x{cam.pixelHeight} samples={vulkanSampleCount} " +
                        $"colorRenderBuffer=0x{colorRenderBuffer.ToInt64():x} " +
                        $"depthRenderBuffer=0x{depthRenderBuffer.ToInt64():x}");
                }
                ImmNativePlugin.SetVulkanCameraRenderBuffers(
                    info.CameraId,
                    colorRenderBuffer,
                    depthRenderBuffer,
                    cam.pixelWidth,
                    cam.pixelHeight,
                    vulkanSampleCount);
#if !UNITY_ANDROID
                int prepared = IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_PREPARE")
                    ? 0
                    : ImmNativePlugin.PrepareCamera(info.CameraId);
                if (prepared == 0 && _loggedVulkanPrepareWarning.Add(cam))
                {
                    Debug.LogWarning($"[IMM_UNITY_VK_PREPARE_20260612] cam={cam.name} cameraId={info.CameraId} prepared=0");
                }
#endif
            }

            if (_useCameraCallbackRendering)
                return;

            int eyeIndex = 0;
            if (stereoMode == (int)StereoMode.TwoPass && cam.stereoEnabled)
            {
                eyeIndex = cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right ? 1 : 0;
            }

            int eventId = (info.CameraId << 8) | (eyeIndex & 0x1);
            if (IsVulkanRuntime())
            {
                bool populateCommandBuffer = true;
#if UNITY_ANDROID
                // The payload is stable for a mono camera. Matrices and native
                // render-buffer metadata are updated before each replay.
                populateCommandBuffer = cam.stereoEnabled || !info.VulkanCommandBufferPopulated;
#endif
                if (!populateCommandBuffer)
                    return;

                info.CommandBuffer.Clear();
                if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") && _configuredVulkanRenderEvents.Add(eventId))
                {
                    int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(eventId);
                    Debug.Log($"[IMM_UNITY_VK_EVENTCFG_20260611] eventId={eventId} configured={configured}");
                }
                bool useCustomBlit = IsEnvFlagEnabled("IMM_UNITY_VK_USE_CUSTOM_BLIT") && !IsEnvFlagEnabled("IMM_UNITY_VK_FORCE_PLAIN_EVENT");
#if UNITY_ANDROID
                useCustomBlit = false;
#endif
                bool bindCameraTarget = !IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_BIND_CAMERA_TARGET");
#if UNITY_ANDROID
                // The explicit RenderBuffers are accessed by the native
                // callback. Pre-binding CameraTarget would cause Unity to
                // resume a camera render pass after the EnsureOutside event.
                bindCameraTarget = false;
#endif
                var cameraTarget = new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
                if (bindCameraTarget)
                {
                    if (IsEnvFlagEnabled("IMM_UNITY_VK_BIND_CAMERA_DEPTH_TARGET"))
                    {
                        info.CommandBuffer.SetRenderTarget(cameraTarget, new RenderTargetIdentifier(BuiltinRenderTextureType.Depth));
                    }
                    else
                    {
                        info.CommandBuffer.SetRenderTarget(cameraTarget);
                    }
                }
                if (useCustomBlit && _renderEventAndDataFunc != IntPtr.Zero)
                {
                    if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") && _configuredVulkanRenderEvents.Add(VulkanCustomBlitEventId))
                    {
                        int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(VulkanCustomBlitEventId);
                        Debug.Log($"[IMM_UNITY_VK_EVENTCFG_20260611] eventId={VulkanCustomBlitEventId} configured={configured}");
                    }
                    info.CommandBuffer.IssuePluginCustomBlit(_renderEventAndDataFunc, (uint)eventId, cameraTarget, cameraTarget, 0, 0);
                }
                else
                {
                    info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId);
                }
                AppendVulkanOverlayFixtureDraw(info.CommandBuffer, cam);
#if UNITY_ANDROID
                if (!cam.stereoEnabled)
                    info.VulkanCommandBufferPopulated = true;
#endif
            }
            else
            {
                info.CommandBuffer.Clear();
                info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId);
                if (IsAppleMetalRuntime() && _appleMetalQueueLogCount < 16)
                {
                    Debug.Log(
                        $"[IMM_UNITY_METAL_MANAGED_QUEUE] cam={cam.name} type={cam.cameraType} " +
                        $"cameraId={info.CameraId} eventId={eventId} viewport={cam.pixelWidth}x{cam.pixelHeight} " +
                        $"commandBufferSize={info.CommandBuffer.sizeInBytes}");
                    _appleMetalQueueLogCount++;
                }
            }
        }

        private static void AppendVulkanOverlayFixtureDraw(CommandBuffer commandBuffer, Camera cam)
        {
            if (!IsEnvFlagEnabled("IMM_UNITY_VK_COMMAND_BUFFER_OVERLAY_FIXTURE") || commandBuffer == null || cam == null)
                return;

            if (_vulkanOverlayFixtureMesh == null)
            {
                _vulkanOverlayFixtureMesh = new Mesh { name = "IMM Vulkan Overlay Fixture Quad" };
                _vulkanOverlayFixtureMesh.vertices = new[]
                {
                    new Vector3(-0.5f, -0.5f, 0.0f),
                    new Vector3( 0.5f, -0.5f, 0.0f),
                    new Vector3( 0.5f,  0.5f, 0.0f),
                    new Vector3(-0.5f,  0.5f, 0.0f)
                };
                _vulkanOverlayFixtureMesh.triangles = new[] { 0, 1, 2, 0, 2, 3, 2, 1, 0, 3, 2, 0 };
                _vulkanOverlayFixtureMesh.RecalculateBounds();
            }

            if (_vulkanOverlayFixtureMaterial == null)
            {
                Shader shader = Shader.Find("Hidden/Internal-Colored");
                _vulkanOverlayFixtureMaterial = shader != null
                    ? new Material(shader)
                    : new Material(Shader.Find("Unlit/Color"));
                _vulkanOverlayFixtureMaterial.color = new Color(1.0f, 0.05f, 0.02f, 1.0f);
                _vulkanOverlayFixtureMaterial.SetInt("_ZTest", (int)CompareFunction.Always);
                _vulkanOverlayFixtureMaterial.SetInt("_ZWrite", 0);
                _vulkanOverlayFixtureMaterial.SetInt("_Cull", (int)CullMode.Off);
            }

            commandBuffer.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);
            commandBuffer.DrawMesh(_vulkanOverlayFixtureMesh, Matrix4x4.TRS(new Vector3(0.45f, -0.05f, 0.0f), Quaternion.identity, Vector3.one * 0.55f), _vulkanOverlayFixtureMaterial);
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
                    CameraEvent renderEvent = IsVulkanRuntime() ? GetVulkanCommandBufferEvent() : CameraEvent.AfterImageEffectsOpaque;
                    if (IsVulkanRuntime())
                    {
                        Debug.Log($"[IMM_UNITY_VK_EVENT_20260612] cam={cam.name} cameraId={info.CameraId} renderEvent={renderEvent}");
                    }
                    cam.AddCommandBuffer(renderEvent, info.CommandBuffer);
                    if (IsAppleMetalRuntime())
                    {
                        int attachedCount = cam.GetCommandBuffers(renderEvent).Length;
                        Debug.Log(
                            $"[IMM_UNITY_METAL_MANAGED_ATTACH] cam={cam.name} type={cam.cameraType} " +
                            $"cameraId={info.CameraId} renderEvent={renderEvent} attachedAtEvent={attachedCount}");
                    }
                }
            }
            return info;
        }

        private void OnCameraPostRender(Camera cam)
        {
            if (!_useCameraCallbackRendering || !_isInitialized || _renderEventFunc == IntPtr.Zero || cam == null)
                return;
            if (!ShouldRenderCamera(cam))
                return;
            if (!HasRenderableDocument())
                return;

            if (!_cameras.TryGetValue(cam, out PerCameraInfo info))
                return;
#if UNITY_ANDROID
            if (IsVulkanRuntime() &&
                _vulkanPresentationCameras.TryGetValue(cam, out VulkanPresentationCamera presenter))
            {
                if (_androidVulkanPostRenderPresentationCount < 8)
                {
                    ++_androidVulkanPostRenderPresentationCount;
                    Debug.Log(
                        $"[IMM_UNITY_VK_POST_RENDER_PRESENT_20260730] camera={info.CameraId} " +
                        $"submission=presenterCommandBuffer");
                }
                return;
            }
#endif

            if (IsVulkanRuntime() && IsEnvFlagEnabled("IMM_UNITY_VK_SAMPLE_WAIT_FOR_END_OF_FRAME"))
                return;

            if (IsVulkanRuntime() && IsEnvFlagEnabled("IMM_UNITY_VK_SAMPLE_EVENT1") && !IsEnvFlagEnabled("IMM_UNITY_VK_SAMPLE_WAIT_FOR_END_OF_FRAME"))
            {
                const int sampleEventId = 1;
                if (info.CameraId != 1)
                    return;
                if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") && _configuredVulkanRenderEvents.Add(sampleEventId))
                {
                    int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(sampleEventId);
                    Debug.Log($"[IMM_UNITY_VK_SAMPLE_EVENT1_20260612] eventId={sampleEventId} camera={info.CameraId} configured={configured}");
                }
                GL.IssuePluginEvent(_renderEventFunc, sampleEventId);
                return;
            }

            int eventId = info.CameraId << 8;
            if (IsVulkanRuntime() &&
                !IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") &&
                _configuredVulkanRenderEvents.Add(eventId))
            {
                int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(eventId);
                Debug.Log($"[IMM_UNITY_VK_EVENTCFG_20260611] eventId={eventId} configured={configured}");
            }
            if (_appleMetalEventLogCount < 8)
            {
                Debug.Log($"[IMM_UNITY_METAL_EVENT] camera={info.CameraId} viewport={cam.pixelWidth}x{cam.pixelHeight} eventId={eventId}");
                _appleMetalEventLogCount++;
            }
            GL.IssuePluginEvent(_renderEventFunc, eventId);
        }

        private IEnumerator IssueVulkanSampleEventAfterEndOfFrame()
        {
            WaitForEndOfFrame wait = new WaitForEndOfFrame();
            while (enabled)
            {
                yield return wait;
                if (!IsVulkanRuntime() || !IsEnvFlagEnabled("IMM_UNITY_VK_SAMPLE_EVENT1") || !_isInitialized || _renderEventFunc == IntPtr.Zero)
                    continue;
                if (!HasRenderableDocument())
                    continue;

                bool hasMainCameraTarget = false;
                foreach (PerCameraInfo info in _cameras.Values)
                {
                    if (info.CameraId == 1)
                    {
                        hasMainCameraTarget = true;
                        break;
                    }
                }
                if (!hasMainCameraTarget)
                    continue;

                const int sampleEventId = 1;
                if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") && _configuredVulkanRenderEvents.Add(sampleEventId))
                {
                    int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(sampleEventId);
                    Debug.Log($"[IMM_UNITY_VK_SAMPLE_WFE_20260612] eventId={sampleEventId} configured={configured}");
                }
                GL.IssuePluginEvent(_renderEventFunc, sampleEventId);
            }
        }

        private bool HasRenderableDocument()
        {
            if (_nativeUnloadsInFlight.Count > 0)
                return true;

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

        private static int ResolveStereoMode(Camera cam)
        {
            if (!cam.stereoEnabled)
                return (int)StereoMode.Mono;

            // Keep XRSettings optional so non-XR Unity package builds compile without XR package symbols.
            Type xrSettingsType = Type.GetType("UnityEngine.XR.XRSettings, UnityEngine.XRModule");
            object mode = xrSettingsType?.GetProperty("stereoRenderingMode")?.GetValue(null);
            string modeName = mode?.ToString();

            if (modeName == "SinglePass")
                return (int)StereoMode.SinglePass;

            // The native plugin doesn't support instanced single-pass; force two-pass.
            return (int)StereoMode.TwoPass;
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
