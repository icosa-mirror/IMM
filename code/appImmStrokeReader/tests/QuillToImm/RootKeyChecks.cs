using System;
using System.IO;
using System.Runtime.InteropServices;
using ImmPlayer;
using Reader = ImmPlayer.ImmStrokeReader;

static class RootKeyChecks
{
    [DllImport("ImmStrokeReader", EntryPoint = "StrokeReader_GetRootAnimationKey")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool QueryNull(int document, int index, IntPtr output);

    public static void Run(string emptyRootFixture, string root)
    {
        Require(!Reader.StrokeReader_GetRootAnimationKey(-1, 0, out _), "Query succeeded before initialization.");
        Require(Reader.StrokeReader_Init(Path.Combine(root, "root-key-queries.log")) == 0, "Reader init failed.");
        int empty = 0, sample = 0;
        try
        {
            empty = Reader.StrokeReader_LoadFromFile(emptyRootFixture);
            Require(empty > 0 && Reader.StrokeReader_GetDocumentInfo(empty, out var emptyInfo) &&
                emptyInfo.rootAnimationKeyCount == 0, "Implicit default visibility became a counted root key.");
            Require(!Reader.StrokeReader_GetRootAnimationKey(empty, 0, out _), "Empty root returned a key.");

            string samplePath = Path.GetFullPath("exampleImmFiles/sample1.imm");
            Require(File.Exists(samplePath), $"Tracked public fixture is missing: {samplePath}");
            sample = Reader.StrokeReader_LoadFromFile(samplePath);
            Require(sample > 0, "Tracked public fixture failed to load.");
            Require(Reader.StrokeReader_GetDocumentInfo(sample, out var sampleInfo) &&
                sampleInfo.rootAnimationKeyCount == 1, "Public sample root-key count changed.");
            Require(Reader.StrokeReader_GetRootAnimationKey(sample, 0, out var key) &&
                key.property == 8 && key.timeTicks == 0 && key.interpolation == 0 && key.intValue == 0,
                "Public sample root offset key was not retained.");
            Require(!Reader.StrokeReader_GetRootAnimationKey(sample, -1, out _) &&
                !Reader.StrokeReader_GetRootAnimationKey(sample, 1, out _) &&
                !Reader.StrokeReader_GetRootAnimationKey(-1, 0, out _) &&
                !QueryNull(sample, 0, IntPtr.Zero), "Invalid root query was accepted.");
            int unloaded = sample;
            Reader.StrokeReader_Unload(sample);
            sample = 0;
            Require(!Reader.StrokeReader_GetRootAnimationKey(unloaded, 0, out _), "Unloaded document returned a root key.");
        }
        finally
        {
            if (empty > 0) Reader.StrokeReader_Unload(empty);
            if (sample > 0) Reader.StrokeReader_Unload(sample);
            Reader.StrokeReader_End();
        }
        Console.WriteLine("[IMM_QUILL_ROOT_KEY_TEST] Passed root key values, implicit defaults, bounds, null output and lifetime checks.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
