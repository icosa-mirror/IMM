using System.IO;
using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using UnityEngine.Networking;
using Type = System.Type;

namespace ImmPlayer
{
    /// <summary>
    /// Example component showing how to use IMM runtime features from C#.
    /// </summary>
    public class ImmFeatureExamples : MonoBehaviour
    {
        public enum LoadSource
        {
            FileSystem,       // Load from absolute path (Editor/Desktop only)
            StreamingAssets   // Load from StreamingAssets folder (works on Android)
        }

        private const string DiagPrefix = "[IMM_DIAG] ";
        private const string DocumentNameEnv = "IMM_UNITY_DOCUMENT_NAME";
        private const string DocumentNameArg = "-immDocumentName";

        private static string GetCommandLineValue(string key)
        {
            string[] arguments = System.Environment.GetCommandLineArgs();
            for (int index = 0; index + 1 < arguments.Length; ++index)
            {
                if (string.Equals(arguments[index], key, System.StringComparison.OrdinalIgnoreCase))
                    return arguments[index + 1];
            }
            return string.Empty;
        }

        private static bool IsEnvFlagEnabled(string name)
        {
            string value = System.Environment.GetEnvironmentVariable(name);
            return !string.IsNullOrEmpty(value) && value != "0";
        }

        private static bool ShouldRunPostLoadFeature(string featureName, string disableFlag)
        {
            if (IsEnvFlagEnabled("IMM_UNITY_DISABLE_FEATURE_POST_LOAD"))
            {
                Debug.Log($"{DiagPrefix}Skipping post-load {featureName}: IMM_UNITY_DISABLE_FEATURE_POST_LOAD=1");
                return false;
            }

            if (IsEnvFlagEnabled(disableFlag))
            {
                Debug.Log($"{DiagPrefix}Skipping post-load {featureName}: {disableFlag}=1");
                return false;
            }

            Debug.Log($"{DiagPrefix}Starting post-load {featureName}");
            return true;
        }

        [System.Serializable]
        public struct LayerListEntry
        {
            public int Index;
            public int Id;
            public ImmDocument.LayerType Type;
            public string Name;
            public string FullName;
        }

        [Header("Document")]
        [SerializeField] private LoadSource loadSource = LoadSource.StreamingAssets;
        [Tooltip("For FileSystem mode only - ignored when using StreamingAssets")]
        [SerializeField] private string directoryPath = "";
        [SerializeField, HideInInspector] private string selectedFileName = "";
        [SerializeField] private bool loadOnStart = true;
        [SerializeField] private bool autoPlay = true;
        [SerializeField] private Transform documentTransform;

        [Header("Background")]
        [SerializeField] private bool applyDocumentBackgroundColor = false;
        [SerializeField] private Camera backgroundCamera;

        [Header("Viewpoint")]
        [SerializeField] private bool applySpawnAreaToViewpoint = true;
        [SerializeField] private Transform spawnAreaTargetTransform;
        [SerializeField] private bool keepCurrentViewHeightForFloorAreas = true;
        [SerializeField] private bool constrainViewpointRotationToYawInXR = true;

        [Header("Layer Target")]
        [SerializeField, HideInInspector] private int selectedLayerIndex = -1;

        [Header("Layer Edits")]
        [SerializeField] private bool layerVisible = true;
        [SerializeField][Range(0f, 1f)] private float layerOpacity = 1.0f;
        [SerializeField] private Vector3 layerPosition = Vector3.zero;
        [SerializeField] private Vector3 layerEuler = Vector3.zero;
        [SerializeField] private float layerScale = 1.0f;

        [Header("Document Status (Read Only)")]
        public ImmDocument.DocumentStateInfo documentState;
        public ImmDocument.DocumentInfoFlags documentInfoFlags;
        public int chapterCount;
        public int currentChapter;
        public int targetChapter;
        public int layerCount;
        public int spawnAreaCount;
        public int activeSpawnAreaId;
        public int currentSpawnAreaIndex;
        public int targetSpawnAreaIndex;
        public Bounds documentBounds;

        [Header("Selected Layer Status (Read Only)")]
        public int selectedLayerId;
        public string selectedLayerName;
        public ImmDocument.LayerType selectedLayerType;
        public int selectedLayerParentId;
        public bool selectedLayerLoaded;
        public bool selectedLayerVisible;
        public float selectedLayerOpacity;
        public bool selectedLayerHasBounds;
        public Bounds selectedLayerBounds;
        public bool selectedLayerVisibilityOverrideEnabled;
        public bool selectedLayerVisibilityOverrideValue;

        public string currentFilePath;
        public ImmDocument.LayerInfo[] layers;
        public ImmDocument.SpawnAreaInfo[] spawnAreas;
        public LayerListEntry[] layerList;

        /// <summary>
        /// True once the native document can render and the sample's initial viewpoint has settled.
        /// </summary>
        public bool IsDocumentRenderReady =>
            _doc != null &&
            _doc.IsSequenceReady() &&
            _spawnAreaIds.Length > 0 &&
            currentSpawnAreaIndex >= 0 &&
            _initialSpawnAreaCoroutine == null &&
            _spawnAreaApplyCoroutine == null;

        private ImmDocument _doc;
        private bool _isApplyingEdits;
        private bool _isSyncingSelection;
        private int _lastSelectedLayerIndex = int.MinValue;
        private Coroutine _visibilityDiagCoroutine;
        private Coroutine _initialSpawnAreaCoroutine;
        private Coroutine _spawnAreaApplyCoroutine;
        private Coroutine _chapterSyncCoroutine;
        private bool _isDocumentTransformDirty;
        private Vector3 _lastLayerPosition;
        private Vector3 _lastLayerEuler;
        private float _lastLayerScale = 1.0f;
        private bool _lastLayerVisible;
        private float _lastLayerOpacity;
        private int[] _spawnAreaIds = new int[0];

        private void Start()
        {
            Debug.Log($"{DiagPrefix}ImmFeatureExamples Start()");
            string requestedDocumentName = GetCommandLineValue(DocumentNameArg);
            if (string.IsNullOrEmpty(requestedDocumentName))
            {
                requestedDocumentName = System.Environment.GetEnvironmentVariable(DocumentNameEnv);
            }
            if (!string.IsNullOrEmpty(requestedDocumentName))
            {
                selectedFileName = Path.GetFileName(requestedDocumentName);
                loadSource = LoadSource.StreamingAssets;
                Debug.Log($"{DiagPrefix}Validation document override: {selectedFileName}");
            }
            CacheLayerTransform();
            CacheLayerVisuals();
            if (loadOnStart)
            {
                LoadDocument();
            }
        }

        private void OnValidate()
        {
            if (!Application.isPlaying)
                return;
            if (_isApplyingEdits || _isSyncingSelection)
                return;

            if (selectedLayerIndex != _lastSelectedLayerIndex)
            {
                SyncLayerFieldsFromSelection();
                return;
            }

            if (HasLayerVisualChanged())
            {
                ApplyLayerEdits(false, applyTransform: false);
                CacheLayerVisuals();
            }

            if (HasLayerTransformChanged())
            {
                ApplyLayerEdits(false, applyVisibility: false, applyOpacity: false, applyTransform: true, logTransformChange: true);
                CacheLayerTransform();
                return;
            }
        }

        private void Update()
        {
            if (!Application.isPlaying || _doc == null)
                return;

            if (documentTransform != null && documentTransform.hasChanged)
            {
                ApplyDocumentTransform();
                documentTransform.hasChanged = false;
            }

            if (HasLayerVisualChanged())
            {
                ApplyLayerEdits(false, applyTransform: false);
                CacheLayerVisuals();
            }

            if (HasLayerTransformChanged())
            {
                ApplyLayerEdits(false, applyVisibility: false, applyOpacity: false, applyTransform: true, logTransformChange: true);
                CacheLayerTransform();
            }

            if (_isDocumentTransformDirty)
            {
                ApplyDocumentTransform();
                _isDocumentTransformDirty = false;
            }
        }

        public void LoadDocument()
        {
            StartCoroutine(LoadDocumentCoroutine());
        }

        public IEnumerator LoadStreamingAssetForValidation(string fileName)
        {
            if (string.IsNullOrEmpty(fileName))
                yield break;
            selectedFileName = Path.GetFileName(fileName);
            loadSource = LoadSource.StreamingAssets;
            yield return LoadDocumentCoroutine();
        }

        public bool PickRandomStreamingAssetsFile()
        {
            string dirPath = Application.streamingAssetsPath;
            if (!Directory.Exists(dirPath))
            {
                Debug.LogWarning($"{DiagPrefix}StreamingAssets folder not found: {dirPath}");
                return false;
            }

            string[] immFiles = Directory.GetFiles(dirPath, "*.imm");
            if (immFiles.Length == 0)
            {
                Debug.LogWarning($"{DiagPrefix}No .imm files found in StreamingAssets: {dirPath}");
                return false;
            }

            int fileIndex = Random.Range(0, immFiles.Length);
            selectedFileName = Path.GetFileName(immFiles[fileIndex]);
            Debug.Log($"{DiagPrefix}Selected random StreamingAssets file: {selectedFileName}");
            return true;
        }

        public bool PickRandomFileSystemFile()
        {
            if (string.IsNullOrEmpty(directoryPath))
            {
                Debug.LogWarning($"{DiagPrefix}Directory path is empty");
                return false;
            }

            if (!Directory.Exists(directoryPath))
            {
                Debug.LogWarning($"{DiagPrefix}Directory not found: {directoryPath}");
                return false;
            }

            string[] immFiles = Directory.GetFiles(directoryPath, "*.imm");
            if (immFiles.Length == 0)
            {
                Debug.LogWarning($"{DiagPrefix}No .imm files found in: {directoryPath}");
                return false;
            }

            int fileIndex = Random.Range(0, immFiles.Length);
            selectedFileName = Path.GetFileName(immFiles[fileIndex]);
            Debug.Log($"{DiagPrefix}Selected random file system file: {selectedFileName}");
            return true;
        }

        public void PickRandomAndLoad()
        {
            bool picked = loadSource == LoadSource.StreamingAssets
                ? PickRandomStreamingAssetsFile()
                : PickRandomFileSystemFile();

            if (picked)
                LoadDocument();
        }

        private IEnumerator LoadDocumentCoroutine()
        {
            if (_doc != null)
            {
                ImmPlayerManager.Instance.UnloadDocument(_doc);
                _doc = null;
            }

            if (string.IsNullOrEmpty(selectedFileName))
                yield break;

            if (loadSource == LoadSource.StreamingAssets)
            {
                yield return StartCoroutine(LoadFromStreamingAssets(selectedFileName));
            }
            else
            {
                yield return StartCoroutine(LoadFromFileSystem());
            }

            if (_doc == null)
                yield break;

            currentFilePath = loadSource == LoadSource.StreamingAssets
                ? Path.Combine(Application.streamingAssetsPath, selectedFileName)
                : Path.Combine(directoryPath, selectedFileName);

            _isDocumentTransformDirty = true;

            if (autoPlay)
            {
                _doc.Resume();
                _doc.Show();
            }
            else
            {
                _doc.Pause();
            }

            if (ShouldRunPostLoadFeature("layer refresh", "IMM_UNITY_DISABLE_LAYER_REFRESH"))
                StartCoroutine(WaitForSequenceAndRefreshLayers());

            if (ShouldRunPostLoadFeature("initial playback state", "IMM_UNITY_DISABLE_INITIAL_PLAYBACK_STATE"))
                StartCoroutine(ApplyInitialPlaybackState());

            if (ShouldRunPostLoadFeature("initial spawn area", "IMM_UNITY_DISABLE_INITIAL_SPAWN_AREA"))
                StartCoroutine(ApplyInitialSpawnAreaViewpoint());
        }

        private IEnumerator LoadFromStreamingAssets(string fileName)
        {
            string streamingPath = Path.Combine(Application.streamingAssetsPath, fileName);
            string requestPath = streamingPath.Contains("://")
                ? streamingPath
                : new System.Uri(streamingPath).AbsoluteUri;
            Debug.Log($"{DiagPrefix}Loading from StreamingAssets: {requestPath}");

            using (UnityWebRequest request = UnityWebRequest.Get(requestPath))
            {
                yield return request.SendWebRequest();

                if (request.result != UnityWebRequest.Result.Success)
                {
                    Debug.LogError($"{DiagPrefix}Failed to load from StreamingAssets: {request.error}");
                    Debug.LogError($"{DiagPrefix}  Path: {requestPath}");
                    yield break;
                }

                byte[] data = request.downloadHandler.data;
                Debug.Log($"{DiagPrefix}Loaded {data.Length} bytes from StreamingAssets");

                // Copy to persistentDataPath and load from there (avoids native plugin memory issues)
                string destPath = Path.Combine(Application.persistentDataPath, fileName);
                File.WriteAllBytes(destPath, data);
                Debug.Log($"{DiagPrefix}Copied to: {destPath}");

                yield return StartCoroutine(WaitForNativeDocumentLoadReady());
                _doc = ImmPlayerManager.Instance.LoadDocument(destPath);
            }
        }

        private IEnumerator LoadFromFileSystem()
        {
            if (string.IsNullOrEmpty(directoryPath))
            {
                Debug.LogError($"{DiagPrefix}Directory path is empty");
                yield break;
            }

            string path = Path.Combine(directoryPath, selectedFileName);
            if (!File.Exists(path))
            {
                Debug.LogError($"{DiagPrefix}File not found: {path}");
                yield break;
            }

            yield return StartCoroutine(WaitForNativeDocumentLoadReady());
            _doc = ImmPlayerManager.Instance.LoadDocument(path);
        }

        private IEnumerator WaitForNativeDocumentLoadReady()
        {
            const int maxFrames = 240;
            for (int frame = 0; frame < maxFrames; ++frame)
            {
                if (ImmPlayerManager.Instance.IsReadyForDocumentLoad)
                    yield break;

                yield return null;
            }

            // Android Unity completes IMM's GLES renderer from a render-thread
            // plugin event. If this fires, the camera did not deliver that event
            // within the startup window, so loading now would be a native crash.
            Debug.LogError($"{DiagPrefix}Native IMM renderer was not ready for document load after {maxFrames} frames");
        }

        public void UnloadDocument()
        {
            if (_doc == null)
                return;

            if (_initialSpawnAreaCoroutine != null)
            {
                StopCoroutine(_initialSpawnAreaCoroutine);
                _initialSpawnAreaCoroutine = null;
            }

            if (_spawnAreaApplyCoroutine != null)
            {
                StopCoroutine(_spawnAreaApplyCoroutine);
                _spawnAreaApplyCoroutine = null;
            }

            if (_chapterSyncCoroutine != null)
            {
                StopCoroutine(_chapterSyncCoroutine);
                _chapterSyncCoroutine = null;
            }

            ImmPlayerManager.Instance.UnloadDocument(_doc);
            _doc = null;
            currentFilePath = string.Empty;
        }

        #region Extended API coverage

        private const string ApiPrefix = "[IMM_E2E_API] ";

        [Header("Extended API Coverage (Read Only)")]
        public bool apiHasAudio;
        public ImmDocument.ChapterInfo apiChapterInfo;
        public long apiChapterTotalTicks;
        public long apiTimeSinceStartTicks;
        public long apiTimeSinceStopTicks;
        public bool apiIsPlaying;
        public long apiPausedAtTick;
        public long apiResumedAtTick;
        public string apiSpawnAreaSummary = "";
        public bool apiSpawnAreaPoseValid;
        public Vector3 apiSpawnAreaPosition;
        public bool apiSpawnAreaNeedsUpdateAfterSet;
        public bool apiSpawnAreaNeedsUpdateAfterConsume;
        public bool apiSpawnAreaThumbnailAvailable;
        public int apiSpawnAreaThumbnailWidth;
        public int apiSpawnAreaThumbnailHeight;
        public PerformanceInfoNative apiPerformanceInfo;
        public string apiLastAction = "";

        [Header("Extended API Coverage Settings")]
        [SerializeField] private int viewportCameraId = 0;
        [SerializeField] private float viewportOriginX = 0f;
        [SerializeField] private float viewportOriginY = 0f;

        /// <summary>
        /// Exercise the document-level wrappers the sample otherwise never reaches: audio
        /// presence, the full chapter layout, playback position, tick-accurate pause/resume,
        /// and the spawn-area volume, pose, needs-update and thumbnail queries. Everything it
        /// reads lands in the <c>api*</c> fields, so the Inspector shows the results.
        /// </summary>
        [ContextMenu("Extended API/Run Document Coverage")]
        public void RunDocumentApiCoverage()
        {
            if (_doc == null || !_doc.IsLoaded)
            {
                Debug.LogWarning($"{ApiPrefix}RunDocumentApiCoverage: no document loaded");
                return;
            }

            apiHasAudio = _doc.HasAudio();

            if (_doc.TryGetChapterInfo(out ImmDocument.ChapterInfo chapterInfo))
            {
                apiChapterInfo = chapterInfo;
                long totalTicks = 0;
                if (chapterInfo.Lengths != null)
                {
                    foreach (long length in chapterInfo.Lengths)
                        totalTicks += length;
                }

                apiChapterTotalTicks = totalTicks;
            }

            _doc.GetTime(out apiTimeSinceStartTicks, out apiTimeSinceStopTicks);
            apiIsPlaying = _doc.IsPlaying();

            // Schedule a pause one second ahead and immediately resume at the current tick, so
            // both deferred-playback entry points are exercised without stopping the sample.
            long playTicks = _doc.GetPlayTime();
            long oneSecond = ImmPlayer.Exporter.ExportLayerTiming.TicksPerSecond;
            _doc.PauseAt(playTicks + oneSecond);
            apiPausedAtTick = playTicks + oneSecond;
            _doc.ResumeAt(playTicks);
            apiResumedAtTick = playTicks;

            int spawnAreaId = _doc.GetActiveSpawnAreaId();
            if (spawnAreaId < 0 && _spawnAreaIds.Length > 0)
                spawnAreaId = _spawnAreaIds[0];

            if (spawnAreaId >= 0)
            {
                SerializedSpawnArea? spawnInfo = _doc.GetSpawnAreaInfo(spawnAreaId);
                if (spawnInfo.HasValue)
                {
                    SerializedSpawnArea info = spawnInfo.Value;
                    apiSpawnAreaSummary =
                        $"id={spawnAreaId} name={info.GetName()} type={info.mType} animated={info.mAnimated}";
                }

                if (_doc.TryGetSpawnAreaPose(spawnAreaId, out SpawnAreaPose pose))
                {
                    apiSpawnAreaPoseValid = true;
                    apiSpawnAreaPosition = pose.GetPosition();
                }

                _doc.SetSpawnAreaNeedsUpdate(true);
                apiSpawnAreaNeedsUpdateAfterSet = _doc.GetSpawnAreaNeedsUpdate();
                apiSpawnAreaNeedsUpdateAfterConsume = _doc.ConsumeSpawnAreaNeedsUpdate();

                if (_doc.TryGetSpawnAreaThumbnail(spawnAreaId, out Texture2D thumbnail))
                {
                    apiSpawnAreaThumbnailAvailable = true;
                    apiSpawnAreaThumbnailWidth = thumbnail.width;
                    apiSpawnAreaThumbnailHeight = thumbnail.height;
                    Destroy(thumbnail);
                }
            }

            apiLastAction = "RunDocumentApiCoverage";
            Debug.Log(
                $"{ApiPrefix}document hasAudio={apiHasAudio} chapters={apiChapterInfo.Count} " +
                $"chapterTicks={apiChapterTotalTicks} playMarkers={apiChapterInfo.HasPlays} " +
                $"timeSinceStart={apiTimeSinceStartTicks} isPlaying={apiIsPlaying} " +
                $"pausedAt={apiPausedAtTick} resumedAt={apiResumedAtTick} " +
                $"spawnArea=[{apiSpawnAreaSummary}] poseValid={apiSpawnAreaPoseValid} " +
                $"needsUpdateAfterSet={apiSpawnAreaNeedsUpdateAfterSet} " +
                $"needsUpdateAfterConsume={apiSpawnAreaNeedsUpdateAfterConsume} " +
                $"thumbnail={apiSpawnAreaThumbnailAvailable}" +
                (apiSpawnAreaThumbnailAvailable
                    ? $" {apiSpawnAreaThumbnailWidth}x{apiSpawnAreaThumbnailHeight}"
                    : string.Empty));
        }

        /// <summary>
        /// Exercise <see cref="ImmDocument.Continue"/> after a plain <see cref="ImmDocument.Pause"/>.
        /// </summary>
        [ContextMenu("Extended API/Pause Then Continue")]
        public void PauseThenContinue()
        {
            if (_doc == null || !_doc.IsLoaded)
            {
                Debug.LogWarning($"{ApiPrefix}PauseThenContinue: no document loaded");
                return;
            }

            _doc.Pause();
            bool pausedState = _doc.IsPlaying();
            _doc.Continue();
            apiLastAction = "Pause(); Continue()";
            Debug.Log($"{ApiPrefix}paused isPlaying={pausedState} afterContinue isPlaying={_doc.IsPlaying()}");
        }

        /// <summary>
        /// Override a camera's viewport origin with the player's explicit call, which the sample
        /// otherwise only reaches through its default render path.
        /// </summary>
        [ContextMenu("Extended API/Apply Camera Viewport Override")]
        public void ApplyCameraViewportOverride()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            if (manager == null)
            {
                Debug.LogWarning($"{ApiPrefix}ApplyCameraViewportOverride: no player manager");
                return;
            }

            manager.SetCameraViewport(viewportCameraId, viewportOriginX, viewportOriginY, 0, 0);
            apiLastAction = $"SetCameraViewport(camera={viewportCameraId}, x={viewportOriginX}, y={viewportOriginY})";
            Debug.Log($"{ApiPrefix}{apiLastAction}");
        }

        /// <summary>Drop the viewport override applied by <see cref="ApplyCameraViewportOverride"/>.</summary>
        [ContextMenu("Extended API/Clear Camera Viewport Override")]
        public void ClearCameraViewportOverride()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            if (manager == null)
            {
                Debug.LogWarning($"{ApiPrefix}ClearCameraViewportOverride: no player manager");
                return;
            }

            manager.ClearCameraViewport(viewportCameraId);
            apiLastAction = $"ClearCameraViewport(camera={viewportCameraId})";
            Debug.Log($"{ApiPrefix}{apiLastAction}");
        }

        /// <summary>Read the player's own per-frame counters for this document.</summary>
        [ContextMenu("Extended API/Log Player Performance Info")]
        public void LogPlayerPerformanceInfo()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            if (manager == null)
            {
                Debug.LogWarning($"{ApiPrefix}LogPlayerPerformanceInfo: no player manager");
                return;
            }

            manager.SetPerformanceMeasurementEnabled(true);
            apiPerformanceInfo = manager.GetPerformanceInfo();
            apiLastAction = "SetPerformanceMeasurementEnabled(true); GetPerformanceInfo()";
            Debug.Log(
                $"{ApiPrefix}performance cpuLoadMs={apiPerformanceInfo.cpuLoadTimeMS} " +
                $"drawCalls={apiPerformanceInfo.numDrawCalls} triangles={apiPerformanceInfo.numTriangles} " +
                $"gpuAverageMs={apiPerformanceInfo.gpuTimeAverageMs:F3}");
        }

        /// <summary>
        /// Abandon an in-flight load. Interesting only while a load is still running, so call it
        /// from the load path (or immediately after starting one) rather than after playback.
        /// </summary>
        [ContextMenu("Extended API/Cancel Pending Load")]
        public void CancelPendingLoad()
        {
            if (_doc == null)
            {
                Debug.LogWarning($"{ApiPrefix}CancelPendingLoad: no document");
                return;
            }

            _doc.CancelLoading();
            apiLastAction = "CancelLoading()";
            Debug.Log($"{ApiPrefix}{apiLastAction}");
        }

        /// <summary>Unload every document the player holds, not just the one this sample loaded.</summary>
        [ContextMenu("Extended API/Unload All Documents")]
        public void UnloadEveryDocument()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            if (manager == null)
            {
                Debug.LogWarning($"{ApiPrefix}UnloadEveryDocument: no player manager");
                return;
            }

            manager.UnloadAllDocuments(true);
            _doc = null;
            layers = new ImmDocument.LayerInfo[0];
            spawnAreas = new ImmDocument.SpawnAreaInfo[0];
            layerList = new LayerListEntry[0];
            _spawnAreaIds = new int[0];
            apiLastAction = "UnloadAllDocuments(synchronous: true)";
            Debug.Log($"{ApiPrefix}{apiLastAction}");
        }

        /// <summary>Shut the native player down, exercising the explicit teardown path.</summary>
        [ContextMenu("Extended API/Shutdown Player")]
        public void ShutdownPlayer()
        {
            ImmPlayerManager manager = ImmPlayerManager.Instance;
            if (manager == null)
            {
                Debug.LogWarning($"{ApiPrefix}ShutdownPlayer: no player manager");
                return;
            }

            manager.Shutdown();
            _doc = null;
            _spawnAreaIds = new int[0];
            apiLastAction = "Shutdown()";
            Debug.Log($"{ApiPrefix}{apiLastAction}");
        }

        #endregion

        public void RefreshStatus()
        {
            if (_doc == null)
                return;

            documentState = _doc.GetStateInfo();
            documentInfoFlags = _doc.GetInfoFlags();
            chapterCount = _doc.GetChapterCount();
            currentChapter = _doc.GetCurrentChapter();
            layerCount = _doc.GetLayerCount();
            spawnAreaCount = _doc.GetSpawnAreaCount();
            SyncSpawnAreaSelection();

            if (_doc.IsSequenceReady())
            {
                documentBounds = _doc.GetBoundingBox();
            }

            layers = _doc.GetLayersManaged();
            spawnAreas = _doc.GetSpawnAreas();
            RefreshLayerList();
        }

        public void NextSpawnArea()
        {
            SetSpawnAreaByOffset(1);
        }

        public void PreviousSpawnArea()
        {
            SetSpawnAreaByOffset(-1);
        }

        public void JumpToSpawnArea()
        {
            SetSpawnAreaByIndex(targetSpawnAreaIndex);
        }

        public bool TrySetSmokeSpawnArea(int spawnAreaIndex)
        {
            if (_doc == null || !_doc.IsLoaded)
                return false;

            SyncSpawnAreaSelection();
            if (_spawnAreaIds.Length == 0)
                return false;

            SetSpawnAreaByIndex(spawnAreaIndex);
            return true;
        }

        public void RefreshLayerList()
        {
            if (_doc == null || !_doc.IsSequenceReady())
            {
                layerList = new LayerListEntry[0];
                return;
            }

            var layers = _doc.GetLayersManaged();
            if (layers.Length == 0)
            {
                layerList = new LayerListEntry[0];
                return;
            }

            layerList = new LayerListEntry[layers.Length];
            for (int i = 0; i < layers.Length; i++)
            {
                layerList[i] = new LayerListEntry
                {
                    Index = i,
                    Id = layers[i].Id,
                    Type = layers[i].Type,
                    Name = layers[i].Name,
                    FullName = layers[i].FullName
                };
            }

            if (selectedLayerIndex >= layers.Length)
            {
                selectedLayerIndex = -1;
            }

            SyncLayerFieldsFromSelection();
        }

        private IEnumerator WaitForSequenceAndRefreshLayers()
        {
            while (_doc != null && _doc.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded)
                yield return null;

            if (_doc != null)
                RefreshLayerList();
        }

        private void ApplyDocumentBackgroundColor()
        {
            if (!string.IsNullOrEmpty(System.Environment.GetEnvironmentVariable("IMM_UNITY_SKIP_DOCUMENT_BACKGROUND_COLOR")))
                return;

            Camera cam = backgroundCamera != null ? backgroundCamera : Camera.main;
            if (cam == null)
                return;

            var info = ImmPlayerManager.Instance.GetPlayerInfo();
            cam.backgroundColor = info.BackgroundColor;
            cam.clearFlags = CameraClearFlags.SolidColor;
        }

        private IEnumerator ApplyInitialPlaybackState()
        {
            if (_doc == null)
                yield break;

            // Wait until the document is fully loaded to enforce play state.
            while (_doc != null)
            {
                var state = _doc.GetStateInfo();
                if (state.Loading == ImmDocument.LoadingState.Loaded)
                    break;
                yield return null;
            }

            if (_doc == null)
                yield break;

            if (applyDocumentBackgroundColor)
                ApplyDocumentBackgroundColor();

            if (autoPlay)
            {
                _doc.Resume();
                _doc.Show();
            }
            else
            {
                _doc.Pause();
            }
        }

        private IEnumerator ApplyInitialSpawnAreaViewpoint()
        {
            if (_initialSpawnAreaCoroutine != null)
            {
                StopCoroutine(_initialSpawnAreaCoroutine);
            }

            _initialSpawnAreaCoroutine = StartCoroutine(ApplyInitialSpawnAreaViewpointRoutine());
            yield return _initialSpawnAreaCoroutine;
            _initialSpawnAreaCoroutine = null;
        }

        private IEnumerator ApplyInitialSpawnAreaViewpointRoutine()
        {
            if (_doc == null)
                yield break;

            while (_doc != null && !_doc.IsSequenceReady())
                yield return null;

            if (_doc == null)
                yield break;

            const int maxFrames = 120;
            int frames = 0;
            while (_doc != null && frames < maxFrames)
            {
                SyncSpawnAreaSelection();
                if (_spawnAreaIds.Length == 0)
                {
                    yield return null;
                    frames++;
                    continue;
                }

                int initialSpawnId = _doc.GetInitialSpawnAreaId();
                int initialIndex = System.Array.IndexOf(_spawnAreaIds, initialSpawnId);
                int index = initialIndex >= 0
                    ? initialIndex
                    : (currentSpawnAreaIndex >= 0 ? currentSpawnAreaIndex : 0);
                SetSpawnAreaByIndex(index);
                yield break;
            }
        }

        public void Play()
        {
            _doc?.Resume();
            ImmNativePlugin.GlobalWork(1);
        }

        public void Pause()
        {
            _doc?.Pause();
            ImmNativePlugin.GlobalWork(1);
        }

        public void Restart()
        {
            _doc?.Restart();
        }

        public void SkipForward()
        {
            if (_doc == null)
                return;

            int count = _doc.GetChapterCount();
            if (count <= 0)
            {
                _doc.SkipForward();
                return;
            }

            int current = _doc.GetCurrentChapter();
            int next = (current + 1) % count;
            RequestChapterAndSync(next);
        }

        public void SkipBack()
        {
            if (_doc == null)
                return;

            int count = _doc.GetChapterCount();
            if (count <= 0)
            {
                _doc.SkipBack();
                return;
            }

            int current = _doc.GetCurrentChapter();
            int previous = (current - 1 + count) % count;
            RequestChapterAndSync(previous);
        }

        public void JumpToChapter()
        {
            if (_doc == null)
                return;

            if (targetChapter < 0)
                targetChapter = 0;

            RequestChapterAndSync(targetChapter);
        }

        private void RequestChapterAndSync(int chapterIndex)
        {
            if (_doc == null)
                return;

            int count = _doc.GetChapterCount();
            if (count <= 0)
                return;

            int clampedChapter = Mathf.Clamp(chapterIndex, 0, count - 1);
            if (_chapterSyncCoroutine != null)
            {
                StopCoroutine(_chapterSyncCoroutine);
            }

            _chapterSyncCoroutine = StartCoroutine(ApplyChapterAndSync(clampedChapter));
        }

        private IEnumerator ApplyChapterAndSync(int chapterIndex)
        {
            if (_doc == null)
                yield break;

            _doc.SetChapter(chapterIndex);

            const int maxFrames = 10;
            int frames = 0;
            while (_doc != null && frames < maxFrames)
            {
                int current = _doc.GetCurrentChapter();
                if (current == chapterIndex)
                    break;

                yield return null;
                frames++;
            }

            if (_doc == null)
                yield break;

            currentChapter = _doc.GetCurrentChapter();
            targetChapter = currentChapter;

            SyncSpawnAreaSelection();
            int activeSpawnId = _doc.GetActiveSpawnAreaId();
            if (activeSpawnId >= 0)
            {
                StartSpawnAreaViewpointApply(activeSpawnId);
            }

            _chapterSyncCoroutine = null;
        }

        public void ApplyLayerVisibility()
        {
            ApplyLayerEdits(true, applyOpacity: false, applyTransform: false);
        }

        public void ApplyLayerOpacity()
        {
            ApplyLayerEdits(true, applyVisibility: false, applyTransform: false);
        }

        public void ApplyLayerTransform()
        {
            ApplyLayerEdits(true, applyVisibility: false, applyOpacity: false, logTransformChange: true);
        }

        public void ClearLayerOverrides()
        {
            if (_doc == null || !_doc.IsSequenceReady())
                return;

            int layerId = ResolveLayerId();
            if (layerId < 0)
                return;

            LogLayerDiagnostics("before-clear", layerId);
            _doc.ClearLayerVisibilityOverride(layerId);
            _doc.ClearLayerTransformOverride(layerId);
            LogLayerDiagnostics("after-clear", layerId);
            RefreshSelectedLayerStatus();
        }

        private int ResolveLayerId()
        {
            if (_doc == null || !_doc.IsSequenceReady())
                return -1;

            if (selectedLayerIndex >= 0)
            {
                var info = _doc.GetLayerInfoManaged(selectedLayerIndex);
                if (info.HasValue)
                    return info.Value.Id;
            }

            return -1;
        }

        private void SetSpawnAreaByOffset(int offset)
        {
            if (_doc == null)
                return;

            SyncSpawnAreaSelection();
            if (_spawnAreaIds.Length == 0)
                return;

            int startIndex = currentSpawnAreaIndex >= 0 ? currentSpawnAreaIndex : 0;
            int count = _spawnAreaIds.Length;
            int targetIndex = (startIndex + offset) % count;
            if (targetIndex < 0)
                targetIndex += count;
            SetSpawnAreaByIndex(targetIndex);
        }

        private void SetSpawnAreaByIndex(int spawnAreaIndex)
        {
            if (_doc == null)
                return;

            SyncSpawnAreaSelection();
            if (_spawnAreaIds.Length == 0)
                return;

            int clampedIndex = Mathf.Clamp(spawnAreaIndex, 0, _spawnAreaIds.Length - 1);
            int spawnAreaId = _spawnAreaIds[clampedIndex];

            _doc.SetActiveSpawnAreaId(spawnAreaId);

            StartSpawnAreaViewpointApply(spawnAreaId);

            activeSpawnAreaId = spawnAreaId;
            currentSpawnAreaIndex = clampedIndex;
            targetSpawnAreaIndex = clampedIndex;
        }

        private void StartSpawnAreaViewpointApply(int spawnAreaId)
        {
            if (_spawnAreaApplyCoroutine != null)
            {
                StopCoroutine(_spawnAreaApplyCoroutine);
            }

            _spawnAreaApplyCoroutine = StartCoroutine(ApplySpawnAreaViewpointDeferred(spawnAreaId));
        }

        private IEnumerator ApplySpawnAreaViewpointDeferred(int requestedSpawnAreaId)
        {
            if (_doc == null)
                yield break;

            var info = _doc.GetSpawnAreaInfoManaged(requestedSpawnAreaId);
            int settleFrames = (info.HasValue && info.Value.Animated) ? 3 : 1;
            for (int i = 0; i < settleFrames; i++)
                yield return null;

            const int maxFrames = 10;
            int resolvedSpawnAreaId = requestedSpawnAreaId;
            for (int i = 0; i < maxFrames; i++)
            {
                if (_doc == null)
                    yield break;

                int activeSpawnAreaId = _doc.GetActiveSpawnAreaId();
                if (activeSpawnAreaId >= 0)
                {
                    resolvedSpawnAreaId = activeSpawnAreaId;
                }

                if (activeSpawnAreaId == requestedSpawnAreaId)
                {
                    break;
                }

                yield return null;
            }

            ApplySpawnAreaViewpoint(resolvedSpawnAreaId);
            _spawnAreaApplyCoroutine = null;
        }

        private void ApplySpawnAreaViewpoint(int spawnAreaId)
        {
            if (!applySpawnAreaToViewpoint || _doc == null)
                return;

            Transform target = ResolveSpawnAreaTargetTransform();
            if (target == null)
                return;

            Transform head = ResolveViewpointHeadTransform(target);
            Transform documentRoot = documentTransform != null ? documentTransform : transform;
            if (_doc.TryGetSpawnAreaViewTargetPose(
                spawnAreaId,
                documentRoot,
                target,
                head,
                keepCurrentViewHeightForFloorAreas,
                out Pose targetPose))
            {
                Pose finalPose = targetPose;
                if (constrainViewpointRotationToYawInXR && IsXrSettingsEnabled())
                {
                    Quaternion yawOnlyRotation = Quaternion.Euler(0.0f, targetPose.rotation.eulerAngles.y, 0.0f);
                    Vector3 headLocalPosition = target.InverseTransformPoint(head.position);
                    if (keepCurrentViewHeightForFloorAreas)
                        headLocalPosition.y = 0.0f;

                    Vector3 worldHeadAnchor = targetPose.position + (targetPose.rotation * headLocalPosition);
                    Vector3 yawOnlyPosition = worldHeadAnchor - (yawOnlyRotation * headLocalPosition);
                    finalPose = new Pose(yawOnlyPosition, yawOnlyRotation);
                }

                target.SetPositionAndRotation(finalPose.position, finalPose.rotation);
            }
        }

        private static bool IsXrSettingsEnabled()
        {
            Type xrSettingsType = Type.GetType("UnityEngine.XR.XRSettings, UnityEngine.XRModule");
            object value = xrSettingsType?.GetProperty("enabled")?.GetValue(null);
            return value is bool enabled && enabled;
        }

        private Transform ResolveSpawnAreaTargetTransform()
        {
            if (spawnAreaTargetTransform != null)
                return spawnAreaTargetTransform;

            Camera mainCamera = Camera.main;
            if (mainCamera == null)
                return null;

            Transform cameraTransform = mainCamera.transform;
            return cameraTransform.parent != null ? cameraTransform.parent : cameraTransform;
        }

        private Transform ResolveViewpointHeadTransform(Transform target)
        {
            Camera mainCamera = Camera.main;
            if (mainCamera == null)
                return target;
            return mainCamera.transform;
        }

        private void SyncSpawnAreaSelection()
        {
            if (_doc == null)
                return;

            _spawnAreaIds = _doc.GetSpawnAreaList();
            spawnAreaCount = _spawnAreaIds.Length;
            activeSpawnAreaId = _doc.GetActiveSpawnAreaId();
            currentSpawnAreaIndex = -1;

            for (int i = 0; i < _spawnAreaIds.Length; i++)
            {
                if (_spawnAreaIds[i] == activeSpawnAreaId)
                {
                    currentSpawnAreaIndex = i;
                    break;
                }
            }

            if (_spawnAreaIds.Length == 0)
            {
                targetSpawnAreaIndex = 0;
                return;
            }

            if (targetSpawnAreaIndex < 0 || targetSpawnAreaIndex >= _spawnAreaIds.Length)
            {
                targetSpawnAreaIndex = currentSpawnAreaIndex >= 0 ? currentSpawnAreaIndex : 0;
            }
        }

        private void ApplyDocumentTransform()
        {
            if (_doc == null || documentTransform == null)
                return;

            _doc.SetTransform(documentTransform);
        }

        private Matrix4x4 ComputeLayerLocalMatrix()
        {
            Matrix4x4 local = Matrix4x4.TRS(layerPosition, Quaternion.Euler(layerEuler), Vector3.one * layerScale);
            return local;
        }

        public void RefreshSelectedLayerStatus()
        {
            SyncLayerFieldsFromSelection();
        }

        private void SyncLayerFieldsFromSelection()
        {
            _lastSelectedLayerIndex = selectedLayerIndex;

            if (_doc == null || !_doc.IsSequenceReady())
                return;

            if (selectedLayerIndex < 0)
                return;

            var info = _doc.GetLayerInfoManaged(selectedLayerIndex);
            if (!info.HasValue)
                return;

            _isSyncingSelection = true;
            try
            {
                var li = info.Value;
                layerVisible = li.IsVisible;
                layerOpacity = li.Opacity;

                selectedLayerId = li.Id;
                selectedLayerName = li.Name;
                selectedLayerType = li.Type;
                selectedLayerParentId = li.ParentId;
                selectedLayerLoaded = li.IsLoaded;
                selectedLayerVisible = li.IsVisible;
                selectedLayerOpacity = li.Opacity;
                selectedLayerHasBounds = li.HasBounds;
                selectedLayerBounds = li.Bounds;

                var diag = _doc.GetLayerDiagnostics(li.Id);
                if (diag.HasValue)
                {
                    selectedLayerVisibilityOverrideEnabled = diag.Value.VisibilityOverrideEnabled;
                    selectedLayerVisibilityOverrideValue = diag.Value.VisibilityOverrideValue;
                }

                CacheLayerTransform();
                CacheLayerVisuals();
            }
            finally
            {
                _isSyncingSelection = false;
            }
        }

        private void ApplyLayerEdits(bool logDiagnostics, bool applyVisibility = true, bool applyOpacity = true, bool applyTransform = true, bool logTransformChange = false)
        {
            if (_doc == null)
                return;
            if (!_doc.IsSequenceReady())
                return;

            int layerId = ResolveLayerId();
            if (layerId < 0)
                return;

            _isApplyingEdits = true;
            try
            {
                if (logDiagnostics) LogLayerDiagnostics("before", layerId);

                if (applyVisibility)
                {
                    bool ok = _doc.SetLayerVisible(layerId, layerVisible);
                    if (!ok && logDiagnostics)
                    {
                        Debug.Log($"{DiagPrefix}apply: SetLayerVisible failed for layerId={layerId}");
                    }

                    if (logDiagnostics)
                    {
                        ScheduleVisibilityDiagnostics(layerId);
                    }

                    if (!logDiagnostics)
                    {
                        Debug.Log($"{DiagPrefix}apply: layerId={layerId} visible={layerVisible} ok={ok}");
                    }
                }

                if (applyOpacity)
                {
                    bool ok = _doc.SetLayerOpacity(layerId, layerOpacity);
                    if (!logDiagnostics)
                    {
                        Debug.Log($"{DiagPrefix}apply: layerId={layerId} opacity={layerOpacity} ok={ok}");
                    }
                }

                if (applyTransform)
                {
                    bool ok = _doc.SetLayerTransform(layerId, ComputeLayerLocalMatrix());
                    CacheLayerTransform();
                    if (logTransformChange)
                    {
                        string layerSummary = "unknown";
                        if (selectedLayerIndex >= 0)
                        {
                            var info = _doc.GetLayerInfoManaged(selectedLayerIndex);
                            if (info.HasValue)
                            {
                                var li = info.Value;
                                layerSummary = $"name={li.Name} type={li.Type} loaded={li.IsLoaded} bounds={li.HasBounds} children={li.NumChildren}";
                            }
                        }

                        Debug.Log($"{DiagPrefix}transform: layerId={layerId} {layerSummary} pos={layerPosition} rot={layerEuler} scale={layerScale}");
                        LogLayerDiagnostics("after-transform", layerId);
                    }
                    else
                    {
                        Debug.Log($"{DiagPrefix}apply: layerId={layerId} transform ok={ok}");
                    }
                }

                if (logDiagnostics) LogLayerDiagnostics("after", layerId);
            }
            finally
            {
                _isApplyingEdits = false;
            }
        }

        private void LogLayerDiagnostics(string phase, int layerId)
        {
            var diag = _doc.GetLayerDiagnostics(layerId);
            if (!diag.HasValue)
            {
                Debug.Log($"{DiagPrefix}{phase}: layerId={layerId} diag unavailable");
                return;
            }

            var d = diag.Value;
            Debug.Log($"{DiagPrefix}{phase}: layerId={layerId} visKeys={d.HasVisibilityKeys} opKeys={d.HasOpacityKeys} " +
                      $"vis={d.IsVisible} op={d.Opacity} worldVis={d.IsWorldVisible} worldOp={d.WorldOpacity} parentId={d.ParentId} " +
                      $"visOverride={d.VisibilityOverrideEnabled} visOverrideValue={d.VisibilityOverrideValue} " +
                      $"transformKeys={d.HasTransformKeys} transformOverride={d.TransformOverrideEnabled}");
        }

        private void ScheduleVisibilityDiagnostics(int layerId)
        {
            if (_visibilityDiagCoroutine != null)
            {
                StopCoroutine(_visibilityDiagCoroutine);
            }

            _visibilityDiagCoroutine = StartCoroutine(LogVisibilityDiagnosticsNextFrame(layerId));
        }

        private IEnumerator LogVisibilityDiagnosticsNextFrame(int layerId)
        {
            yield return new WaitForEndOfFrame();
            LogLayerDiagnostics("post-frame", layerId);
            LogParentVisibilityChain(layerId);
            _visibilityDiagCoroutine = null;
        }

        private void LogParentVisibilityChain(int layerId)
        {
            var visited = new HashSet<int>();
            int currentId = layerId;
            int depth = 0;

            while (currentId >= 0 && depth < 32 && visited.Add(currentId))
            {
                var diag = _doc.GetLayerDiagnostics(currentId);
                if (!diag.HasValue)
                {
                    Debug.Log($"{DiagPrefix}chain[{depth}]: layerId={currentId} diag unavailable");
                    break;
                }

                var d = diag.Value;
                Debug.Log($"{DiagPrefix}chain[{depth}]: layerId={currentId} vis={d.IsVisible} worldVis={d.IsWorldVisible} parentId={d.ParentId}");

                if (d.ParentId < 0 || d.ParentId == currentId)
                    break;

                currentId = d.ParentId;
                depth++;
            }
        }

        private bool HasLayerTransformChanged()
        {
            return layerPosition != _lastLayerPosition
                || layerEuler != _lastLayerEuler
                || Mathf.Abs(layerScale - _lastLayerScale) > 0.0001f;
        }

        private void CacheLayerTransform()
        {
            _lastLayerPosition = layerPosition;
            _lastLayerEuler = layerEuler;
            _lastLayerScale = layerScale;
        }

        private bool HasLayerVisualChanged()
        {
            return layerVisible != _lastLayerVisible
                || Mathf.Abs(layerOpacity - _lastLayerOpacity) > 0.0001f;
        }

        private void CacheLayerVisuals()
        {
            _lastLayerVisible = layerVisible;
            _lastLayerOpacity = layerOpacity;
        }
    }
}
