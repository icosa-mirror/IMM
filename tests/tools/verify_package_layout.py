#!/usr/bin/env python3
"""Verify Unity and Godot package layout contracts used by CI artifacts."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


UNITY_PACKAGES = {
    "com.immersive-foundation.imm-stroke-reader": [
        "package.json",
        "README.md",
        "Runtime/ImmStrokeReader.cs",
        "Runtime/SharpQuillCompat.cs",
        "Plugins/x86_64/ImmStrokeReader.dll",
        "Plugins/Android/arm64-v8a/libImmStrokeReader.so",
        "Plugins/macOS/libImmStrokeReader.dylib",
        "Plugins/iOS/libImmStrokeReader.a",
    ],
    "com.immersive-foundation.imm-unity": [
        "package.json",
        "README.md",
        "Runtime/ImmPlayerManager.cs",
        "Runtime/ImmNativePlugin.cs",
        "Plugins/x86_64/ImmUnityPlugin.dll",
        "Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so",
        "Plugins/OSX/ImmUnityPlugin.bundle",
        "Plugins/iOS/libImmUnityPlugin.a",
    ],
}

GODOT_REQUIRED = [
    "README.md",
    "addons/imm_viewer/README.md",
    "addons/imm_viewer/imm_viewer.gdextension",
    "addons/imm_viewer/imm_viewer_node.gd",
    "addons/imm_viewer/bin/windows/release/imm_godot_extension.dll",
    "addons/imm_viewer/bin/windows/release/ImmGodotPlugin.dll",
    "addons/imm_viewer/bin/macos/release/libimm_godot_extension.dylib",
    "addons/imm_viewer/bin/macos/release/libImmGodotPlugin.dylib",
    "addons/imm_viewer/bin/android/debug/libimm_godot_extension.arm64.so",
    "addons/imm_viewer/bin/android/debug/libImmGodotPlugin.so",
]


def check_file(path: Path, errors: list[str]) -> None:
    if not path.exists():
        errors.append(f"Missing required path: {path}")
    elif path.is_file() and path.stat().st_size <= 0:
        errors.append(f"Required file is empty: {path}")


def verify_unity_package(path: Path, expected_name: str | None, errors: list[str]) -> None:
    manifest_path = path / "package.json"
    check_file(manifest_path, errors)
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        name = manifest.get("name")
        if expected_name and name != expected_name:
            errors.append(f"{manifest_path} has name {name!r}, expected {expected_name!r}")
        if not manifest.get("version"):
            errors.append(f"{manifest_path} is missing version")
        if not manifest.get("unity"):
            errors.append(f"{manifest_path} is missing unity version")

    required = UNITY_PACKAGES.get(expected_name or "", ["package.json", "README.md", "Runtime"])
    for rel in required:
        check_file(path / rel, errors)


def verify_godot_package(path: Path, errors: list[str]) -> None:
    for rel in GODOT_REQUIRED:
        check_file(path / rel, errors)

    manifest_path = path / "addons/imm_viewer/imm_viewer.gdextension"
    if manifest_path.exists():
        text = manifest_path.read_text(encoding="utf-8")
        for token in [
            'entry_symbol="imm_godot_library_init"',
            'compatibility_minimum="4.5"',
            "windows.release.x86_64",
            "macos.release.arm64",
            "android.debug.arm64",
        ]:
            if token not in text:
                errors.append(f"{manifest_path} is missing token: {token}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="kind", required=True)

    unity = subparsers.add_parser("unity")
    unity.add_argument("path", type=Path)
    unity.add_argument("--expected-name", choices=sorted(UNITY_PACKAGES))

    godot = subparsers.add_parser("godot")
    godot.add_argument("path", type=Path)

    args = parser.parse_args()
    errors: list[str] = []
    if args.kind == "unity":
        verify_unity_package(args.path, args.expected_name, errors)
    elif args.kind == "godot":
        verify_godot_package(args.path, errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Package layout verified: {args.path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
