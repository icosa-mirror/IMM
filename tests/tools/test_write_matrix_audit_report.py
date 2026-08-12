#!/usr/bin/env python3
"""Focused checks for matrix audit report generation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        output_dir = Path(temp_dir)
        json_output = output_dir / "matrix-audit.json"
        markdown_output = output_dir / "matrix-audit.md"
        completed = subprocess.run(
            [
                sys.executable,
                str(REPO_ROOT / "tests/tools/write_matrix_audit_report.py"),
                str(REPO_ROOT / "tests/matrix_status.json"),
                "--json-output",
                str(json_output),
                "--markdown-output",
                str(markdown_output),
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        assert completed.returncode == 0, completed.stderr
        audit = json.loads(json_output.read_text(encoding="utf-8"))
        assert audit["schema"] == "imm-testing-matrix-audit-v1"
        assert audit["summary"]["row_count"] == 27
        assert audit["summary"]["supported_count"] == 16
        assert audit["summary"]["release_blocker_count"] == 8
        assert audit["summary"]["audio_pipeline_count"] == 12
        assert audit["summary"]["actual_audio_pipeline_count"] == 10
        assert audit["summary"]["unsupported_audio_pipeline_count"] == 2
        assert "standalone/android/vr/openxr" in audit["summary"]["release_blockers"]
        assert "unity/windows/synthetic-stereo/vulkan" in audit["summary"]["release_blockers"]

        markdown = markdown_output.read_text(encoding="utf-8")
        assert "# IMM Testing Matrix Audit" in markdown
        assert "## Release Blockers" in markdown
        assert "standalone/android/vr/openxr" in markdown
        assert "## Matrix Rows" in markdown
        assert "## Audio Pipelines" in markdown
        assert "AVFoundation" in markdown

    print("Matrix audit report tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
