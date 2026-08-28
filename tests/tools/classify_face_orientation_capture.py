#!/usr/bin/env python3
"""Classify and annotate the deterministic IMM face-orientation render."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from compare_render_metrics import read_rgb_capture
from write_render_report import write_png


@dataclass(frozen=True)
class Region:
    name: str
    bounds: tuple[float, float, float, float]
    color: tuple[int, int, int]


REGIONS = (
    Region("cyan_exterior", (0.29, 0.25, 0.45, 0.75), (40, 210, 245)),
    Region("dark_backface_sentinel", (0.44, 0.30, 0.56, 0.70), (255, 150, 30)),
    Region("green_layout_marker", (0.55, 0.25, 0.72, 0.75), (55, 240, 105)),
)


def is_cyan(r: int, g: int, b: int) -> bool:
    return b >= 100 and b > r * 1.35 and b > g * 1.05


def is_green(r: int, g: int, b: int) -> bool:
    return g >= 100 and g > r * 1.35 and g > b * 1.25


def is_orange(r: int, g: int, b: int) -> bool:
    return r >= 100 and r > g * 1.45 and r > b * 1.8


def pixel_bounds(region: Region, width: int, height: int) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = region.bounds
    return (
        int(x0 * width),
        int(y0 * height),
        max(int(x0 * width) + 1, int(x1 * width)),
        max(int(y0 * height) + 1, int(y1 * height)),
    )


def count_region(
    pixels: bytes,
    width: int,
    height: int,
    region: Region,
    predicate,
) -> dict:
    x0, y0, x1, y1 = pixel_bounds(region, width, height)
    matched = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            offset = (y * width + x) * 3
            if predicate(*pixels[offset : offset + 3]):
                matched += 1
    area = (x1 - x0) * (y1 - y0)
    return {
        "bounds": {"x": x0, "y": y0, "width": x1 - x0, "height": y1 - y0},
        "matched_pixels": matched,
        "matched_share": matched / area,
    }


def draw_outline(
    output: bytearray,
    width: int,
    height: int,
    region: Region,
    color: tuple[int, int, int],
    thickness: int = 4,
) -> None:
    x0, y0, x1, y1 = pixel_bounds(region, width, height)
    for y in range(max(0, y0), min(height, y1)):
        for x in range(max(0, x0), min(width, x1)):
            if x - x0 < thickness or x1 - 1 - x < thickness or y - y0 < thickness or y1 - 1 - y < thickness:
                offset = (y * width + x) * 3
                output[offset : offset + 3] = bytes(color)


def classify(width: int, height: int, pixels: bytes) -> dict:
    cyan = count_region(pixels, width, height, REGIONS[0], is_cyan)
    orange = count_region(pixels, width, height, REGIONS[1], is_orange)
    green = count_region(pixels, width, height, REGIONS[2], is_green)
    checks = {
        "cyan_exterior_visible": {
            "passed": cyan["matched_share"] >= 0.02,
            "actual": cyan["matched_share"],
            "minimum": 0.02,
        },
        "backface_interior_hidden": {
            "passed": orange["matched_share"] <= 0.002,
            "actual": orange["matched_share"],
            "maximum": 0.002,
        },
        "layout_marker_visible": {
            "passed": green["matched_share"] >= 0.02,
            "actual": green["matched_share"],
            "minimum": 0.02,
        },
    }
    failures = [name for name, check in checks.items() if not check["passed"]]
    return {
        "schema": "imm-face-orientation-status-v1",
        "result": "passed" if not failures else "render_failed",
        "failure_class": "" if not failures else "face_orientation",
        "failures": failures,
        "image": {"width": width, "height": height},
        "measurements": {
            "cyan_exterior": cyan,
            "dark_backface_sentinel": orange,
            "green_layout_marker": green,
        },
        "checks": checks,
    }


def write_overlay(path: Path, width: int, height: int, pixels: bytes, status: dict) -> None:
    output = bytearray(pixels)
    passed_color = (40, 220, 80)
    failed_color = (245, 45, 45)
    region_checks = (
        "cyan_exterior_visible",
        "backface_interior_hidden",
        "layout_marker_visible",
    )
    for region, check_name in zip(REGIONS, region_checks):
        color = passed_color if status["checks"][check_name]["passed"] else failed_color
        draw_outline(output, width, height, region, color)
    band_color = passed_color if status["result"] == "passed" else failed_color
    for y in range(min(12, height)):
        for x in range(width):
            offset = (y * width + x) * 3
            output[offset : offset + 3] = bytes(band_color)
    write_png(path, width, height, bytes(output))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--status-output", type=Path, required=True)
    parser.add_argument("--overlay-output", type=Path, required=True)
    parser.add_argument("--capture-output", type=Path)
    args = parser.parse_args()

    width, height, pixels, _format = read_rgb_capture(args.capture)
    status = classify(width, height, pixels)
    args.status_output.parent.mkdir(parents=True, exist_ok=True)
    args.status_output.write_text(
        json.dumps(status, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    write_overlay(args.overlay_output, width, height, pixels, status)
    if args.capture_output:
        write_png(args.capture_output, width, height, pixels)
    print(f"Face-orientation result: {status['result']}")
    for name, check in status["checks"].items():
        print(f"{name}: passed={check['passed']} actual={check['actual']:.6f}")
    return 0 if status["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
