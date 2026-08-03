#!/usr/bin/env python3
"""Focused tests for Android Godot Vulkan evidence classification."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import classify_android_godot_vulkan as classifier


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def passing_fixture(root: Path) -> None:
    write_json(
        root / "firebase-test-lab-result.json",
        {"gcloud_exit_code": 0, "copy_results": {"exit_code": 0}, "passed": True},
    )
    capture_root = root / "ftl-results" / "device" / "artifacts"
    capture_root.mkdir(parents=True)
    for name in classifier.REQUIRED_CAPTURES:
        (capture_root / name).write_bytes(b"png")
    write_json(root / classifier.RENDER_EVIDENCE, {"passed": True})
    write_json(root / classifier.COMPOSITION_EVIDENCE, {"passed": True})


def assert_result(root: Path, status: str, failure_class: str) -> dict:
    value = classifier.classify(root)
    assert value["result"] == status, value
    assert value["failure_class"] == failure_class, value
    return value


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        base = Path(temp_dir)

        passed = base / "passed"
        passing_fixture(passed)
        assert_result(passed, "passed", "")

        diagnostic_failure = base / "diagnostic_failure"
        passing_fixture(diagnostic_failure)
        write_json(
            diagnostic_failure / "firebase-test-lab-result.json",
            {
                "gcloud_exit_code": 1,
                "copy_results": {"exit_code": 0},
                "passed": False,
                "errors": [
                    "Firebase instrumentation result failed (code -1)",
                    "Missing required Firebase Test Lab log marker: smoke passed",
                ],
            },
        )
        classified = assert_result(diagnostic_failure, "passed", "")
        assert classified["warnings"], classified

        render = base / "render"
        passing_fixture(render)
        write_json(
            render / classifier.RENDER_EVIDENCE,
            {"passed": False, "errors": ["missing IMM content"]},
        )
        assert_result(render, "render_failed", "rendering")

        composition = base / "composition"
        passing_fixture(composition)
        write_json(
            composition / classifier.COMPOSITION_EVIDENCE,
            {"passed": False, "errors": ["cyan leakage"]},
        )
        assert_result(composition, "composition_failed", "compositing")

        incomplete = base / "incomplete"
        passing_fixture(incomplete)
        (incomplete / "ftl-results" / "device" / "artifacts" / "vulkan_visual_smoke.png").unlink()
        assert_result(incomplete, "evidence_incomplete", "evidence")

        infrastructure = base / "infrastructure"
        passing_fixture(infrastructure)
        write_json(
            infrastructure / "firebase-test-lab-result.json",
            {"gcloud_exit_code": 1, "copy_results": {"exit_code": 1}, "errors": ["quota"]},
        )
        (infrastructure / "ftl-results" / "device" / "artifacts" / "vulkan_visual_smoke.png").unlink()
        assert_result(infrastructure, "infrastructure_failed", "infrastructure")

        crash = base / "crash"
        passing_fixture(crash)
        logcat = crash / "ftl-results" / "device" / "logcat"
        logcat.write_text("Fatal signal 11", encoding="utf-8")
        assert_result(crash, "runtime_failed", "runtime")

    print("Android Godot Vulkan classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
