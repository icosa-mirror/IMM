#!/usr/bin/env python3
"""Focused checks for downloaded release asset audit validation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
EXPECTED = [
    ("ImmViewer-Windows", "standalone", "windows", "packaged"),
    ("ImmViewer-Android", "standalone", "android", "packaged"),
    ("ImmViewer-macOS", "standalone", "macos", "packaged"),
    ("ImmPlayerPlugin-Godot", "godot", "all", "package"),
    ("upm/com.immersive-foundation.imm-stroke-reader", "unity", "all", "package"),
    ("upm/com.immersive-foundation.imm-unity", "unity", "all", "package"),
]


def run_verify(path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(REPO_ROOT / "tests/tools/verify_release_assets.py"), str(path)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def manifest(product: str, platform: str, mode: str) -> dict:
    return {
        "schema": "imm-ci-artifact-manifest-v1",
        "classification": {"result": "passed", "failure_class": ""},
        "matrix": {
            "product": product,
            "platform": platform,
            "mode": mode,
            "renderer": "native",
        },
        "files": [{"path": "payload.bin", "byte_size": 1, "sha256": "0" * 64}],
        "fixtures": [],
    }


def summary(rel: str) -> dict:
    return {
        "schema": "imm-ci-artifact-summary-v1",
        "passed": True,
        "artifacts": [
            {
                "path": rel,
                "manifests": [{"file": f"{rel}/manifest.json", "content": {}}],
            }
        ],
    }


def write_release_tree(root: Path) -> None:
    for rel, product, platform, mode in EXPECTED:
        artifact = root / rel
        artifact.mkdir(parents=True)
        (artifact / "payload.bin").write_bytes(b"x")
        (artifact / "manifest.json").write_text(json.dumps(manifest(product, platform, mode)) + "\n", encoding="utf-8")
        (artifact / "artifact-summary.json").write_text(json.dumps(summary(rel)) + "\n", encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        write_release_tree(root)
        ok = run_verify(root)
        assert ok.returncode == 0, ok.stderr

        broken_summary = root / "ImmViewer-Windows" / "artifact-summary.json"
        data = json.loads(broken_summary.read_text(encoding="utf-8"))
        data["passed"] = False
        broken_summary.write_text(json.dumps(data) + "\n", encoding="utf-8")
        failed = run_verify(root)
        assert failed.returncode != 0, "release verifier should fail a failed artifact summary"
        assert "did not pass artifact summary validation" in failed.stderr

    print("Release asset verifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
