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
        robo_artifacts = root / "ftl-results/device/artifacts"
        robo_artifacts.mkdir(parents=True)
        (robo_artifacts / "0.png").write_bytes(b"device-details")
        (robo_artifacts / "1.png").write_bytes(b"first-app-frame")
        (robo_artifacts / "6.png").write_bytes(b"last-app-frame")
        nested_app_capture = robo_artifacts / "sdcard/app/files/internal.png"
        nested_app_capture.parent.mkdir(parents=True)
        nested_app_capture.write_bytes(b"newer-but-not-the-screen")
        external_capture = root / "unity-android-vulkan-external.png"
        assert tool.copy_latest_robo_screen_capture(root / "ftl-results", external_capture) == external_capture
        assert external_capture.read_bytes() == b"last-app-frame"
        logcat = root / "ftl-results/device/logcat"
        logcat.write_text(
            "06-12 IMMAVAL renderFrame frame=60 drawCalls=12\n"
            "Missing required log marker: NEGATED_SUCCESS\n",
            encoding="utf-8",
        )
        result_xml = root / "ftl-results/device/test_result_1.xml"
        result_xml.write_text(
            '<testsuites><testsuite tests="1" failures="1" errors="0">'
            '<testcase name="visualSmoke"><failure>Missing required log marker: XML_ONLY_SUCCESS</failure></testcase>'
            "</testsuite></testsuites>\n",
            encoding="utf-8",
        )
        instrumentation_result = root / "ftl-results/device/instrumentation.results"
        instrumentation_result.write_text(
            "FAILURES!!!\nINSTRUMENTATION_CODE: -1\n",
            encoding="utf-8",
        )
        marker_matches, searched_logs = tool.find_text_with_markers(
            root / "ftl-results",
            ["IMMAVAL renderFrame", "NEGATED_SUCCESS", "XML_ONLY_SUCCESS"],
        )
        assert marker_matches["IMMAVAL renderFrame"] == ["device/logcat"]
        assert marker_matches["NEGATED_SUCCESS"] == []
        assert marker_matches["XML_ONLY_SUCCESS"] == []
        assert searched_logs == ["device/logcat"]
        result_failures = tool.find_failed_test_results(root / "ftl-results")
        assert any("1 failure(s) and 0 error(s)" in failure for failure in result_failures)
        assert any("instrumentation.results failed (code -1)" in failure for failure in result_failures)
        instrumentation_result.write_text("INSTRUMENTATION_CODE: -1\n", encoding="utf-8")
        result_xml.write_text(
            '<testsuites><testsuite tests="1" failures="0" errors="0"/></testsuites>\n',
            encoding="utf-8",
        )
        assert tool.find_failed_test_results(root / "ftl-results") == []
        instrumentation_result.write_text("INSTRUMENTATION_CODE: 0\n", encoding="utf-8")
        assert any(
            "instrumentation.results failed (code 0)" in failure
            for failure in tool.find_failed_test_results(root / "ftl-results")
        )
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
        args.external_screen_capture_name = ""
        args.robo_script = Path("unity-wait.roboscript")
        args.test = Path("ftl-test.apk")
        args.timeout = "7m"

        command = tool.build_firebase_command(args, "godot-results")
        assert "--additional-apks" in command
        additional_index = command.index("--additional-apks")
        assert command[additional_index + 1] == "godot-sample.apk"
        assert "--test" in command
        test_index = command.index("--test")
        assert command[test_index + 1] == "ftl-test.apk"
        assert "--robo-script" in command
        robo_script_index = command.index("--robo-script")
        assert command[robo_script_index + 1] == "unity-wait.roboscript"

    print("Firebase Test Lab wrapper tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
