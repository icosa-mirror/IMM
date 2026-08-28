#!/usr/bin/env python3
"""Guard diagnostic capture coverage and final-report publication."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: str, tokens: list[str]) -> None:
    text = (ROOT / path).read_text(encoding="utf-8").replace("\\", "/")
    for token in tokens:
        assert token in text, f"{path}: missing {token}"


def main() -> int:
    require_tokens(
        ".github/workflows/ci-gpu.yml",
        [
            "windows-standalone-directx/face-orientation-status.json",
            "windows-standalone-vulkan/face-orientation-status.json",
            "windows-standalone-opengl/face-orientation-status.json",
            "godot-smoke-windows-vulkan/face-orientation-status.json",
            "godot-smoke-macos-metal/face-orientation-status.json",
            "smoke_status=0",
        ],
    )
    require_tokens(
        ".github/workflows/ci-device.yml",
        [
            "android-standalone-gles/face-orientation-status.json",
            "android-standalone-vulkan/face-orientation-status.json",
            "android-godot-vulkan/face-orientation-status.json",
        ],
    )
    require_tokens(
        ".github/workflows/ci-engine.yml",
        [
            "unity-macos-metal-composition/face-orientation-status.json",
            "unity-windows-directx-composition/face-orientation-status.json",
            "unity-windows-vulkan-ordered-overlay/face-orientation-status.json",
            "unity-android-vulkan/face-orientation-status.json",
        ],
    )
    require_tokens(
        ".github/workflows/ci-ios.yml",
        [
            "Classify standalone iOS Metal face orientation\n        continue-on-error: true",
            "standalone-ios-metal/face-orientation-status.json",
            "unity-ios-player-build/face-orientation-status.json",
            "godot-ios-metal/face-orientation-status.json",
        ],
    )
    require_tokens(
        ".github/workflows/build.yml",
        [
            "face-orientation-captures/face-orientation-status.json",
            "validation/captures/face-orientation/metal-overlay.png",
        ],
    )
    require_tokens(
        "tests/tools/write_visual_evidence_report.py",
        [
            'find_json(root, "face-orientation-status.json")',
            'lines.append(f"- Face orientation:',
            'write_section_json(section_output_dir, "face-orientation-status.json"',
        ],
    )
    require_tokens(
        "code/ImmGodotSampleProject/export_presets.cfg",
        ["sample1.imm,face-orientation.imm", "--imm-godot-face-orientation-capture-path="],
    )
    player_source = (ROOT / "code/libImmPlayer/src/player.cpp").read_text(encoding="utf-8")
    assert player_source.count("mRenderer->SetState(piSTATE_CULL_FACE, true);") >= 6
    require_tokens(
        "code/appImmUnity/src/main.cpp",
        [
            "config.overrideFrontIsCCW = true;",
            "config.frontIsCCW = false;",
            "frontIsCCW=0",
        ],
    )
    require_tokens(
        "code/appImmGodot/src/main.cpp",
        [
            "config.rendererApi == piRenderer::API::Metal",
            "config.overrideFrontIsCCW = true;",
            "config.frontIsCCW = false;",
        ],
    )
    print("Face-orientation CI contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
