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
        [
            "ImmAuthoring_DrawingCreate",
            "ImmAuthoring_DrawingDestroy",
            "ImmAuthoring_FrameGetDrawingHandle",
            "ImmAuthoring_LayerGetProperties",
            "ImmAuthoring_LayerSetProperties",
            "ImmAuthoring_LayerCreate",
            "ImmAuthoring_LayerDestroy",
            "ImmAuthoring_LayerReparent",
            "ImmAuthoring_KeySet",
            "ImmAuthoring_KeyRemove",
            "ImmAuthoring_SetInitialSpawnArea",
            "ImmAuthoring_SpawnAreaSet",
            "ImmAuthoring_SpawnAreaGet",
            "ImmAuthoringSpawnArea",
            "ImmAuthoringKey",
            "IMM_AUTHORING_LAYER_PROPERTY_VISIBILITY",
            "IMM_AUTHORING_LAYER_PROPERTY_OPACITY",
            "IMM_AUTHORING_LAYER_PROPERTY_TRANSFORM",
            "uint64_t * drawingIdOut",
        ],
    )
    require_tokens(
        "code/appImmViewer/src/viewer/liveEditValidation.cpp",
        [
            "player->QueueDrawingCreation(",
            "player->QueueDrawingDeletion(",
            "player->QueueLayerProperties(",
            "player->QueueFrameMapping(",
            "geometryResult == 0 ? player->QueueFrameMapping(",
            "mappingResult=%d revision=%llu",
            'L"[IMM_LIVE_EDIT_CREATE] frame=%llu revision=%llu status=%d result=%d "',
            'L"[IMM_LIVE_EDIT_FRAME_SET] frame=%llu revision=%llu status=%d result=%d handleMatch=%d"',
            'L"[IMM_LIVE_EDIT_DELETE] frame=%llu revision=%llu status=%d result=%d "',
            'L"[IMM_LIVE_EDIT_PROPERTY] frame=%llu revision=%llu status=%d result=%d "',
            "drawingCountAfter=%d handleMatch=%d",
            "referencedDeletionRejected=%d",
            "canonicalOpacityChanged=%d",
            "canonicalTransformChanged=%d",
            'L"[IMM_LIVE_EDIT_KEY_SET] frame=%llu revision=%llu status=%d result=%d keyCountChanged=%d"',
            'L"[IMM_LIVE_EDIT_KEY_REMOVE] frame=%llu revision=%llu status=%d result=%d keyCountRestored=%d"',
            'L"[IMM_LIVE_EDIT_INITIAL_SPAWN] frame=%llu revision=%llu status=%d result=%d initialLayerMatch=%d"',
            'L"[IMM_LIVE_EDIT_SPAWN_AREA] frame=%llu revision=%llu status=%d result=%d spawnAreaMatch=%d"',
            'L"[IMM_LIVE_EDIT_LAYER_CREATE] frame=%llu revision=%llu status=%d result=%d layerMatch=%d countChanged=%d"',
            'L"[IMM_LIVE_EDIT_LAYER_CHILD_CREATE] frame=%llu revision=%llu status=%d result=%d childMatch=%d countChanged=%d"',
            'L"[IMM_LIVE_EDIT_LAYER_REPARENT] frame=%llu revision=%llu status=%d result=%d orderMatch=%d"',
            'L"[IMM_LIVE_EDIT_LAYER_CROSS_PARENT] frame=%llu revision=%llu status=%d result=%d hierarchyMatch=%d"',
            'L"[IMM_LIVE_EDIT_LAYER_DESTROY] frame=%llu revision=%llu status=%d result=%d subtreeMissing=%d countRestored=%d"',
        ],
    )
    require_tokens(
        "code/appImmViewer/scripts/validate_metal_standalone.sh",
        [
            r"\[IMM_LIVE_EDIT_FRAME_SET\].*revision=2 status=4 result=0 handleMatch=1",
            r"\[IMM_LIVE_EDIT_DELETE\].*revision=3 status=4 result=0",
            r"\[IMM_LIVE_EDIT_PROPERTY\].*revision=4 status=4 result=0",
            r"\[IMM_LIVE_EDIT_KEY_SET\].*revision=5 status=4 result=0 keyCountChanged=1",
            r"\[IMM_LIVE_EDIT_KEY_REMOVE\].*revision=6 status=4 result=0 keyCountRestored=1",
            r"\[IMM_LIVE_EDIT_INITIAL_SPAWN\].*revision=7 status=4 result=0 initialLayerMatch=1",
            r"\[IMM_LIVE_EDIT_SPAWN_AREA\].*revision=8 status=4 result=0 spawnAreaMatch=1",
            r"\[IMM_LIVE_EDIT_LAYER_CREATE\].*revision=9 status=4 result=0 layerMatch=1 countChanged=1",
            r"\[IMM_LIVE_EDIT_LAYER_CHILD_CREATE\].*revision=10 status=4 result=0 childMatch=1 countChanged=1",
            r"\[IMM_LIVE_EDIT_LAYER_REPARENT\].*revision=11 status=4 result=0 orderMatch=1",
            r"\[IMM_LIVE_EDIT_LAYER_CROSS_PARENT\].*revision=12 status=4 result=0 hierarchyMatch=1",
            r"\[IMM_LIVE_EDIT_LAYER_DESTROY\].*revision=13 status=4 result=0 subtreeMissing=1 countRestored=1",
        ],
    )
    require_tokens(
        "code/projects/android/appImmViewer/src/androidTest/java/org/linuxfoundation/imm/player/ImmFtlSmokeTest.java",
        [
            "waitForNativeCapture(device, nativeCapture, waitSeconds, true)",
            "waitForNativeCapture(device, nativeCapture, waitSeconds, false)",
            "!requireFullValidation",
        ],
    )
    require_tokens(
        "code/appImmUnity/src/imm_authoring.h",
        [
            "ImmAuthoring_FrameSetHandle",
            "ImmAuthoring_FrameGetDrawingHandle",
            "int32_t frameIndex, uint64_t drawingId",
            "int32_t frameIndex, int32_t drawingIndex",
        ],
    )
    require_tokens(
        "code/libImmImporter/src/document/layerPaintStatic.h",
        [
            "std::vector<std::unique_ptr<DrawingStatic>> mDrawings",
            "bool RemoveLastDrawing(Drawing * expected) override",
            "Drawing * ExtractDrawing(uint32_t drawingIndex) override",
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
