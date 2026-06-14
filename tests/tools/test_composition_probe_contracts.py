#!/usr/bin/env python3
"""Verify scene-composition visual smokes use region and leakage checks."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: Path, tokens: list[str]) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [token for token in tokens if token not in text]


def main() -> int:
    checks = {
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmUnityRuntimeSmoke.cs": [
            "AnalyzeProbeRegion",
            "rearOccluded",
            "MaxOccludedShare",
            "OverlayProbeEnv",
            "CaptureCameraTexture",
            "CaptureWidth",
            "IMM_UNITY_FORCE_TEXTURE_PROJECTION",
            "Resources.Load<Shader>(\"ImmUnitySmokeUnlitColor\")",
            "scene composition overlay rear probe failed",
            "scene composition overlay probe passed",
            "scene composition rear occlusion probe failed",
            "scene composition probe passed",
            "WorldToScreenPoint",
        ],
        ROOT / "code/ImmGodotSampleProject/scripts/visual_smoke_controller.gd": [
            "_analyze_scene_probe_region",
            "_project_probe_rect",
            "rear_occluded_probe",
            "MAX_SCENE_PROBE_OCCLUDED_SHARE",
            "COMPOSITION_MODE_ORDERED_OVERLAY",
            "IMM_GODOT_VISUAL_SMOKE_COMPOSITION_MODE",
            "EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT",
            "scene composition %s ordered overlay rear probe failed",
            "scene composition %s ordered overlay IMM background failed",
            "MIN_ORDERED_OVERLAY_IMM_PIXELS",
            "target_share",
            "scene composition %s rear occlusion leakage probe failed",
            "unproject_position",
        ],
        ROOT / "code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1": [
            "CompositionMode",
            "ordered_overlay",
            "composition_contract",
            "depth_composition",
            "not_claimed",
            "Skipping DirectX baseline PPM comparison for ordered_overlay composition mode",
        ],
        ROOT / ".github/workflows/ci-engine.yml": [
            "unity-windows-directx-composition",
            "unity-windows-vulkan-ordered-overlay",
            "classify_unity_visual_smoke.py",
            "--composition-mode full_depth",
            "--composition-mode ordered_overlay",
            "IMM_UNITY_SMOKE_OVERLAY_FIXTURE",
            "composition-status.json",
            "expected_failed",
            "Compare Unity DirectX render metrics against committed DirectX baseline",
            "unity-windows-directx-composition.png",
            "unity-windows-vulkan-ordered-overlay.png",
        ],
        ROOT / ".github/workflows/ci-gpu.yml": [
            "Run Godot Vulkan ordered overlay smoke",
            "-CompositionMode ordered_overlay",
            "godot-smoke-windows-vulkan-ordered-overlay",
            "godot-vulkan-ordered-overlay.ppm",
            "composition-status.json",
        ],
        ROOT / "tests/tools/composition_status.py": [
            "composition_mode",
            "composition_contract",
            "ordered_overlay",
            "depth_composition",
            "depth_interleaving",
            "render_only",
        ],
    }

    errors: list[str] = []
    for path, tokens in checks.items():
        missing = require_tokens(path, tokens)
        for token in missing:
            errors.append(f"{path.relative_to(ROOT)} missing token: {token}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("Composition probe contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
