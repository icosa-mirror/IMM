using ImmPlayer.Authoring;
using Newtonsoft.Json.Linq;
using SharpQuill;
using Reader = ImmPlayer.ImmStrokeReader;

static class AnimatedConverterChecks
{
    public static void Run(string root, Sequence sequence)
    {
        string project = Path.Combine(root, "animated-source");
        Directory.CreateDirectory(project);
        var group = (LayerGroup)sequence.RootLayer.Children.Single();
        var paint = (LayerPaint)group.Children.Single();
        group.Animation.Timeline = true;
        group.Animation.Duration = 126000;
        group.Animation.MaxRepeatCount = 2;
        group.Animation.StartOffset = 525;
        group.Animation.Keys.Offset = new() { new(525, 525, Interpolation.None) };
        // Initially hidden paint must survive conversion: visibility keys can reveal it.
        paint.Visible = false;
        paint.Framerate = 12; // Quill exports use the sequence rate without resampling the map.
        paint.MaxRepeatCount = 3;
        var empty = new Drawing();
        paint.Drawings.Add(empty);
        paint.Frames = new() { 0, 0, 1, 1, 0 };
        paint.Animation.Keys.Visibility = new() { new(525, true, Interpolation.None), new(3150, false, Interpolation.None) };
        paint.Animation.Keys.Opacity = new() { new(0, 0.25f, Interpolation.Linear), new(3150, 0.75f, Interpolation.None) };
        var modes = new[] { Interpolation.None, Interpolation.Linear, Interpolation.Smoothstep, Interpolation.EaseIn, Interpolation.EaseOut };
        paint.Animation.Keys.Transform.Clear();
        for (int i = 0; i < modes.Length; i++)
            paint.Animation.Keys.Transform.Add(new(i * 525, new SharpQuill.Transform(new(0, 0, 0, 1), 1, "X", new(i, 0, 0)), modes[i]));
        QuillSequenceWriter.Write(sequence, project);
        string output = Path.Combine(root, "animated.imm");
        var result = QuillToImmConverter.ConvertAnimatedPaintProject(project, output);
        Require(result.Succeeded && !result.IsPartial, Describe(result));
        Require(Reader.StrokeReader_Init(Path.Combine(root, "animated-reader.log")) == 0, "Reader init failed.");
        int doc = Reader.StrokeReader_LoadFromFile(output);
        try
        {
            Require(doc > 0 && Reader.StrokeReader_GetAuthoringLayerCount(doc) == 2, "Animated hierarchy missing.");
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, 0, out var groupInfo) &&
                groupInfo.isTimeline == 1 && groupInfo.durationTicks == 126000 && groupInfo.maxRepeatCount == 2, "Nested timeline timing changed.");
            Require(Reader.StrokeReader_GetAuthoringLayerAnimationInfo(doc, 1, out int rate, out int frames, out int repeat) &&
                rate == 24 && frames == 5 && repeat == 3, $"Animated frame metadata changed: rate={rate}, frames={frames}, repeat={repeat}.");
            var map = new int[frames];
            Require(Reader.StrokeReader_GetAuthoringFrameBuffer(doc, 1, map, map.Length) == map.Length &&
                map.SequenceEqual(new[] { 0, 0, 1, 1, 0 }), "Held or empty drawing references changed.");
            var keys = Enumerable.Range(0, Reader.StrokeReader_GetLayerAnimationKeyCount(doc, 1)).Select(index =>
            {
                Require(Reader.StrokeReader_GetLayerAnimationKey(doc, 1, index, out var key), "Key query failed.");
                return key;
            }).ToArray();
            var transforms = keys.Where(x => x.property == 9).ToArray();
            Require(transforms.Length == 5, "Transform keys missing.");
            for (int i = 0; i < 5; i++)
                Require(transforms[i].timeTicks == i * 525 && transforms[i].interpolation == i &&
                    transforms[i].transformValue.flip == 1 && transforms[i].transformValue.transX == i, "Transform interpolation/time/value/flip changed.");
            Require(keys.Count(x => x.property == 0) == 2 && keys.Any(x => x.property == 0 && x.timeTicks == 525 && x.boolValue == 1), "Visibility keys missing.");
            Require(keys.Any(x => x.property == 1 && x.timeTicks == 3150 && x.floatValue == 0.75f), "Opacity keys missing.");
            Require(Enumerable.Range(0, Reader.StrokeReader_GetLayerAnimationKeyCount(doc, 0)).Any(index =>
                Reader.StrokeReader_GetLayerAnimationKey(doc, 0, index, out var key) && key.property == 8 && key.timeTicks == 525 && key.intValue == 525), "Offset key changed or StartOffset applied twice.");
        }
        finally { if (doc > 0) Reader.StrokeReader_Unload(doc); Reader.StrokeReader_End(); }

        byte[] marker = { 9, 8, 7 };
        string jsonPath = Path.Combine(project, "Quill.json");
        string original = File.ReadAllText(jsonPath);
        Reject(() => QuillToImmConverter.ConvertStaticPaintProject(project, output));
        var json = JObject.Parse(original);
        var sourceGroup = json["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!;
        sourceGroup["Animation"]!["Keys"]!["Action"] = new JArray(new JObject { ["Time"] = 0, ["Value"] = "Play", ["Interpolation"] = "None" });
        File.WriteAllText(jsonPath, json.ToString());
        Reject(() => QuillToImmConverter.ConvertAnimatedPaintProject(project, output));
        foreach (Action<JToken> corrupt in new Action<JToken>[]
        {
            x => x["Sequence"]!["RootLayer"]!["Animation"]!["Keys"]!["Action"] = new JArray(new JObject { ["Time"] = 0, ["Value"] = "Stop", ["Interpolation"] = "None" }),
            x => x["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!["Animation"]!["StartOffset"] = 526,
            x => x["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!["Animation"]!["Keys"]!["Offset"]![0]!["Time"] = 0.5,
            x => x["Sequence"]!["RootLayer"]!["Implementation"]!["Children"]![0]!["Implementation"]!["Children"]![0]!["Implementation"]!["Frames"] = new JArray(0, 2)
        })
        {
            var invalid = JObject.Parse(original);
            corrupt(invalid);
            File.WriteAllText(jsonPath, invalid.ToString());
            Reject(() => QuillToImmConverter.ConvertAnimatedPaintProject(project, output));
        }
        File.WriteAllText(jsonPath, original);
        Console.WriteLine("[IMM_QUILL_ANIMATED_TEST] Passed native drawings/held frames/hidden paint/timing/keys/interpolation/reflection checks; playback parity remains untested.");

        void Reject(Func<QuillToImmConversionResult> convert)
        {
            File.WriteAllBytes(output, marker);
            Require(!convert().Succeeded && File.ReadAllBytes(output).SequenceEqual(marker), "Rejected animation changed existing output.");
        }
    }

    private static string Describe(QuillToImmConversionResult result) => string.Join("; ", result.Issues.Select(x => $"{x.Code}: {x.Message}"));
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
