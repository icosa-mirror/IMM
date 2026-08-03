#!/usr/bin/env python3
"""Classify Android Unity Vulkan CI evidence without flattening every failure."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CRASH_FILE_PATTERNS = (
    "data_app_native_crash_*com_ImmersiveFoundation_IMMUnityTest*.txt",
    "SYSTEM_TOMBSTONE_*com_ImmersiveFoundation_IMMUnityTest*.txt",
)
RUNTIME_MARKERS = (
    "[IMM_UNITY_SMOKE] graphics api probe failed",
    "expected=Vulkan actual=OpenGLES",
    "expected=Vulkan actual=Direct3D",
    "Fatal signal 11",
    "libswappywrapper",
)
REQUIRED_CAPTURES = (
    "unity-android-vulkan-render.png",
    "unity-android-vulkan.png",
    "unity-android-vulkan-synthetic-stereo.png",
)
RENDER_EVIDENCE = (
    "unity-android-vulkan-screenshot-metrics.json",
    "unity-android-vulkan-synthetic-stereo-structure.json",
    "unity-android-vulkan-synthetic-left-metrics.json",
    "unity-android-vulkan-synthetic-right-metrics.json",
    "unity-android-vulkan-synthetic-stereo-log-contract.json",
    "unity-android-vulkan-external-render-video-validation.json",
)
COMPOSITION_EVIDENCE = "unity-android-vulkan-composition-metrics.json"


def read_json(path: Path) -> dict | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def evidence_passed(value: dict) -> bool:
    if "passed" in value:
        return value.get("passed") is True
    return value.get("status") == "passed"


def evidence_errors(name: str, value: dict) -> list[str]:
    failures = value.get("errors") or value.get("failures") or []
    if not isinstance(failures, list):
        failures = [failures]
    detail = "; ".join(str(item) for item in failures if str(item))
    return [f"{name} failed{f': {detail}' if detail else ''}"]


def find_files(root: Path, name: str) -> list[Path]:
    return sorted(path for path in root.rglob(name) if path.is_file())


def classify(root: Path) -> dict:
    failures: list[str] = []
    firebase_path = root / "firebase-test-lab-result.json"
    firebase = read_json(firebase_path)

    crash_files = [
        path
        for pattern in CRASH_FILE_PATTERNS
        for path in root.rglob(pattern)
        if path.is_file()
    ]
    log_paths = find_files(root, "logcat")
    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore") for path in log_paths
    )
    runtime_markers = [marker for marker in RUNTIME_MARKERS if marker in log_text]
    if crash_files or runtime_markers:
        failures.extend(f"Android app crash evidence: {path.relative_to(root)}" for path in crash_files)
        failures.extend(f"Android runtime failure marker: {marker}" for marker in runtime_markers)
        return status("runtime_failed", "runtime", failures)

    if firebase is None:
        return status(
            "infrastructure_failed",
            "infrastructure",
            [f"missing or invalid Firebase result: {firebase_path}"],
        )

    copy_exit = (firebase.get("copy_results") or {}).get("exit_code")
    gcloud_exit = firebase.get("gcloud_exit_code")
    infrastructure_failures: list[str] = []
    if copy_exit not in (None, 0):
        infrastructure_failures.append(f"Firebase artifact copy exited with {copy_exit}")
    if gcloud_exit not in (None, 0):
        infrastructure_failures.append(f"Firebase test command exited with {gcloud_exit}")
    infrastructure_failures.extend(str(item) for item in firebase.get("errors", []) if str(item))

    missing: list[str] = []
    for capture_name in REQUIRED_CAPTURES:
        if not find_files(root / "ftl-results", capture_name):
            missing.append(f"missing authoritative capture: {capture_name}")
    for evidence_name in (*RENDER_EVIDENCE, COMPOSITION_EVIDENCE):
        path = root / evidence_name
        if read_json(path) is None:
            missing.append(f"missing or invalid validation evidence: {evidence_name}")
    if missing and infrastructure_failures:
        return status("infrastructure_failed", "infrastructure", infrastructure_failures + missing)
    if missing:
        return status("evidence_incomplete", "evidence", missing)

    render_failures: list[str] = []
    for name in RENDER_EVIDENCE:
        value = read_json(root / name)
        assert value is not None
        if not evidence_passed(value):
            render_failures.extend(evidence_errors(name, value))
    composition = read_json(root / COMPOSITION_EVIDENCE)
    assert composition is not None
    composition_failures = (
        evidence_errors(COMPOSITION_EVIDENCE, composition)
        if not evidence_passed(composition)
        else []
    )

    # Once the full visual evidence set exists, its verdict is authoritative.
    # A Firebase CLI/reporting error remains useful supporting evidence, but it
    # must not mask a black eye image or a real depth-composition defect.
    if render_failures:
        return status(
            "render_failed",
            "rendering",
            render_failures + composition_failures + infrastructure_failures,
        )
    if composition_failures:
        return status(
            "composition_failed",
            "compositing",
            composition_failures + infrastructure_failures,
        )
    if infrastructure_failures:
        return status("infrastructure_failed", "infrastructure", infrastructure_failures)

    return status("passed", "", [])


def status(result: str, failure_class: str, failures: list[str]) -> dict:
    return {
        "schema": "imm-android-unity-vulkan-status-v1",
        "result": result,
        "failure_class": failure_class,
        "failures": failures,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    result = classify(args.artifact_dir)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"Android Unity Vulkan status: {result['result']}"
        f" ({result['failure_class'] or 'no failure class'})"
    )
    for failure in result["failures"]:
        print(f"- {failure}")
    return 0 if result["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
