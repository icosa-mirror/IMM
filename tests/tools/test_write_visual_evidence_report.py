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
                        "renderer": "native",
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

        gpu = input_root / "GPUMatrixEvidence" / "WindowsGodotVulkan-GPU"
        gpu.mkdir(parents=True)
        (gpu / "manifest.json").write_text(
            json.dumps(
                {
                    "schema": "imm-ci-artifact-manifest-v1",
                    "classification": {"result": "expected_failed", "failure_class": "compositing"},
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
            "# IMM Render Report\n\n### godot-vulkan-ordered-overlay.png\n"
            "![Godot](captures/godot-vulkan-ordered-overlay.png)\n",
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
                            "renderer": "native",
                            "status": "supported",
                            "hosted_gate": "CI Engine Matrix / Unity Package Import",
                            "baseline": "tests/baselines/render/unity-windows-directx-sample1.json",
                            "reason": "Unity package import is supported.",
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
                "--markdown-output",
                str(report),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stderr
        text = report.read_text(encoding="utf-8")
        assert "## Matrix Coverage" in text
        assert "| unity/all/non-vr/native | supported | passed | yes |" in text
        assert "| standalone/macos/non-vr/metal | supported | missing evidence | yes |" in text
        assert "| godot/windows/vr/openxr | deferred | deferred | no |" in text
        assert "| godot/windows/non-vr/preflight | supported | passed | no |" in text
        assert "| godot/windows/non-vr/vulkan | supported | expected failure | yes |" in text
        assert "| standalone/ios/non-vr/native | unsupported | unsupported | no |" in text
        assert "## Unity Windows DirectX Composition" in text
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
        assert "## Godot Windows Vulkan Full Depth" in text
        assert "![godot-vulkan-full-depth.png]" in text
        assert "### Missing Supported Evidence" in text
        assert "standalone/macos/non-vr/metal: macOS Metal standalone is supported." in text
        assert "## Enginevalidationevidence" not in text

    print("Visual evidence report tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
