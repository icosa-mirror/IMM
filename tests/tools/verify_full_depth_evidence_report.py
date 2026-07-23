#!/usr/bin/env python3
"""Verify the final validation report contains full-depth compositor evidence."""

from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp"}
MIN_IMAGE_WIDTH = 320
MIN_IMAGE_HEIGHT = 180

REQUIRED_EVIDENCE = {
    "unity": {
        "label": "Unity Windows Vulkan full depth",
        "slugs": ["unity-windows-vulkan-full-depth"],
    },
    "godot": {
        "label": "Godot Windows Vulkan full depth",
        "slugs": ["windows-godot-vulkan", "godot-windows-vulkan-full-depth"],
    },
}


def load_status(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"could not read {path}: {exc}") from exc


def report_references_slug(report_text: str, slug: str) -> bool:
    pattern = re.compile(rf"!\[[^\]]*\]\(captures/{re.escape(slug)}/[^)]+\)")
    return bool(pattern.search(report_text))


def image_dimensions(path: Path) -> tuple[int, int] | None:
    suffix = path.suffix.lower()
    if suffix == ".png":
        data = path.read_bytes()
        if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
            return None
        return struct.unpack(">II", data[16:24])
    return None


def verify_requirement(report_path: Path, captures_root: Path, report_text: str, key: str, requirement: dict) -> list[str]:
    errors: list[str] = []
    matching_dir: Path | None = None
    matching_slug = ""
    for slug in requirement["slugs"]:
        candidate = captures_root / slug
        if candidate.exists():
            matching_dir = candidate
            matching_slug = slug
            break

    if matching_dir is None:
        accepted = ", ".join(requirement["slugs"])
        return [f"{requirement['label']}: missing captures directory; accepted slugs: {accepted}"]

    status_path = matching_dir / "composition-status.json"
    if not status_path.exists():
        errors.append(f"{requirement['label']}: missing composition-status.json in {matching_dir}")
    else:
        status = load_status(status_path)
        expected = {
            "composition_mode": "full_depth",
            "depth_composition": "success",
            "depth_interleaving": "success",
        }
        for field, value in expected.items():
            if status.get(field) != value:
                errors.append(
                    f"{requirement['label']}: expected {field}={value}, got {status.get(field)!r}"
                )

    images = [
        path
        for path in matching_dir.iterdir()
        if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
    ]
    if not images:
        errors.append(f"{requirement['label']}: missing viewable image in {matching_dir}")
    for image in images:
        dimensions = image_dimensions(image)
        if dimensions is None:
            continue
        width, height = dimensions
        if width < MIN_IMAGE_WIDTH or height < MIN_IMAGE_HEIGHT:
            errors.append(
                f"{requirement['label']}: image {image} is too small for visual evidence: {width}x{height}"
            )

    if not report_references_slug(report_text, matching_slug):
        errors.append(f"{requirement['label']}: {report_path} does not embed an image from captures/{matching_slug}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--captures-root", type=Path)
    args = parser.parse_args()

    report_path = args.report.resolve()
    captures_root = (args.captures_root or report_path.parent / "captures").resolve()
    report_text = report_path.read_text(encoding="utf-8")

    errors: list[str] = []
    for key, requirement in REQUIRED_EVIDENCE.items():
        errors.extend(verify_requirement(report_path, captures_root, report_text, key, requirement))

    if errors:
        for error in errors:
            print(error)
        return 1

    print("Full-depth evidence report verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
