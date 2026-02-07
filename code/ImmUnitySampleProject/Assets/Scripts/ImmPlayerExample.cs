using System.Collections;
using System.IO;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Rendering;

namespace ImmPlayer
{
    /// <summary>
    /// Example component demonstrating how to use the IMM Player
    /// Attach this to a GameObject in your scene
    /// </summary>
    public class ImmPlayerExample : MonoBehaviour
    {
        public enum LoadSource
        {
            FileSystem,       // Load from absolute or relative file path (Editor/Desktop only)
            StreamingAssets   // Load from StreamingAssets folder (works on Android)
        }

        private const string LogPrefix = "[IMM] ";
        private static void Log(string message) => Debug.Log(LogPrefix + message);
        private static void LogWarning(string message) => Debug.LogWarning(LogPrefix + message);
        private static void LogError(string message) => Debug.LogError(LogPrefix + message);

        [Header("Document Settings")]
        [SerializeField] private LoadSource loadSource = LoadSource.StreamingAssets;
        [Tooltip("For StreamingAssets: filename only (e.g., 'myfile.imm'). For FileSystem: relative or absolute path.")]
        [SerializeField] private string documentPath = "";
        [SerializeField] private bool loadOnStart = false;

        [Header("Playback Settings")]
        [SerializeField] private bool autoPlay = true;
        [SerializeField][Range(0f, 1f)] private float volume = 1.0f;

        [Header("Camera Settings")]
        [SerializeField] private Camera targetCamera;
        [SerializeField] private int cameraId = 0;
        [SerializeField] private ImmPlayerManager.StereoMode stereoMode = ImmPlayerManager.StereoMode.Mono;

        [Header("Spawn Areas / Viewpoints")]
        [SerializeField] private bool applySpawnAreaToCamera = true;
        [SerializeField] private Transform spawnAreaTargetTransform;
        [SerializeField] private bool keepCurrentViewHeightForFloorAreas = true;

        private ImmDocument _currentDocument;
        private bool _useScriptableRenderPipeline;
        private int[] _spawnAreaIds = new int[0];
        private int _currentSpawnAreaIndex = -1;
        private Coroutine _initialSpawnAreaCoroutine;
        private Coroutine _chapterSyncCoroutine;
        private Coroutine _spawnAreaApplyCoroutine;

        private void Start()
        {
            _useScriptableRenderPipeline = GraphicsSettings.currentRenderPipeline != null;

            // Ensure we have a camera reference
            if (targetCamera == null)
            {
                targetCamera = Camera.main;
                if (targetCamera == null)
                {
                    LogError("No camera assigned and no main camera found");
                    return;
                }
            }

            // Wait for manager to initialize, then load document
            if (loadOnStart && !string.IsNullOrEmpty(documentPath))
            {
                // Ensure manager is initialized first
                var manager = ImmPlayerManager.Instance;
                if (manager != null)
                {
                    // Wait one frame to ensure manager's Start() has run
                    StartCoroutine(LoadDocumentAfterInit());
                }
            }
        }

        private void OnEnable()
        {
            if (_useScriptableRenderPipeline)
            {
                RenderPipelineManager.endCameraRendering += OnEndCameraRendering;
            }
        }

        private void OnDisable()
        {
            if (_useScriptableRenderPipeline)
            {
                RenderPipelineManager.endCameraRendering -= OnEndCameraRendering;
            }
        }

        private IEnumerator LoadDocumentAfterInit()
        {
            // Wait for end of frame to ensure ImmPlayerManager.Start() has completed
            yield return new WaitForEndOfFrame();

            if (!string.IsNullOrEmpty(documentPath))
            {
                yield return StartCoroutine(LoadDocumentCoroutine(documentPath));
            }
        }

        private void Update()
        {
            if (_currentDocument != null && _currentDocument.IsLoaded)
            {
                // Update camera matrices each frame
                ImmPlayerManager.Instance.SetCameraMatrices(cameraId, targetCamera, stereoMode);
            }
        }

        private void OnRenderObject()
        {
            if (_useScriptableRenderPipeline)
                return;

            if (ImmPlayerManager.Instance.UsesCommandBufferRendering)
                return;

            if (_currentDocument != null && _currentDocument.IsLoaded)
            {
                // Issue render event during rendering
                ImmPlayerManager.Instance.IssueRenderEvent(cameraId);
            }
        }

        private void OnEndCameraRendering(ScriptableRenderContext context, Camera camera)
        {
            if (!_useScriptableRenderPipeline)
                return;

            if (camera != targetCamera)
                return;

            if (_currentDocument != null && _currentDocument.IsLoaded)
            {
                ImmPlayerManager.Instance.SetCameraMatrices(cameraId, targetCamera, stereoMode);
                ImmPlayerManager.Instance.IssueRenderEvent(cameraId);
            }
        }

        #region Public Methods

        /// <summary>
        /// Load a document (starts coroutine internally)
        /// </summary>
        public void LoadDocument(string filePath)
        {
            StartCoroutine(LoadDocumentCoroutine(filePath));
        }

        /// <summary>
        /// Load a document from StreamingAssets or FileSystem based on loadSource setting
        /// </summary>
        private IEnumerator LoadDocumentCoroutine(string filePath)
        {
            // Unload existing document if any
            if (_currentDocument != null)
            {
                ImmPlayerManager.Instance.UnloadDocument(_currentDocument);
                _currentDocument = null;
            }

            if (loadSource == LoadSource.StreamingAssets)
            {
                yield return StartCoroutine(LoadFromStreamingAssets(filePath));
            }
            else
            {
                LoadFromFileSystem(filePath);
            }
        }

        /// <summary>
        /// Load from StreamingAssets using UnityWebRequest (works on Android)
        /// </summary>
        private IEnumerator LoadFromStreamingAssets(string fileName)
        {
            string streamingPath = Path.Combine(Application.streamingAssetsPath, fileName);
            Log($"Loading IMM file from StreamingAssets: {streamingPath}");

            using (UnityWebRequest request = UnityWebRequest.Get(streamingPath))
            {
                yield return request.SendWebRequest();

                if (request.result != UnityWebRequest.Result.Success)
                {
                    LogError($"Failed to load from StreamingAssets: {request.error}");
                    LogError($"  Path: {streamingPath}");
                    yield break;
                }

                byte[] data = request.downloadHandler.data;
                Log($"Loaded {data.Length} bytes from StreamingAssets");

                // Copy to persistentDataPath and load from there (avoids native plugin memory issues)
                string destPath = Path.Combine(Application.persistentDataPath, fileName);
                File.WriteAllBytes(destPath, data);
                Log($"Copied to: {destPath}");

                _currentDocument = ImmPlayerManager.Instance.LoadDocument(destPath);
                OnDocumentLoaded(fileName);
            }
        }

        /// <summary>
        /// Load from file system (Editor/Desktop only)
        /// </summary>
        private void LoadFromFileSystem(string filePath)
        {
            // Convert relative paths to absolute paths
            string absolutePath = filePath;
            if (!Path.IsPathRooted(filePath))
            {
                // Relative path - make it relative to Assets folder or project root
                string assetsPath = Path.Combine(Application.dataPath, filePath);
                string projectPath = Path.Combine(Application.dataPath, "..", filePath);

                if (File.Exists(assetsPath))
                {
                    absolutePath = assetsPath;
                }
                else if (File.Exists(projectPath))
                {
                    absolutePath = projectPath;
                }
                else
                {
                    LogError($"IMM file not found: {filePath}");
                    LogError($"  Tried: {assetsPath}");
                    LogError($"  Tried: {projectPath}");
                    return;
                }
            }

            Log($"Loading IMM file from FileSystem: {absolutePath}");

            // Load new document
            _currentDocument = ImmPlayerManager.Instance.LoadDocument(absolutePath);
            OnDocumentLoaded(filePath);
        }

        /// <summary>
        /// Called after document is loaded from any source
        /// </summary>
        private void OnDocumentLoaded(string filePath)
        {
            if (_currentDocument != null)
            {
                Log($"Loaded document: {filePath}");

                // Apply settings
                _currentDocument.SetVolume(volume);

                // Auto-play if enabled
                if (autoPlay)
                {
                    _currentDocument.Resume();
                    _currentDocument.Show();
                }

                // Position the document at this GameObject's transform
                _currentDocument.SetTransform(transform);
                StartInitialSpawnAreaRoutine();
            }
        }

        /// <summary>
        /// Unload the current document
        /// </summary>
        public void UnloadDocument()
        {
            if (_currentDocument != null)
            {
                if (_initialSpawnAreaCoroutine != null)
                {
                    StopCoroutine(_initialSpawnAreaCoroutine);
                    _initialSpawnAreaCoroutine = null;
                }

                if (_chapterSyncCoroutine != null)
                {
                    StopCoroutine(_chapterSyncCoroutine);
                    _chapterSyncCoroutine = null;
                }

                if (_spawnAreaApplyCoroutine != null)
                {
                    StopCoroutine(_spawnAreaApplyCoroutine);
                    _spawnAreaApplyCoroutine = null;
                }

                ImmPlayerManager.Instance.UnloadDocument(_currentDocument);
                _currentDocument = null;
                Log("Document unloaded");
            }
        }

        /// <summary>
        /// Play/resume playback
        /// </summary>
        public void Play()
        {
            if (_currentDocument != null)
            {
                _currentDocument.Resume();
                _currentDocument.Show();
            }
        }

        /// <summary>
        /// Pause playback
        /// </summary>
        public void Pause()
        {
            _currentDocument?.Pause();
        }

        /// <summary>
        /// Stop playback (pause and hide)
        /// </summary>
        public void Stop()
        {
            if (_currentDocument != null)
            {
                _currentDocument.Pause();
                _currentDocument.Hide();
            }
        }

        /// <summary>
        /// Restart from the beginning
        /// </summary>
        public void Restart()
        {
            _currentDocument?.Restart();
        }

        /// <summary>
        /// Skip to next chapter
        /// </summary>
        public void NextChapter()
        {
            if (_currentDocument == null)
                return;

            int count = _currentDocument.GetChapterCount();
            if (count <= 0)
            {
                _currentDocument.SkipForward();
                return;
            }

            int current = _currentDocument.GetCurrentChapter();
            int next = (current + 1) % count;
            RequestChapterAndSync(next);
        }

        /// <summary>
        /// Skip to previous chapter
        /// </summary>
        public void PreviousChapter()
        {
            if (_currentDocument == null)
                return;

            int count = _currentDocument.GetChapterCount();
            if (count <= 0)
            {
                _currentDocument.SkipBack();
                return;
            }

            int current = _currentDocument.GetCurrentChapter();
            int previous = (current - 1 + count) % count;
            RequestChapterAndSync(previous);
        }

        /// <summary>
        /// Jump to a specific chapter index.
        /// </summary>
        public void SetChapter(int chapterIndex)
        {
            if (chapterIndex < 0)
                return;
            RequestChapterAndSync(chapterIndex);
        }

        private void RequestChapterAndSync(int chapterIndex)
        {
            if (_currentDocument == null || !_currentDocument.IsLoaded)
                return;

            int count = _currentDocument.GetChapterCount();
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
            if (_currentDocument == null)
                yield break;

            _currentDocument.SetChapter(chapterIndex);

            const int maxFrames = 10;
            int frames = 0;
            while (_currentDocument != null && frames < maxFrames)
            {
                int current = _currentDocument.GetCurrentChapter();
                if (current == chapterIndex)
                    break;

                yield return null;
                frames++;
            }

            if (_currentDocument == null)
                yield break;

            SyncSpawnAreaSelection();
            int activeSpawnId = _currentDocument.GetActiveSpawnAreaId();
            if (activeSpawnId >= 0)
            {
                StartSpawnAreaViewpointApply(activeSpawnId);
            }

            _chapterSyncCoroutine = null;
        }

        /// <summary>
        /// Skip to next spawn area/viewpoint.
        /// </summary>
        public void NextSpawnArea()
        {
            SetSpawnAreaByOffset(1);
        }

        /// <summary>
        /// Skip to previous spawn area/viewpoint.
        /// </summary>
        public void PreviousSpawnArea()
        {
            SetSpawnAreaByOffset(-1);
        }

        /// <summary>
        /// Jump to a specific spawn area index.
        /// </summary>
        public void SetSpawnArea(int spawnAreaIndex)
        {
            SetSpawnAreaByIndex(spawnAreaIndex);
        }

        /// <summary>
        /// Set playback volume
        /// </summary>
        public void SetVolume(float vol)
        {
            volume = Mathf.Clamp01(vol);
            _currentDocument?.SetVolume(volume);
        }

        #endregion

        #region Inspector Methods (for testing)

        [ContextMenu("Load Document")]
        private void LoadDocumentMenu()
        {
            if (!string.IsNullOrEmpty(documentPath))
            {
                LoadDocument(documentPath);
            }
            else
            {
                LogWarning("Document path is empty");
            }
        }

        [ContextMenu("Unload Document")]
        private void UnloadDocumentMenu()
        {
            UnloadDocument();
        }

        [ContextMenu("Play")]
        private void PlayMenu()
        {
            Play();
        }

        [ContextMenu("Pause")]
        private void PauseMenu()
        {
            Pause();
        }

        [ContextMenu("Restart")]
        private void RestartMenu()
        {
            Restart();
        }

        [ContextMenu("Print Document Info")]
        private void PrintDocumentInfo()
        {
            if (_currentDocument == null || !_currentDocument.IsLoaded)
            {
                Log("No document loaded");
                return;
            }

            Log($"Document ID: {_currentDocument.DocumentId}");
            Log($"File Name: {_currentDocument.FileName}");
            Log($"Chapter Count: {_currentDocument.GetChapterCount()}");
            Log($"Current Chapter: {_currentDocument.GetCurrentChapter()}");
            Log($"Play Time: {_currentDocument.GetPlayTime()}");
            Log($"Volume: {_currentDocument.GetVolume()}");
            Log($"Bounding Box: {_currentDocument.GetBoundingBox()}");
            Log($"Spawn Area Count: {_currentDocument.GetSpawnAreaCount()}");
        }

        [ContextMenu("Dump Layers")]
        private void DumpLayers()
        {
            if (_currentDocument == null || !_currentDocument.IsLoaded)
            {
                Log("No document loaded");
                return;
            }

            var state = _currentDocument.GetState();
            Log($"Dump Layers: loadingState={state.loadingState} playbackState={state.playbackState}");
            if (!ImmNativePlugin.IsSequenceReady(_currentDocument.DocumentId))
            {
                LogWarning("Dump Layers aborted: sequence not ready yet");
                return;
            }

            int count = _currentDocument.GetLayerCount();
            Log($"Layer count: {count}");
            for (int i = 0; i < count; i++)
            {
                LayerInfoNative? info = _currentDocument.GetLayerInfo(i);
                if (info == null)
                {
                    LogWarning($"Layer {i}: no info");
                    continue;
                }

                LayerInfoNative layer = info.Value;
                Log($"Layer {i}: id={layer.id} type={layer.type} parent={layer.parentId} name={layer.name} full={layer.fullName} loaded={layer.isLoaded} vis={layer.isVisible} draws={layer.paintNumDrawings} frames={layer.paintNumFrames} strokes={layer.paintNumStrokes}");
            }
        }

        [ContextMenu("Next Spawn Area")]
        private void NextSpawnAreaMenu()
        {
            NextSpawnArea();
        }

        [ContextMenu("Previous Spawn Area")]
        private void PreviousSpawnAreaMenu()
        {
            PreviousSpawnArea();
        }

        #endregion

        private void SetSpawnAreaByOffset(int offset)
        {
            if (_currentDocument == null || !_currentDocument.IsLoaded)
                return;

            SyncSpawnAreaSelection();
            if (_spawnAreaIds.Length == 0)
                return;

            int startIndex = _currentSpawnAreaIndex >= 0 ? _currentSpawnAreaIndex : 0;
            int count = _spawnAreaIds.Length;
            int targetIndex = (startIndex + offset) % count;
            if (targetIndex < 0)
                targetIndex += count;
            SetSpawnAreaByIndex(targetIndex);
        }

        private void SetSpawnAreaByIndex(int spawnAreaIndex)
        {
            if (_currentDocument == null || !_currentDocument.IsLoaded)
                return;

            SyncSpawnAreaSelection();
            if (_spawnAreaIds.Length == 0)
                return;

            int clampedIndex = Mathf.Clamp(spawnAreaIndex, 0, _spawnAreaIds.Length - 1);
            int spawnAreaId = _spawnAreaIds[clampedIndex];

            _currentDocument.SetActiveSpawnAreaId(spawnAreaId);

            _currentSpawnAreaIndex = clampedIndex;
            StartSpawnAreaViewpointApply(spawnAreaId);
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
            if (_currentDocument == null)
                yield break;

            var info = _currentDocument.GetSpawnAreaInfoManaged(requestedSpawnAreaId);
            int settleFrames = (info.HasValue && info.Value.Animated) ? 3 : 1;
            for (int i = 0; i < settleFrames; i++)
                yield return null;

            const int maxFrames = 10;
            int resolvedSpawnAreaId = requestedSpawnAreaId;
            for (int i = 0; i < maxFrames; i++)
            {
                if (_currentDocument == null)
                    yield break;

                int activeSpawnAreaId = _currentDocument.GetActiveSpawnAreaId();
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

        private void SyncSpawnAreaSelection()
        {
            if (_currentDocument == null || !_currentDocument.IsLoaded)
            {
                _spawnAreaIds = new int[0];
                _currentSpawnAreaIndex = -1;
                return;
            }

            _spawnAreaIds = _currentDocument.GetSpawnAreaList();
            int activeSpawnAreaId = _currentDocument.GetActiveSpawnAreaId();
            _currentSpawnAreaIndex = -1;

            for (int i = 0; i < _spawnAreaIds.Length; i++)
            {
                if (_spawnAreaIds[i] == activeSpawnAreaId)
                {
                    _currentSpawnAreaIndex = i;
                    break;
                }
            }
        }

        private void ApplySpawnAreaViewpoint(int spawnAreaId)
        {
            if (!applySpawnAreaToCamera || _currentDocument == null)
                return;

            Transform target = ResolveSpawnAreaTargetTransform();
            if (target == null)
                return;

            Transform head = ResolveViewpointHeadTransform(target);
            if (_currentDocument.TryGetSpawnAreaViewTargetPose(
                spawnAreaId,
                transform,
                target,
                head,
                keepCurrentViewHeightForFloorAreas,
                out Pose targetPose))
            {
                target.SetPositionAndRotation(targetPose.position, targetPose.rotation);
            }
        }

        private Transform ResolveSpawnAreaTargetTransform()
        {
            if (spawnAreaTargetTransform != null)
                return spawnAreaTargetTransform;

            if (targetCamera != null)
            {
                Transform cam = targetCamera.transform;
                return cam.parent != null ? cam.parent : cam;
            }

            Camera main = Camera.main;
            if (main == null)
                return null;

            Transform mainCam = main.transform;
            return mainCam.parent != null ? mainCam.parent : mainCam;
        }

        private Transform ResolveViewpointHeadTransform(Transform target)
        {
            if (targetCamera != null)
                return targetCamera.transform;

            Camera main = Camera.main;
            if (main == null)
                return target;

            return main.transform;
        }

        private void StartInitialSpawnAreaRoutine()
        {
            if (_initialSpawnAreaCoroutine != null)
            {
                StopCoroutine(_initialSpawnAreaCoroutine);
            }

            _initialSpawnAreaCoroutine = StartCoroutine(ApplyInitialSpawnAreaViewpointWhenReady());
        }

        private IEnumerator ApplyInitialSpawnAreaViewpointWhenReady()
        {
            if (_currentDocument == null)
                yield break;

            while (_currentDocument != null && !_currentDocument.IsSequenceReady())
            {
                yield return null;
            }

            if (_currentDocument == null)
                yield break;

            const int maxFrames = 120;
            for (int frame = 0; frame < maxFrames && _currentDocument != null; frame++)
            {
                SyncSpawnAreaSelection();
                if (_spawnAreaIds.Length == 0)
                {
                    yield return null;
                    continue;
                }

                int initialSpawnId = _currentDocument.GetInitialSpawnAreaId();
                int initialIndex = System.Array.IndexOf(_spawnAreaIds, initialSpawnId);
                int index = initialIndex >= 0
                    ? initialIndex
                    : (_currentSpawnAreaIndex >= 0 ? _currentSpawnAreaIndex : 0);
                SetSpawnAreaByIndex(index);
                _initialSpawnAreaCoroutine = null;
                yield break;
            }

            _initialSpawnAreaCoroutine = null;
        }

        private void OnDestroy()
        {
            UnloadDocument();
        }
    }
}

