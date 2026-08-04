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


def make_probe_pixels(
    width: int,
    height: int,
    show_rear_occluded: bool,
    show_legitimate_cyan: bool = False,
) -> list[tuple[int, int, int]]:
    pixels = [(16, 20, 24)] * (width * height)
    for y in range(height // 2, height):
        for x in range((width * 9) // 10):
            pixels[y * width + x] = (90, 25, 25)
    rectangles = [
        (23, 48, 45, 80, (255, 0, 255)),
        (57, 14, 78, 41, (255, 255, 0)),
    ]
    if show_rear_occluded:
        rectangles.append((45, 35, 69, 65, (0, 255, 255)))
    if show_legitimate_cyan:
        rectangles.append((63, 42, 68, 47, (0, 255, 255)))
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
        probe_legitimate_cyan_path = temp / "probe-legitimate-cyan.ppm"
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
        write_ppm(
            probe_legitimate_cyan_path,
            100,
            100,
            make_probe_pixels(100, 100, False, show_legitimate_cyan=True),
        )
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
                "expected_spatial_luma_regions": [
                    {
                        "name": "upper-detail",
                        "region_normalized": {"x": 0.0, "y": 0.0, "width": 0.75, "height": 0.5},
                        "width": 3,
                        "height": 2,
                        "max_mean_abs_delta": 0.02,
                        "min_correlation": 0.9,
                    }
                ],
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
        cropped_region_specification = {
            "name": "center-cropped-detail",
            "center_crop_aspect_ratio": 1.0,
            "region_normalized": {"x": 0.0, "y": 0.0, "width": 0.75, "height": 0.5},
            "width": 3,
            "height": 2,
            "max_mean_abs_delta": 0.02,
            "min_correlation": 0.9,
        }
        cropped_barred_region = compare_render_metrics.collect_spatial_region_metrics(
            reference_path, barred_path, cropped_region_specification
        )
        assert compare_render_metrics.validate_spatial_contract(contract, same_spatial) == []
        assert compare_render_metrics.validate_spatial_contract(contract, scaled_spatial) == []
        assert compare_render_metrics.validate_spatial_contract(contract, cropped_barred_spatial) == []
        assert compare_render_metrics.validate_spatial_region_contract(
            [cropped_region_specification], [cropped_barred_region]
        ) == []
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

        production_overlay_contract = json.loads(
            (Path(__file__).resolve().parents[2] / "tests/baselines/render/sample1-ordered-overlay.json").read_text(
                encoding="utf-8"
            )
        )["validation"]["expected_color_components"]
        production_overlay_good = compare_render_metrics.collect_color_component_metrics(
            probe_good_path, production_overlay_contract
        )
        assert compare_render_metrics.validate_color_component_contract(
            production_overlay_contract, production_overlay_good
        ) == []
        production_overlay_legitimate_cyan = compare_render_metrics.collect_color_component_metrics(
            probe_legitimate_cyan_path, production_overlay_contract
        )
        assert compare_render_metrics.validate_color_component_contract(
            production_overlay_contract, production_overlay_legitimate_cyan
        ) == []
        production_overlay_wrong_depth = compare_render_metrics.collect_color_component_metrics(
            probe_wrong_depth_path, production_overlay_contract
        )
        assert any(
            "character-occluded-cyan" in error
            for error in compare_render_metrics.validate_color_component_contract(
                production_overlay_contract, production_overlay_wrong_depth
            )
        )

        production_full_depth_contract = json.loads(
            (Path(__file__).resolve().parents[2] / "tests/baselines/render/sample1-full-depth.json").read_text(
                encoding="utf-8"
            )
        )["validation"]["expected_color_components"]
        production_full_depth_good = compare_render_metrics.collect_color_component_metrics(
            probe_good_path, production_full_depth_contract
        )
        assert compare_render_metrics.validate_color_component_contract(
            production_full_depth_contract, production_full_depth_good
        ) == []
        production_full_depth_wrong_depth = compare_render_metrics.collect_color_component_metrics(
            probe_wrong_depth_path, production_full_depth_contract
        )
        assert any(
            "character-occluded-cyan" in error
            for error in compare_render_metrics.validate_color_component_contract(
                production_full_depth_contract, production_full_depth_wrong_depth
            )
        )

        android_composition_contract = json.loads(
            (Path(__file__).resolve().parents[2] / "tests/baselines/render/unity-android-vulkan-composition-sample1.json").read_text(
                encoding="utf-8"
            )
        )["validation"]["expected_color_components"]
        android_composition_good = compare_render_metrics.collect_color_component_metrics(
            probe_good_path, android_composition_contract
        )
        assert compare_render_metrics.validate_color_component_contract(
            android_composition_contract, android_composition_good
        ) == []
        android_composition_legitimate_cyan = compare_render_metrics.collect_color_component_metrics(
            probe_legitimate_cyan_path, android_composition_contract
        )
        assert compare_render_metrics.validate_color_component_contract(
            android_composition_contract, android_composition_legitimate_cyan
        ) == []
        android_composition_wrong_depth = compare_render_metrics.collect_color_component_metrics(
            probe_wrong_depth_path, android_composition_contract
        )
        assert any(
            "rear-occluded-cyan" in error
            for error in compare_render_metrics.validate_color_component_contract(
                android_composition_contract, android_composition_wrong_depth
            )
        )

        production_content_contract = json.loads(
            (Path(__file__).resolve().parents[2] / "tests/baselines/render/sample1-composition-content.json").read_text(
                encoding="utf-8"
            )
        )["validation"]["expected_color_components"]
        production_content_present = compare_render_metrics.collect_color_component_metrics(
            probe_good_path, production_content_contract
        )
        assert compare_render_metrics.validate_color_component_contract(
            production_content_contract, production_content_present
        ) == []
        production_content_missing = compare_render_metrics.collect_color_component_metrics(
            black_path, production_content_contract
        )
        assert any(
            "sample1-lower-red-brush-content" in error
            for error in compare_render_metrics.validate_color_component_contract(
                production_content_contract, production_content_missing
            )
        )

        # Both Android Godot contracts must reject localized foreground/depth
        # corruption around the character. This is the pixel-level regression
        # fixture for the reverse-Z failure that broad whole-frame metrics
        # previously allowed through.
        for contract_name in (
            "godot-android-vulkan-sample1.json",
            "godot-android-vulkan-composition-sample1.json",
        ):
            android_godot_contract = json.loads(
                (
                    Path(__file__).resolve().parents[2]
                    / "tests/baselines/render"
                    / contract_name
                ).read_text(encoding="utf-8")
            )
            depth_regions = android_godot_contract["validation"]["expected_spatial_luma_regions"]
            good_depth_metrics = [
                compare_render_metrics.collect_spatial_region_metrics(
                    probe_good_path,
                    probe_good_path,
                    specification,
                )
                for specification in depth_regions
            ]
            assert compare_render_metrics.validate_spatial_region_contract(
                depth_regions,
                good_depth_metrics,
            ) == []
            wrong_depth_metrics = [
                compare_render_metrics.collect_spatial_region_metrics(
                    probe_good_path,
                    probe_wrong_depth_path,
                    specification,
                )
                for specification in depth_regions
            ]
            depth_errors = compare_render_metrics.validate_spatial_region_contract(
                depth_regions,
                wrong_depth_metrics,
            )
            assert any("character-front-depth-order" in error for error in depth_errors), (
                f"{contract_name} accepted localized reverse-Z foreground corruption"
            )

        # Exercise the production Unity/macOS/Metal contract against both
        # tolerated renderer drift and visually wrong full-size captures. The
        # correlation floor is intentionally below the reviewed CI captures
        # (0.387, 0.444, and 0.451), while these bad poses remain well outside it.
        repo_root = Path(__file__).resolve().parents[2]
        macos_reference_path = repo_root / "tests/baselines/render/unity-windows-directx-sample1.png"
        macos_contract_path = repo_root / "tests/baselines/render/unity-macos-metal-sample1.json"
        macos_width, macos_height, macos_pixels, _ = compare_render_metrics.read_rgb_capture(
            macos_reference_path
        )
        macos_row_bytes = macos_width * 3

        mild_drift_path = temp / "macos-mild-drift.png"
        write_render_report.write_png(
            mild_drift_path,
            macos_width,
            macos_height,
            bytes(min(255, channel + 3) for channel in macos_pixels),
        )
        mild_drift = compare_render_metrics.evaluate_capture(
            mild_drift_path, macos_reference_path, macos_contract_path
        )
        assert mild_drift["passed"], mild_drift["errors"]

        pose_shift = macos_width // 4
        wrong_pose_pixels = b"".join(
            row[pose_shift * 3 :] + row[: pose_shift * 3]
            for y in range(macos_height)
            for row in [macos_pixels[y * macos_row_bytes : (y + 1) * macos_row_bytes]]
        )
        wrong_pose_path = temp / "macos-wrong-pose.png"
        write_render_report.write_png(wrong_pose_path, macos_width, macos_height, wrong_pose_pixels)
        wrong_pose = compare_render_metrics.evaluate_capture(
            wrong_pose_path, macos_reference_path, macos_contract_path
        )
        assert not wrong_pose["passed"], "shifted Unity camera pose unexpectedly passed"
        assert any("spatial luma grid correlation" in error for error in wrong_pose["errors"])

        default_scene_pixels = bytearray([70, 100, 140] * (macos_width * macos_height))
        for x0, y0, x1, y1, color in [
            (180, 250, 430, 500, (255, 0, 255)),
            (800, 100, 1000, 260, (255, 255, 0)),
            (570, 280, 800, 480, (0, 255, 255)),
        ]:
            for y in range(y0, y1):
                for x in range(x0, x1):
                    offset = (y * macos_width + x) * 3
                    default_scene_pixels[offset : offset + 3] = bytes(color)
        default_scene_path = temp / "macos-default-scene-probes.png"
        write_render_report.write_png(
            default_scene_path, macos_width, macos_height, bytes(default_scene_pixels)
        )
        default_scene = compare_render_metrics.evaluate_capture(
            default_scene_path, macos_reference_path, macos_contract_path
        )
        assert not default_scene["passed"], "default scene with probe squares unexpectedly passed"
        assert any("spatial luma grid" in error for error in default_scene["errors"])

        godot_play_contract_path = (
            repo_root / "tests/baselines/render/godot-windows-vulkan-sample-play.json"
        )
        godot_play_reference_path = (
            repo_root / "tests/baselines/render/godot-windows-vulkan-sample-play.png"
        )
        godot_wrong_pose = compare_render_metrics.evaluate_capture(
            wrong_pose_path, godot_play_reference_path, godot_play_contract_path
        )
        assert not godot_wrong_pose["passed"], "shifted Godot Run camera pose unexpectedly passed"
        godot_default_scene = compare_render_metrics.evaluate_capture(
            default_scene_path, godot_play_reference_path, godot_play_contract_path
        )
        assert not godot_default_scene["passed"], "default Godot Run scene unexpectedly passed"

        output_path.write_text(json.dumps({"passed": True}, indent=2), encoding="utf-8")

    print("Render metric drift tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
