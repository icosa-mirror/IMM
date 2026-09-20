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
    require_tokens(
        "code/appImmUnity/src/imm_authoring.h",
        ["ImmAuthoring_DrawingCreate", "uint64_t * drawingIdOut"],
    )
    require_tokens(
        "code/appImmViewer/src/viewer/liveEditValidation.cpp",
        [
            "player->QueueDrawingCreation(",
            'L"[IMM_LIVE_EDIT_CREATE] frame=%llu revision=%llu status=%d result=%d "',
            "drawingCountAfter=%d handleMatch=%d",
        ],
    )
    require_tokens(
        "code/libImmImporter/src/document/layerPaintStatic.h",
        [
            "std::vector<std::unique_ptr<DrawingStatic>> mDrawings",
            "bool RemoveLastDrawing(Drawing * expected) override",
        ],
    )
    require_tokens(
        "code/libImmCore/src/libRender/metal/piMetal_Renderer.mm",
        [
            "A host-owned external command buffer may have been created with unretained references.",
            "[mState->retainedBuffers addObject:buffer->buffer]",
            "[state->commandBuffer addCompletedHandler:",
        ],
    )
    require_tokens(
        "code/libImmCore/src/libRender/vulkan/piVulkan_Renderer.cpp",
        [
            "iEnqueueDeferredDestroy(mState",
            "state->batchRingStampCounter - e.ringStamp",
            "state->vkWaitForFences",
        ],
    )
    static_renderer = (
        ROOT
        / "code/libImmPlayer/src/layerRenderers/layerRendererPaint/static/layerRendererPaintStatic.cpp"
    ).read_text(encoding="utf-8")
    assert "mRetirementFrame + 1" in static_renderer
    for path in [
        "code/libImmPlayer/src/player.h",
        "code/appImmShared/src/imm_engine_bridge.h",
        "code/appImmViewer/src/viewer/viewer.cpp",
    ]:
        assert "maxFramesInFlight" not in (ROOT / path).read_text(encoding="utf-8"), path
    print("Standalone live-edit validation contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
