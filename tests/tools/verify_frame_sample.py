#!/usr/bin/env python3
"""Check raw rendered-frame evidence. Local floors are explicit, never inferred."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path


def positive(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value > 0


def verify(sample: dict, minimum_fps: float | None = None) -> dict:
    errors = []
    if sample.get("schema") != "imm-frame-sample-v1":
        errors.append("Unknown frame sample schema")
    if sample.get("product") not in ("standalone", "unity", "godot", "web"):
        errors.append("Missing or unknown host")
    if not isinstance(sample.get("graphics_api"), str) or sample.get("graphics_api") in ("", "unknown"):
        errors.append("Missing actual graphics API")
    for field in ("width", "height"):
        value = sample.get(field)
        if not isinstance(value, int) or isinstance(value, bool) or value <= 1:
            errors.append(f"Invalid actual {field}")
    for field in ("warmup_seconds", "requested_seconds", "observed_seconds"):
        if not positive(sample.get(field)):
            errors.append(f"Invalid {field}")
    requested = sample.get("requested_seconds")
    observed = sample.get("observed_seconds")
    if positive(requested) and requested < 5:
        errors.append("Sampling must request at least five seconds")
    if positive(requested) and positive(observed) and observed < requested:
        errors.append("Sampling ended before the requested duration")
    rendered = sample.get("imm_rendered_frame_count")
    engine = sample.get("engine_frame_count")
    if not isinstance(rendered, int) or isinstance(rendered, bool) or rendered < 2:
        errors.append("Insufficient actual IMM rendered frames")
    if not isinstance(engine, int) or isinstance(engine, bool) or engine < 0:
        errors.append("Invalid engine frame count")
    elif isinstance(rendered, int) and rendered > engine:
        errors.append("Rendered frames exceed engine frames (duplicate cameras or eyes)")
    intervals = sample.get("intervals_ms")
    if not isinstance(intervals, list) or not intervals or not all(positive(x) for x in intervals):
        errors.append("Missing, non-finite or non-positive rendered-frame intervals")
    elif len(intervals) != rendered:
        errors.append("Rendered-frame count does not match intervals")
    elif positive(observed) and sum(intervals) > observed * 1000 + 0.01:
        errors.append("Rendered intervals exceed the measured elapsed time")
    producer_errors = sample.get("errors")
    if not isinstance(producer_errors, list):
        errors.append("Missing producer validity diagnostics")
    elif producer_errors:
        errors.extend(f"Producer: {error}" for error in producer_errors)
    if minimum_fps is not None and not positive(minimum_fps):
        errors.append("Minimum FPS must be positive and finite")
    metrics = {}
    if not errors:
        ordered = sorted(intervals)
        metrics = {
            "mean_fps": rendered / observed,
            "mean_frame_ms": observed * 1000 / rendered,
            "p95_render_interval_ms": ordered[math.ceil(len(ordered) * 0.95) - 1],
            "maximum_render_gap_ms": max(max(intervals), observed * 1000 - sum(intervals)),
        }
    threshold_passed = None if errors or minimum_fps is None else metrics["mean_fps"] >= minimum_fps
    return {
        "schema": "imm-frame-verdict-v1",
        "valid": not errors,
        "validity_errors": errors,
        "minimum_mean_fps": minimum_fps,
        "threshold_passed": threshold_passed,
        "metrics": metrics,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sample", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--minimum-fps", type=float, help="Explicit local check; not a calibrated CI hardware gate")
    args = parser.parse_args()
    try:
        raw = args.sample.read_bytes()
        sample = json.loads(raw)
        if not isinstance(sample, dict):
            raise ValueError("Sample must be an object")
        result = verify(sample, args.minimum_fps)
        result["sample_sha256"] = hashlib.sha256(raw).hexdigest()
    except (OSError, ValueError) as error:
        result = {"valid": False, "validity_errors": [str(error)], "threshold_passed": None}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(f"{json.dumps(result, indent=2, allow_nan=False)}\n", encoding="utf-8")
    failed = not result["valid"] or result["threshold_passed"] is False
    print(f"Frame sample: {'FAIL' if failed else 'valid'}; verdict: {args.output}")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
