#!/usr/bin/env python3
"""Verify scene-composition visual smokes use region and leakage checks."""

from __future__ import annotations

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
            "0.00015f",
            "rearOccluded",
            "MaxOccludedShare",
            "OverlayProbeEnv",
            "CaptureCameraTexture",
            "CaptureOrderedCameraStackTexture(captureCamera)",
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
            "scene composition %s ordered overlay rear probe failed",
            "scene composition %s %s IMM visibility failed",
            "last_depth_aware_vulkan_composite",
            "last_depth_aware_vulkan_composite_result",
            "last_had_intermediate_texture",
            "last_had_intermediate_depth_texture",
            "last_vulkan_depth_image_handle",
            "MIN_ORDERED_OVERLAY_IMM_PIXELS",
            "target_share",
            "scene composition %s rear occlusion leakage probe failed",
            "unproject_position",
            'return configured_value != "0"',
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
            "--required-capture-name unity-android-vulkan.png",
            "Record Unity Android Vulkan visual metrics",
            "--external-screen-capture-name unity-android-vulkan-robo-final.png",
            "validate_render_video.py",
            "unity-android-vulkan-external-render-sample1.json",
            "unity-android-vulkan-external-render-video-validation.json",
            "unity-android-vulkan-external-render-metrics.json",
            "unity-android-vulkan-external-render.png",
            "--minimum-consecutive-frames 2",
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
            "absent-front-magenta",
            "absent-rear-yellow",
            "maximum_largest_component_share_of_crop",
        ],
        ROOT / "tests/baselines/render/unity-android-vulkan-composition-sample1.json": [
            "expected_spatial_luma_grid",
            '"min_correlation": 0.35',
            '"max_mean_abs_delta": 0.15',
            '"maximum_largest_component_share_of_crop": 0.00015',
            "expected_color_components",
            "front-magenta",
            "rear-visible-yellow",
            "rear-occluded-cyan",
            "minimum_largest_component_share_of_crop",
            "maximum_largest_component_share_of_crop",
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
            "PresentFlatAndroidVulkanFrame",
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
        ],
        ROOT / ".github/workflows/ci-gpu.yml": [
            "Run Godot Vulkan ordered overlay smoke",
            "-CompositionMode ordered_overlay",
            "godot-smoke-windows-vulkan-ordered-overlay",
            "godot-vulkan-ordered-overlay.ppm",
            "composition-status.json",
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
            "conf.projectionMatrix = usesZeroToOneDepth",
            "DepthBuffer::Linear10",
            "SetUnityProjectionAdjusted(true)",
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
        ],
        ROOT / "code/libImmCore/src/libRender/metal/piMetal_Renderer.mm": [
            "if (mState->unityProjectionAdjusted)",
            'withString:@"out.position.z = 0.0;"',
            "iAttachRetainedBufferCleanup(mState);",
            "[previous release];",
        ],
    }

    errors: list[str] = []
    for path, tokens in checks.items():
        missing = require_tokens(path, tokens)
        for token in missing:
            errors.append(f"{path.relative_to(ROOT)} missing token: {token}")

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

    unity_smoke = (
        ROOT / "code/ImmUnitySampleProject/Assets/Scripts/ImmUnityRuntimeSmoke.cs"
    ).read_text(encoding="utf-8")
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
