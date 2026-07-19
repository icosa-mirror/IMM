using System;
using System.Collections.Generic;
using System.Threading;

namespace ImmPlayer.Authoring
{
    [Flags]
    public enum ImmAuthoringFeature
    {
        None = 0,
        Playback = 1 << 0,
        MutablePaintGraph = 1 << 1,
        MemoryExport = 1 << 2,
        FileExport = 1 << 3,
        PaintImport = 1 << 4,
        RuntimePreview = 1 << 5,
        ProgressReporting = 1 << 6,
        Cancellation = 1 << 7
    }

    public sealed class ImmAuthoringCapabilities
    {
        public string Platform { get; }
        public string Architecture { get; }
        public ImmAuthoringFeature Features { get; }
        public bool IsAuthoringSupported => Supports(ImmAuthoringFeature.MutablePaintGraph |
                                                     ImmAuthoringFeature.MemoryExport |
                                                     ImmAuthoringFeature.PaintImport);

        internal ImmAuthoringCapabilities(string platform, string architecture, ImmAuthoringFeature features)
        {
            Platform = platform ?? "Unknown";
            Architecture = architecture ?? "Unknown";
            Features = features;
        }

        public bool Supports(ImmAuthoringFeature features) => (Features & features) == features;
    }

    public static class ImmAuthoringRuntime
    {
        public static ImmAuthoringCapabilities Capabilities { get; } = DetectCapabilities();
        public static ImmAuthoringLimits DefaultLimits => ImmAuthoringLimits.Default;

        private static ImmAuthoringCapabilities DetectCapabilities()
        {
            string architecture = IntPtr.Size == 8 ? "x64" : "x86";
#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
            ImmAuthoringFeature features = ImmAuthoringFeature.Playback;
            if (IntPtr.Size == 8)
            {
                features |= ImmAuthoringFeature.MutablePaintGraph |
                            ImmAuthoringFeature.MemoryExport |
                            ImmAuthoringFeature.FileExport |
                            ImmAuthoringFeature.PaintImport |
                            ImmAuthoringFeature.RuntimePreview |
                            ImmAuthoringFeature.ProgressReporting |
                            ImmAuthoringFeature.Cancellation;
            }
            return new ImmAuthoringCapabilities("Windows", architecture, features);
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
            return new ImmAuthoringCapabilities("macOS", architecture, ImmAuthoringFeature.Playback);
#elif UNITY_ANDROID
            return new ImmAuthoringCapabilities("Android", architecture, ImmAuthoringFeature.Playback);
#elif UNITY_IOS
            return new ImmAuthoringCapabilities("iOS", architecture, ImmAuthoringFeature.Playback);
#else
            return new ImmAuthoringCapabilities(UnityEngine.Application.platform.ToString(), architecture, ImmAuthoringFeature.None);
#endif
        }
    }

    public sealed class ImmAuthoringLimits
    {
        public const long MiB = 1024L * 1024L;

        public static ImmAuthoringLimits Default { get; } = new ImmAuthoringLimits();

        public long MaxInputBytes { get; }
        public long MaxOutputBytes { get; }
        public int MaxLayers { get; }
        public int MaxHierarchyDepth { get; }
        public int MaxDrawings { get; }
        public int MaxStrokes { get; }
        public int MaxPointsPerStroke { get; }
        public long MaxTotalPoints { get; }
        public int MaxFrames { get; }
        public int MaxAnimationKeys { get; }
        public int MaxLayerNameCharacters { get; }

        public ImmAuthoringLimits(
            long maxInputBytes = 256L * MiB,
            long maxOutputBytes = 256L * MiB,
            int maxLayers = 4096,
            int maxHierarchyDepth = 64,
            int maxDrawings = 65536,
            int maxStrokes = 1000000,
            int maxPointsPerStroke = 1000000,
            long maxTotalPoints = 16000000,
            int maxFrames = 1000000,
            int maxAnimationKeys = 1000000,
            int maxLayerNameCharacters = 1024)
        {
            if (maxInputBytes <= 0 || maxOutputBytes <= 0 || maxLayers <= 0 ||
                maxHierarchyDepth <= 0 || maxDrawings <= 0 || maxStrokes <= 0 ||
                maxPointsPerStroke <= 0 || maxTotalPoints <= 0 || maxFrames <= 0 ||
                maxAnimationKeys <= 0 || maxLayerNameCharacters <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(maxInputBytes), "All authoring limits must be positive.");
            }

            MaxInputBytes = maxInputBytes;
            MaxOutputBytes = maxOutputBytes;
            MaxLayers = maxLayers;
            MaxHierarchyDepth = maxHierarchyDepth;
            MaxDrawings = maxDrawings;
            MaxStrokes = maxStrokes;
            MaxPointsPerStroke = maxPointsPerStroke;
            MaxTotalPoints = maxTotalPoints;
            MaxFrames = maxFrames;
            MaxAnimationKeys = maxAnimationKeys;
            MaxLayerNameCharacters = maxLayerNameCharacters;
        }
    }

    public enum ImmAuthoringProgressStage
    {
        Validating = 0,
        ReadingInput,
        InspectingSource,
        ImportingGraph,
        CompilingGraph,
        Serializing,
        WritingOutput,
        Completed
    }

    public readonly struct ImmAuthoringProgress
    {
        public ImmAuthoringProgressStage Stage { get; }
        public long CompletedUnits { get; }
        public long TotalUnits { get; }
        public float Fraction => TotalUnits <= 0 ? 0f : Math.Min(1f, (float)CompletedUnits / TotalUnits);
        public string Message { get; }

        public ImmAuthoringProgress(
            ImmAuthoringProgressStage stage,
            long completedUnits,
            long totalUnits,
            string message = null)
        {
            Stage = stage;
            CompletedUnits = Math.Max(0, completedUnits);
            TotalUnits = Math.Max(0, totalUnits);
            Message = message ?? string.Empty;
        }
    }

    public sealed class ImmAuthoringOperationOptions
    {
        public static ImmAuthoringOperationOptions Default { get; } = new ImmAuthoringOperationOptions();

        public CancellationToken CancellationToken { get; }
        public IProgress<ImmAuthoringProgress> Progress { get; }
        public ImmAuthoringLimits Limits { get; }

        public ImmAuthoringOperationOptions(
            CancellationToken cancellationToken = default,
            IProgress<ImmAuthoringProgress> progress = null,
            ImmAuthoringLimits limits = null)
        {
            CancellationToken = cancellationToken;
            Progress = progress;
            Limits = limits ?? ImmAuthoringLimits.Default;
        }

        internal void Report(ImmAuthoringProgressStage stage, long completed, long total, string message = null)
        {
            Progress?.Report(new ImmAuthoringProgress(stage, completed, total, message));
        }
    }

    internal static class ImmAuthoringLimitValidator
    {
        internal static ImmAuthoringResult Validate(ImmAuthoringSnapshot snapshot, ImmAuthoringLimits limits)
        {
            if (snapshot == null)
                return ImmAuthoringResult.Failure(ImmAuthoringErrorCode.InvalidArgument, "Snapshot cannot be null.");
            limits = limits ?? ImmAuthoringLimits.Default;
            if (snapshot.Layers.Count > limits.MaxLayers)
                return Exceeded("layers", snapshot.Layers.Count, limits.MaxLayers);

            Dictionary<long, int> depths = new Dictionary<long, int>(snapshot.Layers.Count);
            long drawings = 0;
            long strokes = 0;
            long points = 0;
            long frames = 0;
            long keys = 0;
            foreach (ImmAuthoringLayerSnapshot layer in snapshot.Layers)
            {
                if (layer.Properties.Name != null && layer.Properties.Name.Length > limits.MaxLayerNameCharacters)
                    return Exceeded("layer-name characters", layer.Properties.Name.Length, limits.MaxLayerNameCharacters, layer.Id);
                int depth = layer.ParentId == 0 ? 1 : (depths.TryGetValue(layer.ParentId, out int parentDepth) ? parentDepth + 1 : limits.MaxHierarchyDepth + 1);
                depths[layer.Id] = depth;
                if (depth > limits.MaxHierarchyDepth)
                    return Exceeded("hierarchy depth", depth, limits.MaxHierarchyDepth, layer.Id);

                drawings += layer.Drawings.Count;
                frames += layer.Frames.Count;
                keys += layer.AnimationKeys.Count;
                foreach (ImmAuthoringDrawingSnapshot drawing in layer.Drawings)
                {
                    strokes += drawing.Strokes.Count;
                    foreach (ImmAuthoringStrokeSnapshot stroke in drawing.Strokes)
                    {
                        if (stroke.Points.Count > limits.MaxPointsPerStroke)
                            return Exceeded("points per stroke", stroke.Points.Count, limits.MaxPointsPerStroke, stroke.Id);
                        points += stroke.Points.Count;
                    }
                }
            }

            if (drawings > limits.MaxDrawings) return Exceeded("drawings", drawings, limits.MaxDrawings);
            if (strokes > limits.MaxStrokes) return Exceeded("strokes", strokes, limits.MaxStrokes);
            if (points > limits.MaxTotalPoints) return Exceeded("total points", points, limits.MaxTotalPoints);
            if (frames > limits.MaxFrames) return Exceeded("frames", frames, limits.MaxFrames);
            if (keys > limits.MaxAnimationKeys) return Exceeded("animation keys", keys, limits.MaxAnimationKeys);
            return ImmAuthoringResult.Success();
        }

        internal static ImmAuthoringResult Exceeded(string resource, long actual, long maximum, long objectId = 0) =>
            ImmAuthoringResult.Failure(
                ImmAuthoringErrorCode.ResourceLimitExceeded,
                $"Document contains {actual:N0} {resource}; the configured limit is {maximum:N0}.",
                objectId);
    }
}
