#!/usr/bin/env python3
"""Collect a summary of CI artifact directories and embedded manifests."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path


EMBEDDABLE_IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".webp"}
CAPTURE_SUFFIXES = EMBEDDABLE_IMAGE_SUFFIXES | {".ppm"}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_file(path: Path, root: Path) -> dict:
    return {
        "path": path.relative_to(root).as_posix(),
        "byte_size": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def load_json(path: Path) -> dict | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return None


def collect_artifact_dir(path: Path, root: Path) -> dict:
    files = sorted(p for p in path.rglob("*") if p.is_file())
    manifest_paths = [p for p in files if p.name == "manifest.json"]
    preflight_paths = [p for p in files if p.name == "preflight.json"]
    metrics_paths = [p for p in files if p.suffix.lower() == ".json" and "metric" in p.name.lower()]
    contract_paths = [p for p in files if p.suffix.lower() == ".json" and "contract" in p.name.lower()]
    capture_paths = [p for p in files if p.suffix.lower() in CAPTURE_SUFFIXES]
    report_paths = [p for p in files if p.suffix.lower() == ".md" and "report" in p.name.lower()]
    return {
        "path": path.relative_to(root).as_posix(),
        "file_count": len(files),
        "total_bytes": sum(p.stat().st_size for p in files),
        "manifests": [
            {
                "file": p.relative_to(root).as_posix(),
                "content": load_json(p),
            }
            for p in manifest_paths
        ],
        "preflights": [
            {
                "file": p.relative_to(root).as_posix(),
                "content": load_json(p),
            }
            for p in preflight_paths
        ],
        "metrics": [collect_file(p, root) for p in metrics_paths],
        "reports": [collect_file(p, root) for p in report_paths],
        "contracts": [
            {
                "file": p.relative_to(root).as_posix(),
                "content": load_json(p),
            }
            for p in contract_paths
        ],
        "captures": [collect_file(p, root) for p in capture_paths],
    }


def validate_manifest(manifest: object, path: str) -> list[str]:
    if manifest is None:
        return [f"Manifest is not valid JSON: {path}"]
    if not isinstance(manifest, dict):
        return [f"Manifest JSON is not an object: {path}"]
    errors = []
    if manifest.get("schema") != "imm-ci-artifact-manifest-v1":
        errors.append(f"Manifest has unexpected schema: {path}")
    for key in ["classification", "matrix", "git", "runner", "tool_versions", "fixtures", "files"]:
        if key not in manifest:
            errors.append(f"Manifest missing {key}: {path}")
    classification = manifest.get("classification", {})
    if isinstance(classification, dict) and not classification.get("result"):
        errors.append(f"Manifest missing classification.result: {path}")
    if isinstance(classification, dict) and classification.get("result") not in {"passed", "skipped", "expected_failed"}:
        errors.append(f"Manifest reports non-passing result {classification.get('result')!r}: {path}")
    matrix = manifest.get("matrix", {})
    if isinstance(matrix, dict):
        for key in ["product", "platform", "mode", "renderer"]:
            if not matrix.get(key):
                errors.append(f"Manifest missing matrix.{key}: {path}")
    return errors


def markdown_table(rows: list[list[str]]) -> list[str]:
    if not rows:
        return []
    widths = [max(len(row[index]) for row in rows) for index in range(len(rows[0]))]
    lines = []
    header = rows[0]
    lines.append("| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(header)) + " |")
    lines.append("| " + " | ".join("-" * widths[index] for index in range(len(header))) + " |")
    for row in rows[1:]:
        lines.append("| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(row)) + " |")
    return lines


def relative_link(target: str, report_dir: Path, root: Path) -> str:
    path = (root / target).resolve()
    try:
        return path.relative_to(report_dir.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def render_validation_report(summary: dict, report_path: Path, root: Path) -> str:
    report_dir = report_path.parent
    lines = [
        "# IMM Validation Report",
        "",
        f"- Workflow: {summary['git'].get('workflow', '') or 'local'}",
        f"- Job: {summary['git'].get('job', '') or 'local'}",
        f"- Git SHA: {summary['git'].get('sha', '') or 'unknown'}",
        f"- Result: {'passed' if summary.get('passed') else 'failed'}",
        "",
    ]
    errors = summary.get("errors", [])
    if errors:
        lines.append("## Errors")
        lines.extend(f"- {error}" for error in errors)
        lines.append("")

    artifacts = summary.get("artifacts", [])
    rows = [["Artifact", "Files", "Bytes", "Manifests", "Preflights", "Metrics", "Reports", "Contracts", "Captures"]]
    for artifact in artifacts:
        rows.append(
            [
                str(artifact.get("path", "")),
                str(artifact.get("file_count", 0)),
                str(artifact.get("total_bytes", 0)),
                str(len(artifact.get("manifests", []))),
                str(len(artifact.get("preflights", []))),
                str(len(artifact.get("metrics", []))),
                str(len(artifact.get("reports", []))),
                str(len(artifact.get("contracts", []))),
                str(len(artifact.get("captures", []))),
            ]
        )
    lines.append("## Artifact Summary")
    lines.extend(markdown_table(rows))
    lines.append("")

    for artifact in artifacts:
        artifact_path = artifact.get("path", "")
        lines.append(f"## {artifact_path}")
        manifest_rows = [["Manifest", "Result", "Failure Class", "Product", "Platform", "Mode", "Renderer"]]
        for manifest in artifact.get("manifests", []):
            content = manifest.get("content") if isinstance(manifest, dict) else None
            if not isinstance(content, dict):
                manifest_rows.append([str(manifest.get("file", "")), "invalid", "", "", "", "", ""])
                continue
            classification = content.get("classification", {}) if isinstance(content.get("classification"), dict) else {}
            matrix = content.get("matrix", {}) if isinstance(content.get("matrix"), dict) else {}
            manifest_rows.append(
                [
                    str(manifest.get("file", "")),
                    str(classification.get("result", "")),
                    str(classification.get("failure_class", "")),
                    str(matrix.get("product", "")),
                    str(matrix.get("platform", "")),
                    str(matrix.get("mode", "")),
                    str(matrix.get("renderer", "")),
                ]
            )
        if len(manifest_rows) > 1:
            lines.extend(markdown_table(manifest_rows))
            lines.append("")

        preflight_rows = [["Preflight", "Passed", "Errors"]]
        for preflight in artifact.get("preflights", []):
            content = preflight.get("content") if isinstance(preflight, dict) else None
            if isinstance(content, dict):
                preflight_rows.append([str(preflight.get("file", "")), str(content.get("passed", "")), str(len(content.get("errors", [])))])
            else:
                preflight_rows.append([str(preflight.get("file", "")), "invalid", ""])
        if len(preflight_rows) > 1:
            lines.extend(markdown_table(preflight_rows))
            lines.append("")

        contract_rows = [["Contract", "Passed", "Errors"]]
        for contract in artifact.get("contracts", []):
            content = contract.get("content") if isinstance(contract, dict) else None
            if isinstance(content, dict):
                contract_rows.append([str(contract.get("file", "")), str(content.get("passed", "")), str(len(content.get("errors", [])))])
            else:
                contract_rows.append([str(contract.get("file", "")), "invalid", ""])
        if len(contract_rows) > 1:
            lines.extend(markdown_table(contract_rows))
            lines.append("")

        if artifact.get("metrics"):
            lines.append("### Metrics")
            for metric in artifact["metrics"]:
                link = relative_link(metric["path"], report_dir, root)
                lines.append(f"- [{metric['path']}]({link})")
            lines.append("")

        if artifact.get("reports"):
            lines.append("### Reports")
            for report in artifact["reports"]:
                link = relative_link(report["path"], report_dir, root)
                lines.append(f"- [{report['path']}]({link})")
            lines.append("")

        if artifact.get("captures"):
            lines.append("### Captures")
            for capture in artifact["captures"]:
                link = relative_link(capture["path"], report_dir, root)
                suffix = Path(capture["path"]).suffix.lower()
                if suffix in EMBEDDABLE_IMAGE_SUFFIXES:
                    lines.append(f"![{capture['path']}]({link})")
                else:
                    lines.append(f"- [{capture['path']}]({link})")
            lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--artifact-dir", type=Path, action="append", default=[])
    parser.add_argument("--require-manifest", action="store_true")
    args = parser.parse_args()

    root = args.repo_root.resolve()
    artifacts = []
    errors: list[str] = []
    for item in args.artifact_dir:
        path = item.resolve()
        if not path.exists():
            errors.append(f"Artifact directory does not exist: {item}")
            continue
        if not path.is_dir():
            errors.append(f"Artifact path is not a directory: {item}")
            continue
        artifact = collect_artifact_dir(path, root)
        if args.require_manifest and not artifact["manifests"]:
            errors.append(f"Artifact directory has no manifest.json: {item}")
        if args.require_manifest:
            for manifest in artifact["manifests"]:
                errors.extend(validate_manifest(manifest["content"], manifest["file"]))
        artifacts.append(artifact)

    summary = {
        "schema": "imm-ci-artifact-summary-v1",
        "git": {
            "sha": os.environ.get("GITHUB_SHA", ""),
            "ref": os.environ.get("GITHUB_REF", ""),
            "workflow": os.environ.get("GITHUB_WORKFLOW", ""),
            "job": os.environ.get("GITHUB_JOB", ""),
            "run_id": os.environ.get("GITHUB_RUN_ID", ""),
        },
        "artifact_count": len(artifacts),
        "artifacts": artifacts,
        "passed": not errors,
        "errors": errors,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    report_path = args.output.with_name("validation-report.md")
    report_path.write_text(render_validation_report(summary, report_path, root), encoding="utf-8", newline="\n")

    if errors:
        for error in errors:
            print(error)
        return 1

    print(f"Artifact summary written: {args.output}")
    print(f"Validation report written: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
