#!/usr/bin/env python3
"""Verify required and forbidden markers in CI log artifacts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def count_marker(text: str, marker: str) -> int:
    return text.count(marker)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, action="append", required=True)
    parser.add_argument("--require", action="append", default=[])
    parser.add_argument(
        "--optional",
        action="append",
        default=[],
        help="Record a diagnostic marker without making its absence fail the contract.",
    )
    parser.add_argument("--forbid", action="append", default=[])
    parser.add_argument("--label", default="log-marker-contract")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    errors: list[str] = []
    logs = []
    combined_text = ""
    for log_path in args.log:
        if not log_path.exists():
            errors.append(f"Missing log file: {log_path}")
            logs.append({"path": log_path.as_posix(), "exists": False, "byte_size": 0})
            continue
        text = log_path.read_text(encoding="utf-8", errors="replace")
        combined_text += text
        logs.append({"path": log_path.as_posix(), "exists": True, "byte_size": log_path.stat().st_size})

    required = []
    for marker in args.require:
        count = count_marker(combined_text, marker)
        required.append({"marker": marker, "count": count, "passed": count > 0})
        if count <= 0:
            errors.append(f"Required marker not found: {marker}")

    optional = []
    for marker in args.optional:
        count = count_marker(combined_text, marker)
        optional.append({"marker": marker, "count": count, "present": count > 0})

    forbidden = []
    for marker in args.forbid:
        count = count_marker(combined_text, marker)
        forbidden.append({"marker": marker, "count": count, "passed": count == 0})
        if count > 0:
            errors.append(f"Forbidden marker found: {marker}")

    result = {
        "schema": "imm-log-marker-contract-v1",
        "label": args.label,
        "logs": logs,
        "required": required,
        "optional": optional,
        "forbidden": forbidden,
        "passed": not errors,
        "errors": errors,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")

    if errors:
        for error in errors:
            print(error)
        return 1

    print(f"Log marker contract passed: {args.label}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
