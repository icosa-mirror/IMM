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

        foreign_fatal = (
            "10-23 21:27:54.414 32454 9625 E AndroidRuntime: FATAL EXCEPTION: pool-3-thread-4\n"
            "10-23 21:27:54.414 32454 9625 E AndroidRuntime: "
            "Process: com.google.android.permissioncontroller, PID: 32454\n"
            "10-23 21:27:54.414 32454 9625 E AndroidRuntime: java.lang.NullPointerException\n"
        )
        app_start = f"10-23 21:28:15.031 1402 1640 I ActivityManager: Start proc 15901:{classifier.APP_PACKAGE}/u0a282\n"
        cases = (
            ("foreign_before_app", foreign_fatal + app_start, "passed"),
            ("app_java_crash", app_start + foreign_fatal.replace(
                "com.google.android.permissioncontroller", classifier.APP_PACKAGE), "runtime_failed"),
            ("known_app_pid", app_start + foreign_fatal.replace("32454", "15901"), "runtime_failed"),
            ("unobserved_generic_process", app_start + foreign_fatal.replace(
                "com.google.android.permissioncontroller", "Godot"), "runtime_failed"),
            ("missing_app_identity", foreign_fatal, "runtime_failed"),
            ("unmatched_crash_pid", app_start + foreign_fatal.replace(
                "PID: 32454", "PID: 99999"), "runtime_failed"),
            ("different_event", app_start + foreign_fatal.replace(
                "54.414 32454 9625 E AndroidRuntime: Process:",
                "54.415 32454 9625 E AndroidRuntime: Process:"), "runtime_failed"),
            ("unattributed_fatal", app_start + "FATAL EXCEPTION", "runtime_failed"),
        )
        for name, log, expected in cases:
            case = base / name
            passing_fixture(case)
            (case / "ftl-results" / "device" / "logcat").write_text(log, encoding="utf-8")
            assert_result(case, expected, "" if expected == "passed" else "runtime")
        classified = classifier.classify(base / "foreign_before_app")
        assert any("permissioncontroller" in warning for warning in classified["warnings"]), classified

        foreign_with_bad_render = base / "foreign_with_bad_render"
        passing_fixture(foreign_with_bad_render)
        (foreign_with_bad_render / "ftl-results" / "device" / "logcat").write_text(
            foreign_fatal + app_start, encoding="utf-8"
        )
        write_json(foreign_with_bad_render / classifier.RENDER_EVIDENCE,
            {"passed": False, "errors": ["missing IMM content"]})
        assert_result(foreign_with_bad_render, "render_failed", "rendering")

        separate_logs = base / "separate_logs"
        passing_fixture(separate_logs)
        (separate_logs / "ftl-results" / "device" / "logcat").write_text(foreign_fatal, encoding="utf-8")
        other_device = separate_logs / "ftl-results" / "other-device"
        other_device.mkdir()
        (other_device / "logcat").write_text(app_start, encoding="utf-8")
        assert_result(separate_logs, "runtime_failed", "runtime")

    print("Android Godot Vulkan classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
