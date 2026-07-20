#!/usr/bin/env python3
"""Generate a compact native decoder manifest for web-port conformance tests."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import struct
from collections import Counter
from pathlib import Path
from typing import Any


SCHEMA = "imm-native-decoder-manifest-v1"


class StrokeLayerInfo(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("type", ctypes.c_int),
        ("num_drawings", ctypes.c_int),
        ("name", ctypes.c_char * 256),
        ("visible", ctypes.c_int),
        ("opacity", ctypes.c_float),
        ("is_default_spawn", ctypes.c_int),
        ("pivot_rotation", ctypes.c_float * 4),
        ("pivot_scale", ctypes.c_float),
        ("pivot_flip", ctypes.c_int),
        ("pivot_translation", ctypes.c_float * 3),
    ]


class StrokeLayerTransform(ctypes.Structure):
    _fields_ = [
        ("rotation", ctypes.c_float * 4),
        ("scale", ctypes.c_float),
        ("flip", ctypes.c_int),
        ("translation", ctypes.c_float * 3),
    ]


class StrokeInfo(ctypes.Structure):
    _fields_ = [
        ("brush_type", ctypes.c_int),
        ("visibility_mode", ctypes.c_int),
        ("num_points", ctypes.c_int),
        ("bbox_min", ctypes.c_float * 3),
        ("bbox_max", ctypes.c_float * 3),
    ]


class StrokePoint(ctypes.Structure):
    _fields_ = [
        ("position", ctypes.c_float * 3),
        ("normal", ctypes.c_float * 3),
        ("direction", ctypes.c_float * 3),
        ("color", ctypes.c_float * 3),
        ("alpha", ctypes.c_float),
        ("width", ctypes.c_float),
    ]


class StrokePictureInfo(ctypes.Structure):
    _fields_ = [
        ("layer_id", ctypes.c_int),
        ("content_type", ctypes.c_int),
        ("is_viewer_locked", ctypes.c_int),
        ("width", ctypes.c_int),
        ("height", ctypes.c_int),
        ("has_alpha", ctypes.c_int),
        ("data_size", ctypes.c_int),
    ]


def _float_list(values: Any) -> list[float]:
    return [float(value) for value in values]


def _transform_dict(value: StrokeLayerTransform) -> dict[str, Any]:
    return {
        "rotation": _float_list(value.rotation),
        "scale": float(value.scale),
        "flip": value.flip,
        "translation": _float_list(value.translation),
    }


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _configure_api(library: ctypes.CDLL) -> None:
    library.StrokeReader_GetBuildId.restype = ctypes.c_char_p
    library.StrokeReader_Init.argtypes = [ctypes.c_char_p]
    library.StrokeReader_Init.restype = ctypes.c_int
    library.StrokeReader_End.argtypes = []
    library.StrokeReader_LoadFromMemory.argtypes = [ctypes.c_void_p, ctypes.c_int]
    library.StrokeReader_LoadFromMemory.restype = ctypes.c_int
    library.StrokeReader_Unload.argtypes = [ctypes.c_int]
    library.StrokeReader_GetChapterCount.argtypes = [ctypes.c_int]
    library.StrokeReader_GetChapterCount.restype = ctypes.c_int
    library.StrokeReader_GetLayerCount.argtypes = [ctypes.c_int]
    library.StrokeReader_GetLayerCount.restype = ctypes.c_int
    library.StrokeReader_GetLayerInfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(StrokeLayerInfo)]
    library.StrokeReader_GetLayerInfo.restype = ctypes.c_bool
    library.StrokeReader_GetLayerTransform.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(StrokeLayerTransform),
        ctypes.POINTER(StrokeLayerTransform),
    ]
    library.StrokeReader_GetLayerTransform.restype = ctypes.c_bool
    library.StrokeReader_GetDrawingCount.argtypes = [ctypes.c_int, ctypes.c_int]
    library.StrokeReader_GetDrawingCount.restype = ctypes.c_int
    library.StrokeReader_GetLayerAnimationInfo.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    library.StrokeReader_GetLayerAnimationInfo.restype = ctypes.c_bool
    library.StrokeReader_GetFrameBuffer.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.c_int]
    library.StrokeReader_GetFrameBuffer.restype = ctypes.c_int
    library.StrokeReader_GetStrokeCount.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    library.StrokeReader_GetStrokeCount.restype = ctypes.c_int
    library.StrokeReader_GetStrokeInfo.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(StrokeInfo),
    ]
    library.StrokeReader_GetStrokeInfo.restype = ctypes.c_bool
    library.StrokeReader_GetStrokePoints.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(StrokePoint),
        ctypes.c_int,
    ]
    library.StrokeReader_GetStrokePoints.restype = ctypes.c_bool
    library.StrokeReader_GetPictureInfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(StrokePictureInfo)]
    library.StrokeReader_GetPictureInfo.restype = ctypes.c_bool
    library.StrokeReader_GetPicturePixelData.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
    library.StrokeReader_GetPicturePixelData.restype = ctypes.c_int


def _add_dependency_directories(repo_root: Path) -> list[Any]:
    handles: list[Any] = []
    if not hasattr(os, "add_dll_directory"):
        return handles

    directories = {path.parent for path in (repo_root / "thirdparty").rglob("*.dll")}
    directories.add(repo_root / "code" / "appImmStrokeReader" / "exe")
    for directory in sorted(directories):
        handles.append(os.add_dll_directory(str(directory)))
    return handles


def _load_library(repo_root: Path, library_path: Path) -> tuple[ctypes.CDLL, list[Any]]:
    handles = _add_dependency_directories(repo_root)
    loader = ctypes.WinDLL if os.name == "nt" else ctypes.CDLL
    library = loader(str(library_path.resolve()))
    _configure_api(library)
    return library, handles


def _drawing_manifest(library: ctypes.CDLL, doc_id: int, layer_index: int, drawing_index: int) -> dict[str, Any]:
    stroke_count = library.StrokeReader_GetStrokeCount(doc_id, layer_index, drawing_index)
    point_count = 0
    brush_types: Counter[int] = Counter()
    visibility_modes: Counter[int] = Counter()
    digest = hashlib.sha256()

    for stroke_index in range(stroke_count):
        info = StrokeInfo()
        if not library.StrokeReader_GetStrokeInfo(doc_id, layer_index, drawing_index, stroke_index, ctypes.byref(info)):
            raise RuntimeError(f"Could not query layer {layer_index}, drawing {drawing_index}, stroke {stroke_index}")

        points = (StrokePoint * info.num_points)()
        if info.num_points and not library.StrokeReader_GetStrokePoints(
            doc_id, layer_index, drawing_index, stroke_index, points, info.num_points
        ):
            raise RuntimeError(f"Could not read layer {layer_index}, drawing {drawing_index}, stroke {stroke_index}")

        brush_types[info.brush_type] += 1
        visibility_modes[info.visibility_mode] += 1
        point_count += info.num_points
        digest.update(struct.pack("<iii", info.brush_type, info.visibility_mode, info.num_points))
        digest.update(bytes(points))

    return {
        "index": drawing_index,
        "stroke_count": stroke_count,
        "point_count": point_count,
        "brush_type_counts": {str(key): value for key, value in sorted(brush_types.items())},
        "visibility_mode_counts": {str(key): value for key, value in sorted(visibility_modes.items())},
        "canonical_sha256": digest.hexdigest(),
    }


def _picture_manifest(library: ctypes.CDLL, doc_id: int, layer_index: int) -> dict[str, Any] | None:
    info = StrokePictureInfo()
    if not library.StrokeReader_GetPictureInfo(doc_id, layer_index, ctypes.byref(info)):
        return None

    pixels = (ctypes.c_uint8 * info.data_size)()
    bytes_read = library.StrokeReader_GetPicturePixelData(doc_id, layer_index, pixels, info.data_size)
    if bytes_read != info.data_size:
        raise RuntimeError(f"Picture layer {layer_index} returned {bytes_read} of {info.data_size} bytes")

    return {
        "layer_id": info.layer_id,
        "content_type": info.content_type,
        "is_viewer_locked": bool(info.is_viewer_locked),
        "width": info.width,
        "height": info.height,
        "has_alpha": bool(info.has_alpha),
        "decoded_byte_size": info.data_size,
        "decoded_sha256": hashlib.sha256(bytes(pixels)).hexdigest(),
    }


def _layer_manifest(library: ctypes.CDLL, doc_id: int, layer_index: int) -> dict[str, Any]:
    info = StrokeLayerInfo()
    if not library.StrokeReader_GetLayerInfo(doc_id, layer_index, ctypes.byref(info)):
        raise RuntimeError(f"Could not query layer {layer_index}")

    local = StrokeLayerTransform()
    world = StrokeLayerTransform()
    if not library.StrokeReader_GetLayerTransform(doc_id, layer_index, ctypes.byref(local), ctypes.byref(world)):
        raise RuntimeError(f"Could not query transforms for layer {layer_index}")

    frame_rate = ctypes.c_int()
    num_frames = ctypes.c_int()
    max_repeat_count = ctypes.c_int()
    has_animation = library.StrokeReader_GetLayerAnimationInfo(
        doc_id,
        layer_index,
        ctypes.byref(frame_rate),
        ctypes.byref(num_frames),
        ctypes.byref(max_repeat_count),
    )

    frame_manifest = None
    if has_animation and num_frames.value > 0:
        frames = (ctypes.c_int * num_frames.value)()
        frame_count = library.StrokeReader_GetFrameBuffer(doc_id, layer_index, frames, num_frames.value)
        frame_bytes = struct.pack(f"<{frame_count}i", *frames[:frame_count]) if frame_count else b""
        frame_manifest = {
            "frame_rate": frame_rate.value,
            "num_frames": num_frames.value,
            "max_repeat_count": max_repeat_count.value,
            "frames_read": frame_count,
            "frame_map_sha256": hashlib.sha256(frame_bytes).hexdigest(),
        }

    drawing_count = library.StrokeReader_GetDrawingCount(doc_id, layer_index)
    drawings = [_drawing_manifest(library, doc_id, layer_index, index) for index in range(drawing_count)]
    raw_name = bytes(info.name).split(b"\0", 1)[0]

    return {
        "index": layer_index,
        "id": info.id,
        "type": info.type,
        "name": raw_name.decode("utf-8", errors="replace"),
        "visible": bool(info.visible),
        "opacity": float(info.opacity),
        "pivot": {
            "rotation": _float_list(info.pivot_rotation),
            "scale": float(info.pivot_scale),
            "flip": info.pivot_flip,
            "translation": _float_list(info.pivot_translation),
        },
        "local_transform": _transform_dict(local),
        "world_transform": _transform_dict(world),
        "animation": frame_manifest,
        "drawings": drawings,
        "picture": _picture_manifest(library, doc_id, layer_index),
    }


def generate_manifest(repo_root: Path, input_path: Path, library_path: Path, log_path: Path) -> dict[str, Any]:
    library, _dependency_handles = _load_library(repo_root, library_path)

    init_result = library.StrokeReader_Init(os.fsencode(log_path.resolve()))
    if init_result != 0:
        raise RuntimeError(f"StrokeReader_Init failed with {init_result}")

    source = input_path.read_bytes()
    source_buffer = ctypes.create_string_buffer(source)
    doc_id = -1
    try:
        doc_id = library.StrokeReader_LoadFromMemory(source_buffer, len(source))
        if doc_id <= 0:
            raise RuntimeError(f"StrokeReader_LoadFromMemory failed with {doc_id}")

        layer_count = library.StrokeReader_GetLayerCount(doc_id)
        layers = [_layer_manifest(library, doc_id, index) for index in range(layer_count)]
        return {
            "schema": SCHEMA,
            "decoder_build_id": library.StrokeReader_GetBuildId().decode("ascii"),
            "fixture": {
                "path": input_path.relative_to(repo_root).as_posix(),
                "byte_size": len(source),
                "sha256": _sha256_file(input_path),
            },
            "document": {
                "chapter_count": library.StrokeReader_GetChapterCount(doc_id),
                "layer_count": layer_count,
                "layers": layers,
            },
        }
    finally:
        if doc_id > 0:
            library.StrokeReader_Unload(doc_id)
        library.StrokeReader_End()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path("exampleImmFiles/sample1.imm"))
    parser.add_argument("--library", type=Path, default=Path("code/appImmStrokeReader/exe/ImmStrokeReader.dll"))
    parser.add_argument("--output", type=Path, default=Path("tests/baselines/web/sample1-native-decoder.json"))
    parser.add_argument("--log", type=Path, default=Path("artifacts/web-native-manifest.log"))
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    input_path = (repo_root / args.input).resolve() if not args.input.is_absolute() else args.input.resolve()
    library_path = (repo_root / args.library).resolve() if not args.library.is_absolute() else args.library.resolve()
    output_path = (repo_root / args.output).resolve() if not args.output.is_absolute() else args.output.resolve()
    log_path = (repo_root / args.log).resolve() if not args.log.is_absolute() else args.log.resolve()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    manifest = generate_manifest(repo_root, input_path, library_path, log_path)
    output_path.write_text(f"{json.dumps(manifest, indent=2, sort_keys=True)}\n", encoding="utf-8")
    print(f"Wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
