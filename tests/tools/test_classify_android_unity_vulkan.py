#!/usr/bin/env python3
"""Focused tests for Android Unity Vulkan evidence classification."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import classify_android_unity_vulkan as classifier


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
    for name in classifier.RENDER_EVIDENCE:
        key = "status" if name.endswith("log-contract.json") else "passed"
        value = "passed" if key == "status" else True
        write_json(root / name, {key: value})
    write_json(root / classifier.COMPOSITION_EVIDENCE, {"passed": True})


def assert_result(root: Path, result: str, failure_class: str) -> None:
    value = classifier.classify(root)
    assert value["result"] == result, value
    assert value["failure_class"] == failure_class, value


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        base = Path(temp_dir)

        passed = base / "passed"
        passing_fixture(passed)
        assert_result(passed, "passed", "")

        composition = base / "composition"
        passing_fixture(composition)
        write_json(
            composition / classifier.COMPOSITION_EVIDENCE,
            {"passed": False, "errors": ["cyan leakage"]},
        )
        assert_result(composition, "composition_failed", "compositing")

        stereo = base / "stereo"
        passing_fixture(stereo)
        write_json(
            stereo / "unity-android-vulkan-synthetic-right-metrics.json",
            {"passed": False, "errors": ["right eye lacks IMM content"]},
        )
        assert_result(stereo, "render_failed", "rendering")

        missing = base / "missing"
        passing_fixture(missing)
        (missing / "ftl-results" / "device" / "artifacts" / "unity-android-vulkan-synthetic-stereo.png").unlink()
        assert_result(missing, "evidence_incomplete", "evidence")

        infrastructure = base / "infrastructure"
        passing_fixture(infrastructure)
        write_json(
            infrastructure / "firebase-test-lab-result.json",
            {"gcloud_exit_code": 1, "copy_results": {"exit_code": 0}, "errors": ["quota"]},
        )
        classified = classifier.classify(infrastructure)
        assert classified["result"] == "passed", classified
        assert classified["warnings"], classified

        incomplete_infrastructure = base / "incomplete_infrastructure"
        passing_fixture(incomplete_infrastructure)
        write_json(
            incomplete_infrastructure / "firebase-test-lab-result.json",
            {"gcloud_exit_code": 1, "copy_results": {"exit_code": 1}, "errors": ["quota"]},
        )
        (
            incomplete_infrastructure
            / "ftl-results"
            / "device"
            / "artifacts"
            / "unity-android-vulkan-synthetic-stereo.png"
        ).unlink()
        assert_result(incomplete_infrastructure, "infrastructure_failed", "infrastructure")

        infrastructure_with_black_eye = base / "infrastructure_with_black_eye"
        passing_fixture(infrastructure_with_black_eye)
        write_json(
            infrastructure_with_black_eye / "firebase-test-lab-result.json",
            {"gcloud_exit_code": 1, "copy_results": {"exit_code": 0}, "errors": ["reporting API disabled"]},
        )
        write_json(
            infrastructure_with_black_eye / "unity-android-vulkan-synthetic-left-metrics.json",
            {"passed": False, "errors": ["black eye"]},
        )
        assert_result(infrastructure_with_black_eye, "render_failed", "rendering")

        infrastructure_with_composition = base / "infrastructure_with_composition"
        passing_fixture(infrastructure_with_composition)
        write_json(
            infrastructure_with_composition / "firebase-test-lab-result.json",
            {"gcloud_exit_code": 1, "copy_results": {"exit_code": 0}, "errors": ["reporting API disabled"]},
        )
        write_json(
            infrastructure_with_composition / classifier.COMPOSITION_EVIDENCE,
            {"passed": False, "errors": ["cyan leakage"]},
        )
        assert_result(infrastructure_with_composition, "composition_failed", "compositing")

        crash = base / "crash"
        passing_fixture(crash)
        crash_file = crash / "ftl-results" / "device" / "data_app_native_crash_0_com_ImmersiveFoundation_IMMUnityTest.txt"
        crash_file.write_text("backtrace libswappywrapper", encoding="utf-8")
        assert_result(crash, "runtime_failed", "runtime")

        fallback = base / "fallback"
        passing_fixture(fallback)
        logcat = fallback / "ftl-results" / "device" / "logcat"
        logcat.write_text(
            "[IMM_UNITY_SMOKE] graphics api probe failed: expected=Vulkan actual=OpenGLES3",
            encoding="utf-8",
        )
        assert_result(fallback, "runtime_failed", "runtime")

    print("Android Unity Vulkan classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
