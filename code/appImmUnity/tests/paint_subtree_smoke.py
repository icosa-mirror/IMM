"""Exercise real Windows paint lifecycle and renderer ownership failures."""
from pathlib import Path
import argparse
import ctypes as c
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

import exporter_bridge_smoke as smoke
from exporter_benchmark import Exporter, Transform, make_points
from live_edit_benchmark_report import runtime_provenance


def fixture(plugin, output):
    library = c.CDLL(str(plugin))
    exporter = Exporter(library)
    group = smoke.bind(library, 'ImmExporter_CreateGroupLayer', c.c_void_p,
        c.c_void_p, c.c_void_p, c.c_char_p, c.c_int, c.c_float,
        c.POINTER(Transform), c.POINTER(Transform), c.c_int, c.c_int64, c.c_uint32)
    identity = Transform(0, 0, 0, 0, 0, 0, 1, 1)
    sequence = smoke.require(exporter.create_sequence(1, 0, 0, 0, 0, 30, 0, 0, 0, 0), 'sequence')
    try:
        target = smoke.require(group(sequence, None, b'DeleteTarget', 1, 1,
            c.byref(identity), c.byref(identity), 0, 0, 1), 'target group')
        nested = smoke.require(group(sequence, target, b'DeleteNested', 1, 1,
            c.byref(identity), c.byref(identity), 0, 0, 1), 'nested group')
        for parent, name, count, x in ((target, b'DeletePaintA', 2, 2),
                                      (nested, b'DeletePaintB', 3, 2), (None, b'KeepPaint', 2, -2)):
            paint = smoke.require(exporter.create_paint_layer(sequence, parent, name, 1, 1,
                c.byref(identity), c.byref(identity), 0, 0, 0), 'paint layer')
            for index in range(count):
                drawing = smoke.require(exporter.create_drawing(paint), 'drawing')
                try:
                    smoke.require(exporter.drawing_init(drawing, 1, 0), 'drawing init')
                    element = smoke.require(exporter.drawing_get_element(drawing, 0), 'element')
                    smoke.require(exporter.element_init(element, 16, 2, 1), 'Circle/Always')
                    points = make_points(0, 16, live_workload=True)
                    for point in points:
                        point.px += x - 2
                    smoke.require(exporter.element_set_points(element, 0, points, 16), 'points')
                    exporter.compute_element_bounds(element)
                    exporter.compute_drawing_bounds(drawing)
                    exporter.paint_add_frame(paint, exporter.get_drawing_index(drawing))
                finally:
                    exporter.destroy_drawing(drawing)
        smoke.require(exporter.export_to_file(sequence, os.fsencode(output), 96000, 0), 'export')
    finally:
        exporter.destroy_sequence(sequence)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--operation', choices=('delete', 'create'), default='delete')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows standalone renderer required')
    viewer = args.viewer.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixture_path = output / 'nested-paint.imm'
    handles = [os.add_dll_directory(str(path.parent)) for path in smoke.runtime_dependencies(Path(__file__).resolve().parents[3])]
    fixture(args.plugin.resolve(), fixture_path)
    template = json.loads((viewer.parent / 'settings-baseline-directx-sample1.json').read_text(encoding='utf-8'))
    report = {'operation': args.operation, 'viewerSha256': hashlib.sha256(viewer.read_bytes()).hexdigest(),
              'pluginSha256': hashlib.sha256(args.plugin.read_bytes()).hexdigest(),
              'fixtureSha256': hashlib.sha256(fixture_path.read_bytes()).hexdigest(), 'cases': {}}
    cases = ('normal', 'cpu-failure', 'gpu-failure', 'present-failure', 'pending-unload') if args.operation == 'create' else (
        'normal', 'retirement-failure', 'replacement-retirement-failure')
    for case in cases:
        runtime = output / case
        runtime.mkdir()
        shutil.copy2(fixture_path, runtime / 'nested-paint.imm')
        owned_viewer = runtime / viewer.name
        shutil.copy2(viewer, owned_viewer)
        for path in viewer.parent.iterdir():
            if path.is_dir():
                shutil.copytree(path, runtime / path.name)
            elif path.suffix.lower() == '.dll':
                shutil.copy2(path, runtime / path.name)
        settings = json.loads(json.dumps(template))
        settings['File']['Load'] = [str(fixture_path)]
        settings['Rendering'].update({'RenderingAPI': 'DirectX', 'EnableVR': False, 'RenderingTechnique': 'Static'})
        settings['Playback']['PlayerSpawn']['Location'] = 'Custom'
        settings['Playback']['PlayerSpawn']['Custom'] = {'Rotation': [0, 0, 0, 1], 'Scale': 1,
            'Flip': 'N', 'Translation': [.25, 1.5, 4]}
        settings_path = runtime / 'settings.json'
        settings_path.write_text(json.dumps(settings, indent=2), encoding='utf-8')
        environment = {key: value for key, value in os.environ.items() if not key.startswith(
            ('IMM_VIEWER_VALIDATE_', 'IMM_GL_VALIDATE_', 'IMM_LIVE_EDIT_', 'IMM_VIEWER_LIVE_', 'IMM_UNITY_SKIP_GPU_LOAD_'))}
        environment.update({'IMM_VIEWER_LIVE_EDIT': '60', 'IMM_VIEWER_LIVE_DELETE_SUBTREE': '1',
            'IMM_LIVE_EDIT_TRACE_FRAMES': '1', 'IMM_VIEWER_VALIDATE_FRAME': '60',
            'IMM_VIEWER_VALIDATE_MAX_FRAME': '100000', 'IMM_VIEWER_VALIDATE_MIN_NONZERO': '16',
            'IMM_VIEWER_VALIDATE_MIN_DRAWCALLS': '1', 'IMM_VIEWER_VALIDATE_MIN_TRIANGLES': '1',
            'IMM_VIEWER_VALIDATE_DISABLE_AUDIO': '1', 'IMM_VIEWER_VALIDATE_LOCK_CAMERA': '1',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS': '0', 'IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS': '0', 'IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_CAPTURE_PATH': str(runtime / 'capture.ppm')})
        if args.operation == 'create':
            environment.pop('IMM_VIEWER_LIVE_DELETE_SUBTREE')
            environment['IMM_VIEWER_LIVE_CREATE_PAINT'] = '1'
            failure = {'cpu-failure': 'IMM_LIVE_EDIT_FAIL_PREPARE_CPU',
                       'gpu-failure': 'IMM_LIVE_EDIT_FAIL_PREPARE_GPU', 'present-failure': 'IMM_LIVE_EDIT_FAIL_PRESENT'}
            if case in failure:
                environment[failure[case]] = '1'
            if case == 'pending-unload':
                environment['IMM_VIEWER_LIVE_CREATE_PAINT_UNLOAD'] = '1'
                environment['IMM_LIVE_EDIT_HOLD_PAINT_CREATION'] = '1'
        elif case != 'normal':
            environment['IMM_LIVE_EDIT_FAIL_RETIREMENT'] = '1'
        if case == 'replacement-retirement-failure':
            environment.pop('IMM_VIEWER_LIVE_DELETE_SUBTREE')
            environment['IMM_VIEWER_VALIDATE_FRAME'] = '2000'
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        started = time.monotonic()
        with (runtime / 'stdout.log').open('wb') as log:
            child = subprocess.Popen([str(owned_viewer), str(settings_path)], cwd=runtime, env=environment,
                stdout=log, stderr=subprocess.STDOUT, startupinfo=startup)
            try:
                code = child.wait(timeout=90)
            except subprocess.TimeoutExpired:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=10)
                raise RuntimeError(f'{case}: owned viewer timeout')
        text = (runtime / 'debug.txt').read_text(encoding='utf-8', errors='replace')
        if args.operation == 'create':
            expected_stage = 5 if case == 'pending-unload' else (3 if case == 'normal' else 1)
            smoke.require(code == 0 and re.search(fr'\[IMM_LIVE_PAINT_CREATE\] pass=1 stage={expected_stage}\b', text),
                          f'{case}: paint creation lifecycle failed')
        elif case == 'replacement-retirement-failure':
            smoke.require(code == 0 and re.search(r'\[IMM_LIVE_EDIT\].*revision=1 status=5 result=-10 drawingBBoxUnchanged=1', text),
                          'replacement retirement failure must reject before model mutation')
            smoke.require(re.search(r'\[IMM_LIVE_TIMING\] revision=1 status=5 valid=1 .*commitToPresentedMs=-1', text),
                          'rejected replacement must not publish')
            smoke.require(not re.search(r'\[IMM_LIVE_EDIT_FRAME\].*revision=[1-9]', text),
                          'rejected replacement must preserve selected drawing revisions')
        else:
            smoke.require(code == 0 and re.search(r'\[IMM_LIVE_SUBTREE\].*pass=1', text), f'{case}: native probe failed')
        smoke.require('geometryMatch=0' not in text, f'{case}: model/renderer mismatch')
        completed = re.findall(r'\[IMM_LIVE_EDIT_RETIRE\] completedToken=(\d+)', text)
        retired = re.findall(r'renderer delete retiredToken=(\d+)', text)
        cancelled = []
        if args.operation == 'create':
            swaps = re.findall(r'renderer swap activeToken=\d+ retiredToken=(\d+)', text)
            cancelled = re.findall(r'\[IMM_LIVE_EDIT_CANCEL\] completedToken=(\d+)', text)
            smoke.require(len(retired) == (1 if case == 'normal' else 0) and
                len(swaps) == (1 if case == 'normal' else 0) and sorted(retired + swaps) == sorted(completed),
                f'{case}: created paint drawing retirement incomplete')
            smoke.require(len(cancelled) == (1 if case in ('gpu-failure', 'present-failure', 'pending-unload') else 0),
                          f'{case}: prepared slot cancellation incomplete')
        else:
            smoke.require(len(retired) == (5 if case == 'normal' else 0) and sorted(retired) == sorted(completed),
                      f'{case}: every detached drawing must reach renderer retirement')
        capture = (runtime / 'capture.ppm').read_bytes().split(b'\n', 3)
        smoke.require(len(capture) == 4 and capture[0] == b'P6' and capture[2] == b'255', 'RGB capture')
        width, height = map(int, capture[1].split())
        pixels = capture[3]
        smoke.require(len(pixels) == width * height * 3 and
            sum(pixels[i:i+3] != b'\0\0\0' for i in range(0, len(pixels), 3)) >= 16, 'visible surviving paint')
        render = [line for line in text.splitlines() if 'IMM GL validation: frame=' in line]
        smoke.require(render and all(int(re.search(fr'{field}=(\d+)', render[-1])[1]) > 0
            for field in ('paintDrawCalls', 'triangles')), 'positive surviving render')
        report['cases'][case] = {'elapsedSeconds': time.monotonic() - started, 'retiredDrawings': len(retired),
            'completedRetirements': len(completed), 'cancelledPreparedSlots': len(cancelled),
            **runtime_provenance(text, 'DirectX'),
            'logSha256': hashlib.sha256((runtime / 'debug.txt').read_bytes()).hexdigest()}
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    for handle in handles:
        handle.close()
    print(f'[IMM_PAINT_LIFECYCLE_TEST] {args.operation}: lifecycle, renderer ownership and injected rollback passed')


if __name__ == '__main__':
    main()
