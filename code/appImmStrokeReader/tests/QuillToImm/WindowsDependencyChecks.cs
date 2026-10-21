using System;
using System.Runtime.InteropServices;

static class WindowsDependencyChecks
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string moduleName);

    public static void Run(string exporterLibrary)
    {
        if (!OperatingSystem.IsWindows()) return;
        IntPtr plugin = NativeLibrary.Load(exporterLibrary);
        try
        {
            if (GetModuleHandle("vorbisenc.dll") == IntPtr.Zero)
                throw new InvalidOperationException("Packaged Vorbis encoder was not preloaded by the native plugin.");
        }
        finally { NativeLibrary.Free(plugin); }
        Console.WriteLine("[IMM_WINDOWS_DEPENDENCIES] Packaged Vorbis encoder preloaded without changing PATH.");
    }
}
