using System;
using System.Collections.Generic;
using System.IO;
using System.Globalization;
using System.Linq;
using ImmPlayer.Exporter;
using Newtonsoft.Json.Linq;
using SharpQuill;
using UnityEngine;

namespace ImmPlayer.Authoring
{
    public enum QuillToImmIssueSeverity { Information, Warning, Error }

    public sealed class QuillToImmIssue
    {
        public QuillToImmIssueSeverity Severity { get; }
        public string Code { get; }
        public string SourcePath { get; }
        public string Message { get; }

        internal QuillToImmIssue(QuillToImmIssueSeverity severity, string code, string path, string message)
        {
            Severity = severity;
            Code = code;
            SourcePath = path;
            Message = message;
        }
    }

    public sealed class QuillToImmConversionResult
    {
        public bool Succeeded => Export != null && Export.Succeeded;
        public bool IsPartial => Issues.Any(x => x.Code == "OmittedLayer" || x.Code == "OmittedStartupMetadata");
        public IReadOnlyList<QuillToImmIssue> Issues { get; }
        public ImmAuthoringExportResult Export { get; }

        internal QuillToImmConversionResult(List<QuillToImmIssue> issues, ImmAuthoringExportResult export = null)
        {
            Issues = Array.AsReadOnly(issues.ToArray());
            Export = export;
        }
    }

    /// <summary>
    /// Offline static and animated paint profiles. The static entry rejects animation;
    /// layer/pivot/key reflections use the additive offline exporter APIs.
    /// Viewpoints/media/unknown layers require explicit omission permission and
    /// produce partial-output diagnostics. No existing output is touched on failure.
    /// </summary>
    public static class QuillToImmConverter
    {
        public static QuillToImmConversionResult ConvertStaticPaintProject(
            string projectFolder, string outputPath, bool allowOmittedLayers = false)
            => ConvertPaintProject(projectFolder, outputPath, allowOmittedLayers, animated: false);

        /// <summary>Converts paint drawings and layer animation. Root animation and navigation remain unsupported.</summary>
        public static QuillToImmConversionResult ConvertAnimatedPaintProject(
            string projectFolder, string outputPath, bool allowOmittedLayers = false)
            => ConvertPaintProject(projectFolder, outputPath, allowOmittedLayers, animated: true);

        private static QuillToImmConversionResult ConvertPaintProject(
            string projectFolder, string outputPath, bool allowOmittedLayers, bool animated)
        {
            var issues = new List<QuillToImmIssue>();
            try
            {
                if (string.IsNullOrWhiteSpace(projectFolder) || string.IsNullOrWhiteSpace(outputPath))
                    throw new ArgumentException("Project folder and output path are required.");
                string sourceJson = Path.Combine(projectFolder, "Quill.json");
                string sourceBinary = Path.Combine(projectFolder, "Quill.qbin");
                string fullOutput = Path.GetFullPath(outputPath);
                if (string.Equals(fullOutput, Path.GetFullPath(sourceJson), StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(fullOutput, Path.GetFullPath(sourceBinary), StringComparison.OrdinalIgnoreCase))
                    throw new ArgumentException("Output must not replace either source project file.");
                if (!File.Exists(sourceJson) || !File.Exists(sourceBinary))
                    throw new FileNotFoundException("Both Quill.json and Quill.qbin are required.");
                if (new FileInfo(sourceJson).Length + new FileInfo(sourceBinary).Length > ImmAuthoringLimits.Default.MaxInputBytes)
                    throw new InvalidDataException("Project exceeds the default authoring input byte limit.");

                var json = JObject.Parse(File.ReadAllText(sourceJson));
                if ((int?)json["Version"] != 1 || !(json["Sequence"] is JObject settings) ||
                    !(settings["RootLayer"] is JObject root) || (string)root["Type"] != "Group")
                    throw new InvalidDataException("Expected a Version 1 Quill project with a Group root.");
                Preflight(root, "", true, true, issues, allowOmittedLayers, animated);
                if (settings["AnimateOnStart"] != null || settings["TrackingOrigin"] != null)
                    Issue(issues, allowOmittedLayers ? QuillToImmIssueSeverity.Warning : QuillToImmIssueSeverity.Error,
                        "OmittedStartupMetadata", "/", "Startup/tracking metadata is outside the paint profiles.");
                if (issues.Any(x => x.Severity == QuillToImmIssueSeverity.Error))
                    return new QuillToImmConversionResult(issues);

                Sequence sequence = QuillSequenceReader.ReadMetadata(projectFolder);
                if (sequence?.RootLayer == null)
                    throw new InvalidDataException("Quill hierarchy could not be parsed.");
                if (!animated) CheckRoot(sequence.RootLayer);
                var created = ImmAuthoringDocument.Create(animated ? ExportSequenceType.Animated : ExportSequenceType.Still, checked((uint)sequence.Framerate),
                    new UnityEngine.Color(sequence.BackgroundColor.R, sequence.BackgroundColor.G, sequence.BackgroundColor.B, 1f));
                using (ImmAuthoringDocument document = Require(created, "/"))
                {
                    if (animated)
                    {
                        var rootProperties = ImmAuthoringLayerProperties.Default(sequence.RootLayer.Name);
                        rootProperties.Visible = sequence.RootLayer.Visible;
                        rootProperties.Opacity = sequence.RootLayer.Opacity;
                        rootProperties.Transform = Transform(sequence.RootLayer.Transform);
                        rootProperties.Pivot = Transform(sequence.RootLayer.Pivot);
                        rootProperties.IsTimeline = sequence.RootLayer.Animation.Timeline;
                        rootProperties.DurationTicks = ExactNonNegativeInteger(sequence.RootLayer.Animation.Duration, "/");
                        if (rootProperties.DurationTicks == 0 && (bool?)root["Animation"]?["Timeline"] == false)
                        {
                            // The paired legacy Fishgym export normalizes its false/zero
                            // root to Quill's usual one-hour root sequence.
                            rootProperties.DurationTicks = (long)LayerGroup.CreateDefault().Animation.Duration;
                            Issue(issues, QuillToImmIssueSeverity.Information, "NormalizedRootTimeline", "/",
                                "Legacy zero-duration root uses Quill's default root timeline, matching the inspected export.");
                        }
                        rootProperties.MaxRepeatCount = checked((uint)ExactNonNegativeInteger(sequence.RootLayer.Animation.MaxRepeatCount, "/"));
                        Require(document.SetRootProperties(rootProperties), "/");
                        MapAnimation(document, 0, sequence.RootLayer.Animation.Keys, root, "/");
                    }
                    using (var stream = File.OpenRead(sourceBinary))
                    using (var reader = new QBinReader(stream))
                    {
                        MapChildren(document, sequence.RootLayer, root, 0, $"/{sequence.RootLayer.Name}", true, reader, animated);
                    }
                    var snapshot = Require(document.CreateSnapshot(), "/");
                    if (!snapshot.Layers.Any(x => x.Type == ImmAuthoringLayerType.Paint && x.Drawings.Any(d => d.Strokes.Count > 0)))
                        throw new InvalidDataException("The selected paint profile contains no exportable paint.");
                    ImmAuthoringExportResult exported = ImmAuthoringCompiler.ExportToFile(snapshot, fullOutput);
                    if (!exported.Succeeded)
                        Issue(issues, QuillToImmIssueSeverity.Error, "ExportFailed", "/", exported.Message);
                    return new QuillToImmConversionResult(issues, exported);
                }
            }
            catch (Exception exception) when (exception is IOException || exception is InvalidDataException || exception is ArgumentException ||
                exception is Newtonsoft.Json.JsonException || exception is InvalidOperationException ||
                exception is NotSupportedException || exception is OverflowException || exception is UnauthorizedAccessException ||
                exception is FormatException || exception is InvalidCastException)
            {
                Issue(issues, QuillToImmIssueSeverity.Error, "ConversionFailed", "/", exception.Message);
                return new QuillToImmConversionResult(issues);
            }
        }

        private static void Preflight(JObject layer, string parentPath, bool parentVisible, bool isRoot,
            List<QuillToImmIssue> issues, bool allowOmittedLayers, bool animated)
        {
            string path = $"{parentPath}/{(string)layer["Name"]}";
            string type = (string)layer["Type"];
            bool visible = parentVisible && PotentiallyVisible(layer, animated);
            if (type != "Group" && type != "Paint")
            {
                Issue(issues, allowOmittedLayers ? QuillToImmIssueSeverity.Warning : QuillToImmIssueSeverity.Error,
                    "OmittedLayer", path, $"{type ?? "Unknown"} is outside the paint profiles.");
                return;
            }
            if (type == "Paint" && !visible)
            {
                Issue(issues, QuillToImmIssueSeverity.Information, "HiddenPaintExcluded", path,
                    "Statically hidden paint is excluded, matching the inspected Quill exports.");
                return;
            }
            string keepAlive = (string)layer["KeepAlive"]?["Type"];
            if (keepAlive != null && keepAlive != "None")
                Issue(issues, QuillToImmIssueSeverity.Error, "UnsupportedKeepAlive", path,
                    $"KeepAlive {keepAlive} is outside the static profile.");
            foreach (string property in new[] { "Transform", "Pivot" })
            {
                string flip = (string)layer[property]?["Flip"];
                if (flip != null && flip != "N" && flip != "X" && flip != "Y" && flip != "Z")
                    Issue(issues, QuillToImmIssueSeverity.Error, "UnsupportedReflection", path,
                        $"Unknown {property} flip {flip}.");
            }
            var animation = layer["Animation"];
            if (animation != null)
            {
                if (animated)
                {
                    foreach (string field in new[] { "Duration", "MaxRepeatCount", "StartOffset" })
                    {
                        double value = (double?)animation[field] ?? 0;
                        ExactNonNegativeInteger(value, path);
                        if (value != (double)(float)value)
                            throw new NotSupportedException($"{field} cannot be represented exactly by the source reader at {path}.");
                    }
                }
                bool dynamicTiming = (double?)animation["StartOffset"] != null && (double)animation["StartOffset"] != 0;
                if (!isRoot) dynamicTiming |= ((bool?)animation["Timeline"] ?? false) ||
                    ((double?)animation["Duration"] ?? 0) != 0 || ((double?)animation["MaxRepeatCount"] ?? 0) != 0;
                if (dynamicTiming && (!animated || isRoot))
                    Issue(issues, QuillToImmIssueSeverity.Error, "UnsupportedAnimation", path,
                        "Timeline timing is outside the static profile.");
                if (animation["Keys"] is JObject keys)
                    foreach (JProperty channel in keys.Properties())
                    {
                        if (!(channel.Value is JArray values))
                            throw new InvalidDataException($"Invalid key channel at {path}.");
                        bool defaults = values.Count == 0 || (values.Count == 1 && (double?)values[0]["Time"] == 0 &&
                            ((channel.Name == "Visibility" && (bool?)values[0]["Value"] == true) ||
                             (channel.Name == "Offset" && (double?)values[0]["Value"] == 0)));
                        bool supported = animated && !isRoot &&
                            (channel.Name == "Visibility" || channel.Name == "Opacity" || channel.Name == "Transform" || channel.Name == "Offset");
                        if (supported)
                            foreach (JToken key in values)
                            {
                                long time = ExactNonNegativeInteger((double)key["Time"], path);
                                if (time > int.MaxValue) throw new NotSupportedException($"Key time exceeds source reader range at {path}.");
                                if (channel.Name == "Offset" && ExactNonNegativeInteger((double)key["Value"], path) > int.MaxValue)
                                    throw new NotSupportedException($"Offset exceeds source reader range at {path}.");
                            }
                        if (!defaults && !supported)
                            Issue(issues, QuillToImmIssueSeverity.Error, "UnsupportedAnimation", path,
                                $"{channel.Name} keys are outside the {(animated && isRoot ? "verified root mapping" : "static profile")}.");
                    }
            }
            if (!(layer["Implementation"] is JObject implementation))
                throw new InvalidDataException($"Missing layer implementation at {path}.");
            if (type == "Group")
            {
                if (!(implementation["Children"] is JArray children))
                    throw new InvalidDataException($"Missing group children at {path}.");
                foreach (JObject child in children) Preflight(child, path, visible, false, issues, allowOmittedLayers, animated);
            }
            else if (implementation["Drawings"] is JArray drawings)
            {
                if (animated && (!(implementation["Frames"] is JArray frameMap) ||
                    frameMap.Count == 0 || frameMap.Any(x => !IsDrawingIndex(x, drawings.Count))))
                    throw new InvalidDataException($"Invalid animated frame map at {path}.");
                if (!animated && (drawings.Count != 1 || !(implementation["Frames"] is JArray frames) ||
                    frames.Count == 0 || frames.Any(x => !IsZeroFrame(x))))
                    Issue(issues, QuillToImmIssueSeverity.Error, "UnsupportedAnimation", path,
                        "Static paint requires one drawing and frame references to drawing zero.");
            }
            else if (implementation["DataFileOffset"] == null)
                throw new InvalidDataException($"Missing paint data declaration at {path}.");
        }

        private static void CheckRoot(LayerGroup root)
        {
            if (!root.Visible || root.Opacity != 1f || !IsIdentity(root.Transform) || !IsIdentity(root.Pivot))
                throw new NotSupportedException("Nonidentity root properties are outside the initial static profile.");
        }

        private static bool IsZeroFrame(JToken value) =>
            ((value.Type == JTokenType.Integer || value.Type == JTokenType.Float) && (double)value == 0) ||
            (value.Type == JTokenType.String && int.TryParse((string)value, NumberStyles.Integer,
                CultureInfo.InvariantCulture, out int index) && index == 0);

        private static bool IsDrawingIndex(JToken value, int count)
        {
            if (value.Type == JTokenType.String)
                return int.TryParse((string)value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int index) && index >= 0 && index < count;
            if (value.Type != JTokenType.Integer && value.Type != JTokenType.Float) return false;
            double number = (double)value;
            return number >= 0 && number < count && number == Math.Truncate(number);
        }

        private static bool IsIdentity(SharpQuill.Transform t) => t.Scale == 1f && t.Flip == "N" &&
            t.Translation.X == 0f && t.Translation.Y == 0f && t.Translation.Z == 0f &&
            t.Rotation.X == 0f && t.Rotation.Y == 0f && t.Rotation.Z == 0f && Math.Abs(t.Rotation.W) == 1f;

        private static bool PotentiallyVisible(JObject source, bool animated) =>
            ((bool?)source["Visible"] ?? true) || (animated && source["Animation"]?["Keys"]?["Visibility"] is JArray keys &&
                keys.Any(x => (bool?)x["Value"] == true));

        private static void MapChildren(ImmAuthoringDocument document, LayerGroup group, JObject source, long parentId,
            string path, bool visible, QBinReader reader, bool animated)
        {
            var layers = group.Children.Where(x => x is LayerGroup || x is LayerPaint).ToArray();
            var declarations = ((JArray)source["Implementation"]["Children"]).OfType<JObject>()
                .Where(x => (string)x["Type"] == "Group" || (string)x["Type"] == "Paint").ToArray();
            if (layers.Length != declarations.Length) throw new InvalidDataException($"Parsed hierarchy differs at {path}.");
            for (int i = 0; i < layers.Length; i++)
                MapLayer(document, layers[i], declarations[i], parentId, path, visible, reader, animated);
        }

        private static void MapLayer(ImmAuthoringDocument document, Layer layer, JObject source, long parentId,
            string parentPath, bool parentVisible, QBinReader reader, bool animated)
        {
            string path = $"{parentPath}/{layer.Name}";
            bool visible = parentVisible && PotentiallyVisible(source, animated);
            if (!(layer is LayerGroup) && !(layer is LayerPaint)) return;
            if (layer is LayerPaint && !visible) return;
            var properties = ImmAuthoringLayerProperties.Default(layer.Name);
            properties.Visible = layer.Visible;
            properties.Opacity = layer.Opacity;
            properties.Transform = Transform(layer.Transform);
            properties.Pivot = Transform(layer.Pivot);
            if (animated)
            {
                properties.IsTimeline = layer.Animation.Timeline;
                properties.DurationTicks = ExactNonNegativeInteger(layer.Animation.Duration, path);
                properties.MaxRepeatCount = checked((uint)ExactNonNegativeInteger(layer.Animation.MaxRepeatCount, path));
                if (layer.Animation.StartOffset != 0 && !layer.Animation.Keys.Offset.Any(x => x.Value == layer.Animation.StartOffset))
                    throw new NotSupportedException($"StartOffset without a matching offset key is unsupported at {path}.");
            }
            if (layer is LayerGroup group)
            {
                long groupId = Require(document.CreateGroupLayer(parentId, properties), path);
                if (animated) MapAnimation(document, groupId, layer.Animation.Keys, source, path);
                MapChildren(document, group, source, groupId, path, visible, reader, animated);
                return;
            }
            var paint = (LayerPaint)layer;
            properties.PaintMaxRepeatCount = checked((uint)paint.MaxRepeatCount);
            long paintId = Require(document.CreatePaintLayer(parentId, properties), path);
            if (animated) MapAnimation(document, paintId, layer.Animation.Keys, source, path);
            var drawingIds = new List<long>();
            foreach (var drawing in paint.Drawings)
            {
                if (drawing.DataFileOffset < 4 || drawing.DataFileOffset > reader.BaseStream.Length - 4)
                    throw new InvalidDataException($"Drawing offset is outside qbin at {path}.");
                reader.BaseStream.Position = drawing.DataFileOffset;
                DrawingData data = reader.ReadDrawingData(rejectUnknownBrushes: true);
                long drawingId = Require(document.CreateDrawing(paintId), path);
                drawingIds.Add(drawingId);
                foreach (Stroke stroke in data.Strokes)
                {
                    if (stroke.Vertices.Count > 8192)
                        throw new NotSupportedException($"Stroke {stroke.Id} at {path} exceeds the IMM writer's 8192-point limit.");
                    var points = new PaintPoint[stroke.Vertices.Count];
                    for (int i = 0; i < points.Length; i++)
                    {
                        Vertex vertex = stroke.Vertices[i];
                        points[i] = new PaintPoint
                        {
                            Position = Vector(vertex.Position), Normal = Vector(vertex.Normal), Direction = Vector(vertex.Tangent),
                            Color = new UnityEngine.Color(vertex.Color.R, vertex.Color.G, vertex.Color.B, 1f),
                            Alpha = vertex.Opacity, Width = vertex.Width
                        };
                    }
                    Require(document.CreateStroke(drawingId, Brush(stroke.BrushType),
                        stroke.DisableRotationalOpacity ? VisibilityType.Always : VisibilityType.FadePow2, points), path);
                }
            }
            foreach (int frame in paint.Frames)
            {
                if (frame < 0 || frame >= drawingIds.Count)
                    throw new InvalidDataException($"Frame references an invalid drawing at {path}.");
                Require(document.AppendFrame(paintId, drawingIds[frame]), path);
            }
        }

        private static long ExactNonNegativeInteger(double value, string path)
        {
            if (double.IsNaN(value) || double.IsInfinity(value) || value < 0 || value != Math.Truncate(value))
                throw new InvalidDataException($"Expected nonnegative integer timing at {path}.");
            return checked((long)value);
        }

        private static void MapAnimation(ImmAuthoringDocument document, long layerId, Keyframes keys, JObject source, string path)
        {
            var channels = source["Animation"]?["Keys"];
            if (channels?["Visibility"] != null)
                foreach (var key in keys.Visibility)
                    Add(ImmAuthoringAnimationProperty.Visibility, key.Time, ImmAuthoringAnimationValue.FromBool(key.Value), key.Interpolation);
            if (channels?["Opacity"] != null)
                foreach (var key in keys.Opacity)
                    Add(ImmAuthoringAnimationProperty.Opacity, key.Time, ImmAuthoringAnimationValue.FromFloat(key.Value), key.Interpolation);
            if (channels?["Transform"] != null)
                foreach (var key in keys.Transform)
                    Add(ImmAuthoringAnimationProperty.Transform, key.Time, ImmAuthoringAnimationValue.FromTransform(Transform(key.Value)), key.Interpolation);
            // Inspected Quill exports retain paint's default zero offset even when
            // the JSON omits that channel. Groups need no implicit offset key.
            if (channels?["Offset"] != null || (string)source["Type"] == "Paint")
                foreach (var key in keys.Offset)
                    Add(ImmAuthoringAnimationProperty.Offset, key.Time, ImmAuthoringAnimationValue.FromUInt(checked((uint)key.Value)), key.Interpolation);

            void Add(ImmAuthoringAnimationProperty property, long time, ImmAuthoringAnimationValue value, Interpolation interpolation)
            {
                ImmAuthoringInterpolation mapped;
                switch (interpolation)
                {
                    case Interpolation.None: mapped = ImmAuthoringInterpolation.None; break;
                    case Interpolation.Linear: mapped = ImmAuthoringInterpolation.Linear; break;
                    case Interpolation.Smoothstep: mapped = ImmAuthoringInterpolation.Smoothstep; break;
                    case Interpolation.EaseIn: mapped = ImmAuthoringInterpolation.EaseIn; break;
                    case Interpolation.EaseOut: mapped = ImmAuthoringInterpolation.EaseOut; break;
                    default: throw new NotSupportedException($"Unknown interpolation at {path}.");
                }
                Require(document.CreateAnimationKey(layerId, property, time, value, mapped), path);
            }
        }

        private static BrushSectionType Brush(BrushType brush)
        {
            switch (brush)
            {
                case BrushType.Ribbon: return BrushSectionType.Segment;
                case BrushType.Cylinder: return BrushSectionType.Circle;
                case BrushType.Ellipse: return BrushSectionType.Ellipse;
                case BrushType.Cube: return BrushSectionType.Square;
                default: throw new NotSupportedException($"Unsupported brush {brush}.");
            }
        }

        private static ImmAuthoringTransform Transform(SharpQuill.Transform t) => new ImmAuthoringTransform
        {
            Position = Vector(t.Translation), Rotation = new UnityEngine.Quaternion(t.Rotation.X, t.Rotation.Y, t.Rotation.Z, t.Rotation.W),
            Scale = t.Scale, Flip = Flip(t.Flip)
        };
        private static ImmAuthoringFlip Flip(string flip)
        {
            switch (flip)
            {
                case "N": return ImmAuthoringFlip.None;
                case "X": return ImmAuthoringFlip.X;
                case "Y": return ImmAuthoringFlip.Y;
                case "Z": return ImmAuthoringFlip.Z;
                default: throw new InvalidDataException($"Unknown transform flip {flip}.");
            }
        }
        private static UnityEngine.Vector3 Vector(SharpQuill.Vector3 v) => new UnityEngine.Vector3(v.X, v.Y, v.Z);
        private static T Require<T>(ImmAuthoringResult<T> result, string path)
        {
            if (!result.Succeeded) throw new InvalidDataException($"{path}: {result.Message}");
            return result.Value;
        }
        private static void Require(ImmAuthoringResult result, string path)
        {
            if (!result.Succeeded) throw new InvalidDataException($"{path}: {result.Message}");
        }
        private static void Issue(List<QuillToImmIssue> issues, QuillToImmIssueSeverity severity, string code, string path, string message) =>
            issues.Add(new QuillToImmIssue(severity, code, path, message));
    }
}
