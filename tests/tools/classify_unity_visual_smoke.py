#!/usr/bin/env python3
"""Classify Unity visual smoke output into render and compositing status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


RENDER_FAILURE_MARKERS = [
    "[IMM_EDITOR_SMOKE] timed out waiting for capture",
    "[IMM_EDITOR_SMOKE] failed:",
    "[IMM_UNITY_SMOKE] invalid screen size",
]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="ignore") if args.log.exists() else ""
    composition_failures = [
        line.strip()
        for line in text.splitlines()
        if "[IMM_UNITY_SMOKE] scene composition" in line and "failed" in line
    ]
    render_failures = [marker for marker in RENDER_FAILURE_MARKERS if marker in text]
    rendering_succeeded = "[IMM_EDITOR_SMOKE] passed:" in text and args.capture.exists() and not render_failures

    status = {
        "schema": "imm-composition-status-v1",
        "rendering": "success" if rendering_succeeded else "failed",
        "compositing": "expected_failed" if composition_failures else ("success" if rendering_succeeded else "unknown"),
        "failure_class": "compositing" if composition_failures else ("" if rendering_succeeded else "visual"),
        "failures": composition_failures + [f"rendering failure marker: {marker}" for marker in render_failures],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Unity visual smoke status written: {args.output}")

    if status["rendering"] == "success" and status["compositing"] in {"success", "expected_failed"}:
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
