#!/usr/bin/env python3
"""Focused tests for Unity macOS Metal lane classification."""

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
    root.mkdir(parents=True, exist_ok=True)
    for name in (
        "unity-macos-metal-editor-play.png",
        "unity-macos-metal-render.png",
        "unity-macos-metal-full-depth.png",
        "unity-macos-metal-ordered-overlay.png",
    ):
        (root / name).write_bytes(b"capture")
    for name in (
        "unity-macos-metal-editor-play-metrics.json",
        "unity-macos-metal-render-metrics.json",
        "unity-macos-metal-full-depth-metrics.json",
        "unity-macos-metal-ordered-overlay-metrics.json",
    ):
        write_metric(root / name, True)
    (root / "unity-editor-play.log").write_text(
        "[IMM_EDITOR_SMOKE] passed: capture.png\n",
        encoding="utf-8",
    )


def classify(
    root: Path,
    player_outcome: str = "success",
    editor_outcome: str = "success",
) -> tuple[subprocess.CompletedProcess[str], dict, dict, dict, dict]:
    output = root / "lane-status.json"
    editor = root / "editor-status.json"
    full_depth = root / "full-depth-status.json"
    overlay = root / "overlay-status.json"
    completed = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tests/tools/classify_unity_macos_metal.py"),
            "--artifact-dir", str(root),
            "--player-build-outcome", player_outcome,
            "--editor-play-outcome", editor_outcome,
            "--output", str(output),
            "--editor-play-status-output", str(editor),
            "--full-depth-status-output", str(full_depth),
            "--ordered-overlay-status-output", str(overlay),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    return (
        completed,
        json.loads(output.read_text(encoding="utf-8")),
        json.loads(editor.read_text(encoding="utf-8")),
        json.loads(full_depth.read_text(encoding="utf-8")),
        json.loads(overlay.read_text(encoding="utf-8")),
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        prepare(root)
        completed, lane, editor, full_depth, overlay = classify(root)
        assert completed.returncode == 0
        assert lane["result"] == "passed"
        assert editor["rendering"] == "success"
        assert full_depth["compositing"] == "success"
        assert overlay["ordered_overlay"] == "success"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        prepare(root)
        write_metric(root / "unity-macos-metal-full-depth-metrics.json", False, "cyan leakage")
        completed, lane, _editor, full_depth, _overlay = classify(root)
        assert completed.returncode == 1
        assert lane["result"] == "composition_failed"
        assert lane["failure_class"] == "compositing"
        assert full_depth["rendering"] == "success"
        assert full_depth["compositing"] == "failed"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        prepare(root)
        write_metric(root / "unity-macos-metal-editor-play-metrics.json", False, "default scene")
        completed, lane, editor, _full_depth, _overlay = classify(root)
        assert completed.returncode == 1
        assert lane["result"] == "render_failed"
        assert lane["failure_class"] == "rendering"
        assert editor["rendering"] == "failed"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        prepare(root)
        (root / "unity-macos-metal-editor-play.png").unlink()
        completed, lane, _editor, _full_depth, _overlay = classify(
            root, editor_outcome="failure"
        )
        assert completed.returncode == 1
        assert lane["result"] == "runtime_failed"
        assert lane["failure_class"] == "runtime"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        prepare(root)
        completed, lane, _editor, _full_depth, _overlay = classify(
            root, editor_outcome="failure"
        )
        assert completed.returncode == 0
        assert lane["result"] == "passed"
        assert lane["warnings"], "Complete passing evidence should outrank a secondary exit code"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        prepare(root)
        (root / "unity-full-depth-player.log").write_text(
            "[IMM_UNITY_SMOKE] graphics api probe failed\n",
            encoding="utf-8",
        )
        completed, lane, _editor, _full_depth, _overlay = classify(root)
        assert completed.returncode == 1
        assert lane["result"] == "runtime_failed"
        assert lane["failure_class"] == "runtime"

    print("Unity macOS Metal classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
