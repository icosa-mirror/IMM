"""Exercise live static-paint Loop/Offset selection in the real Windows viewer."""
from pathlib import Path
import argparse
import ctypes as c
import hashlib
import json
import os
import re
import shutil
import subprocess

import exporter_bridge_smoke as smoke
from exporter_benchmark import Exporter, Transform
from live_edit_benchmark_report import runtime_provenance

ROOT = Path(__file__).resolve().parents[3]


def fixture(plugin, output):
    exporter = Exporter(c.CDLL(str(plugin)))
    identity = Transform(0, 0, 0, 0, 0, 0, 1, 1)
    sequence = smoke.require(exporter.create_sequence(1, 0, 0, 0, 0, 30, 0, 0, 0, 0), 'sequence')
    try:
        for name, x, counts, color in ((b'KeepPaint', -1, (16, 32), (1, .1, .1)),
                                        (b'BlueSibling', 1, (16,), (.1, .2, 1))):
            paint = smoke.require(exporter.create_paint_layer(sequence, None, name, 1, 1,
                c.byref(identity), c.byref(identity), 0, 0, 1), 'paint')
            for drawing_index, count in enumerate(counts):
                drawing = smoke.require(exporter.create_drawing(paint), 'drawing')
                try:
                    smoke.require(exporter.drawing_init(drawing, 1, 0), 'drawing init')
                    element = smoke.require(exporter.drawing_get_element(drawing, 0), 'element')
                    smoke.require(exporter.element_init(element, count, 2, 1), 'Circle/Always')
                    points = []
                    for index in range(count):
                        t = index / (count - 1)
                        points.append(smoke.Point(x + .5*t + .35*drawing_index, 1.5, 0,
                            0, 1, 0, 0, 0, 1, *color, 1, .03, t, t))
                    buffer = (smoke.Point * count)(*points)
                    smoke.require(exporter.element_set_points(element, 0, buffer, count), 'points')
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
    parser.add_argument('--canonical-repeat', action='store_true', help='Validate canonical repeat edits instead of Loop/Offset keys.')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows D3D11 viewer required')
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    viewer, plugin = args.viewer.resolve(), args.plugin.resolve()
    handles = [os.add_dll_directory(str(path.parent)) for path in smoke.runtime_dependencies(ROOT)]
    source = output / 'playback-keys.imm'
    fixture(plugin, source)
    report = {'passed': False, 'scope': 'static paint selection and stable selected renderer resources; no in-flight or complete upload accounting claim',
              'viewerSha256': hashlib.sha256(viewer.read_bytes()).hexdigest(),
              'pluginSha256': hashlib.sha256(plugin.read_bytes()).hexdigest(),
              'fixtureSha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'cases': {}}
    report_path = output / 'report.json'
    report['operation'] = 'canonical static-paint repeat' if args.canonical_repeat else 'Loop/Offset keys'
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    template = json.loads((viewer.parent / 'settings-baseline-directx-sample1.json').read_text(encoding='utf-8'))
    for case in ('normal', 'cpu-failure', 'gpu-flag', 'present-failure', 'queued-unload', 'prepared-unload'):
        runtime = output / case
        runtime.mkdir()
        shutil.copy2(source, runtime / source.name)
        owned = runtime / viewer.name
        shutil.copy2(viewer, owned)
        for path in viewer.parent.iterdir():
            if path.is_dir():
                shutil.copytree(path, runtime / path.name)
            elif path.suffix.lower() == '.dll':
                shutil.copy2(path, runtime / path.name)
        settings = json.loads(json.dumps(template))
        settings['File']['Load'] = [str(runtime / source.name)]
        settings['Rendering'].update({'RenderingAPI': 'DirectX', 'EnableVR': False, 'RenderingTechnique': 'Static'})
        settings['Playback']['PlayerSpawn']['Location'] = 'Custom'
        settings['Playback']['PlayerSpawn']['Custom'] = {'Rotation': [0, 0, 0, 1], 'Scale': 1,
            'Flip': 'N', 'Translation': [.25, 1.5, 4]}
        settings_path = runtime / 'settings.json'
        settings_path.write_text(json.dumps(settings, indent=2), encoding='utf-8')
        env = {key: value for key, value in os.environ.items() if not key.startswith(
            ('IMM_VIEWER_VALIDATE_', 'IMM_GL_VALIDATE_', 'IMM_LIVE_EDIT_', 'IMM_VIEWER_LIVE_', 'IMM_UNITY_SKIP_GPU_LOAD_'))}
        env.update({'IMM_VIEWER_LIVE_EDIT': '60', 'IMM_VIEWER_LIVE_PAINT_PLAYBACK_KEYS': '1',
            'IMM_LIVE_EDIT_TRACE_FRAMES': '1', 'IMM_VIEWER_VALIDATE_FRAME': '60',
            'IMM_VIEWER_VALIDATE_MAX_FRAME': '100000', 'IMM_VIEWER_VALIDATE_MIN_NONZERO': '16',
            'IMM_VIEWER_VALIDATE_MIN_DRAWCALLS': '1', 'IMM_VIEWER_VALIDATE_MIN_TRIANGLES': '1',
            'IMM_VIEWER_VALIDATE_DISABLE_AUDIO': '1', 'IMM_VIEWER_VALIDATE_LOCK_CAMERA': '1',
            'IMM_VIEWER_VALIDATE_CAPTURE_PATH': str(runtime / 'capture.ppm')})
        if args.canonical_repeat:
            env['IMM_VIEWER_LIVE_PAINT_CANONICAL_REPEAT'] = '1'
        for suffix in ('PICTURE', 'PICTURE360', 'PICTURE360_EQUIRECT', 'PICTURE360_CUBEMAP'):
            env[f'IMM_VIEWER_VALIDATE_MIN_{suffix}_DRAWCALLS'] = '0'
        if case in ('cpu-failure', 'gpu-flag', 'present-failure'):
            env[{'cpu-failure': 'IMM_LIVE_EDIT_FAIL_PREPARE_CPU', 'gpu-flag': 'IMM_LIVE_EDIT_FAIL_PREPARE_GPU',
                 'present-failure': 'IMM_LIVE_EDIT_FAIL_PRESENT'}[case]] = '1'
        if case.endswith('-unload'):
            env['IMM_VIEWER_LIVE_PAINT_PLAYBACK_KEYS_UNLOAD'] = case.split('-')[0]
        if case == 'prepared-unload':
            env['IMM_LIVE_EDIT_HOLD_PAINT_PLAYBACK_KEYS'] = '1'
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        with (runtime / 'stdout.log').open('wb') as log:
            child = subprocess.Popen([str(owned), str(settings_path)], cwd=runtime, env=env,
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
        smoke.require(code == 0 and '[IMM_LIVE_PAINT_PLAYBACK_KEYS] pass=1' in text, f'{case}: native probe failed')
        rejected = case in ('cpu-failure', 'present-failure')
        samples = re.findall(r'\[IMM_LIVE_PAINT_PLAYBACK_KEYS_SAMPLE\] step=(\d+) revision=(\d+) selected=(\d+)', text)
        if rejected:
            smoke.require('rejected=1 result=-10 unchanged=1' in text and not samples, f'{case}: rollback')
        else:
            expected = [0, 1, 0, 0, 1, 0, 0] if args.canonical_repeat else [0, 1, 0, 1, 0, 0, 0]
            smoke.require([int(row[0]) for row in samples] == list(range(7)) and
                [int(row[2]) for row in samples] == expected, f'{case}: selected transitions')
            if args.canonical_repeat:
                smoke.require('[IMM_LIVE_PAINT_REPEAT] pass=1' in text and
                    [int(value) for value in re.findall(r'\[IMM_LIVE_PAINT_REPEAT_SAMPLE\] step=\d+ canonical=(\d+)', text)] ==
                    [0, 1, 3, 4294967295, 1, 0], f'{case}: canonical repeat getter')
            smoke.require('playingBoth=1 stableGeometry=1 siblingStable=1' in text, f'{case}: playing/sibling proof')
        if case.endswith('-unload'):
            smoke.require('sessionInvalid=1 beforeReload=1' in text and 'reloaded=1' in text, f'{case}: unload/recovery')
        smoke.require(not re.search(r'renderer swap|renderer delete|\[IMM_LIVE_EDIT_CANCEL\]|geometryMatch=0', text),
                      f'{case}: unexpected renderer ownership or geometry change')
        traces = re.findall(r'\[IMM_LIVE_EDIT_FRAME\] sample=(\d+) layer=(\d+) revision=(\d+) rendererToken=(\d+) geometryMatch=1 inFrustum=1', text)
        smoke.require(traces and all(int(row[2]) == 0 for row in traces), f'{case}: selected geometry revision changed')
        # Reload can reuse raw layer IDs and renderer token numbers. Verify
        # identity only within the freshly loaded session containing the edits.
        session = text.split('[IMM_LIVE_PAINT_PLAYBACK_KEYS_UNLOAD] sessionInvalid=1 beforeReload=1')[-1]
        selected_tokens = {}
        last_tokens = {}
        layer_tokens = {}
        target_layer = None
        for line in session.splitlines():
            trace = re.search(r'\[IMM_LIVE_EDIT_FRAME\].*layer=(\d+) revision=0 rendererToken=(\d+) geometryMatch=1 inFrustum=1', line)
            if trace:
                layer, token = map(int, trace.groups())
                last_tokens[layer] = token
                layer_tokens.setdefault(layer, set()).add(token)
            sample = re.search(r'\[IMM_LIVE_PAINT_PLAYBACK_KEYS_SAMPLE\] step=[0-5] revision=\d+ selected=(\d+) layer=(\d+)', line)
            if sample:
                selected, target_layer = map(int, sample.groups())
                smoke.require(target_layer in last_tokens, f'{case}: missing selected render trace')
                token = last_tokens[target_layer]
                smoke.require(selected not in selected_tokens or selected_tokens[selected] == token,
                              f'{case}: selected drawing renderer token changed')
                selected_tokens[selected] = token
        if not rejected:
            smoke.require(len(selected_tokens) == 2 and len(set(selected_tokens.values())) == 2,
                          f'{case}: distinct A/B renderer selections')
            smoke.require(all(len(tokens) == (2 if layer == target_layer else 1)
                              for layer, tokens in layer_tokens.items()) and len(layer_tokens) == 2,
                          f'{case}: stable sibling/target renderer identities')
        ppm = (runtime / 'capture.ppm').read_bytes().split(b'\n', 3)
        smoke.require(len(ppm) == 4 and ppm[0] == b'P6' and ppm[2] == b'255', f'{case}: capture')
        width, height = map(int, ppm[1].split())
        rgb = ppm[3]
        smoke.require(len(rgb) == width*height*3, f'{case}: RGB length')
        colors = [rgb[index:index+3] for index in range(0, len(rgb), 3)]
        smoke.require(sum(r > 2*b and r > 2*g and r > 16 for r, g, b in colors) >= 16 and
                      sum(b > 2*r and b > 2*g and b > 16 for r, g, b in colors) >= 16, f'{case}: visible red target/blue sibling')
        report['cases'][case] = {'samples': len(samples), 'traceSamples': len(traces), 'rejected': rejected,
            'captureSha256': hashlib.sha256((runtime / 'capture.ppm').read_bytes()).hexdigest(),
            **runtime_provenance(text, 'DirectX')}
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    report['passed'] = True
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    for handle in handles:
        handle.close()
    print('[IMM_PAINT_PLAYBACK_KEYS_TEST] six rendered publication/rollback/unload cases passed')


if __name__ == '__main__':
    main()
