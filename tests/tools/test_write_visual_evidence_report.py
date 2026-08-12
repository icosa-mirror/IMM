#!/usr/bin/env python3
"""Focused checks for the combined visual evidence report."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


PNG_1X1 = bytes.fromhex(
    "89504e470d0a1a0a0000000d4948445200000001000000010802000000907753de"
    "0000000c4944415408d763f8ffff3f0005fe02fea73581e20000000049454e44ae426082"
)


def main() -> int:
    from write_visual_evidence_report import (
        VISUAL_MATRIX_SYMBOLS,
        add_color_component_table,
        effective_status,
        required_depth_gaps_for_scope,
        row_required_for_scope,
        slugify,
        visual_matrix_cell,
    )

    assert VISUAL_MATRIX_SYMBOLS["render_passed"] == "🟨"

    assert slugify("Unity macOS Metal Composition") == "unity-macos-metal-composition"
    assert slugify("unity-mac-os-metal-composition") == "unity-macos-metal-composition"
    assert slugify("UnityAndroidVulkan") == "unity-android-vulkan"

    missing_firebase_capture = {
        "passed": False,
        "errors": ["missing Firebase Test Lab native-render-after.ppm"],
    }
    missing_evidence_manifest = {
        "classification": {
            "result": "failed",
            "failure_class": "evidence",
        }
    }
    assert effective_status(
        missing_firebase_capture,
        {},
        missing_evidence_manifest,
    ) == ("evidence_incomplete", "evidence"), (
        "Missing Firebase evidence must not be reported as a rendering failure"
    )

    hardware_only_row = {
        "hosted_gate": "",
        "hardware_gate": "CI Engine Matrix / Unity Windows Vulkan",
    }
    assert not row_required_for_scope(hardware_only_row, "hosted"), (
        "Normal hosted validation must not require a hardware-only lane"
    )
    assert row_required_for_scope(hardware_only_row, "all"), (
        "Hardware validation must still require the hardware-only lane"
    )

    unsupported_ios_unity = {
        "status": "unsupported",
        "hosted_gate": "",
        "hardware_gate": "",
        "matrix": {
            "product": "unity",
            "platform": "ios",
            "mode": "non-vr",
            "renderer": "metal",
        },
    }
    hosted_android_unity = {
        "status": "supported",
        "hosted_gate": "CI Engine Matrix / Unity Android Vulkan",
        "hardware_gate": "",
        "matrix": {
            "product": "unity",
            "platform": "android",
            "mode": "non-vr",
            "renderer": "vulkan",
        },
    }
    platform_placeholders = [
        {
            "status": "unsupported",
            "hosted_gate": "",
            "hardware_gate": "",
            "matrix": {
                "product": "standalone",
                "platform": platform,
                "mode": "non-vr",
                "renderer": "native",
            },
        }
        for platform in ("windows", "macos")
    ]
    assert required_depth_gaps_for_scope(
        [
            unsupported_ios_unity,
            hosted_android_unity,
            *platform_placeholders,
        ],
        {
            ("ios", "unity"): "not_tested",
            ("android", "unity"): "render_passed",
        },
        "hosted",
    ) == [("android", "unity")], (
        "Hosted depth enforcement must fail a hosted gap without requiring "
        "unsupported iOS coverage"
    )

    probe_lines: list[str] = []
    add_color_component_table(
        probe_lines,
        {
            "color_component_probes": {
                "probes": [
                    {
                        "name": "character-occluded-cyan",
                        "passed": False,
                        "matched_pixels": 3315,
                        "matched_pixel_share_of_crop": 0.003597005,
                        "largest_component_pixels": 3315,
                        "largest_component_share_of_crop": 0.003597005,
                        "largest_component_bounds": {
                            "x": 640,
                            "y": 331,
                            "width": 51,
                            "height": 65,
                        },
                    }
                ]
            }
        },
    )
    probe_text = "\n".join(probe_lines)
    assert "| character-occluded-cyan | false | 3315 | 0.003597 | 3315 | 0.003597 |" in probe_text
    assert "x=640, y=331, w=51, h=65" in probe_text

    standalone_row = {
        "matrix": {
            "product": "standalone",
            "platform": "windows",
            "mode": "non-vr",
            "renderer": "directx",
        },
        "status": "supported",
        "coverage_status": "passed",
    }
    assert (
        visual_matrix_cell(
            [standalone_row], Path.cwd(), "standalone", "windows", "directx"
        )
        == "passed"
    ), "A successful standalone render must be green because depth does not apply"

    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        lane = temp / "unity-android-vulkan"
        lane.mkdir()
        matrix_row = {
            "matrix": {
                "product": "unity",
                "platform": "android",
                "mode": "non-vr",
                "renderer": "vulkan",
            },
            "status": "supported",
            "coverage_status": "passed",
        }
        manifest = {
            "classification": {"result": "passed"},
            "matrix": matrix_row["matrix"],
            "files": [{"path": "unity-android-vulkan-render-metrics.json"}],
        }
        (lane / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        assert (
            visual_matrix_cell(
                [matrix_row], temp, "unity", "android", "vulkan"
            )
            == "render_passed"
        ), "Render-only evidence must be yellow, not green, for a required Unity depth cell"
        manifest["files"].append(
            {"path": "unity-android-vulkan-composition-metrics.json"}
        )
        (lane / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        assert (
            visual_matrix_cell(
                [matrix_row], temp, "unity", "android", "vulkan"
            )
            == "depth_passed"
        )

    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        input_root = temp / "input"
        engine = input_root / "EngineValidationEvidence"
        unity_captures = engine / "captures" / "unity-windows-directx-composition"
        unity_captures.mkdir(parents=True)
        (unity_captures / "unity-windows-directx-composition.png").write_bytes(PNG_1X1)
        (engine / "ENGINE_VALIDATION_REPORT.md").write_text(
            "# IMM CI Validation Evidence\n\n## Unity Windows DirectX Composition\n\n"
            "![Unity](captures/unity-windows-directx-composition/unity-windows-directx-composition.png)\n",
            encoding="utf-8",
        )
        (engine / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "passed"},
                    "matrix": {
                        "product": "unity",
                        "platform": "all",
                        "mode": "non-vr",
                        "renderer": "preflight",
                    },
                }
            ),
            encoding="utf-8",
        )
        (engine / "composition-status.json").write_text(
            json.dumps(
                {
                    "rendering": "success",
                    "composition_mode": "full_depth",
                    "composition_contract": "depth_composition",
                    "compositing": "expected_failed",
                    "ordered_overlay": "not_tested",
                    "depth_composition": "expected_failed",
                    "depth_interleaving": "expected_failed",
                    "failure_class": "compositing",
                    "failures": ["scene composition rear occlusion probe failed"],
                }
            ),
            encoding="utf-8",
        )
        unity_vulkan = input_root / "EngineValidationEvidence" / "captures" / "unity-windows-vulkan-ordered-overlay"
        unity_vulkan.mkdir(parents=True)
        (unity_vulkan / "unity-windows-vulkan-ordered-overlay.png").write_bytes(PNG_1X1)
        (unity_vulkan / "composition-status.json").write_text(
            json.dumps(
                {
                    "rendering": "success",
                    "composition_mode": "ordered_overlay",
                    "composition_contract": "ordered_overlay",
                    "compositing": "success",
                    "ordered_overlay": "success",
                    "depth_composition": "not_claimed",
                    "depth_interleaving": "not_claimed",
                    "failure_class": "",
                    "failures": [],
                }
            ),
            encoding="utf-8",
        )
        unity_vulkan_full_depth = input_root / "EngineValidationEvidence" / "captures" / "unity-windows-vulkan-full-depth"
        unity_vulkan_full_depth.mkdir(parents=True)
        (unity_vulkan_full_depth / "unity-windows-vulkan-full-depth.png").write_bytes(PNG_1X1)
        (unity_vulkan_full_depth / "composition-status.json").write_text(
            json.dumps(
                {
                    "rendering": "success",
                    "composition_mode": "full_depth",
                    "composition_contract": "depth_composition",
                    "compositing": "success",
                    "ordered_overlay": "not_tested",
                    "depth_composition": "success",
                    "depth_interleaving": "success",
                    "failure_class": "",
                    "failures": [],
                }
            ),
            encoding="utf-8",
        )
        for generic_name in ["composition", "render", "full-depth", "ordered-overlay"]:
            generic_capture = engine / "captures" / generic_name
            generic_capture.mkdir(parents=True)
            (generic_capture / f"{generic_name}.png").write_bytes(PNG_1X1)
            (generic_capture / "render-report.md").write_text(
                f"# {generic_name}\n\n![capture]({generic_name}.png)\n",
                encoding="utf-8",
            )
        (engine / "composition-render-report.md").write_text(
            "# Composition\n\n![capture](captures/composition/composition.png)\n",
            encoding="utf-8",
        )

        metal = input_root / "UnityMacOSMetalComposition"
        metal_captures = metal / "captures"
        metal_captures.mkdir(parents=True)
        (metal_captures / "unity-macos-metal.png").write_bytes(PNG_1X1)
        (metal / "render-report.md").write_text(
            "# IMM Render Report\n\n![Metal](captures/unity-macos-metal.png)\n",
            encoding="utf-8",
        )
        metal_editor_play = metal_captures / "editor-play"
        metal_editor_play.mkdir()
        (metal_editor_play / "unity-editor-play.png").write_bytes(PNG_1X1)
        (metal / "render-metrics.json").write_text(
            json.dumps(
                {
                    "passed": True,
                    "candidate": {"width": 1, "height": 1},
                    "reference": {"width": 1, "height": 1},
                    "contract": {"minimum_spatial_correlation": 0.4},
                    "spatial_luma_grid": {"mean_abs_delta": 0.01, "correlation": 0.99},
                    "errors": [],
                }
            ),
            encoding="utf-8",
        )
        (metal / "composition-status.json").write_text(
            json.dumps(
                {
                    "result": "composition_failed",
                    "failure_class": "compositing",
                    "rendering": "success",
                    "compositing": "failed",
                    "failures": ["cyan depth leakage"],
                }
            ),
            encoding="utf-8",
        )
        (metal / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "failed", "failure_class": "compositing"},
                    "matrix": {
                        "product": "unity",
                        "platform": "macos",
                        "mode": "non-vr",
                        "renderer": "metal",
                    },
                }
            ),
            encoding="utf-8",
        )

        # Semantically distinct eye files must both survive aggregation even
        # when their bytes are identical; identical content is itself useful
        # failure evidence for a stereo contract.
        stereo_root = input_root / "UnityAndroidVulkan"
        stereo = stereo_root / "captures" / "synthetic-stereo"
        stereo.mkdir(parents=True)
        (stereo / "unity-android-vulkan-synthetic-left.png").write_bytes(PNG_1X1)
        (stereo / "unity-android-vulkan-synthetic-right.png").write_bytes(PNG_1X1)
        (stereo_root / "render-report.md").write_text(
            "# Android stereo\n\n![left](captures/synthetic-stereo/unity-android-vulkan-synthetic-left.png)\n",
            encoding="utf-8",
        )

        gpu = input_root / "GPUMatrixEvidence" / "WindowsGodotVulkan-GPU"
        gpu.mkdir(parents=True)
        (gpu / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "failed", "failure_class": "compositing"},
                    "matrix": {
                        "product": "godot",
                        "platform": "windows",
                        "mode": "non-vr",
                        "renderer": "vulkan",
                    },
                }
            ),
            encoding="utf-8",
        )
        godot_overlay = gpu / "godot-smoke-windows-vulkan-ordered-overlay"
        godot_overlay_captures = godot_overlay / "captures"
        godot_overlay_captures.mkdir(parents=True)
        (godot_overlay_captures / "godot-vulkan-ordered-overlay.png").write_bytes(PNG_1X1)
        (godot_overlay / "render-report.md").write_text(
            "# IMM Render Report\n\n## Captures\n### Candidate\n"
            "![Candidate](captures/godot-vulkan-ordered-overlay.png)\n",
            encoding="utf-8",
        )
        passing_render_metrics = {
            "passed": True,
            "candidate": {
                "path": "artifacts/godot-smoke-windows-vulkan/godot-vulkan-render.ppm",
                "width": 1,
                "height": 1,
            },
            "reference": {"width": 1, "height": 1},
            "contract": {"path": "godot-windows-vulkan-sample1.json"},
            "spatial_luma_grid": {"mean_abs_delta": 0.01, "correlation": 0.99},
            "errors": [],
        }
        failed_overlay_metrics = {
            "passed": False,
            "candidate": {
                "path": "artifacts/godot-smoke-windows-vulkan-ordered-overlay/godot-vulkan-ordered-overlay.ppm",
                "width": 1,
                "height": 1,
            },
            "reference": {"width": 1, "height": 1},
            "contract": {"path": "sample1-ordered-overlay.json"},
            "spatial_luma_grid": {"mean_abs_delta": 0.02, "correlation": 0.98},
            "errors": [
                "color component probe character-occluded-cyan share 0.007910 "
                "exceeds contract maximum 0.000250"
            ],
        }
        (godot_overlay / "godot-vulkan-render-metrics.json").write_text(
            json.dumps(passing_render_metrics),
            encoding="utf-8",
        )
        (godot_overlay / "godot-vulkan-ordered-overlay-metrics.json").write_text(
            json.dumps(failed_overlay_metrics),
            encoding="utf-8",
        )
        (godot_overlay / "composition-status.json").write_text(
            json.dumps(
                {
                    "rendering": "success",
                    "composition_mode": "ordered_overlay",
                    "composition_contract": "ordered_overlay",
                    "compositing": "success",
                    "ordered_overlay": "success",
                    "depth_composition": "not_claimed",
                    "depth_interleaving": "not_claimed",
                    "failure_class": "",
                    "failures": [],
                }
            ),
            encoding="utf-8",
        )
        godot_sample_play = godot_overlay_captures / "sample-play"
        godot_sample_play.mkdir()
        (godot_sample_play / "godot-sample-play.png").write_bytes(PNG_1X1)
        godot_full_depth = input_root / "GPUMatrixEvidence" / "godot-windows-vulkan-full-depth"
        godot_full_depth_captures = godot_full_depth / "captures"
        godot_full_depth_captures.mkdir(parents=True)
        (godot_full_depth_captures / "godot-vulkan-full-depth.png").write_bytes(PNG_1X1)
        (godot_full_depth / "render-report.md").write_text(
            "# IMM Render Report\n\n### godot-vulkan-full-depth.png\n"
            "![Godot](captures/godot-vulkan-full-depth.png)\n",
            encoding="utf-8",
        )
        (godot_full_depth / "composition-status.json").write_text(
            json.dumps(
                {
                    "rendering": "success",
                    "composition_mode": "full_depth",
                    "composition_contract": "depth_composition",
                    "compositing": "success",
                    "ordered_overlay": "not_tested",
                    "depth_composition": "success",
                    "depth_interleaving": "success",
                    "failure_class": "",
                    "failures": [],
                }
            ),
            encoding="utf-8",
        )

        core = input_root / "CoreMatrixEvidence" / "godot-local-verifier"
        core.mkdir(parents=True)
        (core / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "passed"},
                    "matrix": {
                        "product": "godot",
                        "platform": "all",
                        "mode": "local-verifier",
                        "renderer": "preflight",
                    },
                }
            ),
            encoding="utf-8",
        )

        matrix = temp / "matrix_status.json"
        matrix.write_text(
            json.dumps(
                {
                    "schema": "imm-testing-matrix-status-v1",
                    "rows": [
                        {
                            "product": "unity",
                            "platform": "all",
                            "mode": "non-vr",
                            "renderer": "preflight",
                            "status": "supported",
                            "hosted_gate": "CI Engine Matrix / Unity Package Import",
                            "baseline": "tests/baselines/content/sample1.json",
                            "reason": "Unity package import is supported.",
                        },
                        {
                            "product": "unity",
                            "platform": "macos",
                            "mode": "non-vr",
                            "renderer": "metal",
                            "status": "supported",
                            "hosted_gate": "CI Engine Matrix / Unity macOS Metal Composition",
                            "baseline": "tests/baselines/render/unity-macos-metal-sample1.json",
                            "reason": "Unity Metal is supported.",
                        },
                        {
                            "product": "standalone",
                            "platform": "macos",
                            "mode": "non-vr",
                            "renderer": "metal",
                            "status": "supported",
                            "hosted_gate": "Build / macOS",
                            "baseline": "tests/baselines/render/macos-metal-sample1.json",
                            "reason": "macOS Metal standalone is supported.",
                        },
                        {
                            "product": "godot",
                            "platform": "windows",
                            "mode": "vr",
                            "renderer": "openxr",
                            "status": "deferred",
                            "hardware_gate": "CI GPU Matrix / Windows Godot OpenXR VR",
                            "baseline": "tests/baselines/content/sample1.json",
                            "owner_decision": "VR is deferred.",
                        },
                        {
                            "product": "godot",
                            "platform": "windows",
                            "mode": "non-vr",
                            "renderer": "preflight",
                            "status": "supported",
                            "hosted_gate": "CI Core Matrix / Godot Local Verifier",
                            "baseline": "tests/baselines/content/sample1.json",
                            "reason": "Godot preflight is supported.",
                        },
                        {
                            "product": "godot",
                            "platform": "windows",
                            "mode": "non-vr",
                            "renderer": "vulkan",
                            "status": "supported",
                            "hardware_gate": "CI GPU Matrix / Windows Godot Vulkan",
                            "baseline": "tests/baselines/render/godot-windows-vulkan-sample1.json",
                            "reason": "Godot Vulkan is supported.",
                        },
                        {
                            "product": "standalone",
                            "platform": "ios",
                            "mode": "non-vr",
                            "renderer": "native",
                            "status": "unsupported",
                            "reason": "No iOS standalone target.",
                        },
                    ],
                }
            ),
            encoding="utf-8",
        )

        output = temp / "output"
        report = output / "VALIDATION_REPORT.md"
        result = subprocess.run(
            [
                sys.executable,
                "tests/tools/write_visual_evidence_report.py",
                "--input-root",
                str(input_root),
                "--output-dir",
                str(output),
                "--matrix-status",
                str(matrix),
                "--required-evidence-scope",
                "hosted",
                "--markdown-output",
                str(report),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, "Expected invalid supported visual evidence to fail aggregation"
        assert "standalone/macos/non-vr/metal (missing evidence)" in result.stdout
        text = report.read_text(encoding="utf-8")
        assert "## Visual Matrix" in text
        assert "| Platform | Standalone | Godot | Unity |" in text
        assert "| Windows | ⬜ | 🟥 | ⬜ |" in text
        assert "| macOS | 🟥 | ⬜ | 🟥 |" in text
        assert "| iOS | ⬜ | ⬜ | ⬜ |" in text
        assert "Windows ·" not in text
        assert "macOS ·" not in text
        assert "Render only" not in text
        assert "## Matrix Coverage" in text
        assert "| unity/all/non-vr/preflight | supported | passed | no |" in text
        assert "| unity/macos/non-vr/metal | supported | failed | yes |" in text
        assert "| standalone/macos/non-vr/metal | supported | missing evidence | yes |" in text
        assert "| godot/windows/vr/openxr | deferred | deferred | no |" in text
        assert "| godot/windows/non-vr/preflight | supported | passed | no |" in text
        assert "| godot/windows/non-vr/vulkan | supported | failed | yes |" in text
        assert "| standalone/ios/non-vr/native | unsupported | unsupported | no |" in text
        assert "## Unity Windows DirectX Composition" in text
        assert "## Unity macOS Metal Composition" in text
        assert "## Unity macOS Metal Composition\n\n- Result: composition_failed" in text
        assert "- Lane status: failed" in text
        assert "unity-android-vulkan-synthetic-left.png" in text
        assert "unity-android-vulkan-synthetic-right.png" in text
        assert "Composition mode: full_depth" in text
        assert "Composition contract: depth_composition" in text
        assert "Ordered overlay: not_tested" in text
        assert "Depth composition: expected_failed" in text
        assert "Depth interleaving: expected_failed" in text
        assert "![unity-windows-directx-composition.png]" in text
        assert "## Unity Windows Vulkan Ordered Overlay" in text
        assert "Composition mode: ordered_overlay" in text
        assert "Ordered overlay: success" in text
        assert "Depth composition: not_claimed" in text
        assert "![unity-windows-vulkan-ordered-overlay.png]" in text
        assert "## Unity Windows Vulkan Full Depth" in text
        assert "Depth composition: success" in text
        assert "Depth interleaving: success" in text
        assert "![unity-windows-vulkan-full-depth.png]" in text
        assert "## Windows Godot Vulkan Ordered Overlay" in text
        assert "![godot-vulkan-ordered-overlay.png]" in text
        assert "## Windows Godot Vulkan Ordered Overlay\n\n- Result: composition_failed" in text
        assert "character-occluded-cyan share 0.007910 exceeds contract maximum 0.000250" in text
        normalized_overlay_metrics = json.loads(
            (
                output
                / "captures"
                / "windows-godot-vulkan-ordered-overlay"
                / "render-metrics.json"
            ).read_text(encoding="utf-8")
        )
        assert normalized_overlay_metrics["passed"] is False
        assert normalized_overlay_metrics["candidate"]["path"].endswith(
            "godot-vulkan-ordered-overlay.ppm"
        )
        assert "## Godot Windows Vulkan Full Depth" in text
        assert "![godot-vulkan-full-depth.png]" in text
        assert "### Missing Supported Evidence" in text
        assert "standalone/macos/non-vr/metal: macOS Metal standalone is supported." in text
        assert "## Enginevalidationevidence" not in text
        assert "## Render\n" not in text
        assert "## Composition\n" not in text
        assert "## Fulldepth\n" not in text
        assert "## Full Depth\n" not in text
        assert "## Orderedoverlay\n" not in text
        assert "## Ordered Overlay\n" not in text
        assert "## Editorplay\n" not in text
        assert "## Sampleplay\n" not in text

    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        raw = temp / "raw" / "UnityAndroidVulkan"
        captures = raw / "captures"
        captures.mkdir(parents=True)
        (captures / "unity-android-vulkan-render.png").write_bytes(PNG_1X1)
        (raw / "render-report.md").write_text(
            "# IMM Render Report\n\n### unity-android-vulkan-render.png\n"
            "![Unity Android Vulkan](captures/unity-android-vulkan-render.png)\n",
            encoding="utf-8",
        )
        strict_metrics = {
            "passed": True,
            "candidate": {"width": 1, "height": 1},
            "reference": {"width": 1, "height": 1},
            "contract": {"minimum_spatial_correlation": 0.78},
            "spatial_luma_grid": {"mean_abs_delta": 0.01, "correlation": 0.99},
            "errors": [],
        }
        (raw / "unity-android-vulkan-render-metrics.json").write_text(
            json.dumps(strict_metrics),
            encoding="utf-8",
        )
        android_manifest = {
            "schema": "imm-ci-artifact-manifest-v1",
            "classification": {"result": "passed"},
            "matrix": {
                "product": "unity",
                "platform": "android",
                "mode": "non-vr",
                "renderer": "vulkan",
            },
        }
        (raw / "manifest.json").write_text(json.dumps(android_manifest), encoding="utf-8")
        preflight = temp / "raw" / "UnityPackageImport"
        preflight.mkdir(parents=True)
        preflight_manifest = {
            "schema": "imm-ci-artifact-manifest-v1",
            "classification": {"result": "passed"},
            "matrix": {
                "product": "unity",
                "platform": "all",
                "mode": "non-vr",
                "renderer": "preflight",
            },
        }
        (preflight / "manifest.json").write_text(json.dumps(preflight_manifest), encoding="utf-8")

        first_output = temp / "first"
        first_report = first_output / "ENGINE_VALIDATION_REPORT.md"
        first_result = subprocess.run(
            [
                sys.executable,
                "tests/tools/write_visual_evidence_report.py",
                "--input-root",
                str(temp / "raw"),
                "--output-dir",
                str(first_output),
                "--markdown-output",
                str(first_report),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert first_result.returncode == 0, first_result.stdout + first_result.stderr
        normalized = first_output / "captures" / "unity-android-vulkan"
        assert (normalized / "render-metrics.json").exists()
        assert (normalized / "manifest.json").exists()
        preserved_preflights = list((first_output / "status-manifests").rglob("manifest.json"))
        assert any(
            json.loads(path.read_text(encoding="utf-8")).get("matrix", {}).get("renderer") == "preflight"
            for path in preserved_preflights
        )

        aggregate_dir = first_output / "status-manifests" / "unity-non-vr"
        aggregate_dir.mkdir(parents=True)
        aggregate_manifest = {
            "schema": "imm-ci-artifact-manifest-v1",
            "classification": {"result": "failed", "failure_class": "rendering"},
            "matrix": {
                "product": "unity",
                "platform": "all",
                "mode": "non-vr",
                "renderer": "native",
            },
        }
        (aggregate_dir / "manifest.json").write_text(json.dumps(aggregate_manifest), encoding="utf-8")

        matrix = temp / "android-matrix.json"
        matrix.write_text(
            json.dumps(
                {
                    "schema": "imm-testing-matrix-status-v1",
                    "rows": [
                        {
                            "product": "unity",
                            "platform": "all",
                            "mode": "non-vr",
                            "renderer": "preflight",
                            "status": "supported",
                            "hosted_gate": "CI Engine Matrix / Unity Package Import",
                            "baseline": "tests/baselines/content/sample1.json",
                            "reason": "Unity package import is validated.",
                        },
                        {
                            "product": "unity",
                            "platform": "android",
                            "mode": "non-vr",
                            "renderer": "vulkan",
                            "status": "supported",
                            "hardware_gate": "CI Engine Matrix / Unity Android Vulkan",
                            "baseline": "tests/baselines/render/unity-android-vulkan-sample1.json",
                            "reason": "Android Vulkan is physically validated.",
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        second_output = temp / "second"
        second_report = second_output / "VALIDATION_REPORT.md"
        second_result = subprocess.run(
            [
                sys.executable,
                "tests/tools/write_visual_evidence_report.py",
                "--input-root",
                str(first_output),
                "--output-dir",
                str(second_output),
                "--matrix-status",
                str(matrix),
                "--required-evidence-scope",
                "all",
                "--markdown-output",
                str(second_report),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert second_result.returncode == 0, second_result.stdout + second_result.stderr
        second_text = second_report.read_text(encoding="utf-8")
        assert "| unity/all/non-vr/preflight | supported | passed | no |" in second_text
        assert "| unity/android/non-vr/vulkan | supported | passed | yes |" in second_text
        assert "## Unity Android Vulkan\n\n- Result: passed" in second_text

    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        raw = temp / "raw" / "UnityWindowsVulkanSyntheticStereo"
        raw.mkdir(parents=True)
        (raw / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {
                        "result": "failed",
                        "failure_class": "runtime",
                        "failures": ["requested Vulkan fell back to Direct3D 11"],
                        "warnings": ["Lavapipe was selected by the loader"],
                    },
                    "matrix": {
                        "product": "unity",
                        "platform": "windows",
                        "mode": "synthetic-stereo",
                        "renderer": "vulkan",
                    },
                }
            ),
            encoding="utf-8",
        )
        build_only = temp / "raw" / "UnityWindowsVulkanPlayerBuild"
        build_only.mkdir(parents=True)
        (build_only / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "passed", "failure_class": ""},
                    "matrix": {
                        "product": "unity",
                        "platform": "windows",
                        "mode": "non-vr",
                        "renderer": "vulkan",
                    },
                }
            ),
            encoding="utf-8",
        )
        directx_visual = temp / "raw" / "UnityWindowsDirectXComposition"
        directx_visual.mkdir(parents=True)
        (directx_visual / "render-metrics.json").write_text(
            json.dumps(
                {
                    "passed": True,
                    "candidate": {"width": 1, "height": 1},
                    "reference": {"width": 1, "height": 1},
                    "contract": {"path": "unity-windows-directx-sample1.json"},
                    "spatial_luma_grid": {
                        "mean_abs_delta": 0.01,
                        "correlation": 0.99,
                    },
                    "errors": [],
                }
            ),
            encoding="utf-8",
        )
        (directx_visual / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "passed", "failure_class": ""},
                    "matrix": {
                        "product": "unity",
                        "platform": "windows",
                        "mode": "non-vr",
                        "renderer": "directx",
                    },
                }
            ),
            encoding="utf-8",
        )
        directx_build = temp / "raw" / "UnityWindowsDirectXPlayerBuild"
        directx_build.mkdir(parents=True)
        (directx_build / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "passed", "failure_class": ""},
                    "matrix": {
                        "product": "unity",
                        "platform": "windows",
                        "mode": "non-vr",
                        "renderer": "directx",
                    },
                }
            ),
            encoding="utf-8",
        )
        matrix = temp / "matrix.json"
        matrix.write_text(
            json.dumps(
                {
                    "rows": [
                        {
                            "product": "unity",
                            "platform": "windows",
                            "mode": "non-vr",
                            "renderer": "directx",
                            "status": "supported",
                            "hosted_gate": "CI Engine Matrix / Unity Windows DirectX",
                            "baseline": "tests/baselines/render/unity-windows-directx-sample1.json",
                            "reason": "Passing visual evidence must outrank a build-only manifest.",
                        },
                        {
                            "product": "unity",
                            "platform": "windows",
                            "mode": "non-vr",
                            "renderer": "vulkan",
                            "status": "supported",
                            "hardware_gate": "CI Engine Matrix / Unity Windows Vulkan",
                            "baseline": "tests/baselines/render/windows-directx-sample1.json",
                            "reason": "A player build alone is not visual evidence.",
                        },
                        {
                            "product": "unity",
                            "platform": "windows",
                            "mode": "synthetic-stereo",
                            "renderer": "vulkan",
                            "status": "supported",
                            "hosted_gate": "CI Engine Matrix / Unity Windows Vulkan stereo",
                            "baseline": "tests/baselines/render/unity-windows-vulkan-sample1.json",
                            "reason": "Vulkan must not fall back to another API.",
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        output = temp / "output"
        report = output / "VALIDATION_REPORT.md"
        completed = subprocess.run(
            [
                sys.executable,
                "tests/tools/write_visual_evidence_report.py",
                "--input-root", str(temp / "raw"),
                "--output-dir", str(output),
                "--matrix-status", str(matrix),
                "--required-evidence-scope", "all",
                "--markdown-output", str(report),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert completed.returncode == 1
        text = report.read_text(encoding="utf-8")
        assert "## Status-Only Evidence" in text
        assert "| unity/windows/non-vr/directx | supported | passed | yes |" in text
        assert "| unity/windows/synthetic-stereo/vulkan | supported | failed | yes |" in text
        assert "### Unity Windows Synthetic Stereo Vulkan" in text
        assert "- Result: runtime_failed" in text
        assert "- Failure class: runtime" in text
        assert "- Failure: requested Vulkan fell back to Direct3D 11" in text
        assert "- Supporting diagnostic: Lavapipe was selected by the loader" in text
        assert "### Unity Windows Non Vr Vulkan" in text
        assert "- Result: evidence_incomplete" in text
        assert "- Failure: no strict visual report was produced for this supported visual row" in text

    print("Visual evidence report tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
