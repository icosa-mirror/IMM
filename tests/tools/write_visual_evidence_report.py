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


def artifact_name(path: Path, input_root: Path) -> str:
    try:
        rel = path.relative_to(input_root)
    except ValueError:
        return path.name
    return rel.parts[0] if rel.parts else path.name


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

    artifact_dirs = sorted(path for path in input_root.iterdir() if path.is_dir()) if input_root.exists() else []
    lines = [
        "# IMM Engine Visual Evidence",
        "",
        "This artifact contains the human-readable render evidence and the images used by the CI validation.",
        "",
    ]

    visual_sections = 0
    for artifact_dir in artifact_dirs:
        reports = sorted(artifact_dir.rglob("render-report.md"))
        images = sorted(path for path in artifact_dir.rglob("*") if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES)
        if not reports and not images:
            continue

        visual_sections += 1
        name = artifact_name(artifact_dir, input_root)
        lines.append(f"## {name}")
        lines.append("")

        for report in reports:
            copied_report = output_dir / f"{name}-{report.name}"
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
