"""Run owned Windows standalone publication benchmarks with isolated runtime/logs.

Generate --live-workload baseline fixtures first. This never controls an existing
viewer or Editor. On timeout it terminates only the child created by this runner.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from live_edit_benchmark_report import CASES, parse_log


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--graphics-api', choices=['DirectX'], default='DirectX')
    parser.add_argument('--settings', type=Path)
    parser.add_argument('--samples', type=int, default=30)
    parser.add_argument('--warmup', type=int, default=5)
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--cases', default='small,medium,large')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('The viewer benchmark runner requires Windows.')
    if not 1 <= args.samples <= 1000 or not 0 <= args.warmup <= 1000 or args.timeout <= 0:
        parser.error('samples/warmup must fit native limits and timeout must be positive')
    viewer = args.viewer.resolve()
    selected = args.cases.split(',')
    if not selected or len(set(selected)) != len(selected) or any(case not in CASES for case in selected):
        parser.error('cases must be unique small/medium/large names')
    settings_path = (args.settings or viewer.parent / 'settings-baseline-directx-sample1.json').resolve()
    baseline = json.loads(args.baseline.read_text(encoding='utf-8'))
    for case in selected:
        fixture = args.fixtures / f'{case}.imm'
        if baseline[case].get('workload') != 'matched-live-first-drawing-v2' or not fixture.is_file():
            parser.error(f'{case}: matched baseline fixture is required')
        if baseline[case]['fixtureSha256'] != hashlib.sha256(fixture.read_bytes()).hexdigest():
            parser.error(f'{case}: fixture hash differs from baseline')
    output = args.output_dir.resolve()
    # A fresh directory prevents stale completion/capture files from passing a run.
    output.mkdir(parents=True, exist_ok=False)
    runtime = output / 'runtime'
    runtime.mkdir()
    staged_viewer = runtime / viewer.name
    shutil.copy2(viewer, staged_viewer)
    dependencies = []
    for dependency in viewer.parent.glob('*.dll'):
        target = runtime / dependency.name
        shutil.copy2(dependency, target)
        dependencies.append({'name': target.name, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
    for resource in viewer.parent.iterdir():
        if resource.is_dir():
            shutil.copytree(resource, runtime / resource.name)
    settings_template = json.loads(settings_path.read_text(encoding='utf-8'))
    report_command = [sys.executable, str(Path(__file__).with_name('live_edit_benchmark_report.py')),
        '--viewer', str(staged_viewer), '--baseline', str(args.baseline.resolve()),
        '--fixtures', str(args.fixtures.resolve()), '--graphics-api', args.graphics_api,
        '--samples', str(args.samples), '--warmup', str(args.warmup), '--output', str(output / 'report.json')]
    provenance = {'viewerSha256': hashlib.sha256(staged_viewer.read_bytes()).hexdigest(), 'dependencies': dependencies,
        'settingsTemplateSha256': hashlib.sha256(settings_path.read_bytes()).hexdigest(), 'cases': {}}
    (output / 'provenance.json').write_text(json.dumps(provenance, indent=2), encoding='utf-8')
    for case in selected:
        _, points, _ = CASES[case]
        case_dir = output / case
        case_dir.mkdir()
        settings = json.loads(json.dumps(settings_template))
        settings['File']['Load'] = [str((args.fixtures / f'{case}.imm').resolve())]
        settings['Rendering']['RenderingAPI'] = args.graphics_api
        settings['Rendering']['EnableVR'] = False
        settings['Rendering']['RenderingTechnique'] = 'Static'
        settings['Playback']['PlayerSpawn']['Location'] = 'Custom'
        settings['Playback']['PlayerSpawn']['Custom'] = {'Rotation': [0, 0, 0, 1], 'Scale': 1, 'Flip': 'N',
                                                        'Translation': [2.25, 1.5, 3]}
        case_settings = case_dir / 'settings.json'
        case_settings.write_text(json.dumps(settings, indent=2), encoding='utf-8')
        environment = os.environ.copy()
        for key in list(environment):
            if key.startswith(('IMM_VIEWER_VALIDATE_', 'IMM_GL_VALIDATE_', 'IMM_LIVE_EDIT_', 'IMM_VIEWER_LIVE_', 'IMM_UNITY_SKIP_GPU_LOAD_')):
                environment.pop(key)
        environment.update({'IMM_VIEWER_LIVE_EDIT': '60', 'IMM_VIEWER_LIVE_BENCHMARK_POINTS': str(points),
            'IMM_VIEWER_LIVE_BENCHMARK_SAMPLES': str(args.samples), 'IMM_VIEWER_LIVE_BENCHMARK_WARMUP': str(args.warmup),
            'IMM_LIVE_EDIT_TRACE_FRAMES': '1', 'IMM_VIEWER_VALIDATE_FRAME': '60', 'IMM_VIEWER_VALIDATE_MAX_FRAME': '100000',
            'IMM_VIEWER_VALIDATE_MIN_NONZERO': '16', 'IMM_VIEWER_VALIDATE_MIN_DRAWCALLS': '1',
            'IMM_VIEWER_VALIDATE_MIN_TRIANGLES': '1', 'IMM_VIEWER_VALIDATE_DISABLE_AUDIO': '1',
            'IMM_VIEWER_VALIDATE_LOCK_CAMERA': '1',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS': '0', 'IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS': '0', 'IMM_VIEWER_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS': '0',
            'IMM_VIEWER_VALIDATE_CAPTURE_PATH': str(case_dir / 'capture.ppm')})
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        # The Windows entry point changes cwd to its executable directory, so
        # every owned case needs its own executable and runtime DLLs as well.
        case_viewer = case_dir / staged_viewer.name
        shutil.copy2(staged_viewer, case_viewer)
        for dependency in runtime.glob('*.dll'):
            shutil.copy2(dependency, case_dir / dependency.name)
        command = [str(case_viewer), str(case_settings)]
        started = time.monotonic()
        # Runtime resources resolve relative to our isolated working directory.
        # Each case owns its own debug.txt; no shared viewer log is touched.
        for resource in runtime.iterdir():
            if resource.is_dir():
                shutil.copytree(resource, case_dir / resource.name)
        with (case_dir / 'stdout.log').open('wb') as log:
            child = subprocess.Popen(command, cwd=case_dir, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                     startupinfo=startup)
            try:
                exit_code = child.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                child.terminate()  # Only our owned child, never another viewer.
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=10)
                raise RuntimeError(f'{case}: owned viewer timed out')
        debug = case_dir / 'debug.txt'
        provenance['cases'][case] = {'exitCode': exit_code, 'elapsedSeconds': time.monotonic() - started,
            'settingsSha256': hashlib.sha256(case_settings.read_bytes()).hexdigest()}
        (output / 'provenance.json').write_text(json.dumps(provenance, indent=2), encoding='utf-8')
        if exit_code != 0 or not debug.is_file():
            raise RuntimeError(f'{case}: viewer failed (exit {exit_code}); inspect retained logs')
        text = debug.read_text(encoding='utf-8', errors='replace')
        if '[IMM_LIVE_BENCHMARK_ERROR]' in text or '[IMM_LIVE_BENCHMARK_DONE]' not in text:
            raise RuntimeError(f'{case}: native benchmark did not complete successfully')
        # Preserve the positive paint/triangle/render gate independently of nonzero
        # pixels, which can otherwise be satisfied by background or user interface.
        render = [line for line in text.splitlines() if 'IMM GL validation: frame=' in line]
        if not render or not all(int(re.search(fr'{field}=(\d+)', render[-1])[1]) > 0
                                 for field in ('paintDrawCalls', 'triangles')):
            raise RuntimeError(f'{case}: no positive paint/triangle rendering evidence')
        capture = (case_dir / 'capture.ppm').read_bytes()
        header = capture.split(b'\n', 3)
        if len(header) != 4 or header[0] != b'P6' or header[2] != b'255':
            raise RuntimeError(f'{case}: expected native RGB PPM capture')
        width, height = map(int, header[1].split())
        pixels = header[3]
        if len(pixels) != width * height * 3 or sum(pixels[i:i+3] != b'\0\0\0' for i in range(0, len(pixels), 3)) < 16:
            raise RuntimeError(f'{case}: capture has no nonbackground pixels')
        parse_log(debug, case, args.samples, args.warmup)
        report_command += ['--log', f'{case}={debug}']
    subprocess.run(report_command, check=True)


if __name__ == '__main__':
    main()
