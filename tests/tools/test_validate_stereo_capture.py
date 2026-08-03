#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import compare_render_metrics


def write_ppm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)


def join_side_by_side(width: int, height: int, left: bytes, right: bytes) -> bytes:
    row_bytes = width * 3
    return b"".join(
        left[y * row_bytes : (y + 1) * row_bytes]
        + right[y * row_bytes : (y + 1) * row_bytes]
        for y in range(height)
    )


def run(
    tool: Path,
    capture: Path,
    root: Path,
    minimum: int = 1,
    output_suffix: str = ".ppm",
    eye_width: int = 2,
    eye_height: int = 1,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(tool),
            str(capture),
            "--left-output",
            str(root / f"left{output_suffix}"),
            "--right-output",
            str(root / f"right{output_suffix}"),
            "--json-output",
            str(root / "result.json"),
            "--eye-width",
            str(eye_width),
            "--eye-height",
            str(eye_height),
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

        # A stereo pair is not valid merely because its halves differ. Model the
        # Quest regression explicitly: one eye contains the reference scene and
        # the other contains only a smooth sky. The structural split passes, but
        # independent baseline validation must reject the sky-only eye.
        scene_width = 8
        scene_height = 8
        scene = bytes(
            channel
            for y in range(scene_height)
            for x in range(scene_width)
            for channel in (
                (220, 80, 40) if x < 4 and y < 4 else
                (30, 160, 220) if x >= 4 and y >= 4 else
                (12, 18, 24)
            )
        )
        sky = bytes([80, 120, 180] * (scene_width * scene_height))
        write_ppm(
            capture,
            scene_width * 2,
            scene_height,
            join_side_by_side(scene_width, scene_height, scene, sky),
        )
        split = run(
            tool,
            capture,
            root,
            minimum=1,
            output_suffix=".png",
            eye_width=scene_width,
            eye_height=scene_height,
        )
        assert split.returncode == 0, split.stdout + split.stderr

        reference = root / "scene-reference.ppm"
        write_ppm(reference, scene_width, scene_height, scene)
        reference_metrics = compare_render_metrics.collect_metrics(reference)
        contract = root / "scene-contract.json"
        contract.write_text(
            json.dumps(
                {
                    "schema": "imm-render-baseline-contract-v1",
                    "baseline": "synthetic-stereo-unit-test",
                    "validation": {
                        "format": "png",
                        "minimum_dimensions": {"width": scene_width, "height": scene_height},
                        "minimum_non_black_pixels": 1,
                        "minimum_near_visible_pixels": 1,
                        "minimum_luma_span": 10,
                        "expected_visible_luma_mean": {
                            "min": reference_metrics["visible_luma_mean"] - 1,
                            "max": reference_metrics["visible_luma_mean"] + 1,
                        },
                        "expected_spatial_luma_grid": {
                            "width": 2,
                            "height": 2,
                            "max_mean_abs_delta": 0.01,
                            "min_correlation": 0.99,
                        },
                    },
                }
            ),
            encoding="utf-8",
        )
        left_result = compare_render_metrics.evaluate_capture(root / "left.png", reference, contract)
        right_result = compare_render_metrics.evaluate_capture(root / "right.png", reference, contract)
        assert left_result["passed"], left_result["errors"]
        assert not right_result["passed"], "sky-only right eye unexpectedly passed baseline validation"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
