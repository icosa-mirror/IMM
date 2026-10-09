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
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmUrpXrRuntimeSmoke.cs": [
            "-immUrpXrSmoke",
            "XRSettings.isDeviceActive",
            "XRDisplaySubsystem",
            "display.GetRenderPassCount() != 1",
            "pass.GetRenderParameterCount() != 2",
            "ImmRenderingDiagnostics.SubmissionCompleted += OnSubmission",
            "submission.NativeSceneEvents != 1",
            "!submittedFrames.Add(submission.FrameIndex)",
            "[IMM_URP_XR_SMOKE] PASS",
            "[IMM_URP_XR_SMOKE] FAIL",
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Editor/ImmUrpXrBuild.cs": [
            "BuildAndroidOpenXRQuestPlayer",
            "BuildWindowsOpenXRPlayer",
            "BuildWindowsDirectXAndOpenXRPlayers",
            "GraphicsDeviceType.Vulkan",
            "GraphicsDeviceType.Direct3D12",
            "OpenXRSettings.RenderMode.SinglePassInstanced",
            "ImmUrpSampleSetup.ScenePath",
            "useXr: true",
            "TrackedPoseDriver.TrackedPose.Center",
            "sample.SetViewingOrigin(origin.transform)",
            "origin.AddComponent<XrSceneBootstrap>()",
            "sample.gameObject.AddComponent<ImmUrpXrRuntimeSmoke>()",
        ],
        ROOT / "tests/tools/prepare_unity_ci_project.py": [
            "--preserve-xr",
            "prepare_project(source, output, preserve_xr=args.preserve_xr)",
            '"Assets/Editor/ImmUrpXrBuild.cs"',
        ],
        ROOT / ".github/workflows/ci-engine.yml": [
            "unity-windows-openxr-vr",
            "Preflight Unity OpenXR VR runner",
            "Run Unity OpenXR VR smoke",
            "player\\openxr\\ImmUnityOpenXR.exe",
            "-immUrpXrSmoke",
            "[IMM_URP_XR_SMOKE] PASS api=Direct3D12",
            "nativeSceneEventsPerFrame=1 displayPasses=1 views=2",
            "unity-openxr-vr-log-contract.json",
            "Build Unity Android OpenXR Quest Vulkan player",
            "ImmPlayer.Editor.ImmUrpXrBuild.BuildAndroidOpenXRQuestPlayer",
            "ImmPlayer.Editor.ImmUrpXrBuild.BuildWindowsDirectXAndOpenXRPlayers",
            "ImmUnityQuestVulkan.apk",
            "com.oculus.intent.category.VR",
            "android.hardware.vr.headtracking",
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
