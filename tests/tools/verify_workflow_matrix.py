#!/usr/bin/env python3
"""Verify that GitHub workflow jobs match the automated testing matrix."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REQUIRED_JOBS = {
    ".github/workflows/build.yml": {
        "build-windows": ["Verify IMM content baseline", "Record Windows DirectX render metrics", "Write Windows DirectX render report", "Write Windows viewer artifact manifest", "Collect Windows viewer artifact summary"],
        "build-android": ["Write Android viewer artifact manifest", "Collect Android viewer artifact summary"],
        "build-macos": ["Smoke macOS Metal standalone viewer", "Record macOS Metal render metrics", "Write macOS Metal render report", "Write macOS viewer artifact manifest", "Collect macOS viewer artifact summary"],
        "build-ios": ["Link-check iOS Unity plugin"],
        "package-unity-plugins": ["Verify stroke reader Unity package layout", "Verify Unity package import harness", "Write ImmUnity package manifest", "Collect ImmUnity package summary"],
        "package-godot-extension": ["Verify Godot addon package layout", "Verify Godot package import harness", "Write Godot addon package manifest", "Collect Godot addon package summary"],
        "release": ["Verify release matrix status", "Verify downloaded release assets", "Write release matrix audit report", "Generate release IMM baseline actual", "Write release baseline drift report", "Verify release IMM baseline", "Write release validation manifest", "Collect release artifact summary", "Create GitHub release"],
    },
    ".github/workflows/ci-core.yml": {
        "baseline-content": ["Verify sample1 IMM baseline", "Write baseline drift report", "Write CI manifest", "Collect artifact summary"],
        "package-source-layout": ["Verify Unity stroke reader package source", "Verify Unity source import harness", "Verify Godot addon source manifest", "Verify Godot source import harness", "Collect artifact summary"],
        "matrix-status": ["Verify matrix status coverage", "Verify workflow matrix wiring", "Write matrix audit report", "Run CI tool self-tests", "Write CI manifest", "Collect artifact summary"],
        "godot-local-verifier": ["Run Godot local verifier", "Write CI manifest", "Collect artifact summary"],
    },
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": ["Preflight Android device", "Run Android GLES smoke", "Record Android GLES screenshot metrics", "Write Android GLES screenshot report", "Write CI manifest", "Collect artifact summary"],
        "android-standalone-vulkan": ["Preflight Android Vulkan device", "Run Android Vulkan smoke", "Record Android Vulkan screenshot metrics", "Write Android Vulkan screenshot report", "Write CI manifest", "Collect artifact summary"],
        "android-openxr-probe": ["Preflight Quest OpenXR device", "Run Android OpenXR probe smoke", "Verify OpenXR log contract", "Write CI manifest", "Collect artifact summary"],
        "android-godot-vulkan": ["Preflight Android Godot device", "Run Android Godot Vulkan smoke", "Record Android Godot Vulkan screenshot metrics", "Write Android Godot Vulkan screenshot report", "Write CI manifest", "Collect artifact summary"],
        "android-quest-vr": ["Preflight Quest VR device", "Run Quest VR app smoke", "Verify Quest VR log contract", "Write CI manifest", "Collect artifact summary"],
        "ios-device-smoke": ["Preflight iOS device runner", "Verify iOS package target", "Write CI manifest", "Collect artifact summary"],
        "device-evidence-report": ["Download device artifacts", "Verify device matrix evidence", "Upload device evidence report"],
    },
    ".github/workflows/ci-engine.yml": {
        "unity-package-import": ["Verify Unity package import harness", "Preflight Unity runner", "Run Unity batchmode package import tests", "Write CI manifest", "Collect artifact summary"],
        "unity-windows-directx-composition": ["Preflight Unity DirectX runner", "Run Unity DirectX composition smoke", "Record Unity DirectX composition metrics", "Write Unity DirectX composition report", "Verify Unity DirectX composition log contract", "Write CI manifest", "Collect artifact summary"],
        "unity-windows-openxr-vr": ["Preflight Unity OpenXR VR runner", "Run Unity OpenXR VR smoke", "Record Unity OpenXR VR metrics", "Write Unity OpenXR VR render report", "Verify Unity OpenXR VR log contract", "Write CI manifest", "Collect artifact summary"],
        "godot-package-import": ["Run Godot local verifier", "Verify Godot package import harness", "Write CI manifest", "Collect artifact summary"],
        "engine-evidence-report": ["Download engine artifacts", "Verify engine matrix evidence", "Upload engine evidence report"],
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-vulkan": ["Install Mesa lavapipe", "Configure Mesa lavapipe Vulkan ICD", "Preflight GPU runner", "Run Vulkan smoke against baseline", "Compare Vulkan render metrics against DirectX baseline", "Stage Vulkan capture evidence", "Write Vulkan render report", "Write CI manifest", "Collect artifact summary"],
        "windows-standalone-openxr-vr": ["Preflight Windows OpenXR VR runner", "Run Windows OpenXR VR smoke", "Verify Windows OpenXR VR log contract", "Write CI manifest", "Collect artifact summary"],
        "windows-standalone-opengl-vr": ["Preflight Windows OpenGL VR runner", "Run Windows OpenGL VR smoke", "Verify Windows OpenGL VR log contract", "Write CI manifest", "Collect artifact summary"],
        "windows-godot-vulkan": ["Install Mesa lavapipe", "Configure Mesa lavapipe Vulkan ICD", "Preflight Godot Vulkan runner", "Build Windows viewer", "Capture DirectX reference", "Run Godot Vulkan visual baseline smoke", "Compare Godot Vulkan render metrics against DirectX baseline", "Write Godot Vulkan render report", "Write CI manifest", "Collect artifact summary"],
        "windows-godot-openxr-vr": ["Preflight Godot OpenXR VR runner", "Build Godot extension", "Run Godot OpenXR VR smoke", "Verify Godot OpenXR VR log contract", "Write CI manifest", "Collect artifact summary"],
        "macos-godot-metal": ["Preflight Godot Metal runner", "Run Godot Metal visual smoke", "Record Godot Metal render metrics", "Write Godot Metal render report", "Write CI manifest", "Collect artifact summary"],
        "gpu-evidence-report": ["Download GPU artifacts", "Verify GPU matrix evidence", "Upload GPU evidence report"],
    },
}

REQUIRED_RUNS_ON = {
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": {"macos-14"},
        "android-standalone-vulkan": {"macos-14"},
        "android-openxr-probe": {"self-hosted", "quest", "openxr"},
        "android-godot-vulkan": {"macos-14"},
        "android-quest-vr": {"self-hosted", "quest", "vr"},
        "ios-device-smoke": {"macos-14"},
    },
    ".github/workflows/ci-engine.yml": {
        "unity-package-import": {"windows-latest"},
        "unity-windows-directx-composition": {"windows-latest"},
        "unity-windows-openxr-vr": {"self-hosted", "windows", "unity", "vr"},
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-vulkan": {"windows-latest"},
        "windows-standalone-openxr-vr": {"self-hosted", "windows", "gpu", "vr", "openxr"},
        "windows-standalone-opengl-vr": {"self-hosted", "windows", "gpu", "vr", "opengl"},
        "windows-godot-vulkan": {"windows-latest"},
        "windows-godot-openxr-vr": {"self-hosted", "windows", "gpu", "godot", "vr", "openxr"},
        "macos-godot-metal": {"macos-14"},
    },
}
REQUIRED_WORKFLOW_TRIGGERS = {
    ".github/workflows/ci-validation.yml": ["ci-core.yml", "ci-device.yml", "ci-engine.yml", "ci-gpu.yml"],
}
REQUIRED_VALIDATION_GATE = "github.event_name == 'workflow_dispatch' || github.event_name == 'schedule' || contains(github.event.head_commit.message, '[CI VALIDATION]')"
GATED_VALIDATION_JOBS = ["device", "engine", "gpu"]


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


def verify_manifest_status_arguments(path: Path, workflow_rel: str, errors: list[str]) -> None:
    lines = path.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if "tests/tools/write_ci_manifest.py" not in line and "tests\\tools\\write_ci_manifest.py" not in line:
            continue
        window = "\n".join(lines[index : min(index + 4, len(lines))])
        if "--status" not in window:
            errors.append(f"{workflow_rel}:{index + 1} write_ci_manifest.py invocation must pass --status")
        if "--failure-class" not in window:
            errors.append(f"{workflow_rel}:{index + 1} write_ci_manifest.py invocation must pass --failure-class")


def verify_reusable_workflow(path: Path, workflow_rel: str, errors: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    if "workflow_call:" not in text:
        errors.append(f"{workflow_rel} must be reusable through workflow_call")


def verify_consolidated_workflow(path: Path, workflow_rel: str, workflow_files: list[str], errors: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    required_tokens = [
        "workflow_dispatch:",
        "schedule:",
        "push:",
        "- main",
        "- develop",
        "- feature/**",
        "pull_request:",
        "types: [opened, synchronize, reopened, labeled]",
    ]
    required_tokens.extend(f"uses: ./.github/workflows/{workflow_file}" for workflow_file in workflow_files)
    for token in required_tokens:
        if token not in text:
            errors.append(f"{workflow_rel} missing trigger/guard token: {token}")
    for job_name in GATED_VALIDATION_JOBS:
        match = re.search(rf"^  {re.escape(job_name)}:\n(?P<body>(?:    .*\n?)*)", text, re.MULTILINE)
        if not match:
            errors.append(f"{workflow_rel} missing gated validation job: {job_name}")
            continue
        job_block = match.group("body")
        if f"if: {REQUIRED_VALIDATION_GATE}" not in job_block:
            errors.append(f"{workflow_rel} job {job_name} must be gated by [CI VALIDATION], schedule, or workflow_dispatch")


def verify_release_assets(path: Path, workflow_rel: str, errors: list[str]) -> None:
    if workflow_rel != ".github/workflows/build.yml":
        return
    text = path.read_text(encoding="utf-8")
    for asset in [
        "release-assets/matrix-audit.json",
        "release-assets/matrix-audit.md",
        "release-assets/baseline-drift.json",
        "release-assets/baseline-drift.md",
    ]:
        if asset not in text:
            errors.append(f"{workflow_rel} release job must attach/include {asset}")


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
        if workflow_rel.startswith(".github/workflows/ci-") and workflow_rel != ".github/workflows/ci-validation.yml":
            verify_reusable_workflow(workflow_path, workflow_rel, errors)
        verify_manifest_status_arguments(workflow_path, workflow_rel, errors)
        verify_release_assets(workflow_path, workflow_rel, errors)
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

    for workflow_rel, workflow_files in REQUIRED_WORKFLOW_TRIGGERS.items():
        workflow_path = root / workflow_rel
        if not workflow_path.exists():
            errors.append(f"Missing workflow: {workflow_rel}")
            continue
        verify_consolidated_workflow(workflow_path, workflow_rel, workflow_files, errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    checked = sum(len(jobs) for jobs in REQUIRED_JOBS.values())
    print(f"Workflow matrix verified: {checked} jobs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
