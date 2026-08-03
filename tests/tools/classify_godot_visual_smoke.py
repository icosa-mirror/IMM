#!/usr/bin/env python3
"""Classify Godot visual smoke output into render and compositing status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from composition_status import COMPOSITION_CONTRACTS, build_composition_fields, classification_succeeded


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
    parser.add_argument("--composition-mode", choices=sorted(COMPOSITION_CONTRACTS), default="full_depth")
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
    if rendering_succeeded and not composition_failures:
        has_scene_composition_diagnostics = (
            "visual smoke scene composition diagnostics" in text
            or "visual smoke PPM scene composition diagnostics" in text
        )
        if args.composition_mode == "full_depth" and not has_scene_composition_diagnostics:
            composition_failures.append("scene composition full depth probe missing failed")
        elif args.composition_mode == "ordered_overlay" and "ordered overlay IMM diagnostics" not in text:
            composition_failures.append("scene composition ordered overlay probe missing failed")

    composition_fields = build_composition_fields(args.composition_mode, rendering_succeeded, composition_failures)
    if not args.log.exists():
        result = "evidence_incomplete"
        failure_class = "evidence"
        evidence_failures = [f"missing log: {args.log}"]
    elif render_failures or not rendering_succeeded:
        result = "render_failed"
        failure_class = "rendering"
        evidence_failures = []
    elif composition_failures:
        result = "composition_failed"
        failure_class = "compositing"
        evidence_failures = []
    else:
        result = "passed"
        failure_class = ""
        evidence_failures = []
    status = {
        "schema": "imm-composition-status-v1",
        "result": result,
        "rendering": "success" if rendering_succeeded else "failed",
        "failure_class": failure_class,
        "failures": evidence_failures + composition_failures + [f"rendering failure marker: {marker}" for marker in render_failures],
    }
    status.update(composition_fields)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Godot visual smoke status written: {args.output}")

    if result == "passed" and classification_succeeded(status):
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
