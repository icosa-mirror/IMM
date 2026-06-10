#!/usr/bin/env python3
"""Verify downloaded release artifacts carry CI audit manifests and summaries."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def load_json(path: Path, errors: list[str]) -> object | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        errors.append(f"{path} is not valid JSON: {exc}")
        return None


def require_file(path: Path, errors: list[str]) -> None:
    if not path.exists():
        errors.append(f"Missing required file: {path}")
    elif not path.is_file():
        errors.append(f"Required path is not a file: {path}")
    elif path.stat().st_size <= 0:
        errors.append(f"Required file is empty: {path}")


def verify_manifest(path: Path, expected_product: str, expected_platform: str, expected_mode: str, errors: list[str]) -> None:
    require_file(path, errors)
    if not path.exists():
        return
    manifest = load_json(path, errors)
    if not isinstance(manifest, dict):
        errors.append(f"{path} JSON root is not an object")
        return
    if manifest.get("schema") != "imm-ci-artifact-manifest-v1":
        errors.append(f"{path} has unexpected schema {manifest.get('schema')!r}")
    matrix = manifest.get("matrix")
    if not isinstance(matrix, dict):
        errors.append(f"{path} is missing matrix object")
        return
    expectations = {
        "product": expected_product,
        "platform": expected_platform,
        "mode": expected_mode,
    }
    for key, expected in expectations.items():
        if matrix.get(key) != expected:
            errors.append(f"{path} matrix.{key} is {matrix.get(key)!r}, expected {expected!r}")
    classification = manifest.get("classification")
    if not isinstance(classification, dict) or classification.get("result") != "passed":
        errors.append(f"{path} classification.result is not passed")
    if not isinstance(manifest.get("files"), list) or not manifest["files"]:
        errors.append(f"{path} does not list hashed files")
    if not isinstance(manifest.get("fixtures"), list):
        errors.append(f"{path} is missing fixtures list")


def verify_summary(path: Path, expected_manifest_rel: str, errors: list[str]) -> None:
    require_file(path, errors)
    if not path.exists():
        return
    summary = load_json(path, errors)
    if not isinstance(summary, dict):
        errors.append(f"{path} JSON root is not an object")
        return
    if summary.get("schema") != "imm-ci-artifact-summary-v1":
        errors.append(f"{path} has unexpected schema {summary.get('schema')!r}")
    if summary.get("passed") is not True:
        errors.append(f"{path} did not pass artifact summary validation")
    artifacts = summary.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        errors.append(f"{path} has no artifact entries")
        return
    manifest_files = []
    for artifact in artifacts:
        if not isinstance(artifact, dict):
            continue
        for manifest in artifact.get("manifests", []):
            if isinstance(manifest, dict):
                manifest_files.append(manifest.get("file"))
    if expected_manifest_rel not in manifest_files:
        errors.append(f"{path} does not summarize {expected_manifest_rel}")


def verify_artifact(root: Path, rel: str, product: str, platform: str, mode: str, errors: list[str]) -> None:
    artifact = root / rel
    if not artifact.exists() or not artifact.is_dir():
        errors.append(f"Missing release artifact directory: {artifact}")
        return
    verify_manifest(artifact / "manifest.json", product, platform, mode, errors)
    verify_summary(artifact / "artifact-summary.json", f"{rel}/manifest.json", errors)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release_assets", type=Path)
    args = parser.parse_args()

    root = args.release_assets
    errors: list[str] = []
    expected = [
        ("ImmViewer-Windows", "standalone", "windows", "packaged"),
        ("ImmViewer-Android", "standalone", "android", "packaged"),
        ("ImmViewer-macOS", "standalone", "macos", "packaged"),
        ("ImmPlayerPlugin-Godot", "godot", "all", "package"),
        ("upm/com.immersive-foundation.imm-stroke-reader", "unity", "all", "package"),
        ("upm/com.immersive-foundation.imm-unity", "unity", "all", "package"),
    ]
    for rel, product, platform, mode in expected:
        verify_artifact(root, rel, product, platform, mode, errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Release asset audit verified: {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
