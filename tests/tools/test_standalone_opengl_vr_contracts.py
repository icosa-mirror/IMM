#!/usr/bin/env python3
"""Verify standalone Windows OpenGL VR smoke contracts are wired."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: Path, tokens: list[str]) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [token for token in tokens if token not in text]


def main() -> int:
    checks = {
        ROOT / "code/appImmViewer/src/mymain.cpp": [
            "IMM_LEGACY_VR_SMOKE hmd_initialized",
            "IMM_LEGACY_VR_SMOKE frame_submitted",
            "mHMD->EndFrame()",
        ],
        ROOT / "code/appImmViewer/scripts/run-windows-opengl-vr-smoke.ps1": [
            "settings-opengl-vr.json",
            "Rendering Backened: OpenGL",
            "XR Runtime: Legacy",
            "Rendering in VR: yes",
            "IMM_LEGACY_VR_SMOKE hmd_initialized",
            "IMM_LEGACY_VR_SMOKE frame_submitted",
        ],
        ROOT / ".github/workflows/ci-gpu.yml": [
            "windows-standalone-opengl-vr",
            "Run Windows OpenGL VR smoke",
            "Verify Windows OpenGL VR log contract",
            "opengl-vr-log-contract.json",
            "--product standalone --platform-name windows --mode vr --renderer opengl",
        ],
        ROOT / "tests/matrix_status.json": [
            '"product": "standalone"',
            '"platform": "windows"',
            '"mode": "vr"',
            '"renderer": "opengl"',
            '"hardware_gate": "CI GPU Matrix / Windows Standalone OpenGL VR"',
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

    print("Standalone OpenGL VR contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

