#!/usr/bin/env python3
"""Focused checks for full-depth validation report evidence."""

from __future__ import annotations

import json
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def png_rgba(width: int, height: int, rgba: bytes = b"\xff\x00\xff\xff") -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + rgba * width for _ in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


def write_status(path: Path, *, depth_interleaving: str = "success") -> None:
    path.write_text(
        json.dumps(
            {
                "rendering": "success",
                "composition_mode": "full_depth",
                "composition_contract": "depth_composition",
                "compositing": "success",
                "ordered_overlay": "not_tested",
                "depth_composition": "success",
                "depth_interleaving": depth_interleaving,
                "failures": [],
            }
        ),
        encoding="utf-8",
    )


def run_verify(report: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(REPO_ROOT / "tests/tools/verify_full_depth_evidence_report.py"),
            "--report",
            str(report),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        captures = root / "captures"
        unity = captures / "unity-windows-vulkan-full-depth"
        godot = captures / "windows-godot-vulkan"
        unity.mkdir(parents=True)
        godot.mkdir(parents=True)
        (unity / "unity-windows-vulkan-full-depth.png").write_bytes(png_rgba(320, 180))
        (godot / "godot-vulkan-visual.png").write_bytes(png_rgba(320, 180, b"\x00\xff\xff\xff"))
        write_status(unity / "composition-status.json")
        write_status(godot / "composition-status.json")

        report = root / "VALIDATION_REPORT.md"
        report.write_text(
            "# IMM CI Validation Evidence\n\n"
            "## Unity Windows Vulkan Full Depth\n\n"
            "![unity](captures/unity-windows-vulkan-full-depth/unity-windows-vulkan-full-depth.png)\n\n"
            "## Windows Godot Vulkan\n\n"
            "![godot](captures/windows-godot-vulkan/godot-vulkan-visual.png)\n",
            encoding="utf-8",
        )
        passed = run_verify(report)
        assert passed.returncode == 0, passed.stdout + passed.stderr

        write_status(godot / "composition-status.json", depth_interleaving="expected_failed")
        failed = run_verify(report)
        assert failed.returncode != 0, "expected failed Godot depth status to fail report verification"
        assert "expected depth_interleaving=success" in failed.stdout

        write_status(godot / "composition-status.json")
        report.write_text(
            "# IMM CI Validation Evidence\n\n"
            "## Unity Windows Vulkan Full Depth\n\n"
            "![unity](captures/unity-windows-vulkan-full-depth/unity-windows-vulkan-full-depth.png)\n",
            encoding="utf-8",
        )
        missing_image_reference = run_verify(report)
        assert missing_image_reference.returncode != 0, "expected missing Godot embedded image to fail"
        assert "does not embed an image" in missing_image_reference.stdout

        report.write_text(
            "# IMM CI Validation Evidence\n\n"
            "## Unity Windows Vulkan Full Depth\n\n"
            "![unity](captures/unity-windows-vulkan-full-depth/unity-windows-vulkan-full-depth.png)\n\n"
            "## Windows Godot Vulkan\n\n"
            "![godot](captures/windows-godot-vulkan/godot-vulkan-visual.png)\n",
            encoding="utf-8",
        )
        (godot / "godot-vulkan-visual.png").write_bytes(png_rgba(16, 16))
        tiny_image = run_verify(report)
        assert tiny_image.returncode != 0, "expected tiny placeholder image to fail"
        assert "too small for visual evidence" in tiny_image.stdout

    print("Full-depth evidence report verification tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
