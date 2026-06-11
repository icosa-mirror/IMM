#!/usr/bin/env python3
"""Focused checks for CI artifact summary manifest validation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def run_collect(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(REPO_ROOT / "tests/tools/collect_ci_artifacts.py"), *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def valid_manifest() -> dict:
    return {
        "schema": "imm-ci-artifact-manifest-v1",
        "classification": {"result": "passed", "failure_class": ""},
        "matrix": {
            "product": "standalone",
            "platform": "windows",
            "mode": "non-vr",
            "renderer": "directx",
        },
        "git": {},
        "runner": {},
        "tool_versions": {},
        "fixtures": [],
        "files": [],
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        artifact = root / "artifact"
        artifact.mkdir()
        (artifact / "manifest.json").write_text(json.dumps(valid_manifest()) + "\n", encoding="utf-8")
        (artifact / "openxr-log-contract.json").write_text(
            json.dumps({"schema": "imm-log-marker-contract-v1", "passed": True}) + "\n",
            encoding="utf-8",
        )
        (artifact / "capture.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        (artifact / "capture.ppm").write_bytes(b"P6\n1 1\n255\n\x00\x00\x00")
        (artifact / "render-report.md").write_text("# Render Validation Report\n", encoding="utf-8")

        ok = run_collect(
            "--repo-root",
            str(root),
            "--artifact-dir",
            str(artifact),
            "--require-manifest",
            "--output",
            str(artifact / "artifact-summary.json"),
        )
        assert ok.returncode == 0, ok.stderr
        summary = json.loads((artifact / "artifact-summary.json").read_text(encoding="utf-8"))
        assert summary["passed"] is True
        assert summary["artifacts"][0]["manifests"][0]["content"]["schema"] == "imm-ci-artifact-manifest-v1"
        assert summary["artifacts"][0]["contracts"][0]["content"]["schema"] == "imm-log-marker-contract-v1"
        assert len(summary["artifacts"][0]["captures"]) == 2
        assert summary["artifacts"][0]["reports"][0]["path"] == "artifact/render-report.md"
        report = (artifact / "validation-report.md").read_text(encoding="utf-8")
        assert "# IMM Validation Report" in report
        assert "Reports" in report
        assert "![artifact/capture.png]" in report
        assert "- [artifact/capture.ppm]" in report

        expected = root / "expected"
        expected.mkdir()
        expected_manifest = valid_manifest()
        expected_manifest["classification"] = {"result": "expected_failed", "failure_class": "compositing"}
        (expected / "manifest.json").write_text(json.dumps(expected_manifest) + "\n", encoding="utf-8")
        expected_result = run_collect(
            "--repo-root",
            str(root),
            "--artifact-dir",
            str(expected),
            "--require-manifest",
            "--output",
            str(expected / "artifact-summary.json"),
        )
        assert expected_result.returncode == 0, expected_result.stdout + expected_result.stderr
        expected_summary = json.loads((expected / "artifact-summary.json").read_text(encoding="utf-8"))
        assert expected_summary["passed"] is True

        broken = root / "broken"
        broken.mkdir()
        (broken / "manifest.json").write_text(json.dumps({"schema": "old"}) + "\n", encoding="utf-8")
        failed = run_collect(
            "--repo-root",
            str(root),
            "--artifact-dir",
            str(broken),
            "--require-manifest",
            "--output",
            str(broken / "artifact-summary.json"),
        )
        assert failed.returncode != 0, "broken manifest should fail --require-manifest validation"
        assert "Manifest has unexpected schema" in failed.stdout
        assert "Manifest missing classification" in failed.stdout

    print("CI artifact collector tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
