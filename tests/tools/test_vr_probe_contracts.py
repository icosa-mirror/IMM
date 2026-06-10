#!/usr/bin/env python3
"""Verify VR matrix rows have concrete XR probe contracts."""

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
            "IMM_UNITY_SMOKE_XR_PROBE",
            "XRSettings.isDeviceActive",
            "XRDisplaySubsystem",
            "xr probe passed",
            "xr probe failed",
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Editor/BuildAutomation.cs": [
            "RunWindowsOpenXREditorPlayModeSmoke",
            "SampleSceneVR.unity",
            "Windows OpenXR VR",
            "IMM_UNITY_SMOKE_XR_PROBE",
        ],
        ROOT / ".github/workflows/ci-engine.yml": [
            "unity-windows-openxr-vr",
            "Preflight Unity OpenXR VR runner",
            "Run Unity OpenXR VR smoke",
            "[IMM_UNITY_SMOKE] xr probe passed",
            "unity-openxr-vr-log-contract.json",
            "Engine Evidence Report",
        ],
        ROOT / "tests/matrix_status.json": [
            '"product": "unity"',
            '"platform": "windows"',
            '"mode": "vr"',
            '"renderer": "openxr"',
            '"status": "supported"',
            '"hardware_gate": "CI Engine Matrix / Unity Windows OpenXR VR"',
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

    print("VR probe contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
