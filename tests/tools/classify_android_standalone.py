#!/usr/bin/env python3
"""Classify Android standalone visual evidence without masking Firebase failures."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CAPTURE_NAMES = ("native-render-after.ppm", "native-render-rejected.ppm")
CRASH_FILE_PATTERNS = ("data_app_native_crash_*.txt", "SYSTEM_TOMBSTONE_*.txt")
RUNTIME_MARKERS = (
    "fatal signal",
    "fatal exception",
    "device lost",
    "device was lost",
    "vk_error_device_lost",
)


def read_json(path: Path) -> dict | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def find_files(root: Path, name: str) -> list[Path]:
    return sorted(path for path in root.rglob(name) if path.is_file())


def result(
    status: str,
    failure_class: str,
    failures: list[str],
    warnings: list[str] | None = None,
) -> dict:
    return {
        "schema": "imm-android-standalone-status-v1",
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


def metric_errors(name: str, value: dict) -> list[str]:
    failures = value.get("errors") or value.get("failures") or []
    if not isinstance(failures, list):
        failures = [failures]
    detail = "; ".join(str(item) for item in failures if str(item))
    return [f"{name} failed{f': {detail}' if detail else ''}"]


def classify(root: Path, metrics_name: str) -> dict:
    crash_files = [
        path
        for pattern in CRASH_FILE_PATTERNS
        for path in root.rglob(pattern)
        if path.is_file()
    ]
    log_paths = find_files(root, "logcat") + find_files(root, "logcat_after.txt")
    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore") for path in log_paths
    ).lower()
    runtime_markers = [marker for marker in RUNTIME_MARKERS if marker in log_text]
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

    captures = [
        path
        for name in CAPTURE_NAMES
        for path in find_files(root / "ftl-results", name)
    ]
    metrics = read_json(root / metrics_name)
    missing: list[str] = []
    if not captures:
        missing.append("missing authoritative capture: native-render-after.ppm")
    if metrics is None:
        missing.append(f"missing or invalid validation evidence: {metrics_name}")
    if missing and warnings:
        return result("infrastructure_failed", "infrastructure", warnings + missing)
    if missing:
        return result("evidence_incomplete", "evidence", missing)

    assert metrics is not None
    if metrics.get("passed") is not True:
        return result(
            "render_failed",
            "rendering",
            metric_errors(metrics_name, metrics),
            warnings,
        )

    # A complete passing image is authoritative. Missing redundant log markers
    # and post-test result-copy errors remain diagnostics rather than converting
    # a visibly correct frame into a false failure.
    return result("passed", "", [], warnings)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--metrics-json", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    classified = classify(args.artifact_dir, args.metrics_json)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(classified, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"Android standalone status: {classified['result']}"
        f" ({classified['failure_class'] or 'no failure class'})"
    )
    for failure in classified["failures"]:
        print(f"- {failure}")
    for warning in classified["warnings"]:
        print(f"- supporting diagnostic: {warning}")
    return 0 if classified["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
