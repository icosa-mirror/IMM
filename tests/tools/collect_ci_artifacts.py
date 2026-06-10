#!/usr/bin/env python3
"""Collect a summary of CI artifact directories and embedded manifests."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path


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
    matrix = manifest.get("matrix", {})
    if isinstance(matrix, dict):
        for key in ["product", "platform", "mode", "renderer"]:
            if not matrix.get(key):
                errors.append(f"Manifest missing matrix.{key}: {path}")
    return errors


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

    if errors:
        for error in errors:
            print(error)
        return 1

    print(f"Artifact summary written: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
