"""Measure native property publication and compare owned control/change captures.

This Windows DirectX probe owns its processes and runtime directories. Publication
timing is distinct from displayed-frame latency, GPU completion and retirement.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import time

from live_edit_benchmark_report import runtime_provenance


PROPERTIES = ('visibility', 'opacity', 'transform')
CASES = {'small': (10, 16), 'medium': (400, 32), 'large': (4000, 64)}
METRICS = ('queueMs', 'commitCallMs', 'commitToPreparingMs',
           'commitToFirstPreparedMs', 'commitToPresentedMs')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_log(path, case, prop, samples, warmup, control=False):
    text = path.read_text(encoding='utf-8', errors='replace')
    if '[IMM_LIVE_PROPERTY_BENCHMARK_ERROR]' in text or 'geometryMatch=0' in text:
        raise ValueError(f'{case}/{prop}: native property probe failed')
    entries = []
    setup_record = re.search(r'\[IMM_LIVE_PROPERTY_SETUP\] property=(\w+) visibilityKeyRemoved=1 '
                             r'overrides=0 pausedFrame=0 paintDrawCalls=(\d+) triangles=(\d+)', text)
    expected_layers = {'small': 2, 'medium': 4, 'large': 8}[case]
    if (not setup_record or setup_record[1] != prop or int(setup_record[2]) != expected_layers or
            int(setup_record[3]) <= 0 or int(setup_record[3]) % expected_layers):
        raise ValueError(f'{case}/{prop}: incomplete canonical setup/render baseline')
    baseline_triangles = int(setup_record[3])
    for line in text.splitlines():
        if '[IMM_LIVE_PROPERTY_BENCHMARK]' not in line:
            continue
        fields = dict(re.findall(r'(\w+)=([^\s]+)', line))
        try:
            entry = {key: int(fields[key]) for key in
                     ('iteration', 'sample', 'warmup', 'points', 'layer', 'revision', 'status', 'control',
                      'effective', 'stableGeometry', 'siblingStable', 'paintDrawCalls', 'triangles', 'changed')}
            entry.update({key: float(fields[key]) for key in METRICS})
        except (KeyError, ValueError) as error:
            raise ValueError(f'{case}/{prop}: incomplete property record') from error
        if (fields.get('property') != prop or entry['control'] != int(control) or
                entry['points'] != CASES[case][1] or entry['status'] != 4 or entry['revision'] <= 0):
            raise ValueError(f'{case}/{prop}: unexpected property operation')
        if any(not math.isfinite(entry[key]) or entry[key] < 0 for key in METRICS):
            raise ValueError(f'{case}/{prop}: invalid timing')
        if not entry['commitToPreparingMs'] <= entry['commitToFirstPreparedMs'] <= entry['commitToPresentedMs']:
            raise ValueError(f'{case}/{prop}: nonmonotonic transitions')
        iteration = len(entries)
        changed = not control and iteration % 2 == 0
        hidden = prop == 'visibility' and changed
        if (any(entry[key] != 1 for key in ('effective', 'stableGeometry', 'siblingStable')) or
                entry['changed'] != int(changed) or entry['paintDrawCalls'] != expected_layers - int(hidden) or
                entry['triangles'] != baseline_triangles - (baseline_triangles // expected_layers if hidden else 0)):
            raise ValueError(f'{case}/{prop}: sample state/render checks failed')
        if (entry['iteration'], entry['sample'], entry['warmup']) != (
                iteration, iteration - warmup, int(iteration < warmup)):
            raise ValueError(f'{case}/{prop}: warmup/sample sequence mismatch')
        if entries and entry['revision'] <= entries[-1]['revision']:
            raise ValueError(f'{case}/{prop}: revision did not advance')
        if entries and entry['layer'] != entries[0]['layer']:
            raise ValueError(f'{case}/{prop}: target changed during measurement')
        entries.append(entry)
    if len(entries) != samples + warmup:
        raise ValueError(f'{case}/{prop}: expected {samples + warmup} records, found {len(entries)}')
    done = [line for line in text.splitlines() if '[IMM_LIVE_PROPERTY_BENCHMARK_DONE]' in line]
    if len(done) != 1 or any(f'{key}=1' not in done[0] for key in
                            ('stableGeometry', 'siblingStable', 'effective')):
        raise ValueError(f'{case}/{prop}: missing correctness completion')
    completion = dict(re.findall(r'(\w+)=([^\s]+)', done[0]))
    expected_completion = {'property': prop, 'control': str(int(control)), 'samples': str(samples),
                           'warmup': str(warmup), 'effect': 'baseline' if control else 'changed'}
    if any(completion.get(key) != value for key, value in expected_completion.items()):
        raise ValueError(f'{case}/{prop}: completion metadata differs from requested run')
    if '[IMM_LIVE_EDIT_SWAP]' in text or '[IMM_LIVE_EDIT_DELETE]' in text:
        raise ValueError(f'{case}/{prop}: property edit replaced geometry')
    renders = [line for line in text.splitlines() if 'IMM GL validation: frame=' in line]
    if not renders or any(not re.search(fr'{key}=([1-9]\d*)\b', renders[-1])
                          for key in ('paintDrawCalls', 'triangles')):
        raise ValueError(f'{case}/{prop}: missing rendered paint')
    setup = text.split('[IMM_LIVE_PROPERTY_SETUP]')
    if len(setup) != 2:
        raise ValueError(f'{case}/{prop}: missing unique property setup')
    trace_pattern = (r'\[IMM_LIVE_EDIT_FRAME\] sample=\d+ layer=(\d+) revision=(\d+) '
                     r'rendererToken=(\d+) geometryMatch=([01]) inFrustum=([01])')
    baseline = {}
    for layer, revision, token, match, in_frustum in re.findall(trace_pattern, setup[0]):
        baseline[int(layer)] = (int(revision), int(token), match, in_frustum)
    if len(baseline) != expected_layers or any(value[0] != 0 or value[2:] != ('1', '1')
                                                        for value in baseline.values()):
        raise ValueError(f'{case}/{prop}: missing baseline renderer-slot evidence')
    observed = set()
    for layer, revision, token, match, in_frustum in re.findall(trace_pattern, setup[1]):
        layer = int(layer)
        if (int(revision), int(token), match, in_frustum) != baseline.get(layer):
            raise ValueError(f'{case}/{prop}: selected drawing/renderer slot changed')
        observed.add(layer)
    target = entries[0]['layer']
    if target not in baseline or not (set(baseline) - {target}).issubset(observed):
        raise ValueError(f'{case}/{prop}: missing immutable sibling renderer evidence')
    if (prop != 'visibility' or control or samples + warmup > 1) and target not in observed:
        raise ValueError(f'{case}/{prop}: missing visible target renderer evidence')
    corpus = re.search(r'\[IMM_LIVE_PROPERTY_CORPUS\] property=\w+ paintLayers=(\d+) '
                       r'drawings=(\d+) frames=(\d+) loadedStrokes=(\d+) points=(\d+)', text)
    if not corpus or tuple(map(int, corpus.groups())) != (
            expected_layers, CASES[case][0], {'small': 24, 'medium': 480, 'large': 4000}[case],
            CASES[case][0], CASES[case][1]):
        raise ValueError(f'{case}/{prop}: workload was not fully resident')
    return entries[warmup:]


def read_ppm(path):
    parts = path.read_bytes().split(b'\n', 3)
    if len(parts) != 4 or parts[0] != b'P6' or parts[2] != b'255':
        raise ValueError(f'{path.name}: expected native RGB capture')
    width, height = map(int, parts[1].split())
    if width <= 0 or height <= 0 or len(parts[3]) != width * height * 3:
        raise ValueError(f'{path.name}: malformed capture')
    return width, height, parts[3]


def compare_captures(control_path, changed_path, prop):
    width, height, before = read_ppm(control_path)
    changed_width, changed_height, after = read_ppm(changed_path)
    if (width, height) != (changed_width, changed_height):
        raise ValueError('Capture dimensions differ')
    def masks(pixels):
        red, blue = [], []
        for index in range(width * height):
            r, g, b = pixels[index * 3:index * 3 + 3]
            if r > 20 and r > 1.5 * g and r > 1.5 * b:
                red.append(index)
            if b > 20 and b > 1.5 * r and b > 1.5 * g:
                blue.append(index)
        return red, blue
    before_red, before_blue = masks(before)
    after_red, after_blue = masks(after)
    if len(before_red) < 16 or len(before_blue) < 16:
        raise ValueError('Control must contain distinct rendered target and sibling')
    blue = set(before_blue) | set(after_blue)
    if any(before[index * 3:index * 3 + 3] != after[index * 3:index * 3 + 3] for index in blue):
        raise ValueError('Immutable sibling pixels changed')
    def centroid(indices):
        return [sum(index % width for index in indices) / len(indices),
                sum(index // width for index in indices) / len(indices)]
    def energy(pixels, indices):
        return sum(sum(pixels[index * 3:index * 3 + 3]) for index in indices)
    result = {'controlTargetPixels': len(before_red), 'changedTargetPixels': len(after_red),
              'siblingPixels': len(before_blue), 'siblingPixelsStable': True,
              'controlCaptureSha256': sha256(control_path), 'changedCaptureSha256': sha256(changed_path)}
    if prop == 'visibility':
        if after_red:
            raise ValueError('Hidden target still renders red pixels')
    else:
        if len(after_red) < 16:
            raise ValueError('Changed target disappeared')
        if prop == 'opacity':
            before_energy, after_energy = energy(before, before_red), energy(after, after_red)
            if after_energy >= before_energy * .98:
                raise ValueError('Opacity edit did not reduce target pixel energy')
            result.update(controlTargetEnergy=before_energy, changedTargetEnergy=after_energy)
        elif prop == 'transform':
            old_center, new_center = centroid(before_red), centroid(after_red)
            distance = math.dist(old_center, new_center)
            if distance < 2:
                raise ValueError('Transform edit did not move rendered target')
            result.update(controlCentroid=old_center, changedCentroid=new_center, displacementPixels=distance)
        else:
            raise ValueError(f'Unknown property: {prop}')
    return result


def summarize(entries):
    result = {}
    for metric in METRICS:
        values = sorted(entry[metric] for entry in entries)
        result[metric] = {'median': statistics.median(values),
                          'p95': values[math.ceil(len(values) * .95) - 1],
                          'min': values[0], 'max': values[-1],
                          'samples': [entry[metric] for entry in entries]}
    result['nativePublicationTargetMs'] = 2
    result['nativePublicationP95MeetsTarget'] = result['commitToPresentedMs']['p95'] < 2
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--settings', type=Path)
    parser.add_argument('--cases', default='small,medium,large')
    parser.add_argument('--properties', default=','.join(PROPERTIES))
    parser.add_argument('--samples', type=int, default=30)
    parser.add_argument('--warmup', type=int, default=5)
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    cases, properties = args.cases.split(','), args.properties.split(',')
    if os.name != 'nt' or not 1 <= args.samples <= 1000 or not 0 <= args.warmup <= 1000 or args.timeout <= 0:
        parser.error('Windows and bounded sample/warmup/timeout values are required')
    if any(case not in CASES for case in cases) or len(set(cases)) != len(cases):
        parser.error('cases must be unique small/medium/large names')
    if any(prop not in PROPERTIES for prop in properties) or len(set(properties)) != len(properties):
        parser.error('properties must be unique visibility/opacity/transform names')
    if (args.samples + args.warmup) % 2 != 1:
        parser.error('samples + warmup must be odd so final capture contains the changed state')
    viewer = args.viewer.resolve()
    fixtures = args.fixtures.resolve()
    manifest = json.loads((fixtures / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('workload') != 'property-live-v1':
        parser.error('A property-live-v1 fixture manifest is required')
    for case in cases:
        info = manifest['cases'][case]
        if (info['fixtureSha256'] != sha256(fixtures / f'{case}.imm') or
                (info['drawings'], info['pointsPerDrawing']) != CASES[case]):
            parser.error(f'{case}: fixture provenance/inventory mismatch')
    template = args.settings or viewer.parent / 'settings-baseline-directx-sample1.json'
    settings = json.loads(template.read_text(encoding='utf-8'))
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {'workload': 'property-live-v1', 'graphicsApi': 'DirectX',
              'endpoint': 'native revision publication; excludes displayed-frame/GPU-completion latency',
              'viewerSha256': sha256(viewer), 'fixtureManifest': manifest,
              'settingsTemplateSha256': sha256(template), 'dependencies': [], 'cases': {}}
    dependencies = list(viewer.parent.glob('*.dll'))
    report['dependencies'] = [{'name': path.name, 'sha256': sha256(path)} for path in dependencies]
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    for case in cases:
        report['cases'][case] = {}
        for prop in properties:
            for control in (True, False):
                runtime = output / case / prop / ('control' if control else 'changed')
                runtime.mkdir(parents=True)
                shutil.copy2(viewer, runtime / viewer.name)
                for dependency in dependencies:
                    shutil.copy2(dependency, runtime / dependency.name)
                for resource in viewer.parent.iterdir():
                    if resource.is_dir():
                        shutil.copytree(resource, runtime / resource.name)
                config = json.loads(json.dumps(settings))
                config['File']['Load'] = [str(fixtures / f'{case}.imm')]
                config['Rendering'].update(RenderingAPI='DirectX', EnableVR=False, RenderingTechnique='Static')
                config['Playback']['PlayerSpawn'].update(Location='Custom', Custom={
                    'Rotation': [0, 0, 0, 1], 'Scale': 1, 'Flip': 'N', 'Translation': [2.25, 1.5, 3]})
                config_path = runtime / 'settings.json'
                config_path.write_text(json.dumps(config, indent=2), encoding='utf-8')
                environment = {key: value for key, value in os.environ.items() if not key.startswith(
                    ('IMM_VIEWER_VALIDATE_', 'IMM_GL_VALIDATE_', 'IMM_LIVE_EDIT_', 'IMM_VIEWER_LIVE_', 'IMM_UNITY_SKIP_GPU_LOAD_'))}
                environment.update({'IMM_VIEWER_LIVE_EDIT': '60',
                    'IMM_VIEWER_LIVE_PROPERTY_BENCHMARK': prop, 'IMM_VIEWER_LIVE_PROPERTY_CONTROL': str(int(control)),
                    'IMM_VIEWER_LIVE_BENCHMARK_POINTS': str(CASES[case][1]),
                    'IMM_VIEWER_LIVE_BENCHMARK_SAMPLES': str(1 if control else args.samples),
                    'IMM_VIEWER_LIVE_BENCHMARK_WARMUP': str(0 if control else args.warmup),
                    'IMM_LIVE_EDIT_TRACE_FRAMES': '1', 'IMM_VIEWER_VALIDATE_FRAME': '60',
                    'IMM_VIEWER_VALIDATE_MAX_FRAME': '100000', 'IMM_VIEWER_VALIDATE_MIN_NONZERO': '16',
                    'IMM_VIEWER_VALIDATE_MIN_DRAWCALLS': '1', 'IMM_VIEWER_VALIDATE_MIN_TRIANGLES': '1',
                    'IMM_VIEWER_VALIDATE_DISABLE_AUDIO': '1', 'IMM_VIEWER_VALIDATE_LOCK_CAMERA': '1',
                    'IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS': '0',
                    'IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS': '0',
                    'IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS': '0',
                    'IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS': '0',
                    'IMM_VIEWER_VALIDATE_CAPTURE_PATH': str(runtime / 'capture.ppm')})
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = subprocess.SW_HIDE
                started = time.monotonic()
                with (runtime / 'stdout.log').open('wb') as log:
                    child = subprocess.Popen([str(runtime / viewer.name), str(config_path)], cwd=runtime,
                                             env=environment, stdout=log, stderr=subprocess.STDOUT, startupinfo=startup)
                    try:
                        code = child.wait(timeout=args.timeout)
                    except subprocess.TimeoutExpired:
                        child.terminate()
                        try:
                            child.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            child.kill()
                            child.wait(timeout=10)
                        raise RuntimeError(f'{case}/{prop}: owned child timed out')
                if code:
                    raise RuntimeError(f'{case}/{prop}: viewer exit {code}; inspect {runtime}')
                entries = parse_log(runtime / 'debug.txt', case, prop,
                                    1 if control else args.samples, 0 if control else args.warmup, control)
                record = {'elapsedSeconds': time.monotonic() - started, 'settingsSha256': sha256(config_path)}
                record.update(runtime_provenance((runtime / 'debug.txt').read_text(
                    encoding='utf-8', errors='replace'), 'DirectX'))
                if not control:
                    record.update(summarize(entries))
                    record['renderedEffect'] = compare_captures(
                        runtime.parent / 'control/capture.ppm', runtime / 'capture.ppm', prop)
                report['cases'][case][f'{prop}-control' if control else prop] = record
                (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'Property benchmark passed: {len(cases)} sizes x {len(properties)} properties; report retained')


if __name__ == '__main__':
    main()
