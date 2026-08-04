#!/usr/bin/env python3
"""Combine all Unity synthetic-stereo evidence into one authoritative status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def read_json(path: Path) -> dict | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def details(label: str, value: dict) -> list[str]:
    failures = value.get("errors") or value.get("failures") or []
    if not isinstance(failures, list):
        failures = [failures]
    suffix = "; ".join(str(item) for item in failures if str(item))
    return [f"{label} failed{f': {suffix}' if suffix else ''}"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-status", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--left-capture", type=Path, required=True)
    parser.add_argument("--right-capture", type=Path, required=True)
    parser.add_argument("--stereo-structure", type=Path, required=True)
    parser.add_argument("--left-metrics", type=Path, required=True)
    parser.add_argument("--right-metrics", type=Path, required=True)
    parser.add_argument("--log-contract", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    json_inputs = {
        "runtime classification": args.runtime_status,
        "stereo structure": args.stereo_structure,
        "left-eye visual contract": args.left_metrics,
        "right-eye visual contract": args.right_metrics,
        "stereo routing contract": args.log_contract,
    }
    values = {label: read_json(path) for label, path in json_inputs.items()}
    evidence_failures = [
        f"missing or invalid {label}: {json_inputs[label]}"
        for label, value in values.items()
        if value is None
    ]
    evidence_failures.extend(
        f"missing synthetic-stereo capture: {path}"
        for path in (args.capture, args.left_capture, args.right_capture)
        if not path.is_file()
    )

    runtime = values["runtime classification"]
    runtime_failures: list[str] = []
    if runtime is not None and runtime.get("result") == "runtime_failed":
        runtime_failures = details("runtime classification", runtime)

    render_failures: list[str] = []
    for label in (
        "stereo structure",
        "left-eye visual contract",
        "right-eye visual contract",
        "stereo routing contract",
    ):
        value = values[label]
        if value is None:
            continue
        passed = value.get("passed") is True or value.get("status") == "passed"
        if not passed:
            render_failures.extend(details(label, value))

    if runtime_failures:
        result = "runtime_failed"
        failure_class = "runtime"
        failures = runtime_failures
        warnings = evidence_failures + render_failures
    elif evidence_failures:
        result = "evidence_incomplete"
        failure_class = "evidence"
        failures = evidence_failures
        warnings = render_failures
    elif render_failures:
        result = "render_failed"
        failure_class = "rendering"
        failures = render_failures
        warnings = []
    else:
        result = "passed"
        failure_class = ""
        failures = []
        warnings = []

    status = {
        "schema": "imm-composition-status-v1",
        "result": result,
        "rendering": "success" if result == "passed" else "failed",
        "failure_class": failure_class,
        "failures": failures,
        "warnings": warnings,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(status, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"Unity synthetic-stereo status written: {args.output}")
    return 0 if result == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
