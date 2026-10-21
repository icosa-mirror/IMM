using ImmPlayer;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;
using Reader = ImmPlayer.ImmStrokeReader;

static class PaintRepeatChecks
{
    public static void Run(string root)
    {
        Directory.CreateDirectory(root);
        string path = Path.Combine(root, "paint-repeat-counts.imm");
        using (var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 24, new Color(0, 0, 0, 1)).Value)
        {
            foreach (uint count in new uint[] { 0, 1, 3 })
            {
                var properties = ImmAuthoringLayerProperties.Default($"Repeat{count}");
                properties.PaintMaxRepeatCount = count;
                properties.MaxRepeatCount = 7;
                long layer = document.CreatePaintLayer(0, properties).Value;
                long drawing = document.CreateDrawing(layer).Value;
                var a = new PaintPoint { Position = Vector3.zero, Normal = Vector3.up, Color = new Color(1, 1, 1, 1), Alpha = 1, Width = 0.01f };
                var b = a; b.Position = new Vector3(1, 0, 0);
                Require(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, new[] { a, b }).Succeeded, "Stroke creation failed.");
                Require(document.AppendFrame(layer, drawing).Succeeded, "Frame creation failed.");
            }
            var exported = ImmAuthoringCompiler.ExportToFile(document, path);
            Require(exported.Succeeded, exported.Message);
        }
        Require(Reader.StrokeReader_Init(Path.ChangeExtension(path, ".log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(path);
        try
        {
            Require(doc > 0, "Repeat fixture failed to load.");
            int[] expected = { 0, 1, 3 };
            for (int i = 0; i < expected.Length; i++)
            {
                Require(Reader.StrokeReader_GetAuthoringLayerAnimationInfo(doc, i, out _, out _, out int count) && count == expected[i],
                    $"Paint repeat count {expected[i]} read back as {count}.");
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, i, out var info) && info.maxRepeatCount == 7,
                    "Paint repeat setter changed the independent timeline repeat count.");
            }
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }
        Console.WriteLine("[IMM_PAINT_REPEAT_TEST] Passed infinite/one/finite drawing repeats with independent timeline repeats.");
    }

    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
