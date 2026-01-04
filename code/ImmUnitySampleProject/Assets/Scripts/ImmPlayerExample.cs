using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer
{
    /// <summary>
    /// Example component demonstrating how to use the IMM Player
    /// Attach this to a GameObject in your scene
    /// </summary>
    public class ImmPlayerExample : MonoBehaviour
    {
        
        private const string LogPrefix = "[IMM] ";
        private static void Log(string message) => Debug.Log(LogPrefix + message);
        private static void LogWarning(string message) => Debug.LogWarning(LogPrefix + message);
        private static void LogError(string message) => Debug.LogError(LogPrefix + message);
[Header("Document Settings")]
        [SerializeField] private string documentPath = "";
        [SerializeField] private bool loadOnStart = false;

        [Header("Playback Settings")]
        [SerializeField] private bool autoPlay = true;
        [SerializeField][Range(0f, 1f)] private float volume = 1.0f;

        [Header("Camera Settings")]
        [SerializeField] private Camera targetCamera;
        [SerializeField] private int cameraId = 0;
        [SerializeField] private ImmPlayerManager.StereoMode stereoMode = ImmPlayerManager.StereoMode.Mono;

        private ImmDocument _currentDocument;
        private bool _useScriptableRenderPipeline;

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

        private System.Collections.IEnumerator LoadDocumentAfterInit()
        {
            // Wait for end of frame to ensure ImmPlayerManager.Start() has completed
            yield return new WaitForEndOfFrame();

            if (!string.IsNullOrEmpty(documentPath))
            {
                LoadDocument(documentPath);
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
        /// Load a document from a file path
        /// </summary>
        public void LoadDocument(string filePath)
        {
            // Unload existing document if any
            if (_currentDocument != null)
            {
                ImmPlayerManager.Instance.UnloadDocument(_currentDocument);
                _currentDocument = null;
            }

            // Convert relative paths to absolute paths
            string absolutePath = filePath;
            if (!System.IO.Path.IsPathRooted(filePath))
            {
                // Relative path - make it relative to Assets folder or project root
                string assetsPath = System.IO.Path.Combine(Application.dataPath, filePath);
                string projectPath = System.IO.Path.Combine(Application.dataPath, "..", filePath);

                if (System.IO.File.Exists(assetsPath))
                {
                    absolutePath = assetsPath;
                }
                else if (System.IO.File.Exists(projectPath))
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

            Log($"Loading IMM file from: {absolutePath}");

            // Load new document
            _currentDocument = ImmPlayerManager.Instance.LoadDocument(absolutePath);

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

            }
        }

        /// <summary>
        /// Unload the current document
        /// </summary>
        public void UnloadDocument()
        {
            if (_currentDocument != null)
            {
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
            _currentDocument?.SkipForward();
        }

        /// <summary>
        /// Skip to previous chapter
        /// </summary>
        public void PreviousChapter()
        {
            _currentDocument?.SkipBack();
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

        #endregion

        private void OnDestroy()
        {
            UnloadDocument();
        }
    }
}

