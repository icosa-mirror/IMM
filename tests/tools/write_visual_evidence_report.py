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
VISUAL_RENDERERS = {"directx", "gles", "metal", "opengl", "vulkan", "webgl"}
REPORT_SUFFIX = "-render-report.md"
AGGREGATE_REPORT_NAMES = {
    "VALIDATION_REPORT.md",
    "ENGINE_VALIDATION_REPORT.md",
    "DEVICE_VALIDATION_REPORT.md",
    "GPU_VALIDATION_REPORT.md",
    "CORE_VALIDATION_REPORT.md",
}
GENERIC_CAPTURE_SECTION_KEYS = {
    "composition",
    "externalrender",
    "fulldepth",
    "orderedoverlay",
    "render",
    "syntheticstereo",
}
VISUAL_MATRIX_TARGETS = {
    "windows": ("DX11", "directx"),
    "android": ("Vulkan", "vulkan"),
    "macos": ("Metal", "metal"),
    "ios": ("Metal", "metal"),
}
VISUAL_MATRIX_PRODUCTS = ("standalone", "godot", "unity")
VISUAL_MATRIX_SYMBOLS = {
    "depth_passed": "🟩",
    "render_passed": "🟧",
    "failed": "🟥",
    "not_tested": "⬜",
}


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
        "godot-smoke-windows-vulkan-ordered-overlay": "Windows-Godot-Vulkan-Ordered-Overlay",
        "godot-smoke-windows-vulkan": "Windows-Godot-Vulkan",
        "windows-standalone-directx": "Windows-Standalone-DirectX",
        "windows-standalone-opengl": "Windows-Standalone-OpenGL",
        "windows-standalone-vulkan": "Windows-Standalone-Vulkan",
        "unity-windows-directx-composition": "Unity-Windows-DirectX-Composition",
        "unity-macos-metal-composition": "Unity-macOS-Metal-Composition",
        "unity-windows-vulkan-ordered-overlay": "Unity-Windows-Vulkan-Ordered-Overlay",
        "unity-windows-vulkan-full-depth": "Unity-Windows-Vulkan-Full-Depth",
        "unity-windows-vulkan-synthetic-stereo": "Unity-Windows-Vulkan-Synthetic-Stereo",
        "godot-windows-vulkan-full-depth": "Godot-Windows-Vulkan-Full-Depth",
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
        "godotsmokewindowsvulkanorderedoverlay": "windows-godot-vulkan-ordered-overlay",
        "godotsmokewindowsvulkan": "windows-godot-vulkan",
        "godotsmokemacosmetal": "macos-godot-metal",
        "unitywindowsdirectxcomposition": "unity-windows-directx-composition",
        "unitymacosmetalcomposition": "unity-macos-metal-composition",
        "unityandroidvulkan": "unity-android-vulkan",
        "unitywindowsvulkanorderedoverlay": "unity-windows-vulkan-ordered-overlay",
        "unitywindowsvulkanfulldepth": "unity-windows-vulkan-full-depth",
        "unitywindowsvulkansyntheticstereo": "unity-windows-vulkan-synthetic-stereo",
        "godotwindowsvulkanfulldepth": "godot-windows-vulkan-full-depth",
        "windowsgodotvulkanorderedoverlay": "windows-godot-vulkan-ordered-overlay",
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
    slug = slug.replace("direct-x", "directx").replace("open-gl", "opengl").replace("mac-os", "macos")
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
    key = normalize_label(key.replace("/", "-"))
    key = re.sub(r"(?i)direct[-_\s]*x", "directx", key)
    key = re.sub(r"(?i)open[-_\s]*gl", "opengl", key)
    key = re.sub(r"(?i)mac[-_\s]*os", "macos", key)
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
    if report.name in AGGREGATE_REPORT_NAMES:
        return slugify(report.parent.name)
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
        # These are subordinate render-mode reports produced inside a lane's
        # artifact. Their captures belong under the authoritative matrix lane,
        # regardless of whether the producer wrote the report at artifact root
        # or below captures/.
        if key in GENERIC_CAPTURE_SECTION_KEYS:
            continue
        current = selected.get(key)
        if current is None or report_rank(report) < report_rank(current):
            selected[key] = report
    for captures_dir in input_root.rglob("captures"):
        if not captures_dir.is_dir():
            continue
        # A lane-level render report owns every capture mode below its local
        # captures directory. Rediscovering those folders as independent
        # sections lets a sibling failed metric/status leak into otherwise
        # valid editor-play or sample-play evidence (for example, the Unity
        # Metal ordered-overlay failure appearing as an "Editorplay" result).
        # Aggregate evidence directories do not have this lane-local report,
        # so their per-lane capture folders are still discovered below.
        if (captures_dir.parent / "render-report.md").is_file():
            continue
        for child in captures_dir.iterdir():
            if not child.is_dir():
                continue
            if not any(path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES for path in child.rglob("*")):
                continue
            key = slugify(child.name)
            # Capture-mode subdirectories are inputs to their parent lane report,
            # not independent validation lanes. Rediscovering them produces
            # misleading sections such as "Render" and "Full depth" alongside
            # the authoritative Unity/macOS/Metal section.
            if key in GENERIC_CAPTURE_SECTION_KEYS:
                continue
            selected.setdefault(key, child)
    return sorted(selected.items(), key=lambda item: display_name(item[0]))


def matrix_key(row: dict) -> str:
    return "/".join(
        str(row.get(part, ""))
        for part in ["product", "platform", "mode", "renderer"]
    )


def row_visual_requirement(row: dict) -> bool:
    baseline = str(row.get("baseline") or "")
    renderer = str(row.get("renderer") or "")
    return baseline.startswith("tests/baselines/render/") or renderer in {"directx", "vulkan", "metal"}


def row_matches_key(row: dict, observed_key: str) -> bool:
    product = str(row.get("product") or "")
    platform = str(row.get("platform") or "")
    mode = str(row.get("mode") or "")
    renderer = str(row.get("renderer") or "")
    key = slugify(observed_key)
    terms = [product, platform, renderer]
    if product == "unity" and platform == "all":
        terms = ["unity", "preflight"] if renderer == "preflight" else ["unity"]
    if renderer == "native":
        terms = [product, platform]
    if renderer == "preflight":
        terms = [product, renderer]
    if mode == "vr":
        terms.append("vr")
    elif mode not in {"", "non-vr"}:
        terms.append(mode)
    elif mode == "non-vr" and any(
        other_mode in key
        for other_mode in ("synthetic-stereo", "openxr-vr", "opengl-vr", "quest-vr")
    ):
        return False
    return all(not term or term == "all" or slugify(term) in key for term in terms)


def observed_matrix_results(input_root: Path, report_keys: set[str]) -> dict[str, set[str]]:
    observed: dict[str, set[str]] = {key: {"present"} for key in report_keys}
    for captures_dir in input_root.rglob("captures"):
        if not captures_dir.is_dir():
            continue
        for child in captures_dir.iterdir():
            if child.is_dir():
                observed.setdefault(slugify(child.name), set()).add("present")
    for manifest_path in input_root.rglob("manifest.json"):
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        matrix = manifest.get("matrix") if isinstance(manifest, dict) else None
        if isinstance(matrix, dict):
            key = slugify("-".join(str(matrix.get(part, "")) for part in ["product", "platform", "mode", "renderer"]))
            classification = manifest.get("classification") if isinstance(manifest, dict) else None
            result = ""
            if isinstance(classification, dict):
                result = str(classification.get("result") or "")
                if str(classification.get("failure_class") or "") == "build":
                    continue
            renderer = str(matrix.get("renderer") or "")
            if (
                renderer in VISUAL_RENDERERS
                and not find_strict_metrics(manifest_path.parent)
                and result not in {"failed", "failure", "cancelled", "expected_failed"}
            ):
                # A build/package manifest is useful supporting evidence, but
                # it cannot make a visual row pass or fail. Keep it explicitly
                # incomplete so a sibling visual pass remains authoritative,
                # while a row with only build evidence still fails closed.
                result = "evidence_incomplete"
            observed.setdefault(key, set()).add(result or "present")
    return observed


def row_observed_results(row: dict, observed: dict[str, set[str]]) -> set[str]:
    results: set[str] = set()
    for key, values in observed.items():
        if row_matches_key(row, key):
            results.update(values)
    return results


def coverage_status_for(row: dict, results: set[str]) -> str:
    status = str(row.get("status") or "unknown")
    if status == "deferred":
        return "deferred"
    if status == "unsupported":
        return "unsupported"
    if status != "supported":
        return status
    if not results:
        return "missing evidence"
    if results & {"failed", "failure", "cancelled"}:
        return "failed"
    if "expected_failed" in results:
        return "expected failure"
    if "passed" in results:
        return "passed"
    if "evidence_incomplete" in results:
        return "evidence_incomplete"
    return "failed"


def matrix_coverage_rows(matrix_status: Path | None, input_root: Path, reports: list[tuple[str, Path]]) -> list[dict]:
    if matrix_status is None:
        return []
    matrix = json.loads(matrix_status.read_text(encoding="utf-8"))
    rows = matrix.get("rows", [])
    if not isinstance(rows, list):
        return []
    observed = observed_matrix_results(input_root, {key for key, _report in reports})
    coverage = []
    for row in rows:
        status = str(row.get("status") or "unknown")
        coverage_status = coverage_status_for(row, row_observed_results(row, observed))
        coverage.append(
            {
                "key": matrix_key(row),
                "status": status,
                "coverage_status": coverage_status,
                "visual": row_visual_requirement(row),
                "hosted_gate": row.get("hosted_gate") or "",
                "hardware_gate": row.get("hardware_gate") or "",
                "reason": row.get("owner_decision") or row.get("reason") or "",
                "matrix": row,
            }
        )
    return coverage


def visual_matrix_row(
    coverage: list[dict], product: str, platform: str, renderer: str
) -> dict | None:
    return next(
        (
            row
            for row in coverage
            if str(row["matrix"].get("product") or "") == product
            and str(row["matrix"].get("platform") or "") == platform
            and str(row["matrix"].get("mode") or "") == "non-vr"
            and str(row["matrix"].get("renderer") or "") == renderer
        ),
        None,
    )


def manifest_has_depth_evidence(manifest: dict) -> bool:
    classification = manifest.get("classification") or {}
    if classification.get("result") != "passed":
        return False
    if (
        classification.get("composition_mode") == "full_depth"
        and classification.get("depth_composition") == "success"
    ):
        return True
    for item in manifest.get("files") or []:
        path = str(item.get("path") or "").lower() if isinstance(item, dict) else ""
        if path.endswith(".json") and (
            "full-depth" in path or "composition-metrics" in path
        ):
            return True
    return False


def row_has_depth_evidence(input_root: Path, row: dict) -> bool:
    if any(manifest_has_depth_evidence(item) for item in matching_manifests(input_root, row)):
        return True
    expected = row.get("matrix") or {}
    for status_path in input_root.rglob("composition-status.json"):
        try:
            status = json.loads(status_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(status, dict):
            continue
        nearby_manifest = find_manifest(status_path.parent, slugify("-".join(
            str(expected.get(part) or "")
            for part in ("product", "platform", "mode", "renderer")
        )))
        if not nearby_manifest:
            continue
        if (
            status.get("composition_mode") == "full_depth"
            and status.get("depth_composition") == "success"
            and status.get("result", "passed") == "passed"
        ):
            return True
    return False


def visual_matrix_cell(
    coverage: list[dict], input_root: Path, product: str, platform: str, renderer: str
) -> str:
    row = visual_matrix_row(coverage, product, platform, renderer)
    if row is None or row["status"] != "supported":
        return "not_tested"
    if row["coverage_status"] != "passed":
        return "failed"
    if product == "standalone":
        return "render_passed"
    return "depth_passed" if row_has_depth_evidence(input_root, row) else "render_passed"


def add_visual_matrix(
    lines: list[str], coverage: list[dict], input_root: Path
) -> dict[tuple[str, str], str]:
    if not coverage:
        return {}
    cells: dict[tuple[str, str], str] = {}
    lines.extend(
        [
            "## Visual Matrix",
            "",
            "🟩 depth composition passed · 🟧 render passed but required depth is absent · 🟥 rendering or attempted depth composition failed · ⬜ not tested/out of scope",
            "",
            "| Platform | Standalone | Godot | Unity |",
            "| --- | :---: | :---: | :---: |",
        ]
    )
    for platform, (backend_label, renderer) in VISUAL_MATRIX_TARGETS.items():
        values = []
        for product in VISUAL_MATRIX_PRODUCTS:
            status = visual_matrix_cell(
                coverage, input_root, product, platform, renderer
            )
            cells[(platform, product)] = status
            values.append(VISUAL_MATRIX_SYMBOLS[status])
        lines.append(
            f"| {display_name(platform)} · {backend_label} | {' | '.join(values)} |"
        )
    lines.append("")
    return cells


def add_matrix_coverage(lines: list[str], coverage: list[dict]) -> None:
    if not coverage:
        return
    supported = [row for row in coverage if row["status"] == "supported"]
    missing = [row for row in supported if row["coverage_status"] == "missing evidence"]
    failed = [row for row in supported if row["coverage_status"] == "failed"]
    incomplete = [row for row in supported if row["coverage_status"] == "evidence_incomplete"]
    expected_failed = [row for row in supported if row["coverage_status"] == "expected failure"]
    deferred = [row for row in coverage if row["status"] == "deferred"]
    unsupported = [row for row in coverage if row["status"] == "unsupported"]
    lines.extend(
        [
            "## Matrix Coverage",
            "",
            f"- Supported rows: {len(supported)}",
            f"- Supported rows with evidence in this artifact: {len(supported) - len(missing)}",
            f"- Supported rows missing evidence in this artifact: {len(missing)}",
            f"- Supported rows failed: {len(failed)}",
            f"- Supported rows evidence-incomplete: {len(incomplete)}",
            f"- Supported rows expected-failed: {len(expected_failed)}",
            f"- Deferred rows: {len(deferred)}",
            f"- Unsupported rows: {len(unsupported)}",
            "",
            "| Row | Status | Evidence | Visual | Gate |",
            "| --- | --- | --- | --- | --- |",
        ]
    )
    for row in coverage:
        gate = row["hardware_gate"] or row["hosted_gate"]
        lines.append(
            f"| {row['key']} | {row['status']} | {row['coverage_status']} | {'yes' if row['visual'] else 'no'} | {gate} |"
        )
    lines.append("")
    if missing:
        lines.append("### Missing Supported Evidence")
        for row in missing:
            lines.append(f"- {row['key']}: {row['reason']}")
        lines.append("")
    if failed:
        lines.append("### Failed Supported Evidence")
        for row in failed:
            lines.append(f"- {row['key']}: {row['reason']}")
        lines.append("")
    if incomplete:
        lines.append("### Evidence-Incomplete Supported Evidence")
        for row in incomplete:
            lines.append(f"- {row['key']}: {row['reason']}")
        lines.append("")
    if expected_failed:
        lines.append("### Expected-Failed Supported Evidence")
        for row in expected_failed:
            lines.append(f"- {row['key']}: {row['reason']}")
        lines.append("")
    if deferred:
        lines.append("### Deferred")
        for row in deferred:
            lines.append(f"- {row['key']}: {row['reason']}")
        lines.append("")
    if unsupported:
        lines.append("### Unsupported")
        for row in unsupported:
            lines.append(f"- {row['key']}: {row['reason']}")
        lines.append("")


def matching_manifests(input_root: Path, row: dict) -> list[dict]:
    expected = row.get("matrix") or {}
    matches: list[dict] = []
    for path in input_root.rglob("manifest.json"):
        try:
            manifest = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        matrix = manifest.get("matrix") if isinstance(manifest, dict) else None
        if not isinstance(matrix, dict):
            continue
        if all(
            str(matrix.get(part) or "").lower() == str(expected.get(part) or "").lower()
            for part in ("product", "platform", "mode", "renderer")
        ):
            matches.append(manifest)
    return matches


def status_only_result(row: dict, manifest: dict) -> tuple[str, str]:
    classification = manifest.get("classification") or {}
    result = str(classification.get("result") or row["coverage_status"])
    failure_class = str(classification.get("failure_class") or "")
    if (
        row["visual"]
        and row["coverage_status"] in {"failed", "evidence_incomplete"}
        and result == "passed"
    ):
        return "evidence_incomplete", "evidence"
    if result in {"failed", "failure"}:
        result = {
            "compositing": "composition_failed",
            "evidence": "evidence_incomplete",
            "infrastructure": "infrastructure_failed",
            "rendering": "render_failed",
            "runtime": "runtime_failed",
            "runtime-launch": "runtime_failed",
        }.get(failure_class, "failed")
    return result, failure_class


def add_status_only_sections(
    lines: list[str],
    coverage: list[dict],
    input_root: Path,
    reports: list[tuple[str, Path]],
) -> None:
    report_keys = {key for key, _report in reports}
    rows = []
    for row in coverage:
        if row["status"] != "supported" or row["coverage_status"] == "missing evidence":
            continue
        has_visual_report = any(row_matches_key(row["matrix"], key) for key in report_keys)
        if not row["visual"] or not has_visual_report:
            rows.append(row)
    if not rows:
        return
    lines.append("## Status-Only Evidence")
    lines.append("")
    for row in rows:
        lines.append(f"### {display_name(row['key'])}")
        lines.append("")
        manifests = matching_manifests(input_root, row)
        manifests.sort(
            key=lambda manifest: (
                str((manifest.get("classification") or {}).get("result") or "") == "passed",
                str((manifest.get("classification") or {}).get("failure_class") or "") == "build",
            )
        )
        manifest = manifests[0] if manifests else {}
        result, failure_class = status_only_result(row, manifest)
        lines.append(f"- Result: {result}")
        if failure_class:
            lines.append(f"- Failure class: {failure_class}")
        classification = manifest.get("classification") or {}
        for failure in classification.get("failures") or []:
            lines.append(f"- Failure: {failure}")
        if (
            result == "evidence_incomplete"
            and not (classification.get("failures") or [])
        ):
            lines.append("- Failure: no strict visual report was produced for this supported visual row")
        for warning in classification.get("warnings") or []:
            lines.append(f"- Supporting diagnostic: {warning}")
        gate = row["hardware_gate"] or row["hosted_gate"]
        if gate:
            lines.append(f"- Gate: {gate}")
        if row["reason"]:
            lines.append(f"- Scope: {row['reason']}")
        lines.append("")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def artifact_root_for(report: Path) -> Path:
    if report.is_dir():
        evidence_names = ["composition-status.json", "render-metrics.json", "manifest.json"]
        if any((report / name).exists() for name in evidence_names):
            return report
        if report.parent.name == "captures":
            return report.parent.parent
        return report
    return report.parent


def image_roots_for_report(report: Path, key: str) -> list[Path]:
    if report.is_dir():
        return [report]
    captures = report.parent / "captures"
    if captures.exists():
        matching = [path for path in captures.iterdir() if path.is_dir() and slugify(path.name) == key]
        if matching:
            return matching
        if any(path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES for path in captures.iterdir()):
            return [captures]
        if report.name in AGGREGATE_REPORT_NAMES:
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
    seen: set[tuple[str, str]] = set()
    unique: list[Path] = []
    for image in sorted(candidates):
        identity = (sha256_file(image), image.name.lower())
        if identity in seen:
            continue
        seen.add(identity)
        unique.append(image)
    return unique


def find_json(root: Path, *names: str) -> dict:
    for name in names:
        path = root / name
        if path.exists():
            return json.loads(path.read_text(encoding="utf-8"))
    return {}


def report_candidate_stems(report: Path) -> set[str]:
    """Return candidate capture stems named by a generated render report."""
    if not report.is_file() or report.suffix.lower() != ".md":
        return set()
    try:
        lines = report.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return set()

    stems: set[str] = set()
    in_candidate_section = False
    for line in lines:
        heading = line.strip().lower()
        if heading.startswith("### "):
            in_candidate_section = heading == "### candidate"
            continue
        if not in_candidate_section:
            continue
        match = re.search(r"!\[[^\]]*\]\(([^)]+)\)", line)
        if match:
            stems.add(Path(match.group(1).split("?", 1)[0]).stem.lower())
            in_candidate_section = False
    return stems


def load_metric_candidates(root: Path) -> list[dict]:
    candidates: list[dict] = []
    for path in sorted(root.rglob("*metrics*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if isinstance(data, dict) and isinstance(data.get("candidate"), dict):
            candidates.append(data)
    return candidates


def metrics_failed(metrics: dict) -> bool:
    return metrics.get("passed") is False or bool(metrics.get("errors"))


def complete_metrics_evidence(metrics: dict) -> bool:
    spatial = metrics.get("spatial_luma_grid")
    return (
        isinstance(metrics.get("passed"), bool)
        and isinstance(metrics.get("candidate"), dict)
        and isinstance(metrics.get("reference"), dict)
        and isinstance(metrics.get("contract"), dict)
        and isinstance(spatial, dict)
        and not spatial.get("error")
        and isinstance(spatial.get("mean_abs_delta"), (int, float))
        and isinstance(spatial.get("correlation"), (int, float))
    )


def find_manifest(root: Path, key: str) -> dict:
    for path in sorted(root.rglob("manifest.json")):
        try:
            manifest = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        matrix = manifest.get("matrix") if isinstance(manifest, dict) else None
        if isinstance(matrix, dict) and row_matches_key(matrix, key):
            return manifest
    return {}


def write_section_json(section_dir: Path, name: str, data: dict) -> None:
    if not data:
        return
    section_dir.mkdir(parents=True, exist_ok=True)
    (section_dir / name).write_text(
        json.dumps(data, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def preserve_status_manifests(input_root: Path, output_dir: Path) -> None:
    """Carry non-build lane results through every evidence aggregation layer."""
    for manifest_path in sorted(input_root.rglob("manifest.json")):
        if output_dir == manifest_path.parent or output_dir in manifest_path.parents:
            continue
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(manifest, dict) or manifest.get("schema") != "imm-ci-artifact-manifest-v1":
            continue
        matrix = manifest.get("matrix")
        classification = manifest.get("classification")
        if not isinstance(matrix, dict) or not isinstance(classification, dict):
            continue
        if classification.get("failure_class") == "build":
            continue
        matrix_key = "-".join(
            str(matrix.get(part, ""))
            for part in ["product", "platform", "mode", "renderer"]
        )
        canonical = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode("utf-8")
        digest = hashlib.sha256(canonical).hexdigest()[:12]
        destination = output_dir / "status-manifests" / f"{slugify(matrix_key)}-{digest}" / "manifest.json"
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        strict_metrics = find_strict_metrics(manifest_path.parent)
        if strict_metrics:
            (destination.parent / "render-metrics.json").write_text(
                json.dumps(strict_metrics, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
                newline="\n",
            )


def find_metrics(root: Path, report: Path | None = None) -> dict:
    candidates = load_metric_candidates(root)
    if not candidates:
        return {}

    candidate_stems = report_candidate_stems(report) if report is not None else set()
    if candidate_stems:
        matched = [
            data
            for data in candidates
            if Path(str(data.get("candidate", {}).get("path", ""))).stem.lower() in candidate_stems
        ]
        if matched:
            # A report is authoritative for its named candidate. Never replace
            # its failed visual result with an unrelated passing metric from
            # another capture mode in the same lane artifact.
            return next((data for data in matched if metrics_failed(data)), matched[0])

    # Without an explicit candidate link, bias toward preserving a complete
    # failure. Choosing the first passing sibling creates a false green report.
    complete = [data for data in candidates if complete_metrics_evidence(data)]
    failed = [data for data in complete if metrics_failed(data)]
    if failed:
        return failed[0]
    passing = [data for data in complete if strict_metrics_evidence(data)]
    if passing:
        return passing[0]
    return candidates[0]


def strict_metrics_evidence(metrics: dict) -> bool:
    return metrics.get("passed") is True and complete_metrics_evidence(metrics)


def find_strict_metrics(root: Path) -> dict:
    return next(
        (data for data in load_metric_candidates(root) if strict_metrics_evidence(data)),
        {},
    )


def effective_status(metrics: dict, status: dict, manifest: dict) -> tuple[str, str]:
    status_result = status.get("result")
    if status_result in {
        "runtime_failed",
        "infrastructure_failed",
        "evidence_incomplete",
    }:
        return (status_result, status.get("failure_class", ""))
    if status_result == "render_failed":
        return ("render_failed", status.get("failure_class", "rendering"))
    errors = metrics.get("errors") or []
    if errors or metrics.get("passed") is False:
        if errors and all(str(error).startswith("color component probe ") for error in errors):
            return ("composition_failed", "compositing")
        return ("render_failed", "rendering")
    if status_result == "composition_failed":
        return ("composition_failed", status.get("failure_class", "compositing"))
    if metrics and not strict_metrics_evidence(metrics):
        return ("evidence_incomplete", "evidence")
    if status_result == "passed":
        return ("passed", "")
    classification = manifest.get("classification") or {}
    result = classification.get("result")
    failure_class = classification.get("failure_class", "")
    if result in {"failed", "failure", "cancelled"}:
        mapped = {
            "visual": "render_failed",
            "rendering": "render_failed",
            "compositing": "composition_failed",
            "runtime": "runtime_failed",
            "runtime-launch": "runtime_failed",
            "infrastructure": "infrastructure_failed",
            "vr-device-infrastructure": "infrastructure_failed",
            "evidence": "evidence_incomplete",
        }.get(failure_class, "infrastructure_failed")
        return (mapped, failure_class)
    if result == "expected_failed":
        return ("composition_failed", failure_class or "compositing")
    if result == "passed":
        return ("passed", failure_class)
    if status.get("compositing") == "expected_failed":
        return ("composition_failed", "compositing")
    if status.get("compositing") == "failed":
        return ("composition_failed", "compositing")
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


def percentile_value(metrics: dict, section: str, key: str) -> str:
    value = metrics.get(section, {}).get("luma_percentiles", {}).get(key)
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
        ("luma_stddev", metric_value(metrics, "reference", "luma_stddev"), metric_value(metrics, "candidate", "luma_stddev")),
        ("luma_p01", percentile_value(metrics, "reference", "p01"), percentile_value(metrics, "candidate", "p01")),
        ("luma_p50", percentile_value(metrics, "reference", "p50"), percentile_value(metrics, "candidate", "p50")),
        ("luma_p95", percentile_value(metrics, "reference", "p95"), percentile_value(metrics, "candidate", "p95")),
        ("luma_p99", percentile_value(metrics, "reference", "p99"), percentile_value(metrics, "candidate", "p99")),
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
    parser.add_argument("--matrix-status", type=Path)
    parser.add_argument(
        "--required-evidence-scope",
        choices=["none", "hosted", "all"],
        default="none",
        help="Fail when supported rows in the selected scope have no evidence.",
    )
    args = parser.parse_args()

    input_root = args.input_root.resolve()
    output_dir = args.output_dir.resolve()
    report_path = args.markdown_output.resolve()
    capture_output_dir = output_dir / "captures"
    output_dir.mkdir(parents=True, exist_ok=True)
    preserve_status_manifests(input_root, output_dir)

    reports = discover_reports(input_root)
    coverage = matrix_coverage_rows(args.matrix_status, input_root, reports)

    lines = [
        "# IMM CI Validation Evidence",
        "",
        "This is the report to read first. It summarizes render correctness, known composition failures, and the captured images used as evidence.",
        "",
        "Result vocabulary: `render_failed` means a produced image violated its visual contract; `composition_failed` means depth or ordering was wrong; `runtime_failed` means the requested player/API did not run; `infrastructure_failed` means the runner or external service failed; `evidence_incomplete` means no authoritative verdict could be formed.",
        "",
    ]
    visual_matrix = add_visual_matrix(lines, coverage, input_root)
    add_matrix_coverage(lines, coverage)
    add_status_only_sections(lines, coverage, input_root, reports)

    visual_sections = 0
    for key, report in reports:
        root = artifact_root_for(report)
        metrics = find_metrics(root, report)
        status = find_json(root, "composition-status.json")
        manifest = find_manifest(root, key)
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
            if status.get("composition_mode"):
                lines.append(f"- Composition mode: {status.get('composition_mode', '')}")
            if status.get("composition_contract"):
                lines.append(f"- Composition contract: {status.get('composition_contract', '')}")
            lines.append(f"- Compositing: {status.get('compositing', '')}")
            if status.get("ordered_overlay"):
                lines.append(f"- Ordered overlay: {status.get('ordered_overlay', '')}")
            if status.get("depth_composition"):
                lines.append(f"- Depth composition: {status.get('depth_composition', '')}")
            if status.get("depth_interleaving"):
                lines.append(f"- Depth interleaving: {status.get('depth_interleaving', '')}")
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
        section_output_dir = capture_output_dir / section_slug
        write_section_json(section_output_dir, "render-metrics.json", metrics)
        write_section_json(section_output_dir, "composition-status.json", status)
        write_section_json(section_output_dir, "manifest.json", manifest)

    if visual_sections == 0:
        lines.append("No visual render evidence was found in the downloaded artifacts.")
        lines.append("")

    report_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8", newline="\n")
    print(f"Visual evidence report written: {report_path}")
    invalid_supported = []
    for row in coverage:
        if row["status"] != "supported":
            continue
        evidence_invalid = row["coverage_status"] in {
            "failed",
            "expected failure",
            "evidence_incomplete",
        }
        missing_required = row["coverage_status"] == "missing evidence" and (
            args.required_evidence_scope == "all"
            or (args.required_evidence_scope == "hosted" and bool(row["hosted_gate"]))
        )
        if evidence_invalid or missing_required:
            invalid_supported.append(row)
    if invalid_supported:
        for row in invalid_supported:
            print(f"Supported validation evidence is not valid: {row['key']} ({row['coverage_status']})")
        return 1
    complete_visual_matrix_scope = all(
        any(
            str(row["matrix"].get("platform") or "") == platform
            for row in coverage
        )
        for platform in VISUAL_MATRIX_TARGETS
    )
    required_depth_gaps = [
        (platform, product)
        for (platform, product), status in visual_matrix.items()
        if complete_visual_matrix_scope
        and product in {"godot", "unity"}
        and status != "depth_passed"
    ]
    if required_depth_gaps:
        for platform, product in required_depth_gaps:
            print(
                f"Required depth validation is not valid: {product}/{platform}"
            )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
