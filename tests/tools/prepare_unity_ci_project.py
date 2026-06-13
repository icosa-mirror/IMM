#!/usr/bin/env python3
"""Prepare a reduced Unity project for hosted non-VR CI smoke tests."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


MINIMAL_DEPENDENCIES = {
    "com.unity.ugui": "1.0.0",
    "com.unity.modules.animation": "1.0.0",
    "com.unity.modules.assetbundle": "1.0.0",
    "com.unity.modules.audio": "1.0.0",
    "com.unity.modules.director": "1.0.0",
    "com.unity.modules.imageconversion": "1.0.0",
    "com.unity.modules.imgui": "1.0.0",
    "com.unity.modules.jsonserialize": "1.0.0",
    "com.unity.modules.particlesystem": "1.0.0",
    "com.unity.modules.physics": "1.0.0",
    "com.unity.modules.physics2d": "1.0.0",
    "com.unity.modules.screencapture": "1.0.0",
    "com.unity.modules.ui": "1.0.0",
    "com.unity.modules.uielements": "1.0.0",
    "com.unity.modules.unitywebrequest": "1.0.0",
    "com.unity.modules.unitywebrequestassetbundle": "1.0.0",
    "com.unity.modules.unitywebrequestaudio": "1.0.0",
    "com.unity.modules.unitywebrequesttexture": "1.0.0",
    "com.unity.modules.unitywebrequestwww": "1.0.0",
    "com.unity.modules.xr": "1.0.0",
}


EXCLUDED_ASSET_PATHS = [
    "Assets/XR",
    "Assets/Scenes/SampleSceneVR.unity",
    "Assets/Scenes/SampleSceneVR.unity.meta",
    "Assets/Scripts/XrSceneBootstrap.cs",
    "Assets/Scripts/XrSceneBootstrap.cs.meta",
]


def copytree(src: Path, dst: Path) -> None:
    def ignore(_dir: str, names: list[str]) -> set[str]:
        return {
            name
            for name in names
            if name
            in {
                "Library",
                "Temp",
                "Obj",
                "Build",
                "Builds",
                "Logs",
                "UserSettings",
            }
        }

    shutil.copytree(src, dst, ignore=ignore)


def remove_path(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def write_minimal_manifest(project: Path) -> None:
    manifest = {
        "dependencies": dict(sorted(MINIMAL_DEPENDENCIES.items())),
    }
    packages = project / "Packages"
    packages.mkdir(parents=True, exist_ok=True)
    (packages / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    remove_path(packages / "packages-lock.json")


def prepare_project(source: Path, output: Path) -> None:
    if output.exists():
        shutil.rmtree(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    copytree(source, output)
    for rel in EXCLUDED_ASSET_PATHS:
        remove_path(output / rel)
    write_minimal_manifest(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("code/ImmUnitySampleProject"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()
    if not (source / "Assets" / "Scenes" / "SampleScene.unity").is_file():
        raise FileNotFoundError(f"Unity source project is missing SampleScene.unity: {source}")
    if not (source / "Packages" / "com.immersive-foundation.imm-unity" / "package.json").is_file():
        raise FileNotFoundError(f"Unity source project is missing embedded IMM package: {source}")

    prepare_project(source, output)
    print(f"Prepared Unity CI project: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
