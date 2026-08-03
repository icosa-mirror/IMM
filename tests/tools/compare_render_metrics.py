#!/usr/bin/env python3
"""Collect and compare lightweight render capture metrics."""

from __future__ import annotations

import argparse
import struct
import hashlib
import json
import math
import sys
import zlib
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compute_rgb_metrics(width: int, height: int, pixels: bytes, format_name: str) -> dict:
    non_black = 0
    near_visible = 0
    min_luma = 255
    max_luma = 0
    min_x = width
    min_y = height
    max_x = -1
    max_y = -1
    luma_sum = 0
    luma_sq_sum = 0
    visible_luma_sum = 0
    visible_x_sum = 0
    visible_y_sum = 0
    visible_r_sum = 0
    visible_g_sum = 0
    visible_b_sum = 0
    visible_chroma_sum = 0
    quadrants = {
        "top_left": {"visible_pixels": 0, "luma_sum": 0},
        "top_right": {"visible_pixels": 0, "luma_sum": 0},
        "bottom_left": {"visible_pixels": 0, "luma_sum": 0},
        "bottom_right": {"visible_pixels": 0, "luma_sum": 0},
    }
    vertical_bins = [{"visible_pixels": 0, "luma_sum": 0} for _ in range(5)]
    luma_histogram = [0] * 256
    visible_luma_histogram = [0] * 256

    for index in range(0, len(pixels), 3):
        r, g, b = pixels[index], pixels[index + 1], pixels[index + 2]
        luma = (r * 299 + g * 587 + b * 114) // 1000
        luma_histogram[luma] += 1
        luma_sum += luma
        luma_sq_sum += luma * luma
        min_luma = min(min_luma, luma)
        max_luma = max(max_luma, luma)
        if r or g or b:
            pixel_index = index // 3
            x = pixel_index % width
            y = pixel_index // width
            non_black += 1
            if r > 32 or g > 32 or b > 32:
                near_visible += 1
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
            visible_luma_sum += luma
            visible_x_sum += x
            visible_y_sum += y
            visible_r_sum += r
            visible_g_sum += g
            visible_b_sum += b
            visible_chroma_sum += max(r, g, b) - min(r, g, b)
            visible_luma_histogram[luma] += 1

            top = y < height / 2
            left = x < width / 2
            quadrant_key = ("top" if top else "bottom") + "_" + ("left" if left else "right")
            quadrants[quadrant_key]["visible_pixels"] += 1
            quadrants[quadrant_key]["luma_sum"] += luma

            bin_index = min(4, int(y * 5 / height))
            vertical_bins[bin_index]["visible_pixels"] += 1
            vertical_bins[bin_index]["luma_sum"] += luma

    bounds = None
    visible_centroid = None
    visible_channel_means = None
    visible_luma_mean = 0.0
    visible_chroma_mean = 0.0
    quadrant_luma_share: dict[str, float] = {}
    vertical_luma_profile: list[float] = []
    if non_black:
        bounds = {"x": min_x, "y": min_y, "width": max_x - min_x + 1, "height": max_y - min_y + 1}
        visible_centroid = {
            "x": visible_x_sum / non_black,
            "y": visible_y_sum / non_black,
            "x_normalized": visible_x_sum / non_black / max(1, width - 1),
            "y_normalized": visible_y_sum / non_black / max(1, height - 1),
        }
        visible_channel_means = {
            "r": visible_r_sum / non_black,
            "g": visible_g_sum / non_black,
            "b": visible_b_sum / non_black,
        }
        visible_luma_mean = visible_luma_sum / non_black
        visible_chroma_mean = visible_chroma_sum / non_black
        for key, value in quadrants.items():
            quadrant_luma_share[key] = value["luma_sum"] / visible_luma_sum if visible_luma_sum else 0.0
        for value in vertical_bins:
            vertical_luma_profile.append(value["luma_sum"] / visible_luma_sum if visible_luma_sum else 0.0)

    pixel_count = width * height
    luma_mean = luma_sum / pixel_count
    luma_variance = max(0.0, (luma_sq_sum / pixel_count) - (luma_mean * luma_mean))

    return {
        "format": format_name,
        "metrics_version": 2,
        "width": width,
        "height": height,
        "non_black_pixels": non_black,
        "near_visible_pixels": near_visible,
        "content_bounds": bounds,
        "min_luma": min_luma,
        "max_luma": max_luma,
        "luma_span": max_luma - min_luma,
        "luma_mean": luma_mean,
        "luma_stddev": math.sqrt(luma_variance),
        "luma_percentiles": histogram_percentiles(luma_histogram, pixel_count),
        "visible_luma_mean": visible_luma_mean,
        "visible_luma_percentiles": histogram_percentiles(visible_luma_histogram, non_black),
        "visible_chroma_mean": visible_chroma_mean,
        "visible_centroid": visible_centroid,
        "visible_channel_means": visible_channel_means,
        "quadrants": quadrants,
        "quadrant_luma_share": quadrant_luma_share,
        "vertical_luma_profile": vertical_luma_profile,
    }


def histogram_percentiles(histogram: list[int], total: int) -> dict[str, int]:
    if total <= 0:
        return {}
    targets = {
        "p01": max(1, math.ceil(total * 0.01)),
        "p05": max(1, math.ceil(total * 0.05)),
        "p50": max(1, math.ceil(total * 0.50)),
        "p95": max(1, math.ceil(total * 0.95)),
        "p99": max(1, math.ceil(total * 0.99)),
    }
    values: dict[str, int] = {}
    cumulative = 0
    pending = dict(targets)
    for value, count in enumerate(histogram):
        cumulative += count
        for key, target in list(pending.items()):
            if cumulative >= target:
                values[key] = value
                del pending[key]
        if not pending:
            break
    return values


def read_ppm_capture(path: Path) -> tuple[int, int, bytes, str]:
    with path.open("rb") as handle:
        magic = handle.readline().strip()
        if magic != b"P6":
            raise ValueError(f"{path} is not a binary PPM (P6)")

        tokens: list[bytes] = []
        while len(tokens) < 3:
            line = handle.readline()
            if not line:
                raise ValueError(f"{path} ended before PPM header was complete")
            line = line.split(b"#", 1)[0].strip()
            if line:
                tokens.extend(line.split())

        width, height, max_value = (int(token) for token in tokens[:3])
        if max_value != 255:
            raise ValueError(f"{path} has unsupported PPM max value {max_value}")
        pixels = handle.read()

    expected_bytes = width * height * 3
    if len(pixels) != expected_bytes:
        raise ValueError(f"{path} has {len(pixels)} pixel bytes, expected {expected_bytes}")
    return width, height, pixels, "ppm-p6"


def read_ppm_metrics(path: Path) -> dict:
    width, height, pixels, format_name = read_ppm_capture(path)
    return compute_rgb_metrics(width, height, pixels, format_name)


def luma_at(pixels: bytes, index: int) -> int:
    base = index * 3
    r, g, b = pixels[base], pixels[base + 1], pixels[base + 2]
    return (r * 299 + g * 587 + b * 114) // 1000


def compute_spatial_luma_grid(width: int, height: int, pixels: bytes, grid_width: int, grid_height: int) -> list[float]:
    return compute_spatial_luma_grid_region(width, height, pixels, grid_width, grid_height, 0, 0, width, height)


def compute_spatial_luma_grid_region(
    width: int,
    height: int,
    pixels: bytes,
    grid_width: int,
    grid_height: int,
    region_x: int,
    region_y: int,
    region_width: int,
    region_height: int,
) -> list[float]:
    values: list[float] = []
    for grid_y in range(grid_height):
        y0 = region_y + grid_y * region_height // grid_height
        y1 = region_y + (grid_y + 1) * region_height // grid_height
        for grid_x in range(grid_width):
            x0 = region_x + grid_x * region_width // grid_width
            x1 = region_x + (grid_x + 1) * region_width // grid_width
            total = 0
            count = 0
            for y in range(y0, y1):
                row = y * width
                for x in range(x0, x1):
                    total += luma_at(pixels, row + x)
                    count += 1
            values.append((total / count / 255.0) if count else 0.0)
    return values


def center_crop_region(width: int, height: int, aspect_ratio: float | None) -> tuple[int, int, int, int]:
    if aspect_ratio is None:
        return 0, 0, width, height
    if not math.isfinite(aspect_ratio) or aspect_ratio <= 0:
        raise ValueError(f"center crop aspect ratio must be positive, got {aspect_ratio!r}")
    source_ratio = width / height
    if source_ratio > aspect_ratio:
        crop_width = max(1, min(width, round(height * aspect_ratio)))
        return (width - crop_width) // 2, 0, crop_width, height
    crop_height = max(1, min(height, round(width / aspect_ratio)))
    return 0, (height - crop_height) // 2, width, crop_height


def compare_luma_grids(reference_grid: list[float], candidate_grid: list[float]) -> dict:
    if len(reference_grid) != len(candidate_grid) or not reference_grid:
        return {"mean_abs_delta": None, "rmse": None, "correlation": None}
    mean_abs_delta = sum(abs(candidate - reference) for reference, candidate in zip(reference_grid, candidate_grid)) / len(reference_grid)
    rmse = math.sqrt(sum((candidate - reference) ** 2 for reference, candidate in zip(reference_grid, candidate_grid)) / len(reference_grid))
    ref_mean = sum(reference_grid) / len(reference_grid)
    cand_mean = sum(candidate_grid) / len(candidate_grid)
    ref_variance = sum((value - ref_mean) ** 2 for value in reference_grid)
    cand_variance = sum((value - cand_mean) ** 2 for value in candidate_grid)
    correlation = None
    if ref_variance > 0.0 and cand_variance > 0.0:
        covariance = sum((reference - ref_mean) * (candidate - cand_mean) for reference, candidate in zip(reference_grid, candidate_grid))
        correlation = covariance / math.sqrt(ref_variance * cand_variance)
    return {"mean_abs_delta": mean_abs_delta, "rmse": rmse, "correlation": correlation}


def collect_spatial_metrics(
    reference_path: Path,
    candidate_path: Path,
    grid_width: int,
    grid_height: int,
    center_crop_aspect_ratio: float | None = None,
) -> dict:
    reference_width, reference_height, reference_pixels, _ = read_rgb_capture(reference_path)
    candidate_width, candidate_height, candidate_pixels, _ = read_rgb_capture(candidate_path)
    reference_region = center_crop_region(reference_width, reference_height, center_crop_aspect_ratio)
    candidate_region = center_crop_region(candidate_width, candidate_height, center_crop_aspect_ratio)
    reference_grid = compute_spatial_luma_grid_region(
        reference_width, reference_height, reference_pixels, grid_width, grid_height, *reference_region
    )
    candidate_grid = compute_spatial_luma_grid_region(
        candidate_width, candidate_height, candidate_pixels, grid_width, grid_height, *candidate_region
    )
    metrics = compare_luma_grids(reference_grid, candidate_grid)
    metrics.update(
        {
            "grid_width": grid_width,
            "grid_height": grid_height,
            "reference_width": reference_width,
            "reference_height": reference_height,
            "candidate_width": candidate_width,
            "candidate_height": candidate_height,
            "center_crop_aspect_ratio": center_crop_aspect_ratio,
            "reference_crop": {
                "x": reference_region[0],
                "y": reference_region[1],
                "width": reference_region[2],
                "height": reference_region[3],
            },
            "candidate_crop": {
                "x": candidate_region[0],
                "y": candidate_region[1],
                "width": candidate_region[2],
                "height": candidate_region[3],
            },
        }
    )
    return metrics


def read_rgb_capture(path: Path) -> tuple[int, int, bytes, str]:
    suffix = path.suffix.lower()
    if suffix == ".ppm":
        return read_ppm_capture(path)
    if suffix == ".png":
        return read_png_capture(path)
    raise ValueError(f"Unsupported render capture format: {path.suffix}")


def paeth_predictor(left: int, up: int, upper_left: int) -> int:
    prediction = left + up - upper_left
    pa = abs(prediction - left)
    pb = abs(prediction - up)
    pc = abs(prediction - upper_left)
    if pa <= pb and pa <= pc:
        return left
    if pb <= pc:
        return up
    return upper_left


def unfilter_png_scanlines(data: bytes, width: int, height: int, bytes_per_pixel: int) -> bytes:
    stride = width * bytes_per_pixel
    expected = height * (stride + 1)
    if len(data) != expected:
        raise ValueError(f"PNG decompressed payload has {len(data)} bytes, expected {expected}")

    rows = []
    offset = 0
    previous = bytearray(stride)
    for _ in range(height):
        filter_type = data[offset]
        offset += 1
        raw = bytearray(data[offset : offset + stride])
        offset += stride

        for i in range(stride):
            left = raw[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
            up = previous[i]
            upper_left = previous[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
            if filter_type == 0:
                value = raw[i]
            elif filter_type == 1:
                value = raw[i] + left
            elif filter_type == 2:
                value = raw[i] + up
            elif filter_type == 3:
                value = raw[i] + ((left + up) // 2)
            elif filter_type == 4:
                value = raw[i] + paeth_predictor(left, up, upper_left)
            else:
                raise ValueError(f"Unsupported PNG scanline filter {filter_type}")
            raw[i] = value & 0xFF

        rows.append(bytes(raw))
        previous = raw
    return b"".join(rows)


def read_png_capture(path: Path) -> tuple[int, int, bytes, str]:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"{path} is not a PNG")

    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    compression = 0
    filter_method = 0
    interlace = 0
    compressed = bytearray()
    offset = 8
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        kind = data[offset + 4 : offset + 8]
        payload_start = offset + 8
        payload_end = payload_start + length
        if payload_end + 4 > len(data):
            raise ValueError(f"{path} has a truncated PNG chunk")
        payload = data[payload_start:payload_end]
        offset = payload_end + 4

        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break

    if width <= 0 or height <= 0:
        raise ValueError(f"{path} is missing a valid PNG IHDR")
    if bit_depth != 8 or color_type not in {2, 6} or compression != 0 or filter_method != 0 or interlace != 0:
        raise ValueError(
            f"{path} uses unsupported PNG encoding: bit_depth={bit_depth} color_type={color_type} "
            f"compression={compression} filter={filter_method} interlace={interlace}"
        )

    bytes_per_pixel = 3 if color_type == 2 else 4
    raw = unfilter_png_scanlines(zlib.decompress(bytes(compressed)), width, height, bytes_per_pixel)
    if color_type == 2:
        rgb = raw
    else:
        rgb = bytearray(width * height * 3)
        for index in range(width * height):
            rgb[index * 3 + 0] = raw[index * 4 + 0]
            rgb[index * 3 + 1] = raw[index * 4 + 1]
            rgb[index * 3 + 2] = raw[index * 4 + 2]
        rgb = bytes(rgb)
    return width, height, rgb, "png"


def read_png_metrics(path: Path) -> dict:
    width, height, pixels, format_name = read_png_capture(path)
    return compute_rgb_metrics(width, height, pixels, format_name)


def collect_metrics(path: Path) -> dict:
    data = {
        "schema": "imm-render-metrics-v1",
        "path": path.as_posix(),
        "byte_size": path.stat().st_size,
        "sha256": sha256_file(path),
    }
    suffix = path.suffix.lower()
    if suffix == ".ppm":
        data.update(read_ppm_metrics(path))
    elif suffix == ".png":
        data.update(read_png_metrics(path))
    else:
        raise ValueError(f"Unsupported render capture format: {path.suffix}")
    return data


def compare_metrics(reference: dict, candidate: dict, allow_dimension_mismatch: bool = False) -> list[str]:
    errors = []
    if not allow_dimension_mismatch:
        for key in ["width", "height"]:
            if key in reference and key in candidate and reference[key] != candidate[key]:
                errors.append(f"{key} differs: reference={reference[key]!r} candidate={candidate[key]!r}")
    if candidate.get("format") in {"ppm-p6", "png"} and candidate.get("non_black_pixels", 0) <= 0:
        errors.append("candidate capture has no non-black pixels")
    if reference.get("format") in {"ppm-p6", "png"} and candidate.get("format") in {"ppm-p6", "png"}:
        ref_visible = reference.get("non_black_pixels", 0)
        cand_visible = candidate.get("non_black_pixels", 0)
        if ref_visible > 0 and not allow_dimension_mismatch:
            visible_ratio = cand_visible / ref_visible
            if visible_ratio < 0.65 or visible_ratio > 1.35:
                errors.append(f"visible pixel count drifted: reference={ref_visible} candidate={cand_visible} ratio={visible_ratio:.3f}")

        for axis in ["x_normalized", "y_normalized"]:
            ref_centroid = (reference.get("visible_centroid") or {}).get(axis)
            cand_centroid = (candidate.get("visible_centroid") or {}).get(axis)
            if ref_centroid is not None and cand_centroid is not None:
                diff = abs(cand_centroid - ref_centroid)
                if diff > 0.12:
                    errors.append(f"visible centroid {axis} drifted by {diff:.3f}: reference={ref_centroid:.3f} candidate={cand_centroid:.3f}")

        ref_profile = reference.get("vertical_luma_profile") or []
        cand_profile = candidate.get("vertical_luma_profile") or []
        if len(ref_profile) == len(cand_profile) and ref_profile:
            for index, (ref_value, cand_value) in enumerate(zip(ref_profile, cand_profile)):
                diff = abs(cand_value - ref_value)
                if diff > 0.18:
                    errors.append(f"vertical luma profile bin {index} drifted by {diff:.3f}: reference={ref_value:.3f} candidate={cand_value:.3f}")

        ref_quadrants = reference.get("quadrant_luma_share") or {}
        cand_quadrants = candidate.get("quadrant_luma_share") or {}
        for key, ref_value in ref_quadrants.items():
            if key in cand_quadrants:
                diff = abs(cand_quadrants[key] - ref_value)
                if diff > 0.20:
                    errors.append(f"quadrant luma share {key} drifted by {diff:.3f}: reference={ref_value:.3f} candidate={cand_quadrants[key]:.3f}")

        ref_luma = reference.get("visible_luma_mean", 0)
        cand_luma = candidate.get("visible_luma_mean", 0)
        if ref_luma > 0 and cand_luma > 0:
            gamma_ratio = cand_luma / ref_luma
            if gamma_ratio < 0.65 or gamma_ratio > 1.45:
                errors.append(f"visible luma mean drifted: reference={ref_luma:.3f} candidate={cand_luma:.3f} ratio={gamma_ratio:.3f}")

        ref_channels = reference.get("visible_channel_means") or {}
        cand_channels = candidate.get("visible_channel_means") or {}
        for channel in ["r", "g", "b"]:
            if channel in ref_channels and channel in cand_channels:
                diff = abs(cand_channels[channel] - ref_channels[channel])
                if diff > 35.0:
                    errors.append(f"visible {channel} channel mean drifted by {diff:.3f}: reference={ref_channels[channel]:.3f} candidate={cand_channels[channel]:.3f}")
    return errors


def formats_are_metric_equivalent(expected_format: object, candidate_format: object) -> bool:
    return {expected_format, candidate_format} <= {"ppm-p6", "png"}


def validate_contract(contract: dict, candidate: dict) -> list[str]:
    errors: list[str] = []
    validation = contract.get("validation", {})
    if not isinstance(validation, dict):
        return ["render baseline contract is missing a validation object"]

    expected_format = validation.get("format")
    if expected_format and candidate.get("format") != expected_format and not formats_are_metric_equivalent(expected_format, candidate.get("format")):
        errors.append(f"format differs from contract: expected={expected_format!r} candidate={candidate.get('format')!r}")

    dimensions = validation.get("requires_dimensions")
    if dimensions:
        for key in ["width", "height"]:
            if key in dimensions and candidate.get(key) != dimensions[key]:
                errors.append(f"{key} differs from contract: expected={dimensions[key]!r} candidate={candidate.get(key)!r}")

    minimum_dimensions = validation.get("minimum_dimensions")
    if minimum_dimensions:
        for key in ["width", "height"]:
            if key in minimum_dimensions and candidate.get(key, 0) < minimum_dimensions[key]:
                errors.append(
                    f"{key} is below contract minimum: minimum={minimum_dimensions[key]!r} candidate={candidate.get(key)!r}"
                )

    minimum_non_black = validation.get("minimum_non_black_pixels")
    if minimum_non_black is not None and candidate.get("non_black_pixels", 0) < minimum_non_black:
        errors.append(f"non_black_pixels {candidate.get('non_black_pixels', 0)} is below contract minimum {minimum_non_black}")

    minimum_near_visible = validation.get("minimum_near_visible_pixels")
    if minimum_near_visible is not None and candidate.get("near_visible_pixels", 0) < minimum_near_visible:
        errors.append(f"near_visible_pixels {candidate.get('near_visible_pixels', 0)} is below contract minimum {minimum_near_visible}")

    minimum_luma_span = validation.get("minimum_luma_span")
    if minimum_luma_span is not None and candidate.get("luma_span", 0) < minimum_luma_span:
        errors.append(f"luma_span {candidate.get('luma_span', 0)} is below contract minimum {minimum_luma_span}")

    if validation.get("requires_orientation_metrics"):
        if not candidate.get("visible_centroid"):
            errors.append("candidate is missing visible centroid orientation metrics")
        if len(candidate.get("vertical_luma_profile") or []) < 5:
            errors.append("candidate is missing vertical luma profile orientation metrics")
        if set(candidate.get("quadrant_luma_share") or {}) != {"top_left", "top_right", "bottom_left", "bottom_right"}:
            errors.append("candidate is missing complete quadrant luma orientation metrics")

    if validation.get("requires_color_metrics"):
        if not candidate.get("visible_channel_means"):
            errors.append("candidate is missing visible channel means")
        if candidate.get("visible_luma_mean", 0) <= 0:
            errors.append("candidate visible luma mean must be positive")

    expected_centroid = validation.get("expected_visible_centroid_normalized")
    if isinstance(expected_centroid, dict):
        centroid = candidate.get("visible_centroid") or {}
        for axis in ["x", "y"]:
            axis_key = f"{axis}_normalized"
            expected_axis = expected_centroid.get(axis)
            actual = centroid.get(axis_key)
            if isinstance(expected_axis, dict) and actual is not None:
                minimum = expected_axis.get("min")
                maximum = expected_axis.get("max")
                if minimum is not None and actual < minimum:
                    errors.append(f"visible centroid {axis_key} {actual:.3f} is below contract minimum {minimum:.3f}")
                if maximum is not None and actual > maximum:
                    errors.append(f"visible centroid {axis_key} {actual:.3f} is above contract maximum {maximum:.3f}")

    expected_profile = validation.get("expected_vertical_luma_profile")
    if isinstance(expected_profile, dict):
        values = expected_profile.get("values")
        tolerance = float(expected_profile.get("tolerance", 0))
        actual_profile = candidate.get("vertical_luma_profile") or []
        if isinstance(values, list):
            if len(actual_profile) != len(values):
                errors.append(f"vertical luma profile has {len(actual_profile)} bins, expected {len(values)}")
            else:
                for index, expected_value in enumerate(values):
                    actual = actual_profile[index]
                    diff = abs(actual - float(expected_value))
                    if diff > tolerance:
                        errors.append(
                            f"vertical luma profile bin {index} differs by {diff:.3f}: "
                            f"expected={float(expected_value):.3f} candidate={actual:.3f}"
                        )

    expected_quadrants = validation.get("expected_quadrant_luma_share")
    if isinstance(expected_quadrants, dict):
        tolerance = float(expected_quadrants.get("tolerance", 0))
        values = expected_quadrants.get("values")
        actual_quadrants = candidate.get("quadrant_luma_share") or {}
        if isinstance(values, dict):
            for key, expected_value in values.items():
                if key not in actual_quadrants:
                    errors.append(f"quadrant luma share is missing {key}")
                    continue
                actual = actual_quadrants[key]
                diff = abs(actual - float(expected_value))
                if diff > tolerance:
                    errors.append(
                        f"quadrant luma share {key} differs by {diff:.3f}: "
                        f"expected={float(expected_value):.3f} candidate={actual:.3f}"
                    )

    expected_channels = validation.get("expected_visible_channel_means")
    if isinstance(expected_channels, dict):
        tolerance = float(expected_channels.get("tolerance", 0))
        values = expected_channels.get("values")
        actual_channels = candidate.get("visible_channel_means") or {}
        if isinstance(values, dict):
            for channel, expected_value in values.items():
                if channel not in actual_channels:
                    errors.append(f"visible channel means is missing {channel}")
                    continue
                actual = actual_channels[channel]
                diff = abs(actual - float(expected_value))
                if diff > tolerance:
                    errors.append(
                        f"visible {channel} channel mean differs by {diff:.3f}: "
                        f"expected={float(expected_value):.3f} candidate={actual:.3f}"
                    )

    expected_luma = validation.get("expected_visible_luma_mean")
    if isinstance(expected_luma, dict):
        actual = candidate.get("visible_luma_mean", 0)
        minimum = expected_luma.get("min")
        maximum = expected_luma.get("max")
        if minimum is not None and actual < minimum:
            errors.append(f"visible luma mean {actual:.3f} is below contract minimum {minimum:.3f}")
        if maximum is not None and actual > maximum:
            errors.append(f"visible luma mean {actual:.3f} is above contract maximum {maximum:.3f}")

    expected_luma_stddev = validation.get("expected_luma_stddev")
    if isinstance(expected_luma_stddev, dict):
        actual = candidate.get("luma_stddev", 0)
        minimum = expected_luma_stddev.get("min")
        maximum = expected_luma_stddev.get("max")
        if minimum is not None and actual < minimum:
            errors.append(f"luma stddev {actual:.3f} is below contract minimum {minimum:.3f}")
        if maximum is not None and actual > maximum:
            errors.append(f"luma stddev {actual:.3f} is above contract maximum {maximum:.3f}")

    expected_percentiles = validation.get("expected_luma_percentiles")
    if isinstance(expected_percentiles, dict):
        actual_percentiles = candidate.get("luma_percentiles") or {}
        for key, expected in expected_percentiles.items():
            actual = actual_percentiles.get(key)
            if actual is None:
                errors.append(f"luma percentile {key} is missing")
                continue
            if not isinstance(expected, dict):
                continue
            target = expected.get("value")
            tolerance = expected.get("tolerance")
            if target is not None and tolerance is not None:
                diff = abs(actual - float(target))
                if diff > float(tolerance):
                    errors.append(
                        f"luma percentile {key} differs by {diff:.3f}: "
                        f"expected={float(target):.3f} candidate={actual:.3f}"
                    )
            minimum = expected.get("min")
            maximum = expected.get("max")
            if minimum is not None and actual < minimum:
                errors.append(f"luma percentile {key} {actual:.3f} is below contract minimum {float(minimum):.3f}")
            if maximum is not None and actual > maximum:
                errors.append(f"luma percentile {key} {actual:.3f} is above contract maximum {float(maximum):.3f}")

    expected_visible_percentiles = validation.get("expected_visible_luma_percentiles")
    if isinstance(expected_visible_percentiles, dict):
        actual_percentiles = candidate.get("visible_luma_percentiles") or {}
        for key, expected in expected_visible_percentiles.items():
            actual = actual_percentiles.get(key)
            if actual is None:
                errors.append(f"visible luma percentile {key} is missing")
                continue
            if not isinstance(expected, dict):
                continue
            target = expected.get("value")
            tolerance = expected.get("tolerance")
            if target is not None and tolerance is not None:
                diff = abs(actual - float(target))
                if diff > float(tolerance):
                    errors.append(
                        f"visible luma percentile {key} differs by {diff:.3f}: "
                        f"expected={float(target):.3f} candidate={actual:.3f}"
                    )
            minimum = expected.get("min")
            maximum = expected.get("max")
            if minimum is not None and actual < minimum:
                errors.append(f"visible luma percentile {key} {actual:.3f} is below contract minimum {float(minimum):.3f}")
            if maximum is not None and actual > maximum:
                errors.append(f"visible luma percentile {key} {actual:.3f} is above contract maximum {float(maximum):.3f}")

    return errors


def validate_spatial_contract(contract: dict, spatial_metrics: dict | None) -> list[str]:
    validation = contract.get("validation", {})
    if not isinstance(validation, dict):
        return []
    expected_grid = validation.get("expected_spatial_luma_grid")
    if not isinstance(expected_grid, dict):
        return []
    if not spatial_metrics:
        return ["spatial luma grid comparison requires a reference capture"]
    if spatial_metrics.get("error"):
        return [f"spatial luma grid comparison failed: {spatial_metrics['error']}"]

    errors: list[str] = []
    mean_abs_delta = spatial_metrics.get("mean_abs_delta")
    max_mean_abs_delta = expected_grid.get("max_mean_abs_delta")
    if mean_abs_delta is None:
        errors.append("spatial luma grid mean_abs_delta is missing")
    elif max_mean_abs_delta is not None and mean_abs_delta > float(max_mean_abs_delta):
        errors.append(
            f"spatial luma grid mean_abs_delta {mean_abs_delta:.3f} exceeds contract maximum {float(max_mean_abs_delta):.3f}"
        )

    correlation = spatial_metrics.get("correlation")
    min_correlation = expected_grid.get("min_correlation")
    if correlation is None:
        errors.append("spatial luma grid correlation is missing")
    elif min_correlation is not None and correlation < float(min_correlation):
        errors.append(
            f"spatial luma grid correlation {correlation:.3f} is below contract minimum {float(min_correlation):.3f}"
        )
    return errors


def collect_color_component_metrics(candidate_path: Path, specification: dict) -> dict:
    width, height, pixels, _ = read_rgb_capture(candidate_path)
    crop_aspect_ratio = specification.get("center_crop_aspect_ratio")
    crop = center_crop_region(
        width,
        height,
        float(crop_aspect_ratio) if crop_aspect_ratio is not None else None,
    )
    crop_x, crop_y, crop_width, crop_height = crop
    crop_pixels = crop_width * crop_height
    probe_results: list[dict] = []

    for probe in specification.get("probes", []):
        name = str(probe.get("name", "unnamed"))
        target = probe.get("target_rgb")
        if not isinstance(target, list) or len(target) != 3:
            raise ValueError(f"color component probe {name!r} requires a three-channel target_rgb")
        target_rgb = tuple(int(channel) for channel in target)
        tolerance = int(probe.get("channel_tolerance", 0))
        region = probe.get("region_normalized", {})
        if not isinstance(region, dict):
            raise ValueError(f"color component probe {name!r} requires region_normalized")
        normalized_x = float(region.get("x", 0.0))
        normalized_y = float(region.get("y", 0.0))
        normalized_width = float(region.get("width", 1.0))
        normalized_height = float(region.get("height", 1.0))
        if (
            normalized_width <= 0.0
            or normalized_height <= 0.0
            or normalized_x < 0.0
            or normalized_y < 0.0
            or normalized_x + normalized_width > 1.0
            or normalized_y + normalized_height > 1.0
        ):
            raise ValueError(f"color component probe {name!r} has an invalid normalized region")

        region_x = crop_x + round(normalized_x * crop_width)
        region_y = crop_y + round(normalized_y * crop_height)
        region_width = max(1, round(normalized_width * crop_width))
        region_height = max(1, round(normalized_height * crop_height))
        region_width = min(region_width, crop_x + crop_width - region_x)
        region_height = min(region_height, crop_y + crop_height - region_y)
        matches = bytearray(region_width * region_height)
        matched_pixels = 0
        for local_y in range(region_height):
            source_row = (region_y + local_y) * width
            match_row = local_y * region_width
            for local_x in range(region_width):
                source_index = (source_row + region_x + local_x) * 3
                r, g, b = pixels[source_index : source_index + 3]
                if (
                    abs(r - target_rgb[0]) <= tolerance
                    and abs(g - target_rgb[1]) <= tolerance
                    and abs(b - target_rgb[2]) <= tolerance
                ):
                    matches[match_row + local_x] = 1
                    matched_pixels += 1

        largest_component = 0
        largest_bounds: dict | None = None
        for start in range(len(matches)):
            if not matches[start]:
                continue
            matches[start] = 0
            stack = [start]
            component_size = 0
            min_x = region_width
            min_y = region_height
            max_x = -1
            max_y = -1
            while stack:
                index = stack.pop()
                local_y, local_x = divmod(index, region_width)
                component_size += 1
                min_x = min(min_x, local_x)
                min_y = min(min_y, local_y)
                max_x = max(max_x, local_x)
                max_y = max(max_y, local_y)
                if local_x > 0 and matches[index - 1]:
                    matches[index - 1] = 0
                    stack.append(index - 1)
                if local_x + 1 < region_width and matches[index + 1]:
                    matches[index + 1] = 0
                    stack.append(index + 1)
                if local_y > 0 and matches[index - region_width]:
                    matches[index - region_width] = 0
                    stack.append(index - region_width)
                if local_y + 1 < region_height and matches[index + region_width]:
                    matches[index + region_width] = 0
                    stack.append(index + region_width)
            if component_size > largest_component:
                largest_component = component_size
                largest_bounds = {
                    "x": region_x + min_x,
                    "y": region_y + min_y,
                    "width": max_x - min_x + 1,
                    "height": max_y - min_y + 1,
                }

        probe_results.append(
            {
                "name": name,
                "target_rgb": list(target_rgb),
                "channel_tolerance": tolerance,
                "region": {
                    "x": region_x,
                    "y": region_y,
                    "width": region_width,
                    "height": region_height,
                },
                "matched_pixels": matched_pixels,
                "largest_component_pixels": largest_component,
                "largest_component_share_of_crop": largest_component / crop_pixels if crop_pixels else 0.0,
                "largest_component_bounds": largest_bounds,
            }
        )

    return {
        "center_crop": {
            "x": crop_x,
            "y": crop_y,
            "width": crop_width,
            "height": crop_height,
        },
        "probes": probe_results,
    }


def validate_color_component_contract(specification: dict, metrics: dict) -> list[str]:
    errors: list[str] = []
    expected_probes = specification.get("probes", [])
    actual_probes = metrics.get("probes", [])
    for expected, actual in zip(expected_probes, actual_probes):
        name = actual["name"]
        share = float(actual["largest_component_share_of_crop"])
        minimum = expected.get("minimum_largest_component_share_of_crop")
        maximum = expected.get("maximum_largest_component_share_of_crop")
        if minimum is not None and share < float(minimum):
            errors.append(
                f"color component probe {name} share {share:.6f} is below contract minimum {float(minimum):.6f}"
            )
        if maximum is not None and share > float(maximum):
            errors.append(
                f"color component probe {name} share {share:.6f} exceeds contract maximum {float(maximum):.6f}"
            )
    if len(actual_probes) != len(expected_probes):
        errors.append(
            f"color component probe result count {len(actual_probes)} differs from contract count {len(expected_probes)}"
        )
    return errors


def evaluate_capture(candidate_path: Path, reference_path: Path | None, contract_path: Path | None) -> dict:
    contract = json.loads(contract_path.read_text(encoding="utf-8")) if contract_path else {}
    validation = contract.get("validation", {}) if isinstance(contract, dict) else {}
    allow_dimension_mismatch = bool(
        isinstance(validation, dict) and validation.get("allow_reference_dimension_mismatch")
    )
    candidate = collect_metrics(candidate_path)
    output = {"candidate": candidate}
    errors: list[str] = []
    spatial_metrics = None
    if reference_path:
        reference = collect_metrics(reference_path)
        output["reference"] = reference
        errors = compare_metrics(reference, candidate, allow_dimension_mismatch)
    if contract_path:
        output["contract"] = {
            "path": contract_path.as_posix(),
            "schema": contract.get("schema"),
            "baseline": contract.get("baseline"),
        }
        expected_grid = validation.get("expected_spatial_luma_grid") if isinstance(validation, dict) else None
        if isinstance(expected_grid, dict):
            grid_width = int(expected_grid.get("width", 32))
            grid_height = int(expected_grid.get("height", 18))
            crop_aspect_ratio = expected_grid.get("center_crop_aspect_ratio")
            spatial_metrics = (
                collect_spatial_metrics(
                    reference_path,
                    candidate_path,
                    grid_width,
                    grid_height,
                    float(crop_aspect_ratio) if crop_aspect_ratio is not None else None,
                )
                if reference_path
                else None
            )
            output["spatial_luma_grid"] = spatial_metrics
        errors.extend(validate_contract(contract, candidate))
        errors.extend(validate_spatial_contract(contract, spatial_metrics))
        color_component_specification = (
            validation.get("expected_color_components") if isinstance(validation, dict) else None
        )
        if isinstance(color_component_specification, dict):
            color_component_metrics = collect_color_component_metrics(candidate_path, color_component_specification)
            output["color_component_probes"] = color_component_metrics
            errors.extend(
                validate_color_component_contract(color_component_specification, color_component_metrics)
            )
    output["passed"] = not errors
    output["errors"] = errors
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--json-output", type=Path, required=True)
    args = parser.parse_args()

    output = evaluate_capture(args.candidate, args.reference, args.contract)
    errors = output["errors"]

    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print(f"Render metrics written: {args.json_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
