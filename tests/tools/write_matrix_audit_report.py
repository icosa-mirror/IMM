#!/usr/bin/env python3
"""Write human-readable and machine-readable testing matrix audit reports."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def row_key(row: dict) -> str:
    return "/".join(str(row.get(key, "")) for key in ["product", "platform", "mode", "renderer"])


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


def summarize(rows: list[dict]) -> dict:
    status_counts = Counter(str(row.get("status", "")) for row in rows)
    supported_rows = [row for row in rows if row.get("status") == "supported"]
    deferred_rows = [row for row in rows if row.get("status") == "deferred"]
    unsupported_rows = [row for row in rows if row.get("status") == "unsupported"]
    waived_rows = [row for row in rows if row.get("status") == "waived"]
    hardware_rows = [row for row in rows if row.get("hardware_gate")]
    hosted_rows = [row for row in rows if row.get("hosted_gate")]
    release_blockers = [row for row in rows if row.get("status") == "deferred"]
    return {
        "row_count": len(rows),
        "status_counts": dict(sorted(status_counts.items())),
        "supported_count": len(supported_rows),
        "deferred_count": len(deferred_rows),
        "unsupported_count": len(unsupported_rows),
        "waived_count": len(waived_rows),
        "hosted_gate_count": len(hosted_rows),
        "hardware_gate_count": len(hardware_rows),
        "release_blocker_count": len(release_blockers),
        "release_blockers": [row_key(row) for row in release_blockers],
    }


def build_markdown(data: dict, summary: dict) -> str:
    rows = data.get("rows", [])
    lines = [
        "# IMM Testing Matrix Audit",
        "",
        f"- Schema: {data.get('schema', '')}",
        f"- Updated: {data.get('updated', '')}",
        f"- Rows: {summary['row_count']}",
        f"- Supported: {summary['supported_count']}",
        f"- Deferred: {summary['deferred_count']}",
        f"- Unsupported: {summary['unsupported_count']}",
        f"- Waived: {summary['waived_count']}",
        f"- Hosted gates: {summary['hosted_gate_count']}",
        f"- Hardware gates: {summary['hardware_gate_count']}",
        f"- Release blockers: {summary['release_blocker_count']}",
        "",
    ]

    if summary["release_blockers"]:
        lines.append("## Release Blockers")
        blocker_rows = [["Row", "Promotion Criteria"]]
        for row in rows:
            if row.get("status") == "deferred":
                blocker_rows.append([row_key(row), str(row.get("promotion_criteria", ""))])
        lines.extend(markdown_table(blocker_rows))
        lines.append("")

    lines.append("## Matrix Rows")
    table = [["Row", "Status", "Hosted Gate", "Hardware Gate", "Baseline"]]
    for row in rows:
        table.append(
            [
                row_key(row),
                str(row.get("status", "")),
                str(row.get("hosted_gate") or ""),
                str(row.get("hardware_gate") or ""),
                str(row.get("baseline") or ""),
            ]
        )
    lines.extend(markdown_table(table))
    lines.append("")

    lines.append("## Unsupported And Waived Decisions")
    decision_rows = [["Row", "Status", "Owner Decision"]]
    for row in rows:
        if row.get("status") in {"unsupported", "waived"}:
            decision_rows.append([row_key(row), str(row.get("status", "")), str(row.get("owner_decision", ""))])
    if len(decision_rows) > 1:
        lines.extend(markdown_table(decision_rows))
    else:
        lines.append("No unsupported or waived rows.")
    lines.append("")

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix_status", type=Path)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    args = parser.parse_args()

    data = json.loads(args.matrix_status.read_text(encoding="utf-8"))
    rows = data.get("rows", [])
    if not isinstance(rows, list):
        rows = []
    summary = summarize(rows)
    audit = {
        "schema": "imm-testing-matrix-audit-v1",
        "matrix_schema": data.get("schema", ""),
        "updated": data.get("updated", ""),
        "summary": summary,
        "rows": [
            {
                "key": row_key(row),
                "status": row.get("status", ""),
                "hosted_gate": row.get("hosted_gate"),
                "hardware_gate": row.get("hardware_gate"),
                "baseline": row.get("baseline"),
                "owner_decision": row.get("owner_decision", ""),
                "promotion_criteria": row.get("promotion_criteria", ""),
            }
            for row in rows
        ],
    }

    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(audit, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    args.markdown_output.write_text(build_markdown(data, summary), encoding="utf-8", newline="\n")
    print(f"Matrix audit JSON written: {args.json_output}")
    print(f"Matrix audit report written: {args.markdown_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
