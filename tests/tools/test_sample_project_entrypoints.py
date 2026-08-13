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
        "Classify Windows Godot Vulkan evidence",
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
    smoke_body = godot_controller.split("func _run_sample_play_smoke() -> void:", 1)[1].split(
        "func _sample_play_smoke_failed", 1
    )[0]
    first_seek = smoke_body.index("viewer.set_time(0, 0)")
    frame_zero_spawn = smoke_body.index("_jump_to_active_spawn_area()", first_seek)
    settle_loop = smoke_body.index("for _frame in range(SAMPLE_PLAY_SMOKE_SETTLE_FRAMES):")
    assert first_seek < frame_zero_spawn < settle_loop

    vulkan_renderer = (
        ROOT / "code/libImmCore/src/libRender/vulkan/piVulkan_Renderer.cpp"
    ).read_text(encoding="utf-8")
    assert "const bool skipWait = state->ownsDedicatedQueue &&" in vulkan_renderer
    assert "return state && state->externalDepthReverseZ && !state->hostRenderPassFrameActive" in vulkan_renderer
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
    unity_ios_postprocessor = (
        ROOT / "code/ImmUnitySampleProject/Assets/Editor/IOSBuildPostprocessor.cs"
    ).read_text(encoding="utf-8")
    engine_workflow = (ROOT / ".github/workflows/ci-engine.yml").read_text(encoding="utf-8")
    ios_workflow = (ROOT / ".github/workflows/ci-ios.yml").read_text(encoding="utf-8")
    validation_workflow = (ROOT / ".github/workflows/ci-validation.yml").read_text(encoding="utf-8")
    assert '"Assets/Scenes/SampleScene.unity"' in unity_automation
    combined_smoke = method_body(
        unity_automation,
        "public static void BuildMacOSMetalSmokePlayerAndRunEditorPlayModeSmoke()",
    )
    assert "BuildMacOSMetalSmokePlayer();" in combined_smoke
    assert "RunMacOSEditorPlayModeSmoke();" in combined_smoke
    ios_player_build = method_body(
        unity_automation,
        "public static void BuildIOSCIPlayer()",
    )
    for token in [
        "BuildTarget.iOS",
        "iOSSdkVersion.DeviceSDK",
        "GraphicsDeviceType.Metal",
        "SmokeScenes[0]",
        "iosXrSettings.InitManagerOnStart = false",
        "[IMM_UNITY_IOS_BUILD_20260812]",
    ]:
        assert token in ios_player_build
    ios_simulator_build = method_body(
        unity_automation,
        "public static void BuildIOSSimulatorCIPlayer()",
    )
    for token in [
        "BuildTarget.iOS",
        "iOSSdkVersion.SimulatorSDK",
        "GraphicsDeviceType.Metal",
        "SmokeScenes[0]",
        "iosXrSettings.InitManagerOnStart = false",
        "[IMM_UNITY_IOS_SIM_BUILD_20260813]",
    ]:
        assert token in ios_simulator_build
    editor_play = method_body(unity_automation, "public static void RunMacOSEditorPlayModeSmoke()")
    assert 'RunEditorPlayModeSmoke("macOS", SmokeScenes[0]' in editor_play
    assert "featureExamples.IsDocumentRenderReady" in unity_automation
    assert "TryPumpEditorPlayCamera()" in unity_automation
    assert "EditorSmokePumpInterval" in unity_automation
    assert "camera.targetTexture = target;" in unity_automation
    assert "camera.Render();" in unity_automation
    assert "capture.ReadPixels(" in unity_automation
    assert "camera.targetTexture = previousCameraTarget;" in unity_automation
    assert "[IMM_EDITOR_READY_CAPTURE_20260804]" in unity_automation
    for token in [
        "[PostProcessBuild(100)]",
        "GetUnityFrameworkTargetGuid()",
        '"usr/lib/libz.tbd"',
        "PBXSourceTree.Sdk",
        "AddFileToBuild(frameworkTarget, zlibGuid)",
        "[IMM_UNITY_IOS_LINK_20260812]",
    ]:
        assert token in unity_ios_postprocessor
    assert 'Environment.SetEnvironmentVariable(RuntimeSmokeDisabledEnv, "1");' in unity_automation
    unity_feature_examples = (
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmFeatureExamples.cs"
    ).read_text(encoding="utf-8")
    assert "public bool IsDocumentRenderReady =>" in unity_feature_examples
    assert "_doc.IsSequenceReady()" in unity_feature_examples
    assert "_spawnAreaIds.Length > 0" in unity_feature_examples
    assert "currentSpawnAreaIndex >= 0" in unity_feature_examples
    assert "_initialSpawnAreaCoroutine == null" in unity_feature_examples
    assert "_spawnAreaApplyCoroutine == null" in unity_feature_examples
    unity_runtime_smoke = (
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmUnityRuntimeSmoke.cs"
    ).read_text(encoding="utf-8")
    assert "IMM_UNITY_SMOKE_DISABLED" in unity_runtime_smoke
    assert "if (IsTruthyValue(Environment.GetEnvironmentVariable(DisabledEnv)))" in unity_runtime_smoke
    for token in [
        "Build Unity macOS Metal smoke player and run project Play-button smoke",
        "buildMethod: ImmPlayer.Editor.BuildAutomation.BuildMacOSMetalSmokePlayerAndRunEditorPlayModeSmoke",
        "manualExit: true",
        "Confirm Unity project Play-button smoke capture",
        "Record Unity project Play-button render metrics",
        "Write Unity project Play-button render report",
        "Classify Unity macOS Metal evidence",
        "Export Unity iOS Metal Xcode project",
        "buildMethod: ImmPlayer.Editor.BuildAutomation.BuildIOSCIPlayer",
        "Compile generated Unity iOS application without signing",
        "CODE_SIGNING_ALLOWED=NO",
        "visualEvidence=false",
    ]:
        assert token in engine_workflow
    for token in [
        "contains(github.event.head_commit.message, '[CI IOS]')",
        "Build iOS Simulator native libraries",
        "BuildIOSSimulatorCIPlayer",
        'build/unity-ios-player/simulator-xcode',
        'runner.temp }}/unity-ios-simulator-derived-data',
        "Run Unity iOS Metal visual smokes",
        "classify_unity_ios_metal.py",
        "UnityIOSMetalComposition",
    ]:
        assert token in ios_workflow
    assert "!contains(github.event.head_commit.message, '[CI IOS]')" in validation_workflow
    assert "artifacts/unity-ios-player-build/derived-data" not in engine_workflow
    assert "-project artifacts/unity-ios-player-build/xcode/" not in engine_workflow
    assert "-immIosPlayerPath ${{ github.workspace }}/artifacts/unity-ios-player-build/xcode" not in engine_workflow

    print("Sample project entrypoint contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
