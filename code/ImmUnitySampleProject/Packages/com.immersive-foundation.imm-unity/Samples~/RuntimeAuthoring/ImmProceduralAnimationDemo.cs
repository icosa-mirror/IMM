using System.Collections;
using System.IO;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer.Samples
{
    /// <summary>
    /// Generates an animated IMM ribbon through the runtime authoring API and
    /// previews it directly from memory. File export remains an explicit option.
    /// </summary>
    [RequireComponent(typeof(ImmAuthoringPreviewCoordinator))]
    public sealed class ImmProceduralAnimationDemo : MonoBehaviour
    {
        private const string LogPrefix = "[IMM_AUTHOR_VISUAL_DEMO]";
        private const int CameraId = 0;

        [Header("Run")]
        [SerializeField] private bool generateOnStart = true;
        [SerializeField] private Camera targetCamera;
        [SerializeField] private string outputFileName = "procedural-ribbon.imm";
        [SerializeField, Min(1f)] private float loadTimeoutSeconds = 30f;

        [Header("Animation")]
        [SerializeField, Min(1)] private int frameRate = 30;
        [SerializeField, Min(2)] private int frameCount = 90;
        [SerializeField, Min(1)] private int strandCount = 5;
        [SerializeField, Min(3)] private int pointsPerStrand = 64;
        [SerializeField, Min(0.25f)] private float turns = 2.5f;
        [SerializeField, Min(0.01f)] private float radius = 0.65f;
        [SerializeField, Min(0.001f)] private float strokeWidth = 0.035f;

        [Header("Runtime Status")]
        [SerializeField, TextArea(2, 5)] private string status = "Not generated";
        [SerializeField] private string generatedFilePath;
        [SerializeField] private long generatedRevision;
        [SerializeField] private long generatedBytes;

        private ImmDocument _loadedDocument;
        private ImmAuthoringPreviewCoordinator _previewCoordinator;
        private Coroutine _operation;
        private bool _useScriptableRenderPipeline;

        public string Status => status;
        public string GeneratedFilePath => generatedFilePath;
        public long GeneratedRevision => generatedRevision;
        public long GeneratedBytes => generatedBytes;

        private void Awake()
        {
            _previewCoordinator = GetComponent<ImmAuthoringPreviewCoordinator>();
            if (_previewCoordinator == null)
                _previewCoordinator = gameObject.AddComponent<ImmAuthoringPreviewCoordinator>();
            _previewCoordinator.PlayerLoadTimeoutSeconds = loadTimeoutSeconds;
            _useScriptableRenderPipeline = GraphicsSettings.currentRenderPipeline != null;
            if (targetCamera == null)
                targetCamera = Camera.main;
        }

        private void OnEnable()
        {
            if (_useScriptableRenderPipeline)
                RenderPipelineManager.endCameraRendering += OnEndCameraRendering;
        }

        private void Start()
        {
            if (generateOnStart)
                GenerateAndPlay();
        }

        private void Update()
        {
            if (_loadedDocument != null && _loadedDocument.IsLoaded && targetCamera != null)
            {
                ImmPlayerManager.Instance.SetCameraMatrices(
                    CameraId,
                    targetCamera,
                    ImmPlayerManager.StereoMode.Mono);
            }
        }

        private void OnRenderObject()
        {
            if (_useScriptableRenderPipeline ||
                _loadedDocument == null ||
                !_loadedDocument.IsLoaded ||
                ImmPlayerManager.Instance.UsesCommandBufferRendering)
            {
                return;
            }

            ImmPlayerManager.Instance.IssueRenderEvent(CameraId);
        }

        private void OnEndCameraRendering(ScriptableRenderContext context, Camera camera)
        {
            if (!_useScriptableRenderPipeline ||
                camera != targetCamera ||
                _loadedDocument == null ||
                !_loadedDocument.IsLoaded)
            {
                return;
            }

            ImmPlayerManager.Instance.SetCameraMatrices(
                CameraId,
                targetCamera,
                ImmPlayerManager.StereoMode.Mono);
            ImmPlayerManager.Instance.IssueRenderEvent(CameraId);
        }

        private void OnDisable()
        {
            if (_useScriptableRenderPipeline)
                RenderPipelineManager.endCameraRendering -= OnEndCameraRendering;
        }

        private void OnDestroy()
        {
            Unload();
        }

        [ContextMenu("Generate and Play")]
        public void GenerateAndPlay()
        {
            StartOperation(false, true);
        }

        [ContextMenu("Export IMM Only")]
        public void ExportOnly()
        {
            StartOperation(true, false);
        }

        [ContextMenu("Unload Generated IMM")]
        public void Unload()
        {
            _previewCoordinator?.ClearPreview();
            _loadedDocument = null;
            SetStatus("Generated IMM unloaded");
        }

        private void StartOperation(bool exportToFile, bool loadAfterBuild)
        {
            if (!Application.isPlaying)
            {
                SetError("Enter Play mode before running the procedural demo.");
                return;
            }
            if (!isActiveAndEnabled)
            {
                SetError("The component must be active to run the demo.");
                return;
            }
            if (_operation != null)
                StopCoroutine(_operation);
            _operation = StartCoroutine(GenerateAnimation(exportToFile, loadAfterBuild));
        }

        private IEnumerator GenerateAnimation(bool exportToFile, bool loadAfterBuild)
        {
            Unload();
            generatedFilePath = string.Empty;
            generatedRevision = 0;
            generatedBytes = 0;
            SetStatus("Building mutable authoring document...");
            yield return null;

            ImmAuthoringResult settingsValidation = ValidateSettings();
            if (!settingsValidation.Succeeded)
            {
                SetError(settingsValidation.Message);
                _operation = null;
                yield break;
            }

            ImmAuthoringResult<ImmAuthoringDocument> createResult = ImmAuthoringDocument.Create(
                ExportSequenceType.Animated,
                (uint)frameRate,
                new Color(0.015f, 0.02f, 0.04f, 1f));
            if (!createResult.Succeeded)
            {
                SetError(createResult.Message);
                _operation = null;
                yield break;
            }

            using (ImmAuthoringDocument document = createResult.Value)
            {
                ImmAuthoringResult buildResult = BuildDocument(document);
                if (!buildResult.Succeeded)
                {
                    SetError($"Build failed: {buildResult}");
                    _operation = null;
                    yield break;
                }

                if (exportToFile)
                {
                    generatedFilePath = ResolveOutputPath();
                    SetStatus($"Exporting revision {document.Revision}...");
                    yield return null;

                    ImmAuthoringExportResult export = ImmAuthoringCompiler.ExportToFile(document, generatedFilePath);
                    if (!export.Succeeded)
                    {
                        SetError($"Export failed: {export.ErrorCode}: {export.Message}");
                        _operation = null;
                        yield break;
                    }

                    generatedRevision = export.SourceRevision;
                    generatedBytes = export.BytesWritten;
                    SetStatus(
                        $"Exported {generatedBytes:N0} bytes from revision {generatedRevision} " +
                        $"in {export.Statistics.TotalTime.TotalMilliseconds:F1} ms");
                }

                if (loadAfterBuild)
                {
                    SetStatus($"Compiling revision {document.Revision} directly to memory...");
                    ImmAuthoringResult<ImmAuthoringPreviewRequest> previewResult =
                        _previewCoordinator.RequestPreview(document, document.Revision);
                    if (!previewResult.Succeeded)
                    {
                        SetError($"Preview request failed: {previewResult.ErrorCode}: {previewResult.Message}");
                        _operation = null;
                        yield break;
                    }

                    ImmAuthoringPreviewRequest preview = previewResult.Value;
                    while (!preview.IsTerminal)
                        yield return null;
                    if (preview.State != ImmAuthoringPreviewState.Installed)
                    {
                        SetError($"Preview failed: {preview.ErrorCode}: {preview.Message}");
                        _operation = null;
                        yield break;
                    }

                    _loadedDocument = _previewCoordinator.InstalledDocument;
                    generatedRevision = _previewCoordinator.InstalledRevision;
                    generatedBytes = preview.Statistics.BytesCompiled;
                    SetStatus(
                        $"Playing {frameCount} frames from revision {generatedRevision} directly from memory; " +
                        $"no IMM file was written ({generatedBytes:N0} bytes, " +
                        $"{preview.Statistics.TotalTime.TotalMilliseconds:F1} ms preview latency)");
                }
            }
            _operation = null;
        }

        private ImmAuthoringResult BuildDocument(ImmAuthoringDocument document)
        {
            ImmAuthoringResult<ImmAuthoringTransaction> transactionResult = document.BeginEdit(document.Revision);
            if (!transactionResult.Succeeded)
                return transactionResult.WithoutValue();

            using (ImmAuthoringTransaction transaction = transactionResult.Value)
            {
                ImmAuthoringDocument editable = transaction.EditableDocument;
                ImmAuthoringLayerProperties properties = ImmAuthoringLayerProperties.Default("Procedural Ribbon");
                properties.IsTimeline = true;
                properties.DurationTicks = ExportLayerTiming.FromFrames(frameCount, (uint)frameRate).DurationTicks;
                properties.MaxRepeatCount = 0;

                ImmAuthoringResult<long> layerResult = editable.CreatePaintLayer(0, properties);
                if (!layerResult.Succeeded)
                    return layerResult.WithoutValue();
                long layerId = layerResult.Value;

                for (int frame = 0; frame < frameCount; frame++)
                {
                    ImmAuthoringResult<long> drawingResult = editable.CreateDrawing(layerId);
                    if (!drawingResult.Succeeded)
                        return drawingResult.WithoutValue();
                    long drawingId = drawingResult.Value;

                    for (int strand = 0; strand < strandCount; strand++)
                    {
                        PaintPoint[] points = BuildStrand(frame, strand);
                        ImmAuthoringResult<long> strokeResult = editable.CreateStroke(
                            drawingId,
                            BrushSectionType.Circle,
                            VisibilityType.Always,
                            points);
                        if (!strokeResult.Succeeded)
                            return strokeResult.WithoutValue();
                    }

                    ImmAuthoringResult frameResult = editable.AppendFrame(layerId, drawingId);
                    if (!frameResult.Succeeded)
                        return frameResult;
                }

                ImmAuthoringResult<long> commit = transaction.Commit();
                return commit.WithoutValue();
            }
        }

        private PaintPoint[] BuildStrand(int frame, int strand)
        {
            PaintPoint[] points = new PaintPoint[pointsPerStrand];
            float animationPhase = frame * Mathf.PI * 2f / frameCount;
            float strandPhase = strand * Mathf.PI * 2f / strandCount;
            Vector3 previous = Vector3.zero;
            float accumulatedLength = 0f;

            for (int pointIndex = 0; pointIndex < points.Length; pointIndex++)
            {
                float t = pointIndex / (points.Length - 1f);
                float envelopeBase = Mathf.Max(0f, Mathf.Sin(t * Mathf.PI));
                float envelope = Mathf.Pow(envelopeBase, 0.65f);
                float angle = t * turns * Mathf.PI * 2f + animationPhase + strandPhase;
                Vector3 position = new Vector3(
                    (t - 0.5f) * 2.4f,
                    Mathf.Sin(angle) * radius * envelope,
                    Mathf.Cos(angle) * radius * envelope);
                if (pointIndex != 0)
                    accumulatedLength += Vector3.Distance(previous, position);
                previous = position;

                points[pointIndex] = new PaintPoint
                {
                    Position = position,
                    Normal = new Vector3(0f, Mathf.Cos(angle), -Mathf.Sin(angle)).normalized,
                    Direction = Vector3.forward,
                    Color = Color.HSVToRGB(
                        Mathf.Repeat(strand / (float)strandCount + t * 0.2f + frame / (float)frameCount * 0.1f, 1f),
                        0.85f,
                        1f),
                    Alpha = 1f,
                    Width = strokeWidth,
                    Length = accumulatedLength,
                    Time = t
                };
            }

            return points;
        }

        private ImmAuthoringResult ValidateSettings()
        {
            if (frameRate <= 0 || ExportLayerTiming.TicksPerSecond % frameRate != 0)
                return ImmAuthoringResult.Failure(
                    ImmAuthoringErrorCode.InvalidArgument,
                    $"Frame rate must be a positive divisor of {ExportLayerTiming.TicksPerSecond}.");
            if (frameCount < 2 || strandCount < 1 || pointsPerStrand < 3)
                return ImmAuthoringResult.Failure(
                    ImmAuthoringErrorCode.InvalidArgument,
                    "Use at least 2 frames, 1 strand, and 3 points per strand.");
            if (targetCamera == null)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Assign a target camera.");
            return ImmAuthoringResult.Success();
        }

        private string ResolveOutputPath()
        {
            string fileName = string.IsNullOrWhiteSpace(outputFileName) ? "procedural-ribbon.imm" : outputFileName.Trim();
            if (!fileName.EndsWith(".imm", System.StringComparison.OrdinalIgnoreCase))
                fileName += ".imm";
            return Path.Combine(Application.persistentDataPath, Path.GetFileName(fileName));
        }

        private void SetStatus(string message)
        {
            status = message;
            Debug.Log($"{LogPrefix} {message}");
        }

        private void SetError(string message)
        {
            status = message;
            Debug.LogError($"{LogPrefix} {message}");
        }
    }
}
