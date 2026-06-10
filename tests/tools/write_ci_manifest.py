#!/usr/bin/env python3
"""Write a compact manifest for a CI matrix leg or local verification run."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_file(path: Path, root: Path) -> dict:
    stat = path.stat()
    return {
        "path": path.relative_to(root).as_posix(),
        "byte_size": stat.st_size,
        "sha256": sha256_file(path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--product", required=True)
    parser.add_argument("--platform-name", required=True)
    parser.add_argument("--mode", required=True)
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--status", default="passed")
    parser.add_argument("--include", action="append", default=[], help="File or directory to hash into the manifest")
    args = parser.parse_args()

    root = args.repo_root.resolve()
    files = []
    for item in args.include:
        path = Path(item).resolve()
        if path.is_file():
            files.append(collect_file(path, root))
        elif path.is_dir():
            for child in sorted(p for p in path.rglob("*") if p.is_file()):
                files.append(collect_file(child, root))

    manifest = {
        "schema": "imm-ci-artifact-manifest-v1",
        "product": args.product,
        "platform": args.platform_name,
        "mode": args.mode,
        "renderer": args.renderer,
        "status": args.status,
        "git": {
            "sha": os.environ.get("GITHUB_SHA", ""),
            "ref": os.environ.get("GITHUB_REF", ""),
            "workflow": os.environ.get("GITHUB_WORKFLOW", ""),
            "job": os.environ.get("GITHUB_JOB", ""),
            "run_id": os.environ.get("GITHUB_RUN_ID", ""),
        },
        "runner": {
            "os": os.environ.get("RUNNER_OS", platform.system()),
            "arch": os.environ.get("RUNNER_ARCH", platform.machine()),
            "name": os.environ.get("RUNNER_NAME", ""),
        },
        "files": files,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote CI manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
