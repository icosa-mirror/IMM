#!/usr/bin/env python3
"""Guard the Phase 4 standalone iOS product and validation surface."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    cmake = (ROOT / "code/projects/ios/CMakeLists.txt").read_text(encoding="utf-8")
    shell = (ROOT / "code/appImmViewer/src/ios/main.mm").read_text(encoding="utf-8")
    core_header = (ROOT / "code/appImmViewer/src/apple/metal_player_core.h").read_text(encoding="utf-8")
    core = (ROOT / "code/appImmViewer/src/apple/metal_player_core.cpp").read_text(encoding="utf-8")
    plist = (ROOT / "code/projects/ios/appImmViewerIOS-Info.plist.in").read_text(encoding="utf-8")
    workflow = (ROOT / ".github/workflows/ci-ios.yml").read_text(encoding="utf-8")

    for token in [
        "add_executable(appImmViewerIOS MACOSX_BUNDLE",
        "appImmViewer/src/ios/main.mm",
        "appImmViewer/src/apple/metal_player_core.cpp",
        "appImmViewerIOS-settings.json",
        "exampleImmFiles/sample1.imm",
        '"-framework UIKit"',
        '"-framework MetalKit"',
        '"-framework AVFoundation"',
        '"-framework CoreGraphics"',
        'XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"',
        'XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"',
    ]:
        assert token in cmake, token

    assert "UIKit" not in core_header and "AppKit" not in core_header
    assert "UIKit" not in core and "AppKit" not in core
    for token in ["BeginNativeFrame", "CopyNativeDrawableToTexture", "GlobalWork", "GlobalRender", "RenderMono", "WriteCapture", "API::AVFoundation"]:
        assert token in core, token

    for token in [
        "MTKViewDelegate",
        "UIDocumentPickerDelegate",
        "AVAudioSessionInterruptionNotification",
        "UIApplicationWillResignActiveNotification",
        "UIApplicationDidBecomeActiveNotification",
        "UIPanGestureRecognizer",
        "UIPinchGestureRecognizer",
        "presentedCopy",
        "IMM_IOS_STANDALONE phase=lifecycle status=passed",
    ]:
        assert token in shell, token

    assert "<string>15.0</string>" in plist
    assert "UIInterfaceOrientationLandscapeLeft" in plist
    assert "com.immersivefoundation.imm" in plist

    for token in [
        "standalone-ios-metal:",
        "Compile standalone iOS application without signing",
        "Run standalone iOS Metal visual and lifecycle validation",
        "standalone-ios-metal-render.png",
        "ios-standalone-metal-sample1.json",
        "--product standalone",
        "--platform-name ios",
        "--renderer metal",
    ]:
        assert token in workflow, token

    print("Standalone iOS Phase 4 contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
