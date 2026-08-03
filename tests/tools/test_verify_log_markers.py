#!/usr/bin/env python3
"""Focused checks for log marker contract validation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def run_verify(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(REPO_ROOT / "tests/tools/verify_log_markers.py"), *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        log = root / "logcat.txt"
        log.write_text(
            "\n".join(
                [
                    "IMM_ANDROID_OPENXR_PROBE begin",
                    "IMM_ANDROID_OPENXR_PROBE enumerateStereoViewsFillResult=0 resultName=XR_SUCCESS",
                    "IMM_ANDROID_OPENXR_PROBE end",
                ]
            ),
            encoding="utf-8",
        )
        output = root / "contract.json"

        ok = run_verify(
            "--log",
            str(log),
            "--require",
            "IMM_ANDROID_OPENXR_PROBE begin",
            "--require",
            "IMM_ANDROID_OPENXR_PROBE enumerateStereoViewsFillResult=0 resultName=XR_SUCCESS",
            "--forbid",
            "Fatal signal",
            "--output",
            str(output),
        )
        assert ok.returncode == 0, ok.stderr
        result = json.loads(output.read_text(encoding="utf-8"))
        assert result["schema"] == "imm-log-marker-contract-v1"
        assert result["passed"] is True
        assert result["required"][1]["count"] == 1

        optional_output = root / "optional.json"
        optional = run_verify(
            "--log",
            str(log),
            "--require",
            "IMM_ANDROID_OPENXR_PROBE begin",
            "--optional",
            "Loaded in CPU",
            "--optional",
            "Loaded in GPU",
            "--output",
            str(optional_output),
        )
        assert optional.returncode == 0, optional.stderr
        optional_result = json.loads(optional_output.read_text(encoding="utf-8"))
        assert optional_result["passed"] is True
        assert optional_result["optional"] == [
            {"marker": "Loaded in CPU", "count": 0, "present": False},
            {"marker": "Loaded in GPU", "count": 0, "present": False},
        ]

        failed = run_verify(
            "--log",
            str(log),
            "--require",
            "IMM_ANDROID_OPENXR_PROBE createSessionResult=0 resultName=XR_SUCCESS",
            "--output",
            str(root / "failed.json"),
        )
        assert failed.returncode != 0, "missing required marker should fail"
        assert "Required marker not found" in failed.stdout

    print("Log marker contract tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
