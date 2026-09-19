#!/usr/bin/env python3
"""Guard live-edit validation wiring in each standalone viewer backend."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: str, tokens: list[str]) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    for token in tokens:
        assert token in text, f"{path}: missing {token}"


def main() -> int:
    require_tokens(
        "code/projects/macos/CMakeLists.txt",
        [
            "appImmViewer/src/macos/metal_player.mm",
            "appImmViewer/src/viewer/liveEditValidation.cpp",
            "validateAppImmViewerMetalLiveEdit",
        ],
    )
    require_tokens(
        "code/appImmViewer/src/macos/metal_player.mm",
        [
            'appImmViewer/src/viewer/liveEditValidation.h',
            "ExePlayer::LiveEditValidation _liveEditValidation",
            "LiveEditValidation::RequestedFrameFromEnvironment()",
            "_liveEditValidation.Tick(_viewer, &_log, _frameIndex",
        ],
    )
    require_tokens(
        "code/appImmViewer/src/apple/metal_player_core.cpp",
        [
            "LiveEditValidation::RequestedFrameFromEnvironment()",
            "mLiveEditValidation.Tick(mViewer, &mLog, mFrameIndex",
        ],
    )
    require_tokens(
        "code/appImmViewer/src/android/cpp/NonVrApp.cpp",
        ["gEngine.liveEditValidation.Tick(*gEngine.viewer, gEngine.log, gEngine.frameCount"],
    )
    print("Standalone live-edit validation contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
