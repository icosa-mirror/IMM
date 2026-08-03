#!/usr/bin/env python3
"""Split and structurally validate side-by-side stereo render evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from compare_render_metrics import compute_rgb_metrics, read_rgb_capture


def write_ppm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)


def split_stereo(width: int, height: int, pixels: bytes) -> tuple[bytes, bytes]:
    if width % 2:
        raise ValueError(f"Stereo capture width must be even, got {width}")
    eye_width = width // 2
    row_bytes = width * 3
    eye_row_bytes = eye_width * 3
    left = bytearray(eye_width * height * 3)
    right = bytearray(eye_width * height * 3)
    for y in range(height):
        source = y * row_bytes
        destination = y * eye_row_bytes
        left[destination : destination + eye_row_bytes] = pixels[source : source + eye_row_bytes]
        right[destination : destination + eye_row_bytes] = pixels[
            source + eye_row_bytes : source + row_bytes
        ]
    return bytes(left), bytes(right)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--left-output", type=Path, required=True)
    parser.add_argument("--right-output", type=Path, required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--eye-width", type=int, default=1280)
    parser.add_argument("--eye-height", type=int, default=720)
    parser.add_argument("--minimum-changed-pixels", type=int, default=1000)
    args = parser.parse_args()

    width, height, pixels, format_name = read_rgb_capture(args.capture)
    expected_width = args.eye_width * 2
    failures: list[str] = []
    if width != expected_width or height != args.eye_height:
        failures.append(
            f"expected {expected_width}x{args.eye_height} stereo capture, got {width}x{height}"
        )

    if width % 2:
        failures.append(f"capture width is not divisible into two eyes: {width}")
        left = right = b""
    else:
        left, right = split_stereo(width, height, pixels)

    changed_pixels = 0
    absolute_delta = 0
    if left and right and len(left) == len(right):
        for index in range(0, len(left), 3):
            delta = (
                abs(left[index] - right[index])
                + abs(left[index + 1] - right[index + 1])
                + abs(left[index + 2] - right[index + 2])
            )
            absolute_delta += delta
            if delta:
                changed_pixels += 1
    if changed_pixels < args.minimum_changed_pixels:
        failures.append(
            f"eyes are identical or insufficiently distinct: changed_pixels={changed_pixels} "
            f"minimum={args.minimum_changed_pixels}"
        )

    eye_width = width // 2 if width % 2 == 0 else 0
    if left and right:
        write_ppm(args.left_output, eye_width, height, left)
        write_ppm(args.right_output, eye_width, height, right)

    result = {
        "schema": "imm-synthetic-stereo-capture-v1",
        "status": "failed" if failures else "passed",
        "capture": str(args.capture),
        "format": format_name,
        "width": width,
        "height": height,
        "eye_width": eye_width,
        "changed_pixels": changed_pixels,
        "mean_absolute_channel_delta": (
            absolute_delta / (max(1, changed_pixels) * 3) if changed_pixels else 0.0
        ),
        "left_metrics": compute_rgb_metrics(eye_width, height, left, "ppm-p6") if left else None,
        "right_metrics": compute_rgb_metrics(eye_width, height, right, "ppm-p6") if right else None,
        "failures": failures,
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
