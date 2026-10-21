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
    /// Initial offline static-paint profile. Animation is an error; layer/pivot
    /// reflections are preserved through the additive offline exporter APIs.
    /// Viewpoints/media/unknown layers require explicit omission permission and
    /// produce partial-output diagnostics. No existing output is touched on failure.
    /// </summary>
    public static class QuillToImmConverter
    {
        public static QuillToImmConversionResult ConvertStaticPaintProject(
            string projectFolder, string outputPath, bool allowOmittedLayers = false)
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
                Preflight(root, "", true, true, issues, allowOmittedLayers);
                if (settings["AnimateOnStart"] != null || settings["TrackingOrigin"] != null)
                    Issue(issues, allowOmittedLayers ? QuillToImmIssueSeverity.Warning : QuillToImmIssueSeverity.Error,
                        "OmittedStartupMetadata", "/", "Startup/tracking metadata is outside the static paint profile.");
                if (issues.Any(x => x.Severity == QuillToImmIssueSeverity.Error))
                    return new QuillToImmConversionResult(issues);

                Sequence sequence = QuillSequenceReader.ReadMetadata(projectFolder);
                if (sequence?.RootLayer == null)
                    throw new InvalidDataException("Quill hierarchy could not be parsed.");
                CheckRoot(sequence.RootLayer);
                var created = ImmAuthoringDocument.Create(ExportSequenceType.Still, checked((uint)sequence.Framerate),
                    new UnityEngine.Color(sequence.BackgroundColor.R, sequence.BackgroundColor.G, sequence.BackgroundColor.B, 1f));
                using (ImmAuthoringDocument document = Require(created, "/"))
                {
                    using (var stream = File.OpenRead(sourceBinary))
                    using (var reader = new QBinReader(stream))
                    {
                        foreach (Layer child in sequence.RootLayer.Children)
                            MapLayer(document, child, 0, $"/{sequence.RootLayer.Name}", true, reader);
                    }
                    var snapshot = Require(document.CreateSnapshot(), "/");
                    if (!snapshot.Layers.Any(x => x.Type == ImmAuthoringLayerType.Paint && x.Drawings.Any(d => d.Strokes.Count > 0)))
                        throw new InvalidDataException("The selected static profile contains no exportable paint.");
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
            List<QuillToImmIssue> issues, bool allowOmittedLayers)
        {
            string path = $"{parentPath}/{(string)layer["Name"]}";
            string type = (string)layer["Type"];
            bool visible = parentVisible && ((bool?)layer["Visible"] ?? true);
            if (type != "Group" && type != "Paint")
            {
                Issue(issues, allowOmittedLayers ? QuillToImmIssueSeverity.Warning : QuillToImmIssueSeverity.Error,
                    "OmittedLayer", path, $"{type ?? "Unknown"} is outside the static paint profile.");
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
                bool dynamicTiming = (double?)animation["StartOffset"] != null && (double)animation["StartOffset"] != 0;
                if (!isRoot) dynamicTiming |= ((bool?)animation["Timeline"] ?? false) ||
                    ((double?)animation["Duration"] ?? 0) != 0 || ((double?)animation["MaxRepeatCount"] ?? 0) != 0;
                if (dynamicTiming)
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
                        if (!defaults)
                            Issue(issues, QuillToImmIssueSeverity.Error, "UnsupportedAnimation", path,
                                $"{channel.Name} keys are outside the static profile.");
                    }
            }
            if (!(layer["Implementation"] is JObject implementation))
                throw new InvalidDataException($"Missing layer implementation at {path}.");
            if (type == "Group")
            {
                if (!(implementation["Children"] is JArray children))
                    throw new InvalidDataException($"Missing group children at {path}.");
                foreach (JObject child in children) Preflight(child, path, visible, false, issues, allowOmittedLayers);
            }
            else if (implementation["Drawings"] is JArray drawings)
            {
                if (drawings.Count != 1 || !(implementation["Frames"] is JArray frames) ||
                    frames.Count == 0 || frames.Any(x => !IsZeroFrame(x)))
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

        private static bool IsIdentity(SharpQuill.Transform t) => t.Scale == 1f && t.Flip == "N" &&
            t.Translation.X == 0f && t.Translation.Y == 0f && t.Translation.Z == 0f &&
            t.Rotation.X == 0f && t.Rotation.Y == 0f && t.Rotation.Z == 0f && Math.Abs(t.Rotation.W) == 1f;

        private static void MapLayer(ImmAuthoringDocument document, Layer layer, long parentId,
            string parentPath, bool parentVisible, QBinReader reader)
        {
            string path = $"{parentPath}/{layer.Name}";
            bool visible = parentVisible && layer.Visible;
            if (!(layer is LayerGroup) && !(layer is LayerPaint)) return;
            if (layer is LayerPaint && !visible) return;
            var properties = ImmAuthoringLayerProperties.Default(layer.Name);
            properties.Visible = layer.Visible;
            properties.Opacity = layer.Opacity;
            properties.Transform = Transform(layer.Transform);
            properties.Pivot = Transform(layer.Pivot);
            if (layer is LayerGroup group)
            {
                long groupId = Require(document.CreateGroupLayer(parentId, properties), path);
                foreach (Layer child in group.Children) MapLayer(document, child, groupId, path, visible, reader);
                return;
            }
            var paint = (LayerPaint)layer;
            properties.PaintMaxRepeatCount = checked((uint)paint.MaxRepeatCount);
            long paintId = Require(document.CreatePaintLayer(parentId, properties), path);
            var drawing = paint.Drawings.Single();
            if (drawing.DataFileOffset < 4 || drawing.DataFileOffset > reader.BaseStream.Length - 4)
                throw new InvalidDataException($"Drawing offset is outside qbin at {path}.");
            reader.BaseStream.Position = drawing.DataFileOffset;
            DrawingData data = reader.ReadDrawingData(rejectUnknownBrushes: true);
            long drawingId = Require(document.CreateDrawing(paintId), path);
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
            foreach (int frame in paint.Frames) Require(document.AppendFrame(paintId, drawingId), path);
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
