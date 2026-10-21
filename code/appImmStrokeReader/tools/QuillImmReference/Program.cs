using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using Newtonsoft.Json.Linq;
using ImmPlayer;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

// Inspection only: neither project nor reference IMM is modified. Matching names
// and indices are candidate correspondences, never proof of export provenance.
if (args.Length < 4 || args.Length > 5 || (args.Length == 5 && args[4] != "--metadata-only"))
    throw new ArgumentException("Expected Quill project directory, reference IMM, native reader DLL, report JSON, and optional --metadata-only.");
bool metadataOnly = args.Length == 5;
string sourcePath = Path.GetFullPath(args[0]);
string referencePath = Path.GetFullPath(args[1]);
string libraryPath = Path.GetFullPath(args[2]);
string reportPath = Path.GetFullPath(args[3]);
Directory.CreateDirectory(Path.GetDirectoryName(reportPath)!);
NativeLibrary.SetDllImportResolver(typeof(Reader).Assembly,
    (name, assembly, searchPath) => name == "ImmStrokeReader" ? NativeLibrary.Load(libraryPath) : IntPtr.Zero);
var sourceJson = JObject.Parse(File.ReadAllText(Path.Combine(sourcePath, "Quill.json")));
var sequence = metadataOnly ? ReadMetadata(sourceJson) : QuillSequenceReader.Read(sourcePath);
var sourceLayers = Flatten(sequence.RootLayer, "").ToArray();
var rawRoot = sourceJson["Sequence"]!["RootLayer"]!;
var rawLayerTypes = RawLayers(rawRoot).GroupBy(x => (string)x["Type"]!).ToDictionary(x => x.Key, x => x.Count());
Require(Reader.StrokeReader_Init(Path.ChangeExtension(reportPath, ".native.log")) == 0, "Native init failed.");
int document = 0;
try
{
    // Memory loading also supports paths that the native file loader cannot open.
    byte[] bytes = File.ReadAllBytes(referencePath);
    var pinned = GCHandle.Alloc(bytes, GCHandleType.Pinned);
    try { document = Reader.StrokeReader_LoadFromMemory(pinned.AddrOfPinnedObject(), bytes.Length); }
    finally { pinned.Free(); }
    Require(document > 0, "Reference IMM load failed.");
    Require(Reader.StrokeReader_GetDocumentInfo(document, out var info), "Document metadata query failed.");
    var nativeLayers = new List<NativeLayer>();
    for (int index = 0; index < Reader.StrokeReader_GetAuthoringLayerCount(document); index++)
    {
        Require(Reader.StrokeReader_GetAuthoringLayerInfo(document, index, out var layer), $"Layer {index} query failed.");
        Require(Reader.StrokeReader_GetAuthoringLayerTransform(document, index, out var local, out _), $"Layer {index} transform failed.");
        nativeLayers.Add(new NativeLayer(index, layer, local));
    }
    foreach (var layer in nativeLayers) layer.Path = NativePath(layer, nativeLayers);
    string rootPath = $"/{sequence.RootLayer.Name}";
    var comparisons = new List<object>();
    foreach (var source in sourceLayers.Where(x => x.Layer is LayerPaint))
    {
        string path = source.Path[rootPath.Length..];
        var matches = nativeLayers.Where(x => x.Info.type == 1 && x.Path == path).ToArray();
        bool uniqueSource = sourceLayers.Count(x => x.Path == source.Path) == 1;
        if (matches.Length != 1 || !uniqueSource)
        {
            comparisons.Add(new { source.Path, source.EffectivelyVisible, matched = false, reason = "Hierarchy path absent or ambiguous; no geometry correspondence assumed." });
            continue;
        }
        comparisons.Add(Compare(document, source, matches[0], metadataOnly));
    }
    var report = new
    {
        schema = "quill-imm-reference-inspection-v1",
        metadataOnly,
        correspondence = "Unique hierarchy paths excluding the source root, then stored drawing/stroke/point indices. Not proof of export provenance or visual parity. Point comparisons skip strokes with differing point counts.",
        inputs = new
        {
            source = Path.GetFileName(sourcePath),
            jsonSha256 = Hash(Path.Combine(sourcePath, "Quill.json")),
            qbinSha256 = Hash(Path.Combine(sourcePath, "Quill.qbin")),
            reference = Path.GetFileName(referencePath), referenceSha256 = Hash(referencePath),
            librarySha256 = Hash(libraryPath)
        },
        source = new
        {
            sequence.Framerate, sequence.BackgroundColor, sequence.DefaultViewpoint,
            sequence.ExportStart, sequence.ExportEnd, sequence.TrackingOrigin, sequence.AnimateOnStart,
            rawLayerTypes,
            counts = sourceLayers.GroupBy(x => x.Layer.Type.ToString()).ToDictionary(x => x.Key, x => x.Count()),
            layers = sourceLayers.Select(x => new
            {
                x.Path, x.EffectivelyVisible, type = x.Layer.Type.ToString(), x.Layer.Name, x.Layer.Visible, x.Layer.Opacity,
                x.Layer.Transform, x.Layer.Pivot, x.Layer.Animation,
                paint = x.Layer is LayerPaint p ? new
                {
                    p.Framerate, p.MaxRepeatCount, drawings = p.Drawings.Count, p.Frames,
                    brushes = metadataOnly ? null : p.Drawings.SelectMany(d => d.Data.Strokes).GroupBy(s => s.BrushType.ToString()).ToDictionary(g => g.Key, g => g.Count()),
                    strokes = metadataOnly ? (int?)null : p.Drawings.Sum(d => d.Data.Strokes.Count),
                    points = metadataOnly ? (int?)null : p.Drawings.Sum(d => d.Data.Strokes.Sum(s => s.Vertices.Count))
                } : null
            })
        },
        native = new
        {
            document = info,
            counts = nativeLayers.GroupBy(x => x.Info.type).ToDictionary(x => x.Key, x => x.Count()),
            layers = nativeLayers.Select(x => new { x.Index, x.Path, info = x.Info, transform = x.Transform,
                animationKeys = NativeKeys(document, x.Index) })
        },
        layerComparisons = sourceLayers.Where(x => x.Layer != sequence.RootLayer).Select(x =>
        {
            var matches = nativeLayers.Where(n => n.Path == x.Path[rootPath.Length..]).ToArray();
            bool uniqueSource = sourceLayers.Count(s => s.Path == x.Path) == 1;
            var match = matches.Length == 1 && uniqueSource ? matches[0] : null;
            return new { x.Path, x.EffectivelyVisible, matched = match != null,
                transformMaxError = match != null ? (double?)TransformError(x.Layer.Transform, match.Transform) : null,
                firstTransformKeyMaxError = match != null && x.Layer.Animation.Keys.Transform.Count > 0
                    ? (double?)TransformError(x.Layer.Animation.Keys.Transform[0].Value, match.Transform) : null,
                sourceFlip = x.Layer.Transform.Flip, nativeFlip = match?.Transform.flip,
                sourcePivot = x.Layer.Pivot, nativeInfo = match?.Info };
        }),
        paintComparisons = comparisons
    };
    File.WriteAllText(reportPath, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true, IncludeFields = true }));
    Console.WriteLine($"[IMM_QUILL_REFERENCE] {Path.GetFileName(sourcePath)}: {sourceLayers.Length} source layers, {nativeLayers.Count} IMM authoring layers; report written.");
}
finally
{
    if (document > 0) Reader.StrokeReader_Unload(document);
    Reader.StrokeReader_End();
}

static object Compare(int document, SourceLayer source, NativeLayer native, bool metadataOnly)
{
    var paint = (LayerPaint)source.Layer;
    int drawingCount = Reader.StrokeReader_GetAuthoringDrawingCount(document, native.Index);
    var errors = new Dictionary<string, double>();
    var compared = new Dictionary<string, long>();
    var shapeIssues = new List<string>();
    var brushPairs = new Dictionary<string, int>();
    var widthRatios = new List<double>();
    int points = 0, strokes = 0;
    if (drawingCount != paint.Drawings.Count)
        shapeIssues.Add($"Drawing counts differ: source {paint.Drawings.Count}, IMM {drawingCount}.");
    for (int d = 0; !metadataOnly && d < Math.Min(drawingCount, paint.Drawings.Count); d++)
    {
        var sourceStrokes = paint.Drawings[d].Data.Strokes;
        int strokeCount = Reader.StrokeReader_GetAuthoringStrokeCount(document, native.Index, d);
        if (strokeCount != sourceStrokes.Count)
        {
            shapeIssues.Add($"Drawing {d} stroke counts differ: source {sourceStrokes.Count}, IMM {strokeCount}; skipped point comparison.");
            continue;
        }
        for (int s = 0; s < strokeCount; s++)
        {
            var stroke = sourceStrokes[s];
            Require(Reader.StrokeReader_GetAuthoringStrokeInfo(document, native.Index, d, s, out var info), "Stroke query failed.");
            string brushPair = $"{stroke.BrushType}->{info.brushType}";
            brushPairs[brushPair] = brushPairs.GetValueOrDefault(brushPair) + 1;
            Error("visibilityMode", stroke.DisableRotationalOpacity ? 1 : 0, info.visibilityMode);
            if (stroke.Vertices.Count != info.numPoints)
            {
                shapeIssues.Add($"Drawing {d} stroke {s} point counts differ: source {stroke.Vertices.Count}, IMM {info.numPoints}.");
                continue;
            }
            var target = new StrokeAuthoringPoint[info.numPoints];
            Require(Reader.StrokeReader_GetAuthoringStrokePoints(document, native.Index, d, s,
                target, target.Length), "Point query failed.");
            for (int p = 0; p < target.Length; p++)
            {
                var a = stroke.Vertices[p];
                var b = target[p].legacy;
                Error("position", a.Position.X, b.px); Error("position", a.Position.Y, b.py); Error("position", a.Position.Z, b.pz);
                Error("normal", a.Normal.X, b.nx); Error("normal", a.Normal.Y, b.ny); Error("normal", a.Normal.Z, b.nz);
                if (!stroke.DisableRotationalOpacity)
                {
                    Error("rotationalOpacityDirection", a.Tangent.X, b.dx);
                    Error("rotationalOpacityDirection", a.Tangent.Y, b.dy);
                    Error("rotationalOpacityDirection", a.Tangent.Z, b.dz);
                }
                Error("colorDirect", a.Color.R, b.r); Error("colorDirect", a.Color.G, b.g); Error("colorDirect", a.Color.B, b.b);
                Error("colorSrgbToLinear", Linear(a.Color.R), b.r); Error("colorSrgbToLinear", Linear(a.Color.G), b.g); Error("colorSrgbToLinear", Linear(a.Color.B), b.b);
                Error("opacity", a.Opacity, b.alpha); Error("widthDirect", a.Width, b.width);
                if (a.Width != 0) widthRatios.Add(b.width / a.Width);
            }
            points += target.Length;
            strokes++;
        }
    }
    Require(Reader.StrokeReader_GetAuthoringLayerAnimationInfo(document, native.Index, out int rate, out int frames, out int repeat), "Paint animation query failed.");
    var frameBuffer = new int[frames];
    Require(Reader.StrokeReader_GetAuthoringFrameBuffer(document, native.Index, frameBuffer, frames) == frames, "Frame query failed.");
    return new
    {
        source.Path, matched = true, nativeIndex = native.Index,
        sourceFrameRate = paint.Framerate, sourceRepeatCount = paint.MaxRepeatCount,
        sourceDrawings = paint.Drawings.Count, nativeDrawings = drawingCount,
        comparedStrokes = strokes, comparedPoints = points, shapeIssues, brushPairs,
        maxAbsoluteError = errors, comparedComponents = compared,
        widthRatioMin = widthRatios.Count > 0 ? (double?)widthRatios.Min() : null,
        widthRatioMax = widthRatios.Count > 0 ? (double?)widthRatios.Max() : null,
        frameRate = rate, repeatCount = repeat, frameBuffer,
        sourceFramesEqual = paint.Frames.SequenceEqual(frameBuffer),
        staticProperties = new
        {
            transformMaxError = TransformError(source.Layer.Transform, native.Transform),
            pivotMaxError = TransformError(source.Layer.Pivot, new StrokeLayerTransform
            {
                rotX = native.Info.pivotRotX, rotY = native.Info.pivotRotY, rotZ = native.Info.pivotRotZ, rotW = native.Info.pivotRotW,
                scale = native.Info.pivotScale, transX = native.Info.pivotTransX, transY = native.Info.pivotTransY, transZ = native.Info.pivotTransZ
            }),
            sourceVisible = source.Layer.Visible, nativeVisible = native.Info.visible,
            opacityError = Math.Abs(source.Layer.Opacity - native.Info.opacity),
            nativeFlip = native.Transform.flip, nativePivotFlip = native.Info.pivotFlip
        }
    };

    void Error(string key, double a, double b)
    {
        Require(double.IsFinite(a) && double.IsFinite(b), $"Nonfinite {key} value.");
        errors[key] = Math.Max(errors.GetValueOrDefault(key), Math.Abs(a - b));
        compared[key] = compared.GetValueOrDefault(key) + 1;
    }
}

static double TransformError(SharpQuill.Transform a, StrokeLayerTransform b) => new double[]
{
    Math.Abs(a.Translation.X - b.transX), Math.Abs(a.Translation.Y - b.transY), Math.Abs(a.Translation.Z - b.transZ),
    Math.Abs(a.Scale - b.scale),
    // q and -q represent the same orientation.
    Math.Min(new double[] { Math.Abs(a.Rotation.X - b.rotX), Math.Abs(a.Rotation.Y - b.rotY), Math.Abs(a.Rotation.Z - b.rotZ), Math.Abs(a.Rotation.W - b.rotW) }.Max(),
        new double[] { Math.Abs(a.Rotation.X + b.rotX), Math.Abs(a.Rotation.Y + b.rotY), Math.Abs(a.Rotation.Z + b.rotZ), Math.Abs(a.Rotation.W + b.rotW) }.Max())
}.Max();

static IEnumerable<SourceLayer> Flatten(Layer layer, string path, bool parentVisible = true)
{
    string current = $"{path}/{layer.Name}";
    bool visible = parentVisible && layer.Visible;
    yield return new SourceLayer(current, layer, visible);
    if (layer is LayerGroup group)
        foreach (var child in group.Children)
            foreach (var item in Flatten(child, current, visible)) yield return item;
}
static IEnumerable<JToken> RawLayers(JToken layer)
{
    yield return layer;
    if ((string)layer["Type"] == "Group")
        foreach (var child in layer["Implementation"]!["Children"]!)
            foreach (var item in RawLayers(child)) yield return item;
}
static Sequence ReadMetadata(JObject original)
{
    // Exercise the production reader on metadata with empty attached data, as the
    // reader regression harness does. Restore drawing declarations/frame references
    // afterward, without allocating any source vertices or media pixels.
    var metadata = (JObject)original.DeepClone();
    foreach (var layer in RawLayers(metadata["Sequence"]!["RootLayer"]!))
    {
        var impl = (JObject)layer["Implementation"]!;
        if ((string)layer["Type"] == "Paint")
        {
            impl["Drawings"] = new JArray(); impl["Frames"] = new JArray();
        }
        else if ((string)layer["Type"] is "Picture" or "Sound") impl["DataFileOffset"] = "-1";
    }
    string temporary = Path.Combine(Path.GetTempPath(), $"imm-quill-reference-{Guid.NewGuid():N}");
    Directory.CreateDirectory(temporary);
    try
    {
        File.WriteAllText(Path.Combine(temporary, "Quill.json"), metadata.ToString());
        File.WriteAllBytes(Path.Combine(temporary, "Quill.qbin"), new byte[4]);
        var sequence = QuillSequenceReader.Read(temporary);
        var raw = RawLayers(original["Sequence"]!["RootLayer"]!).Where(x => (string)x["Type"] == "Paint").ToArray();
        var paint = Flatten(sequence.RootLayer, "").Where(x => x.Layer is LayerPaint).Select(x => (LayerPaint)x.Layer).ToArray();
        Require(raw.Length == paint.Length, "Metadata paint traversal mismatch.");
        for (int i = 0; i < paint.Length; i++)
        {
            var impl = raw[i]["Implementation"]!;
            int count = impl["Drawings"] is JArray drawings ? drawings.Count : 1;
            paint[i].Drawings = Enumerable.Range(0, count).Select(_ => new Drawing()).ToList();
            paint[i].Frames = impl["Frames"]?.ToObject<List<int>>() ?? new List<int> { 0 };
        }
        return sequence;
    }
    finally
    {
        // Only these two files, in the uniquely created temporary directory, exist.
        File.Delete(Path.Combine(temporary, "Quill.json"));
        File.Delete(Path.Combine(temporary, "Quill.qbin"));
        Directory.Delete(temporary);
    }
}
static string NativePath(NativeLayer layer, List<NativeLayer> layers)
{
    var names = new List<string>();
    var visited = new HashSet<int>();
    while (true)
    {
        Require(visited.Add(layer.Info.id), "Native hierarchy contains a cycle.");
        names.Add(layer.Info.name);
        if (layer.Info.parentId == 0) break;
        layer = layers.Single(x => x.Info.id == layer.Info.parentId);
    }
    names.Reverse();
    return $"/{string.Join('/', names)}";
}
static StrokeAnimationKey[] NativeKeys(int document, int layer)
{
    var result = new StrokeAnimationKey[Reader.StrokeReader_GetLayerAnimationKeyCount(document, layer)];
    for (int i = 0; i < result.Length; i++)
        Require(Reader.StrokeReader_GetLayerAnimationKey(document, layer, i, out result[i]), "Key query failed.");
    return result;
}
static double Linear(double value) => value <= 0.04045 ? value / 12.92 : Math.Pow((value + 0.055) / 1.055, 2.4);
static string Hash(string path)
{
    using var stream = File.OpenRead(path);
    return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
}
static void Require(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
record SourceLayer(string Path, Layer Layer, bool EffectivelyVisible);
record NativeLayer(int Index, StrokeAuthoringLayerInfo Info, StrokeLayerTransform Transform)
{
    public string Path { get; set; } = "";
}
