using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using SharpQuill;
using Adapter = ImmStrokeReader.SharpQuillCompat;
using Issue = ImmStrokeReader.SharpQuillConversionIssueCode;
using Reader = ImmPlayer.ImmStrokeReader;

static class HierarchySoundChecks
{
    public static void Run(string path, string directory)
    {
        var result = Adapter.ReadImmAsHierarchyWithReport(path);
        Require(result.Succeeded, $"Hierarchy sound read failed: {string.Join("; ", result.Issues.Select(x => x.Message))}");
        var layers = Walk(result.Sequence.RootLayer).ToArray();
        var sounds = layers.OfType<LayerSound>().ToArray();
        int doc = Reader.StrokeReader_LoadFromFile(path), expected = 0, totalBytes = 0;
        Require(doc > 0, "Sound reference load failed.");
        try
        {
            for (int index = 0; index < Reader.StrokeReader_GetAuthoringLayerCount(doc); index++)
            {
                if (!Reader.StrokeReader_GetSoundAssetInfo(doc, index, out var asset)) continue;
                Require(Reader.StrokeReader_GetAuthoringLayerInfo(doc, index, out var info), "Sound reference layer missing.");
                var settings = asset.properties;
                bool supported = asset.channels <= 2 && (settings.spatialType == 0 || settings.spatialType == 2) && settings.modifier <= 1;
                if (!supported)
                {
                    Require(!sounds.Any(x => x.Name == info.name) && result.Issues.Any(x => x.Code == Issue.LayerOmitted && x.SourceLayerId == info.id),
                        "Unsupported sound was not explicitly omitted.");
                    continue;
                }
                expected++;
                var sound = sounds.Single(x => x.Name == info.name);
                Require((sound.SoundType == SoundType.Flat ? 0 : 2) == settings.spatialType && sound.Gain == settings.gain &&
                    sound.Loop == (settings.loop != 0) && (int)sound.Attenuation.Mode == settings.attenuation &&
                    sound.Attenuation.Minimum == settings.distanceMin && sound.Attenuation.Maximum == settings.distanceMax &&
                    (int)sound.Modifiers.Single().Type == settings.modifier && sound.Modifiers[0].AngleIn == settings.angleInX &&
                    sound.Modifiers[0].AngleBand == settings.angleBand && sound.Modifiers[0].AttenOut == settings.attenuationOutside &&
                    sound.Visible == (info.visible != 0) && sound.Opacity == info.opacity && sound.Animation.Duration == info.durationTicks &&
                    sound.Animation.MaxRepeatCount == info.maxRepeatCount,
                    "Sound settings/common metadata changed.");
                Require(result.Issues.Any(x => x.Code == Issue.SoundMetadataNotPreserved && x.SourceLayerId == info.id), "Sound loss report missing.");
                long lease = Reader.StrokeReader_DecodeSoundPcm16(doc, index, int.MaxValue, out var pcm);
                Require(lease > 0, "Reference PCM decode failed.");
                try
                {
                    byte[] bytes = new byte[pcm.byteCount];
                    Require(Reader.StrokeReader_CopyDecodedSoundPcm16(lease, bytes, bytes.Length) == bytes.Length &&
                        sound.Data.AudioBytes.Length == bytes.Length + 44 && sound.Data.AudioBytes.AsSpan(44).SequenceEqual(bytes) &&
                        BitConverter.ToInt32(sound.Data.AudioBytes, 24) == pcm.sampleRate &&
                        BitConverter.ToUInt16(sound.Data.AudioBytes, 22) == pcm.channels, "Quill WAV PCM changed.");
                    totalBytes = checked(totalBytes + bytes.Length);
                }
                finally { Reader.StrokeReader_ReleaseDecodedSoundPcm16(lease); }
                Require(Reader.StrokeReader_GetAuthoringLayerTransform(doc, index, out var transform, out _) &&
                    sound.Transform.Translation.X == transform.transX && sound.Transform.Translation.Y == transform.transY &&
                    sound.Transform.Translation.Z == transform.transZ && sound.Pivot.Translation.X == info.pivotTransX &&
                    sound.Pivot.Translation.Y == info.pivotTransY && sound.Pivot.Translation.Z == info.pivotTransZ &&
                    sound.Transform.Flip == new[] { "N", "X", "Y", "Z" }[transform.flip] &&
                    sound.Pivot.Flip == new[] { "N", "X", "Y", "Z" }[info.pivotFlip], "Sound local transform/pivot changed.");
                int keyCount = 0;
                for (int keyIndex = 0; keyIndex < Reader.StrokeReader_GetLayerAnimationKeyCount(doc, index); keyIndex++)
                {
                    Require(Reader.StrokeReader_GetLayerAnimationKey(doc, index, keyIndex, out var key), "Sound key read failed.");
                    if (key.interpolation <= 4 && (key.property == 0 || key.property == 1 || key.property == 8 || key.property == 9)) keyCount++;
                }
                Require(sound.Animation.Keys.Visibility.Count + sound.Animation.Keys.Opacity.Count +
                    sound.Animation.Keys.Offset.Count + sound.Animation.Keys.Transform.Count == keyCount, "Supported sound keys were lost.");
            }
        }
        finally { Reader.StrokeReader_Unload(doc); }
        Require(sounds.Length == expected, "Hierarchy sound count differs.");
        if (totalBytes > 0)
        {
            var limited = Adapter.ReadImmAsHierarchyWithReport(path, maxDecodedSoundBytes: totalBytes - 1);
            Require(!limited.Succeeded && limited.Issues.Any(x => x.Code == Issue.ResourceLimit && x.SourceLayerType == 5),
                "Aggregate sound budget returned a partial success.");
            Require(Adapter.ReadImmAsHierarchyWithReport(path, maxDecodedSoundBytes: totalBytes).Succeeded, "Exact aggregate sound budget rejected.");
        }
        string project = Path.Combine(directory, $"{Path.GetFileNameWithoutExtension(path)}-hierarchy-sound-project");
        QuillSequenceWriter.Write(result.Sequence, project);
        var restored = Walk(QuillSequenceReader.Read(project).RootLayer).OfType<LayerSound>().ToArray();
        Require(restored.Length == sounds.Length, "Project read-back sound count differs.");
        foreach (var sound in sounds)
        {
            var copy = restored.Single(x => x.Name == sound.Name);
            Require(copy.Data.AudioBytes.SequenceEqual(sound.Data.AudioBytes) && copy.Gain == sound.Gain && copy.Loop == sound.Loop &&
                copy.SoundType == sound.SoundType, "Project sound payload/settings changed.");
        }
        Reader.StrokeReader_End();
        Console.WriteLine($"[IMM_HIERARCHY_SOUND] {Path.GetFileName(path)}: {sounds.Length} sound layers, exact decoded PCM/project read-back; aggregate byte cap checked.");
    }

    static IEnumerable<Layer> Walk(Layer layer)
    {
        yield return layer;
        if (layer is LayerGroup group)
            foreach (var child in group.Children)
                foreach (var item in Walk(child)) yield return item;
    }
    static void Require(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
}
