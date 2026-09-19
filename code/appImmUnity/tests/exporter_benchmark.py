"""Stage-level benchmark for the IMM exporter C ABI and the stroke reader decode path.

Answers the question the engine plan leaves open: *where* the compile-and-reload
time actually goes. It builds the three corpus cases defined in
`docs/runtime-authoring-engine-contract.md` (Small / Medium / Large) through the
real ImmUnityPlugin.dll, timing each stage separately, then decodes the generated
file through ImmStrokeReader.dll to time that side too.

Corpus shape mirrors `Samples~/RuntimeAuthoring/ImmAuthoringBenchmark.cs`:
per layer, `StrokesPerLayer` drawings each holding one stroke of
`PointsPerStroke` points, plus `Frames` frame mappings round-robined over them.

Stages reported (milliseconds):

    create      ImmExporter_CreateSequence
    layers      ImmExporter_CreatePaintLayer x layers
    drawings    CreateDrawing + DrawingInit + ElementInit x strokes
    points      ImmExporter_ElementSetPoints x strokes (batch point transfer)
    bounds      ComputeElementBounds + ComputeDrawingBounds
    frames      ImmExporter_PaintAddFrame x frames
    export-mem  ImmExporter_ExportToMemory + size/data copy + DestroyMemory
    export-file ImmExporter_ExportToFile + file size
    decode      StrokeReader_LoadFromFile of the same bytes (parse + store)

Usage:
    python code/appImmUnity/tests/exporter_benchmark.py
    python code/appImmUnity/tests/exporter_benchmark.py --repeat 9 --cases medium,large
    python code/appImmUnity/tests/exporter_benchmark.py --json artifacts/exporter-benchmark.json
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import statistics
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import exporter_bridge_smoke as smoke  # noqa: E402  (same directory: staging + bindings)

CASES = {
    "small": {"layers": 1, "strokes": 10, "points": 16, "frames": 12},
    "medium": {"layers": 4, "strokes": 100, "points": 32, "frames": 120},
    "large": {"layers": 8, "strokes": 500, "points": 64, "frames": 300},
}

Point = smoke.Point
Transform = smoke.Transform


def make_points(stroke_index: int, point_count: int) -> tuple:
    points = []
    for point_index in range(point_count):
        t = 0.0 if point_count <= 1 else point_index / (point_count - 1.0)
        points.append(
            Point(
                t, 0.0, stroke_index * 0.002,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0,
                0.2 + (stroke_index % 5) * 0.15, 0.4, 0.9,
                1.0,
                0.01,
                t,
                t,
            )
        )
    return (Point * point_count)(*points)


class Exporter:
    """Bound ImmExporter_* entry points."""

    def __init__(self, library: ctypes.CDLL):
        self.create_sequence = smoke.bind(
            library, "ImmExporter_CreateSequence", ctypes.c_void_p,
            ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float,
            ctypes.c_uint32, ctypes.c_int64, ctypes.c_int64, ctypes.c_int64, ctypes.c_int64)
        self.destroy_sequence = smoke.bind(library, "ImmExporter_DestroySequence", None, ctypes.c_void_p)
        self.create_paint_layer = smoke.bind(
            library, "ImmExporter_CreatePaintLayer", ctypes.c_void_p,
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_float,
            ctypes.POINTER(Transform), ctypes.POINTER(Transform), ctypes.c_int, ctypes.c_int64, ctypes.c_uint32)
        self.create_drawing = smoke.bind(library, "ImmExporter_CreateDrawing", ctypes.c_void_p, ctypes.c_void_p)
        self.destroy_drawing = smoke.bind(library, "ImmExporter_DestroyDrawing", None, ctypes.c_void_p)
        self.get_drawing_index = smoke.bind(library, "ImmExporter_GetDrawingIndex", ctypes.c_uint32, ctypes.c_void_p)
        self.drawing_init = smoke.bind(library, "ImmExporter_DrawingInit", ctypes.c_bool,
                                       ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int)
        self.drawing_get_element = smoke.bind(library, "ImmExporter_DrawingGetElement", ctypes.c_void_p,
                                              ctypes.c_void_p, ctypes.c_uint32)
        self.element_init = smoke.bind(library, "ImmExporter_ElementInit", ctypes.c_bool,
                                       ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_int)
        self.element_set_points = smoke.bind(library, "ImmExporter_ElementSetPoints", ctypes.c_bool,
                                             ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(Point), ctypes.c_uint32)
        self.compute_element_bounds = smoke.bind(library, "ImmExporter_ComputeElementBounds", None, ctypes.c_void_p)
        self.compute_drawing_bounds = smoke.bind(library, "ImmExporter_ComputeDrawingBounds", None, ctypes.c_void_p)
        self.paint_add_frame = smoke.bind(library, "ImmExporter_PaintAddFrame", None, ctypes.c_void_p, ctypes.c_uint32)
        self.export_to_file = smoke.bind(library, "ImmExporter_ExportToFile", ctypes.c_bool,
                                         ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int)
        self.export_to_memory = smoke.bind(library, "ImmExporter_ExportToMemory", ctypes.c_void_p,
                                           ctypes.c_void_p, ctypes.c_int, ctypes.c_int)
        self.get_memory_size = smoke.bind(library, "ImmExporter_GetMemorySize", ctypes.c_uint64, ctypes.c_void_p)
        self.get_memory_data = smoke.bind(library, "ImmExporter_GetMemoryData", ctypes.c_void_p, ctypes.c_void_p)
        self.destroy_memory = smoke.bind(library, "ImmExporter_DestroyMemory", None, ctypes.c_void_p)


def build_case(exporter: Exporter, case: dict, output_path: Path) -> dict:
    """Build one corpus case, returning per-stage milliseconds and byte sizes."""
    layers = case["layers"]
    strokes = case["strokes"]
    point_count = case["points"]
    frames = case["frames"]

    stages = {name: 0.0 for name in
              ("create", "layers", "drawings", "points", "bounds", "frames", "export-mem", "export-file")}
    identity = Transform(0, 0, 0, 0, 0, 0, 1, 1)

    start = time.perf_counter()
    sequence = smoke.require(
        exporter.create_sequence(1, 0, 0.0, 0.0, 0.0, 30, 0, 0, 0, 0), "ImmExporter_CreateSequence")
    stages["create"] = (time.perf_counter() - start) * 1000.0

    try:
        for layer_index in range(layers):
            start = time.perf_counter()
            layer = smoke.require(
                exporter.create_paint_layer(
                    sequence, None, f"Paint {layer_index}".encode(), 1, 1.0,
                    ctypes.byref(identity), ctypes.byref(identity), 1, 0, 0),
                "ImmExporter_CreatePaintLayer")
            stages["layers"] += (time.perf_counter() - start) * 1000.0

            drawing_indices = []
            for stroke_index in range(strokes):
                start = time.perf_counter()
                drawing = smoke.require(exporter.create_drawing(layer), "ImmExporter_CreateDrawing")
                smoke.require(exporter.drawing_init(drawing, 1, 0), "ImmExporter_DrawingInit")
                element = smoke.require(exporter.drawing_get_element(drawing, 0), "ImmExporter_DrawingGetElement")
                smoke.require(exporter.element_init(element, point_count, 2, 1), "ImmExporter_ElementInit")
                stages["drawings"] += (time.perf_counter() - start) * 1000.0

                points = make_points(stroke_index, point_count)
                start = time.perf_counter()
                smoke.require(exporter.element_set_points(element, 0, points, point_count),
                              "ImmExporter_ElementSetPoints")
                stages["points"] += (time.perf_counter() - start) * 1000.0

                start = time.perf_counter()
                exporter.compute_element_bounds(element)
                exporter.compute_drawing_bounds(drawing)
                stages["bounds"] += (time.perf_counter() - start) * 1000.0

                drawing_indices.append(exporter.get_drawing_index(drawing))
                exporter.destroy_drawing(drawing)

            start = time.perf_counter()
            for frame in range(frames):
                exporter.paint_add_frame(layer, drawing_indices[frame % len(drawing_indices)])
            stages["frames"] += (time.perf_counter() - start) * 1000.0

        start = time.perf_counter()
        memory = smoke.require(exporter.export_to_memory(sequence, 96000, 0), "ImmExporter_ExportToMemory")
        memory_bytes = int(exporter.get_memory_size(memory))
        data = exporter.get_memory_data(memory)
        smoke.require(data != 0, "ImmExporter_GetMemoryData")
        exporter.destroy_memory(memory)
        stages["export-mem"] = (time.perf_counter() - start) * 1000.0

        start = time.perf_counter()
        smoke.require(exporter.export_to_file(sequence, os.fsencode(output_path), 96000, 0),
                      "ImmExporter_ExportToFile")
        stages["export-file"] = (time.perf_counter() - start) * 1000.0
    finally:
        exporter.destroy_sequence(sequence)

    return {
        "stages": stages,
        "bytes": memory_bytes,
        "fileBytes": output_path.stat().st_size,
        "drawings": layers * strokes,
        "points": layers * strokes * point_count,
        "frames": layers * frames,
    }


def decode_case(reader: ctypes.CDLL, output_path: Path) -> float:
    """Time StrokeReader_LoadFromFile (importer parse + stroke store fill).

    The caller must have initialized the reader (see initialize_reader).
    """
    load = smoke.bind(reader, "StrokeReader_LoadFromFile", ctypes.c_int, ctypes.c_char_p)
    unload = smoke.bind(reader, "StrokeReader_Unload", None, ctypes.c_int)
    start = time.perf_counter()
    document_id = load(os.fsencode(output_path))
    elapsed = (time.perf_counter() - start) * 1000.0
    smoke.require(document_id > 0, f"StrokeReader_LoadFromFile ({document_id})")
    unload(document_id)
    return elapsed


def initialize_reader(reader: ctypes.CDLL, staging: Path) -> None:
    """StrokeReader_Init once; its log goes to the staging directory."""
    init = smoke.bind(reader, "StrokeReader_Init", ctypes.c_int, ctypes.c_char_p)
    is_initialized = smoke.bind(reader, "StrokeReader_IsInitialized", ctypes.c_bool)
    if not is_initialized():
        smoke.require(init(os.fsencode(staging / "benchmark-stroke-reader.log")) == 0, "StrokeReader_Init")


def finish_reader(reader: ctypes.CDLL) -> None:
    is_initialized = smoke.bind(reader, "StrokeReader_IsInitialized", ctypes.c_bool)
    end = smoke.bind(reader, "StrokeReader_End", None)
    if is_initialized():
        end()


def summarize(samples: list[float]) -> dict:
    return {
        "min": min(samples),
        "median": statistics.median(samples),
        "max": max(samples),
        "mean": statistics.fmean(samples),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repeat", type=int, default=5, help="iterations per case (default 5)")
    parser.add_argument("--cases", default="small,medium,large", help="comma separated case names")
    parser.add_argument("--json", type=Path, help="write the full result set as JSON")
    parser.add_argument("--keep", type=Path, help="keep generated .imm files in this directory")
    arguments = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[3]
    plugin = repo_root / "code/appImmUnity/exe/ImmUnityPlugin.dll"
    stroke_reader = (repo_root / "code/ImmUnitySampleProject/Packages/"
                                "com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll")
    for required in (plugin, stroke_reader):
        if not required.is_file():
            raise FileNotFoundError(f"required binary not found: {required}")

    selected = [name.strip().lower() for name in arguments.cases.split(",") if name.strip()]
    for name in selected:
        if name not in CASES:
            raise SystemExit(f"unknown case '{name}' (known: {', '.join(CASES)})")

    results: dict[str, dict] = {}
    # mkdtemp + best-effort cleanup: the plugin keeps its codec DLLs mapped until the
    # process ends in some runs, and a failed rmtree must not lose the measurements.
    temporary = tempfile.mkdtemp(prefix="imm-exporter-benchmark-")
    try:
        staging = Path(temporary)
        smoke.shutil.copy2(plugin, staging / plugin.name)
        smoke.shutil.copy2(stroke_reader, staging / stroke_reader.name)
        for dependency in smoke.runtime_dependencies(repo_root):
            smoke.shutil.copy2(dependency, staging / dependency.name)

        ctypes.windll.kernel32.SetDllDirectoryW(str(staging))
        library = ctypes.CDLL(str(staging / plugin.name))
        reader = ctypes.CDLL(str(staging / stroke_reader.name))
        library_handle = library._handle
        reader_handle = reader._handle
        try:
            exporter = Exporter(library)
            initialize_reader(reader, staging)
            keep_dir = arguments.keep
            if keep_dir is not None:
                keep_dir.mkdir(parents=True, exist_ok=True)

            for name in selected:
                case = CASES[name]
                samples = []
                for iteration in range(arguments.repeat):
                    output = (keep_dir / f"{name}.imm") if keep_dir else (staging / f"{name}-{iteration}.imm")
                    samples.append(build_case(exporter, case, output))

                merged = {stage: summarize([s["stages"][stage] for s in samples])
                          for stage in samples[0]["stages"]}
                decode_path = (keep_dir / f"{name}.imm") if keep_dir else (staging / f"{name}-0.imm")
                decode = [decode_case(reader, decode_path) for _ in range(arguments.repeat)]

                results[name] = {
                    "case": case,
                    "drawings": samples[0]["drawings"],
                    "points": samples[0]["points"],
                    "frames": samples[0]["frames"],
                    "bytes": samples[0]["bytes"],
                    "fileBytes": samples[0]["fileBytes"],
                    "stages": merged,
                    "decode": summarize(decode),
                }
        finally:
            finish_reader(reader)
            del reader
            smoke._ctypes.FreeLibrary(reader_handle)
            del library
            smoke._ctypes.FreeLibrary(library_handle)
            smoke.release_runtime_dependencies(repo_root)
            ctypes.windll.kernel32.SetDllDirectoryW(None)
    finally:
        smoke.shutil.rmtree(temporary, ignore_errors=True)

    print(f"reference binary: {plugin}")
    print(f"iterations per case: {arguments.repeat}\n")
    header = f"{'case':7s} {'draw':>6s} {'points':>8s} {'KB':>7s} " + " ".join(
        f"{s:>10s}" for s in ("create", "layers", "drawings", "points", "bounds", "frames", "exp-mem", "exp-file", "decode"))
    print(header)
    print("-" * len(header))
    for name, data in results.items():
        stages = data["stages"]
        row = (f"{name:7s} {data['drawings']:6d} {data['points']:8d} "
               f"{data['bytes'] / 1024.0:7.1f} ")
        row += " ".join(f"{stages[s]['median']:10.2f}" for s in
                        ("create", "layers", "drawings", "points", "bounds", "frames", "export-mem", "export-file"))
        row += f" {data['decode']['median']:10.2f}"
        print(row)

    print("\nmedians in ms; exp-mem/exp-file include graph walk + compression;")
    print("decode is StrokeReader_LoadFromFile (importer parse + stroke store) of the same bytes.")

    if arguments.json is not None:
        arguments.json.parent.mkdir(parents=True, exist_ok=True)
        arguments.json.write_text(json.dumps(results, indent=2), encoding="utf-8")
        print(f"\nwrote {arguments.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
