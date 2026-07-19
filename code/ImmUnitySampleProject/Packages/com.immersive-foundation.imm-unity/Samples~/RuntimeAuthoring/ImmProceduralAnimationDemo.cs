using System;
using System.Collections;
using System.IO;
using System.Linq;
using System.Threading;
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
        private const string LogPrefix = "[IMM_PHASE6_VISUAL_DEMO]";
        private const int CameraId = 0;

        [Header("Run")]
        [SerializeField] private bool generateOnStart = true;
        [SerializeField] private Camera targetCamera;
        [SerializeField] private string outputFileName = "procedural-ribbon.imm";
        [SerializeField, Min(1f)] private float loadTimeoutSeconds = 30f;
        [SerializeField] private bool roundTripThroughImporter = true;
        [SerializeField] private bool modifyAfterInitialPreview = true;
        [SerializeField] private bool demonstrateProductionChecksOnStart = true;
        [SerializeField, Min(0.1f)] private float automaticModificationDelaySeconds = 1.5f;

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
        [SerializeField] private long authoringRevision;
        [SerializeField] private long installedRevision;
        [SerializeField] private long stableStrokeId;
        [SerializeField] private long stableFrameId;
        [SerializeField] private long stableAnimationKeyId;
        [SerializeField] private int modificationCount;
        [SerializeField] private string importLossiness = "Not imported";
        [SerializeField] private bool importedSourceCanBeOverwritten;
        [SerializeField] private int importedLayerCount;
        [SerializeField] private int importedStrokeCount;
        [SerializeField] private int structuralDifferenceCount;
        [SerializeField] private string capabilitySummary;
        [SerializeField] private string operationProgressStage = "Idle";
        [SerializeField, Range(0f, 1f)] private float operationProgress;
        [SerializeField] private string productionSafetyStatus = "Not run";

        private ImmDocument _loadedDocument;
        private ImmAuthoringDocument _authoringDocument;
        private ImmAuthoringPreviewCoordinator _previewCoordinator;
        private Coroutine _operation;
        private bool _useScriptableRenderPipeline;
        private long[] _strokeIds;
        private int _builtFrameCount;
        private int _builtStrandCount;
        private long _frameId;
        private long _firstFrameDrawingId;
        private long _alternateFrameDrawingId;
        private long _opacityKeyId;

        public string Status => status;
        public string GeneratedFilePath => generatedFilePath;
        public long GeneratedRevision => generatedRevision;
        public long GeneratedBytes => generatedBytes;
        public long AuthoringRevision => authoringRevision;
        public long InstalledRevision => installedRevision;
        public long StableStrokeId => stableStrokeId;
        public long StableFrameId => stableFrameId;
        public long StableAnimationKeyId => stableAnimationKeyId;
        public int ModificationCount => modificationCount;
        public string ImportLossiness => importLossiness;
        public bool ImportedSourceCanBeOverwritten => importedSourceCanBeOverwritten;
        public int StructuralDifferenceCount => structuralDifferenceCount;
        public string CapabilitySummary => capabilitySummary;
        public string OperationProgressStage => operationProgressStage;
        public float OperationProgress => operationProgress;
        public string ProductionSafetyStatus => productionSafetyStatus;

        private void Awake()
        {
            _previewCoordinator = GetComponent<ImmAuthoringPreviewCoordinator>();
            if (_previewCoordinator == null)
                _previewCoordinator = gameObject.AddComponent<ImmAuthoringPreviewCoordinator>();
            _previewCoordinator.PlayerLoadTimeoutSeconds = loadTimeoutSeconds;
            _useScriptableRenderPipeline = GraphicsSettings.currentRenderPipeline != null;
            if (targetCamera == null)
                targetCamera = Camera.main;
            ImmAuthoringCapabilities capabilities = ImmAuthoringRuntime.Capabilities;
            capabilitySummary = $"{capabilities.Platform} {capabilities.Architecture}: {capabilities.Features}";
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

        [ContextMenu("Modify Existing Graph and Replace Preview")]
        public void ModifyExistingGraphAndReplacePreview()
        {
            if (!Application.isPlaying)
            {
                SetError("Enter Play mode before modifying the procedural demo.");
                return;
            }
            if (_authoringDocument == null)
            {
                SetError("Generate the mutable authoring document before modifying it.");
                return;
            }
            if (_operation != null)
            {
                SetError("Wait for the current authoring operation to finish.");
                return;
            }

            _operation = StartCoroutine(ModifyAndFinishOperation());
        }

        [ContextMenu("Round Trip Current Graph Through Memory Import")]
        public void RoundTripCurrentGraphThroughMemoryImport()
        {
            if (!Application.isPlaying)
            {
                SetError("Enter Play mode before importing the procedural demo.");
                return;
            }
            if (_authoringDocument == null)
            {
                SetError("Generate the mutable authoring document before importing it.");
                return;
            }
            if (_operation != null)
            {
                SetError("Wait for the current authoring operation to finish.");
                return;
            }

            _operation = StartCoroutine(RoundTripAndPreviewOperation());
        }

        [ContextMenu("Unload Generated IMM")]
        public void Unload()
        {
            _previewCoordinator?.ClearPreview();
            _loadedDocument = null;
            DisposeAuthoringDocument();
            SetStatus("Runtime authoring document and native preview unloaded");
        }

        [ContextMenu("Run Phase 6 Controlled Failure Checks")]
        public void RunProductionSafetyChecks()
        {
            if (!Application.isPlaying || _authoringDocument == null)
            {
                productionSafetyStatus = "Generate the runtime document in Play mode first.";
                return;
            }

            ImmAuthoringLimits tinyOutputLimit = new ImmAuthoringLimits(maxOutputBytes: 1);
            ImmAuthoringExportResult limited = ImmAuthoringCompiler.ExportToMemory(
                _authoringDocument,
                new ImmAuthoringOperationOptions(limits: tinyOutputLimit));
            using (CancellationTokenSource cancellation = new CancellationTokenSource())
            {
                cancellation.Cancel();
                ImmAuthoringExportResult cancelled = ImmAuthoringCompiler.ExportToMemory(
                    _authoringDocument,
                    new ImmAuthoringOperationOptions(cancellation.Token));
                ImmAuthoringImportResult corrupt = ImmAuthoringImporter.ImportFromMemory(
                    new byte[] { 0x49, 0x4d, 0x4d, 0x00 });
                bool passed = limited.ErrorCode == ImmAuthoringErrorCode.ResourceLimitExceeded &&
                              cancelled.ErrorCode == ImmAuthoringErrorCode.Cancelled &&
                              corrupt.ErrorCode == ImmAuthoringErrorCode.CorruptInput;
                productionSafetyStatus = passed
                    ? $"Controlled failures: limit={limited.ErrorCode}, cancellation={cancelled.ErrorCode}, malformed={corrupt.ErrorCode}"
                    : $"Unexpected results: limit={limited.ErrorCode}, cancellation={cancelled.ErrorCode}, malformed={corrupt.ErrorCode}";
            }
            Debug.Log($"{LogPrefix} {productionSafetyStatus}");
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
            authoringRevision = 0;
            installedRevision = 0;
            stableStrokeId = 0;
            stableFrameId = 0;
            stableAnimationKeyId = 0;
            modificationCount = 0;
            importLossiness = "Not imported";
            importedSourceCanBeOverwritten = false;
            importedLayerCount = 0;
            importedStrokeCount = 0;
            structuralDifferenceCount = 0;
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

            ImmAuthoringDocument document = createResult.Value;
            _authoringDocument = document;
            ImmAuthoringResult buildResult = BuildDocument(document);
            if (!buildResult.Succeeded)
            {
                SetError($"Build failed: {buildResult}");
                DisposeAuthoringDocument();
                _operation = null;
                yield break;
            }
            authoringRevision = document.Revision;

            if (demonstrateProductionChecksOnStart)
                RunProductionSafetyChecks();

            if (roundTripThroughImporter)
            {
                SetStatus($"Exporting revision {document.Revision} to memory for supported IMM import...");
                yield return null;
                ImmAuthoringResult importResult = RoundTripCurrentGraphThroughMemory();
                if (!importResult.Succeeded)
                {
                    SetError($"Memory import failed: {importResult.ErrorCode}: {importResult.Message}");
                    _operation = null;
                    yield break;
                }
            }

            if (exportToFile)
            {
                generatedFilePath = ResolveOutputPath();
                SetStatus($"Exporting revision {_authoringDocument.Revision}...");
                yield return null;

                ImmAuthoringExportResult export = ImmAuthoringCompiler.ExportToFile(
                    _authoringDocument,
                    generatedFilePath,
                    CreateOperationOptions());
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
                yield return RequestAndInstallPreview(_authoringDocument, "initial");
                if (_loadedDocument != null && modifyAfterInitialPreview)
                {
                    yield return new WaitForSeconds(automaticModificationDelaySeconds);
                    yield return ModifyAuthoringGraphAndReplacePreview();
                }
            }
            _operation = null;
        }

        private IEnumerator ModifyAndFinishOperation()
        {
            yield return ModifyAuthoringGraphAndReplacePreview();
            _operation = null;
        }

        private IEnumerator RoundTripAndPreviewOperation()
        {
            SetStatus($"Exporting mutable revision {_authoringDocument.Revision} to memory for re-import...");
            yield return null;
            ImmAuthoringResult importResult = RoundTripCurrentGraphThroughMemory();
            if (!importResult.Succeeded)
            {
                SetError($"Memory import failed: {importResult.ErrorCode}: {importResult.Message}");
                _operation = null;
                yield break;
            }

            yield return RequestAndInstallPreview(_authoringDocument, "imported");
            _operation = null;
        }

        private IEnumerator ModifyAuthoringGraphAndReplacePreview()
        {
            long baseRevision = _authoringDocument.Revision;
            int nextModification = modificationCount + 1;
            SetStatus($"Editing existing graph at revision {baseRevision} using stable stroke IDs...");

            ImmAuthoringResult<ImmAuthoringTransaction> transactionResult =
                _authoringDocument.BeginEdit(baseRevision);
            if (!transactionResult.Succeeded)
            {
                SetError($"Edit transaction failed: {transactionResult.ErrorCode}: {transactionResult.Message}");
                yield break;
            }

            using (ImmAuthoringTransaction transaction = transactionResult.Value)
            {
                ImmAuthoringDocument editable = transaction.EditableDocument;
                for (int frame = 0; frame < _builtFrameCount; frame++)
                {
                    for (int strand = 0; strand < _builtStrandCount; strand++)
                    {
                        int strokeIndex = frame * _builtStrandCount + strand;
                        long strokeId = _strokeIds[strokeIndex];
                        ImmAuthoringResult replace = editable.ReplaceStroke(
                            strokeId,
                            BrushSectionType.Circle,
                            VisibilityType.Always,
                            BuildStrand(frame, strand, nextModification));
                        if (!replace.Succeeded)
                        {
                            SetError($"Stroke {strokeId} replacement failed: {replace}");
                            transaction.Abort();
                            yield break;
                        }
                    }
                }

                if (_opacityKeyId != 0)
                {
                    ImmAuthoringResult<ImmAuthoringSnapshot> snapshotResult = editable.CreateSnapshot();
                    if (!snapshotResult.Succeeded ||
                        !snapshotResult.Value.TryGetAnimationKey(_opacityKeyId, out ImmAuthoringAnimationKeySnapshot opacityKey))
                    {
                        SetError($"Animation key {_opacityKeyId} could not be queried before replacement.");
                        transaction.Abort();
                        yield break;
                    }

                    float opacity = nextModification % 2 == 0 ? 0.3f : 0.9f;
                    ImmAuthoringResult replaceKey = editable.ReplaceAnimationKey(
                        _opacityKeyId,
                        opacityKey.Property,
                        opacityKey.TimeTicks,
                        ImmAuthoringAnimationValue.FromFloat(opacity),
                        opacityKey.Interpolation);
                    if (!replaceKey.Succeeded)
                    {
                        SetError($"Animation key {_opacityKeyId} replacement failed: {replaceKey}");
                        transaction.Abort();
                        yield break;
                    }
                }

                if (_frameId != 0 && _alternateFrameDrawingId != 0)
                {
                    long targetDrawingId = nextModification % 2 == 0
                        ? _firstFrameDrawingId
                        : _alternateFrameDrawingId;
                    ImmAuthoringResult replaceFrame = editable.SetFrameDrawing(_frameId, targetDrawingId);
                    if (!replaceFrame.Succeeded)
                    {
                        SetError($"Frame mapping {_frameId} replacement failed: {replaceFrame}");
                        transaction.Abort();
                        yield break;
                    }
                }

                ImmAuthoringResult<long> commit = transaction.Commit();
                if (!commit.Succeeded)
                {
                    SetError($"Edit commit failed: {commit.ErrorCode}: {commit.Message}");
                    yield break;
                }

                authoringRevision = commit.Value;
                modificationCount = nextModification;
            }

            yield return RequestAndInstallPreview(_authoringDocument, "replacement");
            if (_loadedDocument != null && installedRevision == authoringRevision)
            {
                string sourceDescription = roundTripThroughImporter
                    ? "the graph was imported from IMM memory"
                    : "the graph remained in the original mutable document";
                SetStatus(
                    $"Replaced native preview with revision {installedRevision} after modifying " +
                    $"{_strokeIds.Length:N0} existing strokes, frame mapping {_frameId}, and animation key " +
                    $"{_opacityKeyId} by stable ID; " +
                    $"{sourceDescription} and no file was written");
            }
        }

        private IEnumerator RequestAndInstallPreview(ImmAuthoringDocument document, string requestKind)
        {
            SetStatus($"Compiling {requestKind} revision {document.Revision} directly to memory...");
            ImmAuthoringResult<ImmAuthoringPreviewRequest> previewResult =
                _previewCoordinator.RequestPreview(document, document.Revision);
            if (!previewResult.Succeeded)
            {
                SetError($"Preview request failed: {previewResult.ErrorCode}: {previewResult.Message}");
                yield break;
            }

            ImmAuthoringPreviewRequest preview = previewResult.Value;
            while (!preview.IsTerminal)
                yield return null;
            if (preview.State != ImmAuthoringPreviewState.Installed)
            {
                SetError($"Preview failed: {preview.ErrorCode}: {preview.Message}");
                yield break;
            }

            _loadedDocument = _previewCoordinator.InstalledDocument;
            generatedRevision = _previewCoordinator.InstalledRevision;
            installedRevision = generatedRevision;
            generatedBytes = preview.Statistics.BytesCompiled;
            SetStatus(
                $"Playing {frameCount} frames from revision {generatedRevision} directly from memory; " +
                $"no IMM file was written ({generatedBytes:N0} bytes, " +
                $"{preview.Statistics.TotalTime.TotalMilliseconds:F1} ms preview latency)");
        }

        private ImmAuthoringResult BuildDocument(ImmAuthoringDocument document)
        {
            _builtFrameCount = frameCount;
            _builtStrandCount = strandCount;
            _strokeIds = new long[_builtFrameCount * _builtStrandCount];
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
                properties.PaintMaxRepeatCount = 0;

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
                    if (frame == 0)
                        _firstFrameDrawingId = drawingId;
                    else if (frame == 1)
                        _alternateFrameDrawingId = drawingId;

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
                        _strokeIds[frame * _builtStrandCount + strand] = strokeResult.Value;
                    }

                    ImmAuthoringResult<long> frameResult = editable.AppendFrameWithId(layerId, drawingId);
                    if (!frameResult.Succeeded)
                        return frameResult.WithoutValue();
                    if (frame == 0)
                        _frameId = frameResult.Value;
                }

                ImmAuthoringResult<long> opacityStart = editable.CreateAnimationKey(
                    layerId,
                    ImmAuthoringAnimationProperty.Opacity,
                    0,
                    ImmAuthoringAnimationValue.FromFloat(0.45f),
                    ImmAuthoringInterpolation.Smoothstep);
                if (!opacityStart.Succeeded)
                    return opacityStart.WithoutValue();
                _opacityKeyId = opacityStart.Value;

                ImmAuthoringResult<long> opacityMiddle = editable.CreateAnimationKey(
                    layerId,
                    ImmAuthoringAnimationProperty.Opacity,
                    properties.DurationTicks / 2,
                    ImmAuthoringAnimationValue.FromFloat(1f),
                    ImmAuthoringInterpolation.Smoothstep);
                if (!opacityMiddle.Succeeded)
                    return opacityMiddle.WithoutValue();

                ImmAuthoringResult<long> opacityEnd = editable.CreateAnimationKey(
                    layerId,
                    ImmAuthoringAnimationProperty.Opacity,
                    properties.DurationTicks,
                    ImmAuthoringAnimationValue.FromFloat(0.45f),
                    ImmAuthoringInterpolation.Smoothstep);
                if (!opacityEnd.Succeeded)
                    return opacityEnd.WithoutValue();

                ImmAuthoringResult<long> commit = transaction.Commit();
                if (commit.Succeeded && _strokeIds.Length > 0)
                {
                    stableStrokeId = _strokeIds[0];
                    stableFrameId = _frameId;
                    stableAnimationKeyId = _opacityKeyId;
                }
                return commit.WithoutValue();
            }
        }

        private ImmAuthoringResult RoundTripCurrentGraphThroughMemory()
        {
            ImmAuthoringResult<ImmAuthoringSnapshot> sourceSnapshotResult = _authoringDocument.CreateSnapshot();
            if (!sourceSnapshotResult.Succeeded)
                return sourceSnapshotResult.WithoutValue();

            ImmAuthoringExportResult export = ImmAuthoringCompiler.ExportToMemory(
                _authoringDocument,
                CreateOperationOptions());
            if (!export.Succeeded)
                return ImmAuthoringResult.Failure(export.ErrorCode, export.Message);

            ImmAuthoringImportResult import = ImmAuthoringImporter.ImportFromMemory(
                export.Data,
                CreateOperationOptions());
            if (!import.Succeeded)
                return ImmAuthoringResult.Failure(import.ErrorCode, import.Message);

            ImmAuthoringResult<ImmAuthoringSnapshot> importedSnapshotResult = import.Document.CreateSnapshot();
            if (!importedSnapshotResult.Succeeded)
            {
                import.Document.Dispose();
                return importedSnapshotResult.WithoutValue();
            }

            ImmAuthoringSnapshot importedSnapshot = importedSnapshotResult.Value;
            ImmAuthoringStructuralComparison comparison = ImmAuthoringStructuralComparer.Compare(
                sourceSnapshotResult.Value,
                importedSnapshot,
                0.02f);
            importLossiness = import.Lossiness.ToString();
            importedSourceCanBeOverwritten = import.CanOverwriteSource;
            importedLayerCount = import.Statistics.ImportedLayerCount;
            importedStrokeCount = import.Statistics.ImportedStrokeCount;
            structuralDifferenceCount = comparison.Differences.Count;
            generatedBytes = export.BytesWritten;
            generatedRevision = export.SourceRevision;

            if (!import.CanOverwriteSource)
            {
                string issueSummary = string.Join("; ", import.Issues.Take(3).Select(issue => issue.Message));
                import.Document.Dispose();
                return ImmAuthoringResult.Failure(
                    ImmAuthoringErrorCode.ValidationFailed,
                    $"Import reported {import.Lossiness} content and cannot safely overwrite its source: {issueSummary}");
            }
            if (!comparison.Equivalent)
            {
                string differenceSummary = string.Join("; ", comparison.Differences.Take(3));
                import.Document.Dispose();
                return ImmAuthoringResult.Failure(
                    ImmAuthoringErrorCode.ValidationFailed,
                    $"Imported structure differs from the source graph: {differenceSummary}");
            }

            ImmAuthoringDocument previous = _authoringDocument;
            _authoringDocument = import.Document;
            previous.Dispose();
            authoringRevision = _authoringDocument.Revision;
            _strokeIds = importedSnapshot.Layers
                .Where(layer => layer.Type == ImmAuthoringLayerType.Paint)
                .SelectMany(layer => layer.Drawings)
                .SelectMany(drawing => drawing.Strokes)
                .Select(stroke => stroke.Id)
                .ToArray();
            ImmAuthoringLayerSnapshot importedPaintLayer = importedSnapshot.Layers
                .FirstOrDefault(layer => layer.Type == ImmAuthoringLayerType.Paint);
            _frameId = importedPaintLayer?.Frames.FirstOrDefault()?.Id ?? 0;
            _firstFrameDrawingId = importedPaintLayer?.Frames.FirstOrDefault()?.DrawingId ?? 0;
            _alternateFrameDrawingId = importedPaintLayer != null && importedPaintLayer.Frames.Count > 1
                ? importedPaintLayer.Frames[1].DrawingId
                : 0;
            ImmAuthoringAnimationKeySnapshot importedOpacityKey = importedSnapshot.Layers
                .SelectMany(layer => layer.AnimationKeys)
                .FirstOrDefault(key => key.Property == ImmAuthoringAnimationProperty.Opacity);
            _opacityKeyId = importedOpacityKey?.Id ?? 0;
            stableStrokeId = _strokeIds.Length > 0 ? _strokeIds[0] : 0;
            stableFrameId = _frameId;
            stableAnimationKeyId = _opacityKeyId;

            SetStatus(
                $"Imported {generatedBytes:N0} IMM bytes into mutable revision {authoringRevision}: " +
                $"{importLossiness}, overwrite safe={importedSourceCanBeOverwritten}, " +
                $"{importedLayerCount} layers, {importedStrokeCount:N0} strokes, " +
                $"{structuralDifferenceCount} structural differences");
            return ImmAuthoringResult.Success();
        }

        private PaintPoint[] BuildStrand(int frame, int strand, int modification = 0)
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
                float deformation = modification == 0
                    ? 0f
                    : Mathf.Sin(t * Mathf.PI * 2f + modification * 0.7f) * 0.32f * envelope;
                float modifiedRadius = radius * (1f + 0.12f * modification);
                Vector3 position = new Vector3(
                    (t - 0.5f) * 2.4f,
                    Mathf.Sin(angle) * modifiedRadius * envelope + deformation,
                    Mathf.Cos(angle) * modifiedRadius * envelope);
                if (pointIndex != 0)
                    accumulatedLength += Vector3.Distance(previous, position);
                previous = position;

                points[pointIndex] = new PaintPoint
                {
                    Position = position,
                    Normal = new Vector3(0f, Mathf.Cos(angle), -Mathf.Sin(angle)).normalized,
                    Direction = Vector3.forward,
                    Color = Color.HSVToRGB(
                        Mathf.Repeat(
                            strand / (float)strandCount + t * 0.2f +
                            frame / (float)frameCount * 0.1f + modification * 0.13f,
                            1f),
                        0.85f,
                        1f),
                    Alpha = 1f,
                    Width = strokeWidth * (1f + modification * 0.3f),
                    Length = accumulatedLength,
                    Time = 0.5f * pointIndex / points.Length
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

        private ImmAuthoringOperationOptions CreateOperationOptions()
        {
            return new ImmAuthoringOperationOptions(progress: new DemoProgress(ReportProgress));
        }

        private void ReportProgress(ImmAuthoringProgress progress)
        {
            operationProgressStage = progress.Stage.ToString();
            operationProgress = progress.Fraction;
        }

        private void DisposeAuthoringDocument()
        {
            _authoringDocument?.Dispose();
            _authoringDocument = null;
            _strokeIds = null;
            _builtFrameCount = 0;
            _builtStrandCount = 0;
            _frameId = 0;
            _firstFrameDrawingId = 0;
            _alternateFrameDrawingId = 0;
            _opacityKeyId = 0;
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

        private sealed class DemoProgress : IProgress<ImmAuthoringProgress>
        {
            private readonly Action<ImmAuthoringProgress> _report;

            internal DemoProgress(Action<ImmAuthoringProgress> report)
            {
                _report = report;
            }

            public void Report(ImmAuthoringProgress value) => _report(value);
        }
    }
}
