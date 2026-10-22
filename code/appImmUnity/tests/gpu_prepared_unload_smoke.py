"""Validate D3D11 uploaded-but-unpublished cancellation using owned viewer cases.

This proves neither in-flight GPU cancellation nor backend allocation leak freedom.
The held capture establishes the old image separately from recovery's changed image.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

from live_edit_benchmark_report import runtime_provenance
from property_edit_benchmark import compare_captures, parse_log, read_ppm


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_gpu_log(text, held_capture):
    if re.search(r'\[IMM_LIVE_GPU_UNLOAD\] pass=0', text) or 'geometryMatch=0' in text:
        raise ValueError('Native uploaded-pending probe reported a failure')
    prepared = list(re.finditer(r'\[IMM_LIVE_GPU_PREPARED\] revision=(\d+) object=(\d+) token=(\d+) '
                                r'points=(\d+) indices=(\d+)', text))
    if len(prepared) != 1:
        raise ValueError('Expected exactly one held GPU preparation, without repeated uploads')
    held = prepared[0]
    revision, drawing, token, points, indices = map(int, held.groups())
    if revision <= 0 or drawing <= 0 or points <= 0 or indices <= 0:
        raise ValueError('Held preparation must contain real uploaded point records and indices')
    queued = re.search(r'\[IMM_LIVE_GPU_UNLOAD\] queued=1 layer=(\d+) revision=(\d+) oldPresented=(\d+)', text)
    if not queued or int(queued[2]) != revision:
        raise ValueError('Held preparation does not match the queued document operation')
    layer, old_presented = int(queued[1]), int(queued[3])
    if old_presented >= revision or queued.start() >= held.start():
        raise ValueError('Held revision must follow the old presented revision and its queue marker')
    frames = re.findall(r'\[IMM_LIVE_GPU_UNLOAD\] stage=3 heldFrames=(\d+) oldState=1 prepared=(\d+) '
                        r'presented=(\d+) layer=(\d+)', text)
    held_frames = [int(row[0]) for row in frames]
    if (len(frames) < 3 or held_frames != list(range(1, len(frames) + 1)) or
            any((int(row[1]), int(row[2]), int(row[3])) != (revision, old_presented, layer) for row in frames)):
        raise ValueError('Missing old-state proof across held rendered frames')
    cancellations = list(re.finditer(r'\[IMM_LIVE_EDIT_CANCEL\] completedToken=(\d+)', text))
    if len(cancellations) != 1 or int(cancellations[0][1]) != token:
        raise ValueError('Held renderer slot must be cancelled exactly once')
    cancelled = cancellations[0]
    if cancelled.start() <= held.end():
        raise ValueError('Cancellation precedes successful held upload')
    # Numeric pool tokens can be reused by the fresh session. Scope the old-slot
    # checks to its upload-through-cancellation interval rather than all logs.
    interval = text[held.end():cancelled.start()]
    if 'renderer swap' in interval or 'renderer delete' in interval:
        raise ValueError('Uploaded old-session replacement was published or retired as presented content')
    trace = re.compile(r'\[IMM_LIVE_EDIT_FRAME\] sample=(\d+) layer=(\d+) revision=(\d+) '
                       r'rendererToken=(\d+) geometryMatch=1 inFrustum=1')
    baseline = [row for row in trace.findall(text[:queued.start()]) if int(row[1]) == layer]
    if not baseline:
        raise ValueError('No selected baseline renderer identity before the edit')
    _, _, baseline_revision, baseline_token = baseline[-1]
    selected = [row for row in trace.findall(interval) if int(row[1]) == layer]
    if (len({row[0] for row in selected}) < 3 or any((row[2], row[3]) != (baseline_revision, baseline_token) for row in selected) or
            int(baseline_token) == token):
        raise ValueError('Held frames failed to retain the original selected renderer geometry')
    if held_capture:
        completion = re.search(r'\[IMM_LIVE_GPU_UNLOAD\] pass=1 stage=3 heldCapture=1 '
                               r'uploadedUnpublished=1 heldFrames=(\d+) stableSibling=1', text)
        if not completion or int(completion[1]) < 3 or 'unloadQueued=1' in text:
            raise ValueError('Held capture did not finish while the replacement remained unpublished')
    else:
        completion = re.search(r'\[IMM_LIVE_GPU_UNLOAD\] pass=1 stage=6 heldCapture=0 '
                               r'uploadedUnpublished=1 sessionInvalid=1 reloaded=1 healthyPresented=1 '
                               r'stableSibling=1 layer=(\d+) revision=(\d+)', text)
        unload = re.search(r'\[IMM_LIVE_GPU_UNLOAD\] unloadQueued=1 revision=(\d+)', text)
        if not completion or not unload or int(unload[1]) != revision or unload.start() >= cancelled.start():
            raise ValueError('Missing explicit unload, invalidated old session or healthy recovery')
        fresh_layer, fresh_revision = int(completion[1]), int(completion[2])
        if not any(int(row[1]) == fresh_layer and int(row[2]) == fresh_revision
                   for row in trace.findall(text[cancelled.end():])):
            raise ValueError('Recovered revision has no selected matching renderer geometry')
    return {'heldRevision': revision, 'heldDrawing': drawing, 'heldToken': token,
            'uploadedPointRecords': points, 'uploadedIndices': indices,
            'heldRenderedFrames': max(int(row[0]) for row in frames),
            'oldSelectedToken': int(baseline_token), 'oldGeometryRetainedUntilCancellation': True,
            'exactlyOneCancellation': True, 'heldCapture': held_capture,
            'evidenceScope': 'uploaded but unpublished renderer resources; no in-flight completion or leak-count claim'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    if os.name != 'nt' or args.timeout <= 0:
        parser.error('Windows standalone DirectX viewer and positive timeout required')
    viewer, fixture = args.viewer.resolve(), args.fixture.resolve()
    for path in (viewer, fixture):
        if not path.is_file():
            raise FileNotFoundError(path)
    manifest_path = fixture.parent / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    info = manifest.get('cases', {}).get('small', {})
    if (manifest.get('workload') != 'property-live-v1' or info.get('fixtureSha256') != sha256(fixture) or
            (info.get('drawings'), info.get('paintLayers'), info.get('pointsPerDrawing')) != (10, 2, 16)):
        parser.error('A verified property-live-v1 Small fixture and manifest are required')
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    template = viewer.parent / 'settings-baseline-directx-sample1.json'
    config_template = json.loads(template.read_text(encoding='utf-8'))
    dependencies = list(viewer.parent.glob('*.dll'))
    report = {'graphicsApi': 'DirectX', 'backend': 'D3D11', 'viewerSha256': sha256(viewer),
              'fixtureSha256': sha256(fixture), 'settingsTemplateSha256': sha256(template),
              'dependencies': [{'name': path.name, 'sha256': sha256(path)} for path in dependencies],
              'scope': 'nonempty GPU-prepared unpublished replacement, old image and reload recovery', 'cases': {}}
    report_path = output / 'report.json'
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    for name in ('control', 'held-capture', 'unload-recovery'):
        runtime = output / name
        runtime.mkdir()
        shutil.copy2(viewer, runtime / viewer.name)
        for dependency in dependencies:
            shutil.copy2(dependency, runtime / dependency.name)
        for resource in viewer.parent.iterdir():
            if resource.is_dir():
                shutil.copytree(resource, runtime / resource.name)
        shutil.copy2(fixture, runtime / 'gpu-unload.imm')
        config = json.loads(json.dumps(config_template))
        config['File']['Load'] = [str(runtime / 'gpu-unload.imm')]
        config['Rendering'].update(RenderingAPI='DirectX', EnableVR=False, RenderingTechnique='Static')
        config['Playback']['PlayerSpawn'].update(Location='Custom', Custom={
            'Rotation': [0, 0, 0, 1], 'Scale': 1, 'Flip': 'N', 'Translation': [2.25, 1.5, 3]})
        settings = runtime / 'settings.json'
        settings.write_text(json.dumps(config, indent=2), encoding='utf-8')
        environment = {key: value for key, value in os.environ.items() if not key.startswith(
            ('IMM_VIEWER_VALIDATE_', 'IMM_GL_VALIDATE_', 'IMM_LIVE_EDIT_', 'IMM_VIEWER_LIVE_', 'IMM_UNITY_SKIP_GPU_LOAD_'))}
        environment.update({'IMM_VIEWER_LIVE_EDIT': '60', 'IMM_LIVE_EDIT_TRACE_FRAMES': '1',
            'IMM_VIEWER_VALIDATE_FRAME': '60', 'IMM_VIEWER_VALIDATE_MAX_FRAME': '100000',
            'IMM_VIEWER_VALIDATE_MIN_NONZERO': '16', 'IMM_VIEWER_VALIDATE_MIN_DRAWCALLS': '1',
            'IMM_VIEWER_VALIDATE_MIN_TRIANGLES': '1', 'IMM_VIEWER_VALIDATE_DISABLE_AUDIO': '1',
            'IMM_VIEWER_VALIDATE_LOCK_CAMERA': '1', 'IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_CAPTURE_PATH': str(runtime / 'capture.ppm')})
        if name == 'control':
            environment.update({'IMM_VIEWER_LIVE_PROPERTY_BENCHMARK': 'transform',
                'IMM_VIEWER_LIVE_PROPERTY_CONTROL': '1', 'IMM_VIEWER_LIVE_BENCHMARK_POINTS': '16',
                'IMM_VIEWER_LIVE_BENCHMARK_SAMPLES': '1', 'IMM_VIEWER_LIVE_BENCHMARK_WARMUP': '0'})
        else:
            environment.update({'IMM_VIEWER_LIVE_GPU_PREPARED_UNLOAD': 'held' if name == 'held-capture' else '1',
                                'IMM_LIVE_EDIT_HOLD_GPU_PREPARED': '1'})
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        started = time.monotonic()
        with (runtime / 'stdout.log').open('wb') as log:
            child = subprocess.Popen([str(runtime / viewer.name), str(settings)], cwd=runtime, env=environment,
                                     stdout=log, stderr=subprocess.STDOUT, startupinfo=startup)
            try:
                code = child.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=10)
                raise RuntimeError(f'{name}: owned viewer timed out')
        if code:
            raise RuntimeError(f'{name}: viewer exited {code}; inspect {runtime}')
        text = (runtime / 'debug.txt').read_text(encoding='utf-8', errors='replace')
        record = {'elapsedSeconds': time.monotonic() - started, 'settingsSha256': sha256(settings)}
        record.update(runtime_provenance(text, 'DirectX'))
        if name == 'control':
            parse_log(runtime / 'debug.txt', 'small', 'transform', 1, 0, True)
        else:
            record.update(validate_gpu_log(text, name == 'held-capture'))
            if name == 'held-capture':
                if read_ppm(output / 'control/capture.ppm') != read_ppm(runtime / 'capture.ppm'):
                    raise ValueError('GPU-prepared held capture differs from the unchanged control image')
                record['oldImageExactlyMatchesControl'] = True
            else:
                record['renderedRecovery'] = compare_captures(
                    output / 'control/capture.ppm', runtime / 'capture.ppm', 'transform')
        report['cases'][name] = record
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('[IMM_GPU_PREPARED_UNLOAD_TEST] control, old held image, cancellation and healthy reload passed (D3D11)')


if __name__ == '__main__':
    main()
