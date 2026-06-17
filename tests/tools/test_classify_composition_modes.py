#!/usr/bin/env python3
"""Check explicit visual smoke composition modes."""

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


def run_unity_classifier(temp: Path, mode: str, log_text: str) -> tuple[int, dict]:
    log = temp / f"{mode}.log"
    capture = temp / f"{mode}.png"
    output = temp / f"{mode}.json"
    log.write_text(log_text, encoding="utf-8")
    capture.write_bytes(PNG_1X1)
    result = subprocess.run(
        [
            sys.executable,
            "tests/tools/classify_unity_visual_smoke.py",
            "--log",
            str(log),
            "--capture",
            str(capture),
            "--composition-mode",
            mode,
            "--output",
            str(output),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    return result.returncode, json.loads(output.read_text(encoding="utf-8"))


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        full_depth_log = "\n".join(
            [
                "[IMM_UNITY_SMOKE] capture=C:/tmp/full_depth.png width=1280 height=720",
                "[IMM_UNITY_SMOKE] scene composition rear occlusion probe failed: share=0.50",
            ]
        )
        returncode, status = run_unity_classifier(temp, "full_depth", full_depth_log)
        assert returncode == 0
        assert status["composition_mode"] == "full_depth"
        assert status["composition_contract"] == "depth_composition"
        assert status["compositing"] == "expected_failed"
        assert status["depth_composition"] == "expected_failed"
        assert status["ordered_overlay"] == "not_tested"
        assert status["depth_interleaving"] == "expected_failed"

        full_depth_pass_log = "[IMM_UNITY_SMOKE] capture=C:/tmp/full_depth_pass.png width=1280 height=720"
        returncode, status = run_unity_classifier(temp, "full_depth", full_depth_pass_log)
        assert returncode == 0
        assert status["composition_mode"] == "full_depth"
        assert status["composition_contract"] == "depth_composition"
        assert status["compositing"] == "success"
        assert status["depth_composition"] == "success"
        assert status["depth_interleaving"] == "success"
        assert status["ordered_overlay"] == "not_tested"

        overlay_log = "\n".join(
            [
                "[IMM_UNITY_SMOKE] capture=C:/tmp/ordered_overlay.png width=1280 height=720",
                "[IMM_UNITY_SMOKE] scene composition overlay rear probe failed: share=0.10",
            ]
        )
        returncode, status = run_unity_classifier(temp, "ordered_overlay", overlay_log)
        assert returncode == 1
        assert status["composition_mode"] == "ordered_overlay"
        assert status["composition_contract"] == "ordered_overlay"
        assert status["compositing"] == "failed"
        assert status["ordered_overlay"] == "failed"
        assert status["depth_composition"] == "not_claimed"
        assert status["depth_interleaving"] == "not_claimed"

        overlay_without_imm_log = "\n".join(
            [
                "[IMM_UNITY_SMOKE] capture=C:/tmp/ordered_overlay_without_imm.png width=1280 height=720",
                "[IMM_UNITY_SMOKE] scene composition ordered overlay IMM background failed: candidate=546 total=921600 share=0.0006 colorBuckets=8",
            ]
        )
        returncode, status = run_unity_classifier(temp, "ordered_overlay", overlay_without_imm_log)
        assert returncode == 1
        assert status["composition_mode"] == "ordered_overlay"
        assert status["composition_contract"] == "ordered_overlay"
        assert status["compositing"] == "failed"
        assert status["ordered_overlay"] == "failed"
        assert status["depth_composition"] == "not_claimed"
        assert status["depth_interleaving"] == "not_claimed"

        upside_down_overlay_log = "\n".join(
            [
                "[IMM_UNITY_SMOKE] capture=C:/tmp/ordered_overlay_upside_down.png width=1280 height=720",
                "[IMM_UNITY_SMOKE] scene composition ordered overlay orientation failed: candidate=578313 total=921600 share=0.6275 colorBuckets=34 uniqueColors=46040 brightTop=3181 brightBottom=74809 brightTopBottom=0.043 paintTop=128683 paintBottom=56199 paintTopBottom=2.290",
            ]
        )
        returncode, status = run_unity_classifier(temp, "ordered_overlay", upside_down_overlay_log)
        assert returncode == 1
        assert status["composition_mode"] == "ordered_overlay"
        assert status["composition_contract"] == "ordered_overlay"
        assert status["compositing"] == "failed"
        assert status["ordered_overlay"] == "failed"
        assert status["depth_composition"] == "not_claimed"
        assert status["depth_interleaving"] == "not_claimed"

        returncode, status = run_unity_classifier(
            temp,
            "render_only",
            "[IMM_UNITY_SMOKE] capture=C:/tmp/render_only.png width=1280 height=720",
        )
        assert returncode == 0
        assert status["composition_mode"] == "render_only"
        assert status["composition_contract"] == "render_only"
        assert status["compositing"] == "not_tested"
        assert status["ordered_overlay"] == "not_tested"
        assert status["depth_composition"] == "not_tested"
        assert status["depth_interleaving"] == "not_claimed"

    print("Composition mode classifier tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
