#!/usr/bin/env python3
"""Build a compact visual evidence artifact from downloaded CI lane artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp"}
REPORT_SUFFIX = "-render-report.md"


def normalize_label(value: str) -> str:
    normalized = value
    replacements = {
        "AndroidGodot": "Android-Godot",
        "AndroidStandalone": "Android-Standalone",
        "MacosGodot": "macOS-Godot",
        "macOSGodot": "macOS-Godot",
        "WindowsGodot": "Windows-Godot",
        "WindowsStandalone": "Windows-Standalone",
        "godot-smoke-macos-metal": "macOS-Godot-Metal",
        "godot-smoke-windows-vulkan": "Windows-Godot-Vulkan",
        "windows-standalone-directx": "Windows-Standalone-DirectX",
        "windows-standalone-opengl": "Windows-Standalone-OpenGL",
        "windows-standalone-vulkan": "Windows-Standalone-Vulkan",
        "unity-windows-directx-composition": "Unity-Windows-DirectX-Composition",
    }
    for old, new in replacements.items():
        normalized = normalized.replace(old, new)
    normalized = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "-", normalized)
    return normalized


def slugify(value: str) -> str:
    compact = re.sub(r"[^a-z0-9]+", "", value.lower())
    suffix = ""
    for candidate_suffix in ["device", "gpu"]:
        if compact.endswith(candidate_suffix):
            suffix = f"-{candidate_suffix}"
            compact = compact[: -len(candidate_suffix)]
            break
    compact_replacements = {
        "androidgodotvulkan": "android-godot-vulkan",
        "androidstandalonegles": "android-standalone-gles",
        "androidstandalonevulkan": "android-standalone-vulkan",
        "macosgodotmetal": "macos-godot-metal",
        "unitywindowsdirectxcomposition": "unity-windows-directx-composition",
        "windowsgodotvulkan": "windows-godot-vulkan",
        "windowsstandalonedirectx": "windows-standalone-directx",
        "windowsstandaloneopengl": "windows-standalone-opengl",
        "windowsstandalonevulkan": "windows-standalone-vulkan",
        "androidgodot": "android-godot",
        "androidstandalone": "android-standalone",
        "macosgodot": "macos-godot",
        "windowsgodot": "windows-godot",
        "windowsstandalone": "windows-standalone",
    }
    for old, new in compact_replacements.items():
        compact = compact.replace(old, new)
    slug = re.sub(r"[^A-Za-z0-9]+", "-", normalize_label(compact)).strip("-").lower()
    slug = slug.replace("direct-x", "directx").replace("open-gl", "opengl")
    return f"{slug}{suffix}" if slug else f"evidence{suffix}"


def file_slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "-", value).strip("-").lower() or "capture"


def unique_destination(base: Path, source_name: str) -> Path:
    destination = base / source_name
    if not destination.exists():
        return destination

    source_path = Path(source_name)
    stem = source_path.stem
    suffix = source_path.suffix
    index = 2
    while True:
        candidate = base / f"{stem}-{index}{suffix}"
        if not candidate.exists():
            return candidate
        index += 1


def copy_image(source: Path, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    destination = unique_destination(output_dir, f"{file_slug(source.stem)}{source.suffix.lower()}")
    shutil.copy2(source, destination)
    return destination


def relative_link(target: Path, report_path: Path) -> str:
    return target.resolve().relative_to(report_path.parent.resolve()).as_posix()


def display_name(key: str) -> str:
    key = slugify(key).replace("direct-x", "directx").replace("open-gl", "opengl")
    replacements = {
        "android": "Android",
        "directx": "DirectX",
        "gles": "GLES",
        "godot": "Godot",
        "gpu": "GPU",
        "ios": "iOS",
        "macos": "macOS",
        "metal": "Metal",
        "opengl": "OpenGL",
        "unity": "Unity",
        "vulkan": "Vulkan",
        "windows": "Windows",
    }
    words = re.split(r"[-_\s]+", key)
    return " ".join(replacements.get(word.lower(), word.capitalize()) for word in words if word)


def report_key(report: Path) -> str:
    if report.name == "render-report.md":
        return slugify(report.parent.name)
    if report.name.endswith(REPORT_SUFFIX):
        return slugify(report.name[: -len(REPORT_SUFFIX)])
    return slugify(report.stem)


def report_rank(report: Path) -> tuple[int, int]:
    parts = {part.lower() for part in report.parts}
    copied_report_penalty = 1 if report.name != "render-report.md" else 0
    aggregate_penalty = 1 if any(part.endswith("evidence") for part in parts) else 0
    return (aggregate_penalty + copied_report_penalty, len(report.parts))


def discover_reports(input_root: Path) -> list[tuple[str, Path]]:
    reports = list(input_root.rglob("render-report.md")) + [
        path for path in input_root.rglob(f"*{REPORT_SUFFIX}") if path.name != "render-report.md"
    ]
    selected: dict[str, Path] = {}
    for report in reports:
        key = report_key(report)
        current = selected.get(key)
        if current is None or report_rank(report) < report_rank(current):
            selected[key] = report
    return sorted(selected.items(), key=lambda item: display_name(item[0]))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_roots_for_report(report: Path, key: str) -> list[Path]:
    captures = report.parent / "captures"
    if captures.exists():
        matching = [path for path in captures.iterdir() if path.is_dir() and slugify(path.name) == key]
        if matching:
            return matching
        if any(path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES for path in captures.iterdir()):
            return [captures]
        if report.name != "render-report.md":
            return []
    return [report.parent]


def find_images_for_report(report: Path, key: str) -> list[Path]:
    candidates = [
        path
        for root in image_roots_for_report(report, key)
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
    ]
    seen: set[str] = set()
    unique: list[Path] = []
    for image in sorted(candidates):
        digest = sha256_file(image)
        if digest in seen:
            continue
        seen.add(digest)
        unique.append(image)
    return unique


def find_json(root: Path, *names: str) -> dict:
    for name in names:
        path = root / name
        if path.exists():
            return json.loads(path.read_text(encoding="utf-8"))
    return {}


def find_metrics(root: Path) -> dict:
    for path in sorted(root.glob("*metrics*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(data, dict) and "candidate" in data:
            return data
    return {}


def effective_status(metrics: dict, status: dict, manifest: dict) -> tuple[str, str]:
    errors = metrics.get("errors") or []
    if errors or metrics.get("passed") is False:
        return ("failed", "rendering")
    classification = manifest.get("classification") or {}
    result = classification.get("result")
    failure_class = classification.get("failure_class", "")
    if result in {"failed", "failure", "cancelled"}:
        return ("failed", failure_class)
    if result == "expected_failed":
        return ("expected failure", failure_class)
    if status.get("compositing") == "expected_failed":
        return ("expected failure", "compositing")
    if status.get("rendering") == "success" or metrics.get("passed") is True:
        return ("passed", "")
    return ("unknown", "")


def metric_value(metrics: dict, section: str, key: str) -> str:
    value = metrics.get(section, {}).get(key)
    if isinstance(value, float):
        return f"{value:.3f}"
    if value is None:
        return ""
    return str(value)


def add_metrics_table(lines: list[str], metrics: dict) -> None:
    if not metrics:
        return
    rows = [
        ("width", metric_value(metrics, "reference", "width"), metric_value(metrics, "candidate", "width")),
        ("height", metric_value(metrics, "reference", "height"), metric_value(metrics, "candidate", "height")),
        ("non_black_pixels", metric_value(metrics, "reference", "non_black_pixels"), metric_value(metrics, "candidate", "non_black_pixels")),
        ("near_visible_pixels", metric_value(metrics, "reference", "near_visible_pixels"), metric_value(metrics, "candidate", "near_visible_pixels")),
        ("visible_luma_mean", metric_value(metrics, "reference", "visible_luma_mean"), metric_value(metrics, "candidate", "visible_luma_mean")),
        ("visible_chroma_mean", metric_value(metrics, "reference", "visible_chroma_mean"), metric_value(metrics, "candidate", "visible_chroma_mean")),
    ]
    lines.append("| Metric | Reference | Candidate |")
    lines.append("| --- | --- | --- |")
    for name, reference, candidate in rows:
        lines.append(f"| {name} | {reference} | {candidate} |")
    lines.append("")


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
        "This is the report to read first. It summarizes render correctness, known composition failures, and the captured images used as evidence.",
        "",
    ]

    visual_sections = 0
    for key, report in discover_reports(input_root):
        metrics = find_metrics(report.parent)
        status = find_json(report.parent, "composition-status.json")
        manifest = find_json(report.parent, "manifest.json")
        images = find_images_for_report(report, key)
        if not metrics and not status and not manifest and not images:
            continue

        visual_sections += 1
        name = display_name(key)
        section_slug = slugify(key)
        result, failure_class = effective_status(metrics, status, manifest)
        lines.append(f"## {name}")
        lines.append("")
        lines.append(f"- Result: {result}")
        if failure_class:
            lines.append(f"- Failure class: {failure_class}")
        classification = manifest.get("classification") or {}
        if classification:
            lines.append(f"- Lane status: {classification.get('result', '')}")
        if status:
            lines.append(f"- Rendering: {status.get('rendering', '')}")
            lines.append(f"- Compositing: {status.get('compositing', '')}")
        lines.append("")

        errors = metrics.get("errors") or []
        failures = status.get("failures") or []
        if errors:
            lines.append("### Render Errors")
            lines.extend(f"- {error}" for error in errors)
            lines.append("")
        if failures:
            lines.append("### Composition Failures")
            lines.extend(f"- {failure}" for failure in failures)
            lines.append("")

        add_metrics_table(lines, metrics)

        for image in images:
            copied_image = copy_image(image, capture_output_dir / section_slug)
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
