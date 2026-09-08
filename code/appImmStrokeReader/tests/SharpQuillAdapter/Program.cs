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
        Console.WriteLine($"[IMM_SHARPQUILL_ADAPTER] includePictures={includePictures}: passed ({importedPaint} paint, {importedPictures} pictures).");
    }
    finally
    {
        Reader.StrokeReader_Unload(document);
    }
}
Reader.StrokeReader_End();

static void Require(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
