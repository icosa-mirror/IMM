#!/usr/bin/env python3
"""Generate the committed, deterministic IMM face-orientation fixture."""

from __future__ import annotations

import argparse
import _ctypes
import ctypes
import os
import shutil
import tempfile
from dataclasses import dataclass
from pathlib import Path


class Transform(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_float)
        for name in ("tx", "ty", "tz", "qx", "qy", "qz", "qw", "scale")
    ]


class Point(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_float)
        for name in (
            "px", "py", "pz",
            "nx", "ny", "nz",
            "dx", "dy", "dz",
            "r", "g", "b", "a",
            "width", "length", "time",
        )
    ]


@dataclass(frozen=True)
class Stroke:
    brush: int
    positions: tuple[tuple[float, float, float], ...]
    normal: tuple[float, float, float]
    color: tuple[float, float, float]
    width: float
    widths: tuple[float, ...] | None = None


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
    unity = repo_root / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64"
    reader = repo_root / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64"
    return tuple(
        unity / name
        for name in ("Audio360.dll", "opusenc.dll", "opus.dll", "vorbisenc.dll")
    ) + tuple(
        reader / name
        for name in ("zlib1.dll", "jpeg62.dll", "libpng16.dll", "ogg.dll", "vorbis.dll")
    )


def make_points(stroke: Stroke) -> ctypes.Array:
    points = []
    length = 0.0
    for index, position in enumerate(stroke.positions):
        if index:
            previous = stroke.positions[index - 1]
            length += sum((position[axis] - previous[axis]) ** 2 for axis in range(3)) ** 0.5
        points.append(Point(
            *position,
            *stroke.normal,
            0.0, 0.0, -1.0,
            *stroke.color,
            1.0,
            stroke.widths[index] if stroke.widths is not None else stroke.width,
            length,
            float(index) / max(1, len(stroke.positions) - 1),
        ))
    return (Point * len(points))(*points)


def generate(library: ctypes.CDLL, output_path: Path) -> None:
    create_sequence = bind(
        library, "ImmExporter_CreateSequence", ctypes.c_void_p,
        ctypes.c_int, ctypes.c_int,
        ctypes.c_float, ctypes.c_float, ctypes.c_float,
        ctypes.c_uint32,
        ctypes.c_int64, ctypes.c_int64, ctypes.c_int64, ctypes.c_int64,
    )
    destroy_sequence = bind(library, "ImmExporter_DestroySequence", None, ctypes.c_void_p)
    create_spawn = bind(
        library, "ImmExporter_CreateSpawnAreaLayer", ctypes.c_void_p,
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p,
        ctypes.POINTER(Transform), ctypes.c_int,
    )
    create_paint = bind(
        library, "ImmExporter_CreatePaintLayer", ctypes.c_void_p,
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p,
        ctypes.c_int, ctypes.c_float,
        ctypes.POINTER(Transform), ctypes.POINTER(Transform),
        ctypes.c_int, ctypes.c_int64, ctypes.c_uint32,
    )
    create_drawing = bind(library, "ImmExporter_CreateDrawing", ctypes.c_void_p, ctypes.c_void_p)
    destroy_drawing = bind(library, "ImmExporter_DestroyDrawing", None, ctypes.c_void_p)
    drawing_index = bind(library, "ImmExporter_GetDrawingIndex", ctypes.c_uint32, ctypes.c_void_p)
    drawing_init = bind(
        library, "ImmExporter_DrawingInit", ctypes.c_bool,
        ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int,
    )
    drawing_element = bind(
        library, "ImmExporter_DrawingGetElement", ctypes.c_void_p,
        ctypes.c_void_p, ctypes.c_uint32,
    )
    element_init = bind(
        library, "ImmExporter_ElementInit", ctypes.c_bool,
        ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_int,
    )
    element_points = bind(
        library, "ImmExporter_ElementSetPoints", ctypes.c_bool,
        ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(Point), ctypes.c_uint32,
    )
    element_bounds = bind(library, "ImmExporter_ComputeElementBounds", None, ctypes.c_void_p)
    drawing_bounds = bind(library, "ImmExporter_ComputeDrawingBounds", None, ctypes.c_void_p)
    add_frame = bind(library, "ImmExporter_PaintAddFrame", None, ctypes.c_void_p, ctypes.c_uint32)
    export_file = bind(
        library, "ImmExporter_ExportToFile", ctypes.c_bool,
        ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int,
    )

    # IMM native camera convention looks down -Z. Host integrations convert
    # this pose to their own camera convention before submitting matrices.
    identity = Transform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0)
    sequence = require(
        create_sequence(0, 0, 0.015, 0.020, 0.035, 30, 0, 0, 0, 0),
        "ImmExporter_CreateSequence",
    )
    drawing = None
    try:
        require(
            create_spawn(sequence, None, b"Face Orientation Camera", ctypes.byref(identity), 0),
            "ImmExporter_CreateSpawnAreaLayer",
        )
        layer = require(
            create_paint(
                sequence, None, b"Face Orientation Geometry", 1, 1.0,
                ctypes.byref(identity), ctypes.byref(identity), 0, 0, 0,
            ),
            "ImmExporter_CreatePaintLayer",
        )
        strokes = (
            # A double-sided asymmetric ribbon is a stable layout marker. It
            # establishes that the document and camera loaded independently of
            # the single-sided face verdicts below.
            Stroke(
                brush=1,
                positions=((0.70, -0.72, -3.0), (1.08, -0.38, -3.0), (0.82, -0.02, -3.0),
                           (1.18, 0.30, -3.0), (0.76, 0.70, -3.0)),
                normal=(0.0, 0.0, 1.0),
                color=(0.10, 0.90, 0.22),
                width=0.12,
            ),
            # A single-sided square bipyramid. Its collapsed end sections make
            # a closed convex solid with known outward winding and no end caps.
            Stroke(
                brush=4,
                positions=((-0.98, -0.70, -3.0), (-0.98, 0.0, -3.0),
                           (-0.98, 0.70, -3.0)),
                normal=(0.0, 0.0, 1.0),
                color=(0.05, 0.72, 0.95),
                width=0.35,
                widths=(0.001, 0.35, 0.001),
            ),
            # A single-sided open square shell points directly at the camera.
            # Its exterior faces point away from the tunnel interior. With
            # correct culling, the dark centre remains visible; inverted face
            # orientation exposes a large orange/red interior.
            Stroke(
                brush=4,
                positions=((0.0, 0.0, -4.20), (0.0, 0.0, -3.25), (0.0, 0.0, -2.30)),
                normal=(0.0, 1.0, 0.0),
                color=(0.95, 0.16, 0.04),
                width=0.48,
            ),
        )
        drawing = require(create_drawing(layer), "ImmExporter_CreateDrawing")
        require(drawing_init(drawing, len(strokes), 0), "ImmExporter_DrawingInit")
        for index, stroke in enumerate(strokes):
            element = require(drawing_element(drawing, index), f"drawing element {index}")
            require(
                element_init(element, len(stroke.positions), stroke.brush, 1),
                f"element {index} init",
            )
            points = make_points(stroke)
            require(
                element_points(element, 0, points, len(points)),
                f"element {index} points",
            )
            element_bounds(element)
        drawing_bounds(drawing)
        add_frame(layer, drawing_index(drawing))
        destroy_drawing(drawing)
        drawing = None
        output_path.parent.mkdir(parents=True, exist_ok=True)
        require(
            export_file(sequence, os.fsencode(output_path), 96000, 0),
            "ImmExporter_ExportToFile",
        )
    finally:
        if drawing:
            destroy_drawing(drawing)
        destroy_sequence(sequence)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("exampleImmFiles/face-orientation.imm"),
    )
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    plugin = args.plugin or repo_root / "code/appImmUnity/exe/ImmUnityPlugin.dll"
    if not plugin.is_file():
        raise FileNotFoundError(f"Unity plugin not found: {plugin}")
    output_path = args.output.resolve()

    with tempfile.TemporaryDirectory(prefix="imm-face-orientation-") as temporary:
        staging = Path(temporary)
        staged_plugin = staging / plugin.name
        shutil.copy2(plugin, staged_plugin)
        for dependency in runtime_dependencies(repo_root):
            if not dependency.is_file():
                raise FileNotFoundError(f"Runtime dependency not found: {dependency}")
            shutil.copy2(dependency, staging / dependency.name)
        ctypes.windll.kernel32.SetDllDirectoryW(str(staging))
        library = ctypes.CDLL(str(staged_plugin))
        library_handle = library._handle
        try:
            generate(library, output_path)
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

    if not output_path.is_file() or output_path.stat().st_size == 0:
        raise RuntimeError(f"Fixture was not written: {output_path}")
    data = output_path.read_bytes()
    require(data[:8] == b"Immersiv", "IMM signature validation")
    require(b"Face Orientation Geometry" in data, "diagnostic layer serialization validation")
    require(b"Face Orientation Camera" in data, "spawn camera serialization validation")
    print(f"Face-orientation IMM fixture written: {output_path} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
