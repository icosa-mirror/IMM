#!/usr/bin/env python3
"""Fast contract for the native multipass eye-matrix routing."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    player_path = ROOT / "code/libImmPlayer/src/player.cpp"
    player_source = player_path.read_text(encoding="utf-8")
    match = re.search(
        r"void Player::RenderStereoMultiPass\(.*?\n    void Player::RenderStereoSinglePass\(",
        player_source,
        re.DOTALL,
    )
    assert match is not None, "RenderStereoMultiPass implementation was not found"
    multipass_source = match.group(0)

    left_assignment = (
        "mDisplayRenderState.mEye[0].mViewerToEye_Prj = "
        "iConvertProjectionMatrix(projection) * d2f(head_to_eye);"
    )
    right_assignment = (
        "mDisplayRenderState.mEye[1].mViewerToEye_Prj = "
        "mDisplayRenderState.mEye[0].mViewerToEye_Prj;"
    )
    assert left_assignment in multipass_source, (
        "Multipass must write the current eye matrix to slot 0 for Vulkan picture shaders"
    )
    assert right_assignment in multipass_source, (
        "Multipass must mirror the current eye matrix to slot 1 for right-eye paint shaders"
    )
    assert "? eyeID : 0" not in multipass_source, (
        "The old non-GL slot-0-only routing loses Vulkan right-eye paint"
    )

    shader_paths = (
        ROOT / "code/libImmPlayer/src/layerRenderers/layerRendererPaint/static/shader_static_brush_vs.glsl",
        ROOT / "code/libImmPlayer/src/layerRenderers/layerRendererPicture/shader_pi2D_vs.glsl",
    )
    for shader_path in shader_paths:
        shader_source = shader_path.read_text(encoding="utf-8")
        assert re.search(
            r"#if\s+STEREOMODE==1\s+#define\s+iid\s+pass\.mID",
            shader_source,
        ), f"{shader_path.name} no longer selects the multipass eye slot from pass.mID"

    print("Stereo multipass matrix contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
