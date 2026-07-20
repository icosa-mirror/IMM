"""Smoke-test legacy and authoring views of the Windows stroke-reader C ABI."""

from __future__ import annotations

import argparse
import ctypes
import importlib.util
import math
import tempfile
from pathlib import Path

PREFIX = "[IMM_STROKE_READER_AUTHORING_SMOKE]"


class LegacyLayerInfo(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int), ("type", ctypes.c_int), ("num_drawings", ctypes.c_int),
        ("name", ctypes.c_char * 256), ("visible", ctypes.c_int), ("opacity", ctypes.c_float),
        ("is_default_spawn", ctypes.c_int), ("pivot_rotation", ctypes.c_float * 4),
        ("pivot_scale", ctypes.c_float), ("pivot_flip", ctypes.c_int),
        ("pivot_translation", ctypes.c_float * 3),
    ]


class AuthoringLayerInfo(ctypes.Structure):
    _fields_ = [
        ("legacy", LegacyLayerInfo), ("parent_id", ctypes.c_int), ("child_index", ctypes.c_int),
        ("is_timeline", ctypes.c_int), ("duration_ticks", ctypes.c_int64),
        ("max_repeat_count", ctypes.c_uint32),
    ]


class LegacyPoint(ctypes.Structure):
    _fields_ = [(name, ctypes.c_float) for name in (
        "px", "py", "pz", "nx", "ny", "nz", "dx", "dy", "dz",
        "r", "g", "b", "alpha", "width",
    )]


class AuthoringPoint(ctypes.Structure):
    _fields_ = [("legacy", LegacyPoint), ("length", ctypes.c_float), ("time", ctypes.c_float)]


class StrokeInfo(ctypes.Structure):
    _fields_ = [
        ("brush_type", ctypes.c_int), ("visibility_mode", ctypes.c_int),
        ("num_points", ctypes.c_int), ("bounds", ctypes.c_float * 6),
    ]


def bind(library: ctypes.CDLL, name: str, result_type: object, *argument_types: object):
    function = getattr(library, name)
    function.restype = result_type
    function.argtypes = list(argument_types)
    return function


def load_generator(repo_root: Path):
    path = repo_root / "tests/tools/generate_native_web_manifest.py"
    spec = importlib.util.spec_from_file_location("generate_native_web_manifest", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def run(repo_root: Path, library_path: Path, input_path: Path) -> None:
    if ctypes.sizeof(LegacyLayerInfo) != 316 or ctypes.sizeof(LegacyPoint) != 56:
        raise RuntimeError("Legacy ctypes ABI sizes changed")

    generator = load_generator(repo_root)
    library, dependency_handles = generator._load_library(repo_root, library_path)
    del dependency_handles
    get_layer_count = bind(library, "StrokeReader_GetAuthoringLayerCount", ctypes.c_int, ctypes.c_int)
    get_layer_info = bind(library, "StrokeReader_GetAuthoringLayerInfo", ctypes.c_bool, ctypes.c_int, ctypes.c_int, ctypes.POINTER(AuthoringLayerInfo))
    get_drawing_count = bind(library, "StrokeReader_GetAuthoringDrawingCount", ctypes.c_int, ctypes.c_int, ctypes.c_int)
    get_stroke_count = bind(library, "StrokeReader_GetAuthoringStrokeCount", ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int)
    get_stroke_info = bind(library, "StrokeReader_GetAuthoringStrokeInfo", ctypes.c_bool, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(StrokeInfo))
    get_stroke_points = bind(library, "StrokeReader_GetAuthoringStrokePoints", ctypes.c_bool, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(AuthoringPoint), ctypes.c_int)

    with tempfile.TemporaryDirectory(prefix="imm-stroke-reader-authoring-") as temporary_directory:
        log_path = Path(temporary_directory) / "stroke-reader.log"
        if library.StrokeReader_Init(str(log_path).encode()) != 0:
            raise RuntimeError("StrokeReader_Init failed")
        source = input_path.read_bytes()
        source_buffer = ctypes.create_string_buffer(source)
        document_id = library.StrokeReader_LoadFromMemory(source_buffer, len(source))
        if document_id <= 0:
            raise RuntimeError(f"StrokeReader_LoadFromMemory failed with {document_id}")
        try:
            legacy_layer_count = library.StrokeReader_GetLayerCount(document_id)
            authoring_layer_count = get_layer_count(document_id)
            if legacy_layer_count != 35 or authoring_layer_count != 74:
                raise RuntimeError(f"Unexpected layer views: legacy={legacy_layer_count}, authoring={authoring_layer_count}")
            authoring_drawing_count = 0
            sampled_point = False
            for layer_index in range(authoring_layer_count):
                layer = AuthoringLayerInfo()
                if not get_layer_info(document_id, layer_index, ctypes.byref(layer)):
                    raise RuntimeError(f"Could not read authoring layer {layer_index}")
                drawing_count = get_drawing_count(document_id, layer_index)
                if drawing_count != layer.legacy.num_drawings:
                    raise RuntimeError(f"Drawing count mismatch for authoring layer {layer_index}")
                authoring_drawing_count += drawing_count
                if sampled_point or drawing_count == 0:
                    continue
                if get_stroke_count(document_id, layer_index, 0) == 0:
                    continue
                stroke = StrokeInfo()
                if not get_stroke_info(document_id, layer_index, 0, 0, ctypes.byref(stroke)):
                    raise RuntimeError("Could not read authoring stroke metadata")
                points = (AuthoringPoint * stroke.num_points)()
                if not get_stroke_points(document_id, layer_index, 0, 0, points, stroke.num_points):
                    raise RuntimeError("Could not read authoring stroke points")
                if not all(math.isfinite(value) for value in (points[0].length, points[0].time)):
                    raise RuntimeError("Authoring point metadata is not finite")
                sampled_point = True
            legacy_drawing_count = sum(
                library.StrokeReader_GetDrawingCount(document_id, index)
                for index in range(legacy_layer_count)
            )
            if authoring_drawing_count != legacy_drawing_count:
                raise RuntimeError(f"Layer views disagree on drawings: legacy={legacy_drawing_count}, authoring={authoring_drawing_count}")
            if not sampled_point:
                raise RuntimeError("No authoring point was available for verification")
        finally:
            library.StrokeReader_Unload(document_id)
            library.StrokeReader_End()

    print(f"{PREFIX} legacy_layers={legacy_layer_count} authoring_layers={authoring_layer_count} drawings={authoring_drawing_count}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path, default=Path("code/appImmStrokeReader/exe/ImmStrokeReader.dll"))
    parser.add_argument("--input", type=Path, default=Path("exampleImmFiles/sample1.imm"))
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parents[3]
    run(repo_root, (repo_root / args.library).resolve(), (repo_root / args.input).resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
