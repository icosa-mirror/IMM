#!/usr/bin/env python3
"""Build a compact visual evidence artifact from downloaded CI lane artifacts."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp"}


def unique_destination(base: Path, source: Path) -> Path:
    destination = base / source.name
    if not destination.exists():
        return destination

    stem = source.stem
    suffix = source.suffix
    index = 2
    while True:
        candidate = base / f"{stem}-{index}{suffix}"
        if not candidate.exists():
            return candidate
        index += 1


def copy_image(source: Path, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    destination = unique_destination(output_dir, source)
    shutil.copy2(source, destination)
    return destination


def relative_link(target: Path, report_path: Path) -> str:
    return target.resolve().relative_to(report_path.parent.resolve()).as_posix()


def display_name(path: Path) -> str:
    return path.name.replace("-", " ").replace("_", " ").title()


def discover_visual_roots(input_root: Path) -> list[Path]:
    report_roots = {path.parent for path in input_root.rglob("render-report.md")}
    roots = sorted(report_roots)
    if roots:
        return roots
    return sorted(
        path
        for path in input_root.rglob("*")
        if path.is_dir() and any(child.is_file() and child.suffix.lower() in IMAGE_SUFFIXES for child in path.iterdir())
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    args = parser.parse_args()

    input_root = args.input_root.resolve()
    output_dir = args.output_dir.resolve()
    report_path = args.markdown_output.resolve()
    capture_output_dir = output_dir / "captures"
    output_dir.mkdir(parents=True, exist_ok=True)

    lines = [
        "# IMM CI Validation Evidence",
        "",
        "This artifact contains the human-readable render evidence and the images used by the CI validation.",
        "",
    ]

    visual_sections = 0
    for visual_root in discover_visual_roots(input_root):
        reports = sorted(visual_root.glob("render-report.md"))
        images = sorted(path for path in visual_root.rglob("*") if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES)
        if not reports and not images:
            continue

        visual_sections += 1
        name = display_name(visual_root)
        lines.append(f"## {name}")
        lines.append("")

        for report in reports:
            copied_report = output_dir / f"{visual_root.name}-{report.name}"
            shutil.copy2(report, copied_report)
            lines.append(f"- Report: [{copied_report.name}]({relative_link(copied_report, report_path)})")
        if reports:
            lines.append("")

        for image in images:
            copied_image = copy_image(image, capture_output_dir / name)
            lines.append(f"### {image.name}")
            lines.append(f"![{image.name}]({relative_link(copied_image, report_path)})")
            lines.append("")

    if visual_sections == 0:
        lines.append("No visual render evidence was found in the downloaded artifacts.")
        lines.append("")

    report_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8", newline="\n")
    print(f"Visual evidence report written: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
