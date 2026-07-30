#!/usr/bin/env python3
"""Focused checks for matrix evidence report validation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def manifest(product: str, platform: str, mode: str, renderer: str) -> dict:
    return {
        "schema": "imm-ci-artifact-manifest-v1",
        "classification": {"result": "passed", "failure_class": ""},
        "matrix": {
            "product": product,
            "platform": platform,
            "mode": mode,
            "renderer": renderer,
        },
        "git": {},
        "runner": {},
        "tool_versions": {},
        "fixtures": [],
        "files": [],
    }


def artifact(
    product: str,
    platform: str,
    mode: str,
    renderer: str,
    *,
    with_capture: bool = False,
    with_metrics: bool = False,
    with_report: bool = False,
    with_contract: bool = False,
    with_preflight: bool = True,
    with_firebase_result: bool = False,
) -> dict:
    artifact_manifest = manifest(product, platform, mode, renderer)
    if with_firebase_result:
        artifact_manifest["files"] = [{"path": "firebase-test-lab-result.json"}]
    return {
        "path": f"{product}-{platform}-{mode}-{renderer}",
        "file_count": 4,
        "total_bytes": 128,
        "manifests": [{"file": "manifest.json", "content": artifact_manifest}],
        "preflights": [{"file": "preflight.json", "content": {"passed": True, "errors": []}}] if with_preflight else [],
        "metrics": [
            {
                "path": "render-metrics.json",
                "byte_size": 12,
                "sha256": "abc",
                "content": {
                    "passed": True,
                    "candidate": {"path": "capture.png", "sha256": "candidate"},
                    "reference": {"path": "reference.ppm", "sha256": "reference"},
                },
            }
        ] if with_metrics else [],
        "reports": [{"path": "render-report.md", "byte_size": 12, "sha256": "ghi"}] if with_report else [],
        "contracts": [{"file": "openxr-log-contract.json", "content": {"passed": True, "errors": []}}] if with_contract else [],
        "captures": [{"path": "capture.png", "byte_size": 8, "sha256": "def"}] if with_capture else [],
    }


def write_matrix(path: Path) -> None:
    path.write_text(
        json.dumps(
            {
                "schema": "imm-testing-matrix-status-v1",
                "updated": "2026-06-10",
                "rows": [
                    {
                        "product": "standalone",
                        "platform": "windows",
                        "mode": "non-vr",
                        "renderer": "vulkan",
                        "status": "supported",
                        "hosted_gate": "Build / Windows with IMM_CI_ENABLE_GPU_SMOKE=1",
                        "hardware_gate": "CI GPU Matrix / Windows Standalone Vulkan",
                        "baseline": "tests/baselines/render/windows-directx-sample1.json",
                        "reason": "test row",
                    },
                    {
                        "product": "standalone",
                        "platform": "android",
                        "mode": "vr",
                        "renderer": "openxr",
                        "status": "supported",
                        "hosted_gate": "Build / Android",
                        "hardware_gate": "CI Device Matrix / Android Quest VR",
                        "baseline": "tests/baselines/content/sample1.json",
                        "reason": "test row",
                    },
                    {
                        "product": "godot",
                        "platform": "android",
                        "mode": "non-vr",
                        "renderer": "vulkan",
                        "status": "supported",
                        "hosted_gate": "Build / Android",
                        "hardware_gate": "CI Device Matrix / Android Godot Vulkan",
                        "baseline": "tests/baselines/content/sample1.json",
                        "reason": "test row",
                    },
                    {
                        "product": "standalone",
                        "platform": "ios",
                        "mode": "non-vr",
                        "renderer": "native",
                        "status": "unsupported",
                        "reason": "ignored unsupported row",
                        "owner_decision": "not supported",
                    },
                ],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def write_summary(path: Path, artifacts: list[dict]) -> None:
    path.write_text(
        json.dumps(
            {
                "schema": "imm-ci-artifact-summary-v1",
                "git": {},
                "artifact_count": len(artifacts),
                "artifacts": artifacts,
                "passed": True,
                "errors": [],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def run_verify(matrix: Path, summary: Path, output_dir: Path, *extra_args: str) -> subprocess.CompletedProcess[str]:
    command = [
        sys.executable,
        str(REPO_ROOT / "tests/tools/verify_matrix_evidence.py"),
        str(matrix),
        "--summary",
        str(summary),
        "--json-output",
        str(output_dir / "matrix-evidence.json"),
        "--markdown-output",
        str(output_dir / "matrix-evidence.md"),
        *extra_args,
    ]
    return subprocess.run(
        command,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        matrix_path = temp / "matrix_status.json"
        write_matrix(matrix_path)

        passing_summary = temp / "passing-summary.json"
        write_summary(
            passing_summary,
            [
                artifact("standalone", "windows", "non-vr", "vulkan", with_capture=True, with_metrics=True, with_report=True),
                artifact("standalone", "android", "vr", "openxr", with_contract=True),
                artifact("godot", "android", "non-vr", "vulkan", with_capture=True, with_metrics=True, with_report=True, with_preflight=False, with_firebase_result=True),
            ],
        )
        passed = run_verify(matrix_path, passing_summary, temp / "passing-output")
        assert passed.returncode == 0, passed.stderr + passed.stdout
        passed_report = json.loads((temp / "passing-output/matrix-evidence.json").read_text(encoding="utf-8"))
        assert passed_report["passed"] is True
        assert passed_report["required_row_count"] == 3

        expected_failed_artifact = artifact(
            "standalone",
            "windows",
            "non-vr",
            "vulkan",
            with_capture=True,
            with_metrics=True,
            with_report=True,
        )
        expected_failed_artifact["manifests"][0]["content"]["classification"]["result"] = "expected_failed"
        expected_failed_summary = temp / "expected-failed-summary.json"
        write_summary(expected_failed_summary, [expected_failed_artifact])
        expected_failed = run_verify(
            matrix_path,
            expected_failed_summary,
            temp / "expected-failed-output",
            "--scope",
            "hardware",
            "--gate-prefix",
            "CI GPU Matrix /",
        )
        assert expected_failed.returncode == 1, "Expected-failed evidence must not satisfy a supported matrix row"
        assert "# IMM Matrix Evidence Report" in (temp / "passing-output/matrix-evidence.md").read_text(encoding="utf-8")

        log_only_visual = artifact(
            "standalone",
            "windows",
            "non-vr",
            "vulkan",
            with_capture=True,
            with_metrics=True,
            with_report=True,
        )
        log_only_visual["metrics"][0]["content"] = {"passed": True, "candidate": {}}
        log_only_summary = temp / "log-only-summary.json"
        write_summary(log_only_summary, [log_only_visual])
        log_only = run_verify(
            matrix_path,
            log_only_summary,
            temp / "log-only-output",
            "--scope",
            "hardware",
            "--gate-prefix",
            "CI GPU Matrix /",
        )
        assert log_only.returncode == 1, "metrics without a reference comparison must not satisfy a visual row"
        assert "missing passing candidate-to-reference visual metrics evidence" in log_only.stdout

        failing_summary = temp / "failing-summary.json"
        write_summary(
            failing_summary,
            [
                artifact("standalone", "windows", "non-vr", "vulkan", with_capture=False, with_metrics=True),
                artifact("standalone", "android", "vr", "openxr", with_contract=False),
                artifact("godot", "android", "non-vr", "vulkan", with_capture=True, with_metrics=True, with_report=True, with_preflight=False),
            ],
        )
        failed = run_verify(matrix_path, failing_summary, temp / "failing-output")
        assert failed.returncode != 0, "missing visual/VR evidence should fail"
        assert "missing capture image/frame evidence" in failed.stdout
        assert "missing human-readable render report evidence" in failed.stdout
        assert "missing passing VR/OpenXR contract evidence" in failed.stdout
        assert "godot/android/non-vr/vulkan: missing passing runner preflight evidence" in failed.stdout

        gpu_only = run_verify(
            matrix_path,
            failing_summary,
            temp / "gpu-only-output",
            "--gate-prefix",
            "CI GPU Matrix /",
        )
        assert gpu_only.returncode != 0, "GPU row should still enforce its visual evidence"
        gpu_report = json.loads((temp / "gpu-only-output/matrix-evidence.json").read_text(encoding="utf-8"))
        assert gpu_report["required_row_count"] == 1
        assert gpu_report["rows"][0]["key"] == "standalone/windows/non-vr/vulkan"

    print("Matrix evidence verification tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
