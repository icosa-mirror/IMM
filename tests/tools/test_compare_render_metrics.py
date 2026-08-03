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


def compress_contrast(pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    return [(96 + r // 8, 96 + g // 8, 96 + b // 8) for r, g, b in pixels]


def scale_2x(width: int, height: int, pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    scaled: list[tuple[int, int, int]] = []
    for y in range(height):
        row = pixels[y * width : (y + 1) * width]
        doubled_row = [pixel for pixel in row for _ in range(2)]
        scaled.extend(doubled_row)
        scaled.extend(doubled_row)
    return scaled


def add_horizontal_bars(width: int, height: int, pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    barred: list[tuple[int, int, int]] = []
    black_bar = [(0, 0, 0)] * (width // 2)
    for y in range(height):
        row = pixels[y * width : (y + 1) * width]
        barred.extend(black_bar + row + black_bar)
    return barred


def blank_right_half(width: int, height: int, pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    return [
        pixel if x < width // 2 else (0, 0, 0)
        for y in range(height)
        for x, pixel in enumerate(pixels[y * width : (y + 1) * width])
    ]


def diagnostic_squares_only(width: int, height: int) -> list[tuple[int, int, int]]:
    pixels = [(0, 0, 0)] * (width * height)
    for x, color in ((1, (255, 0, 255)), (3, (255, 255, 0)), (5, (0, 255, 255))):
        for y in range(1, 3):
            for local_x in range(x, min(width, x + 2)):
                pixels[y * width + local_x] = color
    return pixels


def make_probe_pixels(width: int, height: int, show_rear_occluded: bool) -> list[tuple[int, int, int]]:
    pixels = [(16, 20, 24)] * (width * height)
    rectangles = [
        (23, 48, 45, 80, (255, 0, 255)),
        (57, 14, 78, 41, (255, 255, 0)),
    ]
    if show_rear_occluded:
        rectangles.append((45, 35, 69, 65, (0, 255, 255)))
    for x0, y0, x1, y1, color in rectangles:
        for y in range(y0, y1):
            for x in range(x0, x1):
                pixels[y * width + x] = color
    return pixels


def main() -> int:
    width = 8
    height = 8
    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        reference_path = temp / "reference.ppm"
        flipped_path = temp / "flipped.ppm"
        dark_path = temp / "dark.ppm"
        compressed_path = temp / "compressed.ppm"
        scaled_path = temp / "scaled.ppm"
        barred_path = temp / "barred.ppm"
        partial_path = temp / "partial.ppm"
        black_path = temp / "black.ppm"
        diagnostic_path = temp / "diagnostic-squares.ppm"
        probe_good_path = temp / "probe-good.ppm"
        probe_wrong_depth_path = temp / "probe-wrong-depth.ppm"
        probe_overlay_path = temp / "probe-wrong-depth-overlay.png"
        png_path = temp / "candidate.png"
        output_path = temp / "metrics.json"
        contract_path = temp / "contract.json"

        pixels = make_reference_pixels(width, height)
        write_ppm(reference_path, width, height, pixels)
        write_ppm(flipped_path, width, height, flip_vertical(width, height, pixels))
        write_ppm(dark_path, width, height, darken(pixels))
        write_ppm(compressed_path, width, height, compress_contrast(pixels))
        write_ppm(scaled_path, width * 2, height * 2, scale_2x(width, height, pixels))
        write_ppm(barred_path, width * 2, height, add_horizontal_bars(width, height, pixels))
        write_ppm(partial_path, width, height, blank_right_half(width, height, pixels))
        write_ppm(black_path, width, height, [(0, 0, 0)] * (width * height))
        write_ppm(diagnostic_path, width, height, diagnostic_squares_only(width, height))
        write_ppm(probe_good_path, 100, 100, make_probe_pixels(100, 100, False))
        write_ppm(probe_wrong_depth_path, 100, 100, make_probe_pixels(100, 100, True))
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
        compressed = compare_render_metrics.collect_metrics(compressed_path)
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
                "expected_spatial_luma_grid": {
                    "width": 4,
                    "height": 4,
                    "max_mean_abs_delta": 0.04,
                    "min_correlation": 0.9,
                },
                "expected_visible_channel_means": {
                    "values": reference["visible_channel_means"],
                    "tolerance": 16,
                },
                "expected_visible_luma_mean": {
                    "min": reference["visible_luma_mean"] - 16,
                    "max": reference["visible_luma_mean"] + 16,
                },
                "expected_luma_stddev": {
                    "min": max(0, reference["luma_stddev"] - 10),
                    "max": reference["luma_stddev"] + 10,
                },
                "expected_luma_percentiles": {
                    "p01": {"value": reference["luma_percentiles"]["p01"], "tolerance": 8},
                    "p50": {"value": reference["luma_percentiles"]["p50"], "tolerance": 8},
                    "p95": {"value": reference["luma_percentiles"]["p95"], "tolerance": 8},
                    "p99": {"value": reference["luma_percentiles"]["p99"], "tolerance": 8},
                },
            },
        }
        contract_path.write_text(json.dumps(contract), encoding="utf-8")

        probe_contract = {
            "center_crop_aspect_ratio": 1.0,
            "probes": [
                {
                    "name": "front-magenta",
                    "target_rgb": [255, 0, 255],
                    "channel_tolerance": 8,
                    "region_normalized": {"x": 0.2, "y": 0.45, "width": 0.3, "height": 0.4},
                    "minimum_largest_component_share_of_crop": 0.04,
                },
                {
                    "name": "rear-visible-yellow",
                    "target_rgb": [255, 255, 0],
                    "channel_tolerance": 8,
                    "region_normalized": {"x": 0.55, "y": 0.1, "width": 0.3, "height": 0.35},
                    "minimum_largest_component_share_of_crop": 0.04,
                },
                {
                    "name": "rear-occluded-cyan",
                    "target_rgb": [0, 255, 255],
                    "channel_tolerance": 8,
                    "region_normalized": {"x": 0.4, "y": 0.3, "width": 0.35, "height": 0.4},
                    "maximum_largest_component_share_of_crop": 0.01,
                },
            ],
        }

        assert not compare_render_metrics.compare_metrics(reference, same)
        assert png["format"] == "png"
        assert png["non_black_pixels"] == reference["non_black_pixels"]
        assert not compare_render_metrics.compare_metrics(reference, png)
        assert compare_render_metrics.validate_contract(json.loads(contract_path.read_text(encoding="utf-8")), reference) == []
        assert compare_render_metrics.validate_contract(json.loads(contract_path.read_text(encoding="utf-8")), png) == []
        same_spatial = compare_render_metrics.collect_spatial_metrics(reference_path, png_path, 4, 4)
        scaled_spatial = compare_render_metrics.collect_spatial_metrics(reference_path, scaled_path, 4, 4)
        flipped_spatial = compare_render_metrics.collect_spatial_metrics(reference_path, flipped_path, 4, 4)
        barred_spatial = compare_render_metrics.collect_spatial_metrics(reference_path, barred_path, 4, 4)
        cropped_barred_spatial = compare_render_metrics.collect_spatial_metrics(reference_path, barred_path, 4, 4, 1.0)
        assert compare_render_metrics.validate_spatial_contract(contract, same_spatial) == []
        assert compare_render_metrics.validate_spatial_contract(contract, scaled_spatial) == []
        assert compare_render_metrics.validate_spatial_contract(contract, cropped_barred_spatial) == []
        assert compare_render_metrics.validate_spatial_contract(contract, barred_spatial)
        scaled = compare_render_metrics.collect_metrics(scaled_path)
        assert any("width differs" in error for error in compare_render_metrics.compare_metrics(reference, scaled))
        assert not any(
            "width differs" in error or "height differs" in error
            for error in compare_render_metrics.compare_metrics(reference, scaled, allow_dimension_mismatch=True)
        )
        minimum_dimensions_contract = {"validation": {"minimum_dimensions": {"width": width, "height": height}}}
        assert compare_render_metrics.validate_contract(minimum_dimensions_contract, reference) == []
        assert compare_render_metrics.validate_contract(
            {"validation": {"minimum_dimensions": {"width": width * 2, "height": height}}},
            reference,
        )
        assert any(
            "spatial luma grid" in error
            for error in compare_render_metrics.validate_spatial_contract(contract, flipped_spatial)
        )
        assert any(
            "vertical luma profile" in error or "visible centroid" in error or "quadrant luma share" in error
            for error in compare_render_metrics.validate_contract(contract, flipped)
        )
        assert any(
            "visible luma mean" in error or "channel mean" in error
            for error in compare_render_metrics.validate_contract(contract, dark)
        )
        assert any(
            "luma stddev" in error or "luma percentile" in error
            for error in compare_render_metrics.validate_contract(contract, compressed)
        )
        assert any("vertical luma profile" in error or "centroid" in error for error in compare_render_metrics.compare_metrics(reference, flipped))
        assert any("visible luma mean" in error for error in compare_render_metrics.compare_metrics(reference, dark))

        # Each required known-bad visual class must be rejected by pixels, not
        # merely by an application log or capture-existence check.
        for known_bad_path in (black_path, partial_path, flipped_path, diagnostic_path):
            known_bad = compare_render_metrics.evaluate_capture(known_bad_path, reference_path, contract_path)
            assert not known_bad["passed"], f"known-bad capture unexpectedly passed: {known_bad_path.name}"

        probe_good = compare_render_metrics.collect_color_component_metrics(probe_good_path, probe_contract)
        assert compare_render_metrics.validate_color_component_contract(probe_contract, probe_good) == []
        probe_wrong_depth = compare_render_metrics.collect_color_component_metrics(
            probe_wrong_depth_path, probe_contract
        )
        wrong_depth_errors = compare_render_metrics.validate_color_component_contract(
            probe_contract, probe_wrong_depth
        )
        assert any("rear-occluded-cyan" in error for error in wrong_depth_errors)
        compare_render_metrics.write_color_component_diagnostic_overlay(
            probe_wrong_depth_path,
            probe_wrong_depth,
            probe_overlay_path,
        )
        assert probe_overlay_path.exists()
        _, _, overlay_pixels, _ = compare_render_metrics.read_rgb_capture(probe_overlay_path)
        assert bytes((255, 32, 32)) in overlay_pixels

        output_path.write_text(json.dumps({"passed": True}, indent=2), encoding="utf-8")

    print("Render metric drift tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
