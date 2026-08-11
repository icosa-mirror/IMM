#!/usr/bin/env python3
"""Focused tests for Android standalone evidence classification."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import classify_android_standalone as classifier


METRICS_NAME = "android-vulkan-screenshot-metrics.json"


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def passing_fixture(root: Path) -> None:
    write_json(
        root / "firebase-test-lab-result.json",
        {"gcloud_exit_code": 0, "copy_results": {"exit_code": 0}, "passed": True},
    )
    capture = root / "ftl-results" / "device" / "native-render-after.ppm"
    capture.parent.mkdir(parents=True)
    capture.write_bytes(b"P6\n1 1\n255\n\x00\x00\x00")
    write_json(root / METRICS_NAME, {"passed": True})


def assert_result(root: Path, status: str, failure_class: str) -> dict:
    value = classifier.classify(root, METRICS_NAME)
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
                "errors": ["Missing required Firebase Test Lab log marker: Loaded in GPU"],
            },
        )
        classified = assert_result(diagnostic_failure, "passed", "")
        assert classified["warnings"], classified

        rendering = base / "rendering"
        passing_fixture(rendering)
        write_json(rendering / METRICS_NAME, {"passed": False, "errors": ["scene mismatch"]})
        assert_result(rendering, "render_failed", "rendering")

        evidence = base / "evidence"
        passing_fixture(evidence)
        (evidence / "ftl-results" / "device" / "native-render-after.ppm").unlink()
        assert_result(evidence, "evidence_incomplete", "evidence")

        infrastructure = base / "infrastructure"
        passing_fixture(infrastructure)
        write_json(
            infrastructure / "firebase-test-lab-result.json",
            {
                "gcloud_exit_code": 1,
                "copy_results": {"exit_code": 1},
                "errors": ["Internal System Error 3"],
            },
        )
        (infrastructure / "ftl-results" / "device" / "native-render-after.ppm").unlink()
        assert_result(infrastructure, "infrastructure_failed", "infrastructure")

        crash = base / "crash"
        passing_fixture(crash)
        logcat = crash / "ftl-results" / "device" / "logcat"
        logcat.write_text("Fatal signal 11", encoding="utf-8")
        assert_result(crash, "runtime_failed", "runtime")

    print("Android standalone classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
