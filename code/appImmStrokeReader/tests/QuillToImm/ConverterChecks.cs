using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using Newtonsoft.Json.Linq;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

static class ConverterChecks
{
    public static void Run(string outputDirectory)
    {
        string root = Path.GetFullPath(outputDirectory);
        Directory.CreateDirectory(root);
        string project = Path.Combine(root, "synthetic-source");
        if (Directory.Exists(project)) throw new IOException("Choose a fresh test artifact directory.");
        Directory.CreateDirectory(project);
        string output = Path.Combine(root, "synthetic.imm");
        var sequence = Sequence.CreateDefault();
        sequence.RootLayer.Children.Clear();
        var group = new LayerGroup { Name = "Group", Transform = SharpQuill.Transform.Identity,
            Pivot = new SharpQuill.Transform(new SharpQuill.Quaternion(0, 0, 0, 1), 1, "N", new SharpQuill.Vector3(1, 2, 3)) };
        group.Transform = new SharpQuill.Transform(new SharpQuill.Quaternion(0, 0, 0, 1), 2, "N", new SharpQuill.Vector3(3, 4, 5));
        var paint = new LayerPaint("Paint") { MaxRepeatCount = 1 };
        var drawing = new Drawing();
        foreach (BrushType brush in new[] { BrushType.Ribbon, BrushType.Cylinder, BrushType.Ellipse, BrushType.Cube })
        {
            var stroke = new Stroke { BrushType = brush, DisableRotationalOpacity = brush != BrushType.Ribbon };
            for (int i = 0; i < 3; i++)
                stroke.Vertices.Add(new Vertex(new SharpQuill.Vector3(i, 0, 0), new SharpQuill.Vector3(0, 1, 0),
                    new SharpQuill.Vector3(0, 0, 1), new SharpQuill.Color(0.2f, 0.4f, 0.6f), 1, i == 1 ? 0.02f : 0));
            stroke.UpdateBoundingBox();
            drawing.Data.Strokes.Add(stroke);
        }
        drawing.UpdateBoundingBox(false);
        paint.Drawings.Add(drawing);
        paint.Frames.Add(0);
        group.Children.Add(paint);
        sequence.RootLayer.Children.Add(group);
        QuillSequenceWriter.Write(sequence, project);
        string initialJsonPath = Path.Combine(project, "Quill.json");
        var heldFrames = JObject.Parse(File.ReadAllText(initialJsonPath));
        heldFrames["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!["Implementation"]!["Children"]![0]!["Implementation"]!["Frames"] = new JArray("0", "0");
        File.WriteAllText(initialJsonPath, heldFrames.ToString());
        var baseline = QuillToImmConverter.ConvertStaticPaintProject(project, output);
        Require(baseline.Succeeded && !baseline.IsPartial, Describe(baseline));
        Require(baseline.Export.Statistics.StrokeCount == 4 && baseline.Export.Statistics.PointCount == 12, "Four brush sections were not exported.");
        NativeFixture(output);
        CheckZeroWidthImportRoundTrip(output, root);
        RootKeyChecks.Run(output, root);

        string jsonFile = Path.Combine(project, "Quill.json");
        string json = File.ReadAllText(jsonFile);
        string binaryFile = Path.Combine(project, "Quill.qbin");
        byte[] binary = File.ReadAllBytes(binaryFile);
        var parsed = JObject.Parse(json);
        var sourceGroup = (JObject)parsed["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!;
        sourceGroup["Transform"]!["Flip"] = "UnknownAxis";
        File.WriteAllText(jsonFile, parsed.ToString());
        RejectPreservingOutput("UnsupportedReflection");

        foreach (string axis in new[] { "X", "Y", "Z" })
        {
            parsed = JObject.Parse(json);
            sourceGroup = (JObject)parsed["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!;
            sourceGroup["Transform"]!["Flip"] = axis;
            sourceGroup["Pivot"]!["Flip"] = axis;
            File.WriteAllText(jsonFile, parsed.ToString());
            string reflectedOutput = Path.Combine(root, $"quill-flip-{axis}.imm");
            var reflected = QuillToImmConverter.ConvertStaticPaintProject(project, reflectedOutput);
            Require(reflected.Succeeded && !reflected.IsPartial, Describe(reflected));
            ReflectionChecks.AssertFirstLayerFlip(reflectedOutput, (int)Enum.Parse<ImmAuthoringFlip>(axis));
        }

        parsed = JObject.Parse(json);
        sourceGroup = (JObject)parsed["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!;
        sourceGroup["Animation"]!["Keys"]!["Opacity"] = new JArray(new JObject { ["Time"] = 100, ["Value"] = 0.5, ["Interpolation"] = "Linear" });
        File.WriteAllText(jsonFile, parsed.ToString());
        RejectPreservingOutput("UnsupportedAnimation");

        parsed = JObject.Parse(json);
        ((JArray)parsed["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]!).Add(new JObject
        { ["Name"] = "Future", ["Type"] = "FutureLayer", ["Implementation"] = new JObject() });
        File.WriteAllText(jsonFile, parsed.ToString());
        RejectPreservingOutput("OmittedLayer");
        var partial = QuillToImmConverter.ConvertStaticPaintProject(project, output, allowOmittedLayers: true);
        Require(partial.Succeeded && partial.IsPartial && partial.Issues.Any(x => x.Code == "OmittedLayer"), Describe(partial));

        File.WriteAllText(jsonFile, json);
        var originalPaint = (LayerPaint)QuillSequenceReader.ReadMetadata(project).RootLayer.Children.OfType<LayerGroup>().Single().Children.Single();
        long brushOffset = originalPaint.Drawings.Single().DataFileOffset + 4 + 32;
        byte[] corrupted = (byte[])binary.Clone();
        BitConverter.GetBytes((short)99).CopyTo(corrupted, (int)brushOffset);
        File.WriteAllBytes(binaryFile, corrupted);
        RejectPreservingOutput("ConversionFailed");
        using (var legacyReader = new QBinReader(new MemoryStream(corrupted)))
        {
            legacyReader.BaseStream.Position = originalPaint.Drawings.Single().DataFileOffset;
            Require(legacyReader.ReadDrawingData().Strokes[0].BrushType == BrushType.Cylinder, "Legacy non-strict brush fallback changed.");
        }
        File.WriteAllBytes(binaryFile, binary);
        var overwriteSource = QuillToImmConverter.ConvertStaticPaintProject(project, binaryFile);
        Require(!overwriteSource.Succeeded && File.ReadAllBytes(binaryFile).SequenceEqual(binary), "Source overwrite guard failed.");

        using (var document = ImmAuthoringDocument.Create(ExportSequenceType.Still, 24, new UnityEngine.Color(0, 0, 0, 1)).Value)
        {
            long layer = document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("P")).Value;
            long d = document.CreateDrawing(layer).Value;
            var point = new PaintPoint { Width = -1, Alpha = 1 };
            Require(!document.CreateStroke(d, BrushSectionType.Circle, VisibilityType.Always, new[] { point, point }).Succeeded, "Negative width was accepted.");
            point.Width = 0;
            Require(!document.CreateStroke(d, BrushSectionType.Circle, VisibilityType.Always, new[] { point, point }).Succeeded, "Zero-extent stroke was accepted.");
        }
        ReflectionChecks.Run(root);
        AnimatedConverterChecks.Run(root, sequence);
        Console.WriteLine("[IMM_QUILL_CONVERTER_TEST] Passed native four-brush/zero-tip/hierarchy/pivot checks and rejection/output-preservation cases.");

        void RejectPreservingOutput(string code)
        {
            byte[] marker = { 1, 2, 3, 4 };
            File.WriteAllBytes(output, marker);
            var result = QuillToImmConverter.ConvertStaticPaintProject(project, output);
            Require(!result.Succeeded && result.Issues.Any(x => x.Code == code), Describe(result));
            Require(File.ReadAllBytes(output).SequenceEqual(marker), "Rejected conversion changed existing output.");
        }
    }

    private static void CheckZeroWidthImportRoundTrip(string path, string root)
    {
        var imported = ImmAuthoringImporter.ImportFromFile(path, Path.Combine(root, "zero-tip-import.log"));
        Require(imported.Succeeded, "Zero-tip fixture import failed.");
        using var document = imported.Document;
        var strokes = document.CreateSnapshot().Value.Layers.SelectMany(layer => layer.Drawings)
            .SelectMany(drawing => drawing.Strokes).ToArray();
        Require(strokes.Length == 4, "Import changed the four-brush fixture stroke count.");
        foreach (var stroke in strokes)
            Require(stroke.Points.Count == 3 && stroke.Points[0].Width == 0 &&
                stroke.Points[2].Width == 0 && stroke.Points[1].Width > 0,
                "Importer changed valid zero-width tips.");
        Require(!imported.Issues.Any(issue => issue.Code == ImmAuthoringImportIssueCode.InvalidSourceValue &&
            issue.Message.Contains("Width")), "Valid zero widths were diagnosed as invalid metadata.");
        string roundTrip = Path.Combine(root, "zero-tip-import-roundtrip.imm");
        var exported = ImmAuthoringCompiler.ExportToFile(document, roundTrip);
        Require(exported.Succeeded, exported.Message);
        NativeFixture(roundTrip);
        Console.WriteLine("[IMM_QUILL_IMPORT_TEST] Passed four-brush zero-width tip export/import/export checks.");
    }

    private static void NativeFixture(string path)
    {
        Require(Reader.StrokeReader_Init(Path.ChangeExtension(path, ".log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 2, "Native layer count differs.");
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var group), "Group info missing.");
            Require(group.pivotTransX == 1 && group.pivotTransY == 2 && group.pivotTransZ == 3, "Group pivot was lost.");
            Require(Reader.StrokeReader_GetAuthoringLayerTransform(doc, 0, out var local, out _) &&
                local.scale == 2 && local.transX == 3 && local.transY == 4 && local.transZ == 5, "Local group transform was lost.");
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, 1, out var paint) && paint.parentId == group.id, "Hierarchy was lost.");
            Require(Reader.StrokeReader_GetAuthoringLayerAnimationInfo(doc, 1, out int rate, out int frames, out int repeat) &&
                rate == 24 && frames == 2 && repeat == 1, "Held-frame metadata was lost.");
            var frameBuffer = new int[2];
            Require(Reader.StrokeReader_GetAuthoringFrameBuffer(doc, 1, frameBuffer, 2) == 2 && frameBuffer.SequenceEqual(new[] { 0, 0 }),
                "String-valued held frame references were changed.");
            for (int s = 0; s < 4; s++)
            {
                Require(Reader.StrokeReader_GetAuthoringStrokeInfo(doc, 1, 0, s, out var info) && info.brushType == s + 1, "Brush mapping changed.");
                var points = new StrokeAuthoringPoint[info.numPoints];
                Require(Reader.StrokeReader_GetAuthoringStrokePoints(doc, 1, 0, s, points, points.Length), "Point query failed.");
                Require(points[0].legacy.width == 0 && points[^1].legacy.width == 0 && points[1].legacy.width > 0, "Zero-width tips were changed.");
                Require(info.visibilityMode == (s == 0 ? 0 : 1), "Rotational opacity was changed.");
            }
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
    }

    public static void ComparePaint(string reference, string actual, bool compareAnimation = false, bool compareViewpoints = false)
    {
        Require(Reader.StrokeReader_Init(Path.ChangeExtension(actual, ".comparison.log")) == 0, "Reader init failed.");
        int a = Reader.StrokeReader_LoadFromFile(Path.GetFullPath(reference));
        int b = Reader.StrokeReader_LoadFromFile(Path.GetFullPath(actual));
        long points = 0;
        int strokes = 0;
        try
        {
            Require(a > 0 && b > 0, "Both IMM files must load.");
            if (compareAnimation)
            {
                Require(Reader.StrokeReader_GetRootLayerProperties(a, out var rootA, out var transformA) &&
                    Reader.StrokeReader_GetRootLayerProperties(b, out var rootB, out var transformB) && rootA.Equals(rootB) && transformA.Equals(transformB),
                    "Reader-exposed root metadata differs.");
                Require(Reader.StrokeReader_GetDocumentInfo(a, out var docA) && Reader.StrokeReader_GetDocumentInfo(b, out var docB) &&
                    docA.rootAnimationKeyCount == docB.rootAnimationKeyCount, "Root key counts differ.");
                for (int index = 0; index < docA.rootAnimationKeyCount; index++)
                    Require(Reader.StrokeReader_GetRootAnimationKey(a, index, out var ak) &&
                        Reader.StrokeReader_GetRootAnimationKey(b, index, out var bk) && ak.Equals(bk), $"Stored root key differs at {index}.");
            }
            var source = Layers(a);
            var target = Layers(b);
            var sourcePaint = source.Where(x => x.Value.Info.type == 1).ToArray();
            Require(source.Count(x => x.Value.Info.type == 0) == target.Count(x => x.Value.Info.type == 0), "Group count differs.");
            Require(sourcePaint.Length == target.Count(x => x.Value.Info.type == 1), "Paint layer count differs.");
            if (compareViewpoints)
            {
                Require(source.Count(x => x.Value.Info.type == 8) == target.Count(x => x.Value.Info.type == 8), "Viewpoint count differs.");
                Require(source.Count(x => x.Value.Info.type == 4) == target.Count(x => x.Value.Info.type == 4), "Picture count differs.");
                Require(source.Count(x => x.Value.Info.type == 5) == target.Count(x => x.Value.Info.type == 5), "Sound count differs.");
            }
            foreach (var (path, layer) in source.Where(x => x.Value.Info.type == 0 || x.Value.Info.type == 1 ||
                (compareViewpoints && (x.Value.Info.type == 8 || x.Value.Info.type == 4 || x.Value.Info.type == 5))))
            {
                Require(target.TryGetValue(path, out var other), $"Missing hierarchy path {path}.");
                Require(layer.Info.type == other.Info.type && layer.Info.visible == other.Info.visible && layer.Info.opacity == other.Info.opacity &&
                    layer.Transform.Equals(other.Transform), $"Layer metadata differs at {path}.");
                Require(layer.Info.pivotTransX == other.Info.pivotTransX && layer.Info.pivotTransY == other.Info.pivotTransY &&
                    layer.Info.pivotTransZ == other.Info.pivotTransZ && layer.Info.pivotRotX == other.Info.pivotRotX &&
                    layer.Info.pivotRotY == other.Info.pivotRotY && layer.Info.pivotRotZ == other.Info.pivotRotZ &&
                    layer.Info.pivotRotW == other.Info.pivotRotW && layer.Info.pivotScale == other.Info.pivotScale &&
                    layer.Info.pivotFlip == other.Info.pivotFlip, $"Pivot differs at {path}.");
                if (compareViewpoints && layer.Info.type == 8)
                {
                    Require(layer.Info.isDefaultSpawn == other.Info.isDefaultSpawn &&
                        Reader.StrokeReader_GetLayerSpawnAreaInfo(a, layer.Index, out var spawnA) &&
                        Reader.StrokeReader_GetLayerSpawnAreaInfo(b, other.Index, out var spawnB) && spawnA.Equals(spawnB),
                        $"Viewpoint tracking/volume/locomotion/default differs at {path}.");
                    bool previewA = Reader.StrokeReader_GetSpawnAreaPreviewInfo(a, layer.Index, out var imageA);
                    bool previewB = Reader.StrokeReader_GetSpawnAreaPreviewInfo(b, other.Index, out var imageB);
                    Require(previewA == previewB && (!previewA ||
                        (imageA.width == imageB.width && imageA.height == imageB.height && imageA.channels == imageB.channels &&
                         imageA.encoding == imageB.encoding && imageA.sourceVersion == imageB.sourceVersion)),
                        $"Viewpoint preview presence/dimensions/encoding/color order differs at {path}.");
                    // Raw Quill previews are JPEG-encoded by the compiler. Byte/pixel fidelity
                    // needs a separate codec comparison; import/re-export checks require exact bytes.
                }
                if (compareViewpoints && layer.Info.type == 4)
                {
                    Require(Reader.StrokeReader_GetPictureAssetInfo(a, layer.Index, out var imageA) &&
                        Reader.StrokeReader_GetPictureAssetInfo(b, other.Index, out var imageB) &&
                        imageA.contentType == imageB.contentType && imageA.isViewerLocked == imageB.isViewerLocked &&
                        imageA.image.width == imageB.image.width && imageA.image.height == imageB.image.height &&
                        PicturePixels(a, layer.Info.id).SequenceEqual(PicturePixels(b, other.Info.id)),
                        $"Picture projection/viewer-lock/dimensions/decoded pixels differ at {path}.");
                }
                if (compareViewpoints && layer.Info.type == 5)
                {
                    Require(Reader.StrokeReader_GetSoundAssetInfo(a, layer.Index, out var soundA) &&
                        Reader.StrokeReader_GetSoundAssetInfo(b, other.Index, out var soundB) &&
                        soundA.properties.Equals(soundB.properties) && soundA.channels == soundB.channels &&
                        soundA.sourceDuration == soundB.sourceDuration && soundA.initiallyPlaying == soundB.initiallyPlaying &&
                        soundA.sampleRate > 0 && soundB.sampleRate > 0 && soundA.byteCount > 0 && soundB.byteCount > 0,
                        $"Sound settings/channels/legacy metadata differ or asset is unsupported at {path}.");
                }
                if (compareAnimation)
                {
                    Require(layer.Info.isTimeline == other.Info.isTimeline && layer.Info.durationTicks == other.Info.durationTicks &&
                        layer.Info.maxRepeatCount == other.Info.maxRepeatCount, $"Timeline metadata differs at {path}.");
                    int keyCount = Reader.StrokeReader_GetLayerAnimationKeyCount(a, layer.Index);
                    Require(keyCount == Reader.StrokeReader_GetLayerAnimationKeyCount(b, other.Index), $"Animation key count differs at {path}.");
                    for (int k = 0; k < keyCount; k++)
                    {
                        Require(Reader.StrokeReader_GetLayerAnimationKey(a, layer.Index, k, out var ak) &&
                            Reader.StrokeReader_GetLayerAnimationKey(b, other.Index, k, out var bk) && ak.Equals(bk),
                            $"Stored animation key differs at {path}/{k}.");
                    }
                }
                if (layer.Info.type != 1) continue;
                int drawings = Reader.StrokeReader_GetAuthoringDrawingCount(a, layer.Index);
                Require(drawings == Reader.StrokeReader_GetAuthoringDrawingCount(b, other.Index), "Drawing count differs.");
                Require(Reader.StrokeReader_GetAuthoringLayerAnimationInfo(a, layer.Index, out int ar, out int af, out int ac) &&
                    Reader.StrokeReader_GetAuthoringLayerAnimationInfo(b, other.Index, out int br, out int bf, out int bc) && ar == br && af == bf && ac == bc,
                    $"Paint animation metadata differs at {path}.");
                var framesA = new int[af]; var framesB = new int[af];
                Require(Reader.StrokeReader_GetAuthoringFrameBuffer(a, layer.Index, framesA, af) == af &&
                    Reader.StrokeReader_GetAuthoringFrameBuffer(b, other.Index, framesB, af) == af && framesA.SequenceEqual(framesB), "Frame buffers differ.");
                for (int d = 0; d < drawings; d++)
                {
                    if (compareAnimation)
                    {
                        float scaleA = DrawingScale(a, layer.Info.id, d), scaleB = DrawingScale(b, other.Info.id, d);
                        Require(scaleA == scaleB, $"Drawing quantization scale differs at {path}/{d}: {scaleA:R} versus {scaleB:R}.");
                    }
                    int count = Reader.StrokeReader_GetAuthoringStrokeCount(a, layer.Index, d);
                    Require(count == Reader.StrokeReader_GetAuthoringStrokeCount(b, other.Index, d), "Stroke count differs.");
                    for (int s = 0; s < count; s++)
                    {
                        Require(Reader.StrokeReader_GetAuthoringStrokeInfo(a, layer.Index, d, s, out var ai) &&
                            Reader.StrokeReader_GetAuthoringStrokeInfo(b, other.Index, d, s, out var bi) && ai.numPoints == bi.numPoints &&
                            ai.brushType == bi.brushType && ai.visibilityMode == bi.visibilityMode, $"Stroke shape differs at {path}/{d}/{s}.");
                        var ap = new StrokeAuthoringPoint[ai.numPoints]; var bp = new StrokeAuthoringPoint[ai.numPoints];
                        Require(Reader.StrokeReader_GetAuthoringStrokePoints(a, layer.Index, d, s, ap, ap.Length) &&
                            Reader.StrokeReader_GetAuthoringStrokePoints(b, other.Index, d, s, bp, bp.Length), "Point query failed.");
                        for (int p = 0; p < ap.Length; p++)
                        {
                            Require(ap[p].legacy.Equals(bp[p].legacy), $"Decoded point differs at {path}/{d}/{s}/{p}.");
                            if (compareAnimation)
                                Require(ap[p].length == bp[p].length && ap[p].time == bp[p].time,
                                    $"Decoded point length/time differs at {path}/{d}/{s}/{p}.");
                        }
                        points += ap.Length; strokes++;
                    }
                }
            }
            Console.WriteLine($"[IMM_QUILL_CONVERTER_COMPARE] Exact paint subset match: {sourcePaint.Length} layers, {strokes} strokes, {points} points, hierarchy/transforms/pivots/frame buffers.");
            if (compareAnimation) Console.WriteLine("[IMM_QUILL_ANIMATION_COMPARE] Exact reader-exposed root metadata, root/layer keys and nested timing match; evaluated playback not checked.");
            if (compareViewpoints) Console.WriteLine($"[IMM_QUILL_VIEWPOINT_COMPARE] Exact reader-exposed viewpoint transforms/tracking/volumes/locomotion/defaults and preview presence/dimensions/encoding/color order match for {source.Count(x => x.Value.Info.type == 8)} viewpoints; preview pixels/navigation not checked.");
            if (compareViewpoints) Console.WriteLine($"[IMM_QUILL_PICTURE_COMPARE] Exact picture projection/viewer-lock/dimensions/decoded pixels match for {source.Count(x => x.Value.Info.type == 4)} pictures; evaluated playback not checked.");
            if (compareViewpoints) Console.WriteLine($"[IMM_QUILL_SOUND_COMPARE] Exact sound settings/channels/legacy metadata match for {source.Count(x => x.Value.Info.type == 5)} sounds; codec/rate may differ, decoded samples and playback not checked.");
        }
        finally { if (a > 0) Reader.StrokeReader_Unload(a); if (b > 0) Reader.StrokeReader_Unload(b); Reader.StrokeReader_End(); }
    }

    private record NativeLayer(int Index, StrokeAuthoringLayerInfo Info, StrokeLayerTransform Transform);
    private static float DrawingScale(int document, int layerId, int drawing)
    {
        for (int i = 0; i < Reader.StrokeReader_GetLayerCount(document); i++)
            if (Reader.StrokeReader_GetLayerInfo(document, i, out var info) && info.id == layerId)
                return Reader.StrokeReader_GetDrawingBiggestStroke(document, i, drawing);
        throw new InvalidOperationException($"Paint layer {layerId} is absent from the drawing-scale reader view.");
    }
    private static Dictionary<string, NativeLayer> Layers(int doc)
    {
        var all = new List<NativeLayer>();
        for (int i = 0; i < Reader.StrokeReader_GetAuthoringLayerCount(doc); i++)
        {
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, i, out var info), "Layer query failed.");
            Require(Reader.StrokeReader_GetAuthoringLayerTransform(doc, i, out var t, out _), "Transform query failed.");
            all.Add(new NativeLayer(i, info, t));
        }
        string PathOf(NativeLayer layer) => layer.Info.parentId == 0 ? $"/{layer.Info.name}" :
            $"{PathOf(all.Single(x => x.Info.id == layer.Info.parentId))}/{layer.Info.name}";
        return all.ToDictionary(PathOf);
    }
    private static string Describe(QuillToImmConversionResult result) => string.Join("; ", result.Issues.Select(x => $"{x.Code}: {x.Message}"));
    private static byte[] PicturePixels(int doc, int layerId)
    {
        for (int index = 0; index < Reader.StrokeReader_GetLayerCount(doc); index++)
        {
            if (!Reader.StrokeReader_GetLayerInfo(doc, index, out var layer) || layer.id != layerId) continue;
            Require(Reader.StrokeReader_GetPictureInfo(doc, index, out var info), "Decoded picture info missing.");
            byte[] bytes = new byte[info.dataSize];
            var pinned = GCHandle.Alloc(bytes, GCHandleType.Pinned);
            try { Require(Reader.StrokeReader_GetPicturePixelData(doc, index, pinned.AddrOfPinnedObject(), bytes.Length) == bytes.Length, "Decoded picture data missing."); }
            finally { pinned.Free(); }
            return bytes;
        }
        throw new InvalidOperationException($"Picture {layerId} has no decoded legacy-layer entry.");
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
