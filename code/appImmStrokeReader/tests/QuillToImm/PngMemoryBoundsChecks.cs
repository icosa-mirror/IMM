using System;
using System.Runtime.InteropServices;
using ImmPlayer.Exporter;
using UnityEngine;

static class PngMemoryBoundsChecks
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr VirtualAlloc(IntPtr address, UIntPtr size, uint allocationType, uint protect);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualProtect(IntPtr address, UIntPtr size, uint protect, out uint oldProtect);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualFree(IntPtr address, UIntPtr size, uint freeType);
    [DllImport("ImmUnityPlugin", EntryPoint = "ImmExporter_SpawnAreaSetPreview")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool SetPreview(IntPtr layer, int width, int height, int channels, int encoding, int version, IntPtr data, ulong size);

    public static void Run()
    {
        if (!OperatingSystem.IsWindows()) return;
        byte[] png = Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGO4oxH1HwAFoAJeZn+M9QAAAABJRU5ErkJggg==");
        int page = Environment.SystemPageSize;
        IntPtr allocation = VirtualAlloc(IntPtr.Zero, (UIntPtr)(page * 2), 0x3000, 4);
        Require(allocation != IntPtr.Zero, "Guard-page allocation failed.");
        try
        {
            Require(VirtualProtect(IntPtr.Add(allocation, page), (UIntPtr)page, 1, out _), "Guard-page protection failed.");
            using var sequence = ExportSequence.Create(ExportSequenceType.Still, 24, new Color(0, 0, 0, 1), default);
            Require(sequence != null && sequence.IsValid, "Bounds fixture sequence failed.");
            var transform = new TransformNative { Qw = 1, Scale = 1 };
            IntPtr layer = Native.ImmExporter_CreateSpawnAreaLayer(sequence.Handle, IntPtr.Zero, "Bounds", ref transform, 0);
            Require(layer != IntPtr.Zero, "Bounds fixture spawn failed.");
            // Every supplied buffer ends exactly at a no-access page: an overread crashes
            // the process, rather than being hidden by readable adjacent array memory.
            foreach (int length in new[] { png.Length, 0, 1, 7, 8, 16, 33, png.Length - 1 })
            {
                IntPtr input = IntPtr.Add(allocation, page - length);
                if (length > 0) Marshal.Copy(png, 0, input, length);
                bool accepted = SetPreview(layer, 1, 1, 4, 2, 2, input, (ulong)length);
                Require(accepted == (length == png.Length), $"PNG prefix length {length} had unexpected acceptance.");
            }
        }
        finally { Require(VirtualFree(allocation, UIntPtr.Zero, 0x8000), "Guard-page release failed."); }
        Console.WriteLine("[IMM_PNG_BOUNDS] Complete PNG and seven truncated prefixes passed at a no-access page boundary.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
