#!/usr/bin/env python3
"""Verify every shipped native product selects and links a real platform audio backend."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def require(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def preprocessor_branch(source: str, directive: str) -> str:
    match = re.search(
        rf"{re.escape(directive)}(?P<body>.*?)(?=\n\s*#(?:elif|else|endif))",
        source,
        flags=re.DOTALL,
    )
    return match.group("body") if match else ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.repo_root.resolve()
    errors: list[str] = []

    bridge = (root / "code/appImmShared/src/imm_engine_bridge.cpp").read_text(encoding="utf-8")
    selection_match = re.search(r"bool ImmEngineBridge::InitializeSound\(\).*?(?=mSoundBackend =)", bridge, re.DOTALL)
    selection = selection_match.group(0) if selection_match else ""
    require(errors, bool(selection), "Shared Unity/Godot audio backend selection block is missing")
    android_branch = preprocessor_branch(selection, "#if defined(__ANDROID__) || defined(ANDROID)")
    windows_branch = preprocessor_branch(selection, "#elif defined(WINDOWS)")
    apple_branch = preprocessor_branch(selection, "#elif defined(__APPLE__)")
    fallback_branch = preprocessor_branch(selection, "#else")

    require(errors, "API::Android" in android_branch and 'L"Android"' in android_branch,
            "Shared Unity/Godot bridge must select the Android audio backend on Android")
    require(errors, "API::DirectSoundOVR" in windows_branch and 'L"Audio360"' in windows_branch,
            "Shared Unity/Godot bridge must select Audio360 on Windows")
    require(errors, "API::AVFoundation" in apple_branch and 'L"AVFoundation"' in apple_branch,
            "Shared Unity/Godot bridge must select AVFoundation on Apple platforms")
    require(errors, "API::Null" not in apple_branch,
            "Shared Unity/Godot Apple branch must not select the null audio backend")
    require(errors, "API::Null" in fallback_branch,
            "Shared Unity/Godot bridge must retain a null backend for unsupported platforms")
    require(errors, "[IMM_AUDIO_PIPELINE_20260812]" in bridge and "requested=%ls active=%ls" in bridge,
            "Shared Unity/Godot bridge must report the active audio backend for runtime validation")

    macos_cmake = (root / "code/projects/macos/CMakeLists.txt").read_text(encoding="utf-8")
    ios_cmake = (root / "code/projects/ios/CMakeLists.txt").read_text(encoding="utf-8")
    android_core = (root / "code/libImmCore/CMakeLists.txt").read_text(encoding="utf-8")
    windows_core = (root / "code/libImmCore/libImmCore.vcxproj").read_text(encoding="utf-8")
    require(errors, "piSoundEngineAVFoundation.mm" in macos_cmake and "-framework AVFoundation" in macos_cmake,
            "macOS targets must compile and link the AVFoundation backend")
    require(errors, "piSoundEngineAVFoundation.mm" in ios_cmake and '"-framework AVFoundation"' in ios_cmake,
            "iOS Unity must compile and link the AVFoundation backend")
    require(errors, "piSoundEngineAndroid.cpp" in android_core and "find_library(opensles-lib OpenSLES)" in android_core,
            "Android targets must compile the Android backend and link OpenSL ES")
    require(errors, "piSoundEngineAudioSDKBackend.cpp" in windows_core,
            "Windows targets must compile the Audio360 backend")

    windows_viewer = (root / "code/appImmViewer/src/mymain.cpp").read_text(encoding="utf-8")
    android_viewer = (root / "code/appImmViewer/src/android/cpp/NonVrApp.cpp").read_text(encoding="utf-8")
    macos_viewer = (root / "code/appImmViewer/src/macos/metal_player.mm").read_text(encoding="utf-8")
    require(errors, "API::DirectSoundOVR" in windows_viewer, "Windows standalone must select Audio360")
    require(errors, "API::Android" in android_viewer, "Android standalone must select the Android backend")
    require(errors, "API::AVFoundation" in macos_viewer, "macOS standalone must select AVFoundation")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("Audio pipeline contract verified: 10 shipped product/platform combinations use real backends")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
