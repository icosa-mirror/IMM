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
    godot_export_presets = (ROOT / "code/ImmGodotSampleProject/export_presets.cfg").read_text(encoding="utf-8")
    godot_extension_manifest = (
        ROOT / "code/ImmGodotSampleProject/addons/imm_viewer/imm_viewer.gdextension"
    ).read_text(encoding="utf-8")
    ios_cmake = (ROOT / "code/projects/ios/CMakeLists.txt").read_text(encoding="utf-8")
    gpu_workflow = (ROOT / ".github/workflows/ci-gpu.yml").read_text(encoding="utf-8")
    build_workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
    godot_windows_build = (
        ROOT / "code/projects/windows/build-godot-extension.ps1"
    ).read_text(encoding="utf-8")
    godot_android_build = (
        ROOT / "code/projects/android/build-godot-extension-android.ps1"
    ).read_text(encoding="utf-8")
    godot_xr_extension_manifest = (
        ROOT / "code/ImmGodotXRSampleProject/addons/imm_viewer/imm_viewer.gdextension"
    ).read_text(encoding="utf-8")
    godot_xr_scene = (
        ROOT / "code/ImmGodotXRSampleProject/scenes/XRSampleScene.tscn"
    ).read_text(encoding="utf-8")
    godot_xr_controller = (
        ROOT / "code/ImmGodotXRSampleProject/scripts/xr_sample_controller.gd"
    ).read_text(encoding="utf-8")
    godot_viewer_node = (
        ROOT / "code/appImmGodotGDExtension/src/imm_viewer_node.cpp"
    ).read_text(encoding="utf-8")

    # Both sample projects must be runnable from a clean checkout. The desktop
    # addon is canonical, while local and CI builds mirror every XR-capable
    # platform into the XR sample.
    godot_windows_dlls = [
        "imm_godot_extension.dll",
        "ImmGodotPlugin.dll",
        "Audio360.dll",
        "opus.dll",
        "opusenc.dll",
        "vorbisenc.dll",
        "zlib1.dll",
        "jpeg62.dll",
        "libpng16.dll",
        "ogg.dll",
        "vorbis.dll",
    ]
    for variant in ["debug", "release"]:
        desktop_bin = ROOT / "code/ImmGodotSampleProject/addons/imm_viewer/bin/windows" / variant
        xr_bin = ROOT / "code/ImmGodotXRSampleProject/addons/imm_viewer/bin/windows" / variant
        for dll_name in godot_windows_dlls:
            desktop_dll = desktop_bin / dll_name
            xr_dll = xr_bin / dll_name
            assert desktop_dll.is_file(), f"Missing committed desktop Godot DLL: {desktop_dll}"
            assert xr_dll.is_file(), f"Missing committed XR Godot DLL: {xr_dll}"
            desktop_bytes = desktop_dll.read_bytes()
            xr_bytes = xr_dll.read_bytes()
            assert desktop_bytes[:2] == b"MZ", f"Invalid PE header: {desktop_dll}"
            assert xr_bytes == desktop_bytes, f"XR Godot DLL is stale or mismatched: {xr_dll}"
    assert 'code\\ImmGodotXRSampleProject\\addons\\imm_viewer\\bin\\windows\\$variant' in godot_windows_build
    godot_android_libraries = [
        "libimm_godot_extension.arm64.so",
        "libImmGodotPlugin.so",
    ]
    desktop_android_bin = ROOT / "code/ImmGodotSampleProject/addons/imm_viewer/bin/android/debug"
    xr_android_bin = ROOT / "code/ImmGodotXRSampleProject/addons/imm_viewer/bin/android/debug"
    for library_name in godot_android_libraries:
        desktop_library = desktop_android_bin / library_name
        xr_library = xr_android_bin / library_name
        assert desktop_library.is_file(), f"Missing committed desktop Godot Android library: {desktop_library}"
        assert xr_library.is_file(), f"Missing committed XR Godot Android library: {xr_library}"
        desktop_bytes = desktop_library.read_bytes()
        xr_bytes = xr_library.read_bytes()
        assert desktop_bytes[:4] == b"\x7fELF", f"Invalid ELF header: {desktop_library}"
        assert int.from_bytes(desktop_bytes[18:20], "little") == 183, f"Expected ARM64 ELF: {desktop_library}"
        assert xr_bytes == desktop_bytes, f"XR Godot Android library is stale or mismatched: {xr_library}"
    for token in [
        'android.debug.arm64="res://addons/imm_viewer/bin/android/debug/libimm_godot_extension.arm64.so"',
        'android.release.arm64="res://addons/imm_viewer/bin/android/release/libimm_godot_extension.arm64.so"',
        'android.debug.arm64={"res://addons/imm_viewer/bin/android/debug/libImmGodotPlugin.so":""}',
        'android.release.arm64={"res://addons/imm_viewer/bin/android/release/libImmGodotPlugin.so":""}',
    ]:
        assert token in godot_xr_extension_manifest
    assert "macos." not in godot_xr_extension_manifest
    assert 'code\\ImmGodotXRSampleProject\\addons\\imm_viewer\\bin\\android\\$variant' in godot_android_build
    assert '"GodotCppLib=$relativeGodotCppLib"' in godot_android_build
    assert "for platform in windows android; do" in build_workflow
    assert 'bin/$platform/." "code/ImmGodotXRSampleProject/addons/imm_viewer/bin/$platform/' in build_workflow
    assert (
        "code/ImmGodotXRSampleProject/addons/imm_viewer/bin/"
    ) in build_workflow.split("git add", 1)[1]
    assert 'rm -rf "${plat}debug"' in build_workflow
    assert 'cp -R "${plat}release" "${plat}debug"' in build_workflow
    for token in [
        '[node name="ImmViewer" type="ImmViewerNode" parent="."]',
        '[sub_resource type="ImmViewerCompositorEffect"',
        'compositor = SubResource("Compositor_xr")',
        'renderer_api = 5',
        'render_camera_path = NodePath("../XROrigin3D/XRCamera3D")',
        'document_transform = Transform3D(-1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)',
    ]:
        assert token in godot_xr_scene
    assert 'get_node_or_null("ImmViewer")' in godot_xr_controller
    assert "_bind_authored_imm_resources()" in godot_xr_controller
    assert 'ClassDB.instantiate("ImmViewerNode")' not in godot_xr_controller
    assert 'ClassDB.instantiate("ImmViewerCompositorEffect")' not in godot_xr_controller
    assert 'Variant::TRANSFORM3D, "document_transform"' in godot_viewer_node

    # A res:// path must work in a fresh checkout before Godot has built its
    # editor UID cache. A uid:// main scene can leave Run Project idle forever.
    assert 'run/main_scene="res://scenes/SampleScene.tscn"' in godot_project
    assert 'run/main_scene="uid://' not in godot_project
    assert 'config/icon="res://icon.svg"' in godot_project
    godot_uids = [path.read_text(encoding="utf-8").strip() for path in (ROOT / "code/ImmGodotSampleProject").rglob("*.uid")]
    assert len(godot_uids) == len(set(godot_uids)), "Godot project resource UIDs must be unique"
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
        '"%s passed os=%s capture=%s layers=%d camera_ids=%s"',
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
    for token in [
        'name="iOS Development"',
        'platform="iOS"',
        'include_filter="sample1.imm"',
        'application/bundle_identifier="org.linuxfoundation.imm.godot.sample"',
        'application/export_project_only=true',
        'application/min_ios_version="15.0"',
    ]:
        assert token in godot_export_presets
    for token in [
        'ios.debug="res://addons/imm_viewer/bin/ios/debug/libimm_godot_extension.ios.template_debug.xcframework"',
        'ios.release="res://addons/imm_viewer/bin/ios/release/libimm_godot_extension.ios.template_release.xcframework"',
    ]:
        assert token in godot_extension_manifest
    for token in [
        "IMM_GODOT_CPP_ROOT",
        "IMM_GODOT_CPP_LIBRARY",
        "add_library(ImmGodotIOSGDExtension STATIC",
        "src/imm_viewer_metal_frame.mm",
        "xcrun libtool -static",
        "libimm_godot_extension.a",
        "[IMM_GODOT_IOS_PACKAGE_20260813]",
    ]:
        assert token in ios_cmake
    assert 'OS.get_name() in ["Android", "iOS"]' in godot_controller
    assert "_prepare_embedded_sample_document()" in godot_controller
    assert "OS.get_cmdline_user_args()" in godot_controller
    assert '_get_runtime_option("IMM_GODOT_SAMPLE_PLAY_SMOKE", "")' in godot_controller
    assert '_get_runtime_option("IMM_GODOT_SAMPLE_PLAY_CAPTURE", "")' in godot_controller

    godot_visual_controller = (
        ROOT / "code/ImmGodotSampleProject/scripts/visual_smoke_controller.gd"
    ).read_text(encoding="utf-8")
    assert "OS.get_cmdline_user_args()" in godot_visual_controller
    assert "_runtime_arguments().has(\"--imm-godot-visual-smoke\")" in godot_visual_controller
    assert "[IMM_GODOT_VISUAL_RESULT_20260813]" in godot_visual_controller
    assert 'IMM_GODOT_VISUAL_SMOKE_RESULT_LOG' in godot_visual_controller

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
    assert "[IMM_UNITY_RUNTIME_SMOKE_INSTALL_20260813]" in unity_runtime_smoke
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
        "run_validation:",
        "if: inputs.run_validation == true",
        "Build iOS Simulator native libraries",
        "BuildIOSSimulatorCIPlayer",
        'build/unity-ios-player/simulator-xcode',
        'runner.temp }}/unity-ios-simulator-derived-data',
        "Run Unity iOS Metal visual smokes",
        "classify_unity_ios_metal.py",
        "UnityIOSMetalComposition",
        "simctl list devices available",
        "select_ios_simulator.py",
        "SIMCTL_CHILD_IMM_UNITY_SMOKE_CAPTURE_PATH",
        "SIMCTL_CHILD_IMM_UNITY_EXPECT_GRAPHICS_API=Metal",
        "Godot iOS Package and Metal Validation",
        "Restore Godot iOS native build cache",
        "Save Godot iOS native build cache",
        "Build Godot iOS static GDExtension",
        "libimm_godot_extension.ios.template_debug.xcframework",
        'platform=ios target=template_debug arch=arm64 ios_simulator=no',
        'GODOT_VERSION: 4.7.1-stable',
        'GODOT_CPP_REF: godot-4.5-stable',
        "Build Godot arm64 iOS Simulator engine with Metal",
        "!TARGET_OS_SIMULATOR",
        "apple4_startup_guard=physical-only",
        "image_cube_array_reporting=unchanged",
        "default_cube_array_fallbacks=omitted-on-simulator",
        "timed_present=ordinary-present-on-simulator",
        "vsync=disabled-for-simulator-present-compatibility",
        "--imm-godot-visual-renderer-api=4",
        'local expected_result_name="$2"',
        "find \"$container\" -type f -name 'godot-ios-*' -delete",
        "for attempt in 1 2; do",
        '${label}-early-exit-attempt-${attempt}-system.log',
        'cp "$expected_result" "$artifact_dir/$expected_result_name"',
        "godot-ios-metal-sample1.json",
        "godot-ios-metal-full-depth.json",
        "godot-ios-metal-ordered-overlay.json",
        "godot-ios-metal-sample-play.json",
        "Compile exported Godot iOS Simulator application",
        "Run Godot iOS Metal visual validation in Simulator",
        "[CI IOS GODOT SIMULATOR]",
        "contains(github.event.head_commit.message, '[CI IOS GODOT SIMULATOR]')",
        "[CI IOS GODOT DEVICE]",
        "contains(github.event.head_commit.message, '[CI IOS GODOT DEVICE]')",
        "[CI IOS GODOT COMPAT]",
        "contains(github.event.head_commit.message, '[CI IOS GODOT COMPAT]')",
        '--export-debug "iOS Development"',
        "Compile exported Godot iOS application without signing",
        "Package Godot iOS application for Firebase Test Lab",
        "Run ordinary Godot iOS sample on Firebase Test Lab",
        "gcloud firebase test ios run",
        "--type game-loop",
        "firebase-game-loop",
        'value != "iphone-ipad-minimum-performance-a12"',
        'if "arm64" not in info["UIRequiredDeviceCapabilities"]',
        "firebase-test-matrix.json",
        "embedded-mobileprovision.txt",
        "mkdir -p artifacts/godot-ios-metal/ftl-results",
        "FIREBASE_TEST_LAB_IOS_GODOT_DEVICE",
        "model=iphonese3,version=18.4,locale=en,orientation=landscape",
        "firebase-ios-models.json",
        "runtime=firebase-test-lab-ios-game-loop",
        "godot-ios-sample-play.png",
        "classify_godot_ios_metal.py",
        "Write Godot iOS Metal visual manifest",
        "CODE_SIGNING_ALLOWED=NO",
        "GodotIOSMetal",
    ]:
        assert token in ios_workflow
    assert ios_workflow.count("--imm-godot-visual-renderer-api=4") == 2
    assert "--imm-godot-visual-renderer-api=5" not in ios_workflow
    assert 'test "${{ steps.godot_ios_compatibility_simulator_compile.outcome }}" = success' not in ios_workflow
    for obsolete in [
        "The Simulator GPU lacks image cube arrays required by Godot Metal.",
        "Real-device Firebase validation owns the Metal result.",
        "Godot rejects the Simulator GPU before scene startup.",
        "Only the stock Compatibility Simulator control is supported.",
        "The Simulator GPU reports no image-cube-array support.",
    ]:
        assert obsolete not in ios_workflow
    assert "simctl create" not in ios_workflow
    assert "!contains(github.event.head_commit.message, '[CI IOS]')" in validation_workflow
    for token in [
        "uses: ./.github/workflows/ci-ios.yml",
        "name: iOSStandaloneMetal",
        "path: artifacts/validation-evidence/input/iOSStandaloneMetal",
        "name: UnityIOSMetalComposition",
        "path: artifacts/validation-evidence/input/UnityIOSMetalComposition",
        "name: GodotIOSMetal",
        "path: artifacts/validation-evidence/input/GodotIOSMetal",
    ]:
        assert token in validation_workflow
    assert "artifacts/unity-ios-player-build/derived-data" not in engine_workflow
    assert "-project artifacts/unity-ios-player-build/xcode/" not in engine_workflow
    assert "-immIosPlayerPath ${{ github.workspace }}/artifacts/unity-ios-player-build/xcode" not in engine_workflow

    print("Sample project entrypoint contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
