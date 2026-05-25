#!/usr/bin/env python3
"""Local verifier for the IMM Godot bridge scaffolding."""

from __future__ import annotations

import os
import py_compile
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
REQUIRED_GODOT_DLLS = [
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


def read_text(relative_path: str) -> str:
    return (REPO_ROOT / relative_path).read_text(encoding="utf-8")


def require_tokens(relative_path: str, tokens: list[str]) -> None:
    text = read_text(relative_path)
    missing = [token for token in tokens if token not in text]
    if missing:
        raise AssertionError(f"{relative_path} is missing tokens: {missing}")


def run(command: list[str], *, env: dict[str, str] | None = None) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=REPO_ROOT, env=env, check=True)


def powershell_exe() -> str | None:
    return shutil.which("pwsh") or shutil.which("powershell")


def verify_c_abi_header_syntax() -> None:
    header_path = REPO_ROOT / "code" / "appImmGodot" / "src" / "imm_godot_plugin.h"
    checks = [
        (
            shutil.which("clang"),
            [shutil.which("clang") or "", "-x", "c", "-std=c11", "-fsyntax-only"],
            "c",
            "int main(void) { ImmGodotSpawnArea area; area.type = IMM_GODOT_SPAWN_AREA_EYE_LEVEL; area.volume.type = IMM_GODOT_SPAWN_VOLUME_BOX; return (int)area.type; }\n",
        ),
        (
            shutil.which("clang++"),
            [shutil.which("clang++") or "", "-x", "c++", "-std=c++17", "-fsyntax-only"],
            "cpp",
            "int main() { ImmGodotSpawnArea area = {}; area.type = IMM_GODOT_SPAWN_AREA_FLOOR_LEVEL; area.volume.type = IMM_GODOT_SPAWN_VOLUME_SPHERE; return (int)area.type; }\n",
        ),
    ]

    for compiler, command, suffix, body in checks:
        if compiler is None:
            print(f"{command[0] or suffix} not found; skipping {suffix} ABI header syntax validation")
            continue

        with tempfile.NamedTemporaryFile("w", suffix=f".{suffix}", delete=False, encoding="utf-8") as source:
            source.write(f'#include "{header_path}"\n')
            source.write(body)
            source_path = source.name
        try:
            run(command + [source_path])
        finally:
            Path(source_path).unlink(missing_ok=True)


def verify_static_contracts() -> None:
    require_tokens("code/appImmGodot/src/imm_godot_plugin.h", [
        "#include <stdbool.h>",
        "defined(IMMGODOT_BUILD)",
        "__declspec(dllimport)",
        "#ifdef __cplusplus",
        "IMM_GODOT_SPAWN_AREA_EYE_LEVEL",
        "IMM_GODOT_SPAWN_AREA_FLOOR_LEVEL",
        "IMM_GODOT_SPAWN_VOLUME_SPHERE",
        "IMM_GODOT_SPAWN_VOLUME_BOX",
        "typedef struct ImmGodotViewport",
        "} ImmGodotViewport;",
        "typedef struct ImmGodotRenderAdapter",
        "} ImmGodotRenderAdapter;",
        "typedef struct ImmGodotSpawnArea",
        "} ImmGodotSpawnArea;",
        "char name[256];",
        "uint32_t type;",
        "ImmGodotRenderAdapter",
        "ImmGodotPlayerInfo",
        "ImmGodot_GetPlayerInfo",
        "ImmGodotDocumentState",
        "ImmGodot_GetDocumentState",
        "ImmGodotBounds",
        "ImmGodot_GetBoundingBox",
        "ImmGodot_SetRenderAdapter",
        "ImmGodot_SetMatrixDebugLogging",
        "ImmGodot_SetCameraMatrices",
        "ImmGodot_RenderCamera",
    ])
    require_tokens("code/appImmGodot/appImmGodot.vcxproj", [
        "IMMGODOT_BUILD;%(PreprocessorDefinitions)",
    ])
    require_tokens("code/appImmShared/src/imm_engine_bridge.cpp", [
        "allocatedLogFileName",
        "std::free(allocatedLogFileName)",
        "apiNameWide",
        "std::free(apiNameWide)",
        "IMM_GODOT_NATIVE_CAPTURE_PATH",
        "CreateRenderTarget",
        "GetTextureContent",
        "imm_godot_capture_color",
        "DettachSamplers",
    ])
    require_tokens("code/appImmGodot/src/main.cpp", [
        "gRenderAdapter",
        "ImmGodot_GetPlayerInfo(ImmGodotPlayerInfo *info)",
        "ImmGodot_GetDocumentState(int id, ImmGodotDocumentState *state)",
        "ImmGodot_GetBoundingBox(int id, ImmGodotBounds *bounds)",
        "wchar_t *wideFileName = pistr2ws(fileName)",
        "std::free(wideFileName)",
        "if (info == nullptr)",
        "if (state == nullptr)",
        "if (bounds == nullptr)",
        "ImmGodot_LoadFromFile(const char *fileName)",
        "ImmGodot_SetDocumentToWorld(int id, const float *doc2world)",
        "if (!iBackendReady() || doc2world == nullptr)",
        "extern \"C\" IMMGODOT_EXPORT void ImmGodot_SetCameraMatrices",
        "extern \"C\" IMMGODOT_EXPORT int ImmGodot_RenderCamera",
        "iLogSubmittedMatrix",
        "ImmGodot_SetMatrixDebugLogging",
        "beforeRenderCamera",
        "afterRenderCamera",
        "IMM_GODOT_SPAWN_AREA_FLOOR_LEVEL",
        "IMM_GODOT_SPAWN_VOLUME_SPHERE",
        "IMM_GODOT_SPAWN_VOLUME_BOX",
        "std::strncpy(serializedSpawnArea->name",
        "std::free(spawnAreaName)",
    ])
    require_tokens("code/appImmGodotGDExtension/src/imm_viewer_node.cpp", [
        "set_document_transform",
        "ImmGodot_SetDocumentToWorld",
        "last_document_to_world",
        "get_background_color",
        "get_document_state",
        "get_bounding_box",
        "bounding_box_valid",
        "bounding_box_min",
        "bounding_box_max",
        "document_loading_state",
        "document_playback_state",
        "get_spawn_area_info",
        "set_active_spawn_area_index",
        "get_active_spawn_area_id",
        "ImmGodot_GetSpawnAreaInfo",
        "set_camera_matrices",
        "set_matrix_debug_logging",
        "get_render_diagnostics",
        'globalize_path("user://imm_godot_log.txt")',
        'globalize_path("user://")',
        'globalize_path("res://" + path)',
        "submit_camera_matrices",
        "copy_matrix",
        "render_adapter_before_camera",
    ])
    require_tokens("code/ImmGodotSampleProject/scripts/smoke_test_runner.gd", [
        "IMM_GODOT_REQUIRE_EXTENSION",
        "IMM_GODOT_SMOKE_DOCUMENT",
        "viewer.global_work(true)",
        "document_path_override",
        "NativeSmokeScene.tscn",
        "IMM_GODOT_MATRIX_DIAGNOSTICS_JSON",
        "IMM Godot smoke lifecycle cycles",
        "lifecycle_cycles := 2",
        "render_camera(1, Vector2i(1600, 900), 0)",
        '"camera_id"',
        "get_document_state",
        "get_bounding_box",
        "bounding_box_valid",
        "bounding_box_min",
        "bounding_box_max",
        "document_loading_state",
        "document_playback_state",
        "document_name",
        "document_size_bytes",
        "godot_version",
        "Engine.get_version_info",
        "rendering_method",
        "gl_compatibility",
        "_get_file_size",
        "background_color",
        "get_background_color",
        "get_spawn_area_info",
        "set_active_spawn_area_index",
        "spawn_area_count",
        "document_to_world",
        "set_document_transform",
        "set_camera_matrices",
        "matrix_debug_logging",
        "get_document_path",
        "_resolve_project_path",
        "FileAccess.file_exists",
        "_wait_for_loaded_document",
    ])
    require_tokens("code/libImmCore/src/libRender/metal/piMetal_Renderer.mm", [
        "iCreateOwnedCommandBuffer",
        "iReleaseOwnedCommandBuffer",
        "[[queue commandBuffer] retain]",
        "iReleaseOwnedCommandBuffer(commandBuffer)",
    ])
    require_tokens("code/ImmGodotSampleProject/scripts/imm_viewer_node.gd", [
        "var _spawn_area_ids := PackedInt32Array()",
        "_spawn_area_ids = PackedInt32Array()",
        "func get_spawn_area_ids() -> PackedInt32Array:",
    ])
    require_tokens("code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs", [
        "IMM_UNITY_MATRIX_DIAGNOSTICS_JSON",
        "Application.unityVersion",
        "unity_version",
        "GetLastMatrixDiagnosticsJson",
        "LogLastMatrixDiagnostics",
        "LogDeterministicMatrixDiagnostics",
        "CapturePlayerParityDiagnostics",
        "CaptureDocumentIdentityDiagnostics",
        "document_to_world",
        "document_name",
        "document_size_bytes",
        "background_color",
        "bounding_box_min",
        "spawn_area_count",
        "MatrixDiagnostics",
    ])
    require_tokens("code/ImmUnitySampleProject/Assets/Scripts/ImmPlayerExample.cs", [
        "LogDeterministicMatrixDiagnostics(1, true, _currentDocument)",
    ])
    require_tokens("code/ImmUnitySampleProject/Assets/Editor/BuildAutomation.cs", [
        "CaptureDeterministicParityDiagnostics",
        "IMM_UNITY_PARITY_DOCUMENT",
        "LogDeterministicMatrixDiagnostics(1, true, document, documentPath)",
    ])
    require_tokens("code/projects/windows/compare-matrix-diagnostics.py", [
        "imm_unity_matrix_diagnostics_v1",
        "imm_godot_matrix_diagnostics_v1",
        "require_schema",
        "compare_optional_float_array",
        "compare_optional_scalar",
        "ENGINE_VERSION_FIELDS",
        "write_summary",
        "--require-extended",
        "--summary-json",
        "unity_version",
        "godot_version",
        "document_to_world",
        "document_name",
        "document_size_bytes",
        "background_color",
        "bounding_box_min",
        "spawn_area_count",
        "camera_id",
    ])
    require_tokens("code/projects/windows/build-godot-extension.ps1", [
        "godot-extension-dlls.txt",
        "Verified staged Godot DLL set",
        "Resolve-MsBuildPath",
        "Test-CppToolchainForMsBuild",
        "MSBUILD_EXE_PATH",
        "vswhere.exe",
        "Using MSBuild:",
        "$configurationLower = $Configuration.ToLowerInvariant()",
        "configuration=$configurationLower",
        "[string]$SummaryDir",
        "Write-GodotBuildSummary",
        "godot-build-summary.txt",
        "godot-build-summary.json",
        "ConvertTo-Json -Depth 6",
        "Resolve-ExecutablePath",
        "Using SCons:",
        "scons_path = $SConsPath",
        "git_path = $GitPath",
        "$Label executable was not found",
        "Audio360.dll",
        "vorbisenc.dll",
    ])
    require_tokens("code/appImmGodotGDExtension/SConstruct", [
        "extension_root = os.path.dirname(os.path.abspath(script_path))",
        "repo_root = os.path.abspath(os.path.join(extension_root, \"..\", \"..\"))",
        "godot_cpp_include_dirs",
        "required_godot_cpp_files",
        "gdextension_interface.h",
        "node3d.hpp",
        "transform3d.hpp",
        "Build/bootstrap godot-cpp first",
        "Glob(os.path.join(extension_root, \"src\", \"*.cpp\"))",
        "Could not find GDExtension C++ sources",
    ])
    require_tokens("code/projects/windows/run-godot-smoke.ps1", [
        "IMM_GODOT_REQUIRE_EXTENSION",
        "IMM_GODOT_SMOKE_DOCUMENT",
        "[string]$DocumentPath",
        "DocumentPath=$resolvedDocumentPath",
        "document_path = $resolvedDocumentPath",
        "imm_godot_log.txt",
        "godot-smoke-summary.json",
        "godot-matrix-diagnostics.json",
        "imm_viewer.gdextension",
        "NativeSmokeScene.tscn",
        "project_artifacts",
        "ConvertFrom-Json",
        "imm_godot_matrix_diagnostics_v1",
        "MatrixDiagnosticsSchema",
        "MatrixDiagnosticsCameraId",
        "MatrixDiagnosticsDocumentName",
        "MatrixDiagnosticsDocumentSizeBytes",
        "GodotVersion",
        "HasLifecycleMarker",
        "has_lifecycle_marker",
        "diagnosticsError",
        "DiagnosticsError",
        "missingRuntimeDlls",
        "missing_runtime_dlls",
        'phase = "preflight"',
        "DocumentLoadingState",
        "DocumentPlaybackState",
        "BoundingBoxValid",
        "Assert-FloatArrayClose",
        "expectedDocumentToWorld",
        "expectedWorldToHead",
        "expectedProjection",
        "DocumentToWorldMaxDelta",
        "WorldToHeadMaxDelta",
        "ProjectionMaxDelta",
        "HasMatrixDiagnostics",
        "NativeLogFiles",
        "bounding_box_min.Count -ne 3",
        "document_loading_state -lt 0",
        "SpawnAreaCount",
        "ActiveSpawnAreaIndex",
        "ActiveSpawnAreaId",
        "spawn_area_count -lt 0",
        "background_color.Count -ne 4",
        "document_to_world.Count -ne 16",
        "document_name",
        "document_size_bytes",
        "Godot native smoke did not print matrix diagnostics.",
        "Godot smoke test did not print the lifecycle coverage marker.",
        "Godot smoke document does not exist",
    ])
    require_tokens("code/projects/windows/capture-unity-parity.ps1", [
        "UNITY_EXE",
        "IMM_UNITY_PARITY_DOCUMENT",
        "finally",
        "Remove-Item Env:\\IMM_UNITY_PARITY_DOCUMENT",
        "[switch]$SyncBuiltPlugins",
        "Write-UnityParitySummary",
        "unity-parity-summary.txt",
        "unity-parity-summary.json",
        'phase = $Phase',
        '-Phase "plugin_sync"',
        '-Phase "unity_batch"',
        '-Phase "diagnostics"',
        '-Phase "complete"',
        "has_diagnostics = $HasDiagnostics",
        "synced_plugins = $SyncedPlugins",
        "unity_version = $UnityVersionValue",
        "UnityVersion=$UnityVersionValue",
        "Unity matrix diagnostics must contain unity_version",
        "ConvertFrom-Json",
        "Synced Unity parity plugin DLLs",
        "ImmUnityPlugin.dll was not found for parity capture plugin sync",
        "Packages\\com.immersive-foundation.imm-unity\\Plugins\\x86_64",
        "CaptureDeterministicParityDiagnostics",
        "IMM_UNITY_MATRIX_DIAGNOSTICS_JSON",
        "unity-matrix-diagnostics.log",
    ])
    require_tokens("code/projects/windows/run-unity-godot-parity.ps1", [
        "run-godot-smoke.ps1",
        "capture-unity-parity.ps1",
        "compare-matrix-diagnostics.py",
        "exampleImmFiles\\sample1.imm",
        "$resolvedDocumentPath = (Resolve-Path $defaultDocumentPath).Path",
        '"-DocumentPath", $resolvedDocumentPath',
        "[switch]$SyncBuiltUnityPlugins",
        "-SyncBuiltPlugins",
        "Write-ParitySummary",
        "unity-godot-parity-summary.txt",
        "unity-godot-parity-summary.json",
        '-Phase "preflight"',
        '-Phase "godot_smoke"',
        '-Phase "unity_capture"',
        '-Phase "comparison"',
        '-Phase "complete"',
        "--require-extended",
        "comparisonExitCode",
        "unity-godot-parity-comparison.log",
        "unity-godot-parity-comparison.json",
        "artifacts\\parity",
    ])
    require_tokens(".github/workflows/build.yml", [
        "GODOT_CPP_REF",
        "unity_exe",
        "parity_document",
        "Verify Godot bridge scaffolding",
        "python code/appImmGodotGDExtension/verify_local.py",
        "Download Godot",
        "Run Godot native smoke",
        '$parityDocument = (Resolve-Path "exampleImmFiles\\sample1.imm").Path',
        '$godotArgs += @("-DocumentPath", $parityDocument)',
        "Run Unity/Godot parity diagnostics",
        "run-unity-godot-parity.ps1",
        '$parityArgs += @("-DocumentPath", $parityDocument)',
        "-SyncBuiltUnityPlugins",
        "-RequireExtension",
        "artifacts\\parity",
        "unity-godot-parity-summary.json",
        "artifacts\\godot-build",
        "build-godot-extension.log",
        "godot-build-summary.json",
        "-SummaryDir artifacts\\godot-build",
        "buildExitCode",
        "ImmGodotBuild-Windows",
        "ImmUnityParity-Windows",
        "ImmGodotSmoke-Windows",
    ])
    verify_native_c_abi_contracts()
    verify_native_string_conversion_ownership()
    verify_godot_method_contracts()
    verify_deterministic_matrix_contracts()
    verify_godot_scene_contracts()
    verify_windows_helper_contracts()
    verify_workflow_contracts()


def verify_native_c_abi_contracts() -> None:
    header = read_text("code/appImmGodot/src/imm_godot_plugin.h")
    implementation = read_text("code/appImmGodot/src/main.cpp")
    declarations = set(re.findall(
        r"\bIMMGODOT_EXPORT\s+[\w:<>*&\s]+?\s+(ImmGodot_\w+)\s*\(",
        header,
        flags=re.MULTILINE,
    ))
    definitions = set(re.findall(
        r'extern\s+"C"\s+IMMGODOT_EXPORT\s+[\w:<>*&\s]+?\s+(ImmGodot_\w+)\s*\(',
        implementation,
        flags=re.MULTILINE,
    ))
    missing_definitions = sorted(declarations - definitions)
    if missing_definitions:
        raise AssertionError(f"main.cpp is missing native C ABI definitions: {missing_definitions}")

    c_abi_reference_declarations = re.findall(
        r"\bIMMGODOT_EXPORT\s+[^;]+&[^;]+;",
        header,
        flags=re.MULTILINE,
    )
    if c_abi_reference_declarations:
        raise AssertionError(f"native C ABI declarations must not use C++ references: {c_abi_reference_declarations}")
    c_abi_reference_definitions = re.findall(
        r'extern\s+"C"\s+IMMGODOT_EXPORT\s+[^{]+&[^{]+\{',
        implementation,
        flags=re.MULTILINE,
    )
    if c_abi_reference_definitions:
        raise AssertionError(f"native C ABI definitions must not use C++ references: {c_abi_reference_definitions}")
    if "const_cast<char *>" in read_text("code/appImmGodotGDExtension/src/imm_viewer_node.cpp"):
        raise AssertionError("ImmViewerNode should pass document paths through the native ABI without const_cast")
    for token in [
        "bool iBackendReady()",
        "return gBridge.IsInitialized();",
        "!iBackendReady() || fileName == nullptr",
        "if (!iBackendReady())",
        "ImmGodot_SetCameraMatrices",
        "ImmGodot_RenderCamera",
        "*state = {}",
        "*bounds = {}",
        "*info = {}",
        "spawnAreaIds == nullptr || spawnAreaIdsSize <= 0",
        "const int writeCount = (num < spawnAreaIdsSize) ? num : spawnAreaIdsSize",
        "*serializedSpawnArea = {}",
        "spawnAreaId < 0 || spawnAreaId >= spawnAreaCount",
    ]:
        if token not in implementation:
            raise AssertionError(f"native Godot spawn-area C ABI is missing guard token: {token}")

    guarded_functions = [
        "ImmGodot_SetCameraMatrices",
        "ImmGodot_RenderCamera",
    ]
    for function_name in guarded_functions:
        match = re.search(
            rf"{function_name}\s*\([^)]*\)\s*\{{(?P<body>.*?)\n\}}",
            implementation,
            flags=re.DOTALL,
        )
        if match is None or "if (!iBackendReady())" not in match.group("body"):
            raise AssertionError(f"{function_name} must guard against an uninitialized backend")


def verify_native_string_conversion_ownership() -> None:
    for relative_path in [
        "code/appImmGodot/src/main.cpp",
        "code/appImmShared/src/imm_engine_bridge.cpp",
    ]:
        text = read_text(relative_path)
        for line_number, line in enumerate(text.splitlines(), start=1):
            if "pistr2ws(" not in line and "piws2str(" not in line:
                continue

            assignment = re.search(r"\*\s*(?P<name>[A-Za-z_]\w*)\s*=.*(?:pistr2ws|piws2str)\(", line)
            if assignment is None:
                raise AssertionError(
                    f"{relative_path}:{line_number} must assign string conversion results to a pointer before use"
                )

            variable_name = assignment.group("name")
            if f"std::free({variable_name})" not in text:
                raise AssertionError(
                    f"{relative_path}:{line_number} converts a string into {variable_name} without std::free({variable_name})"
                )


def verify_godot_method_contracts() -> None:
    smoke_runner = read_text("code/ImmGodotSampleProject/scripts/smoke_test_runner.gd")
    script_stub = read_text("code/ImmGodotSampleProject/scripts/imm_viewer_node.gd")
    native_extension = read_text("code/appImmGodotGDExtension/src/imm_viewer_node.cpp")

    required_match = re.search(r"var required_methods := \[(.*?)\]", smoke_runner, flags=re.DOTALL)
    if required_match is None:
        raise AssertionError("smoke_test_runner.gd must keep an explicit required_methods list")
    if "var spawn_area_ids: Array" in smoke_runner:
        raise AssertionError("smoke_test_runner.gd must not force native PackedInt32Array spawn ids into Array")

    required_methods = set(re.findall(r'"([^"]+)"', required_match.group(1)))
    exercised_methods = set(re.findall(r"\bviewer\.([A-Za-z_]\w*)\s*\(", smoke_runner))
    inherited_engine_methods = {"get_script"}
    smoke_methods = sorted((required_methods | exercised_methods) - inherited_engine_methods)
    script_methods = set(re.findall(r"^func\s+([A-Za-z_]\w*)\s*\(", script_stub, flags=re.MULTILINE))
    native_methods = set(re.findall(r'D_METHOD\("([^"]+)"', native_extension))

    missing_script_methods = sorted(set(smoke_methods) - script_methods)
    missing_native_methods = sorted(set(smoke_methods) - native_methods)
    if missing_script_methods:
        raise AssertionError(f"script ImmViewerNode stub is missing smoke methods: {missing_script_methods}")
    if missing_native_methods:
        raise AssertionError(f"native ImmViewerNode binding is missing smoke methods: {missing_native_methods}")


def parse_float_array(text: str, pattern: str, label: str) -> list[float]:
    match = re.search(pattern, text, flags=re.DOTALL)
    if match is None:
        raise AssertionError(f"Could not find {label}")
    values = [float(token.rstrip("f")) for token in re.findall(r"[-+]?\d+(?:\.\d+)?f?", match.group(1))]
    if len(values) != 16:
        raise AssertionError(f"{label} must contain 16 floats, got {len(values)}")
    return values


def parse_powershell_float_array(text: str, variable_name: str, expected_count: int) -> list[float]:
    pattern = rf"\${variable_name}\s*=\s*@\((.*?)\)"
    match = re.search(pattern, text, flags=re.DOTALL)
    if match is None:
        raise AssertionError(f"Could not find PowerShell ${variable_name} array")
    values = [float(token) for token in re.findall(r"[-+]?\d+(?:\.\d+)?", match.group(1))]
    if len(values) != expected_count:
        raise AssertionError(f"PowerShell ${variable_name} must contain {expected_count} floats, got {len(values)}")
    return values


def assert_matrix_equal(label: str, left: list[float], right: list[float]) -> None:
    deltas = [abs(left_value - right_value) for left_value, right_value in zip(left, right)]
    worst_delta = max(deltas)
    if worst_delta > 1e-6:
        worst_index = deltas.index(worst_delta)
        raise AssertionError(
            f"{label} deterministic matrix drifted at index {worst_index}: "
            f"Godot={left[worst_index]}, Unity={right[worst_index]}"
        )


def verify_deterministic_matrix_contracts() -> None:
    godot_smoke = read_text("code/ImmGodotSampleProject/scripts/smoke_test_runner.gd")
    unity_manager = read_text("code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs")
    windows_smoke = read_text("code/projects/windows/run-godot-smoke.ps1")
    godot_world_to_head = parse_float_array(
        godot_smoke,
        r"var explicit_world_to_head := PackedFloat32Array\(\[(.*?)\]\)",
        "Godot explicit_world_to_head",
    )
    godot_projection = parse_float_array(
        godot_smoke,
        r"var explicit_projection := PackedFloat32Array\(\[(.*?)\]\)",
        "Godot explicit_projection",
    )
    unity_world_to_head = parse_float_array(
        unity_manager,
        r"float\[\] worldToHead\s*=\s*\{(.*?)\};",
        "Unity deterministic worldToHead",
    )
    unity_projection = parse_float_array(
        unity_manager,
        r"float\[\] projection\s*=\s*\{(.*?)\};",
        "Unity deterministic projection",
    )
    windows_document_to_world = parse_powershell_float_array(windows_smoke, "expectedDocumentToWorld", 16)
    windows_world_to_head = parse_powershell_float_array(windows_smoke, "expectedWorldToHead", 16)
    windows_projection = parse_powershell_float_array(windows_smoke, "expectedProjection", 16)
    assert_matrix_equal("world_to_head", godot_world_to_head, unity_world_to_head)
    assert_matrix_equal("projection", godot_projection, unity_projection)
    assert_matrix_equal("windows smoke world_to_head", windows_world_to_head, godot_world_to_head)
    assert_matrix_equal("windows smoke projection", windows_projection, godot_projection)
    assert_matrix_equal("windows smoke document_to_world", windows_document_to_world, [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.5, 0.0, -0.25, 1.0,
    ])


def verify_godot_scene_contracts() -> None:
    sample_scene = read_text("code/ImmGodotSampleProject/scenes/SampleScene.tscn")
    native_scene = read_text("code/ImmGodotSampleProject/scenes/NativeSmokeScene.tscn")
    script_stub = read_text("code/ImmGodotSampleProject/scripts/imm_viewer_node.gd")
    manifest = read_text("code/ImmGodotSampleProject/addons/imm_viewer/imm_viewer.gdextension")
    project_config = read_text("code/ImmGodotSampleProject/project.godot")
    sample_project_root = REPO_ROOT / "code" / "ImmGodotSampleProject"
    expected_document_path = "../../exampleImmFiles/sample1.imm"

    if 'type="ImmViewerNode"' not in native_scene:
        raise AssertionError("NativeSmokeScene.tscn must instantiate native ImmViewerNode")
    if 'path="res://scripts/imm_viewer_node.gd"' not in sample_scene:
        raise AssertionError("SampleScene.tscn must remain script-stub smoke scene")
    if "class_name ImmViewerNode" in script_stub:
        raise AssertionError("script stub must not shadow the native ImmViewerNode class_name")
    if 'entry_symbol="imm_godot_library_init"' not in manifest:
        raise AssertionError("gdextension manifest must use the native imm_godot_library_init entry point")
    if 'compatibility_minimum="4.2"' not in manifest:
        raise AssertionError("gdextension manifest must stay aligned with the Godot 4.2 CI target")
    if 'windows.debug.x86_64="res://bin/windows/debug/imm_godot_extension.dll"' not in manifest:
        raise AssertionError("gdextension manifest must point at the staged Windows debug DLL")
    if 'windows.release.x86_64="res://bin/windows/release/imm_godot_extension.dll"' not in manifest:
        raise AssertionError("gdextension manifest must point at the staged Windows release DLL")
    if 'renderer/rendering_method="gl_compatibility"' not in project_config:
        raise AssertionError("Godot sample project must use the Compatibility renderer path")
    for label, text in [
        ("SampleScene.tscn", sample_scene),
        ("NativeSmokeScene.tscn", native_scene),
        ("imm_viewer_node.gd", script_stub),
    ]:
        if expected_document_path not in text:
            raise AssertionError(f"{label} must use {expected_document_path} for the default sample document")
    if not (sample_project_root / expected_document_path).resolve().is_file():
        raise AssertionError(f"Godot default sample document path does not resolve to an existing file: {expected_document_path}")


def verify_windows_helper_contracts() -> None:
    build_script = read_text("code/projects/windows/build-godot-extension.ps1")
    smoke_script = read_text("code/projects/windows/run-godot-smoke.ps1")
    for dll in REQUIRED_GODOT_DLLS:
        if dll not in build_script:
            raise AssertionError(f"build-godot-extension.ps1 does not stage {dll}")
        if dll not in smoke_script:
            raise AssertionError(f"run-godot-smoke.ps1 does not preflight {dll}")

    require_tokens("code/projects/windows/build-godot-extension.ps1", [
        "ExpectedDllCount=$($requiredDlls.Count)",
        "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))",
        "godot-extension-dlls.txt",
        "Microsoft.Cpp.Default.props",
        "Install Visual Studio Build Tools",
        "Get-FileHash -Algorithm SHA256",
        "SHA256=$($hash.Hash)",
        "Write-GodotBuildSummary -Phase \"toolchain\"",
        "Write-GodotBuildSummary -Phase \"bootstrap\"",
        "Write-GodotBuildSummary -Phase \"msbuild\"",
        "Write-GodotBuildSummary -Phase \"gdextension\"",
        "Write-GodotBuildSummary -Phase \"stage_dependencies\"",
        "Write-GodotBuildSummary -Phase \"complete\" -Status \"passed\"",
    ])
    require_tokens("code/projects/windows/run-godot-smoke.ps1", [
        "[string]$LogDir",
        "godot-smoke-output.log",
        "godot-smoke-summary.txt",
        "godot-smoke-summary.json",
        "ProjectArtifacts=project.godot;imm_viewer.gdextension;NativeSmokeScene.tscn",
        "Write-GodotSmokePreflightSummary",
        "Copy-GodotSmokeProjectArtifacts",
        "Godot executable does not exist",
        "ConvertTo-Json -Depth 8",
        "rendering_method",
        "godot_version",
        "Godot matrix diagnostics must contain godot_version",
        "Godot smoke must run the Compatibility renderer path",
        "passed = $smokePassed",
        "$hasLifecycleMarker",
        "diagnostics_error = $diagnosticsError",
        "godot-matrix-diagnostics.json",
        "Get-FileHash -Algorithm SHA256",
        "SHA256=$($hash.Hash)",
        "world_to_head.Count -ne 16",
        "Godot matrix diagnostics $Name mismatch",
        "Remove-Item Env:\\IMM_GODOT_SMOKE_DOCUMENT",
    ])
    require_tokens("code/appImmGodotGDExtension/verify_local.py", [
        "parse-powershell-syntax.ps1",
        '"-File"',
        '"-ExecutionPolicy"',
    ])
    require_tokens("code/projects/windows/capture-unity-parity.bat", [
        "capture-unity-parity.ps1",
    ])
    require_tokens("code/projects/windows/run-unity-godot-parity.bat", [
        "run-unity-godot-parity.ps1",
    ])
    for batch_path in [
        "code/projects/windows/build-godot-extension.bat",
        "code/projects/windows/run-godot-smoke.bat",
        "code/projects/windows/capture-unity-parity.bat",
        "code/projects/windows/run-unity-godot-parity.bat",
    ]:
        require_tokens(batch_path, [
            "powershell -ExecutionPolicy Bypass -File",
            "exit /b %ERRORLEVEL%",
        ])


def verify_workflow_contracts() -> None:
    workflow = read_text(".github/workflows/build.yml")
    required_tokens = [
        "code/ImmGodotSampleProject/bin/windows/release/*.dll",
        "code/ImmGodotSampleProject/bin/windows/release/godot-extension-dlls.txt",
        "artifacts\\godot-smoke",
        "artifacts\\godot-build",
        "artifacts/parity",
        "if: always()",
        "-SummaryDir artifacts\\godot-build",
        "godot-build-summary.json",
        '"-RequireExtension", "-LogDir", "artifacts\\godot-smoke"',
        '$parityDocument = (Resolve-Path "exampleImmFiles\\sample1.imm").Path',
        '$godotArgs += @("-DocumentPath", $parityDocument)',
        '$parityArgs += @("-DocumentPath", $parityDocument)',
        "unity-matrix-diagnostics.log",
        "unity-godot-parity-summary.json",
        "ImmGodotBuild-Windows",
        "build-godot-extension.log",
    ]
    missing = [token for token in required_tokens if token not in workflow]
    if missing:
        raise AssertionError(f"build workflow is missing Godot smoke artifact tokens: {missing}")
    godot_extension_upload = re.search(
        r"- name: Upload Godot GDExtension DLLs(?P<body>.*?)(?:\n    - name:|\Z)",
        workflow,
        flags=re.DOTALL,
    )
    if godot_extension_upload is None:
        raise AssertionError("build workflow is missing Upload Godot GDExtension DLLs step")
    godot_extension_upload_body = godot_extension_upload.group("body")
    for token in ["if: always()", "if-no-files-found: ignore"]:
        if token not in godot_extension_upload_body:
            raise AssertionError(f"Upload Godot GDExtension DLLs step must include {token}")


def verify_python() -> None:
    py_compile.compile(str(REPO_ROOT / "code/appImmGodotGDExtension/SConstruct"), doraise=True)
    py_compile.compile(str(REPO_ROOT / "code/projects/windows/compare-matrix-diagnostics.py"), doraise=True)
    require_tokens("code/appImmGodotGDExtension/SConstruct", [
        "import glob",
        "configuration must be 'debug' or 'release'",
        "os.path.isfile(imm_godot_lib)",
        "os.path.isfile(imm_godot_runtime)",
        "os.path.isdir(include_dir)",
        "os.path.isfile(required_file)",
        "glob.glob",
        "os.path.isfile(godot_cpp_lib)",
        "Could not find godot-cpp include directory",
        "Could not find required godot-cpp header",
        "os.makedirs(output_dir, exist_ok=True)",
        "env.Depends",
    ])


def verify_powershell_syntax() -> None:
    executable = powershell_exe()
    if executable is None:
        print("PowerShell not found; skipping PowerShell AST syntax validation")
        return

    scripts = [
        "code/projects/windows/build-godot-extension.ps1",
        "code/projects/windows/run-godot-smoke.ps1",
        "code/projects/windows/capture-unity-parity.ps1",
        "code/projects/windows/run-unity-godot-parity.ps1",
    ]
    tmp_dir = REPO_ROOT / "build" / "godot_verify_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)
    parser_path = tmp_dir / "parse-powershell-syntax.ps1"
    parser = r"""
param([string[]]$Paths)
$failed = $false
foreach ($path in $Paths) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) {
        $failed = $true
        foreach ($err in $errors) {
            Write-Error "${path}:$($err.Extent.StartLineNumber):$($err.Extent.StartColumnNumber): $($err.Message)"
        }
    }
}
if ($failed) { exit 1 }
"""
    parser_path.write_text(parser, encoding="utf-8")
    run([executable, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(parser_path), *scripts])


def verify_comparator_fixture() -> None:
    tmp_dir = REPO_ROOT / "build" / "godot_verify_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)
    unity_log = tmp_dir / "unity.log"
    godot_json = tmp_dir / "godot.json"
    summary_json = tmp_dir / "comparison-summary.json"
    unity_log.write_text(
        'IMM_UNITY_MATRIX_DIAGNOSTICS_JSON {"schema":"imm_unity_matrix_diagnostics_v1","camera_id":1,"world_to_head":[1,0,0,0,0,1,0,0,0,0,1,0,0.25,1.6,6,1],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.1,0],"document_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0.5,0,-0.25,1],"document_path":"C:/repo/exampleImmFiles/sample1.imm","document_name":"sample1.imm","document_size_bytes":5831101,"unity_version":"2022.3.0f1","document_loading_state":2,"document_playback_state":0,"background_color":[0.04,0.05,0.08,1],"bounding_box_valid":true,"bounding_box_min":[-1,-2,-3],"bounding_box_max":[4,5,6],"spawn_area_count":1,"active_spawn_area_index":0,"active_spawn_area_id":12}\n',
        encoding="utf-8",
    )
    godot_json.write_text(
        '{"schema":"imm_godot_matrix_diagnostics_v1","camera_id":1,"last_matrix_camera_id":1,"world_to_head":[1,0,0,0,0,1,0,0,0,0,1,0,0.25,1.6,6,1],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.10000000149,0],"document_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0.5,0,-0.25,1],"document_path":"D:/work/exampleImmFiles/sample1.imm","document_name":"sample1.imm","document_size_bytes":5831101,"godot_version":"4.2.2-stable","document_loading_state":2,"document_playback_state":0,"background_color":[0.04,0.05,0.08,1],"bounding_box_valid":true,"bounding_box_min":[-1,-2,-3],"bounding_box_max":[4,5,6],"spawn_area_count":1,"active_spawn_area_index":0,"active_spawn_area_id":12}\n',
        encoding="utf-8",
    )
    run([
        sys.executable,
        "code/projects/windows/compare-matrix-diagnostics.py",
        "--unity",
        str(unity_log),
        "--godot",
        str(godot_json),
        "--require-extended",
        "--summary-json",
        str(summary_json),
    ])
    summary = json.loads(summary_json.read_text(encoding="utf-8"))
    if not summary.get("passed"):
        raise AssertionError("Comparator summary JSON did not report passed=true")
    if "checks" not in summary or not summary["checks"]:
        raise AssertionError("Comparator summary JSON did not include checks")
    if not any(check.get("name") == "document_size_bytes" for check in summary["checks"]):
        raise AssertionError("Comparator summary JSON did not include document identity checks")
    if summary.get("unity_version") != "2022.3.0f1" or summary.get("godot_version") != "4.2.2-stable":
        raise AssertionError("Comparator summary JSON did not preserve engine version fields")
    required_version_checks = {"unity_version.unity", "godot_version.godot"}
    actual_version_checks = {check.get("name") for check in summary["checks"] if check.get("name") in required_version_checks}
    if actual_version_checks != required_version_checks:
        raise AssertionError("Comparator summary JSON did not include required engine version checks")

    mismatch_godot_json = tmp_dir / "godot-mismatch.json"
    mismatch_summary_json = tmp_dir / "comparison-mismatch-summary.json"
    mismatch_godot_json.write_text(
        '{"schema":"imm_godot_matrix_diagnostics_v1","camera_id":1,"last_matrix_camera_id":1,"world_to_head":[2,0,0,0,0,1,0,0,0,0,1,0,0.25,1.6,6,1],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.10000000149,0],"document_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0.5,0,-0.25,1],"document_path":"D:/work/exampleImmFiles/sample1.imm","document_name":"sample1.imm","document_size_bytes":5831101,"godot_version":"4.2.2-stable","document_loading_state":2,"document_playback_state":0,"background_color":[0.04,0.05,0.08,1],"bounding_box_valid":true,"bounding_box_min":[-1,-2,-3],"bounding_box_max":[4,5,6],"spawn_area_count":1,"active_spawn_area_index":0,"active_spawn_area_id":12}\n',
        encoding="utf-8",
    )
    mismatch = subprocess.run([
        sys.executable,
        "code/projects/windows/compare-matrix-diagnostics.py",
        "--unity",
        str(unity_log),
        "--godot",
        str(mismatch_godot_json),
        "--require-extended",
        "--summary-json",
        str(mismatch_summary_json),
    ], cwd=REPO_ROOT, check=False)
    if mismatch.returncode == 0:
        raise AssertionError("Comparator mismatch fixture unexpectedly passed")
    mismatch_summary = json.loads(mismatch_summary_json.read_text(encoding="utf-8"))
    if mismatch_summary.get("passed"):
        raise AssertionError("Comparator mismatch summary JSON unexpectedly reported passed=true")
    if not any((not check.get("ok", True)) for check in mismatch_summary.get("checks", [])):
        raise AssertionError("Comparator mismatch summary JSON did not include a failed check")

    camera_mismatch_godot_json = tmp_dir / "godot-camera-mismatch.json"
    camera_mismatch_summary_json = tmp_dir / "comparison-camera-mismatch-summary.json"
    camera_mismatch_godot_json.write_text(
        '{"schema":"imm_godot_matrix_diagnostics_v1","camera_id":2,"last_matrix_camera_id":2,"world_to_head":[1,0,0,0,0,1,0,0,0,0,1,0,0.25,1.6,6,1],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.10000000149,0],"document_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0.5,0,-0.25,1],"document_path":"D:/work/exampleImmFiles/sample1.imm","document_name":"sample1.imm","document_size_bytes":5831101,"godot_version":"4.2.2-stable","document_loading_state":2,"document_playback_state":0,"background_color":[0.04,0.05,0.08,1],"bounding_box_valid":true,"bounding_box_min":[-1,-2,-3],"bounding_box_max":[4,5,6],"spawn_area_count":1,"active_spawn_area_index":0,"active_spawn_area_id":12}\n',
        encoding="utf-8",
    )
    camera_mismatch = subprocess.run([
        sys.executable,
        "code/projects/windows/compare-matrix-diagnostics.py",
        "--unity",
        str(unity_log),
        "--godot",
        str(camera_mismatch_godot_json),
        "--require-extended",
        "--summary-json",
        str(camera_mismatch_summary_json),
    ], cwd=REPO_ROOT, check=False)
    if camera_mismatch.returncode == 0:
        raise AssertionError("Comparator camera-id mismatch fixture unexpectedly passed")
    camera_mismatch_summary = json.loads(camera_mismatch_summary_json.read_text(encoding="utf-8"))
    if camera_mismatch_summary.get("passed"):
        raise AssertionError("Comparator camera-id mismatch summary JSON unexpectedly reported passed=true")
    if camera_mismatch_summary.get("camera_check", {}).get("ok", True):
        raise AssertionError("Comparator camera-id mismatch summary JSON did not report camera_check.ok=false")

    document_mismatch_godot_json = tmp_dir / "godot-document-mismatch.json"
    document_mismatch_summary_json = tmp_dir / "comparison-document-mismatch-summary.json"
    document_mismatch_godot_json.write_text(
        '{"schema":"imm_godot_matrix_diagnostics_v1","camera_id":1,"last_matrix_camera_id":1,"world_to_head":[1,0,0,0,0,1,0,0,0,0,1,0,0.25,1.6,6,1],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.10000000149,0],"document_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0.5,0,-0.25,1],"document_path":"D:/work/exampleImmFiles/other.imm","document_name":"other.imm","document_size_bytes":99,"godot_version":"4.2.2-stable","document_loading_state":2,"document_playback_state":0,"background_color":[0.04,0.05,0.08,1],"bounding_box_valid":true,"bounding_box_min":[-1,-2,-3],"bounding_box_max":[4,5,6],"spawn_area_count":1,"active_spawn_area_index":0,"active_spawn_area_id":12}\n',
        encoding="utf-8",
    )
    document_mismatch = subprocess.run([
        sys.executable,
        "code/projects/windows/compare-matrix-diagnostics.py",
        "--unity",
        str(unity_log),
        "--godot",
        str(document_mismatch_godot_json),
        "--require-extended",
        "--summary-json",
        str(document_mismatch_summary_json),
    ], cwd=REPO_ROOT, check=False)
    if document_mismatch.returncode == 0:
        raise AssertionError("Comparator document-identity mismatch fixture unexpectedly passed")
    document_mismatch_summary = json.loads(document_mismatch_summary_json.read_text(encoding="utf-8"))
    if document_mismatch_summary.get("passed"):
        raise AssertionError("Comparator document-identity mismatch summary JSON unexpectedly reported passed=true")
    if not any(check.get("name") == "document_size_bytes" and not check.get("ok", True) for check in document_mismatch_summary.get("checks", [])):
        raise AssertionError("Comparator document-identity mismatch summary JSON did not include a failed document_size_bytes check")

    invalid_schema_godot_json = tmp_dir / "godot-invalid-schema.json"
    invalid_schema_summary_json = tmp_dir / "comparison-invalid-schema-summary.json"
    invalid_schema_godot_json.write_text(
        '{"schema":"wrong_schema","camera_id":1,"last_matrix_camera_id":1,"world_to_head":[1,0,0,0,0,1,0,0,0,0,1,0,0.25,1.6,6,1],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.10000000149,0]}\n',
        encoding="utf-8",
    )
    invalid_schema = subprocess.run([
        sys.executable,
        "code/projects/windows/compare-matrix-diagnostics.py",
        "--unity",
        str(unity_log),
        "--godot",
        str(invalid_schema_godot_json),
        "--summary-json",
        str(invalid_schema_summary_json),
    ], cwd=REPO_ROOT, check=False)
    if invalid_schema.returncode == 0:
        raise AssertionError("Comparator invalid-schema fixture unexpectedly passed")
    invalid_schema_summary = json.loads(invalid_schema_summary_json.read_text(encoding="utf-8"))
    if invalid_schema_summary.get("passed") or "error" not in invalid_schema_summary:
        raise AssertionError("Comparator invalid-schema summary JSON did not record a failure error")

    invalid_shape_godot_json = tmp_dir / "godot-invalid-shape.json"
    invalid_shape_summary_json = tmp_dir / "comparison-invalid-shape-summary.json"
    invalid_shape_godot_json.write_text(
        '{"schema":"imm_godot_matrix_diagnostics_v1","camera_id":1,"last_matrix_camera_id":1,"world_to_head":[1,0],"projection":[1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-0.10000000149,0]}\n',
        encoding="utf-8",
    )
    invalid_shape = subprocess.run([
        sys.executable,
        "code/projects/windows/compare-matrix-diagnostics.py",
        "--unity",
        str(unity_log),
        "--godot",
        str(invalid_shape_godot_json),
        "--summary-json",
        str(invalid_shape_summary_json),
    ], cwd=REPO_ROOT, check=False)
    if invalid_shape.returncode == 0:
        raise AssertionError("Comparator invalid-shape fixture unexpectedly passed")
    invalid_shape_summary = json.loads(invalid_shape_summary_json.read_text(encoding="utf-8"))
    if invalid_shape_summary.get("passed") or "error" not in invalid_shape_summary:
        raise AssertionError("Comparator invalid-shape summary JSON did not record a failure error")


def verify_optional_godot_smoke() -> None:
    if os.environ.get("IMM_GODOT_RUN_LOCAL_SMOKE") != "1":
        print("IMM_GODOT_RUN_LOCAL_SMOKE is not set; skipping local Godot smoke")
        return

    godot_exe = os.environ.get("GODOT_EXE", "/Applications/Godot.app/Contents/MacOS/Godot")
    env = os.environ.copy()
    env["IMM_GODOT_SMOKE_DOCUMENT"] = str((REPO_ROOT / "exampleImmFiles" / "sample1.imm").resolve())
    run([
        godot_exe,
        "--headless",
        "--path",
        "code/ImmGodotSampleProject",
        "--script",
        "res://scripts/smoke_test_runner.gd",
    ], env=env)


def main() -> int:
    verify_static_contracts()
    verify_c_abi_header_syntax()
    verify_python()
    verify_powershell_syntax()
    verify_comparator_fixture()
    verify_optional_godot_smoke()
    print("IMM Godot local verifier passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
