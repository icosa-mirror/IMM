"""End-to-end smoke test for the Windows ImmUnity exporter C ABI."""

from __future__ import annotations

import argparse
import _ctypes
import ctypes
import os
import shutil
import tempfile
from pathlib import Path

PREFIX = "[IMM_EXPORTER_BRIDGE_SMOKE]"


class Transform(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_float)
        for name in ("tx", "ty", "tz", "qx", "qy", "qz", "qw", "scale")
    ]


class Point(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_float)
        for name in (
            "px",
            "py",
            "pz",
            "nx",
            "ny",
            "nz",
            "dx",
            "dy",
            "dz",
            "r",
            "g",
            "b",
            "a",
            "width",
            "length",
            "time",
        )
    ]


def bind(library: ctypes.CDLL, name: str, result_type: object, *argument_types: object):
    function = getattr(library, name)
    function.restype = result_type
    function.argtypes = list(argument_types)
    return function


def require(value: object, operation: str):
    if not value:
        raise RuntimeError(f"{operation} failed")
    return value


def runtime_dependencies(repo_root: Path) -> tuple[Path, ...]:
    package_directory = (
        repo_root
        / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64"
    )
    return tuple(
        package_directory / name
        for name in (
            "Audio360.dll",
            "opusenc.dll",
            "opus.dll",
            "zlib1.dll",
            "jpeg62.dll",
            "libpng16.dll",
            "ogg.dll",
            "vorbis.dll",
            "vorbisenc.dll",
        )
    )

def export_smoke_file(library: ctypes.CDLL, output_path: Path) -> None:
    create_sequence = bind(
        library,
        "ImmExporter_CreateSequence",
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_uint32,
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.c_int64,
    )
    destroy_sequence = bind(library, "ImmExporter_DestroySequence", None, ctypes.c_void_p)
    create_paint_layer = bind(
        library,
        "ImmExporter_CreatePaintLayer",
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_float,
        ctypes.POINTER(Transform),
        ctypes.POINTER(Transform),
        ctypes.c_int,
        ctypes.c_int64,
        ctypes.c_uint32,
    )
    create_drawing = bind(library, "ImmExporter_CreateDrawing", ctypes.c_void_p, ctypes.c_void_p)
    destroy_drawing = bind(library, "ImmExporter_DestroyDrawing", None, ctypes.c_void_p)
    get_drawing_index = bind(
        library, "ImmExporter_GetDrawingIndex", ctypes.c_uint32, ctypes.c_void_p
    )
    drawing_init = bind(
        library,
        "ImmExporter_DrawingInit",
        ctypes.c_bool,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_int,
    )
    drawing_get_element = bind(
        library,
        "ImmExporter_DrawingGetElement",
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_uint32,
    )
    element_init = bind(
        library,
        "ImmExporter_ElementInit",
        ctypes.c_bool,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_int,
        ctypes.c_int,
    )
    element_set_points = bind(
        library,
        "ImmExporter_ElementSetPoints",
        ctypes.c_bool,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.POINTER(Point),
        ctypes.c_uint32,
    )
    compute_element_bounds = bind(
        library, "ImmExporter_ComputeElementBounds", None, ctypes.c_void_p
    )
    compute_drawing_bounds = bind(
        library, "ImmExporter_ComputeDrawingBounds", None, ctypes.c_void_p
    )
    paint_add_frame = bind(
        library,
        "ImmExporter_PaintAddFrame",
        None,
        ctypes.c_void_p,
        ctypes.c_uint32,
    )
    export_to_file = bind(
        library,
        "ImmExporter_ExportToFile",
        ctypes.c_bool,
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_int,
    )

    identity = Transform(0, 0, 0, 0, 0, 0, 1, 1)
    sequence = require(
        create_sequence(1, 0, 0.02, 0.03, 0.04, 30, 0, 0, 0, 0),
        "ImmExporter_CreateSequence",
    )
    drawing = None
    try:
        layer = require(
            create_paint_layer(
                sequence,
                None,
                b"Smoke Paint",
                1,
                1.0,
                ctypes.byref(identity),
                ctypes.byref(identity),
                1,
                420,
                0,
            ),
            "ImmExporter_CreatePaintLayer",
        )
        drawing = require(create_drawing(layer), "ImmExporter_CreateDrawing")
        drawing_index = get_drawing_index(drawing)
        require(drawing_init(drawing, 1, 0), "ImmExporter_DrawingInit")
        element = require(
            drawing_get_element(drawing, 0), "ImmExporter_DrawingGetElement"
        )
        require(element_init(element, 2, 2, 1), "ImmExporter_ElementInit")
        points = (Point * 2)(
            Point(0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0.03, 0, 0),
            Point(
                1,
                0.25,
                0,
                0,
                1,
                0,
                0,
                0,
                1,
                0,
                0.5,
                1,
                1,
                0.03,
                1.0307764,
                1,
            ),
        )
        require(
            element_set_points(element, 0, points, len(points)),
            "ImmExporter_ElementSetPoints",
        )
        compute_element_bounds(element)
        compute_drawing_bounds(drawing)
        paint_add_frame(layer, drawing_index)
        destroy_drawing(drawing)
        drawing = None
        require(
            export_to_file(sequence, os.fsencode(output_path), 96000, 0),
            "ImmExporter_ExportToFile",
        )
    finally:
        if drawing:
            destroy_drawing(drawing)
        destroy_sequence(sequence)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[3]
    plugin = arguments.plugin or repo_root / "code/appImmUnity/exe/ImmUnityPlugin.dll"
    if not plugin.is_file():
        raise FileNotFoundError(f"Unity plugin not found: {plugin}")

    with tempfile.TemporaryDirectory(prefix="imm-exporter-bridge-") as temporary:
        staging = Path(temporary)
        staged_plugin = staging / plugin.name
        shutil.copy2(plugin, staged_plugin)
        for dependency in runtime_dependencies(repo_root):
            if not dependency.is_file():
                raise FileNotFoundError(f"Runtime dependency not found: {dependency}")
            shutil.copy2(dependency, staging / dependency.name)

        output_path = arguments.output or staging / "exporter-bridge-smoke.imm"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        ctypes.windll.kernel32.SetDllDirectoryW(str(staging))
        library = ctypes.CDLL(str(staged_plugin))
        library_handle = library._handle
        try:
            export_smoke_file(library, output_path)
        finally:
            del library
            _ctypes.FreeLibrary(library_handle)
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.GetModuleHandleW.restype = ctypes.c_void_p
            kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
            kernel32.FreeLibrary.restype = ctypes.c_bool
            kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
            for dependency in reversed(runtime_dependencies(repo_root)):
                dependency_handle = kernel32.GetModuleHandleW(dependency.name)
                if dependency_handle:
                    kernel32.FreeLibrary(dependency_handle)
            kernel32.SetDllDirectoryW(None)

        data = output_path.read_bytes()
        require(data[:8] == b"Immersiv", "IMM signature validation")
        require(b"Root" in data, "Root layer serialization validation")
        require(b"Smoke Paint" in data, "Paint layer serialization validation")
        require(len(data) > 64, "IMM payload size validation")

        if arguments.output is None:
            print(f"{PREFIX} Exported and validated {len(data)} bytes")
        else:
            print(f"{PREFIX} Exported and validated {len(data)} bytes at {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())