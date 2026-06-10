#!/usr/bin/env python3
"""Focused checks for render report image generation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_ppm(path: Path, width: int, height: int, color: tuple[int, int, int]) -> None:
    payload = bytes(color) * width * height
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + payload)


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        reference = temp / "reference.ppm"
        candidate = temp / "candidate.ppm"
        metrics = temp / "metrics.json"
        images = temp / "images"
        report = temp / "render-report.md"

        write_ppm(reference, 2, 2, (8, 16, 32))
        write_ppm(candidate, 2, 2, (64, 96, 128))
        metrics.write_text(
            json.dumps(
                {
                    "passed": True,
                    "errors": [],
                    "reference": {"width": 2, "height": 2, "non_black_pixels": 4, "near_visible_pixels": 0, "visible_luma_mean": 14.0},
                    "candidate": {"width": 2, "height": 2, "non_black_pixels": 4, "near_visible_pixels": 4, "visible_luma_mean": 89.0},
                }
            ),
            encoding="utf-8",
        )

        result = subprocess.run(
            [
                sys.executable,
                "tests/tools/write_render_report.py",
                "--metrics-json",
                str(metrics),
                "--reference-capture",
                str(reference),
                "--candidate-capture",
                str(candidate),
                "--output-dir",
                str(images),
                "--markdown-output",
                str(report),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stderr
        assert (images / "reference.png").read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
        assert (images / "candidate.png").read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
        text = report.read_text(encoding="utf-8")
        assert "# Render Validation Report" in text
        assert "![Reference]" in text
        assert "![Candidate]" in text
        assert "visible_luma_mean" in text

    print("Render report tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
