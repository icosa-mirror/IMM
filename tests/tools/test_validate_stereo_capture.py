#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_ppm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)


def run(tool: Path, capture: Path, root: Path, minimum: int = 1) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(tool),
            str(capture),
            "--left-output",
            str(root / "left.ppm"),
            "--right-output",
            str(root / "right.ppm"),
            "--json-output",
            str(root / "result.json"),
            "--eye-width",
            "2",
            "--eye-height",
            "1",
            "--minimum-changed-pixels",
            str(minimum),
        ],
        text=True,
        capture_output=True,
    )


def main() -> int:
    tool = Path(__file__).with_name("validate_stereo_capture.py")
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        capture = root / "stereo.ppm"
        left = bytes([10, 20, 30, 40, 50, 60])
        right = bytes([10, 20, 31, 70, 80, 90])
        write_ppm(capture, 4, 1, left + right)
        passed = run(tool, capture, root)
        assert passed.returncode == 0, passed.stdout + passed.stderr
        result = json.loads((root / "result.json").read_text(encoding="utf-8"))
        assert result["status"] == "passed"
        assert result["changed_pixels"] == 2
        assert (root / "left.ppm").read_bytes().endswith(left)
        assert (root / "right.ppm").read_bytes().endswith(right)

        write_ppm(capture, 4, 1, left + left)
        failed = run(tool, capture, root)
        assert failed.returncode != 0
        result = json.loads((root / "result.json").read_text(encoding="utf-8"))
        assert result["status"] == "failed"
        assert "insufficiently distinct" in result["failures"][0]
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
