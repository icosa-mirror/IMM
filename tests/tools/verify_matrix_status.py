#!/usr/bin/env python3
"""Verify machine-readable testing matrix status coverage."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


VALID_STATUSES = {"supported", "deferred", "unsupported", "waived"}
REQUIRED_FIELDS = {"product", "platform", "mode", "renderer", "status", "reason"}
REQUIRED_ROWS = {
    ("standalone", "windows", "non-vr"),
    ("standalone", "windows", "vr"),
    ("standalone", "android", "non-vr"),
    ("standalone", "android", "vr"),
    ("standalone", "ios", "non-vr"),
    ("standalone", "macos", "non-vr"),
    ("standalone", "macos", "vr"),
    ("unity", "all", "non-vr"),
    ("unity", "windows", "vr"),
    ("godot", "windows", "non-vr"),
    ("godot", "android", "non-vr"),
    ("godot", "ios", "non-vr"),
    ("godot", "macos", "non-vr"),
    ("godot", "windows", "vr"),
    ("godot", "android", "vr"),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix_status", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--release", action="store_true", help="Apply release-blocking policy: no deferred rows are allowed")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    data = json.loads(args.matrix_status.read_text(encoding="utf-8"))
    errors: list[str] = []

    if data.get("schema") != "imm-testing-matrix-status-v1":
        errors.append("Unexpected or missing matrix status schema")

    rows = data.get("rows")
    if not isinstance(rows, list) or not rows:
        errors.append("Matrix status must contain a non-empty rows list")
        rows = []

    seen_keys: set[tuple[str, str, str, str]] = set()
    coverage_keys: set[tuple[str, str, str]] = set()
    supported_count = 0

    for index, row in enumerate(rows):
        missing = REQUIRED_FIELDS - set(row)
        if missing:
            errors.append(f"Row {index} is missing fields: {sorted(missing)}")
            continue

        key = (row["product"], row["platform"], row["mode"], row["renderer"])
        if key in seen_keys:
            errors.append(f"Duplicate row key: {key}")
        seen_keys.add(key)
        coverage_keys.add((row["product"], row["platform"], row["mode"]))

        status = row["status"]
        if status not in VALID_STATUSES:
            errors.append(f"Row {key} has invalid status {status!r}")
        if not str(row["reason"]).strip():
            errors.append(f"Row {key} must include a reason")

        baseline = row.get("baseline")
        if baseline:
            baseline_path = repo_root / baseline
            if not baseline_path.exists():
                errors.append(f"Row {key} references missing baseline: {baseline}")

        if status == "supported":
            supported_count += 1
            if not row.get("hosted_gate") and not row.get("hardware_gate"):
                errors.append(f"Supported row {key} must name a hosted_gate or hardware_gate")

        if status in {"deferred", "unsupported", "waived"} and not row.get("reason"):
            errors.append(f"{status} row {key} must explain why it is not fully gated")
        if status in {"deferred", "unsupported", "waived"} and not str(row.get("owner_decision", "")).strip():
            errors.append(f"{status} row {key} must include owner_decision")
        if status == "deferred" and not str(row.get("promotion_criteria", "")).strip():
            errors.append(f"Deferred row {key} must include promotion_criteria")
        if args.release and status == "deferred":
            errors.append(f"Release policy blocks deferred row {key}; promote to supported or mark unsupported/waived with owner_decision")

    missing_required = REQUIRED_ROWS - coverage_keys
    for key in sorted(missing_required):
        errors.append(f"Missing required product/platform/mode coverage row: {key}")

    if supported_count == 0:
        errors.append("Matrix status has no supported rows")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Matrix status verified: {len(rows)} rows, {supported_count} supported")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
