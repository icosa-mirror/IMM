#!/usr/bin/env python3
"""Classify Godot visual smoke output into render and compositing status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


RENDER_FAILURE_MARKERS = [
    "visual smoke PNG was too flat",
    "visual smoke PNG had only",
    "visual smoke PNG content bounds were too small",
    "visual smoke PNG orientation check failed",
    "ImmViewer did not load",
    "ImmViewer sequence was not ready",
    "render diagnostics did not report IMM draw calls",
    "render diagnostics did not report foreground paint draw calls",
    "ImmViewerCompositorEffect did not start a Metal frame",
    "ImmViewerCompositorEffect did not start a Vulkan frame",
    "ImmGodot_RenderCamera returned",
]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--renderer", required=True, choices=["metal", "vulkan"])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="ignore") if args.log.exists() else ""
    composition_failures = [
        line.strip().removeprefix("ERROR: ").strip()
        for line in text.splitlines()
        if "scene composition" in line and "probe failed" in line
    ]
    render_failures = [marker for marker in RENDER_FAILURE_MARKERS if marker in text]
    success_marker = f"IMM Godot {args.renderer.title()} visual smoke passed"
    rendering_succeeded = success_marker in text or (
        "visual smoke content diagnostics" in text
        and "visual smoke render diagnostics" in text
        and "visual smoke compositor diagnostics" in text
        and not render_failures
    )

    status = {
        "schema": "imm-composition-status-v1",
        "rendering": "success" if rendering_succeeded else "failed",
        "compositing": "expected_failed" if composition_failures else ("success" if rendering_succeeded else "unknown"),
        "failure_class": "compositing" if composition_failures else ("" if rendering_succeeded else "visual"),
        "failures": composition_failures + [f"rendering failure marker: {marker}" for marker in render_failures],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Godot visual smoke status written: {args.output}")

    if status["rendering"] == "success" and status["compositing"] in {"success", "expected_failed"}:
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
