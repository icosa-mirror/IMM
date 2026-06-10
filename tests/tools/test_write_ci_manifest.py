#!/usr/bin/env python3
"""Focused checks for CI manifest audit fields."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    root = Path.cwd()
    with tempfile.TemporaryDirectory() as temp_dir:
        output = Path(temp_dir) / "manifest.json"
        fixture = Path(temp_dir) / "fixture.imm"
        fixture.write_bytes(b"fixture")
        included = Path(temp_dir) / "artifact.txt"
        included.write_text("artifact", encoding="utf-8")

        subprocess.run(
            [
                sys.executable,
                "tests/tools/write_ci_manifest.py",
                "--output",
                str(output),
                "--repo-root",
                str(root),
                "--product",
                "standalone",
                "--platform-name",
                "windows",
                "--mode",
                "non-vr",
                "--renderer",
                "directx",
                "--fixture",
                str(fixture),
                "--include",
                str(included),
            ],
            check=True,
        )
        manifest = json.loads(output.read_text(encoding="utf-8"))
        assert manifest["schema"] == "imm-ci-artifact-manifest-v1"
        assert manifest["classification"]["result"] == "passed"
        assert manifest["matrix"]["product"] == "standalone"
        assert manifest["matrix"]["renderer"] == "directx"
        assert manifest["tool_versions"]["python"]["version"]["exit_code"] == 0
        assert manifest["fixtures"][0]["sha256"]
        assert manifest["files"][0]["sha256"]

    print("CI manifest tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
