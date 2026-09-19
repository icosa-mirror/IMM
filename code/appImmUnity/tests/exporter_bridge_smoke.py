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


class LayerTransform(ctypes.Structure):
    _fields_ = [
        ("rotation", ctypes.c_float * 4),
        ("scale", ctypes.c_float),
        ("flip", ctypes.c_int),
        ("translation", ctypes.c_float * 3),
    ]


class LayerInfo(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("type", ctypes.c_int),
        ("numDrawings", ctypes.c_int),
        ("name", ctypes.c_char * 256),
        ("visible", ctypes.c_int),
        ("opacity", ctypes.c_float),
        ("isDefaultSpawn", ctypes.c_int),
        ("pivotRotation", ctypes.c_float * 4),
        ("pivotScale", ctypes.c_float),
        ("pivotFlip", ctypes.c_int),
        ("pivotTranslation", ctypes.c_float * 3),
    ]


class AuthoringLayerInfo(ctypes.Structure):
    _fields_ = [
        ("legacy", LayerInfo),
        ("parentId", ctypes.c_int),
        ("childIndex", ctypes.c_int),
        ("isTimeline", ctypes.c_int),
        ("durationTicks", ctypes.c_int64),
        ("maxRepeatCount", ctypes.c_uint32),
    ]


class AnimationKey(ctypes.Structure):
    _fields_ = [
        ("property", ctypes.c_int),
        ("timeTicks", ctypes.c_int64),
        ("interpolation", ctypes.c_int),
        ("boolValue", ctypes.c_int),
        ("intValue", ctypes.c_uint32),
        ("floatValue", ctypes.c_float),
        ("doubleValue", ctypes.c_double),
        ("transformValue", LayerTransform),
    ]


class PerformanceInfo(ctypes.Structure):
    """Mirror of the native ImmUnityPerformanceInfo."""

    _fields_ = [
        ("cpuLoadTimeMS", ctypes.c_int),
        ("numDrawCalls", ctypes.c_int),
        ("numDrawCallsCulled", ctypes.c_int),
        ("numPaintDrawCalls", ctypes.c_int),
        ("numPictureDrawCalls", ctypes.c_int),
        ("numPicture2DDrawCalls", ctypes.c_int),
        ("numPicture360DrawCalls", ctypes.c_int),
        ("numPicture360EquirectDrawCalls", ctypes.c_int),
        ("numPicture360CubemapDrawCalls", ctypes.c_int),
        ("numModelDrawCalls", ctypes.c_int),
        ("numTriangles", ctypes.c_int),
        ("numTrianglesCulled", ctypes.c_int),
        ("gpuTimeAverageMs", ctypes.c_float),
    ]


# Layer::AnimProperty / Layer::InterpolationType, mirrored by the managed enums.
PROPERTY_VISIBILITY = 0
PROPERTY_DRAW_IN_TIME = 5
INTERPOLATION_LINEAR = 1

PAINT_LAYER_NAME = b"Smoke Paint"
PAINT_REPEAT_COUNT = 3
DRAW_IN_TIME_SECONDS = 0.25


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
    imm_unity_directory = (
        repo_root
        / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64"
    )
    stroke_reader_directory = (
        repo_root
        / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64"
    )
    return (
        *(
            imm_unity_directory / name
            for name in (
                "Audio360.dll",
                "opusenc.dll",
                "opus.dll",
                "vorbisenc.dll",
            )
        ),
        *(
            stroke_reader_directory / name
            for name in (
                "zlib1.dll",
                "jpeg62.dll",
                "libpng16.dll",
                "ogg.dll",
                "vorbis.dll",
            )
        ),
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
    create_group_layer = bind(
        library,
        "ImmExporter_CreateGroupLayer",
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
    paint_set_max_repeat_count = bind(
        library,
        "ImmExporter_PaintSetMaxRepeatCount",
        ctypes.c_bool,
        ctypes.c_void_p,
        ctypes.c_uint32,
    )
    layer_add_animation_key = bind(
        library,
        "ImmExporter_LayerAddAnimationKey",
        ctypes.c_bool,
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int64,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_uint32,
        ctypes.c_float,
        ctypes.c_double,
        ctypes.POINTER(Transform),
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
                PAINT_LAYER_NAME,
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
        require(
            paint_set_max_repeat_count(layer, PAINT_REPEAT_COUNT),
            "ImmExporter_PaintSetMaxRepeatCount",
        )
        require(
            layer_add_animation_key(
                layer,
                PROPERTY_VISIBILITY,
                0,
                INTERPOLATION_LINEAR,
                1,
                0,
                0.0,
                0.0,
                ctypes.byref(identity),
            ),
            "ImmExporter_LayerAddAnimationKey (visibility)",
        )
        require(
            layer_add_animation_key(
                layer,
                PROPERTY_DRAW_IN_TIME,
                5,
                INTERPOLATION_LINEAR,
                0,
                0,
                0.0,
                DRAW_IN_TIME_SECONDS,
                ctypes.byref(identity),
            ),
            "ImmExporter_LayerAddAnimationKey (draw-in time)",
        )

        # Rejections: a null handle, an out-of-range property, an out-of-range
        # interpolation, and a repeat count aimed at a non-paint layer.
        require(
            not layer_add_animation_key(
                None,
                PROPERTY_VISIBILITY,
                0,
                INTERPOLATION_LINEAR,
                1,
                0,
                0.0,
                0.0,
                ctypes.byref(identity),
            ),
            "ImmExporter_LayerAddAnimationKey null-handle rejection",
        )
        require(
            not layer_add_animation_key(
                layer,
                99,
                0,
                INTERPOLATION_LINEAR,
                1,
                0,
                0.0,
                0.0,
                ctypes.byref(identity),
            ),
            "ImmExporter_LayerAddAnimationKey invalid-property rejection",
        )
        require(
            not layer_add_animation_key(
                layer,
                PROPERTY_VISIBILITY,
                0,
                99,
                1,
                0,
                0.0,
                0.0,
                ctypes.byref(identity),
            ),
            "ImmExporter_LayerAddAnimationKey invalid-interpolation rejection",
        )
        group = require(
            create_group_layer(
                sequence,
                None,
                b"Smoke Group",
                1,
                1.0,
                ctypes.byref(identity),
                ctypes.byref(identity),
                0,
                0,
                0,
            ),
            "ImmExporter_CreateGroupLayer",
        )
        require(
            not paint_set_max_repeat_count(group, PAINT_REPEAT_COUNT),
            "ImmExporter_PaintSetMaxRepeatCount group-layer rejection",
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


def verify_export_round_trip(staging: Path, output_path: Path) -> None:
    """Re-read the exported file through ImmStrokeReader and check the authored keys."""
    reader_path = staging / "ImmStrokeReader.dll"
    if not reader_path.is_file():
        raise FileNotFoundError(f"Stroke reader plugin not found: {reader_path}")

    reader = ctypes.CDLL(str(reader_path))
    reader_handle = reader._handle
    try:
        _verify_export_round_trip(reader, staging, output_path)
    finally:
        del reader
        _ctypes.FreeLibrary(reader_handle)


def _verify_export_round_trip(reader: ctypes.CDLL, staging: Path, output_path: Path) -> None:
    init = bind(reader, "StrokeReader_Init", ctypes.c_int, ctypes.c_char_p)
    is_initialized = bind(reader, "StrokeReader_IsInitialized", ctypes.c_bool)
    load_from_file = bind(reader, "StrokeReader_LoadFromFile", ctypes.c_int, ctypes.c_char_p)
    get_authoring_layer_count = bind(
        reader, "StrokeReader_GetAuthoringLayerCount", ctypes.c_int, ctypes.c_int
    )
    get_authoring_layer_info = bind(
        reader,
        "StrokeReader_GetAuthoringLayerInfo",
        ctypes.c_bool,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(AuthoringLayerInfo),
    )
    get_key_count = bind(
        reader, "StrokeReader_GetLayerAnimationKeyCount", ctypes.c_int, ctypes.c_int, ctypes.c_int
    )
    get_key = bind(
        reader,
        "StrokeReader_GetLayerAnimationKey",
        ctypes.c_bool,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(AnimationKey),
    )
    unload = bind(reader, "StrokeReader_Unload", None, ctypes.c_int)
    end = bind(reader, "StrokeReader_End", None)

    if not is_initialized():
        require(init(os.fsencode(staging / "stroke-reader-log.txt")) == 0, "StrokeReader_Init")

    document_id = load_from_file(os.fsencode(output_path))
    require(document_id > 0, "StrokeReader_LoadFromFile")
    try:
        paint_layer = None
        for index in range(get_authoring_layer_count(document_id)):
            info = AuthoringLayerInfo()
            if not get_authoring_layer_info(document_id, index, ctypes.byref(info)):
                continue
            if info.legacy.name.split(b"\0", 1)[0] == PAINT_LAYER_NAME:
                paint_layer = (index, info)
                break

        require(paint_layer is not None, "authored paint layer read-back")
        index, info = paint_layer
        require(
            info.maxRepeatCount == PAINT_REPEAT_COUNT,
            f"round-trip maxRepeatCount ({info.maxRepeatCount} != {PAINT_REPEAT_COUNT})",
        )

        key_count = get_key_count(document_id, index)
        require(
            key_count == 2,
            f"round-trip animation key count ({key_count} != 2)",
        )

        found = {}
        for key_index in range(key_count):
            key = AnimationKey()
            require(
                get_key(document_id, index, key_index, ctypes.byref(key)),
                "StrokeReader_GetLayerAnimationKey",
            )
            found[key.property] = key

        require(PROPERTY_VISIBILITY in found, "round-trip visibility key")
        require(found[PROPERTY_VISIBILITY].boolValue == 1, "round-trip visibility value")
        require(PROPERTY_DRAW_IN_TIME in found, "round-trip draw-in-time key")
        require(
            abs(found[PROPERTY_DRAW_IN_TIME].doubleValue - DRAW_IN_TIME_SECONDS) < 1e-6,
            "round-trip draw-in-time value",
        )
    finally:
        unload(document_id)
        end()


def release_runtime_dependencies(repo_root: Path) -> None:
    """Drop handles this process took on the staged codec DLLs, so staging can be deleted."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetModuleHandleW.restype = ctypes.c_void_p
    kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    kernel32.FreeLibrary.restype = ctypes.c_bool
    kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
    for dependency in reversed(runtime_dependencies(repo_root)):
        dependency_handle = kernel32.GetModuleHandleW(dependency.name)
        if dependency_handle:
            kernel32.FreeLibrary(dependency_handle)


def verify_capability_exports(library: ctypes.CDLL) -> None:
    """Exercise the read-only player capability exports that are safe before Init.

    Document-scoped commands (UnloadAll, PauseAt, CancelDocumentLoad) are skipped:
    they index the player's document table, which only exists after Init.
    """
    get_load_time = bind(library, "GetLoadTimeInMs", ctypes.c_int)
    set_perf_enabled = bind(library, "SetPerformanceMeasurementEnabled", None, ctypes.c_int)
    get_perf_info = bind(library, "GetPerformanceInfo", None, ctypes.POINTER(PerformanceInfo))
    get_chapter_info = bind(
        library,
        "GetChapterInfoEx",
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_int64),
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_int),
    )
    get_has_audio = bind(library, "GetDocumentHasAudio", ctypes.c_bool, ctypes.c_int)

    # Only meaningful after a load (it measures from the load start time, which is
    # never set before Init), so this only proves the entry point resolves.
    get_load_time()

    set_perf_enabled(1)
    performance = PerformanceInfo()
    get_perf_info(ctypes.byref(performance))
    require(
        performance.numDrawCalls == 0 and performance.numTriangles == 0,
        "GetPerformanceInfo reports no counters before any render",
    )

    # An unknown document id must resolve to zero chapters without leaking the
    # temporary chapter-length array the export allocates.
    has_plays = ctypes.c_int(1)
    lengths = (ctypes.c_int64 * 4)()
    count = get_chapter_info(-1, lengths, len(lengths), ctypes.byref(has_plays))
    require(count == 0, f"GetChapterInfoEx unknown document ({count})")
    require(has_plays.value == 0, "GetChapterInfoEx clears hasPlays for an unknown document")
    require(
        get_chapter_info(-1, None, 0, None) == 0,
        "GetChapterInfoEx tolerates a null output buffer",
    )

    require(not get_has_audio(-1), "GetDocumentHasAudio unknown document")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[3]
    plugin = arguments.plugin or repo_root / "code/appImmUnity/exe/ImmUnityPlugin.dll"
    if not plugin.is_file():
        raise FileNotFoundError(f"Unity plugin not found: {plugin}")
    stroke_reader = (
        repo_root
        / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll"
    )
    if not stroke_reader.is_file():
        raise FileNotFoundError(f"Stroke reader plugin not found: {stroke_reader}")

    with tempfile.TemporaryDirectory(prefix="imm-exporter-bridge-") as temporary:
        staging = Path(temporary)
        staged_plugin = staging / plugin.name
        shutil.copy2(plugin, staged_plugin)
        shutil.copy2(stroke_reader, staging / stroke_reader.name)
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
            verify_capability_exports(library)
        finally:
            del library
            _ctypes.FreeLibrary(library_handle)
            release_runtime_dependencies(repo_root)

        data = output_path.read_bytes()
        require(data[:8] == b"Immersiv", "IMM signature validation")
        require(b"Root" in data, "Root layer serialization validation")
        require(b"Smoke Paint" in data, "Paint layer serialization validation")
        require(len(data) > 64, "IMM payload size validation")

        try:
            verify_export_round_trip(staging, output_path)
        finally:
            release_runtime_dependencies(repo_root)
            ctypes.windll.kernel32.SetDllDirectoryW(None)

        if arguments.output is None:
            print(f"{PREFIX} Exported and validated {len(data)} bytes")
        else:
            print(f"{PREFIX} Exported and validated {len(data)} bytes at {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())