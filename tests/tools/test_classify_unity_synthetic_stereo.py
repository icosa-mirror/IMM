#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value), encoding="utf-8")


def invoke(tool: Path, root: Path) -> tuple[subprocess.CompletedProcess[str], dict]:
    completed = subprocess.run(
        [
            sys.executable,
            str(tool),
            "--runtime-status", str(root / "runtime.json"),
            "--capture", str(root / "stereo.png"),
            "--left-capture", str(root / "left.ppm"),
            "--right-capture", str(root / "right.ppm"),
            "--stereo-structure", str(root / "structure.json"),
            "--left-metrics", str(root / "left.json"),
            "--right-metrics", str(root / "right.json"),
            "--log-contract", str(root / "log.json"),
            "--output", str(root / "status.json"),
        ],
        text=True,
        capture_output=True,
    )
    return completed, json.loads((root / "status.json").read_text(encoding="utf-8"))


def main() -> int:
    tool = Path(__file__).with_name("classify_unity_synthetic_stereo.py")
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        for name in ("stereo.png", "left.ppm", "right.ppm"):
            (root / name).write_bytes(b"capture")
        write_json(root / "runtime.json", {"result": "passed", "failures": []})
        write_json(root / "structure.json", {"status": "passed", "failures": []})
        write_json(root / "left.json", {"passed": True, "errors": []})
        write_json(root / "right.json", {"passed": True, "errors": []})
        write_json(root / "log.json", {"status": "passed", "failures": []})

        passed, status = invoke(tool, root)
        assert passed.returncode == 0, passed.stdout + passed.stderr
        assert status["result"] == "passed"

        write_json(root / "right.json", {"passed": False, "errors": ["sky-only eye"]})
        failed, status = invoke(tool, root)
        assert failed.returncode != 0
        assert status["result"] == "render_failed"
        assert status["failure_class"] == "rendering"
        assert any("sky-only eye" in item for item in status["failures"])

        write_json(
            root / "runtime.json",
            {"result": "runtime_failed", "failures": ["Vulkan unavailable"]},
        )
        (root / "left.ppm").unlink()
        runtime_failed, status = invoke(tool, root)
        assert runtime_failed.returncode != 0
        assert status["result"] == "runtime_failed"
        assert status["failure_class"] == "runtime"

        write_json(root / "runtime.json", {"result": "passed", "failures": []})
        incomplete, status = invoke(tool, root)
        assert incomplete.returncode != 0
        assert status["result"] == "evidence_incomplete"
        assert status["failure_class"] == "evidence"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
