using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImmPlayer;
using Reader = ImmPlayer.ImmStrokeReader;

if (args.Length != 3)
    throw new ArgumentException("Expected native library, sample1.imm, and native log paths.");
string libraryPath = Path.GetFullPath(args[0]);
string samplePath = Path.GetFullPath(args[1]);
string logPath = Path.GetFullPath(args[2]);
Directory.CreateDirectory(Path.GetDirectoryName(logPath));
NativeLibrary.SetDllImportResolver(typeof(Reader).Assembly,
    (name, assembly, searchPath) => name == "ImmStrokeReader" ? NativeLibrary.Load(libraryPath) : IntPtr.Zero);
Require(Reader.StrokeReader_Init(logPath) == 0, $"Reader initialization failed (log: {logPath}).");

foreach (bool includePictures in new[] { false, true })
{
    var sequence = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsSequence(samplePath, includePictures, 0);
    Require(sequence?.RootLayer != null, "Adapter import failed.");
    int document = Reader.StrokeReader_LoadFromFile(samplePath);
    Require(document > 0, "Native fixture load failed.");
    try
    {
        int paintCount = 0, pictureCount = 0, spawnCount = 0;
        for (int index = 0; index < Reader.StrokeReader_GetLayerCount(document); index++)
        {
            Require(Reader.StrokeReader_GetLayerInfo(document, index, out StrokeLayerInfo info), "Cannot read layer.");
            if (info.type == 1) paintCount++;
            if (info.type == 4) pictureCount++;
            if (info.type == 8) spawnCount++;
        }
        Require(spawnCount > 0 && paintCount > 0 && pictureCount > 0, "Fixture must contain paint, pictures and spawn areas.");
        int importedPaint = sequence.RootLayer.Children.OfType<SharpQuill.LayerPaint>().Count();
        int importedPictures = sequence.RootLayer.Children.OfType<SharpQuill.LayerPicture>().Count();
        Require(importedPaint == paintCount, $"Expected {paintCount} paint layers; got {importedPaint}.");
        Require(importedPictures == (includePictures ? pictureCount : 0), "Picture inclusion changed.");
        if (includePictures)
        {
            string project = Path.Combine(Path.GetDirectoryName(logPath), $"quill-picture-readback-{Guid.NewGuid():N}");
            SharpQuill.QuillSequenceWriter.Write(sequence, project);
            var readback = SharpQuill.QuillSequenceReader.Read(project);
            var expectedPictures = sequence.RootLayer.Children.OfType<SharpQuill.LayerPicture>().ToArray();
            var actualPictures = readback.RootLayer.Children.OfType<SharpQuill.LayerPicture>().ToArray();
            Require(actualPictures.Length == expectedPictures.Length, "Writer silently omitted adapter pictures.");
            for (int index = 0; index < expectedPictures.Length; index++)
            {
                var expected = expectedPictures[index]; var actual = actualPictures[index];
                Require(actual.PictureType == expected.PictureType && actual.ViewerLocked == expected.ViewerLocked &&
                    actual.Data.Width == expected.Data.Width && actual.Data.Height == expected.Data.Height &&
                    actual.Data.HasAlpha == expected.Data.HasAlpha && actual.Data.Pixels.SequenceEqual(expected.Data.Pixels),
                    "Native adapter picture changed during project write/read-back.");
            }
        }
        var reported = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsSequenceWithReport(samplePath, includePictures, 0);
        Require(reported.Succeeded && reported.IsPartial && reported.Sequence.RootLayer.Children.OfType<SharpQuill.LayerPaint>().Count() == importedPaint &&
            reported.Sequence.RootLayer.Children.OfType<SharpQuill.LayerPicture>().Count() == importedPictures, "Reporting changed legacy conversion output.");
        Require(reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.RootMetadataNotPreserved) &&
            reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.ChapterSubset), "Root/chapter limitations were not reported.");
        int sounds = 0;
        for (int index = 0; index < Reader.StrokeReader_GetAuthoringLayerCount(document); index++)
        {
            Require(Reader.StrokeReader_GetAuthoringLayerInfo(document, index, out var layer), "Cannot inspect source authoring layer.");
            if (layer.type == 5) sounds++;
            if (layer.type == 5 || layer.type == 8)
                Require(reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.LayerOmitted &&
                    x.SourceLayerId == layer.id && x.SourceLayerName == layer.name && x.SourceLayerType == layer.type), "Omitted sound/spawn layer not identified.");
            if (layer.type == 4)
                Require(reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.PictureExcluded && x.SourceLayerId == layer.id) == !includePictures,
                    "Picture-exclusion diagnostic did not match selection.");
            if (Reader.StrokeReader_GetLayerAnimationKeyCount(document, index) > 0 || layer.isTimeline != 0 || layer.durationTicks != 0 || layer.maxRepeatCount != 0)
                Require(reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.AnimationNotPreserved && x.SourceLayerId == layer.id),
                    "Uncopied animation/timing not identified.");
            if (layer.type == 0 || layer.parentId != 0)
                Require(reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.FlattenedHierarchy && x.SourceLayerId == layer.id), "Flattened hierarchy not identified.");
        }
        Require(sounds == 3 && !reported.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.DiagnosticUnavailable ||
            x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.DataReadFailed), "Fixture/report coverage incomplete.");
        Console.WriteLine($"[IMM_SHARPQUILL_ADAPTER] includePictures={includePictures}: passed ({importedPaint} paint, {importedPictures} pictures).");
    }
    finally
    {
        Reader.StrokeReader_Unload(document);
    }
}
var allFrames = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsSequenceWithReport(samplePath);
Require(allFrames.Succeeded && !allFrames.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.ChapterSubset),
    "Unselected chapter was reported as a selected subset.");
Console.WriteLine("[IMM_SHARPQUILL_REPORT] Passed source-layer omission, hierarchy/key, picture-option, chapter and failed-load diagnostics with legacy subset parity.");
var failed = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsSequenceWithReport($"{samplePath}.missing");
Require(!failed.Succeeded && failed.Sequence == null && failed.Issues.Any(x => x.Code == global::ImmStrokeReader.SharpQuillConversionIssueCode.LoadFailed),
    "Failed load was not reported.");
Reader.StrokeReader_End();

static void Require(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
