"""Capture actual timed-stroke DrawIn rendering in an isolated Windows GL viewer."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import exporter_bridge_smoke as smoke
from exporter_benchmark import Exporter
from live_edit_benchmark_report import runtime_provenance
from property_edit_benchmark import read_ppm

ROOT = Path(__file__).resolve().parents[3]
VALUES = {'keyless': None, 'zero': 0.0, 'partial': 0.4, 'full': 1.0}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rendering_errors(text):
    pattern = re.compile(r'Renderer Error|GL_(?:INVALID_[A-Z_]+|OUT_OF_MEMORY|STACK_(?:OVERFLOW|UNDERFLOW))|'
        r'(?:shader|program)[^\r\n]*(?:compil|link)[^\r\n]*(?:failed|error)|'
        r'(?:shader|program)[^\r\n]*(?:failed|error)[^\r\n]*(?:compil|link)', re.IGNORECASE)
    lines = [line for line in text.splitlines() if pattern.search(line)]
    return {'count': len(lines), 'examples': list(dict.fromkeys(lines))[:4]}


def generate_fixture(exporter, add_key, path, value):
    identity = smoke.Transform(0, 0, 0, 0, 0, 0, 1, 1)
    sequence = smoke.require(exporter.create_sequence(1, 0, 0, 0, 0, 30, 0, 0, 0, 0), 'draw-in sequence')
    try:
        for target in (True, False):
            layer = smoke.require(exporter.create_paint_layer(sequence, None,
                b'TimedRedTarget' if target else b'BlueReference', 1, 1, ctypes.byref(identity),
                ctypes.byref(identity), 0, 0, 0), 'draw-in paint')
            if target and value is not None:
                smoke.require(add_key(layer, 5, 0, 0, 0, 0, 0, value, ctypes.byref(identity)), 'DrawIn key')
            drawing = smoke.require(exporter.create_drawing(layer), 'draw-in drawing')
            try:
                smoke.require(exporter.drawing_init(drawing, 1, 0), 'draw-in drawing init')
                element = smoke.require(exporter.drawing_get_element(drawing, 0), 'draw-in element')
                smoke.require(exporter.element_init(element, 64, 2, 1), 'Circle/Always stroke')
                points = []
                for index in range(64):
                    t = index / 63
                    color = (1, .1, .1) if target else (.1, .2, 1)
                    points.append(smoke.Point(1.2 + 2.1 * t, 1.7 if target else 1.2, 0,
                        0, 1, 0, 0, 0, 1, *color, 1, .045, t, .1 + .8 * t))
                buffer = (smoke.Point * len(points))(*points)
                smoke.require(exporter.element_set_points(element, 0, buffer, len(buffer)), 'timed points')
                exporter.compute_element_bounds(element)
                exporter.compute_drawing_bounds(drawing)
                exporter.paint_add_frame(layer, exporter.get_drawing_index(drawing))
            finally:
                exporter.destroy_drawing(drawing)
        smoke.require(exporter.export_to_file(sequence, os.fsencode(path), 96000, 0), 'draw-in export')
    finally:
        exporter.destroy_sequence(sequence)


def capture(viewer, dependencies, template, fixture, runtime, technique, timeout):
    runtime.mkdir()
    shutil.copy2(viewer, runtime / viewer.name)
    for dependency in dependencies:
        shutil.copy2(dependency, runtime / dependency.name)
    config = json.loads(json.dumps(template))
    config['File']['Load'] = [str(fixture)]
    config['Rendering'].update(RenderingAPI='OpenGL', EnableVR=False, RenderingTechnique=technique)
    config['Playback']['PlayerSpawn'].update(Location='Custom', Custom={
        'Rotation': [0, 0, 0, 1], 'Scale': 1, 'Flip': 'N', 'Translation': [2.25, 1.45, 3]})
    settings = runtime / 'settings.json'
    settings.write_text(json.dumps(config, indent=2), encoding='utf-8')
    environment = {key: value for key, value in os.environ.items() if not key.startswith(
        ('IMM_VIEWER_VALIDATE_', 'IMM_GL_VALIDATE_', 'IMM_VIEWER_LIVE_', 'IMM_LIVE_EDIT_', 'IMM_UNITY_SKIP_GPU_LOAD_'))}
    environment.update({'IMM_VIEWER_VALIDATE_FRAME': '90', 'IMM_VIEWER_VALIDATE_MAX_FRAME': '20000',
        'IMM_VIEWER_VALIDATE_MIN_NONZERO': '100', 'IMM_VIEWER_VALIDATE_MIN_DRAWCALLS': '2',
        'IMM_VIEWER_VALIDATE_MIN_TRIANGLES': '1', 'IMM_VIEWER_VALIDATE_DISABLE_AUDIO': '1',
        'IMM_VIEWER_VALIDATE_LOCK_CAMERA': '1', 'IMM_VIEWER_VALIDATE_CAPTURE_PATH': str(runtime / 'capture.ppm'),
        'IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS': '0', 'IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS': '0',
        'IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS': '0',
        'IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS': '0'})
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    with (runtime / 'stdout.log').open('wb') as log:
        child = subprocess.Popen([str(runtime / viewer.name), str(settings)], cwd=runtime,
            env=environment, stdout=log, stderr=subprocess.STDOUT, startupinfo=startup)
        try:
            code = child.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            child.terminate()
            child.wait(timeout=10)
            raise RuntimeError(f'{runtime.name}: owned viewer timed out')
    if code:
        raise RuntimeError(f'{runtime.name}: viewer exited {code}')
    text = (runtime / 'debug.txt').read_text(encoding='utf-8', errors='replace')
    diagnostics = text + '\n' + (runtime / 'stdout.log').read_text(encoding='utf-8', errors='replace')
    if 'IMM GL validation failed:' in text or not re.search(r'IMM GL validation:.*paintDrawCalls=2\b', text):
        raise ValueError(f'{runtime.name}: missing successful two-paint rendering validation')
    return {'fixtureSha256': digest(fixture), 'settingsSha256': digest(settings),
        'renderingErrors': rendering_errors(diagnostics),
        **runtime_provenance(text, 'OpenGL')}


def evaluate(captures):
    images = {name: read_ppm(path) for name, path in captures.items()}
    if len({(image[0], image[1]) for image in images.values()}) != 1:
        raise ValueError('Capture dimensions differ')
    masks = {}
    for name, (_, _, pixels) in images.items():
        masks[name] = {i for i in range(0, len(pixels), 3)
            if pixels[i] > 48 and pixels[i] > pixels[i + 1] * 1.5 and pixels[i] > pixels[i + 2] * 1.5}
    full = images['full'][2]
    blue = {i for i in range(0, len(full), 3)
        if full[i + 2] > 48 and full[i + 2] > full[i] * 1.5 and full[i + 2] > full[i + 1] * 1.5}
    counts = {name: len(mask) for name, mask in masks.items()}
    checks = {'referenceVisible': len(blue) >= 100, 'targetVisible': counts['full'] >= 100,
        'zeroHidden': counts['zero'] == 0, 'partialCoverage': 0 < counts['partial'] < counts['full'],
        'keylessEqualsFull': images['keyless'] == images['full'],
        'referenceStable': all(all(pixels[i:i + 3] == full[i:i + 3] for i in blue)
            for _, _, pixels in images.values())}
    return {'passed': all(checks.values()), 'checks': checks, 'redPixels': counts, 'referencePixels': len(blue)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--plugin', type=Path, default=ROOT / 'code/appImmUnity/exe/ImmUnityPlugin.dll')
    parser.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    if os.name != 'nt' or args.timeout <= 0:
        parser.error('Windows GL viewer and positive timeout required')
    viewer, plugin, output = args.viewer.resolve(), args.plugin.resolve(), args.output_dir.resolve()
    for source in (viewer, plugin):
        if not source.is_file():
            raise FileNotFoundError(source)
    output.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'scope': 'Actual Windows OpenGL timed-stroke pixels; no Android/device or Unity claim',
        'viewerSha256': digest(viewer), 'exporterSha256': digest(plugin), 'cases': {}, 'techniques': {}}
    report_path = output / 'report.json'
    dependencies = sorted(viewer.parent.glob('*.dll'))
    report['runtimeDependencies'] = [{'name': path.name, 'sha256': digest(path)} for path in dependencies]
    template_path = ROOT / 'code/appImmViewer/exe/settings-baseline-opengl-sample1.json'
    template = json.loads(template_path.read_text(encoding='utf-8'))
    report['templateSha256'] = digest(template_path)
    exporter_runtime = output / 'exporter-runtime'
    exporter_runtime.mkdir()
    for dependency in (*plugin.parent.glob('*.dll'), *smoke.runtime_dependencies(ROOT)):
        shutil.copy2(dependency, exporter_runtime / dependency.name)
    try:
        with os.add_dll_directory(str(exporter_runtime)):
            library = ctypes.CDLL(str(exporter_runtime / plugin.name))
            exporter = Exporter(library)
            add_key = smoke.bind(library, 'ImmExporter_LayerAddAnimationKey', ctypes.c_bool,
                ctypes.c_void_p, ctypes.c_int, ctypes.c_int64, ctypes.c_int, ctypes.c_int,
                ctypes.c_uint32, ctypes.c_float, ctypes.c_double, ctypes.POINTER(smoke.Transform))
            fixtures = {}
            for name, value in VALUES.items():
                fixtures[name] = output / f'{name}.imm'
                generate_fixture(exporter, add_key, fixtures[name], value)
        for technique in ('Static', 'Pretessellated'):
            captures = {}
            for name, fixture in fixtures.items():
                runtime = output / f'{technique.lower()}-{name}'
                report['cases'][runtime.name] = capture(viewer, dependencies, template, fixture, runtime, technique, args.timeout)
                captures[name] = runtime / 'capture.ppm'
            result = evaluate(captures)
            result['checks']['renderingErrorFree'] = all(report['cases'][f'{technique.lower()}-{name}']['renderingErrors']['count'] == 0
                for name in VALUES)
            result['passed'] = all(result['checks'].values())
            report['techniques'][technique] = result
        report['passed'] = all(item['passed'] for item in report['techniques'].values())
    except Exception as error:
        report['error'] = str(error)
    finally:
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f"[IMM_DRAW_IN_RENDER] passed={int(report['passed'])} techniques={len(report['techniques'])} cases={len(report['cases'])}")
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
