using System;
using System.IO;
using System.Text;
using UnityEngine;
// The adapter lives in the ImmStrokeReader namespace while the P/Invoke class of the
// same name lives in ImmPlayer, so alias both it and the SharpQuill model type.
using StrokeReaderCompat = ImmStrokeReader.SharpQuillCompat;
using SharpQuillSequence = SharpQuill.Sequence;

namespace ImmPlayer
{
    /// <summary>
    /// Example component for the IMM stroke reader package: loads an IMM document without
    /// the player/renderer and reports what the reader exposes.
    /// </summary>
    /// <remarks>
    /// This is the sample-project counterpart to the playback examples: it exercises the
    /// stroke reader surface that the player does not need - raw layer/stroke/drawing
    /// queries, the authoring view (frames and animation keys), spawn-area viewpoint data,
    /// picture pixels, chapter navigation, the SharpQuill adapter and the plugin build id.
    ///
    /// Open Assets/Scenes/StrokeReaderSampleScene.unity and press Play, or use the
    /// context-menu entries to run one step at a time. Use <see cref="immFilePath"/> to
    /// point at a specific file; otherwise the component picks the first *.imm file in
    /// StreamingAssets.
    /// </remarks>
    public class ImmStrokeReaderExample : MonoBehaviour
    {
        private const string Prefix = "[IMM_SR_SAMPLE] ";

        private enum LoadMode
        {
            /// <summary>StrokeReaderDocument.Load(path).</summary>
            FilePath,
            /// <summary>Read the file into memory and use StrokeReaderDocument.Load(bytes).</summary>
            Memory
        }

        [Header("Source")]
        [Tooltip("Absolute path or StreamingAssets-relative path to an IMM file. Empty = first *.imm in StreamingAssets.")]
        [SerializeField] private string immFilePath = "";

        [SerializeField] private LoadMode loadMode = LoadMode.FilePath;

        [Tooltip("Load automatically on Start.")]
        [SerializeField] private bool loadOnStart = true;

        [Tooltip("Walk every layer, drawing and stroke of the loaded document.")]
        [SerializeField] private bool reportAllLayers = true;

        [Tooltip("Maximum strokes inspected per drawing (points are the expensive part).")]
        [SerializeField] private int maxStrokesPerDrawing = 4;

        [Tooltip("Also run the SharpQuill adapter over the same file.")]
        [SerializeField] private bool exerciseSharpQuill = true;

        [Header("Status")]
        [SerializeField, HideInInspector] private string lastSummary = "";
        [SerializeField, HideInInspector] private bool lastLoadSucceeded;

        private StrokeReaderDocument _document;
        private string _resolvedPath;
        private readonly StringBuilder _report = new StringBuilder();
        private Vector2 _scroll;

        private void Start()
        {
            if (loadOnStart)
            {
                LoadAndReport();
            }
        }

        private void OnDestroy()
        {
            _document?.Dispose();
            _document = null;

            // Nothing else in the sample project owns the stroke reader, so this scene
            // also covers the shutdown path (StrokeReader_End releases the plugin state).
            if (ImmStrokeReader.StrokeReader_IsInitialized())
            {
                ImmStrokeReader.StrokeReader_End();
                Debug.Log($"{Prefix}StrokeReader_End called");
            }
        }

        private void OnGUI()
        {
            GUILayout.BeginArea(new Rect(12, 12, 640, 360), GUI.skin.box);
            GUILayout.Label($"IMM stroke reader: {(_document != null && _document.IsLoaded ? "loaded" : "no document")}");
            GUILayout.Label($"build: {ImmStrokeReader.GetBuildId()}");
            if (!string.IsNullOrEmpty(_resolvedPath))
            {
                GUILayout.Label($"file: {Path.GetFileName(_resolvedPath)}");
            }
            GUILayout.Label("Buttons: load, chapter step, unload");
            if (GUILayout.Button("Load and report"))
            {
                LoadAndReport();
            }
            if (GUILayout.Button("Next chapter"))
            {
                NextChapter();
            }
            if (GUILayout.Button("Unload"))
            {
                Unload();
            }
            _scroll = GUILayout.BeginScrollView(_scroll);
            GUILayout.Label(lastSummary);
            GUILayout.EndScrollView();
            GUILayout.EndArea();
        }

        [ContextMenu("Load And Report")]
        public void LoadAndReport()
        {
            Unload();
            _report.Clear();

            string path = ResolvePath();
            if (string.IsNullOrEmpty(path))
            {
                lastLoadSucceeded = false;
                lastSummary = "No IMM file found. Copy one into Assets/StreamingAssets/ or set the file path.";
                Debug.LogWarning(Prefix + lastSummary);
                return;
            }
            _resolvedPath = path;

            Log($"build id          : {ImmStrokeReader.GetBuildId()}");
            Log($"native init       : {ImmStrokeReader.StrokeReader_IsInitialized()}");
            Log($"source            : {path}");
            Log($"load mode         : {loadMode}");

            _document = new StrokeReaderDocument();
            bool loaded = loadMode == LoadMode.Memory
                ? _document.Load(File.ReadAllBytes(path), LogPath())
                : _document.Load(path, LogPath());

            lastLoadSucceeded = loaded;
            if (!loaded)
            {
                lastSummary = "Load failed; see the console for the stroke reader log location.";
                Debug.LogError(Prefix + lastSummary);
                _document.Dispose();
                _document = null;
                return;
            }

            Log($"document id       : {_document.DocId}");
            Log($"loaded documents  : {ImmStrokeReader.StrokeReader_GetDocumentCount()}");

            ReportDocumentInfo();
            ReportChapters();
            ReportLayers();
            ReportAuthoringView();
            if (exerciseSharpQuill)
            {
                ReportSharpQuill(path);
            }

            lastSummary = _report.ToString();
            Debug.Log($"{Prefix}report for '{Path.GetFileName(path)}'{Environment.NewLine}{lastSummary}");
        }

        [ContextMenu("Unload")]
        public void Unload()
        {
            if (_document == null)
            {
                return;
            }

            _document.Dispose();
            _document = null;
            Log("document unloaded");
        }

        [ContextMenu("Next Chapter")]
        public void NextChapter()
        {
            if (_document == null || !_document.IsLoaded)
            {
                return;
            }

            int chapters = _document.ChapterCount;
            if (chapters <= 1)
            {
                Debug.Log($"{Prefix}document has {chapters} chapter(s); nothing to cycle");
                return;
            }

            int next = (_document.CurrentChapter + 1) % chapters;
            bool applied = _document.SetChapter(next);
            Log($"set chapter {next}/{chapters - 1} -> {applied}; current={_document.CurrentChapter}");
            lastSummary = _report.ToString();
        }

        private void ReportDocumentInfo()
        {
            if (!_document.GetDocumentInfo(out StrokeDocumentInfo info))
            {
                Log("document info     : unavailable");
                return;
            }

            Log($"sequence type     : {info.sequenceType}");
            Log($"frame rate        : {info.frameRate}");
            Log($"background        : ({info.backgroundR:F3}, {info.backgroundG:F3}, {info.backgroundB:F3})");
            Log($"capabilities      : 0x{info.capabilities:X8}");
            Log($"root anim keys    : {info.rootAnimationKeyCount}");
        }

        private void ReportChapters()
        {
            int chapters = _document.ChapterCount;
            Log($"chapters          : {chapters} (current {_document.CurrentChapter})");
            if (chapters <= 1)
            {
                return;
            }

            // The drawing each chapter starts at; the wrapper exposes chapters through the
            // document, while this mapping is only available on the native entry point.
            for (int layer = 0; layer < Mathf.Min(_document.LayerCount, 4); layer++)
            {
                int drawingIndex = ImmStrokeReader.StrokeReader_GetDrawingIndexForChapter(_document.DocId, layer, _document.CurrentChapter);
                if (drawingIndex > 0)
                {
                    Log($"  layer {layer}: chapter drawing index {drawingIndex}");
                }
            }
        }

        private void ReportLayers()
        {
            int layerCount = _document.LayerCount;
            Log($"layers            : {layerCount}");

            int layerLimit = reportAllLayers ? layerCount : Mathf.Min(layerCount, 8);
            int totalDrawings = 0;
            int totalStrokes = 0;
            int totalPoints = 0;

            for (int layerIndex = 0; layerIndex < layerLimit; layerIndex++)
            {
                if (!_document.GetLayerInfo(layerIndex, out StrokeLayerInfo info))
                {
                    continue;
                }

                float biggest = 0.0f;
                int drawings = _document.GetDrawingCount(layerIndex);
                Log($"  layer {layerIndex}: '{info.name}' type={info.type} drawings={drawings} visible={info.visible} opacity={info.opacity:F2} defaultSpawn={info.isDefaultSpawn}");

                if (_document.GetLayerSpawnAreaInfo(layerIndex, out StrokeSpawnAreaInfo spawnArea))
                {
                    Log($"    viewpoint: floorLevel={spawnArea.IsFloorLevel} volume={spawnArea.volumeType} " +
                        $"offset=({spawnArea.volumeOffsetX:F2}, {spawnArea.volumeOffsetY:F2}, {spawnArea.volumeOffsetZ:F2}) " +
                        $"extent=({spawnArea.volumeExtentX:F2}, {spawnArea.volumeExtentY:F2}, {spawnArea.volumeExtentZ:F2}) " +
                        $"locomotion={spawnArea.locomotion}");
                }

                if (_document.GetLayerTransform(layerIndex, out StrokeLayerTransform local, out StrokeLayerTransform world))
                {
                    Log($"    transform: local=({local.transX:F2}, {local.transY:F2}, {local.transZ:F2}) " +
                        $"world=({world.transX:F2}, {world.transY:F2}, {world.transZ:F2})");
                }

                if (_document.GetPictureInfo(layerIndex, out StrokePictureInfo picture))
                {
                    // Pixel data is copied out through a pinned managed buffer: the native
                    // side only hands back a pointer into the loaded picture.
                    int bufferSize = Mathf.Min(Mathf.Max(picture.dataSize, 0), 1 << 20);
                    int pixelsRead = 0;
                    if (bufferSize > 0)
                    {
                        var pixels = new byte[bufferSize];
                        System.Runtime.InteropServices.GCHandle pinned =
                            System.Runtime.InteropServices.GCHandle.Alloc(pixels, System.Runtime.InteropServices.GCHandleType.Pinned);
                        try
                        {
                            pixelsRead = ImmStrokeReader.StrokeReader_GetPicturePixelData(
                                _document.DocId, layerIndex, pinned.AddrOfPinnedObject(), pixels.Length);
                        }
                        finally
                        {
                            pinned.Free();
                        }
                    }

                    Log($"    picture: {picture.width}x{picture.height} contentType={picture.contentType} " +
                        $"hasAlpha={picture.hasAlpha} dataSize={picture.dataSize} pixelsRead={pixelsRead} locked={picture.isViewerLocked}");
                }

                for (int drawingIndex = 0; drawingIndex < drawings; drawingIndex++)
                {
                    totalDrawings++;
                    int strokeCount = _document.GetStrokeCount(layerIndex, drawingIndex);
                    totalStrokes += strokeCount;

                    if (_document.TryGetDrawingBiggestStroke(layerIndex, drawingIndex, out float drawingBiggest))
                    {
                        biggest = Mathf.Max(biggest, drawingBiggest);
                    }

                    int strokeLimit = Mathf.Min(strokeCount, Mathf.Max(0, maxStrokesPerDrawing));
                    for (int strokeIndex = 0; strokeIndex < strokeLimit; strokeIndex++)
                    {
                        if (!_document.GetStrokeInfo(layerIndex, drawingIndex, strokeIndex, out StrokeInfo strokeInfo))
                        {
                            continue;
                        }

                        totalPoints += strokeInfo.numPoints;
                        StrokePoint[] points = _document.GetStrokePoints(layerIndex, drawingIndex, strokeIndex);
                        if (points != null && points.Length > 0)
                        {
                            StrokePoint first = points[0];
                            Log($"    drawing {drawingIndex} stroke {strokeIndex}: brush={strokeInfo.brushType} " +
                                $"visibility={strokeInfo.visibilityMode} points={strokeInfo.numPoints} " +
                                $"first=({first.px:F2}, {first.py:F2}, {first.pz:F2}) width={first.width:F3}");
                        }
                    }

                    if (strokeCount > strokeLimit)
                    {
                        Log($"    drawing {drawingIndex}: {strokeCount} strokes ({strokeCount - strokeLimit} not inspected)");
                    }
                }

                if (biggest > 0.0f)
                {
                    Log($"    biggest stroke extent: {biggest:F3}");
                }
            }

            Log($"totals            : {totalDrawings} drawings, {totalStrokes} strokes, {totalPoints} points inspected");
        }

        private void ReportAuthoringView()
        {
            int authoringLayers = _document.AuthoringLayerCount;
            Log($"authoring layers  : {authoringLayers}");

            int limit = reportAllLayers ? authoringLayers : Mathf.Min(authoringLayers, 8);
            for (int layerIndex = 0; layerIndex < limit; layerIndex++)
            {
                if (!_document.GetAuthoringLayerInfo(layerIndex, out StrokeAuthoringLayerInfo info))
                {
                    continue;
                }

                int drawings = _document.GetAuthoringDrawingCount(layerIndex);
                Log($"  authoring {layerIndex}: '{info.name}' parent={info.parentId} childIndex={info.childIndex} " +
                    $"timeline={info.isTimeline} duration={info.durationTicks} maxRepeat={info.maxRepeatCount} drawings={drawings}");

                if (_document.GetAuthoringLayerTransform(layerIndex, out StrokeLayerTransform local, out _))
                {
                    Log($"    local transform: ({local.transX:F2}, {local.transY:F2}, {local.transZ:F2})");
                }

                StrokeAnimationKey[] keys = _document.GetLayerAnimationKeys(layerIndex);
                if (keys != null && keys.Length > 0)
                {
                    Log($"    animation keys: {keys.Length}");
                    for (int keyIndex = 0; keyIndex < Mathf.Min(keys.Length, 4); keyIndex++)
                    {
                        StrokeAnimationKey key = keys[keyIndex];
                        Log($"      key {keyIndex}: property={key.property} time={key.timeTicks} interpolation={key.interpolation} " +
                            $"bool={key.boolValue} int={key.intValue} float={key.floatValue:F3} double={key.doubleValue:F3}");
                    }
                }

                // Frame mapping and paint metadata live on the raw entry points; the
                // authoring importer uses exactly these calls.
                if (ImmStrokeReader.StrokeReader_GetAuthoringLayerAnimationInfo(
                        _document.DocId, layerIndex, out int frameRate, out int frameCount, out int maxRepeatCount))
                {
                    Log($"    paint frames: rate={frameRate} count={frameCount} maxRepeat={maxRepeatCount}");
                    if (frameCount > 0)
                    {
                        var frames = new int[Mathf.Min(frameCount, 16)];
                        int read = ImmStrokeReader.StrokeReader_GetAuthoringFrameBuffer(_document.DocId, layerIndex, frames, frames.Length);
                        Log($"    frame buffer: read {read} of {frameCount} entries");
                    }
                }

                for (int drawingIndex = 0; drawingIndex < Mathf.Min(drawings, maxStrokesPerDrawing); drawingIndex++)
                {
                    int strokes = _document.GetAuthoringStrokeCount(layerIndex, drawingIndex);
                    if (strokes <= 0)
                    {
                        continue;
                    }

                    if (_document.GetAuthoringStrokeInfo(layerIndex, drawingIndex, 0, out StrokeInfo info))
                    {
                        StrokeAuthoringPoint[] points = _document.GetAuthoringStrokePoints(layerIndex, drawingIndex, 0);
                        Log($"    drawing {drawingIndex}: {strokes} strokes, first brush={info.brushType} points={info.numPoints} " +
                            $"authoringPoints={(points != null ? points.Length : 0)}");
                    }
                }
            }
        }

        private void ReportSharpQuill(string path)
        {
            try
            {
                SharpQuillSequence sequence = StrokeReaderCompat.ReadImmAsSequence(path);
                if (sequence == null || sequence.RootLayer == null)
                {
                    Log("sharpquill        : adapter returned no sequence");
                    return;
                }

                Log($"sharpquill        : root='{sequence.RootLayer.Name}' children={sequence.RootLayer.Children.Count} " +
                    $"chapters={StrokeReaderCompat.GetImmChapterCount(path)}");
            }
            catch (Exception exception)
            {
                Log($"sharpquill        : adapter failed ({exception.GetType().Name}: {exception.Message})");
            }
        }

        private string ResolvePath()
        {
            if (!string.IsNullOrEmpty(immFilePath))
            {
                if (Path.IsPathRooted(immFilePath) && File.Exists(immFilePath))
                {
                    return immFilePath;
                }

                string streamingCandidate = Path.Combine(Application.streamingAssetsPath, immFilePath);
                if (File.Exists(streamingCandidate))
                {
                    return streamingCandidate;
                }

                return File.Exists(immFilePath) ? Path.GetFullPath(immFilePath) : null;
            }

            string streamingRoot = Application.streamingAssetsPath;
            if (!Directory.Exists(streamingRoot))
            {
                return null;
            }

            string[] candidates = Directory.GetFiles(streamingRoot, "*.imm", SearchOption.AllDirectories);
            return candidates.Length > 0 ? candidates[0] : null;
        }

        private static string LogPath()
        {
            string directory = Path.Combine(Application.persistentDataPath, "imm-stroke-reader");
            Directory.CreateDirectory(directory);
            return Path.Combine(directory, "imm_stroke_reader_sample_log.txt");
        }

        private void Log(string line)
        {
            _report.AppendLine(line);
        }
    }
}
