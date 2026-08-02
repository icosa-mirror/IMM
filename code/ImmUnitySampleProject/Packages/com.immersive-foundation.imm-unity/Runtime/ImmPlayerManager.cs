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
    [DisallowMultipleComponent]
    internal sealed class ImmAndroidVulkanPresenter : MonoBehaviour
    {
        private ImmPlayerManager _owner;
        private Camera _camera;

        internal void Configure(ImmPlayerManager owner, Camera camera)
        {
            _owner = owner;
            _camera = camera;
        }

        private void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (_owner == null || !_owner.enabled || !_owner.PresentFlatAndroidVulkanFrame(_camera, source, destination))
                Graphics.Blit(source, destination);
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
            if (forceBackbuffer)
                return ImmProjectionDestination.ForcedBackbuffer;
            if (forceRenderTexture)
                return ImmProjectionDestination.ForcedRenderTexture;
            if (hasExplicitRenderTexture)
                return ImmProjectionDestination.ExplicitRenderTexture;
            if (cameraType == CameraType.SceneView)
                return ImmProjectionDestination.EditorSceneView;
            if (stereoEnabled)
                return ImmProjectionDestination.XrDisplay;
            if (isEditor && cameraType == CameraType.Game)
                return ImmProjectionDestination.EditorGameView;
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
        private readonly Dictionary<Camera, ImmProjectionDestination> _lastProjectionDestinationLogged =
            new Dictionary<Camera, ImmProjectionDestination>();
        private readonly HashSet<Camera> _loggedVulkanRenderTargetSource = new HashSet<Camera>();
        private readonly HashSet<Camera> _loggedVulkanPrepareWarning = new HashSet<Camera>();
        private readonly HashSet<int> _configuredVulkanRenderEvents = new HashSet<int>();
        private const string NearDiagPrefix = "[IMMDBG_NEAR_20260208A] ";
        private const string ProjectionDestinationDiagPrefix = "[IMM_PROJECTION_TARGET_20260725] ";
        private const int VulkanCustomBlitEventId = 6;
        private static readonly List<UnityEngine.XR.XRDisplaySubsystem> _xrDisplaySubsystems = new List<UnityEngine.XR.XRDisplaySubsystem>();
        private bool _useCommandBufferRendering = false;
        private bool _useCameraCallbackRendering = false;
        private Coroutine _vulkanSampleEventCoroutine = null;
        private int _appleMetalEventLogCount = 0;
        private bool _flatAndroidVulkanHostComposition;
        private static Mesh _vulkanOverlayFixtureMesh;
        private static Material _vulkanOverlayFixtureMaterial;

        private static HashSet<string> _debugFlagFileCache;

        private static bool IsEnvFlagEnabled(string name)
        {
            string value = Environment.GetEnvironmentVariable(name);
            if (!string.IsNullOrEmpty(value))
                return value != "0";
#if UNITY_ANDROID && !UNITY_EDITOR
            // Env vars never reach an Android app process. Mirror the flags to a device
            // file instead (one flag name per line):
            //   adb shell "echo IMM_UNITY_VK_NO_RENDER_EVENTS > /sdcard/Android/data/<pkg>/files/imm_debug_flags.txt"
            if (_debugFlagFileCache == null)
            {
                _debugFlagFileCache = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                try
                {
                    string path = Path.Combine(Application.persistentDataPath, "imm_debug_flags.txt");
                    if (File.Exists(path))
                    {
                        foreach (string line in File.ReadAllLines(path))
                        {
                            string flag = line.Trim();
                            if (flag.Length > 0 && !flag.StartsWith("#"))
                                _debugFlagFileCache.Add(flag);
                        }
                        Debug.Log($"[IMM_DEBUG_FLAGS] loaded {_debugFlagFileCache.Count} flags from {path}");
                    }
                }
                catch (Exception e)
                {
                    Debug.LogWarning($"[IMM_DEBUG_FLAGS] failed to read flag file: {e.Message}");
                }
            }
            return _debugFlagFileCache.Contains(name);
#else
            return false;
#endif
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
            PushFlagFileToNativeEnvironment();
        }

        // Mirror every flag-file entry into the native process environment at
        // boot. Raw-getenv toggles live all over the player/renderer libs and
        // the Android process env is otherwise EMPTY - they were silently dead
        // on device (only main.cpp's helper falls back to debug.imm sysprops).
        // Supports both bare-flag lines (NAME -> "1") and NAME=VALUE lines.
        private static void PushFlagFileToNativeEnvironment()
        {
#if UNITY_ANDROID && !UNITY_EDITOR
            try
            {
                string path = Path.Combine(Application.persistentDataPath, "imm_debug_flags.txt");
                if (!File.Exists(path))
                    return;
                int pushed = 0;
                foreach (string line in File.ReadAllLines(path))
                {
                    string flag = line.Trim();
                    if (flag.Length == 0 || flag.StartsWith("#"))
                        continue;
                    int eq = flag.IndexOf('=');
                    string name = eq > 0 ? flag.Substring(0, eq).Trim() : flag;
                    string value = eq > 0 ? flag.Substring(eq + 1).Trim() : "1";
                    if (name.Length == 0)
                        continue;
                    ImmNativePlugin.SetRuntimeFlag(name, value);
                    pushed++;
                }
                Debug.Log($"[IMM_DEBUG_FLAGS] pushed {pushed} flags into the native environment");
            }
            catch (Exception e)
            {
                Debug.LogWarning($"[IMM_DEBUG_FLAGS] failed to push flags to native env: {e.Message}");
            }
#endif
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
            // Heartbeat: proves the main thread is alive when native logs go silent
            // (distinguishes an app-wide wedge from a render-thread-only stall).
            if (Time.frameCount % 72 == 0)
                Debug.Log($"[IMM_HEARTBEAT] frame={Time.frameCount} t={Time.realtimeSinceStartup:F1}s");
            foreach (var kvp in _cameras)
            {
                MaybeDumpVulkanEyeTargets(kvp.Value);
                MaybeCaptureEyeBurst(kvp.Value);
            }
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

#if UNITY_ANDROID && !UNITY_EDITOR
                bool allowDedicatedVulkanQueue =
                    SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan;
                ImmNativePlugin.SetVulkanDedicatedQueueAllowed(allowDedicatedVulkanQueue ? 1 : 0);
                Debug.Log(
                    $"[IMM_UNITY_VK_QUEUE_20260802] vulkanDevice={allowDedicatedVulkanQueue} " +
                    $"mode={(allowDedicatedVulkanQueue ? "dedicated-requested" : "host-access-queue")}");
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

        public int LoadedDocumentCount => _loadedDocuments.Count;

        public bool IsInitialized => _isInitialized;

        public int OwnedInputBufferCount => _documentMemoryPtrs.Count;

        public int PendingUnloadDocumentCount => _pendingUnloadDocuments.Count;

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
                    continue;

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
            public readonly float[] WorldToHead = new float[16];
            public readonly float[] HeadProj = new float[16];
            public readonly float[] WorldToLeft = new float[16];
            public readonly float[] LeftProj = new float[16];
            public readonly float[] WorldToRight = new float[16];
            public readonly float[] RightProj = new float[16];
            // Quest Vulkan: IMM renders each eye into its own offscreen texture on its
            // dedicated queue; Unity composites it back with a material blit.
            // SINGLE-buffered per eye (write==read, same frame): the native composite
            // bridge queues a wait-only submission on Unity's queue so the blit
            // executes after the eye submit on the GPU - same-frame reads are
            // ordered-correct. The old TRIPLE buffer sampled LAST frame's image;
            // that one-frame-stale pose, re-warped by the compositor, was the
            // world-locked-to-head artifact (root-caused + user-verified 2026-07-28).
            // Opt-in: IMM_UNITY_VK_TRIPLE_BUFFER restores the stale-read scheme for
            // A/B; IMM_UNITY_VK_NO_DOUBLE_BUFFER still forces single (now default).
            public readonly RenderTexture[] VulkanEyeTargets = new RenderTexture[2];
            public readonly RenderTexture[,] VulkanEyeBuffers = new RenderTexture[2, 3];
        }

        private Material _vulkanCompositeMaterial;
        private int _vulkanCompositeLogCount;
        private int _vulkanOnRenderImageLogCount;

        // Returns the WRITE buffer for this frame (handed to the native renderer)
        // and updates VulkanEyeTargets[eye] to the READ buffer Unity samples
        // (previous frame's image; two frames from being rewritten).
        private RenderTexture EnsureVulkanEyeTarget(PerCameraInfo info, int eye, int width, int height)
        {
            bool buffered = IsEnvFlagEnabled("IMM_UNITY_VK_TRIPLE_BUFFER") &&
                            !IsEnvFlagEnabled("IMM_UNITY_VK_NO_DOUBLE_BUFFER");
            int writeSlot = buffered ? Time.frameCount % 3 : 0;
            int readSlot = buffered ? (Time.frameCount + 2) % 3 : 0;
            RenderTexture write = EnsureVulkanEyeBuffer(info, eye, writeSlot, width, height);
            RenderTexture read = buffered ? EnsureVulkanEyeBuffer(info, eye, readSlot, width, height) : write;
            info.VulkanEyeTargets[eye] = read;
            return write;
        }

        private RenderTexture EnsureVulkanEyeBuffer(PerCameraInfo info, int eye, int parity, int width, int height)
        {
            RenderTexture rt = info.VulkanEyeBuffers[eye, parity];
            if (rt != null && (rt.width != width || rt.height != height))
            {
                rt.Release();
                UnityEngine.Object.Destroy(rt);
                rt = null;
            }
            if (rt == null)
            {
                rt = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32)
                {
                    name = $"IMM Vulkan Eye {eye}.{parity} (cam {info.CameraId})",
                    antiAliasing = 1,
                    useMipMap = false,
                    autoGenerateMips = false
                };
                rt.Create();
                Debug.Log($"[IMM_UNITY_VK_OFFSCREEN_20260716] created eye buffer cam={info.CameraId} eye={eye} parity={parity} {width}x{height}");
            }
            info.VulkanEyeBuffers[eye, parity] = rt;
            return rt;
        }

        private int _rtDumpCounter;

        // On-demand stereo-pair burst (glitch forensics): captures BOTH eyes'
        // READ buffers within the same frame for N consecutive frames, so a
        // user-triggered capture at the moment of a perceived glitch yields
        // true simultaneous stereo pairs (the periodic dump writes the eyes
        // ~200ms apart and cannot be compared as a pair).
        private static int _burstFramesRemaining;
        private static int _burstId;

        public static void RequestEyeBurst(int frames = 8)
        {
            // 16 full-res ReadPixels = a deliberate multi-frame hitch. Armed only
            // via flag so a stray stick-click can't tank a session (user hit
            // this: "pressing the right analog stick killed performance").
            if (Instance == null || !IsEnvFlagEnabled("IMM_UNITY_VK_ENABLE_BURST"))
            {
                Debug.Log("[IMM_BURST] ignored (arm with IMM_UNITY_VK_ENABLE_BURST)");
                return;
            }
            _burstId++;
            _burstFramesRemaining = frames;
            Debug.Log($"[IMM_BURST] capture burst {_burstId} requested ({frames} frames)");
        }

        private void MaybeCaptureEyeBurst(PerCameraInfo info)
        {
            if (_burstFramesRemaining <= 0)
                return;
            RenderTexture left = info.VulkanEyeTargets[0];
            RenderTexture right = info.VulkanEyeTargets[1];
            if (left == null || right == null)
                return;
            int frame = _burstFramesRemaining--;
            for (int eye = 0; eye < 2; eye++)
            {
                RenderTexture rt = eye == 0 ? left : right;
                RenderTexture prev = RenderTexture.active;
                var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false);
                RenderTexture.active = rt;
                tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
                tex.Apply(false);
                RenderTexture.active = prev;
                string path = System.IO.Path.Combine(Application.persistentDataPath,
                    $"imm_burst{_burstId}_f{frame}_eye{eye}.png");
                System.IO.File.WriteAllBytes(path, tex.EncodeToPNG());
                Destroy(tex);
            }
            if (_burstFramesRemaining == 0)
                Debug.Log($"[IMM_BURST] burst {_burstId} complete");
        }

        private void MaybeDumpVulkanEyeTargets(PerCameraInfo info)
        {
            if (!IsEnvFlagEnabled("IMM_UNITY_VK_DUMP_RTS"))
                return;
            _rtDumpCounter++;
            // Fire at ~frame 60 (<1s at 72fps) and re-dump every 60 frames so a
            // short or interrupted (doff/don) session still captures a recent
            // frame; the old "exactly frame 300" gate needed ~4.2s of
            // uninterrupted rendering, which a flickering-mount session never
            // reached (that is why on-device dumps came up empty).
            if (_rtDumpCounter < 60 || _rtDumpCounter % 60 != 0)
                return;
            for (int eye = 0; eye < 2; eye++)
            {
                RenderTexture rt = info.VulkanEyeTargets[eye];
                if (rt == null)
                    continue;
                RenderTexture previous = RenderTexture.active;
                RenderTexture.active = rt;
                var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false);
                tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
                tex.Apply();
                RenderTexture.active = previous;
                string path = Path.Combine(Application.persistentDataPath, $"imm_rt_eye{eye}.png");
                File.WriteAllBytes(path, tex.EncodeToPNG());
                Destroy(tex);
                Debug.Log($"[IMM_RT_DUMP] wrote {path}");
            }
        }

        // World->tracking-space matrix published by the free-fly rig (Assets/
        // Scripts/ImmFreeFly.cs). Identity when the rig hasn't moved. Folded
        // into the XR-params eye views under IMM_UNITY_VK_FREEFLY_COMPOSE.
        public static Matrix4x4 ExternalWorldToTracking = Matrix4x4.identity;

        private GameObject _compositeQuad;
        private Material _compositeQuadMaterial;
        private bool _compositeQuadFailed;
        private int _lastMatrixSetFrame = -1;

        // In-pass composite: a fullscreen quad in Unity's own camera pass
        // replaces the per-eye CommandBuffer.Blit (the blit's render-target
        // switch broke Unity's pass and measured ~9ms/frame on Quest 3).
        // Kill-switch IMM_UNITY_VK_NO_COMPOSITE_QUAD restores the blit.
        private Material EnsureCompositeQuad(Camera cam, PerCameraInfo info)
        {
            if (_compositeQuadFailed || cam == null)
                return null;
            if (_compositeQuadMaterial == null)
            {
                Shader shader = Resources.Load<Shader>("ImmVulkanCompositeQuad");
                if (shader == null)
                {
                    _compositeQuadFailed = true;
                    Debug.LogWarning("[IMM_QUAD] ImmVulkanCompositeQuad shader missing from Resources; falling back to Blit composite");
                    return null;
                }
                _compositeQuadMaterial = new Material(shader);
                _compositeQuadMaterial.SetFloat("_FlipY", IsEnvFlagEnabled("IMM_UNITY_VK_QUAD_FLIPY") ? 1f : 0f);
                Debug.Log("[IMM_QUAD] composite quad material created (in-pass composite active)");
            }
            if (_compositeQuad == null)
            {
                _compositeQuad = GameObject.CreatePrimitive(PrimitiveType.Quad);
                _compositeQuad.name = "ImmVulkanCompositeQuad";
                var collider = _compositeQuad.GetComponent<Collider>();
                if (collider != null)
                    Destroy(collider);
                var renderer = _compositeQuad.GetComponent<MeshRenderer>();
                renderer.sharedMaterial = _compositeQuadMaterial;
                renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
                renderer.receiveShadows = false;
                renderer.motionVectorGenerationMode = MotionVectorGenerationMode.ForceNoMotion;
                // Parent in front of the camera purely to defeat frustum culling;
                // the shader emits fullscreen clip-space coordinates regardless.
                _compositeQuad.transform.SetParent(cam.transform, false);
                _compositeQuad.transform.localPosition = new Vector3(0f, 0f, 0.5f);
                Debug.Log("[IMM_QUAD] composite quad created under camera");
            }
            if (info.VulkanEyeTargets[0] != null)
                _compositeQuadMaterial.SetTexture("_EyeTex0", info.VulkanEyeTargets[0]);
            if (info.VulkanEyeTargets[1] != null)
                _compositeQuadMaterial.SetTexture("_EyeTex1", info.VulkanEyeTargets[1]);
            return _compositeQuadMaterial;
        }

        private Material GetVulkanCompositeMaterial()
        {
            if (_vulkanCompositeMaterial == null)
            {
                // ALPHA composite is the ship default: documents without full 360
                // coverage (e.g. The Art of Change) must show Unity content
                // through empty IMM pixels - opaque overwrote it everywhere (the
                // missing-white-cube bug). Opaque saves the eye-buffer read and
                // is opt-in for full-360 docs: IMM_UNITY_VK_OPAQUE_COMPOSITE
                // (the old NO_OPAQUE_COMPOSITE kill remains honored as alpha).
                if (IsEnvFlagEnabled("IMM_UNITY_VK_OPAQUE_COMPOSITE") &&
                    !IsEnvFlagEnabled("IMM_UNITY_VK_NO_OPAQUE_COMPOSITE"))
                {
                    Shader opaque = Resources.Load<Shader>("ImmVulkanCompositeOpaque");
                    if (opaque != null)
                        _vulkanCompositeMaterial = new Material(opaque);
                }
                if (_vulkanCompositeMaterial == null)
                {
                    Material loaded = Resources.Load<Material>("ImmVulkanComposite");
                    if (loaded != null)
                    {
                        _vulkanCompositeMaterial = loaded;
                    }
                    else
                    {
                        Shader shader = Shader.Find("Unlit/Transparent");
                        if (shader != null)
                            _vulkanCompositeMaterial = new Material(shader);
                        Debug.LogWarning("[IMM_UNITY_VK_OFFSCREEN_20260716] ImmVulkanComposite material missing from Resources; Shader.Find fallback " + (_vulkanCompositeMaterial != null ? "succeeded" : "FAILED"));
                    }
                }
                if (_vulkanCompositeMaterial != null)
                {
                    // Parity has ONE owner: the native Vulkan renderer draws with a
                    // negative-viewport-height (GL-convention projection in, top-down
                    // image out), and C# sends the BACKBUFFER-convention projection on
                    // this path (UseRenderIntoTextureProjection returns false for the
                    // stereo Quest camera). The offscreen RT is therefore already
                    // display-oriented and the composite must NOT flip. The old (1,-1)
                    // flip was calibrated against a texture-convention projection
                    // default that no longer exists; with today's parity it inverted
                    // the headset view (look-up-goes-down + broken stereo fusion on
                    // Quest's vertically-asymmetric frusta = "no depth").
                    // A/B without rebuild: IMM_UNITY_VK_COMPOSITE_VFLIP restores the flip.
                    // Always write ST explicitly - the loaded asset may carry a stale
                    // serialized flip from earlier sessions.
                    bool flip = IsEnvFlagEnabled("IMM_UNITY_VK_COMPOSITE_VFLIP") &&
                                !IsEnvFlagEnabled("IMM_UNITY_VK_NO_COMPOSITE_VFLIP");
                    _vulkanCompositeMaterial.SetTextureScale("_MainTex", new Vector2(1f, flip ? -1f : 1f));
                    _vulkanCompositeMaterial.SetTextureOffset("_MainTex", new Vector2(0f, flip ? 1f : 0f));
                    Debug.Log($"[IMM_VK_PARITY] compositeVFlip={flip} composite={(_vulkanCompositeMaterial.shader != null ? _vulkanCompositeMaterial.shader.name : "?")} projectionOwner=native-negative-viewport");
                }
            }
            return _vulkanCompositeMaterial;
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
            // Quest runs Vulkan; the native plugin now supports the Unity Vulkan
            // external-device overlay path on Android as well as desktop.
            return SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan;
#else
            return false;
#endif
        }

        /// <summary>
        /// Authoritative per-eye view/projection straight from the XR display subsystem's
        /// render parameters. On Quest+Vulkan, Camera.GetStereoViewMatrix returned the
        /// camera's static (untracked) view - IMM content rendered head-locked. The XR
        /// render pass parameters are what the runtime actually tracks with.
        /// </summary>
        private static bool TryGetXrEyeViewProjection(Camera cam, int eye, out Matrix4x4 view, out Matrix4x4 projection)
        {
            view = Matrix4x4.identity;
            projection = Matrix4x4.identity;
            if (cam == null)
                return false;
            SubsystemManager.GetSubsystems(_xrDisplaySubsystems);
            if (_xrDisplaySubsystems.Count == 0)
                return false;
            UnityEngine.XR.XRDisplaySubsystem display = _xrDisplaySubsystems[0];
            if (display == null || !display.running)
                return false;
            int renderPassCount = display.GetRenderPassCount();
            if (renderPassCount <= 0)
                return false;
            // MultiPass: one render pass per eye, one render parameter each.
            // SinglePass: one pass with two render parameters.
            int passIndex = renderPassCount > 1 ? Mathf.Clamp(eye, 0, renderPassCount - 1) : 0;
            display.GetRenderPass(passIndex, out UnityEngine.XR.XRDisplaySubsystem.XRRenderPass renderPass);
            int parameterCount = renderPass.GetRenderParameterCount();
            if (parameterCount <= 0)
                return false;
            int parameterIndex = renderPassCount > 1 ? 0 : Mathf.Clamp(eye, 0, parameterCount - 1);
            renderPass.GetRenderParameter(cam, parameterIndex, out UnityEngine.XR.XRDisplaySubsystem.XRRenderParameter parameter);
            view = parameter.view;
            projection = parameter.projection;
            return true;
        }

        private static RenderTexture GetXrEyeRenderTexture(Camera cam, int stereoMode)
        {
            if (cam == null || !cam.stereoEnabled)
                return null;
            SubsystemManager.GetSubsystems(_xrDisplaySubsystems);
            if (_xrDisplaySubsystems.Count == 0)
                return null;
            UnityEngine.XR.XRDisplaySubsystem display = _xrDisplaySubsystems[0];
            if (display == null || !display.running)
                return null;
            int renderPassCount = display.GetRenderPassCount();
            if (renderPassCount <= 0)
                return null;
            int passIndex = 0;
            if (stereoMode == (int)StereoMode.TwoPass &&
                cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right &&
                renderPassCount > 1)
            {
                passIndex = 1;
            }
            return display.GetRenderTextureForRenderPass(passIndex);
        }

        private static CameraEvent GetVulkanCommandBufferEvent(Camera cam)
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

            // Unity 6 XR on Quest stops executing AfterSkybox command buffers once the
            // XR eye render path takes over (~2 frames after FOCUSED) - PreCull kept
            // issuing events but the marker never dispatched. Keep the proven XR hook.
            // Flat Android presentation is performed by OnRenderImage. Schedule the
            // native offscreen render before image effects so it is complete before
            // Unity hands the presenter its real final destination.
            return cam != null && cam.stereoEnabled
                ? CameraEvent.AfterImageEffectsOpaque
                : CameraEvent.AfterForwardOpaque;
        }

        private static bool IsFlatAndroidVulkanCamera(Camera cam)
        {
            return Application.platform == RuntimePlatform.Android &&
                   IsVulkanRuntime() &&
                   cam != null &&
                   !cam.stereoEnabled &&
                   !IsEnvFlagEnabled("IMM_UNITY_VK_NO_OFFSCREEN_TARGET");
        }

        private bool UsesFlatAndroidVulkanHostComposition(Camera cam)
        {
            return _flatAndroidVulkanHostComposition && IsFlatAndroidVulkanCamera(cam);
        }

        private bool UsesFlatAndroidVulkanPresenter(Camera cam)
        {
            return IsFlatAndroidVulkanCamera(cam) && !UsesFlatAndroidVulkanHostComposition(cam);
        }

        public void SetFlatAndroidVulkanHostCompositionForValidation(bool enabled)
        {
            _flatAndroidVulkanHostComposition = enabled;
            Debug.Log(
                $"[IMM_UNITY_ANDROID_VK_HOST_COMPOSITION_20260802] enabled={(enabled ? 1 : 0)} " +
                $"frame={Time.frameCount}");
        }

        private void EnsureFlatAndroidVulkanPresenter(Camera cam)
        {
            if (!UsesFlatAndroidVulkanPresenter(cam))
                return;

            ImmAndroidVulkanPresenter presenter = cam.GetComponent<ImmAndroidVulkanPresenter>();
            if (presenter == null)
                presenter = cam.gameObject.AddComponent<ImmAndroidVulkanPresenter>();
            presenter.Configure(this, cam);
        }

        internal bool PresentFlatAndroidVulkanFrame(
            Camera cam,
            RenderTexture source,
            RenderTexture destination)
        {
            if (!UsesFlatAndroidVulkanPresenter(cam) ||
                !_cameras.TryGetValue(cam, out PerCameraInfo info))
                return false;

            RenderTexture eyeTarget = info.VulkanEyeTargets[0];
            Material composite = GetVulkanCompositeMaterial();
            if (eyeTarget == null || composite == null)
                return false;

            // OnRenderImage is Unity's supported final-presentation hook in the
            // built-in pipeline. Unity owns the destination (including Android's
            // Vulkan swapchain/intermediate and pre-rotation), so no native code
            // attempts to discover or retain the display image.
            Graphics.Blit(source, destination);
            Graphics.Blit(eyeTarget, destination, composite);
            if (_vulkanOnRenderImageLogCount < 8)
            {
                ++_vulkanOnRenderImageLogCount;
                Debug.Log(
                    $"[IMM_UNITY_VK_ONRENDERIMAGE_20260802] frame={Time.frameCount} cam={cam.name} " +
                    $"source={(source != null ? source.GetInstanceID() : 0)} " +
                    $"destination={(destination != null ? destination.GetInstanceID() : 0)} " +
                    $"imm={eyeTarget.GetInstanceID()} shader={composite.shader.name} " +
                    $"supported={composite.shader.isSupported}");
            }
            return true;
        }

        public void SetRenderCamera(Camera camera)
        {
            renderCamera = camera;
        }

        public void ClearRenderCamera()
        {
            renderCamera = null;
        }

        public RenderTexture GetAndroidVulkanPresentationTargetForValidation(Camera cam)
        {
            if (cam == null || !_cameras.TryGetValue(cam, out PerCameraInfo info))
                return null;
            return info.VulkanEyeTargets[0];
        }

        public RenderTexture GetAndroidVulkanUnityPresentationTargetForValidation(Camera cam)
        {
            // The fork composites directly into Unity's camera target. There is no
            // second texture containing both IMM and Unity geometry, so callers
            // must capture the presented frame for composition diagnostics.
            return null;
        }

        private bool ShouldRenderCamera(Camera cam)
        {
            if (cam == null)
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
        }

        private int _preCullCount;

        private void OnCameraPreCull(Camera cam)
        {
            if (cam != null)
            {
                _preCullCount++;
                if (_preCullCount <= 12 || _preCullCount % 144 == 0)
                {
                    Matrix4x4 l = cam.GetStereoViewMatrix(Camera.StereoscopicEye.Left);
                    // Derive the camera's world pose from the view matrix IMM actually
                    // uses (camera path). Turn your head slowly: fwd should point where
                    // you look (and track the SAME way you turn), up should stay ~ +Y.
                    // "behind me" => fwd ~ negated; "rotation opposite" => fwd turns the
                    // wrong way; "upside down" => up ~ -Y.
                    Matrix4x4 lInv = l.inverse;
                    Vector3 camPos = lInv.GetColumn(3);
                    Vector3 camFwd = lInv.MultiplyVector(new Vector3(0f, 0f, -1f));
                    Vector3 camUp = lInv.MultiplyVector(new Vector3(0f, 1f, 0f));
                    // Stereo/IPD probe: separation between the two eyes' world positions
                    // should be ~0.06m (6cm) along the head's right axis. ~0 => no stereo
                    // => "flat". Large => odd world-scale. This is the offscreen per-eye
                    // matrices IMM actually renders with.
                    Vector3 rPos = cam.GetStereoViewMatrix(Camera.StereoscopicEye.Right).inverse.GetColumn(3);
                    Vector3 eyeSep = camPos - rPos;
                    Debug.Log($"[IMM_PRECULL] n={_preCullCount} cam={cam.name} stereo={cam.stereoEnabled} xrActive={UnityEngine.XR.XRSettings.isDeviceActive} camPath pos=({camPos.x:F2},{camPos.y:F2},{camPos.z:F2}) fwd=({camFwd.x:F2},{camFwd.y:F2},{camFwd.z:F2}) up=({camUp.x:F2},{camUp.y:F2},{camUp.z:F2}) ipd={eyeSep.magnitude:F4} eyeSep=({eyeSep.x:F3},{eyeSep.y:F3},{eyeSep.z:F3})");
                }
            }
            if (!_isInitialized || _renderEventFunc == IntPtr.Zero || cam == null)
                return;
            if (!ShouldRenderCamera(cam))
                return;

            PerCameraInfo info = GetOrCreateCameraInfo(cam, _useCommandBufferRendering);
            bool useHostComposition = UsesFlatAndroidVulkanHostComposition(cam);

            if (IsVulkanRuntime() && IsEnvFlagEnabled("IMM_UNITY_VK_NO_RENDER_EVENTS"))
            {
                // Diagnostic: issue NO plugin events at all (Quest: every custom marker
                // callback makes Unity finalize the XR frame mid-frame). Native rendering
                // and deferred init will not run.
                info.CommandBuffer.Clear();
                return;
            }

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

            // Single pose sample per frame: multipass runs this per eye pass with
            // poses ~half a frame apart, but the compositor timewarps BOTH eyes
            // from ONE frame pose - so the first-rendered (left) eye is warped
            // from the wrong reference during head motion ("renders from a
            // different position", left-eye judder). Upload matrices only on the
            // frame's first eye pass so both eyes share one pose sample.
            // Kill: IMM_UNITY_VK_NO_SINGLE_POSE.
            bool updateMatrices = _lastMatrixSetFrame != Time.frameCount ||
                                  !cam.stereoEnabled ||
                                  IsEnvFlagEnabled("IMM_UNITY_VK_NO_SINGLE_POSE");
            if (updateMatrices)
            {
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

            // Quest+Vulkan: Camera.GetStereoViewMatrix returns the camera's static view
            // (no head tracking) - IMM rendered head-locked. Pull the tracked per-eye
            // view/projection from the XR display subsystem's render parameters instead.
            bool xrMatricesApplied = false;
            if (Application.platform == RuntimePlatform.Android && IsVulkanRuntime() &&
                !IsEnvFlagEnabled("IMM_UNITY_VK_NO_XR_RENDER_PARAMS"))
            {
                if (TryGetXrEyeViewProjection(cam, 0, out Matrix4x4 leftView, out Matrix4x4 leftProj) &&
                    TryGetXrEyeViewProjection(cam, 1, out Matrix4x4 rightView, out Matrix4x4 rightProj))
                {
                    // Free-fly locomotion (ImmFreeFly moves a rig above the tracked
                    // camera). If the XR render-parameter views turn out to be
                    // TRACKING-space (rig ignored), fold the rig in here; if they
                    // are already world-space this would double-apply - hence the
                    // runtime flag for the on-device A/B.
                    if (IsEnvFlagEnabled("IMM_UNITY_VK_FREEFLY_COMPOSE"))
                    {
                        leftView = leftView * ExternalWorldToTracking;
                        rightView = rightView * ExternalWorldToTracking;
                    }
                    ConvertMatrixToArray(info.WorldToLeft, leftView);
                    ConvertMatrixToArray(info.LeftProj, GL.GetGPUProjectionMatrix(leftProj, renderIntoTexture));
                    ConvertMatrixToArray(info.WorldToRight, rightView);
                    ConvertMatrixToArray(info.RightProj, GL.GetGPUProjectionMatrix(rightProj, renderIntoTexture));
                    xrMatricesApplied = true;
                    if (stereoMode == (int)StereoMode.Mono)
                        stereoMode = (int)StereoMode.TwoPass;
                    if (_preCullCount <= 12 || _preCullCount % 144 == 0)
                    {
                        // Same derivation as [IMM_PRECULL] but for the XR-render-params
                        // path, so we can compare which one tracks correctly.
                        Matrix4x4 xlInv = leftView.inverse;
                        Vector3 xPos = xlInv.GetColumn(3);
                        Vector3 xFwd = xlInv.MultiplyVector(new Vector3(0f, 0f, -1f));
                        Vector3 xUp = xlInv.MultiplyVector(new Vector3(0f, 1f, 0f));
                        Debug.Log($"[IMM_XRPARAM] n={_preCullCount} xrPath pos=({xPos.x:F2},{xPos.y:F2},{xPos.z:F2}) fwd=({xFwd.x:F2},{xFwd.y:F2},{xFwd.z:F2}) up=({xUp.x:F2},{xUp.y:F2},{xUp.z:F2})");
                    }
                }
                else if (_preCullCount <= 12 || _preCullCount % 144 == 0)
                {
                    Debug.LogWarning($"[IMM_XRPARAM] n={_preCullCount} XR render parameters unavailable; falling back to camera stereo matrices");
                }
            }

            bool hasStereoMatrices = cam.stereoEnabled || xrMatricesApplied;
            ImmNativePlugin.SetMatrices(
                info.CameraId,
                stereoMode,
                info.WorldToHead,
                info.HeadProj,
                hasStereoMatrices ? info.WorldToLeft : null,
                hasStereoMatrices ? info.LeftProj : null,
                hasStereoMatrices ? info.WorldToRight : null,
                hasStereoMatrices ? info.RightProj : null);
            ImmNativePlugin.SetCameraViewport(info.CameraId, cam.pixelWidth, cam.pixelHeight);
            _lastMatrixSetFrame = Time.frameCount;
            }
            if (IsVulkanRuntime())
            {
                if (IsEnvFlagEnabled("IMM_UNITY_VK_BLUE_CANARY") && cam.clearFlags != CameraClearFlags.SolidColor)
                {
                    // Diagnostic canary: prove Unity's own Vulkan output path is visible at all.
                    cam.clearFlags = CameraClearFlags.SolidColor;
                    cam.backgroundColor = new Color(0.0f, 0.4f, 1.0f, 1.0f);
                    Debug.Log("[IMM_UNITY_VK_CANARY_20260716] camera forced to solid blue clear");
                }

                int vulkanEye = 0;
                if (stereoMode == (int)StereoMode.TwoPass && cam.stereoEnabled && cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right)
                    vulkanEye = 1;
                bool useOffscreenTargets = Application.platform == RuntimePlatform.Android &&
                    !IsEnvFlagEnabled("IMM_UNITY_VK_NO_OFFSCREEN_TARGET") &&
                    !useHostComposition;

                if (useOffscreenTargets)
                {
                    // Quest: IMM must not write into the XR eye buffer directly - its own-queue
                    // writes race Unity's camera pass and get cleared/overwritten. Render into an
                    // offscreen texture instead; the composite blit below runs inside Unity's own
                    // pass where ordering is guaranteed.
                    // A/B lever: route BOTH eyes' native draws into eye0's RT. Distinguishes an
                    // eye1-image problem (strokes appear doubled) from an eye1-pass problem
                    // (still a single stroke set).
                    int rtEye = IsEnvFlagEnabled("IMM_UNITY_VK_EYE0_RT_BOTH_EYES") ? 0 : vulkanEye;
                    RenderTexture eyeTarget = EnsureVulkanEyeTarget(info, rtEye, cam.pixelWidth, cam.pixelHeight);
                    // Host-depth interleave groundwork (opt-in): hand the XR
                    // swapchain's depth surface to the plugin so IMM strokes can
                    // depth-test against Unity geometry. Off by default until the
                    // native 4x depth-prime draw lands (attaching 1x host depth
                    // today forces the pass back to single-sampled).
                    IntPtr hostDepthPtr = IntPtr.Zero;
                    if (IsEnvFlagEnabled("IMM_UNITY_VK_HOST_DEPTH"))
                    {
                        RenderTexture xrRt = GetXrEyeRenderTexture(cam, (int)StereoMode.TwoPass);
                        if (xrRt != null)
                            hostDepthPtr = xrRt.depthBuffer.GetNativeRenderBufferPtr();
                    }
                    ImmNativePlugin.SetVulkanCameraEyeRenderBuffers(
                        info.CameraId,
                        vulkanEye,
                        eyeTarget.colorBuffer.GetNativeRenderBufferPtr(),
                        hostDepthPtr,
                        eyeTarget.width,
                        eyeTarget.height,
                        1);
                    if (!_loggedVulkanRenderTargetSource.Contains(cam))
                    {
                        _loggedVulkanRenderTargetSource.Add(cam);
                        Debug.Log($"[IMM_UNITY_VK_RT_SRC_20260612] cam={cam.name} cameraId={info.CameraId} source=offscreenRT {eyeTarget.width}x{eyeTarget.height} pixel={cam.pixelWidth}x{cam.pixelHeight} samples=1");
                    }
                }
                else
                {
                    RenderTexture vulkanTargetTexture = cam.targetTexture != null ? cam.targetTexture : cam.activeTexture;
                    string vulkanTargetSource = vulkanTargetTexture != null ? "cameraTexture" : null;
                    if (vulkanTargetTexture == null)
                    {
                        // On Quest (XR + offscreen swapchain) Display.main resolves to a 1x1 dummy
                        // buffer - the real target is the XR eye texture for the current render pass.
                        vulkanTargetTexture = GetXrEyeRenderTexture(cam, stereoMode);
                        if (vulkanTargetTexture != null)
                            vulkanTargetSource = "xrEyePass";
                    }
                    RenderBuffer colorBuffer = vulkanTargetTexture != null ? vulkanTargetTexture.colorBuffer : Display.main.colorBuffer;
                    RenderBuffer depthBuffer = vulkanTargetTexture != null ? vulkanTargetTexture.depthBuffer : Display.main.depthBuffer;
                    int vulkanSampleCount = vulkanTargetTexture != null
                        ? Math.Max(1, vulkanTargetTexture.antiAliasing)
                        : (cam.allowMSAA ? Math.Max(1, QualitySettings.antiAliasing) : 1);
                    if (!_loggedVulkanRenderTargetSource.Contains(cam))
                    {
                        _loggedVulkanRenderTargetSource.Add(cam);
                        string source = vulkanTargetTexture != null ? $"{vulkanTargetSource} {vulkanTargetTexture.width}x{vulkanTargetTexture.height}" : "display";
                        Debug.Log($"[IMM_UNITY_VK_RT_SRC_20260612] cam={cam.name} cameraId={info.CameraId} source={source} pixel={cam.pixelWidth}x{cam.pixelHeight} samples={vulkanSampleCount}");
                    }
                    ImmNativePlugin.SetVulkanCameraRenderBuffers(
                        info.CameraId,
                        colorBuffer.GetNativeRenderBufferPtr(),
                        depthBuffer.GetNativeRenderBufferPtr(),
                        cam.pixelWidth,
                        cam.pixelHeight,
                        vulkanSampleCount);
                }
                // Offscreen path: the render event calls RenderCamera which prepares
                // internally. A managed PrepareCamera here runs on the MAIN thread and
                // races the render-thread event between its prepare and its eye render
                // (no native lock on Android) - the player's global head state can be
                // overwritten mid-eye, splitting the two eyes' views.
                if (!useOffscreenTargets)
                {
                    int prepared = IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_PREPARE")
                        ? 0
                        : ImmNativePlugin.PrepareCamera(info.CameraId);
                    if (prepared == 0 && _loggedVulkanPrepareWarning.Add(cam))
                    {
                        Debug.LogWarning($"[IMM_UNITY_VK_PREPARE_20260612] cam={cam.name} cameraId={info.CameraId} prepared=0");
                    }
                }
            }

            if (_useCameraCallbackRendering)
                return;

            int eyeIndex = 0;
            if (stereoMode == (int)StereoMode.TwoPass && cam.stereoEnabled)
            {
                eyeIndex = cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right ? 1 : 0;
            }

            int eventId = (info.CameraId << 8) | (eyeIndex & 0x1);
            info.CommandBuffer.Clear();
            if (IsVulkanRuntime())
            {
                if (!IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_MANAGED_CONFIG") && _configuredVulkanRenderEvents.Add(eventId))
                {
                    int configured = ImmNativePlugin.ConfigureVulkanRenderEvent(eventId);
                    Debug.Log($"[IMM_UNITY_VK_EVENTCFG_20260611] eventId={eventId} configured={configured}");
                }
                bool useCustomBlit = (useHostComposition || IsEnvFlagEnabled("IMM_UNITY_VK_USE_CUSTOM_BLIT")) &&
                    !IsEnvFlagEnabled("IMM_UNITY_VK_FORCE_PLAIN_EVENT");
                bool bindCameraTarget = !useHostComposition && !IsEnvFlagEnabled("IMM_UNITY_VK_SKIP_BIND_CAMERA_TARGET");
                var cameraTarget = new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
                if (bindCameraTarget)
                {
                    if (IsEnvFlagEnabled("IMM_UNITY_VK_BIND_CAMERA_DEPTH_TARGET"))
                    {
                        info.CommandBuffer.SetRenderTarget(cameraTarget, new RenderTargetIdentifier(BuiltinRenderTextureType.Depth));
                    }
                    else if (!UsesFlatAndroidVulkanPresenter(cam))
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
                if (Application.platform == RuntimePlatform.Android &&
                    !useHostComposition &&
                    !IsEnvFlagEnabled("IMM_UNITY_VK_NO_OFFSCREEN_TARGET"))
                {
                    // Probe: blit the LEFT RT into BOTH eyes. Right eye lights up -> the
                    // right RT was empty (native render side); still dark -> the right
                    // pass composite itself is broken (Unity side).
                    int blitEye = IsEnvFlagEnabled("IMM_UNITY_VK_BLIT_LEFT_RT_BOTH_EYES") ? 0 : (eyeIndex & 1);
                    // Composite path: the Blit is the validated default. The in-pass
                    // overlay quad (opt-in IMM_UNITY_VK_COMPOSITE_QUAD) measured NO
                    // win over the blit (27.8 vs 29.6 fps stereo, 2026-07-26) and
                    // showed intermittent per-eye oddness/flicker - the ~9ms
                    // composite cost is intrinsic fullscreen-alpha work, not blit
                    // pass-break overhead. Kept for future experiments only.
                    bool useQuad = IsEnvFlagEnabled("IMM_UNITY_VK_COMPOSITE_QUAD") &&
                                   !IsEnvFlagEnabled("IMM_UNITY_VK_NO_COMPOSITE_QUAD") &&
                                   EnsureCompositeQuad(cam, info) != null;
                    if (useQuad)
                    {
                        // In-pass composite: bind this pass's eye RT through the command
                        // buffer, which executes INSIDE the eye's pass - immune to the
                        // cull-both-then-render-both ordering that made a PreCull-time
                        // Shader.SetGlobalFloat eye index race (both eyes sampled the
                        // same RT - "vision feels odd").
                        RenderTexture quadTarget = info.VulkanEyeTargets[blitEye];
                        if (quadTarget != null)
                            info.CommandBuffer.SetGlobalTexture("_ImmEyeTex", quadTarget);
                        if (_preCullCount <= 12)
                            Debug.Log($"[IMM_UNITY_VK_QUADEYE] n={_preCullCount} eye={eyeIndex} quadEye={blitEye} viaCB=1");
                    }
                    else if (!UsesFlatAndroidVulkanPresenter(cam))
                    {
                        RenderTexture eyeTarget = info.VulkanEyeTargets[blitEye];
                        Material composite = GetVulkanCompositeMaterial();
                        if (eyeTarget != null && composite != null)
                        {
                            // The plugin event above renders IMM into the offscreen texture
                            // (fence-completed on IMM's own queue before the callback returns);
                            // composite it into the eye buffer inside Unity's own pass.
                            info.CommandBuffer.Blit(eyeTarget, cameraTarget, composite);
                            if (_vulkanCompositeLogCount < 8)
                            {
                                ++_vulkanCompositeLogCount;
                                Debug.Log(
                                    $"[IMM_UNITY_VK_COMPOSITE_20260802] frame={Time.frameCount} eye={eyeIndex} " +
                                    $"blitEye={blitEye} rt={eyeTarget.GetInstanceID()} " +
                                    $"shader={composite.shader.name} supported={composite.shader.isSupported} " +
                                    $"colorPtr=0x{eyeTarget.colorBuffer.GetNativeRenderBufferPtr().ToInt64():X}");
                            }
                        }
                    }
                }
                AppendVulkanOverlayFixtureDraw(info.CommandBuffer, cam);
            }
            else
            {
                info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId);
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
                    CameraEvent renderEvent = IsVulkanRuntime() ? GetVulkanCommandBufferEvent(cam) : CameraEvent.AfterImageEffectsOpaque;
                    if (IsVulkanRuntime())
                    {
                        Debug.Log($"[IMM_UNITY_VK_EVENT_20260612] cam={cam.name} cameraId={info.CameraId} renderEvent={renderEvent}");
                    }
                    cam.AddCommandBuffer(renderEvent, info.CommandBuffer);
                    EnsureFlatAndroidVulkanPresenter(cam);
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
