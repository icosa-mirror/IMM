#!/usr/bin/env python3
"""Focused tests for Windows Godot Vulkan lane classification."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def write_metric(path: Path, passed: bool, failure: str = "visual mismatch") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"passed": passed, "errors": [] if passed else [failure]}),
        encoding="utf-8",
    )


def prepare(root: Path) -> tuple[Path, Path, Path]:
    main = root / "main"
    render_only = root / "render-only"
    overlay = root / "ordered-overlay"
    for directory in (main, render_only, overlay):
        directory.mkdir(parents=True)
    for path in (
        main / "godot-sample-play.png",
        main / "godot-vulkan-render.ppm",
        main / "godot-vulkan-visual.ppm",
        overlay / "godot-vulkan-ordered-overlay.ppm",
    ):
        path.write_bytes(b"capture")
    write_metric(main / "godot-sample-play-metrics.json", True)
    write_metric(main / "godot-vulkan-render-metrics.json", True)
    write_metric(main / "godot-vulkan-full-depth-render-metrics.json", True)
    write_metric(overlay / "godot-vulkan-ordered-overlay-metrics.json", True)
    (main / "godot.log").write_text("ordinary diagnostic output\n", encoding="utf-8")
    return main, render_only, overlay


def classify(root: Path) -> tuple[subprocess.CompletedProcess[str], dict, dict, dict]:
    main, render_only, overlay = prepare(root)
    lane = root / "lane.json"
    full_depth = root / "full-depth.json"
    ordered_overlay = root / "ordered-overlay.json"
    completed = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tests/tools/classify_windows_godot_vulkan.py"),
            "--artifact-dir",
            str(main),
            "--render-only-dir",
            str(render_only),
            "--ordered-overlay-dir",
            str(overlay),
            "--output",
            str(lane),
            "--full-depth-status-output",
            str(full_depth),
            "--ordered-overlay-status-output",
            str(ordered_overlay),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    return (
        completed,
        json.loads(lane.read_text(encoding="utf-8")),
        json.loads(full_depth.read_text(encoding="utf-8")),
        json.loads(ordered_overlay.read_text(encoding="utf-8")),
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        completed, lane, full_depth, overlay = classify(Path(temp_dir))
        assert completed.returncode == 0
        assert lane["result"] == "passed"
        assert lane["warnings"], "A missing redundant log marker should be a warning"
        assert full_depth["rendering"] == "success"
        assert full_depth["compositing"] == "success"
        assert overlay["rendering"] == "success"
        assert overlay["ordered_overlay"] == "success"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        main_dir, _render_only, overlay_dir = prepare(root)
        write_metric(
            overlay_dir / "godot-vulkan-ordered-overlay-metrics.json",
            False,
            "cyan leakage",
        )
        # Re-run without letting classify() recreate the passing metric.
        lane_path = root / "lane.json"
        full_path = root / "full.json"
        overlay_path = root / "overlay.json"
        completed = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tests/tools/classify_windows_godot_vulkan.py"),
                "--artifact-dir", str(main_dir),
                "--render-only-dir", str(root / "render-only"),
                "--ordered-overlay-dir", str(overlay_dir),
                "--output", str(lane_path),
                "--full-depth-status-output", str(full_path),
                "--ordered-overlay-status-output", str(overlay_path),
            ],
            check=False,
        )
        lane = json.loads(lane_path.read_text(encoding="utf-8"))
        overlay = json.loads(overlay_path.read_text(encoding="utf-8"))
        assert completed.returncode == 1
        assert lane["result"] == "composition_failed"
        assert lane["failure_class"] == "compositing"
        assert overlay["rendering"] == "success"
        assert overlay["compositing"] == "failed"
        assert overlay["ordered_overlay"] == "failed"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        main_dir, _render_only, _overlay = prepare(root)
        write_metric(main_dir / "godot-vulkan-render-metrics.json", False, "reverse Z")
        completed, lane, _full_depth, _overlay_status = classify_existing(root)
        assert completed.returncode == 1
        assert lane["result"] == "render_failed"
        assert lane["failure_class"] == "rendering"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        main_dir, _render_only, _overlay = prepare(root)
        (main_dir / "godot-vulkan-visual.ppm").unlink()
        completed, lane, _full_depth, _overlay_status = classify_existing(root)
        assert completed.returncode == 1
        assert lane["result"] == "evidence_incomplete"
        assert lane["failure_class"] == "evidence"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        main_dir, _render_only, _overlay = prepare(root)
        (main_dir / "godot.log").write_text("VK_ERROR_DEVICE_LOST\n", encoding="utf-8")
        completed, lane, _full_depth, _overlay_status = classify_existing(root)
        assert completed.returncode == 1
        assert lane["result"] == "runtime_failed"
        assert lane["failure_class"] == "runtime"

    print("Windows Godot Vulkan classifier tests passed")
    return 0


def classify_existing(root: Path) -> tuple[subprocess.CompletedProcess[str], dict, dict, dict]:
    lane_path = root / "lane.json"
    full_path = root / "full.json"
    overlay_path = root / "overlay.json"
    completed = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tests/tools/classify_windows_godot_vulkan.py"),
            "--artifact-dir", str(root / "main"),
            "--render-only-dir", str(root / "render-only"),
            "--ordered-overlay-dir", str(root / "ordered-overlay"),
            "--output", str(lane_path),
            "--full-depth-status-output", str(full_path),
            "--ordered-overlay-status-output", str(overlay_path),
        ],
        check=False,
    )
    return (
        completed,
        json.loads(lane_path.read_text(encoding="utf-8")),
        json.loads(full_path.read_text(encoding="utf-8")),
        json.loads(overlay_path.read_text(encoding="utf-8")),
    )


if __name__ == "__main__":
    raise SystemExit(main())
