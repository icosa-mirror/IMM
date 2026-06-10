#!/usr/bin/env python3
"""Write JSON and Markdown reports for IMM baseline drift."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def flatten(value: object, prefix: str = "") -> dict[str, object]:
    if isinstance(value, dict):
        result: dict[str, object] = {}
        for key, child in value.items():
            child_prefix = f"{prefix}.{key}" if prefix else str(key)
            result.update(flatten(child, child_prefix))
        return result
    if isinstance(value, list):
        result = {}
        for index, child in enumerate(value):
            child_prefix = f"{prefix}[{index}]"
            result.update(flatten(child, child_prefix))
        return result
    return {prefix: value}


def drift(expected: object, actual: object) -> dict:
    expected_flat = flatten(expected)
    actual_flat = flatten(actual)
    expected_keys = set(expected_flat)
    actual_keys = set(actual_flat)
    changed = []
    for key in sorted(expected_keys & actual_keys):
        if expected_flat[key] != actual_flat[key]:
            changed.append(
                {
                    "path": key,
                    "expected": expected_flat[key],
                    "actual": actual_flat[key],
                }
            )
    return {
        "added": [{"path": key, "actual": actual_flat[key]} for key in sorted(actual_keys - expected_keys)],
        "removed": [{"path": key, "expected": expected_flat[key]} for key in sorted(expected_keys - actual_keys)],
        "changed": changed,
    }


def markdown_table(rows: list[list[str]]) -> list[str]:
    if not rows:
        return []
    widths = [max(len(row[index]) for row in rows) for index in range(len(rows[0]))]
    lines = [
        "| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(rows[0])) + " |",
        "| " + " | ".join("-" * widths[index] for index in range(len(rows[0]))) + " |",
    ]
    for row in rows[1:]:
        lines.append("| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(row)) + " |")
    return lines


def render_markdown(report: dict) -> str:
    summary = report["summary"]
    lines = [
        "# IMM Baseline Drift Report",
        "",
        f"- Expected baseline: {report['expected']}",
        f"- Actual baseline: {report['actual']}",
        f"- Result: {'drift detected' if summary['has_drift'] else 'no drift'}",
        f"- Added paths: {summary['added_count']}",
        f"- Removed paths: {summary['removed_count']}",
        f"- Changed paths: {summary['changed_count']}",
        "",
    ]
    for section, columns in [
        ("changed", ["Path", "Expected", "Actual"]),
        ("added", ["Path", "Actual"]),
        ("removed", ["Path", "Expected"]),
    ]:
        entries = report["drift"][section]
        if not entries:
            continue
        lines.append(f"## {section.title()}")
        table = [columns]
        for entry in entries:
            if section == "changed":
                table.append([entry["path"], json.dumps(entry["expected"], sort_keys=True), json.dumps(entry["actual"], sort_keys=True)])
            elif section == "added":
                table.append([entry["path"], json.dumps(entry["actual"], sort_keys=True)])
            else:
                table.append([entry["path"], json.dumps(entry["expected"], sort_keys=True)])
        lines.extend(markdown_table(table))
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected", type=Path, required=True)
    parser.add_argument("--actual", type=Path, required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    args = parser.parse_args()

    expected = json.loads(args.expected.read_text(encoding="utf-8"))
    actual = json.loads(args.actual.read_text(encoding="utf-8"))
    drift_data = drift(expected, actual)
    summary = {
        "added_count": len(drift_data["added"]),
        "removed_count": len(drift_data["removed"]),
        "changed_count": len(drift_data["changed"]),
    }
    summary["has_drift"] = any(summary[key] for key in ["added_count", "removed_count", "changed_count"])
    report = {
        "schema": "imm-baseline-drift-report-v1",
        "expected": args.expected.as_posix(),
        "actual": args.actual.as_posix(),
        "summary": summary,
        "drift": drift_data,
    }

    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    args.markdown_output.write_text(render_markdown(report), encoding="utf-8", newline="\n")
    print(f"Baseline drift JSON written: {args.json_output}")
    print(f"Baseline drift report written: {args.markdown_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
