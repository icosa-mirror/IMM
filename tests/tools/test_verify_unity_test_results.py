#!/usr/bin/env python3
"""Focused checks for Unity package-test discovery and result validation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from verify_engine_package_import import UNITY_PACKAGE_NAMES, write_unity_harness


TEST_CLASS = "ImmPlayer.Tests.ImmCameraMatrixFrameGateTests"


def run_verifier(path: Path, required: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            "tests/tools/verify_unity_test_results.py",
            str(path),
            "--require-test",
            required,
        ],
        check=False,
        capture_output=True,
        text=True,
    )


def main() -> int:
    workflow = Path(".github/workflows/ci-engine.yml").read_text(encoding="utf-8")
    assert "testMode: all" in workflow
    assert "artifacts/unity-package-import/test-runner/editmode-results.xml" in workflow
    assert "artifacts/unity-package-import/test-runner/playmode-results.xml" in workflow
    assert "customParameters: -testResults artifacts/unity-editmode-results.xml" not in workflow

    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        stroke_package = temp / "stroke"
        player_package = temp / "player"
        stroke_package.mkdir()
        player_package.mkdir()
        baseline = temp / "baseline.imm"
        baseline.write_bytes(b"fixture")
        project = write_unity_harness(temp / "harness", stroke_package, player_package, baseline)
        manifest = json.loads((project / "Packages" / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["testables"] == [UNITY_PACKAGE_NAMES["player"]]
        consumer = project / "Assets" / "Phase6PackageConsumer"
        consumer_manifest = json.loads((consumer / "ImmPhase6PackageConsumer.asmdef").read_text(encoding="utf-8"))
        assert consumer_manifest["name"] == "ImmPhase6PackageConsumer"
        assert consumer_manifest["references"] == ["ImmUnity.Runtime"]
        consumer_source = (consumer / "ImmPackageConsumerSmoke.cs").read_text(encoding="utf-8")
        assert "ImmAuthoringRuntime.Capabilities" in consumer_source
        assert "ImmAuthoringDocument.Create" in consumer_source

        passing = temp / "passing.xml"
        passing.write_text(
            """<?xml version="1.0" encoding="utf-8"?>
<test-run result="Passed" total="2" passed="2" failed="0">
  <test-suite result="Passed">
    <test-case name="First" fullname="ImmPlayer.Tests.ImmCameraMatrixFrameGateTests.First" result="Passed" />
    <test-case name="Second" fullname="ImmPlayer.Tests.ImmCameraMatrixFrameGateTests.Second" result="Passed" />
  </test-suite>
</test-run>
""",
            encoding="utf-8",
        )
        result = run_verifier(passing, f"{TEST_CLASS}.First")
        assert result.returncode == 0, result.stderr

        missing = run_verifier(passing, f"{TEST_CLASS}.Missing")
        assert missing.returncode == 1
        assert "Required Unity test did not run" in missing.stderr

        failing = temp / "failing.xml"
        failing.write_text(
            """<test-run result="Failed" total="1" passed="0" failed="1">
  <test-case name="First" fullname="ImmPlayer.Tests.ImmCameraMatrixFrameGateTests.First" result="Failed" />
</test-run>
""",
            encoding="utf-8",
        )
        failed = run_verifier(failing, f"{TEST_CLASS}.First")
        assert failed.returncode == 1
        assert "reported 1 failed test" in failed.stderr

        no_results = run_verifier(temp / "missing.xml", f"{TEST_CLASS}.First")
        assert no_results.returncode == 1
        assert "result XML is missing" in no_results.stderr

    print("Unity test result verification self-tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
