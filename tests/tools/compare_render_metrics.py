#!/usr/bin/env python3
"""Collect and compare lightweight render capture metrics."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_ppm_metrics(path: Path) -> dict:
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

    for index in range(0, len(pixels), 3):
        r, g, b = pixels[index], pixels[index + 1], pixels[index + 2]
        luma = (r * 299 + g * 587 + b * 114) // 1000
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
        "format": "ppm-p6",
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
        "visible_luma_mean": visible_luma_mean,
        "visible_chroma_mean": visible_chroma_mean,
        "visible_centroid": visible_centroid,
        "visible_channel_means": visible_channel_means,
        "quadrants": quadrants,
        "quadrant_luma_share": quadrant_luma_share,
        "vertical_luma_profile": vertical_luma_profile,
    }


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
        with path.open("rb") as handle:
            signature = handle.read(8)
        if signature != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"{path} is not a PNG")
        data["format"] = "png"
        data["note"] = "PNG identity recorded; install a richer image dependency if pixel metrics are required."
    else:
        raise ValueError(f"Unsupported render capture format: {path.suffix}")
    return data


def compare_metrics(reference: dict, candidate: dict) -> list[str]:
    errors = []
    for key in ["format", "width", "height"]:
        if key in reference and key in candidate and reference[key] != candidate[key]:
            errors.append(f"{key} differs: reference={reference[key]!r} candidate={candidate[key]!r}")
    if candidate.get("format") == "ppm-p6" and candidate.get("non_black_pixels", 0) <= 0:
        errors.append("candidate capture has no non-black pixels")
    if reference.get("format") == "ppm-p6" and candidate.get("format") == "ppm-p6":
        ref_visible = reference.get("non_black_pixels", 0)
        cand_visible = candidate.get("non_black_pixels", 0)
        if ref_visible > 0:
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


def validate_contract(contract: dict, candidate: dict) -> list[str]:
    errors: list[str] = []
    validation = contract.get("validation", {})
    if not isinstance(validation, dict):
        return ["render baseline contract is missing a validation object"]

    expected_format = validation.get("format")
    if expected_format and candidate.get("format") != expected_format:
        errors.append(f"format differs from contract: expected={expected_format!r} candidate={candidate.get('format')!r}")

    dimensions = validation.get("requires_dimensions")
    if dimensions:
        for key in ["width", "height"]:
            if key in dimensions and candidate.get(key) != dimensions[key]:
                errors.append(f"{key} differs from contract: expected={dimensions[key]!r} candidate={candidate.get(key)!r}")

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

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--json-output", type=Path, required=True)
    args = parser.parse_args()

    candidate = collect_metrics(args.candidate)
    output = {"candidate": candidate}
    errors: list[str] = []
    if args.reference:
        reference = collect_metrics(args.reference)
        output["reference"] = reference
        errors = compare_metrics(reference, candidate)
    if args.contract:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        output["contract"] = {"path": args.contract.as_posix(), "schema": contract.get("schema"), "baseline": contract.get("baseline")}
        errors.extend(validate_contract(contract, candidate))
    output["passed"] = not errors
    output["errors"] = errors

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
