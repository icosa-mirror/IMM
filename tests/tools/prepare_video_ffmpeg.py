#!/usr/bin/env python3
"""Expose the pinned wheel's bundled FFmpeg to the existing video validator."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import imageio_ffmpeg


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--github-path', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'passed': False}), encoding='utf-8')
    source = Path(imageio_ffmpeg.get_ffmpeg_exe()).resolve()
    bundled = (Path(imageio_ffmpeg.__file__).parent / 'binaries').resolve()
    if imageio_ffmpeg.__version__ != '0.6.0' or source.parent != bundled:
        raise RuntimeError('Expected the pinned 0.6.0 wheel bundled FFmpeg, not a system fallback')
    bin_dir = args.bin_dir.resolve()
    bin_dir.mkdir(parents=True, exist_ok=True)
    executable = bin_dir / ('ffmpeg.exe' if os.name == 'nt' else 'ffmpeg')
    shutil.copy2(source, executable)
    executable.chmod(executable.stat().st_mode | 0o111)
    result = subprocess.run([str(executable), '-version'], check=True,
                            capture_output=True, text=True, timeout=30)
    version = result.stdout.splitlines()[0]
    if args.github_path:
        with args.github_path.open('a', encoding='utf-8') as stream:
            stream.write(f'{bin_dir}\n')
    args.report.write_text(json.dumps({
        'passed': True, 'package': 'imageio-ffmpeg==0.6.0', 'version': version,
        'executable': str(executable),
        'sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
    }, indent=2), encoding='utf-8')
    print(f'[IMM_VIDEO_FFMPEG] {version}')


if __name__ == '__main__':
    main()
