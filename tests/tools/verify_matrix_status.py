#!/usr/bin/env python3
"""Verify machine-readable testing matrix status coverage."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


VALID_STATUSES = {"supported", "deferred", "unsupported", "waived"}
REQUIRED_FIELDS = {"product", "platform", "mode", "renderer", "status", "reason"}
EXPECTED_AUDIO_PIPELINES = {
    ("standalone", "windows"): "Audio360",
    ("standalone", "android"): "OpenSL ES",
    ("standalone", "macos"): "AVFoundation",
    ("standalone", "ios"): None,
    ("unity", "windows"): "Audio360",
    ("unity", "android"): "OpenSL ES",
    ("unity", "macos"): "AVFoundation",
    ("unity", "ios"): "AVFoundation",
    ("godot", "windows"): "Audio360",
    ("godot", "android"): "OpenSL ES",
    ("godot", "macos"): "AVFoundation",
    ("godot", "ios"): None,
}
REQUIRED_ROWS = {
    ("standalone", "windows", "non-vr"),
    ("standalone", "windows", "vr"),
    ("standalone", "android", "non-vr"),
    ("standalone", "android", "vr"),
    ("standalone", "ios", "non-vr"),
    ("standalone", "macos", "non-vr"),
    ("standalone", "macos", "vr"),
    ("unity", "all", "non-vr"),
    ("unity", "android", "non-vr"),
    ("unity", "windows", "synthetic-stereo"),
    ("unity", "windows", "vr"),
    ("godot", "windows", "non-vr"),
    ("godot", "android", "non-vr"),
    ("godot", "ios", "non-vr"),
    ("godot", "macos", "non-vr"),
    ("godot", "windows", "vr"),
    ("godot", "android", "vr"),
    ("web", "browser", "non-vr"),
}
KNOWN_HOSTED_GATES = {
    "Build / Android",
    "Build / macOS",
    "Build / Package Unity Plugins and CI Core Matrix / Package Source Layout",
    "Build / Windows",
    "Build / Windows and CI Core Matrix / Godot Local Verifier",
    "Build / Windows with IMM_CI_ENABLE_GPU_SMOKE=1",
    "CI Engine Matrix / Unity Package Import",
    "CI Engine Matrix / Unity Android Vulkan",
    "CI Engine Matrix / Unity Windows DirectX Composition",
    "CI Engine Matrix / Unity Windows Vulkan Synthetic Stereo",
    "CI Engine Matrix / Unity macOS Metal Composition",
    "CI Core Matrix / Godot Local Verifier",
    "CI Core Matrix / Package Source Layout",
    "Godot local verifier checks Unity XR scene bootstrap",
    "Godot local verifier checks Windows standalone OpenGL VR settings",
    "Web Player Build and Validation / Extended browser verification",
}
KNOWN_HARDWARE_GATES = {
    "CI Device Matrix / Android Godot Vulkan",
    "CI Device Matrix / Android OpenXR Probe",
    "CI Device Matrix / Android Quest VR",
    "CI Device Matrix / Android Standalone GLES",
    "CI Device Matrix / Android Standalone Vulkan",
    "CI Engine Matrix / Unity Package Import",
    "CI Engine Matrix / Unity Windows OpenXR VR",
    "CI Engine Matrix / Unity Windows Vulkan",
    "CI GPU Matrix / macOS Godot Metal",
    "CI GPU Matrix / Windows Godot Vulkan",
    "CI GPU Matrix / Windows Godot OpenXR VR",
    "CI GPU Matrix / Windows Standalone DirectX",
    "CI GPU Matrix / Windows Standalone OpenGL",
    "CI GPU Matrix / Windows Standalone OpenXR VR",
    "CI GPU Matrix / Windows Standalone OpenGL VR",
    "CI GPU Matrix / Windows Standalone Vulkan",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix_status", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--release", action="store_true", help="Apply release-blocking policy: no deferred rows are allowed")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    data = json.loads(args.matrix_status.read_text(encoding="utf-8"))
    errors: list[str] = []

    if data.get("schema") != "imm-testing-matrix-status-v1":
        errors.append("Unexpected or missing matrix status schema")

    rows = data.get("rows")
    if not isinstance(rows, list) or not rows:
        errors.append("Matrix status must contain a non-empty rows list")
        rows = []

    audio_pipelines = data.get("audio_pipelines")
    if not isinstance(audio_pipelines, list):
        errors.append("Matrix status must contain an audio_pipelines list")
        audio_pipelines = []

    seen_audio_keys: set[tuple[str, str]] = set()
    for index, pipeline in enumerate(audio_pipelines):
        if not isinstance(pipeline, dict):
            errors.append(f"Audio pipeline {index} must be an object")
            continue
        key = (str(pipeline.get("product", "")), str(pipeline.get("platform", "")))
        if key in seen_audio_keys:
            errors.append(f"Duplicate audio pipeline key: {key}")
        seen_audio_keys.add(key)
        if key not in EXPECTED_AUDIO_PIPELINES:
            errors.append(f"Unexpected audio pipeline key: {key}")
            continue

        expected_backend = EXPECTED_AUDIO_PIPELINES[key]
        status = pipeline.get("status")
        backend = pipeline.get("backend")
        validation_gate = pipeline.get("validation_gate")
        if expected_backend is None:
            if status != "unsupported" or backend is not None or validation_gate is not None:
                errors.append(f"Unsupported audio pipeline {key} must have status=unsupported and null backend/validation_gate")
        else:
            if status != "actual":
                errors.append(f"Audio pipeline {key} must have status=actual")
            if backend != expected_backend:
                errors.append(f"Audio pipeline {key} backend is {backend!r}, expected {expected_backend!r}")
            if not str(validation_gate or "").strip():
                errors.append(f"Actual audio pipeline {key} must name a validation_gate")
            if str(backend).lower() in {"null", "dummy", "silent"}:
                errors.append(f"Actual audio pipeline {key} cannot use {backend!r}")

    missing_audio_keys = set(EXPECTED_AUDIO_PIPELINES) - seen_audio_keys
    for key in sorted(missing_audio_keys):
        errors.append(f"Missing required audio pipeline coverage: {key}")

    seen_keys: set[tuple[str, str, str, str]] = set()
    coverage_keys: set[tuple[str, str, str]] = set()
    supported_count = 0

    for index, row in enumerate(rows):
        missing = REQUIRED_FIELDS - set(row)
        if missing:
            errors.append(f"Row {index} is missing fields: {sorted(missing)}")
            continue

        key = (row["product"], row["platform"], row["mode"], row["renderer"])
        if key in seen_keys:
            errors.append(f"Duplicate row key: {key}")
        seen_keys.add(key)
        coverage_keys.add((row["product"], row["platform"], row["mode"]))

        status = row["status"]
        if status not in VALID_STATUSES:
            errors.append(f"Row {key} has invalid status {status!r}")
        if not str(row["reason"]).strip():
            errors.append(f"Row {key} must include a reason")

        baseline = row.get("baseline")
        if baseline:
            baseline_path = repo_root / baseline
            if not baseline_path.exists():
                errors.append(f"Row {key} references missing baseline: {baseline}")

        if status == "supported":
            supported_count += 1
            if not row.get("hosted_gate") and not row.get("hardware_gate"):
                errors.append(f"Supported row {key} must name a hosted_gate or hardware_gate")

        hosted_gate = row.get("hosted_gate")
        if hosted_gate is not None and hosted_gate not in KNOWN_HOSTED_GATES:
            errors.append(f"Row {key} references unknown hosted_gate: {hosted_gate}")
        hardware_gate = row.get("hardware_gate")
        if hardware_gate is not None and hardware_gate not in KNOWN_HARDWARE_GATES:
            errors.append(f"Row {key} references unknown hardware_gate: {hardware_gate}")

        if status in {"deferred", "unsupported", "waived"} and not row.get("reason"):
            errors.append(f"{status} row {key} must explain why it is not fully gated")
        if status in {"deferred", "unsupported", "waived"} and not str(row.get("owner_decision", "")).strip():
            errors.append(f"{status} row {key} must include owner_decision")
        if status == "deferred" and not str(row.get("promotion_criteria", "")).strip():
            errors.append(f"Deferred row {key} must include promotion_criteria")
        if args.release and status == "deferred":
            errors.append(f"Release policy blocks deferred row {key}; promote to supported or mark unsupported/waived with owner_decision")

    missing_required = REQUIRED_ROWS - coverage_keys
    for key in sorted(missing_required):
        errors.append(f"Missing required product/platform/mode coverage row: {key}")

    if supported_count == 0:
        errors.append("Matrix status has no supported rows")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Matrix status verified: {len(rows)} rows, {supported_count} supported")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
