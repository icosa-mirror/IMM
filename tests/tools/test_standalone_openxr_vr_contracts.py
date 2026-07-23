#!/usr/bin/env python3
"""Verify standalone Windows OpenXR VR smoke contracts are wired."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: Path, tokens: list[str]) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [token for token in tokens if token not in text]


def main() -> int:
    checks = {
        ROOT / "code/appImmViewer/scripts/run-windows-openxr-vr-smoke.ps1": [
            "settings-openxr-probe.json",
            "IMM_OPENXR_STANDALONE createSessionResult=0 resultName=XR_SUCCESS",
            "IMM_OPENXR_STANDALONE enumerateStereoViewsFillResult=0",
            "IMM_OPENXR_STANDALONE waitFrameResult=0 resultName=XR_SUCCESS shouldRender=1",
            "IMM_OPENXR_STANDALONE endFrameResult=0 resultName=XR_SUCCESS layerCount=1 projectionViews=2",
            "OpenXR standalone startup probe passed",
        ],
        ROOT / ".github/workflows/ci-gpu.yml": [
            "windows-standalone-openxr-vr",
            "Preflight Windows OpenXR VR runner",
            "Run Windows OpenXR VR smoke",
            "Verify Windows OpenXR VR log contract",
            "openxr-vr-log-contract.json",
            "CI GPU Matrix /",
        ],
        ROOT / "tests/matrix_status.json": [
            '"product": "standalone"',
            '"platform": "windows"',
            '"mode": "vr"',
            '"renderer": "openxr"',
            '"hardware_gate": "CI GPU Matrix / Windows Standalone OpenXR VR"',
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

    print("Standalone OpenXR VR contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
