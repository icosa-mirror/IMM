#!/usr/bin/env python3
"""Focused checks for CI manifest audit fields."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    root = Path.cwd()
    with tempfile.TemporaryDirectory() as temp_dir:
        output = Path(temp_dir) / "manifest.json"
        fixture = Path(temp_dir) / "fixture.imm"
        fixture.write_bytes(b"fixture")
        included = Path(temp_dir) / "artifact.txt"
        included.write_text("artifact", encoding="utf-8")

        subprocess.run(
            [
                sys.executable,
                "tests/tools/write_ci_manifest.py",
                "--output",
                str(output),
                "--repo-root",
                str(root),
                "--product",
                "standalone",
                "--platform-name",
                "windows",
                "--mode",
                "non-vr",
                "--renderer",
                "directx",
                "--status",
                "success",
                "--failure-class",
                "visual",
                "--fixture",
                str(fixture),
                "--include",
                str(included),
            ],
            check=True,
        )
        manifest = json.loads(output.read_text(encoding="utf-8"))
        assert manifest["schema"] == "imm-ci-artifact-manifest-v1"
        assert manifest["classification"]["result"] == "passed"
        assert manifest["classification"]["failure_class"] == ""
        assert manifest["matrix"]["product"] == "standalone"
        assert manifest["matrix"]["renderer"] == "directx"
        assert manifest["tool_versions"]["python"]["version"]["exit_code"] == 0
        assert manifest["fixtures"][0]["sha256"]
        assert manifest["files"][0]["sha256"]

        failed_output = Path(temp_dir) / "failed-manifest.json"
        subprocess.run(
            [
                sys.executable,
                "tests/tools/write_ci_manifest.py",
                "--output",
                str(failed_output),
                "--repo-root",
                str(root),
                "--product",
                "standalone",
                "--platform-name",
                "windows",
                "--mode",
                "non-vr",
                "--renderer",
                "directx",
                "--status",
                "failure",
                "--failure-class",
                "content-parse",
            ],
            check=True,
        )
        failed_manifest = json.loads(failed_output.read_text(encoding="utf-8"))
        assert failed_manifest["classification"]["result"] == "failed"
        assert failed_manifest["classification"]["failure_class"] == "content-parse"

        expected_output = Path(temp_dir) / "expected-manifest.json"
        subprocess.run(
            [
                sys.executable,
                "tests/tools/write_ci_manifest.py",
                "--output",
                str(expected_output),
                "--repo-root",
                str(root),
                "--product",
                "godot",
                "--platform-name",
                "windows",
                "--mode",
                "non-vr",
                "--renderer",
                "vulkan",
                "--status",
                "expected_failed",
                "--failure-class",
                "compositing",
            ],
            check=True,
        )
        expected_manifest = json.loads(expected_output.read_text(encoding="utf-8"))
        assert expected_manifest["classification"]["result"] == "expected_failed"
        assert expected_manifest["classification"]["failure_class"] == "compositing"

        classified_status = Path(temp_dir) / "classified-status.json"
        classified_status.write_text(
            json.dumps(
                {
                    "result": "runtime_failed",
                    "failure_class": "runtime",
                    "failures": ["requested Vulkan fell back to Direct3D"],
                    "warnings": ["supporting log marker absent"],
                    "rendering": "success",
                    "compositing": "success",
                    "depth_composition": "success",
                    "ordered_overlay": "success",
                }
            ),
            encoding="utf-8",
        )
        classified_output = Path(temp_dir) / "classified-manifest.json"
        subprocess.run(
            [
                sys.executable,
                "tests/tools/write_ci_manifest.py",
                "--output",
                str(classified_output),
                "--repo-root",
                str(root),
                "--product",
                "unity",
                "--platform-name",
                "android",
                "--mode",
                "non-vr",
                "--renderer",
                "vulkan",
                "--status",
                "failure",
                "--classification-json",
                str(classified_status),
            ],
            check=True,
        )
        classified_manifest = json.loads(classified_output.read_text(encoding="utf-8"))
        assert classified_manifest["classification"] == {
            "result": "failed",
            "failure_class": "runtime",
            "failures": ["requested Vulkan fell back to Direct3D"],
            "warnings": ["supporting log marker absent"],
            "rendering": "success",
            "compositing": "success",
            "depth_composition": "success",
            "ordered_overlay": "success",
        }

        skipped_status = Path(temp_dir) / "skipped-status.json"
        skipped_status.write_text(
            json.dumps(
                {
                    "result": "skipped",
                    "failure_class": "",
                    "failures": [],
                    "warnings": ["host Vulkan unavailable"],
                }
            ),
            encoding="utf-8",
        )
        skipped_output = Path(temp_dir) / "skipped-manifest.json"
        subprocess.run(
            [
                sys.executable,
                "tests/tools/write_ci_manifest.py",
                "--output",
                str(skipped_output),
                "--repo-root",
                str(root),
                "--product",
                "unity",
                "--platform-name",
                "windows",
                "--mode",
                "synthetic-stereo",
                "--renderer",
                "vulkan",
                "--status",
                "success",
                "--classification-json",
                str(skipped_status),
            ],
            check=True,
        )
        skipped_manifest = json.loads(skipped_output.read_text(encoding="utf-8"))
        assert skipped_manifest["classification"] == {
            "result": "skipped",
            "failure_class": "",
            "failures": [],
            "warnings": ["host Vulkan unavailable"],
        }

    print("CI manifest tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
