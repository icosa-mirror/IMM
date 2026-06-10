#!/usr/bin/env python3
"""Focused checks for render metric drift detection."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import compare_render_metrics
import write_render_report


def write_ppm(path: Path, width: int, height: int, pixels: list[tuple[int, int, int]]) -> None:
    payload = bytearray()
    for r, g, b in pixels:
        payload.extend([r, g, b])
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + bytes(payload))


def make_reference_pixels(width: int, height: int) -> list[tuple[int, int, int]]:
    pixels: list[tuple[int, int, int]] = []
    for y in range(height):
        for x in range(width):
            if 1 <= x <= 5 and 1 <= y <= 3:
                pixels.append((240, 48, 32))
            elif 2 <= x <= 6 and 5 <= y <= 6:
                pixels.append((32, 120, 220))
            else:
                pixels.append((0, 0, 0))
    return pixels


def flip_vertical(width: int, height: int, pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    flipped: list[tuple[int, int, int]] = []
    for y in range(height):
        source_y = height - y - 1
        start = source_y * width
        flipped.extend(pixels[start : start + width])
    return flipped


def darken(pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    return [(r // 3, g // 3, b // 3) for r, g, b in pixels]


def main() -> int:
    width = 8
    height = 8
    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        reference_path = temp / "reference.ppm"
        flipped_path = temp / "flipped.ppm"
        dark_path = temp / "dark.ppm"
        png_path = temp / "candidate.png"
        output_path = temp / "metrics.json"
        contract_path = temp / "contract.json"

        pixels = make_reference_pixels(width, height)
        write_ppm(reference_path, width, height, pixels)
        write_ppm(flipped_path, width, height, flip_vertical(width, height, pixels))
        write_ppm(dark_path, width, height, darken(pixels))
        write_render_report.write_png(
            png_path,
            width,
            height,
            bytes(channel for pixel in pixels for channel in pixel),
        )
        reference = compare_render_metrics.collect_metrics(reference_path)
        same = compare_render_metrics.collect_metrics(reference_path)
        flipped = compare_render_metrics.collect_metrics(flipped_path)
        dark = compare_render_metrics.collect_metrics(dark_path)
        png = compare_render_metrics.collect_metrics(png_path)

        contract = {
            "schema": "imm-render-baseline-contract-v1",
            "baseline": "synthetic",
            "validation": {
                "format": "ppm-p6",
                "requires_dimensions": {"width": width, "height": height},
                "minimum_non_black_pixels": 1,
                "minimum_near_visible_pixels": 1,
                "minimum_luma_span": 16,
                "requires_orientation_metrics": True,
                "requires_color_metrics": True,
                "expected_visible_centroid_normalized": {
                    "x": {"min": reference["visible_centroid"]["x_normalized"] - 0.05, "max": reference["visible_centroid"]["x_normalized"] + 0.05},
                    "y": {"min": reference["visible_centroid"]["y_normalized"] - 0.05, "max": reference["visible_centroid"]["y_normalized"] + 0.05},
                },
                "expected_vertical_luma_profile": {
                    "values": reference["vertical_luma_profile"],
                    "tolerance": 0.06,
                },
                "expected_quadrant_luma_share": {
                    "values": reference["quadrant_luma_share"],
                    "tolerance": 0.06,
                },
                "expected_visible_channel_means": {
                    "values": reference["visible_channel_means"],
                    "tolerance": 16,
                },
                "expected_visible_luma_mean": {
                    "min": reference["visible_luma_mean"] - 16,
                    "max": reference["visible_luma_mean"] + 16,
                },
            },
        }
        contract_path.write_text(json.dumps(contract), encoding="utf-8")

        assert not compare_render_metrics.compare_metrics(reference, same)
        assert png["format"] == "png"
        assert png["non_black_pixels"] == reference["non_black_pixels"]
        assert not compare_render_metrics.compare_metrics(reference, png)
        assert compare_render_metrics.validate_contract(json.loads(contract_path.read_text(encoding="utf-8")), reference) == []
        assert any(
            "vertical luma profile" in error or "visible centroid" in error or "quadrant luma share" in error
            for error in compare_render_metrics.validate_contract(contract, flipped)
        )
        assert any(
            "visible luma mean" in error or "channel mean" in error
            for error in compare_render_metrics.validate_contract(contract, dark)
        )
        assert any("vertical luma profile" in error or "centroid" in error for error in compare_render_metrics.compare_metrics(reference, flipped))
        assert any("visible luma mean" in error for error in compare_render_metrics.compare_metrics(reference, dark))

        output_path.write_text(json.dumps({"passed": True}, indent=2), encoding="utf-8")

    print("Render metric drift tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
