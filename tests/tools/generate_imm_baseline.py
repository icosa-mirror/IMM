#!/usr/bin/env python3
"""Generate repository-stable baseline metadata for IMM fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_magic(path: Path, size: int = 16) -> str:
    with path.open("rb") as handle:
        return handle.read(size).hex()


def generate_baseline(path: Path, repo_root: Path) -> dict:
    stat = path.stat()
    rel_path = path.relative_to(repo_root).as_posix()
    return {
        "schema": "imm-content-baseline-v1",
        "fixture": {
            "path": rel_path,
            "file_name": path.name,
            "byte_size": stat.st_size,
            "sha256": sha256_file(path),
            "magic_hex": read_magic(path),
        },
        "expected_content": {
            "loadable_by_standalone": True,
            "loadable_by_unity_stroke_reader": True,
            "loadable_by_godot_viewer_node": True,
            "requires_non_empty_document": True,
            "requires_finite_bounds": True,
            "requires_layer_count_greater_than": 0,
            "requires_chapter_count_greater_than_or_equal": 1,
            "requires_spawn_area_query": True,
            "requires_audio_metadata_or_clean_absence": True,
        },
        "runtime_assertions": {
            "standalone": [
                "document load reaches CPU-ready state",
                "document load reaches GPU-ready state when renderer is available",
                "background color can be read",
                "playback controls do not crash",
            ],
            "unity": [
                "package imports",
                "native plugin loads for the target platform",
                "public wrapper API reports content values consistent with this baseline",
            ],
            "godot": [
                "addon imports",
                "ImmViewerNode loads the fixture",
                "public node API reports content values consistent with this baseline",
            ],
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    fixture = args.fixture.resolve()
    baseline = generate_baseline(fixture, repo_root)
    text = json.dumps(baseline, indent=2, sort_keys=True) + "\n"

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8", newline="\n")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
