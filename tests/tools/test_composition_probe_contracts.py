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
            "target_share",
            "scene composition %s rear occlusion leakage probe failed",
            "unproject_position",
        ],
        ROOT / ".github/workflows/ci-engine.yml": [
            "unity-windows-directx-composition",
            "classify_unity_visual_smoke.py",
            "composition-status.json",
            "expected_failed",
            "Compare Unity DirectX render metrics against committed DirectX baseline",
            "unity-windows-directx-composition.png",
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
