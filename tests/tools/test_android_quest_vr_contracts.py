#!/usr/bin/env python3
"""Verify Android Quest VR app smoke contracts are wired."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: Path, tokens: list[str]) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [token for token in tokens if token not in text]


def main() -> int:
    checks = {
        ROOT / "code/appImmViewer/src/android/cpp/OvrApp.cpp": [
            "IMM_ANDROID_VR_SMOKE viewer_initialized",
            "IMM_ANDROID_VR_SMOKE loading_frame_submitted",
            "IMM_ANDROID_VR_SMOKE document_frame_submitted",
            "vrapi_SubmitFrame2(appState.Ovr, &frameDesc)",
        ],
        ROOT / "code/projects/android/run-android-quest-vr-smoke.ps1": [
            "-PimmNonVr=OFF",
            "-PimmManifest=vr",
            "IMM_ANDROID_VR_SMOKE viewer_initialized",
            "Loaded in CPU",
            "Loaded in GPU",
            "IMM_ANDROID_VR_SMOKE document_frame_submitted",
        ],
        ROOT / ".github/workflows/ci-device.yml": [
            "Run Quest VR app smoke",
            "run-android-quest-vr-smoke.ps1",
            "IMM_ANDROID_VR_SMOKE document_frame_submitted",
            "--renderer gles",
        ],
        ROOT / "tests/matrix_status.json": [
            '"product": "standalone"',
            '"platform": "android"',
            '"mode": "vr"',
            '"renderer": "gles"',
            '"hardware_gate": "CI Device Matrix / Android Quest VR"',
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

    print("Android Quest VR contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
