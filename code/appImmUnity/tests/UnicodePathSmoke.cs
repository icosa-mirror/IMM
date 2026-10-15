using System;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using ImmPlayer;

internal static class UnicodePathSmoke
{
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool SetDllDirectory(string path);
    [DllImport("ImmUnityPlugin")]
    private static extern int BeginUnicodePathTestLog([MarshalAs(UnmanagedType.LPWStr)] string path);
    [DllImport("ImmUnityPlugin")]
    private static extern void EndUnicodePathTestLog();
    [DllImport("ImmUnityPlugin")]
    private static extern int LegacyAnsiPathExists(string path);
    [DllImport("ImmUnityPlugin")]
    private static extern int TestStrictUtf8Decoder();
    private static void Require(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
    private static int Main(string[] args)
    {
        Require(SetDllDirectory(AppContext.BaseDirectory), "native dependency directory");
        Require(TestStrictUtf8Decoder() == 1, "strict UTF-8 validation");
        Require(BeginUnicodePathTestLog(args[4]) == 1, "log initialization");
        try
        {
            int baseline = ImmNativePlugin.LoadFromFile(args[0]);
            Require(baseline > 0, "ASCII file import");
            Require(File.Exists(args[1]) && File.Exists(args[2]), "Unicode fixture existence");
            Require(ImmNativePlugin.LoadFromFile(args[1]) == baseline, "Chinese file import equals ASCII");
            Require(ImmNativePlugin.LoadFromFile(args[2]) == baseline, "supplementary Unicode and percent path import");
            Require(ImmNativePlugin.LoadFromFile(args[3]) < 0, "missing Unicode file rejection");
            Console.WriteLine($"[IMM_UNICODE_PATH_TEST] imports passed; layers={baseline}; legacy default-marshaled Chinese path exists={LegacyAnsiPathExists(args[1])}");
        }
        finally { EndUnicodePathTestLog(); }
        string log = File.ReadAllText(args[4], Encoding.UTF8);
        Require(log.Contains(args[1]) && log.Contains(args[2]), "complete Unicode filenames in UTF-8 log");
        Require(log.Contains($"[IMM_FILE_OPEN] open failed filename=\"{args[3]}\" errno=2 osError=2"),
            "missing file diagnostic contains complete path and CRT/OS errors");
        Console.WriteLine("[IMM_UNICODE_PATH_TEST] Unicode diagnostics and literal percent logging passed");
        return 0;
    }
}
