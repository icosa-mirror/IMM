#!/usr/bin/env python3
"""Focused checks for Firebase Test Lab result handling."""

from __future__ import annotations

import importlib.util
import json
import tempfile
from argparse import Namespace
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = REPO_ROOT / "tests/tools/run_firebase_test_lab_android.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("run_firebase_test_lab_android", TOOL_PATH)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    tool = load_tool()
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        stderr_path = root / "gcloud-firebase-test.stderr.txt"
        stderr_path.write_text(
            "ERROR: (gcloud.firebase.test.android.run) PERMISSION_DENIED\n"
            "service: toolresults.googleapis.com\n"
            "reason: SERVICE_DISABLED\n",
            encoding="utf-8",
        )
        assert tool.is_tool_results_api_disabled(stderr_path)

        stderr_path.write_text(
            "18:42:13 An infrastructure error occurred. Attempts exhausted.\n"
            "ERROR: (gcloud.firebase.test.android.run) Firebase Test Lab infrastructure failure: Exhausted test run attempts\n",
            encoding="utf-8",
        )
        assert tool.is_firebase_infrastructure_failure(stderr_path)

        args = Namespace(
            artifact_dir=root,
            project="imm-ci",
            device="model=Pixel2.arm,version=33,locale=en,orientation=landscape",
            results_bucket="bucket",
            results_dir="dir",
            test_type="instrumentation",
        )
        capture = root / "ftl-results/device/screencap_after.png"
        capture.parent.mkdir(parents=True)
        capture.write_bytes(b"png")
        logcat = root / "ftl-results/device/logcat"
        logcat.write_text("06-12 IMMAVAL renderFrame frame=60 drawCalls=12\n", encoding="utf-8")
        diagnostics = tool.collect_diagnostic_lines(root / "ftl-results")
        assert diagnostics == ["device/logcat: 06-12 IMMAVAL renderFrame frame=60 drawCalls=12"]
        summary_path = tool.write_summary(
            args,
            1,
            True,
            {"source": "gs://bucket/dir", "destination": "out", "exit_code": 0},
            [],
            {"Loaded in CPU": ["device/logcat"]},
            ["device/logcat"],
            [capture],
            diagnostics,
        )
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        assert summary["passed"] is True
        assert summary["gcloud_exit_code"] == 1
        assert summary["gcloud_exit_ignored"] is True
        assert summary["diagnostic_lines"] == diagnostics

        args.additional_apk = [Path("godot-sample.apk")]
        args.app = Path("ftl-target.apk")
        args.client_label = "IMM Android Godot Vulkan"
        args.directory_to_pull = []
        args.environment_variable = []
        args.required_capture_name = []
        args.required_marker = []
        args.test = Path("ftl-test.apk")
        args.timeout = "7m"

        command = tool.build_firebase_command(args, "godot-results")
        assert "--additional-apks" in command
        additional_index = command.index("--additional-apks")
        assert command[additional_index + 1] == "godot-sample.apk"
        assert "--test" in command
        test_index = command.index("--test")
        assert command[test_index + 1] == "ftl-test.apk"

    print("Firebase Test Lab wrapper tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
