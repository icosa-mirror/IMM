#!/usr/bin/env python3
"""Focused checks for baseline drift report generation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def run_report(expected: Path, actual: Path, output_dir: Path) -> tuple[dict, str]:
    json_output = output_dir / "baseline-drift.json"
    markdown_output = output_dir / "baseline-drift.md"
    completed = subprocess.run(
        [
            sys.executable,
            str(REPO_ROOT / "tests/tools/write_baseline_drift_report.py"),
            "--expected",
            str(expected),
            "--actual",
            str(actual),
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
    return json.loads(json_output.read_text(encoding="utf-8")), markdown_output.read_text(encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        expected = root / "expected.json"
        actual = root / "actual.json"
        data = {
            "schema": "test",
            "fixture": {"path": "sample.imm", "byte_size": 4},
            "expected_content": {"requires_layer_count_greater_than": 0},
        }
        expected.write_text(json.dumps(data), encoding="utf-8")
        actual.write_text(json.dumps(data), encoding="utf-8")
        report, markdown = run_report(expected, actual, root)
        assert report["schema"] == "imm-baseline-drift-report-v1"
        assert report["summary"]["has_drift"] is False
        assert "Result: no drift" in markdown

        changed = json.loads(json.dumps(data))
        changed["fixture"]["byte_size"] = 5
        changed["fixture"]["sha256"] = "abc"
        actual.write_text(json.dumps(changed), encoding="utf-8")
        report, markdown = run_report(expected, actual, root)
        assert report["summary"]["has_drift"] is True
        assert report["summary"]["changed_count"] == 1
        assert report["summary"]["added_count"] == 1
        assert "fixture.byte_size" in markdown
        assert "fixture.sha256" in markdown

    print("Baseline drift report tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
