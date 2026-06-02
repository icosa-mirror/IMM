#!/usr/bin/env python3
"""Run local Godot extension checks that do not require Godot or godot-cpp."""

from __future__ import annotations

import shutil
import subprocess
import sys
import configparser
import os
import platform
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "code/ImmGodotSampleProject/addons/imm_viewer/imm_viewer.gdextension"
PROJECT = ROOT / "code/ImmGodotSampleProject/project.godot"
ABI_HEADER = ROOT / "code/appImmGodot/src/imm_godot_plugin.h"
ABI_SOURCE = ROOT / "code/appImmGodot/src/main.cpp"
REGISTER_TYPES = ROOT / "code/appImmGodotGDExtension/src/register_types.cpp"
VIEWER_HEADER = ROOT / "code/appImmGodotGDExtension/src/imm_viewer_node.h"
VIEWER_SOURCE = ROOT / "code/appImmGodotGDExtension/src/imm_viewer_node.cpp"
COMPOSITOR_EFFECT_HEADER = ROOT / "code/appImmGodotGDExtension/src/imm_viewer_compositor_effect.h"
COMPOSITOR_EFFECT_SOURCE = ROOT / "code/appImmGodotGDExtension/src/imm_viewer_compositor_effect.cpp"
WINDOWS_BUILD_HELPER = ROOT / "code/projects/windows/build-godot-extension.ps1"
WINDOWS_SMOKE_HELPER = ROOT / "code/projects/windows/run-godot-smoke.ps1"
WORKFLOW = ROOT / ".github/workflows/build.yml"
SCONSTRUCT = ROOT / "code/appImmGodotGDExtension/SConstruct"
GODOT_SMOKE_RUNNER = ROOT / "code/ImmGodotSampleProject/scripts/smoke_test_runner.gd"
GODOT_SCRIPT_STUB = ROOT / "code/ImmGodotSampleProject/addons/imm_viewer/imm_viewer_node.gd"
GODOT_VISUAL_CONTROLLER = ROOT / "code/ImmGodotSampleProject/scripts/visual_smoke_controller.gd"
GODOT_SAMPLE_SCENE = ROOT / "code/ImmGodotSampleProject/scenes/SampleScene.tscn"
GODOT_VISUAL_SCENE = ROOT / "code/ImmGodotSampleProject/scenes/VisualSmokeScene.tscn"
GODOT_EXTENSION_SOURCES = [
    ROOT / "code/appImmGodotGDExtension/src/imm_viewer_compositor_effect.cpp",
    ROOT / "code/appImmGodotGDExtension/src/imm_viewer_metal_frame.mm",
    ROOT / "code/appImmGodotGDExtension/src/imm_viewer_node.cpp",
    ROOT / "code/appImmGodotGDExtension/src/register_types.cpp",
]
GODOT_RUNTIME_DEPENDENCIES = {
    "Audio360.dll": ROOT / "thirdparty/audio360-sdk/Audio360/Windows/x64/Audio360.dll",
    "opus.dll": ROOT / "thirdparty/opus/bin/opus.dll",
    "opusenc.dll": ROOT / "thirdparty/libopusenc/bin/opusenc.dll",
    "vorbisenc.dll": ROOT / "thirdparty/libvorbis/bin/vorbisenc.dll",
    "zlib1.dll": ROOT / "thirdparty/zlib/bin/zlib1.dll",
    "jpeg62.dll": ROOT / "thirdparty/libjpeg-turbo/bin/jpeg62.dll",
    "libpng16.dll": ROOT / "thirdparty/libpng/bin/libpng16.dll",
    "ogg.dll": ROOT / "thirdparty/libogg/bin/ogg.dll",
    "vorbis.dll": ROOT / "thirdparty/libvorbis/bin/vorbis.dll",
}


def unquote(value: str) -> str:
    return value.strip().strip('"')


def verify_manifest() -> str:
    parser = configparser.ConfigParser()
    parser.optionxform = str
    with MANIFEST.open(encoding="utf-8") as handle:
        parser.read_file(handle)

    entry_symbol = unquote(parser.get("configuration", "entry_symbol", fallback=""))
    if entry_symbol != "imm_godot_library_init":
        raise RuntimeError(f"Unexpected GDExtension entry symbol: {entry_symbol!r}")

    expected_libraries = {
        "windows.debug.x86_64": "res://addons/imm_viewer/bin/windows/debug/imm_godot_extension.dll",
        "windows.release.x86_64": "res://addons/imm_viewer/bin/windows/release/imm_godot_extension.dll",
    }
    for key, expected in expected_libraries.items():
        actual = unquote(parser.get("libraries", key, fallback=""))
        if actual != expected:
            raise RuntimeError(f"Unexpected GDExtension library path for {key}: {actual!r}")

    print("GDExtension manifest paths ok", flush=True)
    return entry_symbol


def verify_project_renderer() -> None:
    project = PROJECT.read_text(encoding="utf-8")
    if 'paths=["res://addons/imm_viewer/imm_viewer.gdextension"]' not in project:
        raise RuntimeError("Godot sample project does not list the IMM GDExtension in native_extensions")

    rendering_match = re.search(r"^renderer/rendering_method=(.+)$", project, re.MULTILINE)
    # Godot only writes renderer/rendering_method when it differs from the default,
    # so an absent key means the sample is using the Forward+ default.
    rendering_method = unquote(rendering_match.group(1)) if rendering_match else "forward_plus"
    if rendering_method != "forward_plus":
        raise RuntimeError(f"Unexpected sample renderer path: {rendering_method!r}")
    if 'run/main_scene="uid://dxrq2se1fvtxw"' not in project and 'run/main_scene="res://scenes/SampleScene.tscn"' not in project:
        raise RuntimeError("Godot sample project Run button must launch SampleScene.tscn")

    print("Godot sample renderer path ok", flush=True)


def verify_registration(entry_symbol: str) -> None:
    source = REGISTER_TYPES.read_text(encoding="utf-8")
    if f"{entry_symbol}(" not in source:
        raise RuntimeError(f"GDExtension entry symbol {entry_symbol!r} is not defined in register_types.cpp")
    if "ClassDB::register_class<ImmViewerNode>()" not in source:
        raise RuntimeError("register_types.cpp does not register ImmViewerNode")
    if "MODULE_INITIALIZATION_LEVEL_SCENE" not in source:
        raise RuntimeError("register_types.cpp does not use scene-level initialization")

    print("GDExtension registration ok", flush=True)


def verify_c_abi_exports() -> None:
    header = ABI_HEADER.read_text(encoding="utf-8")
    source = ABI_SOURCE.read_text(encoding="utf-8")

    declared = set(re.findall(r"IMMGODOT_EXPORT\s+[^;()]+?\s+(ImmGodot_[A-Za-z0-9_]+)\s*\(", header))
    defined = set(re.findall(r'extern\s+"C"\s+IMMGODOT_EXPORT\s+[^;()]+?\s+(ImmGodot_[A-Za-z0-9_]+)\s*\(', source))

    missing_definitions = sorted(declared - defined)
    missing_declarations = sorted(defined - declared)
    if missing_definitions or missing_declarations:
        details: list[str] = []
        if missing_definitions:
            details.append("missing definitions: " + ", ".join(missing_definitions))
        if missing_declarations:
            details.append("missing declarations: " + ", ".join(missing_declarations))
        raise RuntimeError("ImmGodot C ABI mismatch; " + "; ".join(details))

    print(f"ImmGodot C ABI exports ok ({len(declared)} functions)", flush=True)


def verify_native_method_bindings() -> None:
    header = VIEWER_HEADER.read_text(encoding="utf-8")
    source = VIEWER_SOURCE.read_text(encoding="utf-8")

    public_section_match = re.search(r"public:\s*(.*?)\n\s*private:", header, re.DOTALL)
    if public_section_match is None:
        raise RuntimeError("Could not find ImmViewerNode public method section")

    public_section = public_section_match.group(1)
    method_pattern = re.compile(
        r"^\s*(?:static\s+)?(?:void|bool|int|int64_t|double|float|String|NodePath|Color|Dictionary|Transform3D|PackedInt32Array)"
        r"\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(",
        re.MULTILINE,
    )
    ignored_methods = {
        "_bind_methods",
        "_ready",
        "_exit_tree",
        "_process",
    }
    public_method_list = [name for name in method_pattern.findall(public_section) if name not in ignored_methods]
    duplicate_methods = sorted({name for name in public_method_list if public_method_list.count(name) > 1})
    if duplicate_methods:
        raise RuntimeError("ImmViewerNode public method declarations are duplicated: " + ", ".join(duplicate_methods))
    public_methods = set(public_method_list)
    bound_methods = set(re.findall(r'ClassDB::bind_method\s*\(\s*D_METHOD\s*\(\s*"([^"]+)"', source))
    missing_bindings = sorted(public_methods - bound_methods)
    stale_bindings = sorted(bound_methods - public_methods)
    if missing_bindings or stale_bindings:
        details: list[str] = []
        if missing_bindings:
            details.append("missing bindings: " + ", ".join(missing_bindings))
        if stale_bindings:
            details.append("stale bindings: " + ", ".join(stale_bindings))
        raise RuntimeError("ImmViewerNode method binding mismatch; " + "; ".join(details))
    for token in ["is_document_timeline_ready", "ImmGodot_GetDocumentState", "set_time", "seek_relative_seconds"]:
        if token not in source:
            raise RuntimeError(f"ImmViewerNode timeline safety token is missing: {token}")

    print(f"ImmViewerNode method bindings ok ({len(public_methods)} methods)", flush=True)


def verify_render_thread_queue() -> None:
    source = VIEWER_SOURCE.read_text(encoding="utf-8")
    header = VIEWER_HEADER.read_text(encoding="utf-8")
    compositor_header = COMPOSITOR_EFFECT_HEADER.read_text(encoding="utf-8")
    compositor_source = COMPOSITOR_EFFECT_SOURCE.read_text(encoding="utf-8")
    register_types = REGISTER_TYPES.read_text(encoding="utf-8")
    sconstruct = SCONSTRUCT.read_text(encoding="utf-8")
    abi_header = ABI_HEADER.read_text(encoding="utf-8")
    abi_source = ABI_SOURCE.read_text(encoding="utf-8")
    sample = (ROOT / "code/ImmGodotSampleProject/scripts/sample_scene_controller.gd").read_text(encoding="utf-8")
    script_stub = GODOT_SCRIPT_STUB.read_text(encoding="utf-8")
    if "call_on_render_thread" in source:
        raise RuntimeError("ImmViewerNode should not use the old call_on_render_thread smoke render queue")
    if "render_last_camera_on_render_thread" in source or "render_last_camera_on_render_thread" in header:
        raise RuntimeError("ImmViewerNode should not expose the old render-thread smoke callback")
    if "call_on_render_thread" in script_stub:
        raise RuntimeError("ImmViewerNode script stub should not use call_on_render_thread")
    if "#include <godot_cpp/classes/rendering_device.hpp>" not in source:
        raise RuntimeError("ImmViewerNode does not include RenderingDevice for backend diagnostics")
    if "queue_render_camera_transform" not in source:
        raise RuntimeError("ImmViewerNode does not expose camera/viewport render queuing")
    for token in ["std::mutex _render_request_mutex", "std::lock_guard<std::mutex>", "RenderRequest _pending_render_request"]:
        if token not in source and token not in header:
            raise RuntimeError(f"ImmViewerNode render request synchronization token is missing: {token}")
    if "ImmViewerCompositorEffect::queue_render_request" not in source:
        raise RuntimeError("ImmViewerNode does not publish queued camera renders to ImmViewerCompositorEffect")
    for method in ["register_render_camera", "unregister_render_camera", "is_render_camera_registered"]:
        if method not in source:
            raise RuntimeError(f"ImmViewerNode render camera lifecycle method is missing: {method}")
    for token in ["auto_queue_render", "render_camera_path", "update_auto_render_camera", "get_visible_rect().size", "queue_render_camera_transform", "register_render_camera"]:
        if token not in source and token not in header:
            raise RuntimeError(f"ImmViewerNode native auto render scheduling token is missing: {token}")
        if token not in script_stub:
            raise RuntimeError(f"ImmViewerNode script stub auto render scheduling token is missing: {token}")
    for token in ["render_adapter_before_camera", "render_adapter_after_camera", "render_adapter_graphics_initialized"]:
        if token not in source and token not in header:
            raise RuntimeError(f"ImmViewerNode native render adapter diagnostics token is missing: {token}")
    for token in ["adapter_before_render_count", "adapter_after_render_count", "adapter_last_viewport_width"]:
        if token not in source and token not in header:
            raise RuntimeError(f"ImmViewerNode native render adapter diagnostics token is missing: {token}")
        if token not in script_stub:
            raise RuntimeError(f"ImmViewerNode script stub render adapter diagnostics token is missing: {token}")
    if "register_render_camera" in sample or "unregister_render_camera" in sample:
        raise RuntimeError("Sample controller should not own render camera lifecycle")
    for token in ["ImmGodotRendererApi_Metal", "ImmGodot_InitEx", "ImmGodotMetalFrame", "ImmGodot_BeginMetalFrame", "ImmGodot_EndMetalFrame", "ImmGodotVulkanFrame", "ImmGodot_BeginVulkanFrame", "ImmGodot_EndVulkanFrame"]:
        if token not in abi_header or token not in abi_source:
            raise RuntimeError(f"ImmGodot native renderer selection token is missing from C ABI: {token}")
    for token in ["renderer_api", "set_renderer_api", "get_renderer_api", "ImmGodot_InitEx", "get_render_backend_diagnostics", "actual_rendering_method", "actual_rendering_driver", "metal_adapter_candidate", "vulkan_adapter_candidate", "wants_vulkan_renderer", "driver_is_vulkan", "has_rendering_device", "has_generic_driver_resources", "has_vulkan_driver_resources", "has_compositor_effect_path"]:
        if token not in source and token not in header:
            raise RuntimeError(f"ImmViewerNode native renderer selection token is missing: {token}")
    for token in ["renderer_api", "Vulkan", "get_render_backend_diagnostics", "actual_rendering_method", "actual_rendering_driver", "metal_adapter_candidate", "vulkan_adapter_candidate", "wants_vulkan_renderer", "driver_is_vulkan", "has_rendering_device", "has_generic_driver_resources", "has_vulkan_driver_resources", "has_compositor_effect_path"]:
        if token not in script_stub:
            raise RuntimeError(f"ImmViewerNode script stub renderer backend token is missing: {token}")
    for token in ["ImmViewerCompositorEffect", "CompositorEffect", "_render_callback", "RenderSceneBuffersRD", "get_color_texture", "DRIVER_RESOURCE_COMMAND_QUEUE", "DRIVER_RESOURCE_TEXTURE", "DRIVER_RESOURCE_VULKAN_INSTANCE", "DRIVER_RESOURCE_VULKAN_IMAGE", "get_driver_resource", "queue_render_request", "ImmGodot_RenderCamera", "ImmViewerGodotBeginMetalTextureFrame", "ImmViewerGodotBeginVulkanTextureFrame", "last_metal_frame_started", "last_vulkan_frame_started", "last_command_queue_handle", "last_color_texture_handle"]:
        if token not in compositor_source and token not in compositor_header:
            raise RuntimeError(f"ImmViewerCompositorEffect token is missing: {token}")
    if "g_queued_render_request.queued = false" in compositor_source:
        raise RuntimeError("ImmViewerCompositorEffect should keep the latest render request for continuous editor playback")
    for token in ["ensure_intermediate_texture", "_intermediate_texture", "_intermediate_size", "_intermediate_format"]:
        if token not in compositor_source and token not in compositor_header:
            raise RuntimeError(f"ImmViewerCompositorEffect persistent intermediate token is missing: {token}")
    if "texture(source_color, uv_interp)" not in compositor_source:
        raise RuntimeError("ImmViewerCompositorEffect composite shader must sample the Metal intermediate texture without an extra vertical flip")
    if "1.0 - uv_interp.y" in compositor_source:
        raise RuntimeError("ImmViewerCompositorEffect composite shader should not vertically flip the Godot-owned intermediate texture")
    if "register_class<ImmViewerCompositorEffect>" not in register_types:
        raise RuntimeError("ImmViewerCompositorEffect is not registered with ClassDB")
    for token in ["register_extension_dll_directory", "GetModuleHandleExW", "GetModuleFileNameW", "AddDllDirectory", "SetDllDirectoryW"]:
        if token not in register_types:
            raise RuntimeError(f"Windows editor DLL dependency search token is missing: {token}")
    for token in ["src/imm_viewer_compositor_effect.cpp", "src/imm_viewer_metal_frame.mm", "src/imm_viewer_vulkan_frame.cpp", "FRAMEWORKS=[\"Metal\", \"Foundation\"]"]:
        if token not in sconstruct:
            raise RuntimeError(f"SConstruct is missing compositor/Metal build token: {token}")
    metal_helper = (ROOT / "code/appImmGodotGDExtension/src/imm_viewer_metal_frame.mm").read_text(encoding="utf-8")
    for token in ["MTLRenderPassDescriptor", "MTLLoadActionLoad", "MTLStoreActionStore", "ImmGodotMetalFrameMode_CommandQueueRenderPass", "ImmGodot_BeginMetalFrame", "ImmGodot_EndMetalFrame"]:
        if token not in metal_helper:
            raise RuntimeError(f"Metal frame helper token is missing: {token}")
    if "ImmViewerCompositorEffect::queue_render_request" not in source:
        raise RuntimeError("ImmViewerNode does not publish queued camera renders to ImmViewerCompositorEffect")
    if "src/imm_viewer_compositor_effect.cpp" not in sconstruct:
        raise RuntimeError("SConstruct does not build ImmViewerCompositorEffect")
    visual_controller = GODOT_VISUAL_CONTROLLER.read_text(encoding="utf-8")
    for token in ["IMM_GODOT_VISUAL_SMOKE", "IMM_GODOT_VISUAL_RENDERER_API", "IMM_GODOT_VISUAL_SMOKE_PNG", "IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES", "_exercise_reload_cycles", "ClassDB.instantiate(\"ImmViewerNode\")", "ClassDB.instantiate(\"ImmViewerCompositorEffect\")", "Compositor.new", "camera.compositor", "_selected_renderer_api", "IMM_RENDERER_API_METAL", "IMM_RENDERER_API_VULKAN", "SAMPLE_DOCUMENT_PATH", "callback_count", "last_command_queue_handle", "last_color_texture_handle", "last_metal_frame_started", "last_vulkan_frame_started", "last_render_result", "MIN_ORIENTATION_LUMA_DELTA", "orientation_luma_delta", "visual smoke PNG orientation check failed", "IMM Godot Metal visual smoke passed", "IMM Godot Vulkan visual smoke passed"]:
        if token not in visual_controller:
            raise RuntimeError(f"Visual smoke controller token is missing: {token}")

    print("ImmViewerNode camera/viewport render queue ownership ok", flush=True)


def parse_tscn(path: Path) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    sections: list[dict[str, str]] = []
    properties: dict[str, str] = {}
    current_section: dict[str, str] | None = None

    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            current_section = {}
            for key, value in re.findall(r'([A-Za-z_]+)=("[^"]*"|\S+)', line):
                current_section[key] = unquote(value)
            sections.append(current_section)
            continue
        if current_section is not None and "=" in line:
            key, value = line.split("=", 1)
            current_section[key.strip()] = value.strip()

    resources = [section for section in sections if section.get("type") == "Script" and "path" in section]
    nodes = [section for section in sections if "name" in section and "type" in section]
    return resources, nodes


def node_by_name(nodes: list[dict[str, str]], name: str) -> dict[str, str]:
    for node in nodes:
        if node.get("name") == name:
            return node
    raise RuntimeError(f"Godot scene is missing node {name!r}")


def verify_godot_scenes() -> None:
    sample_resources, sample_nodes = parse_tscn(GODOT_SAMPLE_SCENE)
    visual_resources, visual_nodes = parse_tscn(GODOT_VISUAL_SCENE)

    sample_resource_paths = {resource["path"] for resource in sample_resources}
    visual_resource_paths = {resource["path"] for resource in visual_resources}

    if "res://scripts/sample_scene_controller.gd" not in sample_resource_paths:
        raise RuntimeError("SampleScene.tscn does not reference sample_scene_controller.gd")
    if "res://addons/imm_viewer/imm_viewer_node.gd" in sample_resource_paths:
        raise RuntimeError("SampleScene.tscn must use native ImmViewerNode, not the script stub")
    if "res://scripts/visual_smoke_controller.gd" not in visual_resource_paths:
        raise RuntimeError("VisualSmokeScene.tscn does not reference visual_smoke_controller.gd")
    if "res://addons/imm_viewer/imm_viewer_node.gd" in visual_resource_paths:
        raise RuntimeError("VisualSmokeScene.tscn must not reference the script stub")

    if node_by_name(sample_nodes, "ImmViewer").get("type") != "ImmViewerNode":
        raise RuntimeError("SampleScene.tscn ImmViewer must be native ImmViewerNode")
    if any(node.get("type") == "ImmViewerNode" for node in visual_nodes):
        raise RuntimeError("VisualSmokeScene.tscn should create ImmViewerNode after explicitly loading the extension")

    viewer = node_by_name(sample_nodes, "ImmViewer")
    if viewer.get("document_path") != '"res://../../exampleImmFiles/sample1.imm"':
        raise RuntimeError("SampleScene.tscn ImmViewer must reference sample1.imm")
    if viewer.get("load_on_ready") == "true":
        raise RuntimeError("SampleScene.tscn ImmViewer must let the controller warm up compositor resources before loading")
    if viewer.get("auto_play") == "false":
        raise RuntimeError("SampleScene.tscn ImmViewer must not disable auto-play")
    if viewer.get("auto_queue_render") != "true":
        raise RuntimeError("SampleScene.tscn ImmViewer must enable auto_queue_render")
    if viewer.get("render_camera_path") != 'NodePath("../CameraRig/Camera3D")':
        raise RuntimeError("SampleScene.tscn ImmViewer must target ../CameraRig/Camera3D for render_camera_path")

    for required in ["CameraRig", "Camera3D", "StatusLabel"]:
        node_by_name(sample_nodes, required)
        node_by_name(visual_nodes, required)

    print("Godot sample/visual smoke scenes ok", flush=True)


def verify_windows_build_wiring() -> None:
    helper = WINDOWS_BUILD_HELPER.read_text(encoding="utf-8")
    smoke_helper = WINDOWS_SMOKE_HELPER.read_text(encoding="utf-8")
    smoke_runner = GODOT_SMOKE_RUNNER.read_text(encoding="utf-8")
    script_stub = GODOT_SCRIPT_STUB.read_text(encoding="utf-8")
    sconstruct = SCONSTRUCT.read_text(encoding="utf-8")
    workflow = WORKFLOW.read_text(encoding="utf-8")

    for script_path, script_text in [
        (WINDOWS_BUILD_HELPER, helper),
        (WINDOWS_SMOKE_HELPER, smoke_helper),
    ]:
        if script_text.count("{") != script_text.count("}"):
            raise RuntimeError(f"PowerShell brace balance looks wrong in {script_path}")
        if script_text.count("(") != script_text.count(")"):
            raise RuntimeError(f"PowerShell parenthesis balance looks wrong in {script_path}")

    for token in ["BootstrapGodotCpp", "GodotCppRef", "PreflightOnly", "RunSmoke", "SmokeRendererApi", "run-godot-smoke.ps1", "A full run with -BootstrapGodotCpp will clone", "Godot extension preflight passed", "godot-4.5-stable", "github.com/godotengine/godot-cpp.git", "godot-cpp generated bindings were not found yet", "Skipping godot-cpp build; cached library and generated bindings are already present", "(Join-Path $godotCpp \"bin\\libgodot-cpp.windows.$target.x86_64.lib\")", "(Join-Path $godotCpp \"bin\\libgodot-cpp.windows.$target.dev.x86_64.lib\")", "(Join-Path $godotCpp \"bin\\libgodot-cpp.windows.$shortTarget.x86_64.lib\")", "Godot staged output DLLs are missing", "Verified staged Godot DLL set", "ExpectedDllCount=", "GeneratedUtc=", "godot-extension-dlls.txt"]:
        if token not in helper:
            raise RuntimeError(f"Windows Godot build helper is missing bootstrap wiring token: {token}")

    if re.search(r"^\s*class_name\s+ImmViewerNode\b", script_stub, re.MULTILINE):
        raise RuntimeError("Script stub must not claim the native ImmViewerNode class_name")

    for token in ["Configuration", "PreflightOnly", "Godot smoke preflight passed", "addons\\imm_viewer\\bin\\windows\\$variant", "imm_godot_extension.dll", "ImmGodotPlugin.dll", "Audio360.dll", "opus.dll", "opusenc.dll", "zlib1.dll", "jpeg62.dll", "libpng16.dll", "ogg.dll", "vorbis.dll", "Godot GDExtension runtime DLLs are missing", "GodotExe", "RequireExtension", "SmokeScene", "LogDir", "LoadUnloadCycles", "RendererApi", "IMM_GODOT_LOAD_UNLOAD_CYCLES", "IMM_GODOT_RENDERER_API", "godot-smoke-output.log", "godot-smoke-summary.txt", "godot-extension-dlls.txt", "Expected staged DLLs:", "FOUND`t", "MISSING`t", "Mirrored $Configuration GDExtension DLLs for Godot editor feature lookup", "SuccessMarker=", "HasSuccessMarker=", "did not print success marker", "ExtensionDir=", "EditorExtensionDir=", "EditorExtensionDll=", "SampleScene.tscn", "IMM_GODOT_EXPECT_NATIVE", "smoke_test_runner.gd", "--headless"]:
        if token not in smoke_helper:
            raise RuntimeError(f"Windows Godot smoke helper is missing token: {token}")

    for token in ["EXPECTED_RENDERER", "EXTENSION_PATH", "GDExtensionManager.load_extension", "SAMPLE_SCENE", "IMM_GODOT_LOAD_UNLOAD_CYCLES", "IMM_GODOT_RENDERER_API", "requested_renderer_api", "render backend diagnostics reported renderer_api", "_exercise_load_unload_cycles", 'is_class("ImmViewerNode")', "auto_queue_render", "render_camera_path", "is_render_camera_registered", "load_document", "is_loaded", "ImmViewer did not load", "document_loaded signal was not emitted by load_document", "document_unloaded signal was not emitted by unload_document", "playback_changed signal was not emitted by auto-play load", "spawn_area_changed signal was not emitted by next/previous spawn-area navigation", "native_backend_initialized", "native_backend_failed", "did not match expected_native", "_connect_viewer_signals", "_wait_for_timeline_ready", "get_document_state", "set_document_transform", "get_document_transform", "get_background_color", "RenderingServer.set_default_clear_color", "RenderingServer.get_default_clear_color", "get_chapter_count", "get_current_chapter", "set_time", "get_time", "get_play_time", "get_play_time_seconds", "seek_relative_seconds", "seek_relative_seconds(0.5) did not advance get_play_time()", "seek_relative_seconds should clamp below zero", "get_bounding_box", "get_layer_count", "get_layer_info", "get_layer_diagnostics", "set_layer_visible", "clear_layer_visibility_override", "set_layer_opacity", "set_layer_transform", "clear_layer_transform_override", "visibility_override_enabled", "override-capable layer", "transform_override_enabled", "get_spawn_area_ids", "get_active_spawn_area_index", "get_spawn_area_info", "get_active_spawn_area_info", "next_spawn_area", "previous_spawn_area", "get_active_spawn_area_index %d was outside %d authored spawn areas", "get_spawn_area_info(%d) returned an empty Dictionary", "get_active_spawn_area_info id %d did not match active spawn id %d", "next_spawn_area moved active index to %d instead of %d", "previous_spawn_area restored active index to %d instead of %d", "_validate_spawn_area_info", "_vector_is_finite", "basis_x", "basis_y", "basis_z", "raw_position", "raw_rotation", "raw_rotation_w", "scale", "basis_x/basis_y were not orthogonal", "converted basis did not preserve right-handed orientation", "transform scale was not positive", "allow_translation", "locomotion", "set_volume", "get_volume", "skip_forward", "skip_back", "pause()", "play()", "toggle_pause()", "restart()", "queue_render_camera_transform", "get_render_diagnostics", "get_render_backend_diagnostics", "vulkan_adapter_candidate", "last_projection_size", "adapter_graphics_initialized_count", "adapter_before_render_count", "adapter_after_render_count", "adapter_last_viewport_width", "camera %d was not auto-registered by ImmViewer", "IMM Godot smoke test passed"]:
        if token not in smoke_runner:
            raise RuntimeError(f"Godot smoke runner is missing token: {token}")
    if "viewer.register_render_camera(CAMERA_ID)" in smoke_runner or "viewer.unregister_render_camera(CAMERA_ID)" in smoke_runner:
        raise RuntimeError("Godot smoke runner should validate ImmViewerNode-owned camera registration, not register cameras itself")

    for token in ["runtime_dependencies", "Audio360.dll", "opus.dll", "opusenc.dll", "vorbisenc.dll", "zlib1.dll", "jpeg62.dll", "libpng16.dll", "ogg.dll", "vorbis.dll", "Godot runtime dependency DLLs are missing"]:
        if token not in sconstruct:
            raise RuntimeError(f"SConstruct is missing Godot runtime dependency staging token: {token}")

    for token in ["Build Godot GDExtension", "-BootstrapGodotCpp", "-BuildGodotCpp", "GODOT_CPP_REF", "GODOT_VERSION", "godot-4.5-stable", "4.5-stable", "Cache godot-cpp", "thirdparty/godot-cpp", "Download Godot", "IMM_CI_ENABLE_GPU_SMOKE", "Capture Windows DirectX standalone baseline", "Smoke Windows Vulkan standalone viewer", "ImmViewerVulkanSmokeLogs-Windows", "Check Godot GDExtension staging", "Smoke Windows Godot Vulkan playback", "IMM_GODOT_TRACE_INTERMEDIATE_TEXTURE", "-Configuration Release", "-RequireExtension -PreflightOnly -LogDir artifacts\\godot-extension-preflight", "Upload Godot smoke logs", "ImmGodotSmokeLogs-Windows", "Upload Windows Godot GDExtension platform binaries", "Internal-ImmGodotGDExtension-Windows-platform", "Package Windows Viewer Artifact", "ImmViewer-Windows", "settings-vulkan-sample1.json", "settings-vr.json"]:
        if token not in workflow:
            raise RuntimeError(f"Windows workflow is missing Godot GDExtension build token: {token}")
    for token in ["Smoke macOS Metal standalone viewer", "IMM_METAL_VALIDATE_EXPECTED_VALUES=0", "Download Godot for macOS", "Smoke macOS Godot Metal visual playback", "ImmGodotSmokeLogs-macOS", "ImmViewer-macOS"]:
        if token not in workflow:
            raise RuntimeError(f"macOS workflow is missing Metal/Godot coverage token: {token}")
    for token in ["build-windows", "build-android", "Internal-ImmGodotGDExtension-*-platform", "Internal-ImmGodotGDExtension-", "test -f \"$addon/bin/windows/release/imm_godot_extension.dll\"", "test -f \"$addon/bin/android/debug/libimm_godot_extension.arm64.so\""]:
        if token not in workflow:
            raise RuntimeError(f"Godot packaging workflow is missing platform merge token: {token}")
    for token in ["Internal-ImmStrokeReaderPlugin-Windows", "Internal-ImmStrokeReaderPlugin-Android", "Internal-ImmStrokeReaderPlugin-macOS", "Internal-ImmStrokeReaderPlugin-iOS", "Internal-ImmViewerPlugin-Windows", "Internal-ImmViewerPlugin-Android", "Internal-ImmViewerPlugin-macOS", "Internal-ImmViewerPlugin-iOS"]:
        if token not in workflow:
            raise RuntimeError(f"Unity packaging workflow is missing internal platform artifact token: {token}")
    for token in ["Stage Android Viewer APKs", "Upload Android Viewer APKs", "Verify Android APK names", "ImmViewer-Windows.zip", "ImmViewer-macOS.zip", "ImmPlayerPlugin-Unity", "ImmStrokeReaderPlugin-Unity", "ImmPlayerPlugin-Godot", "ImmPlayerPlugin-Godot.zip", "release-assets/ImmViewer-Android/ImmViewer-Android-Vulkan.apk"]:
        if token not in workflow:
            raise RuntimeError(f"Release workflow is missing supported viewer artifact token: {token}")
    for stale_text in ["name: ImmViewerMetal-macOS", "ImmViewerMetal-macOS.zip", "name: ImmViewerPlugin-UPM", "ImmViewerPlugin-UPM.zip", "name: ImmStrokeReaderPlugin-UPM", "ImmStrokeReaderPlugin-UPM.zip", "name: ImmGodotGDExtension", "release-assets/ImmGodotGDExtension", "ImmGodotGDExtension.zip"]:
        if stale_text in workflow:
            raise RuntimeError(f"Workflow still publishes an old final artifact name: {stale_text}")
    for stale_text in ["name: ImmViewer-Android-VR", "name: ImmViewer-Android-Vulkan", "release-assets/ImmViewer-Android-VR", "release-assets/ImmViewer-Android-Vulkan"]:
        if stale_text in workflow:
            raise RuntimeError(f"Workflow still publishes a split Android viewer artifact: {stale_text}")
    for stale_text in ["name: ImmViewerVulkan-Windows", "name: ImmViewer-Windows-VR", "ImmViewerVulkan-Windows.zip", "ImmViewer-Windows-VR.zip", "release-assets/ImmViewerVulkan-Windows", "release-assets/ImmViewer-Windows-VR"]:
        if stale_text in workflow:
            raise RuntimeError(f"Workflow still publishes a split Windows viewer artifact: {stale_text}")
    for stale_text in ["name: ImmStrokeReaderPlugin-Windows", "name: ImmStrokeReaderPlugin-Android", "name: ImmStrokeReaderPlugin-macOS", "name: ImmStrokeReaderPlugin-iOS", "name: ImmViewerPlugin-Windows", "name: ImmViewerPlugin-Android", "name: ImmViewerPlugin-macOS", "name: ImmViewerPlugin-iOS", "name: ImmGodotGDExtension-Windows-platform", "name: ImmGodotGDExtension-Android-platform", "name: ImmGodotGDExtension-macOS-platform", "pattern: ImmGodotGDExtension-*-platform"]:
        if stale_text in workflow:
            raise RuntimeError(f"Workflow still exposes an intermediate artifact without the Internal prefix: {stale_text}")
    for stale_text in ["Run Godot script smoke test", "-LogDir artifacts\\godot-smoke-script"]:
        if stale_text in workflow:
            raise RuntimeError(f"Windows workflow still runs the removed script-stub Godot smoke path: {stale_text}")
    for stale_text in ["Run Godot native smoke test", "-RequireExtension -LoadUnloadCycles 2 -LogDir artifacts\\godot-smoke-native"]:
        if stale_text in workflow:
            raise RuntimeError(f"Windows workflow still treats native Godot rendering smoke as a CI gate: {stale_text}")
    for token in ["- '**'", "contains(github.event.head_commit.message, '[CI BUILD]')", "Opt-in branch builds must not push generated binaries back", "github.ref == 'refs/heads/main' || github.ref == 'refs/heads/develop'"]:
        if token not in workflow:
            raise RuntimeError(f"Windows workflow is missing branch opt-in CI token: {token}")

    readme = (ROOT / "code/appImmGodotGDExtension/README.md").read_text(encoding="utf-8")
    if "callbacks are currently no-op placeholders" in readme:
        raise RuntimeError("GDExtension README still describes render adapter callbacks as no-op placeholders")
    for stale_text in ["not yet buildable", "future binary location", "future render callback", "still does not prove visible Metal rendering", "now attempts to build an `MTLRenderPassDescriptor`", "RenderingServer.call_on_render_thread"]:
        if stale_text in readme:
            raise RuntimeError(f"GDExtension README still contains stale status text: {stale_text}")
    for token in ["builds locally", "visual smoke path", "Godot-created intermediate texture", "final composite into scene color", "verifies non-background content pixels"]:
        if token not in readme:
            raise RuntimeError(f"GDExtension README is missing current status token: {token}")

    print("Windows Godot build/smoke wiring ok", flush=True)


def verify_runtime_dependency_sources() -> None:
    missing = [
        f"{name} => {path.relative_to(ROOT)}"
        for name, path in GODOT_RUNTIME_DEPENDENCIES.items()
        if not path.exists()
    ]
    if missing:
        raise RuntimeError("Godot runtime dependency sources are missing: " + "; ".join(missing))

    print(f"Godot runtime dependency sources ok ({len(GODOT_RUNTIME_DEPENDENCIES)} DLLs)", flush=True)


def verify_shared_engine_bridge() -> None:
    bridge = (ROOT / "code/appImmShared/src/imm_engine_bridge.cpp").read_text()
    for token in ["Sound backend unavailable; continuing without audio.", "Sound backend init failed; continuing without audio.", "Failed to create fallback null SoundBackend.", "Failed to initialize fallback null SoundBackend.", "piSoundEngineBackend::API::Null"]:
        if token not in bridge:
            raise RuntimeError(f"Shared IMM engine bridge is missing audio fallback token: {token}")
    print("Shared IMM engine bridge audio fallback ok", flush=True)


def verify_powershell_syntax() -> None:
    powershell = shutil.which("pwsh") or shutil.which("powershell")
    if powershell is None:
        print("PowerShell not found; skipping PowerShell AST syntax validation", flush=True)
        return

    scripts = [
        WINDOWS_BUILD_HELPER,
        WINDOWS_SMOKE_HELPER,
    ]
    script_literals = "\n".join(f"    '{str(script).replace(chr(39), chr(39) + chr(39))}'" for script in scripts)
    command = [
        powershell,
        "-NoProfile",
        "-Command",
        rf"""
$ErrorActionPreference = "Stop"
$hadErrors = $false
$paths = @(
{script_literals}
)
foreach ($path in $paths) {{
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors -and $errors.Count -gt 0) {{
        $hadErrors = $true
        foreach ($errorRecord in $errors) {{
            Write-Error "${{path}}:$($errorRecord.Extent.StartLineNumber):$($errorRecord.Extent.StartColumnNumber): $($errorRecord.Message)"
        }}
    }}
}}
if ($hadErrors) {{
    exit 1
}}
""",
    ]
    run(command)
    print(f"PowerShell AST syntax ok ({len(scripts)} scripts)", flush=True)


def run(command: list[str], cwd: Path = ROOT) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def find_godot_cpp_checkout() -> Path | None:
    candidates: list[Path] = []
    env_path = os.environ.get("GODOT_CPP_PATH")
    if env_path:
        candidates.append(Path(env_path))
    candidates.append(ROOT / "thirdparty/godot-cpp")

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def verify_godot_cpp_syntax(clang: str | None) -> None:
    if clang is None:
        print("clang++ not found; skipping GDExtension godot-cpp syntax-only compile")
        return

    godot_cpp = find_godot_cpp_checkout()
    if godot_cpp is None:
        print("godot-cpp checkout not found; skipping GDExtension godot-cpp syntax-only compile")
        return

    required_headers = [
        godot_cpp / "include/godot_cpp/core/class_db.hpp",
        godot_cpp / "gen/include/godot_cpp/classes/camera3d.hpp",
        godot_cpp / "gen/include/godot_cpp/classes/compositor_effect.hpp",
        godot_cpp / "gen/include/godot_cpp/classes/render_data.hpp",
        godot_cpp / "gen/include/godot_cpp/classes/render_scene_buffers_rd.hpp",
        godot_cpp / "gen/include/godot_cpp/classes/rendering_server.hpp",
        godot_cpp / "gdextension/gdextension_interface.h",
    ]
    missing_headers = [path for path in required_headers if not path.exists()]
    if missing_headers:
        print(
            "godot-cpp generated headers not found; skipping GDExtension godot-cpp syntax-only compile: "
            + ", ".join(str(path) for path in missing_headers),
            flush=True,
        )
        return

    base_command = [
        clang,
        "-std=c++17",
        "-DTYPED_METHOD_BIND",
        f"-I{godot_cpp / 'include'}",
        f"-I{godot_cpp / 'gen/include'}",
        f"-I{godot_cpp / 'gdextension'}",
        "-Icode",
        "-I.",
        "-fsyntax-only",
    ]
    for source in GODOT_EXTENSION_SOURCES:
        run(base_command + [str(source.relative_to(ROOT))])


def env_flag_enabled(name: str) -> bool:
    value = os.environ.get(name, "")
    return value.lower() in {"1", "true", "yes", "on"}


def find_godot_executable() -> Path | None:
    env_path = os.environ.get("GODOT_EXE")
    if env_path and Path(env_path).exists():
        return Path(env_path).resolve()

    for name in ["godot", "godot4", "Godot"]:
        path = shutil.which(name)
        if path:
            return Path(path).resolve()

    if platform.system() == "Darwin":
        for candidate in [
            Path("/Applications/Godot.app/Contents/MacOS/Godot"),
            Path("/Applications/Godot_mono.app/Contents/MacOS/Godot"),
        ]:
            if candidate.exists():
                return candidate

    return None


def verify_godot_script_smoke() -> None:
    if not env_flag_enabled("IMM_GODOT_RUN_LOCAL_SMOKE"):
        print("IMM_GODOT_RUN_LOCAL_SMOKE is not set; skipping local Godot script smoke")
        return

    godot = find_godot_executable()
    if godot is None:
        raise RuntimeError("IMM_GODOT_RUN_LOCAL_SMOKE is set, but no Godot executable was found")

    sample_project = ROOT / "code/ImmGodotSampleProject"
    command = [
        str(godot),
        "--headless",
        "--path",
        str(sample_project),
        "--script",
        "res://scripts/smoke_test_runner.gd",
    ]
    print("+ " + " ".join(command), flush=True)
    env = os.environ.copy()
    env["IMM_GODOT_EXPECT_NATIVE"] = "1"
    env["IMM_GODOT_SMOKE_SCENE"] = "res://scenes/SampleScene.tscn"
    result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout, end="", flush=True)
    if result.returncode != 0 or "IMM Godot smoke test passed" not in result.stdout:
        raise RuntimeError(f"Local Godot script smoke failed with exit code {result.returncode}")


def main() -> int:
    extension_dir = ROOT / "code/appImmGodotGDExtension"
    entry_symbol = verify_manifest()
    verify_project_renderer()
    verify_registration(entry_symbol)
    verify_c_abi_exports()
    verify_native_method_bindings()
    verify_render_thread_queue()
    verify_godot_scenes()
    verify_windows_build_wiring()
    verify_runtime_dependency_sources()
    verify_shared_engine_bridge()
    verify_powershell_syntax()
    run([sys.executable, str(extension_dir / "verify_sample_api.py")])
    run(
        [
            sys.executable,
            "-m",
            "py_compile",
            str(extension_dir / "SConstruct"),
            str(extension_dir / "verify_sample_api.py"),
            str(extension_dir / "verify_local.py"),
        ]
    )

    clang = shutil.which("clang++")
    if clang is None:
        print("clang++ not found; skipping appImmGodot syntax-only compile")
        verify_godot_cpp_syntax(clang)
        verify_godot_script_smoke()
        return 0

    run(
        [
            clang,
            "-std=c++17",
            "-Icode",
            "-Ithirdparty/ogg/include",
            "-Ithirdparty/ogg/libvorbis-1.3.5/include",
            "-Ithirdparty/ogg/libvorbis-1.3.5/include/vorbis",
            "-Ithirdparty/ogg/libvorbis-1.3.5/lib",
            "-Ithirdparty/lpng1637",
            "-Ithirdparty/libjpeg-turbo/libjpeg-turbo",
            "-Ithirdparty/libjpeg-turbo/libjpeg-turbo/include",
            "-fsyntax-only",
            "code/appImmGodot/src/main.cpp",
        ]
    )
    run(
        [
            clang,
            "-std=c++17",
            "-Icode",
            "-Ithirdparty/ogg/include",
            "-Ithirdparty/ogg/libvorbis-1.3.5/include",
            "-Ithirdparty/ogg/libvorbis-1.3.5/include/vorbis",
            "-Ithirdparty/ogg/libvorbis-1.3.5/lib",
            "-Ithirdparty/lpng1637",
            "-Ithirdparty/libjpeg-turbo/libjpeg-turbo",
            "-Ithirdparty/libjpeg-turbo/libjpeg-turbo/include",
            "-fsyntax-only",
            "code/appImmShared/src/imm_engine_bridge.cpp",
        ]
    )
    verify_godot_cpp_syntax(clang)
    verify_godot_script_smoke()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
