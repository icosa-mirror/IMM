#!/usr/bin/env python3
"""Verify scene-composition visual smokes use region and leakage checks."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require_tokens(path: Path, tokens: list[str]) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [token for token in tokens if token not in text]


def main() -> int:
    checks = {
        ROOT / "tests/tools/prepare_unity_ci_project.py": [
            '"com.unity.test-framework": "1.1.33"',
            '"com.unity.xr.management": "4.5.4"',
            "strip_android_xr_manifest(output)",
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmUnityRuntimeSmoke.cs": [
            "AnalyzeProbeRegion",
            "MinFrontProbeShare",
            "MinRearVisibleProbeShare",
            "0.0005f",
            "rearOccluded",
            "MaxOccludedShare",
            "OverlayProbeEnv",
            "CaptureCameraTexture",
            "CaptureOrderedCameraStackTexture(captureCamera)",
            "const int compositionProbeLayer = 29;",
            "cam.cullingMask = 1 << compositionProbeLayer;",
            "[IMM_ORDERED_OVERLAY_PROBE_LAYER_20260804]",
            "CaptureWidth",
            "captureCamera.targetTexture = renderTexture",
            "camera.targetTexture = renderTexture",
            "Resources.Load<Shader>(\"ImmUnitySmokeUnlitColor\")",
            "scene composition overlay rear probe failed",
            "scene composition ordered overlay orientation failed",
            "scene composition overlay probe passed",
            "scene composition rear occlusion probe failed",
            "scene composition probe passed",
            "normalizedRegion",
            "IMM_UNITY_ANDROID_VULKAN_CI",
            "unity-android-vulkan.png",
            'expected = "Vulkan";',
            "? CaptureScreenTexture()",
            "render source=unity-vulkan-presented-screen",
            "frameCount = 90;",
            "[IMM_UNITY_ANDROID_VK_SMOKE_FRAMES_20260729]",
            "scene composition probes created",
            "const float rearOccludedDistance = 10.0f;",
            "const float originalRearOccludedDistance = 3.95f;",
            "FindObjectOfType<ImmFeatureExamples>()",
            "FreezeCompositionPlaybackIfRequested();",
            "[IMM_UNITY_ANDROID_VK_EXTERNAL_RENDER_HOLD_20260813]",
            "WaitForSecondsRealtime(externalRenderHoldSeconds)",
        ],
        ROOT / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs": [
            'Graphics.Blit(source, destination, composite, 0);',
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Resources/ImmUnitySmokeUnlitColor.shader": [
            'Shader "IMM/SmokeUnlitColor"',
            '"RenderType" = "Opaque"',
            'Name "ShadowCaster"',
            '"LightMode" = "ShadowCaster"',
        ],
        ROOT / "code/ImmGodotSampleProject/scripts/visual_smoke_controller.gd": [
            "_analyze_scene_probe_region",
            "_project_probe_rect",
            "rear_occluded_probe",
            "MAX_SCENE_PROBE_OCCLUDED_SHARE",
            "COMPOSITION_MODE_ORDERED_OVERLAY",
            "IMM_GODOT_VISUAL_SMOKE_COMPOSITION_MODE",
            "EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT",
            "[IMM_GODOT_COMPOSITION_STAGE_20260804]",
            'set("access_resolved_color", true)',
            'set("access_resolved_depth", true)',
            "IMMSceneRearOccludedProbe\", SCENE_REAR_OCCLUDED_PROBE_COLOR, Vector3(0.75, 0.75, probe_depth), false",
            "REAR_OCCLUDED_PROBE_REGION_SCALE := 0.70",
            "region_scale: float = 1.0",
            "rect.grow_individual(-inset_x, -inset_y, -inset_x, -inset_y)",
            "[IMM_GODOT_ORDERED_OVERLAY_PROBE_SPLIT_20260804]",
            "scene composition %s %s IMM visibility failed",
            "last_depth_aware_vulkan_composite",
            "last_depth_aware_vulkan_composite_result",
            "full-depth Metal composition did not run the Godot render-graph depth composite path",
            "[IMM_GODOT_METAL_DEPTH_COMPOSITE_20260804]",
            "last_had_intermediate_texture",
            "last_had_intermediate_depth_texture",
            "last_had_depth_composited_texture",
            "last_depth_color_merge_result",
            "did not merge IMM depth with Godot host color in a separate render-graph target",
            "last_vulkan_depth_image_handle",
            "MIN_ORDERED_OVERLAY_IMM_PIXELS",
            "const DEFAULT_VISUAL_SMOKE_PLAYER_FRAME := 60",
            "platform load time cannot choose",
            'OS.set_environment("IMM_VIEWER_VALIDATE_FIXED_DT", VISUAL_SMOKE_FIXED_DT)',
            'OS.set_environment("IMM_VIEWER_VALIDATE_PLAYER_FRAME", str(player_frame))',
            "native validation clock did not reach player frame",
            "target_share",
            "scene composition %s rear occlusion leakage probe failed",
            "unproject_position",
            'return configured_value != "0"',
        ],
        ROOT / "tests/baselines/render/godot-ios-metal-sample-play.json": [
            '"min_correlation": 0.50',
            '"max_mean_abs_delta": 0.20',
            '"sample1-lower-red-brush-content"',
            '"minimum_matched_pixel_share_of_crop": 0.01',
        ],
        ROOT / "code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1": [
            "CompositionMode",
            "ordered_overlay",
            "composition_contract",
            "depth_composition",
            "not_claimed",
            'IMM_GODOT_RENDER_GRAPH_VULKAN_DEPTH_COMPOSITION = "1"',
            "Render-only baseline comparison is performed by the shared render-metrics contract.",
            "render fidelity is validated from the separate render-only capture",
        ],
        ROOT / ".github/workflows/ci-engine.yml": [
            "unity-macos-metal-composition",
            "unity-windows-directx-composition",
            "unity-windows-vulkan-ordered-overlay",
            "unity-windows-vulkan-full-depth",
            "classify_unity_visual_smoke.py",
            "--composition-mode full_depth",
            "--composition-mode ordered_overlay",
            "IMM_UNITY_SMOKE_OVERLAY_FIXTURE",
            "composition-status.json",
            "Compare Unity DirectX render metrics against committed DirectX baseline",
            "unity-windows-directx-composition.png",
            "unity-windows-vulkan-ordered-overlay.png",
            "unity-windows-vulkan-full-depth.png",
            "unity-macos-metal-full-depth.png",
            "unity-macos-metal-ordered-overlay.png",
            "sample1-full-depth.json",
            "sample1-ordered-overlay.json",
            "unity-macos-metal-ordered-overlay-diagnostic.png",
            "sealed-editor-play",
            "--allow-host-vulkan-rejection",
            "-immSmokeFrames 120",
            "-immSmokeExpectedGraphicsApi Metal",
            "-immSmokeMinOrderedOverlayImmUniqueColors 3000",
            "unity-android-vulkan:",
            "name: ImmPlayerPlugin-Unity",
            "model=dm3q,version=34",
            "Prepare clean non-XR Unity Android project",
            "Verify Firebase Adreno device target",
            "Build Unity Android Vulkan smoke player",
            "Restore cached Unity Android Vulkan shell APK",
            "steps.cache_unity_android_vulkan_shell.outputs.cache-primary-key",
            "Inject same-commit plugin into Unity Android Vulkan shell APK",
            "Verify Unity Android APK is non-XR",
            "repack_android_native_library.py",
            'work_dir="${RUNNER_TEMP}/unity-android-vulkan-repack"',
            'zipalign" -P 16',
            'apksigner" verify',
            "Run Unity Android Vulkan smoke in Firebase Test Lab",
            "--test-type robo",
            'required-marker "[IMM_UNITY_VK_VALIDATION_ARGS_20260731]"',
            'required-marker "[IMM_UNITY_VK_QUEUE_20260802] dedicatedQueueAllowed=1"',
            'required-marker "[IMM_UNITY_VK_QUEUE_20260802] mode=dedicated queueIndex=1"',
            'required-marker "Vulkan renderer initialized with external device"',
            'required-marker "[IMM_UNITY_VK_RT_SRC_20260612]"',
            'required-marker "source=offscreenRT"',
            'required-marker "Vulkan renderer began external image frame"',
            'required-marker "Unity Vulkan render:"',
            'required-marker "[IMM_UNITY_SMOKE] render source=unity-vulkan-presented-screen"',
            'required-marker "[IMM_UNITY_SMOKE] scene composition probe passed"',
            'required-marker "[IMM_UNITY_ANDROID_VK_COMPOSITION_RT_20260801]"',
            'required-marker "[IMM_UNITY_VK_ONRENDERIMAGE_20260802]"',
            'required-marker "shader=Unlit/Transparent supported=True"',
            'required-marker "[IMM_UNITY_ANDROID_VK_COMPOSITION_HOLD_20260731]"',
            'required-marker "[IMM_SYNTH_NATIVE_EYE_20260804] pair capture="',
            "--required-capture-name unity-android-vulkan.png",
            "--required-capture-name unity-android-vulkan-synthetic-stereo-native.png",
            "Record Unity Android Vulkan visual metrics",
            "--external-screen-capture-name unity-android-vulkan-robo-final.png",
            "validate_render_video.py",
            "sudo timeout 120 apt-get install --yes ffmpeg",
            "unity-android-vulkan-external-render-sample1.json",
            "unity-android-vulkan-external-render-video-validation.json",
            "unity-android-vulkan-external-render-metrics.json",
            "unity-android-vulkan-external-render.png",
            "--minimum-consecutive-frames 2",
            "--sample-interval-seconds 2",
            "unity-android-vulkan-composition-metrics.json",
            "unity-windows-directx-sample1.png",
            "unity-android-vulkan-sample1.json",
            "unity-android-vulkan-composition-sample1.json",
        ],
        ROOT / "tests/baselines/render/unity-android-vulkan-external-sample1.json": [
            "expected_color_components",
            "front-magenta",
            "rear-visible-yellow",
            "rear-occluded-cyan",
            "minimum_largest_component_share_of_crop",
            "maximum_largest_component_share_of_crop",
        ],
        ROOT / "tests/baselines/render/unity-android-vulkan-external-render-sample1.json": [
            "expected_spatial_luma_grid",
            '"min_correlation": 0.48',
            "absent-front-magenta",
            "absent-rear-yellow",
            "maximum_largest_component_share_of_crop",
        ],
        ROOT / "tests/baselines/render/unity-android-vulkan-composition-sample1.json": [
            "expected_spatial_luma_grid",
            '"min_correlation": 0.35',
            '"max_mean_abs_delta": 0.15',
            '"maximum_largest_component_share_of_crop": 0.0005',
            "expected_color_components",
            "front-magenta",
            "rear-visible-yellow",
            "rear-occluded-cyan",
            "minimum_largest_component_share_of_crop",
            "maximum_largest_component_share_of_crop",
            '"region_normalized": { "x": 0.50, "y": 0.46, "width": 0.04, "height": 0.09 }',
        ],
        ROOT / "tests/baselines/render/godot-android-vulkan-sample1.json": [
            "expected_spatial_luma_regions",
            "character-front-depth-order",
            '"max_mean_abs_delta": 0.18',
        ],
        ROOT / "tests/baselines/render/godot-android-vulkan-composition-sample1.json": [
            "expected_spatial_luma_regions",
            "character-front-depth-order",
            '"max_mean_abs_delta": 0.18',
        ],
        ROOT / "tests/baselines/render/sample1-composition-content.json": [
            "expected_color_components",
            "sample1-lower-red-brush-content",
            "minimum_matched_pixel_share_of_crop",
        ],
        ROOT / "tests/baselines/render/sample1-full-depth.json": [
            "expected_color_components",
            "sample1-lower-red-brush-content",
            "front-visible-magenta",
            "rear-visible-yellow",
            "character-occluded-cyan",
            "minimum_matched_pixel_share_of_crop",
            "maximum_largest_component_share_of_crop",
            '"region_normalized": { "x": 0.50, "y": 0.46, "width": 0.04, "height": 0.09 }',
        ],
        ROOT / "code/ImmGodotSampleProject/addons/imm_viewer/imm_viewer_node.gd": [
            "Godot's Vulkan clip space is zero-to-one",
            "z_far / depth",
            "(z_far * z_near) / depth",
        ],
        ROOT / "code/appImmGodotGDExtension/src/imm_viewer_compositor_effect.cpp": [
            "Godot 4.3+",
            "float host_depth_normal = 1.0 - host_depth;",
            "imm_depth > host_depth_normal",
            "IMM_GODOT_RENDER_GRAPH_DEPTH_COMPOSITION",
            "render_depth_texture_handle",
            "depth_composited_texture",
            "preserved_host_color",
            "frag_color = preserved_host_color",
            'fragment_shader_source += mirror_imm_x ? "#define MIRROR_IMM_X\\n" : "";',
            "vec2 imm_uv = vec2(1.0 - uv_interp.x, uv_interp.y)",
            "vec2 imm_uv = uv_interp;",
            "depth_composited_texture, color_texture, false",
            "The IMM 360 picture is valid far-plane content",
        ],
        ROOT / "code/appImmGodotGDExtension/src/imm_viewer_metal_frame.mm": [
            "depth_texture_handle",
            "pass_descriptor.depthAttachment.texture = depth_texture;",
            "pass_descriptor.depthAttachment.clearDepth = 1.0;",
        ],
        ROOT / "tests/baselines/render/sample1-ordered-overlay.json": [
            "expected_color_components",
            "sample1-lower-red-brush-content",
            "front-visible-magenta",
            "rear-visible-yellow",
            "character-occluded-cyan",
            "minimum_matched_pixel_share_of_crop",
            "maximum_largest_component_share_of_crop",
            '"region_normalized": { "x": 0.50, "y": 0.46, "width": 0.04, "height": 0.09 }',
        ],
        ROOT / "tests/tools/validate_render_video.py": [
            '"color_component_probes": result.get("color_component_probes")',
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Editor/BuildAutomation.cs": [
            "BuildMacOSMetalSmokePlayer",
            "GraphicsDeviceType.Metal",
            "BuildTarget.StandaloneOSX",
            "EditorSmokePlayerPathArg",
            "BuildAndroidVulkanSmokePlayer",
            "GraphicsDeviceType.Vulkan",
            "AndroidArchitecture.ARM64",
            'new[] { "IMM_UNITY_ANDROID_VULKAN_CI" }',
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Plugins/Android/AndroidManifest.xml": [
            "com.immersivefoundation.imm.ImmUnityPlayerActivity",
            'android:exported="true"',
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Plugins/Android/ImmUnityPlayerActivity.java": [
            "updateUnityCommandLineArguments",
            "ApplicationInfo.FLAG_DEBUGGABLE",
            'ValidationArgument = "-force-vulkan-layers"',
            "[IMM_UNITY_VK_VALIDATION_ARGS_20260731]",
        ],
        ROOT / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/Resources/ImmVulkanDepthComposite.shader": [
            "_CameraDepthTexture",
            "_ImmColorTex",
            "_ImmDepthTex",
            "SAMPLE_DEPTH_TEXTURE",
            "UNITY_REVERSED_Z",
            "unityIsNearer",
        ],
        ROOT / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs": [
            "#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN || UNITY_ANDROID",
            "SetVulkanCameraRenderBuffers",
            "return CameraEvent.AfterForwardOpaque;",
            "GetVulkanCommandBufferEvent(Camera cam)",
            "return cam != null && cam.stereoEnabled",
            "info.CommandBuffer.IssuePluginEvent(_renderEventFunc, eventId)",
            "info.CommandBuffer.SetRenderTarget(cameraTarget);",
            "PresentAndroidVulkanFrame",
            "UsesStereoAndroidVulkanPresenter",
            "cam.stereoActiveEye == Camera.MonoOrStereoscopicEye.Right ? 1 : 0",
            "[IMM_UNITY_VK_STEREO_PRESENT_20260811]",
            "_syntheticStereoCameraForValidation == cam",
            "VulkanEyeTargets[presentationEye]",
            "[IMM_SYNTH_PRESENT_EYE_20260804]",
            "private void OnRenderImage(RenderTexture source, RenderTexture destination)",
            "Material composite = GetVulkanCompositeMaterial();",
            "Graphics.Blit(eyeTarget, destination, composite);",
            "SetFlatAndroidVulkanSharedDepthCompositionForValidation",
            "VulkanEyeDepthTargets",
            "VulkanEyeDepthWriteTargets",
            "ImmVulkanDepthComposite",
            "eyeDepthTarget != null ? 1 : 0",
            "[IMM_UNITY_VK_DEPTH_COMPOSITE_20260803]",
            "SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan",
            "ImmNativePlugin.SetVulkanDedicatedQueueAllowed(allowDedicatedVulkanQueue ? 1 : 0)",
            "[IMM_UNITY_VK_ONRENDERIMAGE_20260802]",
            "useAndroidVulkanStereoSequence",
            "info.CommandBuffer.IssuePluginEvent(_renderEventFunc, leftEventId)",
            "info.CommandBuffer.IssuePluginEvent(_renderEventFunc, rightEventId)",
            "info.VulkanStereoCommandBufferFrame == Time.frameCount",
            "[IMM_UNITY_VK_STEREO_FRAME_20260811]",
            "EnsureCompositeQuad(cam, info)",
            "public readonly ImmCameraMatrixFrameGate MatrixFrameGate",
            "info.MatrixFrameGate.TryBeginSubmission(",
            "info.MatrixFrameGate.Reset();",
        ],
        ROOT / "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Tests/Runtime/ImmCameraMatrixFrameGateTests.cs": [
            "TwoCamerasCanSubmitInTheSameFrameWhileEachReusesItsMultipassPose",
            "XrCameraAcceptsTheNextFramesChangedPose",
            "MonoAndDiagnosticModesCanSubmitEveryCallback",
        ],
        ROOT / "code/ImmUnitySampleProject/Assets/Resources/ImmVulkanCompositeQuad.shader": [
            "sampler2D _EyeTex0;",
            "sampler2D _EyeTex1;",
            "UNITY_VERTEX_OUTPUT_STEREO",
            "UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(i);",
            "unity_StereoEyeIndex == 0",
        ],
        ROOT / ".github/workflows/ci-gpu.yml": [
            "Run Godot Vulkan ordered overlay smoke",
            "[IMM_GODOT_CI_RETRY_20260811]",
            "-CompositionMode ordered_overlay",
            "godot-smoke-windows-vulkan-ordered-overlay",
            "godot-vulkan-ordered-overlay.ppm",
            "composition-status.json",
            "sample1-full-depth.json",
            "sample1-ordered-overlay.json",
            "godot-vulkan-ordered-overlay-diagnostic.png",
            "IMM_GODOT_RENDER_GRAPH_DEPTH_COMPOSITION=1",
        ],
        ROOT / "tests/tools/run_firebase_test_lab_android.py": [
            "--infrastructure-retry-delay-seconds",
            "default=90.0",
            "time.sleep(retry_delay)",
        ],
        ROOT / ".github/workflows/ci-device.yml": [
            "godot-android-vulkan-sample1.json",
            "godot-android-vulkan-composition-sample1.json",
            "android-godot-vulkan-screenshot-metrics.json",
            "android-godot-vulkan-composition-metrics.json",
            "vulkan_render_candidate.png",
            "vulkan_visual_smoke.png",
            "classify_android_godot_vulkan.py",
            "android-godot-vulkan-status.json",
        ],
        ROOT / "tests/tools/composition_status.py": [
            "composition_mode",
            "composition_contract",
            "ordered_overlay",
            "depth_composition",
            "depth_interleaving",
            "render_only",
        ],
        ROOT / "code/appImmUnity/src/main.cpp": [
            "config.rendererApi = piRenderer::API::Vulkan;",
            "CommandRecordingState(",
            "kUnityVulkanRenderPass_EnsureInside",
            "RenderPreparedCamera(",
            "iRenderUnityVulkanCameraInHostRenderPass(",
            "[IMM_UNITY_VK_HOST_RT_20260612]",
            "BeginExternalImageFrame(",
            "EndExternalImageFrame();",
            "AccessRenderBufferTexture(",
            "const bool passiveAccess = vulkanRenderer->UsesDedicatedQueue();",
            "kUnityVulkanResourceAccess_ObserveOnly",
            "kUnityVulkanResourceAccess_PipelineBarrier",
            "unityVulkanDevice.allowDedicatedQueue = sAllowDedicatedVulkanQueue;",
            "Unity Vulkan render:",
        ],
        ROOT / "code/appImmShared/src/imm_engine_bridge.cpp": [
            "mConfig.reverseDepthBuffer",
            "mConfig.overrideFrontIsCCW",
            "conf.projectionMatrix = usesZeroToOneDepth",
            "DepthBuffer::Linear10",
            "SetUnityProjectionAdjusted(true)",
        ],
        ROOT / "code/appImmGodot/src/main.cpp": [
            "Godot's Metal projection is already adjusted for its render target",
            "if (config.rendererApi == piRenderer::API::Metal)",
            "config.overrideFrontIsCCW = true;",
            "config.frontIsCCW = false;",
            "Vulkan retains its existing host convention",
        ],
        ROOT / "code/libImmPlayer/src/layerRenderers/layerRendererPaint/static/layerRendererPaintStatic.cpp": [
            "Static paint's reflected geometry has the opposite submitted winding.",
            "mRasterState[3] = renderer->CreateRasterState(forcePaintWireframe,!frontIsCCW, piRenderer::CullMode::FRONT",
        ],
        ROOT / "code/appImmViewer/src/viewer/viewer.cpp": [
            "Settings::Rendering::API::Vulkan",
            "conf.frontIsCCW =",
            "negative-height viewport",
        ],
        ROOT / "code/libImmCore/src/libRender/vulkan/piVulkan_Renderer.cpp": [
            "bool ownsDedicatedQueue = false;",
            "externalDevice->allowDedicatedQueue &&",
            "[IMM_UNITY_VK_QUEUE_20260802] mode=host reason=dedicated-queue-not-authorized layoutOwner=unity",
            "batchRingBridgeSemaphores",
            "batchAppendDepthShaderReadTransition",
            "!clearExternalDepth",
            "[IMM_UNITY_VK_DEPTH_SAMPLE_20260803]",
            "iFlushBatch(",
            "BeginExternalImageFrameWithView(",
            "mState->batchAppendShaderReadTransition =",
            "mState->ownsDedicatedQueue &&",
            "externalDepthReverseZ",
            "[IMM_EXTERNAL_DEPTH_CLEAR_20260804] convention=normal-z clear=1",
            "clearExternalDepthAsReverseZ ? 0.0f : 1.0f",
        ],
        ROOT / "code/libImmCore/src/libRender/metal/piMetal_Renderer.mm": [
            "if (mState->unityProjectionAdjusted)",
            'withString:@"out.position.z = 0.0;"',
            "iAttachRetainedBufferCleanup(mState);",
            "[previous release];",
        ],
        ROOT / "tests/tools/compare_render_metrics.py": [
            "collect_surface_detail_metrics",
            "validate_surface_detail_contract",
            "localized_excess_edge_pixel_share",
            "edge_pixel_share_ratio",
            'output["surface_detail"]',
        ],
        ROOT / "code/projects/web/app/tests/web-player-firefox.mjs": [
            '"webgl.disabled": false',
            '"webgl.force-enabled": true',
            'canvas.getContext("webgl2")',
            "assert.equal(webgl2Available, true",
        ],
    }

    errors: list[str] = []
    for path, tokens in checks.items():
        missing = require_tokens(path, tokens)
        for token in missing:
            errors.append(f"{path.relative_to(ROOT)} missing token: {token}")

    surface_detail_contracts = {
        "android-standalone-gles-sample1.json": 1.04,
        "android-standalone-vulkan-sample1.json": 1.04,
        # Godot's Android screenshot is captured at the device's wider display
        # resolution. Its valid renderer-specific edge density is higher than
        # the DirectX reference; face winding is checked by the dedicated fixture.
        "godot-android-vulkan-sample1.json": 1.25,
        # The iOS Simulator Metal capture also has a stable renderer-specific
        # edge density above the DirectX reference. Its dedicated face fixture
        # provides the strict winding verdict.
        "godot-ios-metal-sample1.json": 1.20,
        "godot-windows-vulkan-sample1.json": 1.04,
        "ios-standalone-metal-sample1.json": 1.04,
        "macos-metal-sample1.json": 1.04,
        "macos-standalone-metal-sample1.json": 1.04,
        "unity-android-vulkan-external-render-sample1.json": 1.04,
        "unity-android-vulkan-sample1.json": 1.04,
        "unity-macos-metal-sample1.json": 1.04,
        "unity-windows-directx-sample1.json": 1.04,
        "web-three-sample1.json": 1.04,
        "windows-directx-sample1.json": 1.04,
    }
    for contract_name, expected_ceiling in surface_detail_contracts.items():
        contract_path = ROOT / "tests/baselines/render" / contract_name
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        surface_detail = contract.get("validation", {}).get("expected_surface_detail")
        if not isinstance(surface_detail, dict):
            errors.append(f"{contract_name} must require fine surface-detail validation")
            continue
        if surface_detail.get("max_edge_pixel_share_ratio") != expected_ceiling:
            errors.append(
                f"{contract_name} must retain its reviewed {expected_ceiling} surface edge-density ceiling"
            )
        if surface_detail.get("analysis_width", 0) < 320 or surface_detail.get("analysis_height", 0) < 180:
            errors.append(f"{contract_name} surface-detail analysis resolution is too coarse")

    unity_ci_preparer = (
        ROOT / "tests/tools/prepare_unity_ci_project.py"
    ).read_text(encoding="utf-8")
    if '"com.unity.xr.openxr"' in unity_ci_preparer:
        errors.append("Clean non-XR Unity CI project must not install the OpenXR package")

    unity_native = (
        ROOT / "code/appImmUnity/src/main.cpp"
    ).read_text(encoding="utf-8")
    access_queue_call = "AccessQueue(iUnityVulkanQueueRenderCallback, event_id, &context, true)"
    if unity_native.count(access_queue_call) != 1:
        errors.append("Unity Vulkan host fallback must contain exactly one synchronized AccessQueue call")
    else:
        passive_branch = unity_native.find("if (passiveAccess)")
        access_queue = unity_native.find(access_queue_call)
        if passive_branch < 0 or passive_branch > access_queue:
            errors.append("Unity Vulkan dedicated-queue bypass must precede the host AccessQueue fallback")
        elif "return true;" not in unity_native[passive_branch:access_queue]:
            errors.append("Unity Vulkan dedicated-queue path must return before the host AccessQueue fallback")

    vulkan_native = (
        ROOT / "code/libImmCore/src/libRender/vulkan/piVulkan_Renderer.cpp"
    ).read_text(encoding="utf-8")
    if vulkan_native.count("mState->ownsDedicatedQueue &&") < 3:
        errors.append(
            "Unity Vulkan shader-read transitions and bridge synchronization must remain dedicated-queue-only"
        )
    godot_depth_clear = vulkan_native.find(
        "const bool clearExternalDepthAsReverseZ ="
    )
    godot_depth_clear_call = vulkan_native.find(
        "clearExternalDepthAsReverseZ ? 0.0f : 1.0f"
    )
    if godot_depth_clear < 0 or godot_depth_clear_call < godot_depth_clear:
        errors.append(
            "External depth clear must use the same Unity-only reverse-Z decision as its depth pipeline"
        )
    elif "mState->externalDepthReverseZ" not in vulkan_native[
        godot_depth_clear:godot_depth_clear_call
    ]:
        errors.append(
            "External depth clear must distinguish Unity reverse-Z from Godot normal-Z"
        )
    elif "mState->currentDepthState->lessEqual" not in vulkan_native[
        godot_depth_clear:godot_depth_clear_call
    ]:
        errors.append(
            "External depth clear must preserve reverse-Z for Unity paths without a dedicated queue"
        )

    unity_smoke = (
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmUnityRuntimeSmoke.cs"
    ).read_text(encoding="utf-8")
    full_depth_contract = json.loads(
        (
            ROOT / "tests/baselines/render/sample1-full-depth.json"
        ).read_text(encoding="utf-8")
    )
    lower_red_probe = next(
        probe
        for probe in full_depth_contract["validation"]["expected_color_components"]["probes"]
        if probe["name"] == "sample1-lower-red-brush-content"
    )
    if lower_red_probe.get("minimum_matched_pixel_share_of_crop") != 0.0005:
        errors.append(
            "Full-depth lower-red presence threshold must retain the reviewed total-pixel 0.0005 floor"
        )
    if "minimum_largest_component_share_of_crop" in lower_red_probe:
        errors.append(
            "Stippled lower-red IMM content must not require one contiguous color component"
        )
    if 'Overlay Fixture Camera", StringComparison.Ordinal' not in unity_smoke:
        errors.append("Unity ordered-overlay probes must use the scene camera, not the late overlay camera")
    if 'Environment.SetEnvironmentVariable("IMM_UNITY_FORCE_TEXTURE_PROJECTION"' in unity_smoke:
        errors.append(
            "Unity smoke capture must detect its explicit RenderTexture instead of forcing projection"
        )
    if "enableDiagnosticCameraTarget = true;" in unity_smoke:
        errors.append(
            "Unity Android Vulkan smoke must exercise the production display target"
        )
    if unity_smoke.find("FreezeCompositionPlaybackIfRequested();") > unity_smoke.find(
        'WriteCapture(renderCapture, _renderCapturePath, "render candidate");'
    ):
        errors.append(
            "Unity composition playback must be frozen before its render candidate is captured"
        )

    windows_godot_smoke = (
        ROOT / "code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1"
    ).read_text(encoding="utf-8")
    if "compare-ppm-captures.ps1" in windows_godot_smoke:
        errors.append(
            "Godot Vulkan smoke must use only the shared render-metrics baseline comparator"
        )

    for workflow_name in ["ci-engine.yml", "ci-gpu.yml"]:
        workflow_text = (ROOT / ".github/workflows" / workflow_name).read_text(encoding="utf-8")
        if "expected_failed" in workflow_text:
            errors.append(f"{workflow_name} must not downgrade composition failures to expected_failed")

    metal_renderer = (
        ROOT / "code/libImmCore/src/libRender/metal/piMetal_Renderer.mm"
    ).read_text(encoding="utf-8")
    unity_adjust_start = metal_renderer.find("if (mState->unityProjectionAdjusted)")
    unity_adjust_end = metal_renderer.find("NSError *compileError", unity_adjust_start)
    if unity_adjust_start < 0 or unity_adjust_end < 0:
        errors.append("Unity Metal projection-adjust block could not be located")
    else:
        unity_adjust = metal_renderer[unity_adjust_start:unity_adjust_end]
        if "out.position.y = -out.position.y" in unity_adjust:
            errors.append(
                "Unity Metal projection is already render-target adjusted and must not be flipped again"
            )
        if 'withString:@"out.position.z = 0.0;"' not in unity_adjust:
            errors.append(
                "Unity Metal reversed-Z backdrop must remain at depth zero"
            )

    skipped_external_cleanup = (
        "if (!mState->externalCommandBuffer)\n"
        "    {\n"
        "        iAttachRetainedBufferCleanup(mState);"
    )
    if skipped_external_cleanup in metal_renderer:
        errors.append(
            "Unity-owned Metal command buffers must retire replaced buffers on completion"
        )

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("Composition probe contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
