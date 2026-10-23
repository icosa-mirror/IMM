#!/usr/bin/env python3
"""Exercise the actual bundled codec and unchanged video-validation CLI."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report_path = out / 'report.json'
    report_path.write_text(json.dumps({'passed': False}), encoding='utf-8')
    env = dict(os.environ)
    env['PATH'] = f'{args.bin_dir.resolve()}{os.pathsep}{env.get("PATH", "")}'
    ffmpeg = shutil.which('ffmpeg', path=env['PATH'])
    if ffmpeg is None or Path(ffmpeg).resolve().parent != args.bin_dir.resolve():
        raise RuntimeError('The provisioned FFmpeg must be selected on PATH')
    reference = out / 'reference.ppm'
    reference.write_bytes(b'P6\n64 64\n255\n' + bytes([240, 48, 32]) * 4096)
    black = out / 'black.ppm'
    black.write_bytes(b'P6\n64 64\n255\n' + bytes(4096 * 3))
    contract = out / 'contract.json'
    contract.write_text(json.dumps({'validation': {
        'requires_dimensions': {'width': 64, 'height': 64},
        'minimum_non_black_pixels': 2048,
    }}), encoding='utf-8')
    rows = []
    for case, source, expected in [('matching', reference, True), ('blank', black, False),
                                   ('corrupt', None, False)]:
        video = out / f'{case}.mp4'
        if source is not None:
            subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-y',
                            '-loop', '1', '-framerate', '4', '-i', str(source),
                            '-t', '2', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', str(video)],
                           check=True, capture_output=True, timeout=30)
        else:
            video.write_bytes(b'not a video')
        verdict = out / f'{case}-verdict.json'
        with (out / f'{case}.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([
                sys.executable, str(Path(__file__).with_name('validate_render_video.py')),
                str(video), '--reference', str(reference), '--contract', str(contract),
                '--capture-output', str(out / f'{case}.png'), '--json-output', str(verdict),
                '--sample-interval-seconds', '0.5', '--minimum-consecutive-frames', '2',
            ], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=30)
        evidence = json.loads(verdict.read_text(encoding='utf-8'))
        if result.returncode != (0 if expected else 1) or evidence['passed'] != expected:
            raise RuntimeError(f'{case}: expected acceptance={expected}, exit={result.returncode}')
        rows.append({'case': case, 'accepted': evidence['passed'], 'exitCode': result.returncode})
    report_path.write_text(json.dumps({
        'passed': True, 'scope': 'real bundled codec and synthetic validator acceptance/rejection; not device rendering',
        'ffmpeg': ffmpeg, 'cases': rows,
    }, indent=2), encoding='utf-8')
    print('[IMM_VIDEO_FFMPEG_TEST] real matching, blank and corrupt video cases passed')


if __name__ == '__main__':
    main()
