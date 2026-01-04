using System.IO;
using UnityEngine;
using System.Collections;
using System.Collections.Generic;

namespace ImmPlayer
{
    /// <summary>
    /// Example component showing how to use IMM runtime features from C#.
    /// </summary>
    public class ImmFeatureExamples : MonoBehaviour
    {
        private const string DiagPrefix = "[IMM_DIAG] ";
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
        [SerializeField] private string documentPath = "";
        [SerializeField] private bool loadOnStart = true;
        [SerializeField] private bool autoPlay = true;
        [SerializeField] private Transform documentTransform;

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
        public int layerCount;
        public int spawnAreaCount;
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

        public ImmDocument.LayerInfo[] layers;
        public ImmDocument.SpawnAreaInfo[] spawnAreas;
        public LayerListEntry[] layerList;

        private ImmDocument _doc;
        private bool _isApplyingEdits;
        private bool _isSyncingSelection;
        private int _lastSelectedLayerIndex = int.MinValue;
        private Coroutine _visibilityDiagCoroutine;
        private bool _isDocumentTransformDirty;
        private Vector3 _lastLayerPosition;
        private Vector3 _lastLayerEuler;
        private float _lastLayerScale = 1.0f;
        private bool _lastLayerVisible;
        private float _lastLayerOpacity;

        private void Start()
        {
            Debug.Log($"{DiagPrefix}ImmFeatureExamples Start()");
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
            if (_doc != null)
            {
                ImmPlayerManager.Instance.UnloadDocument(_doc);
                _doc = null;
            }

            if (string.IsNullOrEmpty(documentPath))
                return;

            string path = ResolvePath(documentPath);
            if (string.IsNullOrEmpty(path))
                return;

            _doc = ImmPlayerManager.Instance.LoadDocument(path);
            if (_doc == null)
                return;

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

            StartCoroutine(WaitForSequenceAndRefreshLayers());
            StartCoroutine(ApplyInitialPlaybackState());
        }

        public void UnloadDocument()
        {
            if (_doc == null)
                return;

            ImmPlayerManager.Instance.UnloadDocument(_doc);
            _doc = null;
        }

        public void RefreshStatus()
        {
            if (_doc == null)
                return;

            documentState = _doc.GetStateInfo();
            documentInfoFlags = _doc.GetInfoFlags();
            chapterCount = _doc.GetChapterCount();
            layerCount = _doc.GetLayerCount();
            spawnAreaCount = _doc.GetSpawnAreaCount();

            if (_doc.IsSequenceReady())
            {
                documentBounds = _doc.GetBoundingBox();
            }

            layers = _doc.GetLayersManaged();
            spawnAreas = _doc.GetSpawnAreas();
            RefreshLayerList();
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
            while (_doc != null && !_doc.IsSequenceReady())
                yield return null;

            if (_doc != null)
                RefreshLayerList();
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
            _doc?.SkipForward();
        }

        public void SkipBack()
        {
            _doc?.SkipBack();
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

        private static string ResolvePath(string path)
        {
            if (Path.IsPathRooted(path))
                return path;

            string assetsPath = Path.Combine(Application.dataPath, path);
            if (File.Exists(assetsPath))
                return assetsPath;

            string projectPath = Path.Combine(Application.dataPath, "..", path);
            if (File.Exists(projectPath))
                return projectPath;

            return null;
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
