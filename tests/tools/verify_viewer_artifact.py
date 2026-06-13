#!/usr/bin/env python3
"""Verify standalone viewer artifact layouts used by release packaging."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


WINDOWS_REQUIRED = [
    "appImmViewer_Release.exe",
    "sample1.imm",
    "settings.json",
    "settings-vulkan.json",
    "settings-opengl-vr.json",
    "README.md",
    "viewer_settings.md",
]


def check_path(path: Path, errors: list[str]) -> None:
    if not path.exists():
        errors.append(f"Missing required path: {path}")
    elif path.is_file() and path.stat().st_size <= 0:
        errors.append(f"Required file is empty: {path}")


def verify_windows(path: Path, errors: list[str]) -> None:
    for rel in WINDOWS_REQUIRED:
        check_path(path / rel, errors)


def verify_macos(path: Path, errors: list[str]) -> None:
    app = path / "appImmViewerMetal.app"
    check_path(app, errors)
    check_path(app / "Contents/Info.plist", errors)
    check_path(app / "Contents/MacOS/appImmViewerMetal", errors)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=["windows", "macos"])
    parser.add_argument("path", type=Path)
    args = parser.parse_args()

    errors: list[str] = []
    if args.platform == "windows":
        verify_windows(args.path, errors)
    else:
        verify_macos(args.path, errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"{args.platform} viewer artifact verified: {args.path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
