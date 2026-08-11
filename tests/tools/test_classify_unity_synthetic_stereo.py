#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value), encoding="utf-8")


def invoke(
    tool: Path, root: Path, extra_args: list[str] | None = None
) -> tuple[subprocess.CompletedProcess[str], dict]:
    command = [
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
        ]
    command.extend(extra_args or [])
    completed = subprocess.run(
        command,
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
        assert status["failures"] == ["runtime classification failed: Vulkan unavailable"]
        assert status["warnings"], "Missing downstream evidence should remain diagnostic"

        runtime_log = root / "player.log"
        runtime_log.write_text(
            "Forcing GfxDevice: Vulkan\n"
            "Vulkan detection: 0\n"
            "[IMM_UNITY_SMOKE] graphics api expected=Vulkan actual=Direct3D11\n",
            encoding="utf-8",
        )
        skipped, status = invoke(
            tool,
            root,
            [
                "--runtime-log",
                str(runtime_log),
                "--allow-host-vulkan-rejection",
            ],
        )
        assert skipped.returncode == 0, skipped.stdout + skipped.stderr
        assert status["result"] == "skipped"
        assert status["rendering"] == "not_tested"
        assert status["failure_class"] == ""

        runtime_log.write_text(
            "Forcing GfxDevice: Vulkan\nVulkan detection: 0\n",
            encoding="utf-8",
        )
        not_exact, status = invoke(
            tool,
            root,
            [
                "--runtime-log",
                str(runtime_log),
                "--allow-host-vulkan-rejection",
            ],
        )
        assert not_exact.returncode != 0
        assert status["result"] == "runtime_failed"

        write_json(root / "runtime.json", {"result": "passed", "failures": []})
        incomplete, status = invoke(tool, root)
        assert incomplete.returncode != 0
        assert status["result"] == "evidence_incomplete"
        assert status["failure_class"] == "evidence"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
