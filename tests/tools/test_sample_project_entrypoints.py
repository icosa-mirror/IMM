#!/usr/bin/env python3
"""Guard the user-facing Godot Run and Unity Play validation contracts."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def method_body(source: str, signature: str) -> str:
    start = source.index(signature)
    next_method = source.find("\n        public static void ", start + len(signature))
    return source[start:] if next_method < 0 else source[start:next_method]


def main() -> int:
    godot_project = (ROOT / "code/ImmGodotSampleProject/project.godot").read_text(encoding="utf-8")
    godot_scene = (ROOT / "code/ImmGodotSampleProject/scenes/SampleScene.tscn").read_text(encoding="utf-8")
    godot_controller = (ROOT / "code/ImmGodotSampleProject/scripts/sample_scene_controller.gd").read_text(encoding="utf-8")
    godot_helper = (ROOT / "code/projects/windows/run-godot-sample-play-smoke.ps1").read_text(encoding="utf-8")
    gpu_workflow = (ROOT / ".github/workflows/ci-gpu.yml").read_text(encoding="utf-8")

    # A res:// path must work in a fresh checkout before Godot has built its
    # editor UID cache. A uid:// main scene can leave Run Project idle forever.
    assert 'run/main_scene="res://scenes/SampleScene.tscn"' in godot_project
    assert 'run/main_scene="uid://' not in godot_project
    assert "load_on_ready = true" not in godot_scene
    warmup = godot_controller.index("for _frame in range(3):")
    load = godot_controller.index("viewer.load_document()", warmup)
    assert warmup < load
    assert 'viewer.get_bounding_box() if sequence_ready else {}' in godot_controller
    assert 'int(viewer.get_layer_count()) if sequence_ready else 0' in godot_controller
    for token in [
        "IMM_GODOT_SAMPLE_PLAY_SMOKE",
        "IMM_GODOT_SAMPLE_PLAY_CAPTURE",
        'SAMPLE_PLAY_SMOKE_PREFIX := "[IMM_GODOT_SAMPLE_PLAY_20260803]"',
        '"%s passed capture=%s layers=%d camera_ids=%s"',
        "get_viewport().get_texture().get_image()",
        "save_png(capture_path)",
    ]:
        assert token in godot_controller
    godot_args = godot_helper.split("$godotArgs = @(", 1)[1].split(")", 1)[0]
    assert "--scene" not in godot_args
    assert "--script" not in godot_args
    assert '"--resolution", "1280x720"' in godot_args
    assert '"IMM_ASSERT_"' in godot_helper
    assert '$timedOut = $false' in godot_helper
    assert 'Get-Content -LiteralPath $diagnosticPath' in godot_helper
    assert '".godot\\extension_list.cfg"' in godot_helper
    assert '--editor --headless --audio-driver Dummy --path $project --quit' in godot_helper
    assert 'continuing to Run Project' in godot_helper
    for token in [
        "Run Godot project Run-button smoke",
        "Record Godot project Run-button render metrics",
        "Verify Godot project Run-button log contract",
        "Enforce Godot project Run-button contract",
    ]:
        assert token in gpu_workflow
    assert "godot-windows-vulkan-sample-play.json" in gpu_workflow
    directx_metrics_step = gpu_workflow.split(
        "- name: Compare DirectX render metrics against committed DirectX baseline", 1
    )[1].split("- name:", 1)[0]
    assert "--reference tests\\baselines\\render\\windows-directx-sample1.ppm" in directx_metrics_step
    assert "godot-windows-vulkan-sample-play.png" not in directx_metrics_step
    godot_play_metrics_step = gpu_workflow.split(
        "- name: Record Godot project Run-button render metrics", 1
    )[1].split("- name:", 1)[0]
    assert "--reference tests\\baselines\\render\\godot-windows-vulkan-sample-play.png" in godot_play_metrics_step
    godot_play_runtime_step = gpu_workflow.split(
        "- name: Run Godot project Run-button smoke", 1
    )[1].split("- name:", 1)[0]
    assert "-TimeoutSeconds 180" in godot_play_runtime_step
    assert "const SAMPLE_PLAY_SMOKE_SETTLE_FRAMES := 3" in godot_controller

    vulkan_renderer = (
        ROOT / "code/libImmCore/src/libRender/vulkan/piVulkan_Renderer.cpp"
    ).read_text(encoding="utf-8")
    assert "const bool skipWait = state->ownsDedicatedQueue &&" in vulkan_renderer
    assert "return state && state->ownsDedicatedQueue && !state->hostRenderPassFrameActive" in vulkan_renderer
    shader_read_handoff = vulkan_renderer.index(
        "if (mState->externalFrameColorTexture &&\n"
        "        !mState->externalFramePreservesHostColor"
    )
    assert shader_read_handoff >= 0

    godot_native = (ROOT / "code/appImmGodot/src/main.cpp").read_text(encoding="utf-8")
    assert "if (bounds == nullptr || !iPlayer().IsSequenceReady(id))" in godot_native

    player_source = (ROOT / "code/libImmPlayer/src/player.cpp").read_text(encoding="utf-8")
    assert "info.numChildren = target->GetType() == Layer::Type::Group" in player_source

    godot_node = (
        ROOT / "code/appImmGodotGDExtension/src/imm_viewer_node.cpp"
    ).read_text(encoding="utf-8")
    assert 'std::getenv("IMM_GODOT_LOG_FILE")' in godot_node

    unity_automation = (ROOT / "code/ImmUnitySampleProject/Assets/Editor/BuildAutomation.cs").read_text(encoding="utf-8")
    engine_workflow = (ROOT / ".github/workflows/ci-engine.yml").read_text(encoding="utf-8")
    assert '"Assets/Scenes/SampleScene.unity"' in unity_automation
    combined = method_body(
        unity_automation,
        "public static void BuildMacOSMetalSmokePlayerAndRunEditorPlayModeSmoke()",
    )
    assert combined.index("BuildMacOSMetalSmokePlayer();") < combined.index("RunMacOSEditorPlayModeSmoke();")
    editor_play = method_body(unity_automation, "public static void RunMacOSEditorPlayModeSmoke()")
    assert 'RunEditorPlayModeSmoke("macOS", SmokeScenes[0]' in editor_play
    for token in [
        "BuildMacOSMetalSmokePlayerAndRunEditorPlayModeSmoke",
        "Record Unity project Play-button render metrics",
        "Verify Unity project Play-button log contract",
        "Enforce Unity project Play-button contract",
    ]:
        assert token in engine_workflow

    print("Sample project entrypoint contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
