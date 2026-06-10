#!/usr/bin/env python3
"""Collect and compare lightweight render capture metrics."""

from __future__ import annotations

import argparse
import hashlib
import json
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
    min_luma = 255
    max_luma = 0
    min_x = width
    min_y = height
    max_x = -1
    max_y = -1

    for index in range(0, len(pixels), 3):
        r, g, b = pixels[index], pixels[index + 1], pixels[index + 2]
        luma = (r * 299 + g * 587 + b * 114) // 1000
        min_luma = min(min_luma, luma)
        max_luma = max(max_luma, luma)
        if r or g or b:
            pixel_index = index // 3
            x = pixel_index % width
            y = pixel_index // width
            non_black += 1
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)

    bounds = None
    if non_black:
        bounds = {"x": min_x, "y": min_y, "width": max_x - min_x + 1, "height": max_y - min_y + 1}

    return {
        "format": "ppm-p6",
        "width": width,
        "height": height,
        "non_black_pixels": non_black,
        "content_bounds": bounds,
        "min_luma": min_luma,
        "max_luma": max_luma,
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
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--json-output", type=Path, required=True)
    args = parser.parse_args()

    candidate = collect_metrics(args.candidate)
    output = {"candidate": candidate}
    errors: list[str] = []
    if args.reference:
        reference = collect_metrics(args.reference)
        output["reference"] = reference
        errors = compare_metrics(reference, candidate)
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
