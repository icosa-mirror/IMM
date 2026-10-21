using System;
using System.Runtime.InteropServices;

static class GuardedMemoryChecks
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr VirtualAlloc(IntPtr address, UIntPtr size, uint allocationType, uint protect);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualProtect(IntPtr address, UIntPtr size, uint protect, out uint oldProtect);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualFree(IntPtr address, UIntPtr size, uint freeType);

    public static void Run(byte[] data, int[] lengths, Func<IntPtr, int, bool> accepts)
    {
        int page = Environment.SystemPageSize;
        int accessible = checked(((data.Length + page - 1) / page) * page);
        IntPtr allocation = VirtualAlloc(IntPtr.Zero, (UIntPtr)(accessible + page), 0x3000, 4);
        Require(allocation != IntPtr.Zero, "Guard-page allocation failed.");
        try
        {
            Require(VirtualProtect(IntPtr.Add(allocation, accessible), (UIntPtr)page, 1, out _), "Guard-page protection failed.");
            foreach (int length in lengths)
            {
                Require(length >= 0 && length <= data.Length, "Invalid guarded prefix length.");
                IntPtr input = IntPtr.Add(allocation, accessible - length);
                if (length > 0) Marshal.Copy(data, 0, input, length);
                Require(accepts(input, length) == (length == data.Length), $"Guarded prefix length {length} had unexpected acceptance.");
            }
        }
        finally { Require(VirtualFree(allocation, UIntPtr.Zero, 0x8000), "Guard-page release failed."); }
    }

    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
