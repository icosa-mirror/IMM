#!/usr/bin/env python3
"""Verify CI artifact summaries provide evidence for matrix rows."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


VALID_SCOPES = {"hardware", "hosted", "all-supported"}


def row_key(row: dict) -> tuple[str, str, str, str]:
    return (
        str(row.get("product", "")),
        str(row.get("platform", "")),
        str(row.get("mode", "")),
        str(row.get("renderer", "")),
    )


def display_key(key: tuple[str, str, str, str]) -> str:
    return "/".join(key)


def is_visual_row(row: dict) -> bool:
    baseline = str(row.get("baseline") or "")
    renderer = str(row.get("renderer") or "")
    return baseline.startswith("tests/baselines/render/") or renderer in {"directx", "vulkan", "metal"}


def is_vr_row(row: dict) -> bool:
    return str(row.get("mode") or "") == "vr" or str(row.get("renderer") or "") == "openxr"


def load_summary(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != "imm-ci-artifact-summary-v1":
        raise ValueError(f"Unexpected artifact summary schema in {path}")
    data["_summary_path"] = path.as_posix()
    return data


def discover_summaries(paths: list[Path], roots: list[Path]) -> list[dict]:
    summary_paths: list[Path] = []
    for path in paths:
        summary_paths.append(path)
    for root in roots:
        summary_paths.extend(sorted(root.rglob("artifact-summary.json")))

    seen: set[Path] = set()
    summaries = []
    for path in summary_paths:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        summaries.append(load_summary(resolved))
    return summaries


def passing_contracts(artifact: dict) -> list[dict]:
    contracts = []
    for contract in artifact.get("contracts", []):
        content = contract.get("content") if isinstance(contract, dict) else None
        if isinstance(content, dict) and content.get("passed") is True:
            contracts.append(contract)
    return contracts


def passing_preflights(artifact: dict) -> list[dict]:
    preflights = []
    for preflight in artifact.get("preflights", []):
        content = preflight.get("content") if isinstance(preflight, dict) else None
        if isinstance(content, dict) and content.get("passed") is True:
            preflights.append(preflight)
    return preflights


def passing_reference_metrics(artifact: dict) -> list[dict]:
    metrics = []
    for metric in artifact.get("metrics", []):
        content = metric.get("content") if isinstance(metric, dict) else None
        candidate = content.get("candidate") if isinstance(content, dict) else None
        reference = content.get("reference") if isinstance(content, dict) else None
        if (
            isinstance(content, dict)
            and content.get("passed") is True
            and isinstance(candidate, dict)
            and bool(candidate.get("path"))
            and bool(candidate.get("sha256"))
            and isinstance(reference, dict)
            and bool(reference.get("path"))
            and bool(reference.get("sha256"))
        ):
            metrics.append(metric)
    return metrics


def has_firebase_test_lab_result(files: list[dict]) -> bool:
    return any(str(item.get("path", "")).endswith("firebase-test-lab-result.json") for item in files)


def index_evidence(summaries: list[dict]) -> dict[tuple[str, str, str, str], list[dict]]:
    evidence: dict[tuple[str, str, str, str], list[dict]] = defaultdict(list)
    for summary in summaries:
        for artifact in summary.get("artifacts", []):
            for manifest in artifact.get("manifests", []):
                content = manifest.get("content") if isinstance(manifest, dict) else None
                if not isinstance(content, dict):
                    continue
                if content.get("schema") != "imm-ci-artifact-manifest-v1":
                    continue
                classification = content.get("classification", {})
                if not isinstance(classification, dict):
                    continue
                matrix = content.get("matrix", {})
                if not isinstance(matrix, dict):
                    continue
                key = (
                    str(matrix.get("product", "")),
                    str(matrix.get("platform", "")),
                    str(matrix.get("mode", "")),
                    str(matrix.get("renderer", "")),
                )
                evidence[key].append(
                    {
                        "summary": summary.get("_summary_path", ""),
                        "artifact": artifact.get("path", ""),
                        "manifest": manifest.get("file", ""),
                        "result": str(classification.get("result") or "unknown"),
                        "failure_class": str(classification.get("failure_class") or ""),
                        "metrics": artifact.get("metrics", []),
                        "passing_reference_metrics": passing_reference_metrics(artifact),
                        "reports": artifact.get("reports", []),
                        "captures": artifact.get("captures", []),
                        "contracts": artifact.get("contracts", []),
                        "passing_contracts": passing_contracts(artifact),
                        "preflights": artifact.get("preflights", []),
                        "passing_preflights": passing_preflights(artifact),
                        "files": content.get("files", []),
                    }
                )
    return evidence


def selected_rows(rows: list[dict], scope: str, gate_prefix: str) -> list[dict]:
    selected = []
    for row in rows:
        if row.get("status") != "supported":
            continue
        if scope == "hardware" and not row.get("hardware_gate"):
            continue
        if scope == "hosted" and not row.get("hosted_gate"):
            continue
        if gate_prefix:
            gates = [str(row.get("hosted_gate") or ""), str(row.get("hardware_gate") or "")]
            if not any(gate.startswith(gate_prefix) for gate in gates):
                continue
        selected.append(row)
    return selected


def evaluate_row(row: dict, evidence: list[dict]) -> dict:
    errors: list[str] = []
    if not evidence:
        errors.append("missing manifest evidence")
    nonpassing = [item for item in evidence if item["result"] != "passed"]
    passing = [item for item in evidence if item["result"] == "passed"]
    if evidence and not passing:
        classifications = sorted(
            {
                f"{item['result']} ({item['failure_class']})"
                if item["failure_class"]
                else item["result"]
                for item in nonpassing
            }
        )
        errors.append(f"validation manifest reports {', '.join(classifications)}")

    has_preflight = any(item["passing_preflights"] for item in evidence)
    has_firebase_result = any(has_firebase_test_lab_result(item.get("files", [])) for item in evidence)
    if row.get("hardware_gate") and not has_preflight and not has_firebase_result:
        errors.append("missing passing runner preflight evidence")

    if is_visual_row(row):
        if not any(item["passing_reference_metrics"] for item in evidence):
            errors.append("missing passing candidate-to-reference visual metrics evidence")
        if not any(item["reports"] for item in evidence):
            errors.append("missing human-readable render report evidence")
        if not any(item["captures"] for item in evidence):
            errors.append("missing capture image/frame evidence")
        if not any(
            any(str(capture.get("path", "")).lower().endswith((".png", ".jpg", ".jpeg", ".webp")) for capture in item["captures"])
            for item in evidence
        ):
            errors.append("missing viewable PNG/JPEG/WebP capture evidence")

    if is_vr_row(row) and not any(item["passing_contracts"] for item in evidence):
        errors.append("missing passing VR/OpenXR contract evidence")

    return {
        "key": display_key(row_key(row)),
        "hosted_gate": row.get("hosted_gate"),
        "hardware_gate": row.get("hardware_gate"),
        "requires_visual_evidence": is_visual_row(row),
        "requires_vr_contract": is_vr_row(row),
        "evidence_count": len(evidence),
        "evidence": evidence,
        "passed": not errors,
        "errors": errors,
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


def write_reports(results: dict, json_output: Path, markdown_output: Path) -> None:
    json_output.parent.mkdir(parents=True, exist_ok=True)
    markdown_output.parent.mkdir(parents=True, exist_ok=True)
    json_output.write_text(json.dumps(results, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")

    lines = [
        "# IMM Matrix Evidence Report",
        "",
        f"- Scope: {results['scope']}",
        f"- Gate prefix: {results['gate_prefix'] or 'all'}",
        f"- Required rows: {results['required_row_count']}",
        f"- Rows with evidence: {results['rows_with_evidence']}",
        f"- Result: {'passed' if results['passed'] else 'failed'}",
        "",
    ]
    table = [["Row", "Evidence", "Visual", "VR Contract", "Result"]]
    for row in results["rows"]:
        table.append(
            [
                row["key"],
                str(row["evidence_count"]),
                "yes" if row["requires_visual_evidence"] else "no",
                "yes" if row["requires_vr_contract"] else "no",
                "passed" if row["passed"] else "; ".join(row["errors"]),
            ]
        )
    lines.extend(markdown_table(table))
    lines.append("")

    for row in results["rows"]:
        if not row["evidence"]:
            continue
        lines.append(f"## {row['key']}")
        evidence_table = [["Summary", "Artifact", "Manifest", "Preflights", "Metrics", "Reports", "Contracts", "Captures"]]
        for item in row["evidence"]:
            failure_suffix = f"/{item['failure_class']}" if item["failure_class"] else ""
            manifest_label = f"{item['manifest']} [{item['result']}{failure_suffix}]"
            evidence_table.append(
                [
                    str(item["summary"]),
                    str(item["artifact"]),
                    manifest_label,
                    str(len(item["passing_preflights"])),
                    str(len(item["metrics"])),
                    str(len(item["reports"])),
                    str(len(item["passing_contracts"])),
                    str(len(item["captures"])),
                ]
            )
        lines.extend(markdown_table(evidence_table))
        lines.append("")

    markdown_output.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix_status", type=Path)
    parser.add_argument("--scope", choices=sorted(VALID_SCOPES), default="hardware")
    parser.add_argument("--gate-prefix", default="", help="Only require rows whose hosted or hardware gate starts with this value")
    parser.add_argument("--summary", type=Path, action="append", default=[])
    parser.add_argument("--evidence-root", type=Path, action="append", default=[])
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    args = parser.parse_args()

    matrix = json.loads(args.matrix_status.read_text(encoding="utf-8"))
    rows = matrix.get("rows", [])
    if not isinstance(rows, list):
        rows = []
    summaries = discover_summaries(args.summary, args.evidence_root)
    evidence = index_evidence(summaries)

    results_rows = []
    for row in selected_rows(rows, args.scope, args.gate_prefix):
        key = row_key(row)
        results_rows.append(evaluate_row(row, evidence.get(key, [])))

    passed = all(row["passed"] for row in results_rows)
    results = {
        "schema": "imm-matrix-evidence-report-v1",
        "scope": args.scope,
        "gate_prefix": args.gate_prefix,
        "summary_count": len(summaries),
        "required_row_count": len(results_rows),
        "rows_with_evidence": sum(1 for row in results_rows if row["evidence_count"] > 0),
        "passed": passed,
        "rows": results_rows,
    }
    write_reports(results, args.json_output, args.markdown_output)

    if not passed:
        for row in results_rows:
            for error in row["errors"]:
                print(f"{row['key']}: {error}")
        return 1
    print(f"Matrix evidence verified: {len(results_rows)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
