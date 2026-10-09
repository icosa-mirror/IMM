"""Validate RenderGraph packet acknowledgement against the built Windows plugin."""
import ctypes
import os
from pathlib import Path
import sys


def main():
    root = Path(__file__).resolve().parents[2]
    plugin = Path(sys.argv[1]).resolve()
    dependencies = root / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64"
    with os.add_dll_directory(str(dependencies)):
        native = ctypes.WinDLL(str(plugin))
    native.GetRenderGraphPacketSize.restype = ctypes.c_int
    assert native.GetRenderGraphPacketSize() == 496, "Managed/native packet size differs"
    poll = native.GetRenderGraphPacketResult
    poll.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_uint64)]
    poll.restype = ctypes.c_int
    packet = ctypes.create_string_buffer(496)
    result, fence = ctypes.c_int32(), ctypes.c_uint64()
    assert poll(None, ctypes.byref(result), ctypes.byref(fence)) == -1
    ctypes.c_uint32.from_buffer(packet, 0).value = 3
    ctypes.c_uint32.from_buffer(packet, 4).value = 496
    assert poll(packet, ctypes.byref(result), ctypes.byref(fence)) == 0, "Fresh packet is not pending"
    ctypes.c_uint64.from_buffer(packet, 464).value = 123456789
    ctypes.c_int32.from_buffer(packet, 472).value = -2147024809  # E_INVALIDARG
    ctypes.c_int32.from_buffer(packet, 476).value = 1
    assert poll(packet, ctypes.byref(result), ctypes.byref(fence)) == 1
    assert result.value == -2147024809 and fence.value == 123456789, "Completion field offsets differ"
    for version in (1, 2, 99):
        ctypes.c_uint32.from_buffer(packet, 0).value = version
        assert poll(packet, ctypes.byref(result), ctypes.byref(fence)) == -1, f"Unsupported ABI {version} was accepted"
    print("IMM_RENDER_GRAPH_ABI PASS size, pending/completed acknowledgement, result/fence offsets and invalid ABI")


if __name__ == "__main__":
    main()
