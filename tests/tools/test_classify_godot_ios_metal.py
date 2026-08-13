#!/usr/bin/env python3
"""Focused negative and success tests for Godot iOS Metal classification."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def write_metric(path: Path, passed: bool, failure: str = "visual mismatch") -> None:
    path.write_text(
        json.dumps({"passed": passed, "errors": [] if passed else [failure]}),
        encoding="utf-8",
    )


def prepare(root: Path) -> None:
    for stem in ("render", "full-depth", "ordered-overlay", "sample-play"):
        (root / f"godot-ios-metal-{stem}.png").write_bytes(b"capture")
        write_metric(root / f"godot-ios-metal-{stem}-metrics.json", True)
    (root / "godot-ios-sample-play.png").write_bytes(b"capture")
    write_metric(root / "godot-ios-sample-play-metrics.json", True)
    for mode in ("full-depth", "ordered-overlay"):
        (root / f"godot-ios-{mode}-result.log").write_text(
            "[IMM_GODOT_VISUAL_RESULT_20260813] passed renderer=Metal os=iOS\n",
            encoding="utf-8",
        )
    (root / "godot-ios-sample-play.log").write_text(
        "[IMM_GODOT_SAMPLE_PLAY_20260803] passed os=iOS capture=user://capture.png layers=3 camera_ids=[0]\n",
        encoding="utf-8",
    )


def run(root: Path, outcome: str = "success") -> tuple[subprocess.CompletedProcess[str], dict, dict, dict]:
    output = root / "lane-status.json"
    full = root / "full-depth-status.json"
    overlay = root / "ordered-overlay-status.json"
    completed = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tests/tools/classify_godot_ios_metal.py"),
            "--artifact-dir", str(root),
            "--runtime-outcome", outcome,
            "--output", str(output),
            "--full-depth-status-output", str(full),
            "--ordered-overlay-status-output", str(overlay),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    return completed, *(json.loads(path.read_text(encoding="utf-8")) for path in (output, full, overlay))


def main() -> int:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        completed, lane, full, overlay = run(root)
        assert completed.returncode == 0
        assert lane["result"] == "passed"
        assert lane["run_button"] == "success"
        assert lane["depth_composition"] == "success"
        assert lane["ordered_overlay"] == "success"
        assert full["depth_composition"] == "success"
        assert overlay["ordered_overlay"] == "success"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        write_metric(root / "godot-ios-metal-render-metrics.json", False, "default scene")
        completed, lane, _full, _overlay = run(root)
        assert completed.returncode == 1
        assert lane["result"] == "render_failed"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        write_metric(root / "godot-ios-metal-full-depth-metrics.json", False, "cyan leakage")
        completed, lane, full, _overlay = run(root)
        assert completed.returncode == 1
        assert lane["result"] == "composition_failed"
        assert full["depth_composition"] == "failed"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        (root / "godot-ios-metal-ordered-overlay.png").unlink()
        completed, lane, _full, _overlay = run(root)
        assert completed.returncode == 1
        assert lane["result"] == "evidence_incomplete"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        (root / "godot-ios-full-depth-result.log").write_text(
            "[IMM_GODOT_VISUAL_RESULT_20260813] failed renderer=Metal os=iOS failures=[device lost]\n",
            encoding="utf-8",
        )
        completed, lane, _full, _overlay = run(root)
        assert completed.returncode == 1
        assert lane["result"] == "runtime_failed"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        (root / "godot-ios-sample-play.log").write_text(
            "[IMM_GODOT_SAMPLE_PLAY_20260803] passed os=macOS\n",
            encoding="utf-8",
        )
        completed, lane, _full, _overlay = run(root)
        assert completed.returncode == 1
        assert lane["result"] == "runtime_failed"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        prepare(root)
        completed, lane, _full, _overlay = run(root, "failure")
        assert completed.returncode == 1
        assert lane["result"] == "runtime_failed"

    print("Godot iOS Metal classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
