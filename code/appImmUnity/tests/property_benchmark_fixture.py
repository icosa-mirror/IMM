"""Generate spatially separated, independently inventoried live property fixtures."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

import exporter_bridge_smoke as smoke
from exporter_benchmark import Exporter, finish_reader, initialize_reader, verify_live_corpus

ROOT = Path(__file__).resolve().parents[3]
CASES = {
    'small': {'layers': 2, 'strokes': 5, 'points': 16, 'frames': 12},
    'medium': {'layers': 4, 'strokes': 100, 'points': 32, 'frames': 120},
    'large': {'layers': 8, 'strokes': 500, 'points': 64, 'frames': 500},
}


def build_fixture(exporter, name, path):
    case = CASES[name]
    columns = 4 if name == 'large' else 2
    rows = case['layers'] // columns
    identity = smoke.Transform(0, 0, 0, 0, 0, 0, 1, 1)
    sequence = smoke.require(exporter.create_sequence(1, 0, 0, 0, 0, 30, 0, 0, 0, 0), 'property sequence')
    positions = []
    try:
        for layer_index in range(case['layers']):
            x = (layer_index % columns - (columns - 1) / 2) * .75
            y = (layer_index // columns - (rows - 1) / 2) * .6
            transform = smoke.Transform(x, y, 0, 0, 0, 0, 1, 1)
            layer_name = 'PropertyTarget' if layer_index == 0 else f'PropertySibling{layer_index}'
            layer = smoke.require(exporter.create_paint_layer(sequence, None, layer_name.encode(), 1, 1,
                ctypes.byref(transform), ctypes.byref(identity), 0, 0, 0), 'property paint layer')
            indices = []
            for drawing_index in range(case['strokes']):
                drawing = smoke.require(exporter.create_drawing(layer), 'property drawing')
                try:
                    smoke.require(exporter.drawing_init(drawing, 1, 0), 'property drawing init')
                    element = smoke.require(exporter.drawing_get_element(drawing, 0), 'property element')
                    smoke.require(exporter.element_init(element, case['points'], 2, 1), 'property Circle/Always')
                    color = (1.0, .1, .1) if layer_index == 0 else (.1, .2, 1.0)
                    points = []
                    for index in range(case['points']):
                        t = index / (case['points'] - 1)
                        points.append(smoke.Point(2 + .5 * t, 1.5, drawing_index * .002,
                            0, 1, 0, 0, 0, 1, *color, 1, .03, t, t))
                    buffer = (smoke.Point * case['points'])(*points)
                    smoke.require(exporter.element_set_points(element, 0, buffer, len(buffer)), 'property points')
                    exporter.compute_element_bounds(element)
                    exporter.compute_drawing_bounds(drawing)
                    indices.append(exporter.get_drawing_index(drawing))
                finally:
                    exporter.destroy_drawing(drawing)
            for frame in range(case['frames']):
                exporter.paint_add_frame(layer, indices[frame % len(indices)])
            positions.append({'name': layer_name, 'translation': [x, y, 0]})
        smoke.require(exporter.export_to_file(sequence, os.fsencode(path), 96000, 0), 'property export')
    finally:
        exporter.destroy_sequence(sequence)
    return positions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--cases', default='small,medium,large')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'code/appImmUnity/exe/ImmUnityPlugin.dll')
    parser.add_argument('--stroke-reader', type=Path, default=ROOT / 'code/ImmUnitySampleProject/Packages/'
        'com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll')
    args = parser.parse_args()
    names = [name.strip().lower() for name in args.cases.split(',') if name.strip()]
    if not names or len(set(names)) != len(names) or any(name not in CASES for name in names):
        parser.error('cases must select distinct small, medium or large')
    if args.output_dir.exists() and any(args.output_dir.iterdir()):
        parser.error('output directory must be new or empty')
    for path in (args.plugin, args.stroke_reader):
        if not path.is_file():
            raise FileNotFoundError(path)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix='imm-property-fixture-'))
    # The process owns loaded codec DLLs until exit; retaining this temporary
    # binary staging avoids unlink attempts against mapped dependencies.
    for path in (args.plugin, args.stroke_reader, *smoke.runtime_dependencies(ROOT)):
        shutil.copy2(path, staging / path.name)
    ctypes.windll.kernel32.SetDllDirectoryW(str(staging))
    plugin = ctypes.CDLL(str(staging / args.plugin.name))
    reader = ctypes.CDLL(str(staging / args.stroke_reader.name))
    manifest = {'workload': 'property-live-v1', 'cases': {},
        'pluginSha256': hashlib.sha256(args.plugin.read_bytes()).hexdigest(),
        'readerSha256': hashlib.sha256(args.stroke_reader.read_bytes()).hexdigest(),
        'strokeWidth': .03, 'frameRate': 30,
        'endpoint': 'internal native Commit entry to publication; no display or GPU completion claim'}
    initialize_reader(reader, staging)
    try:
        exporter = Exporter(plugin)
        for name in names:
            path = args.output_dir / f'{name}.imm'
            positions = build_fixture(exporter, name, path)
            inventory = verify_live_corpus(reader, path, CASES[name])
            manifest['cases'][name] = {'fixtureSha256': inventory['fixtureSha256'],
                'drawings': inventory['drawings'], 'pointsPerDrawing': CASES[name]['points'],
                'paintLayers': CASES[name]['layers'], 'frames': inventory['frames'],
                'nativeCorpus': inventory, 'layerTransforms': positions,
                'targetColor': [1, .1, .1], 'siblingColor': [.1, .2, 1],
                'targetName': 'PropertyTarget', 'siblingName': 'PropertySibling1',
                'transformDelta': [.15, 0, 0], 'changedOpacity': .25}
        (args.output_dir / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    finally:
        finish_reader(reader)
    print(f'[IMM_PROPERTY_FIXTURE] generated {len(names)} inventoried property-live-v1 fixtures')


if __name__ == '__main__':
    main()
