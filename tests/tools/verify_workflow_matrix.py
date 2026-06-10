#!/usr/bin/env python3
"""Verify that GitHub workflow jobs match the automated testing matrix."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


REQUIRED_JOBS = {
    ".github/workflows/build.yml": {
        "build-windows": ["Verify IMM content baseline", "Record Windows DirectX render metrics", "Write Windows viewer artifact manifest"],
        "build-android": ["Write Android viewer artifact manifest"],
        "build-macos": ["Write macOS viewer artifact manifest"],
        "build-ios": ["Link-check iOS Unity plugin"],
        "package-unity-plugins": ["Verify stroke reader Unity package layout", "Write ImmUnity package manifest"],
        "package-godot-extension": ["Verify Godot addon package layout", "Write Godot addon package manifest"],
        "release": ["Verify release matrix status", "Verify downloaded release assets", "Write release validation manifest"],
    },
    ".github/workflows/ci-core.yml": {
        "baseline-content": ["Verify sample1 IMM baseline", "Write CI manifest"],
        "package-source-layout": ["Verify Unity stroke reader package source", "Verify Godot addon source manifest"],
        "matrix-status": ["Verify matrix status coverage", "Write CI manifest"],
        "godot-local-verifier": ["Run Godot local verifier", "Write CI manifest"],
    },
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": ["Preflight Android device", "Run Android GLES smoke", "Write CI manifest"],
        "android-standalone-vulkan": ["Preflight Android Vulkan device", "Run Android Vulkan smoke", "Write CI manifest"],
        "android-openxr-probe": ["Preflight Quest OpenXR device", "Run Android OpenXR probe smoke", "Write CI manifest"],
        "android-godot-vulkan": ["Preflight Android Godot device", "Run Android Godot Vulkan smoke", "Write CI manifest"],
    },
    ".github/workflows/ci-engine.yml": {
        "unity-package-import": ["Preflight Unity runner", "Run Unity batchmode package import tests", "Write CI manifest"],
        "godot-package-import": ["Run Godot local verifier", "Write CI manifest"],
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-vulkan": ["Preflight GPU runner", "Run Vulkan smoke against baseline", "Write CI manifest"],
        "windows-godot-vulkan": ["Preflight Godot Vulkan runner", "Run Godot Vulkan smoke", "Write CI manifest"],
        "macos-godot-metal": ["Preflight Godot Metal runner", "Run Godot Metal visual smoke", "Write CI manifest"],
    },
}

REQUIRED_RUNS_ON = {
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": {"self-hosted", "android-device"},
        "android-standalone-vulkan": {"self-hosted", "android-device", "vulkan"},
        "android-openxr-probe": {"self-hosted", "quest", "openxr"},
        "android-godot-vulkan": {"self-hosted", "android-device", "godot", "vulkan"},
    },
    ".github/workflows/ci-engine.yml": {
        "unity-package-import": {"self-hosted", "unity"},
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-vulkan": {"self-hosted", "windows", "gpu", "vulkan"},
        "windows-godot-vulkan": {"self-hosted", "windows", "gpu", "godot", "vulkan"},
        "macos-godot-metal": {"self-hosted", "macos", "gpu", "metal", "godot"},
    },
}


def step_names(job: dict) -> set[str]:
    return {step.get("name", "") for step in job.get("steps", []) if isinstance(step, dict)}


def runs_on_labels(value: object) -> set[str]:
    if isinstance(value, str):
        return {value}
    if isinstance(value, list):
        return {str(item) for item in value}
    return set()


def load_workflow(path: Path) -> dict:
    """Load enough workflow structure for this verifier without external deps."""
    lines = path.read_text(encoding="utf-8").splitlines()
    jobs: dict[str, dict] = {}
    current_job: str | None = None
    in_steps = False

    for raw in lines:
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue

        indent = len(raw) - len(raw.lstrip(" "))
        if indent == 2 and stripped.endswith(":") and not stripped.startswith("-"):
            name = stripped[:-1]
            if name not in {"on", "env", "permissions", "defaults", "concurrency", "jobs"}:
                current_job = name
                jobs[current_job] = {"steps": [], "runs-on": None}
                in_steps = False
            continue

        if current_job is None:
            continue

        if indent == 4 and stripped.startswith("runs-on:"):
            value = stripped.split(":", 1)[1].strip()
            if value.startswith("[") and value.endswith("]"):
                jobs[current_job]["runs-on"] = [item.strip() for item in value[1:-1].split(",") if item.strip()]
            else:
                jobs[current_job]["runs-on"] = value
            continue

        if indent == 4 and stripped == "steps:":
            in_steps = True
            continue

        if in_steps and indent >= 4 and stripped.startswith("- name:"):
            jobs[current_job]["steps"].append({"name": stripped.split(":", 1)[1].strip().strip("'\"")})

    return {"jobs": jobs}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    args = parser.parse_args()

    root = args.repo_root.resolve()
    errors: list[str] = []

    for workflow_rel, jobs in REQUIRED_JOBS.items():
        workflow_path = root / workflow_rel
        if not workflow_path.exists():
            errors.append(f"Missing workflow: {workflow_rel}")
            continue
        workflow = load_workflow(workflow_path)
        actual_jobs = workflow.get("jobs", {})
        for job_name, required_steps in jobs.items():
            job = actual_jobs.get(job_name)
            if not job:
                errors.append(f"{workflow_rel} missing job {job_name}")
                continue
            names = step_names(job)
            for required_step in required_steps:
                if required_step not in names:
                    errors.append(f"{workflow_rel} job {job_name} missing step {required_step!r}")

            required_labels = REQUIRED_RUNS_ON.get(workflow_rel, {}).get(job_name)
            if required_labels:
                actual_labels = runs_on_labels(job.get("runs-on"))
                missing_labels = required_labels - actual_labels
                if missing_labels:
                    errors.append(f"{workflow_rel} job {job_name} missing runs-on labels: {sorted(missing_labels)}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    checked = sum(len(jobs) for jobs in REQUIRED_JOBS.values())
    print(f"Workflow matrix verified: {checked} jobs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
