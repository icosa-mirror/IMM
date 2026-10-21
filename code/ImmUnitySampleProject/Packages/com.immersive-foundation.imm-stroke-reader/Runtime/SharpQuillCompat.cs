using System;
using System.IO;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using ImmPlayer;
using SharpQuill;
using UnityEngine;
using Color = SharpQuill.Color;
using Quaternion = SharpQuill.Quaternion;
using Transform = SharpQuill.Transform;
using Vector3 = UnityEngine.Vector3;

namespace ImmStrokeReader
{
    public enum SharpQuillConversionIssueCode
    {
        LoadFailed, DiagnosticUnavailable, RootMetadataNotPreserved, FlattenedHierarchy,
        AnimationNotPreserved, LayerOmitted, PictureExcluded, UnsupportedPictureType,
        ChapterSubset, DataReadFailed, UnsupportedBrush, ConversionFailed, ValueNotRepresentable, StrokeMetadataNotPreserved, SoundMetadataNotPreserved, ResourceLimit, ViewpointMetadataNotPreserved
    }

    public sealed class SharpQuillConversionIssue
    {
        public SharpQuillConversionIssueCode Code { get; }
        public int SourceLayerId { get; }
        public string SourceLayerName { get; }
        public int SourceLayerType { get; }
        public string Message { get; }
        internal SharpQuillConversionIssue(SharpQuillConversionIssueCode code, int id, string name, string message, int sourceLayerType = -1)
        { Code = code; SourceLayerId = id; SourceLayerName = name; SourceLayerType = sourceLayerType; Message = message; }
    }

    /// <summary>A legacy adapter subset plus explicit omissions; not a full reverse-conversion certificate.</summary>
    public sealed class SharpQuillReadResult
    {
        public Sequence Sequence { get; }
        public IReadOnlyList<SharpQuillConversionIssue> Issues { get; }
        public bool Succeeded => Sequence != null;
        public bool IsPartial => Succeeded && Issues.Count != 0;
        internal SharpQuillReadResult(Sequence sequence, List<SharpQuillConversionIssue> issues)
        { Sequence = sequence; Issues = issues.AsReadOnly(); }
    }

    public static class SharpQuillCompat
    {
        private const string LogPrefix = "[IMM2QUILL_20260209A] ";

        public static Sequence ReadImmAsSequence(string path, bool includePictures = false, int chapterIndex = -1)
            => ReadImmAsSequenceCore(path, includePictures, chapterIndex, null);

        /// <summary>Returns the existing paint/picture subset with source-layer loss diagnostics. Requires current reader APIs.</summary>
        public static SharpQuillReadResult ReadImmAsSequenceWithReport(string path, bool includePictures = false, int chapterIndex = -1)
        {
            var issues = new List<SharpQuillConversionIssue>();
            var sequence = ReadImmAsSequenceCore(path, includePictures, chapterIndex, issues);
            if (sequence == null)
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LoadFailed, -1, "", "IMM loading or initialization failed."));
            return new SharpQuillReadResult(sequence, issues);
        }

        /// <summary>Read the authoring hierarchy and supported keys without flattening. Unsupported content is reported; legacy readers are unchanged.</summary>
        public static SharpQuillReadResult ReadImmAsHierarchyWithReport(string path, bool includePictures = true, int maxDecodedSoundBytes = 256 * 1024 * 1024)
        {
            var issues = new List<SharpQuillConversionIssue>();
            Sequence sequence = null;
            if (maxDecodedSoundBytes <= 0) throw new ArgumentOutOfRangeException(nameof(maxDecodedSoundBytes));
            try { sequence = ReadImmAsSequenceCore(path, includePictures, -1, issues, true, maxDecodedSoundBytes); }
            catch (EntryPointNotFoundException)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DiagnosticUnavailable, -1, "",
                    "The hierarchy reader requires current authoring reader entry points."));
            }
            catch (NotSupportedException exception)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ValueNotRepresentable, -1, "", exception.Message));
            }
            catch (Exception exception) when (exception is InvalidDataException || exception is OverflowException)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ConversionFailed, -1, "", exception.Message));
            }
            catch (OutOfMemoryException)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ResourceLimit, -1, "", "Hierarchy conversion allocation failed; no partial sequence returned."));
            }
            if (sequence == null && issues.Count == 0)
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LoadFailed, -1, "", "IMM loading or initialization failed."));
            return new SharpQuillReadResult(sequence, issues);
        }

        private static Sequence ReadImmAsSequenceCore(string path, bool includePictures, int chapterIndex,
            List<SharpQuillConversionIssue> issues, bool preserveHierarchy = false, int maxDecodedSoundBytes = 0)
        {
            if (string.IsNullOrEmpty(path))
            {
                return null;
            }

            if (!EnsureInitialized())
            {
                return null;
            }

            if (!TryLoadDocument(path, out int docId))
            {
                return null;
            }

            try
            {
                if (chapterIndex >= 0)
                {
                    int chapterCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetChapterCount(docId);
                    if (chapterIndex >= chapterCount)
                    {
                        Debug.LogWarning($"{LogPrefix}Requested chapter {chapterIndex} is out of range (chapterCount={chapterCount}), falling back to chapter 0");
                        chapterIndex = 0;
                    }
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_SetChapter(docId, chapterIndex))
                    {
                        Debug.LogWarning($"{LogPrefix}StrokeReader_SetChapter({chapterIndex}) failed, using default chapter");
                    }
                }
                if (preserveHierarchy) return ConvertHierarchyDocument(docId, includePictures, issues, maxDecodedSoundBytes);
                if (issues != null) CaptureLossReport(docId, includePictures, chapterIndex, issues);
                return ConvertDocument(docId, includePictures, chapterIndex, issues);
            }
            finally
            {
                ImmPlayer.ImmStrokeReader.StrokeReader_Unload(docId);
            }
        }

        /// <summary>
        /// Returns the number of chapters in an IMM file, or 0 on failure.
        /// Uses a fast path that reads only the scene graph header without loading layer assets.
        /// </summary>
        public static int GetImmChapterCount(string path)
        {
            if (string.IsNullOrEmpty(path))
            {
                return 0;
            }

            if (!EnsureInitialized())
            {
                return 0;
            }

            return ImmPlayer.ImmStrokeReader.StrokeReader_GetChapterCountFromFile(path);
        }

        private static bool EnsureInitialized()
        {
            if (ImmPlayer.ImmStrokeReader.StrokeReader_IsInitialized())
            {
                return true;
            }
            return ImmPlayer.ImmStrokeReader.StrokeReader_Init(null) == 0;
        }

        private static bool TryLoadDocument(string path, out int docId)
        {
            docId = ImmPlayer.ImmStrokeReader.StrokeReader_LoadFromFile(path);
            if (docId >= 0)
            {
                return true;
            }

            Debug.LogWarning($"{LogPrefix}StrokeReader_LoadFromFile failed for '{path}', trying memory fallback");

            byte[] immBytes;
            try
            {
                immBytes = File.ReadAllBytes(path);
            }
            catch
            {
                return false;
            }

            if (immBytes == null || immBytes.Length == 0)
            {
                return false;
            }

            IntPtr data = Marshal.AllocHGlobal(immBytes.Length);
            try
            {
                Marshal.Copy(immBytes, 0, data, immBytes.Length);
                docId = ImmPlayer.ImmStrokeReader.StrokeReader_LoadFromMemory(data, immBytes.Length);
                if (docId >= 0)
                {
                    Debug.Log($"{LogPrefix}Memory fallback succeeded for '{path}'");
                }
                return docId >= 0;
            }
            finally
            {
                Marshal.FreeHGlobal(data);
            }
        }

        public static bool WriteImmAsQuillProject(string immPath, string outputFolder, bool includePictures = false)
        {
            Sequence sequence = ReadImmAsSequence(immPath, includePictures);
            if (sequence == null)
            {
                return false;
            }

            QuillSequenceWriter.Write(sequence, outputFolder);
            return true;
        }

        private static Sequence ConvertHierarchyDocument(int docId, bool includePictures, List<SharpQuillConversionIssue> issues, int remainingSoundBytes)
        {
            if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetDocumentInfo(docId, out var document) ||
                !ImmPlayer.ImmStrokeReader.StrokeReader_GetRootLayerProperties(docId, out var rootInfo, out var rootTransform))
                throw new InvalidDataException("Root/document metadata could not be read.");
            if (document.frameRate > int.MaxValue)
                throw new NotSupportedException("The native document frame rate cannot be represented by SharpQuill.");
            foreach (float component in new[] { document.backgroundR, document.backgroundG, document.backgroundB })
                if (float.IsNaN(component) || float.IsInfinity(component)) throw new InvalidDataException("Nonfinite native background color.");
            var root = new LayerGroup(rootInfo.name);
            var sequence = new Sequence { RootLayer = root, DefaultViewpoint = "", Framerate = document.frameRate == 0 ? 24 : (int)document.frameRate,
                BackgroundColor = new Color(document.backgroundR, document.backgroundG, document.backgroundB) };
            if (document.frameRate == 0)
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.RootMetadataNotPreserved, 0, rootInfo.name,
                    "Native document frame rate is zero; the Quill sequence uses 24 fps while paint rates are retained.", 0));
            ApplyAuthoringProperties(root, rootInfo, rootTransform);
            ReadAuthoringKeys(docId, -1, document.rootAnimationKeyCount, root, rootInfo, issues);
            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.RootMetadataNotPreserved, 0, rootInfo.name,
                "Original Quill export range/startup/tracking-origin navigation is not reconstructed; root properties, supported root keys and representable native default viewpoint selection are copied.", 0));
            if (rootInfo.isTimeline == 0)
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.RootMetadataNotPreserved, 0, rootInfo.name,
                    "The native root timeline flag is retained in memory, but the Quill reader normalizes project roots to timelines.", 0));
            var records = new List<(int Index, StrokeAuthoringLayerInfo Info)>();
            var byId = new Dictionary<int, StrokeAuthoringLayerInfo>();
            int count = ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerCount(docId);
            for (int index = 0; index < count; index++)
            {
                if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerInfo(docId, index, out var info) || info.id <= 0 || byId.ContainsKey(info.id))
                    throw new InvalidDataException($"Invalid authoring layer identity at index {index}.");
                records.Add((index, info)); byId.Add(info.id, info);
            }
            var pictures = new Dictionary<int, int>();
            if (includePictures)
                for (int index = 0; index < ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerCount(docId); index++)
                {
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerInfo(docId, index, out var legacy))
                        throw new InvalidDataException($"Legacy layer {index} could not be inspected.");
                    if (legacy.type == 4) pictures.Add(legacy.id, index);
                }
            var layers = new Dictionary<int, Layer>();
            uint nextStrokeId = 0;
            foreach (var record in records)
            {
                var info = record.Info;
                bool ancestorsSupported = true;
                var visited = new HashSet<int> { info.id };
                int parent = info.parentId;
                while (parent != 0)
                {
                    if (!visited.Add(parent) || !byId.TryGetValue(parent, out var ancestor))
                        throw new InvalidDataException($"Missing or cyclic parent chain for '{info.name}'.");
                    ancestorsSupported &= ancestor.type == 0; parent = ancestor.parentId;
                }
                if (!ancestorsSupported || (info.type != 0 && info.type != 1 && info.type != 4 && info.type != 5 && info.type != 8))
                {
                    issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LayerOmitted, info.id, info.name,
                        ancestorsSupported ? $"Native layer type {info.type} is not mapped in the hierarchy reader." : "The branch has an unsupported parent and was omitted.", info.type));
                    continue;
                }
                if (info.type == 4 && !includePictures)
                {
                    issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.PictureExcluded, info.id, info.name, "Picture inclusion was disabled.", info.type));
                    continue;
                }
                Layer layer;
                if (info.type == 0) layer = new LayerGroup(info.name);
                else if (info.type == 1) layer = ReadAuthoringPaint(docId, record.Index, info, ref nextStrokeId, issues);
                else if (info.type == 8)
                {
                    layer = ReadAuthoringViewpoint(docId, record.Index, info, issues);
                    if (layer == null) continue;
                }
                else if (info.type == 5)
                {
                    layer = ReadAuthoringSound(docId, record.Index, info, issues, ref remainingSoundBytes);
                    if (layer == null) continue;
                }
                else
                {
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetPictureAssetInfo(docId, record.Index, out var picture) ||
                        picture.contentType < 0 || picture.contentType > 2)
                    {
                        issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.UnsupportedPictureType, info.id, info.name,
                            "Picture projection is not supported by the hierarchy reader.", info.type));
                        continue;
                    }
                    if (!pictures.TryGetValue(info.id, out int legacyIndex)) throw new InvalidDataException($"Decoded picture '{info.name}' is unavailable.");
                    layer = ConvertPictureLayer(docId, legacyIndex, info.legacy);
                    if (layer == null) throw new InvalidDataException($"Picture '{info.name}' could not be read.");
                }
                if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerTransform(docId, record.Index, out var local, out _))
                    throw new InvalidDataException($"Local transform for '{info.name}' could not be read.");
                ApplyAuthoringProperties(layer, info, local);
                ReadAuthoringKeys(docId, record.Index, ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerAnimationKeyCount(docId, record.Index), layer, info, issues);
                layers.Add(info.id, layer);
            }
            records.Sort((a, b) => a.Info.childIndex.CompareTo(b.Info.childIndex));
            foreach (var record in records)
            {
                if (!layers.TryGetValue(record.Info.id, out var layer)) continue;
                var parent = record.Info.parentId == 0 ? root : (LayerGroup)layers[record.Info.parentId];
                parent.Children.Add(layer);
            }
            var defaults = records.FindAll(x => x.Info.type == 8 && x.Info.isDefaultSpawn != 0);
            if (defaults.Count == 1 && layers.TryGetValue(defaults[0].Info.id, out var defaultLayer) && defaultLayer is LayerViewpoint)
            {
                string PathFor(int id)
                {
                    var segments = new List<string>();
                    while (id != 0) { segments.Add(byId[id].name); id = byId[id].parentId; }
                    segments.Add(root.Name); segments.Reverse();
                    return segments.Exists(x => string.IsNullOrEmpty(x) || x.Contains("/")) ? null : string.Join("/", segments);
                }
                string defaultPath = PathFor(defaults[0].Info.id);
                int matches = 0;
                foreach (var record in records)
                    if (layers.TryGetValue(record.Info.id, out var candidate) && candidate is LayerViewpoint && PathFor(record.Info.id) == defaultPath) matches++;
                if (defaultPath != null && matches == 1) sequence.DefaultViewpoint = defaultPath;
                else issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ViewpointMetadataNotPreserved,
                    defaults[0].Info.id, defaults[0].Info.name, "Default viewpoint path is ambiguous or contains an unrepresentable name; no default is declared.", 8));
            }
            else if (defaults.Count != 0)
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ViewpointMetadataNotPreserved, 0, root.Name,
                    "The native default viewpoint is omitted or not unique; no Quill default is declared.", 8));
            sequence.LastStrokeId = nextStrokeId == 0 ? 0 : nextStrokeId - 1;
            return sequence;
        }

        private static LayerViewpoint ReadAuthoringViewpoint(int docId, int index, StrokeAuthoringLayerInfo info,
            List<SharpQuillConversionIssue> issues)
        {
            if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerSpawnAreaInfo(docId, index, out var spawn))
                throw new InvalidDataException($"Viewpoint '{info.name}' metadata could not be read.");
            if (spawn.volumeType != 0)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LayerOmitted, info.id, info.name,
                    $"Native viewpoint volume type {spawn.volumeType} has no SharpQuill sphere representation.", info.type));
                return null;
            }
            foreach (float value in new[] { spawn.volumeOffsetX, spawn.volumeOffsetY, spawn.volumeOffsetZ, spawn.volumeExtentX })
                if (float.IsNaN(value) || float.IsInfinity(value)) throw new InvalidDataException($"Nonfinite viewpoint volume for '{info.name}'.");
            if (spawn.volumeExtentX < 0 || spawn.locomotion < 0 || spawn.locomotion > 7 || (spawn.isFloorLevel != 0 && spawn.isFloorLevel != 1))
                throw new InvalidDataException($"Invalid viewpoint settings for '{info.name}'.");
            var layer = LayerViewpoint.CreateDefault();
            layer.Name = info.name; layer.TypeStr = spawn.IsFloorLevel ? "FloorLevel" : "EyeLevel";
            layer.Sphere = new SharpQuill.Vector4(spawn.volumeOffsetX, spawn.volumeOffsetY, spawn.volumeOffsetZ, spawn.volumeExtentX);
            layer.AllowTranslationX = spawn.AllowTranslationX; layer.AllowTranslationY = spawn.AllowTranslationY; layer.AllowTranslationZ = spawn.AllowTranslationZ;
            layer.Exporting = true;
            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ViewpointMetadataNotPreserved, info.id, info.name,
                "Native viewpoint availability is copied to Quill gizmo visibility; original gizmo color/show-volume/version/import metadata is unavailable and uses SharpQuill defaults. Source navigation behavior is not certified.", info.type));
            if (ImmPlayer.ImmStrokeReader.StrokeReader_GetSpawnAreaPreviewInfo(docId, index, out _))
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ViewpointMetadataNotPreserved, info.id, info.name,
                    "Stored viewpoint preview is available in the native reader but is not yet written to the Quill project.", info.type));
            return layer;
        }

        private static LayerSound ReadAuthoringSound(int docId, int index, StrokeAuthoringLayerInfo info,
            List<SharpQuillConversionIssue> issues, ref int remainingBytes)
        {
            if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetSoundAssetInfo(docId, index, out var asset))
                throw new InvalidDataException($"Sound metadata for '{info.name}' could not be read.");
            var sound = asset.properties;
            if (asset.sampleRate <= 0 || asset.byteCount <= 0)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                    "Sound asset framing or codec metadata is invalid; no partial sequence returned.", info.type));
                throw new InvalidDataException($"Sound '{info.name}' has invalid encoded data.");
            }
            if ((asset.channels != 1 && asset.channels != 2) || (sound.spatialType != 0 && sound.spatialType != 2) ||
                sound.modifier < 0 || sound.modifier > 1 || (sound.modifier == 1 && sound.spatialType != 2))
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LayerOmitted, info.id, info.name,
                    $"Sound channels {asset.channels}, spatial type {sound.spatialType} or modifier {sound.modifier} cannot be represented by SharpQuill.", info.type));
                return null;
            }
            foreach (float value in new[] { sound.gain, sound.distanceMin, sound.distanceMax, sound.angleInX,
                sound.angleInY, sound.angleBand, sound.attenuationOutside })
                if (float.IsNaN(value) || float.IsInfinity(value) || value < 0)
                    throw new InvalidDataException($"Nonfinite or negative sound setting for '{info.name}'.");
            if (sound.attenuation < 0 || sound.attenuation > 2 || sound.distanceMax < sound.distanceMin ||
                (sound.loop != 0 && sound.loop != 1)) throw new InvalidDataException($"Invalid sound setting for '{info.name}'.");
            if (sound.angleInY != 0 || (sound.modifier == 0 && (sound.angleInX != 0 || sound.angleBand != 0 || sound.attenuationOutside != 0)))
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LayerOmitted, info.id, info.name,
                    "Sound modifier fields have no exact SharpQuill representation.", info.type));
                return null;
            }
            StrokeSoundPcmInfo pcm = default;
            long lease = remainingBytes == 0 ? -2 : ImmPlayer.ImmStrokeReader.StrokeReader_DecodeSoundPcm16(docId, index, remainingBytes, out pcm);
            if (lease <= 0)
            {
                issues.Add(new SharpQuillConversionIssue(lease == -2 ? SharpQuillConversionIssueCode.ResourceLimit : SharpQuillConversionIssueCode.DataReadFailed,
                    info.id, info.name, $"PCM16 decode failed ({lease}); remaining aggregate PCM budget is {remainingBytes} bytes.", info.type));
                throw new InvalidDataException($"Sound '{info.name}' could not be decoded; no partial sequence returned.");
            }
            try
            {
                if (pcm.byteCount <= 0 || pcm.byteCount > remainingBytes || pcm.sampleRate != asset.sampleRate || pcm.channels != asset.channels ||
                    (long)pcm.frameCount * pcm.channels * 2 != pcm.byteCount)
                    throw new InvalidDataException($"Decoded sound metadata for '{info.name}' is inconsistent.");
                byte[] bytes = new byte[pcm.byteCount];
                if (ImmPlayer.ImmStrokeReader.StrokeReader_CopyDecodedSoundPcm16(lease, bytes, bytes.Length) != bytes.Length)
                    throw new InvalidDataException($"Decoded sound '{info.name}' could not be copied.");
                byte[] wave;
                using (var stream = new MemoryStream(checked(bytes.Length + 44)))
                {
                    using (var writer = new BinaryWriter(stream, System.Text.Encoding.ASCII, true))
                    {
                        writer.Write(System.Text.Encoding.ASCII.GetBytes("RIFF")); writer.Write(checked(bytes.Length + 36));
                        writer.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt ")); writer.Write(16); writer.Write((ushort)1);
                        writer.Write((ushort)pcm.channels); writer.Write(pcm.sampleRate); writer.Write(checked(pcm.sampleRate * pcm.channels * 2));
                        writer.Write((ushort)(pcm.channels * 2)); writer.Write((ushort)16);
                        writer.Write(System.Text.Encoding.ASCII.GetBytes("data")); writer.Write(bytes.Length); writer.Write(bytes);
                    }
                    wave = stream.ToArray();
                }
                remainingBytes -= bytes.Length;
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.SoundMetadataNotPreserved, info.id, info.name,
                    $"Encoded sound was decoded to PCM16; original codec bytes, import path, native source duration ({asset.sourceDuration}) and initially-playing field ({asset.initiallyPlaying}) are not reconstructed.", info.type));
                return new LayerSound(info.name) { SoundType = sound.spatialType == 0 ? SoundType.Flat : SoundType.Positional,
                    Gain = sound.gain, Loop = sound.loop != 0,
                    Attenuation = new SoundAttenuation { Mode = (SoundAttenuationMode)sound.attenuation, Minimum = sound.distanceMin, Maximum = sound.distanceMax },
                    Modifiers = new List<SoundModifier> { new SoundModifier { Type = (SoundModifierType)sound.modifier,
                        AngleIn = sound.angleInX, AngleBand = sound.angleBand, AttenOut = sound.attenuationOutside } },
                    Data = new SoundData { AudioBytes = wave, Length = wave.Length } };
            }
            finally { ImmPlayer.ImmStrokeReader.StrokeReader_ReleaseDecodedSoundPcm16(lease); }
        }

        private static float ExactTimingFloat(long value)
        {
            float converted = value;
            if (value < 0 || converted >= 9223372036854775808f || (long)converted != value)
                throw new NotSupportedException($"Timing value {value} cannot be represented exactly by SharpQuill's float fields.");
            return converted;
        }

        private static Transform AuthoringTransform(StrokeLayerTransform value)
        {
            if (value.flip < 0 || value.flip > 3) throw new NotSupportedException($"Unknown native transform flip {value.flip}.");
            foreach (float component in new[] { value.rotX, value.rotY, value.rotZ, value.rotW, value.scale, value.transX, value.transY, value.transZ })
                if (float.IsNaN(component) || float.IsInfinity(component)) throw new InvalidDataException("Nonfinite native transform.");
            return new Transform { Rotation = new Quaternion(value.rotX, value.rotY, value.rotZ, value.rotW), Scale = value.scale,
                Flip = ConvertFlip(value.flip), Translation = new SharpQuill.Vector3(value.transX, value.transY, value.transZ) };
        }

        private static void ApplyAuthoringProperties(Layer layer, StrokeAuthoringLayerInfo info, StrokeLayerTransform local)
        {
            if (float.IsNaN(info.opacity) || float.IsInfinity(info.opacity)) throw new InvalidDataException("Nonfinite native opacity.");
            layer.Visible = info.visible != 0; layer.Opacity = info.opacity; layer.Transform = AuthoringTransform(local);
            layer.Pivot = AuthoringTransform(new StrokeLayerTransform { rotX = info.pivotRotX, rotY = info.pivotRotY, rotZ = info.pivotRotZ,
                rotW = info.pivotRotW, scale = info.pivotScale, flip = info.pivotFlip, transX = info.pivotTransX, transY = info.pivotTransY, transZ = info.pivotTransZ });
            layer.Animation = new SharpQuill.Animation { Timeline = info.isTimeline != 0, Duration = ExactTimingFloat(info.durationTicks),
                MaxRepeatCount = ExactTimingFloat(info.maxRepeatCount) };
            layer.Animation.Keys.Visibility.Clear(); layer.Animation.Keys.Offset.Clear();
        }

        private static void ReadAuthoringKeys(int docId, int layerIndex, int count, Layer layer, StrokeAuthoringLayerInfo info, List<SharpQuillConversionIssue> issues)
        {
            for (int index = 0; index < count; index++)
            {
                StrokeAnimationKey key;
                bool read = layerIndex < 0 ? ImmPlayer.ImmStrokeReader.StrokeReader_GetRootAnimationKey(docId, index, out key) :
                    ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerAnimationKey(docId, layerIndex, index, out key);
                if (!read) throw new InvalidDataException($"Animation key {index} for '{info.name}' could not be read.");
                Interpolation interpolation;
                switch (key.interpolation)
                {
                    case 0: interpolation = Interpolation.None; break;
                    case 1: interpolation = Interpolation.Linear; break;
                    case 2: interpolation = Interpolation.Smoothstep; break;
                    case 3: interpolation = Interpolation.EaseIn; break;
                    case 4: interpolation = Interpolation.EaseOut; break;
                    default:
                        issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.AnimationNotPreserved, info.id, info.name,
                            $"Key property {key.property} at {key.timeTicks} uses unsupported interpolation {key.interpolation} and was omitted.", info.type));
                        continue;
                }
                if (key.property != 0 && key.property != 1 && key.property != 8 && key.property != 9)
                {
                    issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.AnimationNotPreserved, info.id, info.name,
                        $"Native animation property {key.property} at {key.timeTicks} has no verified SharpQuill mapping and was omitted.", info.type));
                    continue;
                }
                if (key.timeTicks < int.MinValue || key.timeTicks > int.MaxValue)
                    throw new NotSupportedException($"Key time {key.timeTicks} cannot fit SharpQuill's integer time field.");
                int time = (int)key.timeTicks;
                switch (key.property)
                {
                    case 0: layer.Animation.Keys.Visibility.Add(new Keyframe<bool>(time, key.boolValue != 0, interpolation)); break;
                    case 1:
                        if (float.IsNaN(key.floatValue) || float.IsInfinity(key.floatValue)) throw new InvalidDataException("Nonfinite opacity key.");
                        layer.Animation.Keys.Opacity.Add(new Keyframe<float>(time, key.floatValue, interpolation)); break;
                    case 8:
                        if (key.intValue > int.MaxValue) throw new NotSupportedException($"Offset {key.intValue} cannot fit SharpQuill's signed integer field.");
                        layer.Animation.Keys.Offset.Add(new Keyframe<int>(time, (int)key.intValue, interpolation));
                        if (time == 0) layer.Animation.StartOffset = ExactTimingFloat(key.intValue);
                        break;
                    case 9: layer.Animation.Keys.Transform.Add(new Keyframe<Transform>(time, AuthoringTransform(key.transformValue), interpolation)); break;
                }
            }
        }

        private static LayerPaint ReadAuthoringPaint(int docId, int index, StrokeAuthoringLayerInfo info, ref uint nextStrokeId,
            List<SharpQuillConversionIssue> issues)
        {
            if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerAnimationInfo(docId, index, out int rate, out int frameCount, out int repeats) || rate <= 0 || frameCount < 0)
                throw new InvalidDataException($"Invalid paint animation metadata for '{info.name}'.");
            if (repeats < 0) throw new NotSupportedException("Native unsigned paint repeat count cannot fit SharpQuill's signed field.");
            var paint = new LayerPaint(info.name) { Framerate = rate, MaxRepeatCount = repeats };
            int drawings = ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringDrawingCount(docId, index);
            for (int drawingIndex = 0; drawingIndex < drawings; drawingIndex++)
            {
                var drawing = new Drawing();
                int strokes = ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringStrokeCount(docId, index, drawingIndex);
                for (int strokeIndex = 0; strokeIndex < strokes; strokeIndex++)
                {
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringStrokeInfo(docId, index, drawingIndex, strokeIndex, out var strokeInfo) || strokeInfo.numPoints < 0)
                        throw new InvalidDataException($"Stroke metadata for '{info.name}' could not be read.");
                    if (strokeInfo.brushType < 1 || strokeInfo.brushType > 4 || (strokeInfo.visibilityMode != 0 && strokeInfo.visibilityMode != 1))
                        throw new NotSupportedException($"Native brush/visibility {strokeInfo.brushType}/{strokeInfo.visibilityMode} has no supported Quill mapping.");
                    var points = new StrokeAuthoringPoint[strokeInfo.numPoints];
                    if (points.Length != 0 && !ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringStrokePoints(docId, index, drawingIndex, strokeIndex, points, points.Length))
                        throw new InvalidDataException($"Stroke points for '{info.name}' could not be read.");
                    var stroke = new Stroke { Id = checked(nextStrokeId++), BrushType = ConvertBrushType(strokeInfo.brushType), DisableRotationalOpacity = strokeInfo.visibilityMode == 1,
                        BoundingBox = new BoundingBox(strokeInfo.bboxMinX, strokeInfo.bboxMaxX, strokeInfo.bboxMinY, strokeInfo.bboxMaxY, strokeInfo.bboxMinZ, strokeInfo.bboxMaxZ) };
                    foreach (var point in points)
                    {
                        var value = point.legacy;
                        stroke.Vertices.Add(new Vertex(new SharpQuill.Vector3(value.px, value.py, value.pz), new SharpQuill.Vector3(value.nx, value.ny, value.nz),
                            new SharpQuill.Vector3(value.dx, value.dy, value.dz), new Color(value.r, value.g, value.b), value.alpha, value.width));
                    }
                    drawing.Data.Strokes.Add(stroke);
                }
                drawing.UpdateBoundingBox(false); paint.Drawings.Add(drawing);
            }
            var frames = new int[frameCount];
            if (frameCount != 0 && ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringFrameBuffer(docId, index, frames, frames.Length) != frames.Length)
                throw new InvalidDataException($"Paint frame map for '{info.name}' could not be read.");
            foreach (int frame in frames)
            {
                if (frame < 0 || frame >= drawings) throw new InvalidDataException($"Paint frame map for '{info.name}' references a missing drawing.");
                paint.Frames.Add(frame);
            }
            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.StrokeMetadataNotPreserved, info.id, info.name,
                "Stroke IDs are reconstructed sequentially; native quantization scale/point length/time are not explicit Quill fields. Re-export can requantize geometry.", info.type));
            return paint;
        }

        private static void CaptureLossReport(int docId, bool includePictures, int chapterIndex,
            List<SharpQuillConversionIssue> issues)
        {
            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.RootMetadataNotPreserved, 0, "",
                "Sequence/root properties, root keys and default viewpoint are not reconstructed; SharpQuill defaults are used."));
            if (chapterIndex >= 0)
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.ChapterSubset, 0, "",
                    $"Paint frame selection is restricted to chapter {chapterIndex}; this is not the full animation."));
            try
            {
                int count = ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerCount(docId);
                for (int index = 0; index < count; index++)
                {
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetAuthoringLayerInfo(docId, index, out var layer))
                    {
                        issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DiagnosticUnavailable, -1, "",
                            $"Authoring layer {index} could not be inspected; the report is incomplete."));
                        continue;
                    }
                    if (layer.type == 0 || layer.parentId != 0)
                        issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.FlattenedHierarchy, layer.id, layer.name,
                            "Groups are not reconstructed; converted paint/picture layers are placed directly under the output root.", layer.type));
                    if (ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerAnimationKeyCount(docId, index) > 0 ||
                        layer.isTimeline != 0 || layer.durationTicks != 0 || layer.maxRepeatCount != 0)
                        issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.AnimationNotPreserved, layer.id, layer.name,
                            "Layer key channels and nested timeline properties are not copied; paint frame buffers use the legacy adapter path.", layer.type));
                    if (layer.type == 4)
                    {
                        if (!includePictures)
                            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.PictureExcluded, layer.id, layer.name,
                                "Picture content was excluded by includePictures=false.", layer.type));
                        else if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetPictureAssetInfo(docId, index, out var picture))
                            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DiagnosticUnavailable, layer.id, layer.name,
                                "Picture projection could not be inspected; the report is incomplete.", layer.type));
                        else if (picture.contentType < 0 || picture.contentType > 2)
                            issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.UnsupportedPictureType, layer.id, layer.name,
                                "The adapter cannot map this native picture projection to a SharpQuill picture type.", layer.type));
                    }
                    else if (layer.type != 0 && layer.type != 1)
                        issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.LayerOmitted, layer.id, layer.name,
                            $"Native layer type {layer.type} is not converted (including sound and spawn metadata).", layer.type));
                }
            }
            catch (EntryPointNotFoundException)
            {
                issues.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DiagnosticUnavailable, -1, "",
                    "This reader binary lacks the authoring APIs required for a complete loss report."));
            }
        }

        private static Sequence ConvertDocument(int docId, bool includePictures, int chapterIndex = -1,
            List<SharpQuillConversionIssue> issues = null)
        {
            Sequence sequence = Sequence.CreateDefault();
            if (sequence.RootLayer == null)
            {
                return sequence;
            }

            uint maxStrokeId = 0;
            int layerCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerCount(docId);
            for (int layerIdx = 0; layerIdx < layerCount; layerIdx++)
            {
                if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerInfo(docId, layerIdx, out StrokeLayerInfo info))
                {
                    issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, -1, "",
                        $"Legacy layer {layerIdx} could not be read."));
                    continue;
                }

                // Spawn areas are metadata, not paint. Converting them to LayerPaint
                // creates empty layers in consumers such as Open Brush.
                if (info.type == 8)
                {
                    continue;
                }

                bool isPicture = info.type == 4;
                if (isPicture && !includePictures)
                {
                    continue;
                }

                Layer layer = isPicture
                    ? ConvertPictureLayer(docId, layerIdx, info, issues)
                    : ConvertPaintLayer(docId, layerIdx, info, ref maxStrokeId, chapterIndex, issues);

                if (layer != null)
                {
                    sequence.RootLayer.Children.Add(layer);
                }
                else
                    issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                        "The legacy layer could not be converted."));
            }

            sequence.LastStrokeId = maxStrokeId;
            return sequence;
        }

        private static LayerPaint ConvertPaintLayer(int docId, int layerIdx, StrokeLayerInfo info, ref uint maxStrokeId, int chapterIndex = -1, List<SharpQuillConversionIssue> issues = null)
        {
            LayerPaint layer = new LayerPaint(info.name);
            ApplyCommonLayerProperties(layer, docId, layerIdx, info, issues);

            // Get animation info
            int frameRate = 24;
            int numFrames = 0;
            int maxRepeatCount = 0;
            bool hasAnimationInfo = ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerAnimationInfo(docId, layerIdx, out frameRate, out numFrames, out maxRepeatCount);
            
            if (hasAnimationInfo)
            {
                layer.Framerate = frameRate;
                layer.MaxRepeatCount = maxRepeatCount;
            }

            if (!hasAnimationInfo)
                issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                    "Legacy paint animation metadata could not be read.", info.type));

            // Get frame buffer (maps timeline frames to drawing indices)
            int[] frameBuffer = null;
            if (numFrames > 0)
            {
                frameBuffer = new int[numFrames];
                int framesRead = ImmPlayer.ImmStrokeReader.StrokeReader_GetFrameBuffer(docId, layerIdx, frameBuffer, numFrames);
                if (framesRead != numFrames)
                {
                    Debug.LogWarning($"{LogPrefix}Frame buffer mismatch: expected {numFrames}, got {framesRead}");
                    issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                        $"Paint frame buffer read {framesRead} of {numFrames} entries; fallback frame mapping is used.", info.type));
                    frameBuffer = null;
                }
            }

            // Load all drawings
            int drawingCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetDrawingCount(docId, layerIdx);
            for (int drawingIdx = 0; drawingIdx < drawingCount; drawingIdx++)
            {
                Drawing drawing = new Drawing();
                int strokeCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetStrokeCount(docId, layerIdx, drawingIdx);

                for (int strokeIdx = 0; strokeIdx < strokeCount; strokeIdx++)
                {
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetStrokeInfo(docId, layerIdx, drawingIdx, strokeIdx, out StrokeInfo strokeInfo))
                    {
                        issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                            $"Stroke metadata could not be read at drawing {drawingIdx}, stroke {strokeIdx}.", info.type));
                        continue;
                    }

                    if (strokeInfo.brushType < 1 || strokeInfo.brushType > 4)
                        issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.UnsupportedBrush, info.id, info.name,
                            $"Native brush {strokeInfo.brushType} uses the legacy Cylinder fallback at drawing {drawingIdx}, stroke {strokeIdx}.", info.type));

                    if (strokeInfo.numPoints <= 0)
                    {
                        continue;
                    }

                    StrokePoint[] points = new StrokePoint[strokeInfo.numPoints];
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetStrokePoints(docId, layerIdx, drawingIdx, strokeIdx, points, points.Length))
                    {
                        issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                            $"Stroke points could not be read at drawing {drawingIdx}, stroke {strokeIdx}.", info.type));
                        continue;
                    }

                    uint strokeId = (uint)strokeIdx;
                    if (strokeId > maxStrokeId)
                    {
                        maxStrokeId = strokeId;
                    }

                    Stroke stroke = new Stroke
                    {
                        Id = strokeId,
                        BrushType = ConvertBrushType(strokeInfo.brushType),
                        DisableRotationalOpacity = strokeInfo.visibilityMode == 1,
                        BoundingBox = new BoundingBox(
                            strokeInfo.bboxMinX,
                            strokeInfo.bboxMinY,
                            strokeInfo.bboxMinZ,
                            strokeInfo.bboxMaxX,
                            strokeInfo.bboxMaxY,
                            strokeInfo.bboxMaxZ)
                    };

                    for (int p = 0; p < points.Length; p++)
                    {
                        StrokePoint pt = points[p];
                        stroke.Vertices.Add(new Vertex(
                            new SharpQuill.Vector3(pt.px, pt.py, pt.pz),
                            new SharpQuill.Vector3(pt.nx, pt.ny, pt.nz),
                            new SharpQuill.Vector3(pt.dx, pt.dy, pt.dz),
                            new Color(pt.r, pt.g, pt.b),
                            pt.alpha,
                            pt.width));
                    }

                    drawing.Data.Strokes.Add(stroke);
                }

                drawing.UpdateBoundingBox(true);
                layer.Drawings.Add(drawing);
            }

            // Build frame buffer using original mapping
            if (frameBuffer != null && numFrames > 0)
            {
                for (int i = 0; i < numFrames; i++)
                {
                    int drawingIndex = frameBuffer[i];
                    // Clamp to valid range
                    if (drawingIndex < 0 || drawingIndex >= layer.Drawings.Count)
                    {
                        drawingIndex = 0;
                    }
                    layer.Frames.Add(drawingIndex);
                }
            }
            else if (layer.Drawings.Count > 0)
            {
                // Fallback: 1:1 mapping if no frame buffer available
                for (int i = 0; i < layer.Drawings.Count; i++)
                {
                    layer.Frames.Add(i);
                }
            }
            else
            {
                // No drawings at all - add empty one
                layer.Drawings.Add(new Drawing());
                layer.Frames.Add(0);
            }

            // For chapter-specific loading, override Frames to point at the single
            // drawing that corresponds to the requested chapter's start frame.
            if (chapterIndex >= 0 && layer.Drawings.Count > 0)
            {
                int drawingIndex = ImmPlayer.ImmStrokeReader.StrokeReader_GetDrawingIndexForChapter(docId, layerIdx, chapterIndex);
                if (drawingIndex < 0 || drawingIndex >= layer.Drawings.Count)
                    drawingIndex = 0;
                layer.Frames.Clear();
                layer.Frames.Add(drawingIndex);
            }

            return layer;
        }

        private static LayerPicture ConvertPictureLayer(int docId, int layerIdx, StrokeLayerInfo info, List<SharpQuillConversionIssue> issues = null)
        {
            if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetPictureInfo(docId, layerIdx, out StrokePictureInfo pictureInfo))
            {
                return null;
            }

            if (pictureInfo.width <= 0 || pictureInfo.height <= 0 || pictureInfo.dataSize <= 0)
            {
                return null;
            }

            byte[] pixels = new byte[pictureInfo.dataSize];
            IntPtr buffer = Marshal.AllocHGlobal(pictureInfo.dataSize);
            try
            {
                int bytesRead = ImmPlayer.ImmStrokeReader.StrokeReader_GetPicturePixelData(docId, layerIdx, buffer, pictureInfo.dataSize);
                if (bytesRead <= 0)
                {
                    return null;
                }

                Marshal.Copy(buffer, pixels, 0, pictureInfo.dataSize);
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }

            LayerPicture layer = new LayerPicture(info.name)
            {
                PictureType = ConvertPictureType(pictureInfo.contentType),
                ViewerLocked = pictureInfo.isViewerLocked != 0,
                Data = new PictureData
                {
                    Width = pictureInfo.width,
                    Height = pictureInfo.height,
                    HasAlpha = pictureInfo.hasAlpha != 0,
                    Pixels = pixels
                }
            };

            ApplyCommonLayerProperties(layer, docId, layerIdx, info, issues);
            return layer;
        }

        private static void ApplyCommonLayerProperties(Layer layer, int docId, int layerIdx, StrokeLayerInfo info, List<SharpQuillConversionIssue> issues = null)
        {
            layer.Visible = info.visible != 0;
            layer.Opacity = info.opacity;
            layer.Pivot = ConvertPivot(info);

            // All IMM layers are placed as direct children of the sequence root (hierarchy is flattened).
            // Use the world transform so that ancestor transforms are correctly included.
            if (ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerTransform(docId, layerIdx, out StrokeLayerTransform _, out StrokeLayerTransform world))
            {
                layer.Transform = ConvertTransform(world);
            }
            else
            {
                layer.Transform = Transform.Identity;
                issues?.Add(new SharpQuillConversionIssue(SharpQuillConversionIssueCode.DataReadFailed, info.id, info.name,
                    "World transform could not be read; identity fallback was used.", info.type));
            }
        }

        private static Transform ConvertTransform(StrokeLayerTransform source)
        {
            float rotW = source.rotW;
            if (source.rotX == 0f && source.rotY == 0f && source.rotZ == 0f && source.rotW == 0f)
            {
                rotW = 1f;
            }

            float scale = source.scale;
            if (scale == 0f || float.IsNaN(scale) || float.IsInfinity(scale))
            {
                scale = 1f;
            }

            return new Transform
            {
                Rotation = new Quaternion(source.rotX, source.rotY, source.rotZ, rotW),
                Scale = scale,
                Flip = ConvertFlip(source.flip),
                Translation = new SharpQuill.Vector3(source.transX, source.transY, source.transZ)
            };
        }

        private static Transform ConvertPivot(StrokeLayerInfo source)
        {
            float rotW = source.pivotRotW;
            if (source.pivotRotX == 0f && source.pivotRotY == 0f && source.pivotRotZ == 0f && source.pivotRotW == 0f)
            {
                rotW = 1f;
            }

            float scale = source.pivotScale;
            if (scale == 0f || float.IsNaN(scale) || float.IsInfinity(scale))
            {
                scale = 1f;
            }

            return new Transform
            {
                Rotation = new Quaternion(source.pivotRotX, source.pivotRotY, source.pivotRotZ, rotW),
                Scale = scale,
                Flip = ConvertFlip(source.pivotFlip),
                Translation = new SharpQuill.Vector3(source.pivotTransX, source.pivotTransY, source.pivotTransZ)
            };
        }

        private static BrushType ConvertBrushType(int immBrushType)
        {
            switch (immBrushType)
            {
                case 1:
                    return BrushType.Ribbon;
                case 2:
                    return BrushType.Cylinder;
                case 3:
                    return BrushType.Ellipse;
                case 4:
                    return BrushType.Cube;
                default:
                    return BrushType.Cylinder;
            }
        }

        private static PictureType ConvertPictureType(int contentType)
        {
            switch (contentType)
            {
                case 0:
                    return PictureType.TwoD;
                case 1:
                    return PictureType.ThreeSixty_Equirect_Mono;
                case 2:
                    return PictureType.ThreeSixty_Equirect_Stereo;
                default:
                    return PictureType.Unknown;
            }
        }

        private static string ConvertFlip(int flip)
        {
            switch (flip)
            {
                case 1:
                    return "X";
                case 2:
                    return "Y";
                case 3:
                    return "Z";
                default:
                    return "N";
            }
        }
    }
}
