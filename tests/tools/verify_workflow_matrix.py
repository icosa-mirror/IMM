#!/usr/bin/env python3
"""Verify that GitHub workflow jobs match the automated testing matrix."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


REQUIRED_JOBS = {
    ".github/workflows/build.yml": {
        "build-windows": ["Verify IMM content baseline", "Record Windows DirectX render metrics", "Compare Windows Vulkan render metrics against committed DirectX baseline", "Write Windows DirectX render report", "Write Windows viewer artifact manifest", "Collect Windows viewer artifact summary"],
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
        "core-evidence-report": ["Download core artifacts", "Verify core matrix evidence", "Upload core evidence report", "Hide per-lane core artifacts"],
    },
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": ["Check Firebase Test Lab configuration", "Build Android GLES APKs", "Run Android GLES smoke in Firebase Test Lab", "Record Android GLES screenshot metrics", "Write Android GLES screenshot report", "Write CI manifest", "Collect artifact summary"],
        "android-standalone-vulkan": ["Check Firebase Test Lab configuration", "Build Android Vulkan APKs", "Run Android Vulkan smoke in Firebase Test Lab", "Record Android Vulkan screenshot metrics", "Write Android Vulkan screenshot report", "Write CI manifest", "Collect artifact summary"],
        "android-openxr-probe": ["Preflight Quest OpenXR device", "Run Android OpenXR probe smoke", "Verify OpenXR log contract", "Write CI manifest", "Collect artifact summary"],
        "android-godot-vulkan": ["Check Firebase Test Lab configuration", "Build Android Godot APK", "Run Android Godot Vulkan smoke in Firebase Test Lab", "Record Android Godot Vulkan screenshot metrics", "Write Android Godot Vulkan screenshot report", "Write CI manifest", "Collect artifact summary"],
        "android-quest-vr": ["Preflight Quest VR device", "Run Quest VR app smoke", "Verify Quest VR log contract", "Write CI manifest", "Collect artifact summary"],
        "ios-device-smoke": ["Preflight iOS device runner", "Verify iOS package target", "Write CI manifest", "Collect artifact summary"],
        "device-evidence-report": ["Download device artifacts", "Verify device matrix evidence", "Upload device evidence report", "Hide per-lane device artifacts"],
    },
    ".github/workflows/ci-engine.yml": {
        "unity-windows-native-plugin-build": ["Download same-commit Unity native plugin build artifact", "Stage same-commit Unity native plugin", "Verify Unity native plugin exports", "Upload same-commit Unity native plugin"],
        "unity-package-import": ["Verify Unity package import harness", "Preflight Unity runner", "Run Unity batchmode package import tests", "Write CI manifest", "Collect artifact summary"],
        "unity-macos-metal-composition": ["Download same-commit macOS Unity package", "Stage same-commit macOS Unity native plugin", "Preflight Unity macOS Metal runner", "Build Unity macOS Metal smoke player", "Run Unity macOS Metal visual smokes", "Classify Unity macOS Metal visual smokes", "Record Unity macOS Metal render metrics", "Write Unity macOS Metal render reports", "Verify Unity macOS Metal log contract", "Write CI manifest", "Collect artifact summary", "Upload Unity macOS Metal artifacts"],
        "unity-windows-directx-player-build": ["Download same-commit Unity native plugin", "Preflight Unity DirectX runner", "Build Unity DirectX smoke player", "Write CI manifest", "Collect artifact summary", "Upload Unity DirectX smoke player", "Upload Unity DirectX build artifacts"],
        "unity-windows-directx-composition": ["Preflight Unity DirectX runner", "Run Unity DirectX composition smoke", "Compare Unity DirectX render metrics against committed DirectX baseline", "Write Unity DirectX composition report", "Verify Unity DirectX composition log contract", "Write CI manifest", "Collect artifact summary"],
        "unity-windows-vulkan-player-build": ["Download same-commit Unity native plugin", "Preflight Unity Vulkan runner", "Build Unity Vulkan smoke player", "Write CI manifest", "Collect artifact summary", "Upload Unity Vulkan smoke player", "Upload Unity Vulkan build artifacts"],
        "unity-windows-vulkan-ordered-overlay": ["Preflight Unity Vulkan runner", "Run Unity Vulkan ordered overlay smoke", "Classify Unity Vulkan ordered overlay status", "Write Unity Vulkan ordered overlay report", "Stage Unity Vulkan ordered overlay capture evidence", "Verify Unity Vulkan ordered overlay log contract", "Verify Unity Vulkan ordered overlay native render contract", "Write CI manifest", "Collect artifact summary"],
        "unity-windows-vulkan-full-depth": ["Preflight Unity Vulkan runner", "Run Unity Vulkan full depth smoke", "Classify Unity Vulkan full depth status", "Write Unity Vulkan full depth report", "Stage Unity Vulkan full depth capture evidence", "Verify Unity Vulkan full depth log contract", "Verify Unity Vulkan full depth native render contract", "Write CI manifest", "Collect artifact summary"],
        "unity-windows-openxr-vr": ["Download same-commit Windows native plugin", "Preflight Unity OpenXR VR runner", "Run Unity OpenXR VR smoke", "Record Unity OpenXR VR metrics", "Write Unity OpenXR VR render report", "Verify Unity OpenXR VR log contract", "Write CI manifest", "Collect artifact summary"],
        "godot-package-import": ["Run Godot local verifier", "Verify Godot package import harness", "Write CI manifest", "Collect artifact summary"],
        "engine-evidence-report": ["Download engine artifacts", "Verify engine matrix evidence", "Write engine visual evidence report", "Write engine aggregate status manifests", "Upload engine visual evidence", "Hide per-lane engine artifacts"],
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-directx": ["Download Windows viewer build artifact", "Stage Windows viewer build artifact", "Preflight DirectX runner", "Capture DirectX sample1", "Compare DirectX render metrics against committed DirectX baseline", "Write DirectX render report", "Write CI manifest", "Collect artifact summary"],
        "windows-standalone-vulkan": ["Install Mesa lavapipe", "Configure Mesa lavapipe Vulkan ICD", "Preflight GPU runner", "Run Vulkan smoke against baseline", "Compare Vulkan render metrics against committed DirectX baseline", "Stage Vulkan capture evidence", "Write Vulkan render report", "Write CI manifest", "Collect artifact summary"],
        "windows-standalone-opengl": ["Download Windows viewer build artifact", "Stage Windows viewer build artifact", "Install Mesa llvmpipe OpenGL", "Configure Mesa llvmpipe OpenGL", "Preflight OpenGL runner", "Capture OpenGL sample1", "Compare OpenGL render metrics against committed DirectX baseline", "Write OpenGL render report", "Write CI manifest", "Collect artifact summary"],
        "windows-standalone-openxr-vr": ["Download Windows viewer build artifact", "Stage Windows viewer build artifact", "Preflight Windows OpenXR VR runner", "Run Windows OpenXR VR smoke", "Verify Windows OpenXR VR log contract", "Write CI manifest", "Collect artifact summary"],
        "windows-standalone-opengl-vr": ["Download Windows viewer build artifact", "Stage Windows viewer build artifact", "Preflight Windows OpenGL VR runner", "Run Windows OpenGL VR smoke", "Verify Windows OpenGL VR log contract", "Write CI manifest", "Collect artifact summary"],
        "windows-godot-vulkan": ["Download Windows viewer build artifact", "Stage Windows viewer build artifact", "Download Windows Godot extension build artifact", "Stage Windows Godot extension build artifact", "Install Mesa lavapipe", "Configure Mesa lavapipe Vulkan ICD", "Preflight Godot Vulkan runner", "Run Godot Vulkan visual baseline smoke", "Record Godot Vulkan full depth metrics", "Write Godot Vulkan render report", "Run Godot Vulkan ordered overlay smoke", "Record Godot Vulkan ordered overlay metrics", "Write Godot Vulkan ordered overlay report", "Write CI manifest", "Collect artifact summary"],
        "windows-godot-openxr-vr": ["Download Windows Godot extension build artifact", "Stage Windows Godot extension build artifact", "Preflight Godot OpenXR VR runner", "Run Godot OpenXR VR smoke", "Verify Godot OpenXR VR log contract", "Write CI manifest", "Collect artifact summary"],
        "macos-standalone-metal": ["Download macOS viewer build artifact", "Preflight macOS Metal runner", "Verify macOS Metal build artifact", "Record macOS Metal render metrics", "Write macOS Metal render report", "Write CI manifest", "Collect artifact summary"],
        "macos-godot-metal": ["Preflight Godot Metal runner", "Download macOS Godot extension build artifact", "Stage macOS Godot extension build artifact", "Run Godot Metal visual smoke", "Record Godot Metal render metrics", "Write Godot Metal render report", "Write CI manifest", "Collect artifact summary"],
        "gpu-evidence-report": ["Download GPU artifacts", "Verify GPU matrix evidence", "Upload GPU evidence report", "Hide per-lane GPU artifacts"],
    },
    ".github/workflows/web-pages.yml": {
        "build": ["Build Wasm decoder", "Run playback tests", "Run browser smoke test", "Build Pages bundle", "Verify Pages bundle layout", "Upload Pages artifact"],
        "verify": ["Download decoder artifact", "Run extended browser verification", "Upload failed browser evidence"],
        "verify-firefox": ["Install Playwright Firefox and system dependencies", "Download decoder artifact", "Run Firefox browser verification", "Upload failed Firefox evidence"],
    },
}

REQUIRED_RUNS_ON = {
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": {"ubuntu-latest"},
        "android-standalone-vulkan": {"ubuntu-latest"},
        "android-openxr-probe": {"self-hosted", "quest", "openxr"},
        "android-godot-vulkan": {"ubuntu-latest"},
        "android-quest-vr": {"self-hosted", "quest", "vr"},
        "ios-device-smoke": {"macos-14"},
    },
    ".github/workflows/ci-engine.yml": {
        "unity-windows-native-plugin-build": {"windows-latest"},
        "unity-package-import": {"ubuntu-latest"},
        "unity-macos-metal-composition": {"macos-14"},
        "unity-windows-directx-player-build": {"ubuntu-latest"},
        "unity-windows-directx-composition": {"windows-latest"},
        "unity-windows-vulkan-player-build": {"ubuntu-latest"},
        "unity-windows-vulkan-ordered-overlay": {"self-hosted", "windows"},
        "unity-windows-vulkan-full-depth": {"self-hosted", "windows"},
        "unity-windows-openxr-vr": {"self-hosted", "windows", "unity", "vr"},
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-directx": {"windows-latest"},
        "windows-standalone-vulkan": {"windows-latest"},
        "windows-standalone-opengl": {"windows-latest"},
        "windows-standalone-openxr-vr": {"self-hosted", "windows", "gpu", "vr", "openxr"},
        "windows-standalone-opengl-vr": {"self-hosted", "windows", "gpu", "vr", "opengl"},
        "windows-godot-vulkan": {"windows-latest"},
        "windows-godot-openxr-vr": {"self-hosted", "windows", "gpu", "godot", "vr", "openxr"},
        "macos-standalone-metal": {"macos-14"},
        "macos-godot-metal": {"macos-15"},
    },
}
REQUIRED_JOB_TIMEOUTS = {
    ".github/workflows/ci-core.yml": {
        "baseline-content",
        "package-source-layout",
        "matrix-status",
        "godot-local-verifier",
        "core-evidence-report",
    },
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles",
        "android-standalone-vulkan",
        "android-openxr-probe",
        "android-godot-vulkan",
        "android-quest-vr",
        "ios-device-smoke",
        "device-evidence-report",
    },
    ".github/workflows/ci-engine.yml": {
        "unity-windows-native-plugin-build",
        "unity-package-import",
        "unity-macos-metal-composition",
        "unity-windows-directx-player-build",
        "unity-windows-directx-composition",
        "unity-windows-vulkan-player-build",
        "unity-windows-vulkan-ordered-overlay",
        "unity-windows-vulkan-full-depth",
        "unity-windows-openxr-vr",
        "godot-package-import",
        "engine-evidence-report",
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-directx",
        "windows-standalone-vulkan",
        "windows-standalone-opengl",
        "windows-standalone-openxr-vr",
        "windows-standalone-opengl-vr",
        "windows-godot-vulkan",
        "windows-godot-openxr-vr",
        "macos-standalone-metal",
        "macos-godot-metal",
        "gpu-evidence-report",
    },
}
REQUIRED_STEP_TIMEOUTS = {
    ".github/workflows/ci-device.yml": {
        "android-standalone-gles": {"Run Android GLES smoke in Firebase Test Lab"},
        "android-standalone-vulkan": {"Run Android Vulkan smoke in Firebase Test Lab"},
        "android-openxr-probe": {"Run Android OpenXR probe smoke"},
        "android-godot-vulkan": {"Run Android Godot Vulkan smoke in Firebase Test Lab"},
        "android-quest-vr": {"Run Quest VR app smoke"},
    },
    ".github/workflows/ci-engine.yml": {
        "unity-package-import": {"Run Unity batchmode package import tests"},
        "unity-macos-metal-composition": {"Build Unity macOS Metal smoke player", "Run Unity macOS Metal visual smokes"},
        "unity-windows-directx-composition": {"Run Unity DirectX composition smoke"},
        "unity-windows-vulkan-ordered-overlay": {"Run Unity Vulkan ordered overlay smoke"},
        "unity-windows-vulkan-full-depth": {"Run Unity Vulkan full depth smoke"},
        "unity-windows-openxr-vr": {"Run Unity OpenXR VR smoke"},
    },
    ".github/workflows/ci-gpu.yml": {
        "windows-standalone-directx": {"Capture DirectX sample1"},
        "windows-standalone-vulkan": {"Run Vulkan smoke against baseline"},
        "windows-standalone-opengl": {"Capture OpenGL sample1"},
        "windows-standalone-openxr-vr": {"Run Windows OpenXR VR smoke"},
        "windows-standalone-opengl-vr": {"Run Windows OpenGL VR smoke"},
        "windows-godot-vulkan": {"Run Godot Vulkan visual baseline smoke"},
        "windows-godot-openxr-vr": {"Run Godot OpenXR VR smoke"},
        "macos-standalone-metal": {"Verify macOS Metal build artifact"},
        "macos-godot-metal": {"Run Godot Metal visual smoke"},
    },
}
REQUIRED_WORKFLOW_TRIGGERS = {
    ".github/workflows/ci-validation.yml": ["build.yml", "web-pages.yml", "ci-core.yml", "ci-device.yml", "ci-engine.yml", "ci-gpu.yml"],
}
GATED_VALIDATION_JOBS = ["device", "engine", "gpu"]
VALIDATION_JOB_LABELS = {
    "device": "device-ci",
    "engine": "engine-ci",
    "gpu": "gpu-ci",
}


def step_names(job: dict) -> set[str]:
    return {step.get("name", "") for step in job.get("steps", []) if isinstance(step, dict)}


def has_positive_timeout(value: object) -> bool:
    try:
        return int(str(value)) > 0
    except ValueError:
        return False


def steps_by_name(job: dict) -> dict[str, dict]:
    return {step.get("name", ""): step for step in job.get("steps", []) if isinstance(step, dict)}


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
    current_step: dict | None = None
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
                jobs[current_job] = {"steps": [], "runs-on": None, "timeout-minutes": None}
                current_step = None
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

        if indent == 4 and stripped.startswith("timeout-minutes:"):
            jobs[current_job]["timeout-minutes"] = stripped.split(":", 1)[1].strip()
            continue

        if indent == 4 and stripped == "steps:":
            in_steps = True
            continue

        if in_steps and indent >= 4 and stripped.startswith("- name:"):
            current_step = {"name": stripped.split(":", 1)[1].strip().strip("'\""), "timeout-minutes": None}
            jobs[current_job]["steps"].append(current_step)
            continue

        if in_steps and current_step is not None and indent >= 8 and stripped.startswith("timeout-minutes:"):
            current_step["timeout-minutes"] = stripped.split(":", 1)[1].strip()

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
        for token in [
            "github.event_name == 'schedule'",
            "github.event_name == 'workflow_dispatch'",
            "inputs.mode == 'full'",
            "inputs.mode == 'hardware'",
            "inputs.mode == 'release'",
            "hardware: ${{ github.event_name == 'workflow_dispatch' && inputs.mode == 'hardware' }}",
            "contains(github.event.head_commit.message, '[CI VALIDATION]')",
            "contains(github.event.head_commit.message, '[RELEASE]')",
            f"contains(github.event.pull_request.labels.*.name, '{VALIDATION_JOB_LABELS[job_name]}')",
        ]:
            if token not in job_block:
                errors.append(f"{workflow_rel} job {job_name} missing validation gate token: {token}")


def verify_build_orchestration_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    if workflow_rel == ".github/workflows/build.yml":
        for forbidden in ["workflow_dispatch:", "\n  push:", "\n  pull_request:", "\n  schedule:"]:
            if forbidden in text:
                errors.append(f"{workflow_rel} must be reusable only; found event trigger {forbidden.strip()}")
        for token in [
            "workflow_call:",
            "mode:",
            "sync_binaries:",
            "release:",
            "if: inputs.mode == 'build'",
            "inputs.mode == 'publish'",
            "msbuild imm.sln /t:Rebuild",
            "macos-standalone-metal-sample1.json",
            "[skip ci]",
        ]:
            if token not in text:
                errors.append(f"{workflow_rel} missing build/publish orchestration token: {token}")
        return

    if workflow_rel != ".github/workflows/ci-validation.yml":
        return
    for token in [
        "name: Build and Validation Pipeline",
        "- quick",
        "- build",
        "- full",
        "- hardware",
        "- release",
        "uses: ./.github/workflows/build.yml",
        "uses: ./.github/workflows/web-pages.yml",
        "actions: write",
        "contents: write",
        "mode: build",
        "mode: publish",
        "needs.validation-evidence.result == 'success'",
        "needs.web.result == 'success'",
        "name: Deploy Web Player Pages",
        "uses: actions/deploy-pages@v5",
    ]:
        if token not in text:
            errors.append(f"{workflow_rel} missing unified pipeline token: {token}")


def verify_self_hosted_hardware_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    hardware_workflows = {
        ".github/workflows/ci-device.yml",
        ".github/workflows/ci-engine.yml",
        ".github/workflows/ci-gpu.yml",
    }
    if workflow_rel not in hardware_workflows:
        return

    text = path.read_text(encoding="utf-8")
    for token in [
        "workflow_call:",
        "hardware:",
        "description: Run explicitly requested jobs requiring private hardware.",
        "default: false",
        "type: boolean",
    ]:
        if token not in text:
            errors.append(f"{workflow_rel} missing hardware input contract token: {token}")

    for match in re.finditer(
        r"^  (?P<name>[A-Za-z0-9_-]+):\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        text,
        re.MULTILINE | re.DOTALL,
    ):
        job_name = match.group("name")
        body = match.group("body")
        self_hosted = re.search(r"^    runs-on:.*\bself-hosted\b", body, re.MULTILINE) is not None
        hardware_gated = "inputs.hardware" in body
        if self_hosted and not hardware_gated:
            errors.append(f"{workflow_rel} self-hosted job {job_name} must require inputs.hardware")
        if hardware_gated and not self_hosted:
            errors.append(f"{workflow_rel} hosted job {job_name} must not require inputs.hardware")


def verify_web_pipeline_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    if workflow_rel == ".github/workflows/web-pages.yml":
        for token in ["workflow_call:", "uses: actions/upload-pages-artifact@v5"]:
            if token not in text:
                errors.append(f"{workflow_rel} missing reusable web build token: {token}")
        for forbidden in ["\n  push:", "\n  pull_request:", "\n  paths:", "uses: actions/deploy-pages@"]:
            if forbidden in text:
                errors.append(f"{workflow_rel} must not trigger or deploy independently; found {forbidden.strip()}")
        return

    if workflow_rel != ".github/workflows/ci-validation.yml":
        return

    match = re.search(
        r"^  web:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        text,
        re.MULTILINE | re.DOTALL,
    )
    if not match:
        errors.append(f"{workflow_rel} missing web orchestration job")
        return

    web_body = match.group("body")
    for token in [
        "github.event_name == 'pull_request'",
        "github.event_name == 'schedule'",
        "github.event_name == 'workflow_dispatch' && inputs.mode != 'quick'",
        "github.event_name == 'push'",
        "contains(github.event.head_commit.message, '[CI BUILD]')",
        "contains(github.event.head_commit.message, '[CI VALIDATION]')",
        "contains(github.event.head_commit.message, '[RELEASE]')",
        "uses: ./.github/workflows/web-pages.yml",
    ]:
        if token not in web_body:
            errors.append(f"{workflow_rel} web job missing build-equivalent gate token: {token}")

    deploy_match = re.search(
        r"^  deploy-pages:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        text,
        re.MULTILINE | re.DOTALL,
    )
    if not deploy_match:
        errors.append(f"{workflow_rel} missing Pages deployment job")
        return

    deploy_body = deploy_match.group("body")
    for token in [
        "needs.web.result == 'success'",
        "github.ref == 'refs/heads/main'",
        "uses: actions/deploy-pages@v5",
        "name: github-pages",
    ]:
        if token not in deploy_body:
            errors.append(f"{workflow_rel} deploy-pages job missing deployment gate token: {token}")


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


def verify_render_contract_references(root: Path, errors: list[str]) -> None:
    for contract_path in (root / "tests" / "baselines" / "render").glob("*.json"):
        try:
            contract = json.loads(contract_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            errors.append(f"{contract_path.relative_to(root).as_posix()} is not valid JSON: {exc}")
            continue
        reference_capture = contract.get("reference_capture")
        if not reference_capture:
            continue
        reference_path = root / str(reference_capture)
        if not reference_path.exists():
            errors.append(
                f"{contract_path.relative_to(root).as_posix()} references missing capture: {reference_capture}"
            )


def verify_unity_vulkan_full_depth_display_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    if workflow_rel != ".github/workflows/ci-engine.yml":
        return

    text = path.read_text(encoding="utf-8")
    match = re.search(
        r"^  unity-windows-vulkan-full-depth:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        text,
        re.MULTILINE | re.DOTALL,
    )
    if not match:
        return

    body = match.group("body")
    required_tokens = [
        '$env:IMM_UNITY_VK_USE_HOST_DEPTH = "1"',
        '$env:IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH = "1"',
        '--composition-mode full_depth',
        '--require "source=display"',
        '--require "composition playback freeze documents="',
        '--require "hostRenderPassHasDepth=1"',
        '--require "assumeHostDepth=0"',
        '--require "Vulkan renderer began host render pass frame with host depth"',
    ]
    for token in required_tokens:
        if token not in body:
            errors.append(f"{workflow_rel} unity-windows-vulkan-full-depth missing display-depth contract token: {token}")

    forbidden_tokens = [
        "IMM_UNITY_SMOKE_CAPTURE_CAMERA_TEXTURE",
    ]
    for token in forbidden_tokens:
        if token in body:
            errors.append(f"{workflow_rel} unity-windows-vulkan-full-depth must not use camera-target diagnostic token: {token}")


def verify_unity_same_commit_native_plugin_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    if workflow_rel != ".github/workflows/ci-engine.yml":
        return

    text = path.read_text(encoding="utf-8")
    job_names = [
        "unity-windows-native-plugin-build",
        "unity-macos-metal-composition",
        "unity-windows-directx-player-build",
        "unity-windows-vulkan-player-build",
    ]
    job_bodies: dict[str, str] = {}
    for job_name in job_names:
        match = re.search(
            rf"^  {re.escape(job_name)}:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
            text,
            re.MULTILINE | re.DOTALL,
        )
        if match:
            job_bodies[job_name] = match.group("body")

    native_body = job_bodies.get("unity-windows-native-plugin-build", "")
    for token in [
        "name: ImmPlayerPlugin-Unity",
        "path: artifacts/prebuilt-unity-native",
        "Copy-Item -Force $source.FullName $destination",
        "GetRenderEventAndDataFunc",
        "ConfigureVulkanRenderEvent",
        "name: UnityWindowsNativePluginSourceBuild",
    ]:
        if token not in native_body:
            errors.append(f"{workflow_rel} unity-windows-native-plugin-build missing same-commit contract token: {token}")

    for job_name in ["unity-windows-directx-player-build", "unity-windows-vulkan-player-build"]:
        body = job_bodies.get(job_name, "")
        for token in [
            "needs: unity-windows-native-plugin-build",
            "name: UnityWindowsNativePluginSourceBuild",
            "path: code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64",
            "versioning: None",
        ]:
            if token not in body:
                errors.append(f"{workflow_rel} {job_name} missing same-commit native plugin token: {token}")

    macos_body = job_bodies.get("unity-macos-metal-composition", "")
    for token in [
        "name: ImmPlayerPlugin-Unity",
        "Plugins/OSX/ImmUnityPlugin.bundle",
        'chmod +x "$plugin_binary"',
        "codesign --verify --deep --strict",
        "buildMethod: ImmPlayer.Editor.BuildAutomation.BuildMacOSMetalSmokePlayer",
        "Print :CFBundleExecutable",
        "enableGpu: true",
        "cacheUnityInstallationOnMac: true",
    ]:
        if token not in macos_body:
            errors.append(
                f"{workflow_rel} unity-macos-metal-composition missing same-commit native plugin token: {token}"
            )


def verify_godot_macos_clean_source_build_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    if workflow_rel not in {".github/workflows/build.yml", ".github/workflows/ci-gpu.yml"}:
        return

    text = path.read_text(encoding="utf-8")
    job_name = "build-macos" if workflow_rel == ".github/workflows/build.yml" else "macos-godot-metal"
    match = re.search(
        rf"^  {re.escape(job_name)}:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        text,
        re.MULTILINE | re.DOTALL,
    )
    if not match:
        return

    body = match.group("body")
    tokens = [
        "rm -f code/ImmGodotSampleProject/addons/imm_viewer/bin/macos/release/libimm_godot_extension.dylib",
        "rm -f code/ImmGodotSampleProject/addons/imm_viewer/bin/macos/release/libImmGodotPlugin.dylib",
        "rm -f code/ImmGodotSampleProject/addons/imm_viewer/bin/macos/debug/libimm_godot_extension.dylib",
        "rm -f code/ImmGodotSampleProject/addons/imm_viewer/bin/macos/debug/libImmGodotPlugin.dylib",
    ]
    if workflow_rel == ".github/workflows/build.yml":
        tokens.extend([
            "rm -f code/appImmGodotGDExtension/src/*.os",
            'arch="$(uname -m)"',
            'platform=macos target=template_release arch="$arch"',
        ])
    else:
        tokens.extend([
            "name: ImmPlayerPlugin-Godot",
            "Stage macOS Godot extension build artifact",
            "cp \"$extension\" code/ImmGodotSampleProject/addons/imm_viewer/bin/macos/debug/libimm_godot_extension.dylib",
            "cp \"$native\" code/ImmGodotSampleProject/addons/imm_viewer/bin/macos/debug/libImmGodotPlugin.dylib",
            "IMM_GODOT_DEBUG=1",
            'IMM_GODOT_LOG_FILE="$PWD/artifacts/godot-smoke-macos-metal/imm-godot-native.log"',
        ])
    for token in tokens:
        if token not in body:
            errors.append(f"{workflow_rel} {job_name} missing clean artifact contract token: {token}")


def verify_full_depth_validation_report_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    if workflow_rel != ".github/workflows/ci-validation.yml":
        return

    text = path.read_text(encoding="utf-8")
    required_tokens = [
        "Verify full depth validation evidence",
        "github.event_name == 'workflow_dispatch' && inputs.mode == 'hardware'",
        "needs.engine.result != 'skipped' && needs.gpu.result != 'skipped'",
        "python tests/tools/verify_full_depth_evidence_report.py",
        "--report artifacts/validation-evidence/view/VALIDATION_REPORT.md",
    ]
    for token in required_tokens:
        if token not in text:
            errors.append(f"{workflow_rel} missing full-depth validation report contract token: {token}")


def verify_ci_core_self_test_contract(path: Path, workflow_rel: str, errors: list[str]) -> None:
    if workflow_rel != ".github/workflows/ci-core.yml":
        return

    text = path.read_text(encoding="utf-8")
    required_tokens = [
        "python tests/tools/test_write_visual_evidence_report.py",
        "python tests/tools/test_verify_full_depth_evidence_report.py",
    ]
    for token in required_tokens:
        if token not in text:
            errors.append(f"{workflow_rel} missing CI tool self-test command: {token}")


def verify_godot_vulkan_smoke_contract(root: Path, errors: list[str]) -> None:
    script_rel = "code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1"
    script_path = root / script_rel
    if not script_path.exists():
        errors.append(f"Missing Godot Vulkan smoke script: {script_rel}")
        return

    text = script_path.read_text(encoding="utf-8")
    required_tokens = [
        "visual smoke scene composition diagnostics",
        "visual smoke PPM scene composition diagnostics",
        "ordered overlay IMM diagnostics",
        "scene composition full depth probe missing failed",
        "scene composition ordered overlay probe missing failed",
    ]
    for token in required_tokens:
        if token not in text:
            errors.append(f"{script_rel} missing composition smoke contract token: {token}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    args = parser.parse_args()

    root = args.repo_root.resolve()
    errors: list[str] = []

    verify_godot_vulkan_smoke_contract(root, errors)
    for workflow_rel, jobs in REQUIRED_JOBS.items():
        workflow_path = root / workflow_rel
        if not workflow_path.exists():
            errors.append(f"Missing workflow: {workflow_rel}")
            continue
        if workflow_rel in {".github/workflows/build.yml", ".github/workflows/web-pages.yml"} or (
            workflow_rel.startswith(".github/workflows/ci-") and workflow_rel != ".github/workflows/ci-validation.yml"
        ):
            verify_reusable_workflow(workflow_path, workflow_rel, errors)
        verify_build_orchestration_contract(workflow_path, workflow_rel, errors)
        verify_self_hosted_hardware_contract(workflow_path, workflow_rel, errors)
        verify_web_pipeline_contract(workflow_path, workflow_rel, errors)
        verify_manifest_status_arguments(workflow_path, workflow_rel, errors)
        verify_release_assets(workflow_path, workflow_rel, errors)
        verify_unity_vulkan_full_depth_display_contract(workflow_path, workflow_rel, errors)
        verify_unity_same_commit_native_plugin_contract(workflow_path, workflow_rel, errors)
        verify_godot_macos_clean_source_build_contract(workflow_path, workflow_rel, errors)
        verify_full_depth_validation_report_contract(workflow_path, workflow_rel, errors)
        verify_ci_core_self_test_contract(workflow_path, workflow_rel, errors)
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

            if job_name in REQUIRED_JOB_TIMEOUTS.get(workflow_rel, set()) and not has_positive_timeout(job.get("timeout-minutes")):
                errors.append(f"{workflow_rel} job {job_name} must set a positive timeout-minutes")

            required_timeout_steps = REQUIRED_STEP_TIMEOUTS.get(workflow_rel, {}).get(job_name, set())
            actual_steps = steps_by_name(job)
            for step_name in required_timeout_steps:
                step = actual_steps.get(step_name)
                if step is not None and not has_positive_timeout(step.get("timeout-minutes")):
                    errors.append(f"{workflow_rel} job {job_name} step {step_name!r} must set a positive timeout-minutes")

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

    verify_render_contract_references(root, errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    checked = sum(len(jobs) for jobs in REQUIRED_JOBS.values())
    print(f"Workflow matrix verified: {checked} jobs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
