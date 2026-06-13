#!/usr/bin/env python3
"""Classify Unity visual smoke output into render and compositing status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from composition_status import COMPOSITION_CONTRACTS, build_composition_fields, classification_succeeded


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
    parser.add_argument("--composition-mode", choices=sorted(COMPOSITION_CONTRACTS), default="full_depth")
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="ignore") if args.log.exists() else ""
    composition_failures = [
        line.strip()
        for line in text.splitlines()
        if "[IMM_UNITY_SMOKE] scene composition" in line and "failed" in line
    ]
    render_failures = [marker for marker in RENDER_FAILURE_MARKERS if marker in text]
    rendering_succeeded = (
        ("[IMM_EDITOR_SMOKE] passed:" in text or "[IMM_UNITY_SMOKE] capture=" in text)
        and args.capture.exists()
        and not render_failures
    )

    composition_fields = build_composition_fields(args.composition_mode, rendering_succeeded, composition_failures)
    status = {
        "schema": "imm-composition-status-v1",
        "rendering": "success" if rendering_succeeded else "failed",
        "failure_class": "compositing" if composition_failures else ("" if rendering_succeeded else "visual"),
        "failures": composition_failures + [f"rendering failure marker: {marker}" for marker in render_failures],
    }
    status.update(composition_fields)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Unity visual smoke status written: {args.output}")

    if classification_succeeded(status):
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
