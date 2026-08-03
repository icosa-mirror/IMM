#!/usr/bin/env python3
"""Classify Android Godot Vulkan from authoritative visual evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CRASH_FILE_PATTERNS = (
    "data_app_native_crash_*.txt",
    "SYSTEM_TOMBSTONE_*.txt",
)
RUNTIME_MARKERS = (
    "fatal signal",
    "fatal exception",
    "device lost",
    "device was lost",
    "vk_error_device_lost",
    "immviewernode setup failed",
    "immviewercompositoreffect setup failed",
)
REQUIRED_CAPTURES = (
    "vulkan_render_candidate.png",
    "vulkan_visual_smoke.png",
)
RENDER_EVIDENCE = "android-godot-vulkan-screenshot-metrics.json"
COMPOSITION_EVIDENCE = "android-godot-vulkan-composition-metrics.json"


def read_json(path: Path) -> dict | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def find_files(root: Path, name: str) -> list[Path]:
    return sorted(path for path in root.rglob(name) if path.is_file())


def evidence_errors(name: str, value: dict) -> list[str]:
    failures = value.get("errors") or value.get("failures") or []
    if not isinstance(failures, list):
        failures = [failures]
    detail = "; ".join(str(item) for item in failures if str(item))
    return [f"{name} failed{f': {detail}' if detail else ''}"]


def result(
    status: str,
    failure_class: str,
    failures: list[str],
    warnings: list[str] | None = None,
) -> dict:
    return {
        "schema": "imm-android-godot-vulkan-status-v1",
        "result": status,
        "failure_class": failure_class,
        "failures": failures,
        "warnings": warnings or [],
    }


def firebase_warnings(value: dict) -> list[str]:
    warnings: list[str] = []
    copy_exit = (value.get("copy_results") or {}).get("exit_code")
    gcloud_exit = value.get("gcloud_exit_code")
    if copy_exit not in (None, 0):
        warnings.append(f"Firebase artifact copy exited with {copy_exit}")
    if gcloud_exit not in (None, 0):
        warnings.append(f"Firebase test command exited with {gcloud_exit}")
    warnings.extend(str(item) for item in value.get("errors", []) if str(item))
    return warnings


def classify(root: Path) -> dict:
    crash_files = [
        path
        for pattern in CRASH_FILE_PATTERNS
        for path in root.rglob(pattern)
        if path.is_file()
    ]
    log_paths = find_files(root, "logcat") + find_files(root, "logcat_after.txt")
    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore") for path in log_paths
    )
    lowered_log = log_text.lower()
    runtime_markers = [marker for marker in RUNTIME_MARKERS if marker in lowered_log]
    if crash_files or runtime_markers:
        failures = [
            f"Android app crash evidence: {path.relative_to(root)}"
            for path in crash_files
        ]
        failures.extend(
            f"Android runtime failure marker: {marker}" for marker in runtime_markers
        )
        return result("runtime_failed", "runtime", failures)

    firebase_path = root / "firebase-test-lab-result.json"
    firebase = read_json(firebase_path)
    if firebase is None:
        return result(
            "evidence_incomplete",
            "evidence",
            [f"missing or invalid Firebase result: {firebase_path}"],
        )
    warnings = firebase_warnings(firebase)

    missing: list[str] = []
    for capture_name in REQUIRED_CAPTURES:
        if not find_files(root / "ftl-results", capture_name):
            missing.append(f"missing authoritative capture: {capture_name}")
    for evidence_name in (RENDER_EVIDENCE, COMPOSITION_EVIDENCE):
        if read_json(root / evidence_name) is None:
            missing.append(f"missing or invalid validation evidence: {evidence_name}")
    if missing and warnings:
        return result(
            "infrastructure_failed",
            "infrastructure",
            warnings + missing,
        )
    if missing:
        return result("evidence_incomplete", "evidence", missing)

    render = read_json(root / RENDER_EVIDENCE)
    composition = read_json(root / COMPOSITION_EVIDENCE)
    assert render is not None
    assert composition is not None
    if render.get("passed") is not True:
        return result(
            "render_failed",
            "rendering",
            evidence_errors(RENDER_EVIDENCE, render),
            warnings,
        )
    if composition.get("passed") is not True:
        return result(
            "composition_failed",
            "compositing",
            evidence_errors(COMPOSITION_EVIDENCE, composition),
            warnings,
        )

    # Instrumentation/log assertions are useful diagnostics and can stop an
    # incomplete run early. Once both required device images exist and pass
    # their visual contracts, a secondary gcloud or marker error must not turn
    # the rendered result into a false runtime failure.
    return result("passed", "", [], warnings)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    classified = classify(args.artifact_dir)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(classified, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"Android Godot Vulkan status: {classified['result']}"
        f" ({classified['failure_class'] or 'no failure class'})"
    )
    for failure in classified["failures"]:
        print(f"- {failure}")
    for warning in classified["warnings"]:
        print(f"- supporting diagnostic: {warning}")
    return 0 if classified["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
