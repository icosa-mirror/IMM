#!/usr/bin/env python3
"""Focused checks for matrix status release policy."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run_verify(path: Path, *extra_args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, "tests/tools/verify_matrix_status.py", str(path), *extra_args],
        check=False,
        capture_output=True,
        text=True,
    )


def main() -> int:
    source = Path("tests/matrix_status.json")
    current = run_verify(source)
    assert current.returncode == 0, current.stderr

    release = run_verify(source, "--release")
    assert release.returncode != 0, "release policy should block current deferred rows"
    assert "Release policy blocks deferred row" in release.stderr

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir) / "matrix_status.json"
        data = json.loads(source.read_text(encoding="utf-8"))
        for row in data["rows"]:
            if row["status"] == "deferred":
                row["status"] = "waived"
                row["owner_decision"] = row.get("owner_decision") or "Temporary owner waiver for release policy test."
        temp_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
        waived = run_verify(temp_path, "--release")
        assert waived.returncode == 0, waived.stderr

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir) / "matrix_status.json"
        data = json.loads(source.read_text(encoding="utf-8"))
        data["rows"][0]["hosted_gate"] = "Build / Imaginary"
        temp_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
        unknown = run_verify(temp_path)
        assert unknown.returncode != 0, "unknown hosted gate should fail matrix status verification"
        assert "references unknown hosted_gate" in unknown.stderr

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir) / "matrix_status.json"
        data = json.loads(source.read_text(encoding="utf-8"))
        unity_macos = next(
            pipeline
            for pipeline in data["audio_pipelines"]
            if pipeline["product"] == "unity" and pipeline["platform"] == "macos"
        )
        unity_macos["backend"] = "Null"
        temp_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
        null_audio = run_verify(temp_path)
        assert null_audio.returncode != 0, "null audio must fail matrix status verification"
        assert "expected 'AVFoundation'" in null_audio.stderr

    print("Matrix status release policy tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
