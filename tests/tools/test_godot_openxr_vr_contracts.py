#!/usr/bin/env python3
"""Verify Godot OpenXR VR smoke contracts are wired."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: Path, tokens: list[str]) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [token for token in tokens if token not in text]


def main() -> int:
    checks = {
        ROOT / "code/ImmGodotSampleProject/scenes/OpenXRSmokeScene.tscn": [
            "XROrigin3D",
            "XRCamera3D",
            "openxr_smoke_controller.gd",
        ],
        ROOT / "code/ImmGodotSampleProject/scripts/openxr_smoke_controller.gd": [
            'XRServer.find_interface("OpenXR")',
            "get_viewport().use_xr = true",
            "XRServer.primary_interface = xr_interface",
            "IMM_GODOT_OPENXR_SMOKE viewer_initialized",
            "IMM_GODOT_OPENXR_SMOKE document_ready",
            "IMM_GODOT_OPENXR_SMOKE frame_submitted",
            "IMM Godot OpenXR VR smoke passed",
        ],
        ROOT / "code/projects/windows/run-godot-openxr-vr-smoke.ps1": [
            "--rendering-driver",
            "vulkan",
            "OpenXRSmokeScene.tscn",
            "IMM_GODOT_OPENXR_SMOKE interface_initialized",
            "IMM_GODOT_OPENXR_SMOKE frame_submitted",
            "IMM Godot OpenXR VR smoke passed",
        ],
        ROOT / ".github/workflows/ci-gpu.yml": [
            "windows-godot-openxr-vr",
            "Run Godot OpenXR VR smoke",
            "Verify Godot OpenXR VR log contract",
            "godot-openxr-vr-log-contract.json",
            "--product godot --platform-name windows --mode vr --renderer openxr",
        ],
        ROOT / "tests/matrix_status.json": [
            '"product": "godot"',
            '"platform": "windows"',
            '"mode": "vr"',
            '"renderer": "openxr"',
            '"hardware_gate": "CI GPU Matrix / Windows Godot OpenXR VR"',
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

    print("Godot OpenXR VR contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

