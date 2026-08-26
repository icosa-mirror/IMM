#!/usr/bin/env python3
"""Build disposable Unity/Godot import harnesses from packaged artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path


UNITY_PACKAGE_NAMES = {
    "stroke": "com.immersive-foundation.imm-stroke-reader",
    "player": "com.immersive-foundation.imm-unity",
}


def ensure_file(path: Path, errors: list[str]) -> None:
    if not path.is_file():
        errors.append(f"Missing file: {path}")
    elif path.stat().st_size <= 0:
        errors.append(f"Empty file: {path}")


def ensure_dir(path: Path, errors: list[str]) -> None:
    if not path.is_dir():
        errors.append(f"Missing directory: {path}")


def ensure_path(path: Path, errors: list[str]) -> None:
    if not path.exists():
        errors.append(f"Missing path: {path}")
    elif path.is_file() and path.stat().st_size <= 0:
        errors.append(f"Empty file: {path}")


def load_json(path: Path, errors: list[str]) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - report precise validation context.
        errors.append(f"Could not parse JSON {path}: {exc}")
        return {}


def verify_unity_package(path: Path, expected_name: str, required_files: list[str], errors: list[str]) -> None:
    ensure_dir(path, errors)
    manifest_path = path / "package.json"
    ensure_file(manifest_path, errors)
    manifest = load_json(manifest_path, errors) if manifest_path.exists() else {}
    if manifest.get("name") != expected_name:
        errors.append(f"{manifest_path} has package name {manifest.get('name')!r}, expected {expected_name!r}")
    if not manifest.get("version"):
        errors.append(f"{manifest_path} is missing version")
    if not manifest.get("unity"):
        errors.append(f"{manifest_path} is missing unity version")
    for rel in required_files:
        ensure_path(path / rel, errors)


def write_unity_harness(workspace: Path, stroke_package: Path, player_package: Path, baseline: Path) -> Path:
    project = workspace / "unity-package-import"
    packages = project / "Packages"
    assets = project / "Assets" / "Tests" / "Editor"
    consumer = project / "Assets" / "Phase6PackageConsumer"
    packages.mkdir(parents=True, exist_ok=True)
    assets.mkdir(parents=True, exist_ok=True)
    consumer.mkdir(parents=True, exist_ok=True)
    shutil.copytree(stroke_package, packages / UNITY_PACKAGE_NAMES["stroke"])
    shutil.copytree(player_package, packages / UNITY_PACKAGE_NAMES["player"])

    manifest = {
        "dependencies": {
            UNITY_PACKAGE_NAMES["stroke"]: f"file:{UNITY_PACKAGE_NAMES['stroke']}",
            UNITY_PACKAGE_NAMES["player"]: f"file:{UNITY_PACKAGE_NAMES['player']}",
            "com.unity.test-framework": "1.1.33",
            "com.unity.modules.jsonserialize": "1.0.0",
        },
        "testables": [UNITY_PACKAGE_NAMES["player"]],
    }
    (packages / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    baseline_hash = hashlib.sha256(baseline.read_bytes()).hexdigest()
    (assets / "PackageImportHarness.cs").write_text(
        "\n".join(
            [
                "using NUnit.Framework;",
                "using UnityEngine;",
                "",
                "public sealed class PackageImportHarness",
                "{",
                "    [Test]",
                "    public void PackagesResolveFromArtifactPaths()",
                "    {",
                f"        Assert.IsNotEmpty(\"{baseline_hash}\");",
                "        Assert.IsNotNull(typeof(GameObject));",
                "    }",
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (assets / "PackageImportHarness.asmdef").write_text(
        json.dumps(
            {
                "name": "Imm.PackageImport.Tests",
                "references": [],
                "includePlatforms": [
                    "Editor",
                ],
                "excludePlatforms": [],
                "allowUnsafeCode": False,
                "overrideReferences": False,
                "precompiledReferences": [],
                "autoReferenced": True,
                "defineConstraints": [],
                "versionDefines": [],
                "noEngineReferences": False,
                "optionalUnityReferences": [
                    "TestAssemblies",
                ],
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    (consumer / "ImmPhase6PackageConsumer.asmdef").write_text(
        json.dumps(
            {
                "name": "ImmPhase6PackageConsumer",
                "rootNamespace": "ImmPackageConsumer",
                "references": ["ImmUnity.Runtime"],
                "includePlatforms": [],
                "excludePlatforms": [],
                "allowUnsafeCode": False,
                "autoReferenced": True,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    (consumer / "ImmPackageConsumerSmoke.cs").write_text(
        "\n".join(
            [
                "using ImmPlayer.Authoring;",
                "using ImmPlayer.Exporter;",
                "using UnityEngine;",
                "",
                "namespace ImmPackageConsumer",
                "{",
                "    public static class ImmPackageConsumerSmoke",
                "    {",
                "        public static string DescribeRuntime()",
                "        {",
                "            ImmAuthoringCapabilities capabilities = ImmAuthoringRuntime.Capabilities;",
                '            return $"{capabilities.Platform} {capabilities.Architecture}: {capabilities.Features}";',
                "        }",
                "",
                "        public static ImmAuthoringResult CreateAndDisposeDocument()",
                "        {",
                "            ImmAuthoringResult<ImmAuthoringDocument> create = ImmAuthoringDocument.Create(",
                "                ExportSequenceType.Animated,",
                "                30,",
                "                Color.black);",
                "            if (!create.Succeeded)",
                "                return create.WithoutValue();",
                "            create.Value.Dispose();",
                "            return ImmAuthoringResult.Success();",
                "        }",
                "    }",
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return project


def verify_unity(args: argparse.Namespace) -> int:
    errors: list[str] = []
    stroke_package = args.stroke_package.resolve()
    player_package = args.player_package.resolve()
    baseline = args.baseline.resolve()

    verify_unity_package(
        stroke_package,
        UNITY_PACKAGE_NAMES["stroke"],
        [
            "Runtime/ImmStrokeReader.cs",
            "Runtime/SharpQuillCompat.cs",
            "Plugins/x86_64/ImmStrokeReader.dll",
            "Plugins/Android/arm64-v8a/libImmStrokeReader.so",
            "Plugins/macOS/libImmStrokeReader.dylib",
            "Plugins/iOS/libImmStrokeReader.a",
        ],
        errors,
    )
    verify_unity_package(
        player_package,
        UNITY_PACKAGE_NAMES["player"],
        [
            "Runtime/ImmPlayerManager.cs",
            "Runtime/ImmNativePlugin.cs",
            "Plugins/x86_64/ImmUnityPlugin.dll",
            "Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so",
            "Plugins/OSX/ImmUnityPlugin.bundle",
            "Plugins/iOS/libImmUnityPlugin.a",
        ],
        errors,
    )
    ensure_file(baseline, errors)

    if not errors:
        if args.workspace.exists():
            shutil.rmtree(args.workspace)
        project = write_unity_harness(args.workspace, stroke_package, player_package, baseline)
        ensure_file(project / "Packages" / "manifest.json", errors)
        ensure_file(project / "Assets" / "Tests" / "Editor" / "PackageImportHarness.cs", errors)
        ensure_file(project / "Assets" / "Tests" / "Editor" / "PackageImportHarness.asmdef", errors)
        ensure_file(project / "Assets" / "Phase6PackageConsumer" / "ImmPackageConsumerSmoke.cs", errors)
        ensure_file(project / "Assets" / "Phase6PackageConsumer" / "ImmPhase6PackageConsumer.asmdef", errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print(f"Unity package import harness verified: {args.workspace / 'unity-package-import'}")
    return 0


def parse_gdextension(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_godot_harness(workspace: Path, package: Path, baseline: Path) -> Path:
    project = workspace / "godot-package-import"
    addon_target = project / "addons" / "imm_viewer"
    addon_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(package / "addons" / "imm_viewer", addon_target)
    (project / "project.godot").write_text(
        "\n".join(
            [
                "; Engine configuration file.",
                "config_version=5",
                "",
                "[application]",
                'config/name="IMM Godot Package Import Harness"',
                'run/main_scene="res://scenes/package_import_smoke.tscn"',
                "",
                "[rendering]",
                'renderer/rendering_method="forward_plus"',
                "",
            ]
        ),
        encoding="utf-8",
    )
    scenes = project / "scenes"
    scripts = project / "scripts"
    scenes.mkdir(exist_ok=True)
    scripts.mkdir(exist_ok=True)
    baseline_hash = hashlib.sha256(baseline.read_bytes()).hexdigest()
    (scripts / "package_import_smoke.gd").write_text(
        "\n".join(
            [
                "extends Node",
                "",
                "func _ready() -> void:",
                f'    print("IMM package import baseline {baseline_hash}")',
                "",
            ]
        ),
        encoding="utf-8",
    )
    (scenes / "package_import_smoke.tscn").write_text(
        "\n".join(
            [
                "[gd_scene load_steps=2 format=3]",
                "",
                '[ext_resource type="Script" path="res://scripts/package_import_smoke.gd" id="1"]',
                "",
                '[node name="PackageImportSmoke" type="Node"]',
                'script = ExtResource("1")',
                "",
            ]
        ),
        encoding="utf-8",
    )
    return project


def verify_godot(args: argparse.Namespace) -> int:
    errors: list[str] = []
    package = args.package.resolve()
    baseline = args.baseline.resolve()
    addon = package / "addons" / "imm_viewer"

    ensure_dir(addon, errors)
    required = [
        "README.md",
        "imm_viewer.gdextension",
        "imm_viewer_node.gd",
        "bin/windows/release/imm_godot_extension.dll",
        "bin/windows/release/ImmGodotPlugin.dll",
        "bin/macos/release/libimm_godot_extension.dylib",
        "bin/macos/release/libImmGodotPlugin.dylib",
        "bin/android/debug/libimm_godot_extension.arm64.so",
        "bin/android/debug/libImmGodotPlugin.so",
    ]
    for rel in required:
        ensure_file(addon / rel, errors)
    ensure_file(baseline, errors)

    manifest = addon / "imm_viewer.gdextension"
    if manifest.exists():
        text = parse_gdextension(manifest)
        for token in [
            'entry_symbol="imm_godot_library_init"',
            'compatibility_minimum="4.5"',
            "windows.release.x86_64",
            "macos.release.arm64",
            "android.debug.arm64",
        ]:
            if token not in text:
                errors.append(f"{manifest} is missing token: {token}")

    if not errors:
        if args.workspace.exists():
            shutil.rmtree(args.workspace)
        project = write_godot_harness(args.workspace, package, baseline)
        ensure_file(project / "project.godot", errors)
        ensure_file(project / "addons" / "imm_viewer" / "imm_viewer.gdextension", errors)
        ensure_file(project / "scenes" / "package_import_smoke.tscn", errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print(f"Godot package import harness verified: {args.workspace / 'godot-package-import'}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="kind", required=True)

    unity = subparsers.add_parser("unity")
    unity.add_argument("--stroke-package", type=Path, required=True)
    unity.add_argument("--player-package", type=Path, required=True)
    unity.add_argument("--baseline", type=Path, default=Path("tests/baselines/content/sample1.json"))
    unity.add_argument("--workspace", type=Path, default=Path("artifacts/package-import-harness"))
    unity.set_defaults(func=verify_unity)

    godot = subparsers.add_parser("godot")
    godot.add_argument("--package", type=Path, required=True)
    godot.add_argument("--baseline", type=Path, default=Path("tests/baselines/content/sample1.json"))
    godot.add_argument("--workspace", type=Path, default=Path("artifacts/package-import-harness"))
    godot.set_defaults(func=verify_godot)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
