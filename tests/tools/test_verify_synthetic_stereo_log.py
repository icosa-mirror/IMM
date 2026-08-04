#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


PREFIX = "[IMM_UNITY_VK_SYNTH_STEREO_20260803]"


def invoke(tool: Path, log: Path, output: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(tool), "--log", str(log), "--output", str(output)],
        text=True,
        capture_output=True,
    )


def main() -> int:
    tool = Path(__file__).with_name("verify_synthetic_stereo_log.py")
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        log = root / "player.log"
        output = root / "result.json"
        common = (
            "[IMM_UNITY_SMOKE] graphics api expected=Vulkan actual=Vulkan\n"
            f"{PREFIX} passed leftTargetId=10 rightTargetId=11 capture=x\n"
            "[IMM_UNITY_SMOKE] synthetic stereo Vulkan capture=x\n"
        )
        log.write_text(
            common
            + f"{PREFIX} matrices cameraId=3 selectedEye=0 halfIpd=0.150 leftTx=1.150000 rightTx=0.850000\n"
            + f"{PREFIX} matrices cameraId=3 selectedEye=1 halfIpd=0.150 leftTx=1.150000 rightTx=0.850000\n"
            + f"{PREFIX} dispatch cameraId=3 eye=0 eventId=768 targetId=10 targetPtr=0xABC\n"
            + f"{PREFIX} dispatch cameraId=3 eye=1 eventId=769 targetId=11 targetPtr=0xDEF\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=0 targetId=10 targetPtr=0xABC\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=1 targetId=11 targetPtr=0xDEF\n",
            encoding="utf-8",
        )
        passed = invoke(tool, log, output)
        assert passed.returncode == 0, passed.stdout + passed.stderr
        assert json.loads(output.read_text(encoding="utf-8"))["status"] == "passed"

        log.write_text(
            common
            + f"{PREFIX} matrices cameraId=3 selectedEye=0 halfIpd=0.150 leftTx=1.150000 rightTx=0.850000\n"
            + f"{PREFIX} matrices cameraId=3 selectedEye=1 halfIpd=0.150 leftTx=1.150000 rightTx=0.850000\n"
            + f"{PREFIX} dispatch cameraId=3 eye=0 eventId=768 targetId=10 targetPtr=0xABC\n"
            + f"{PREFIX} dispatch cameraId=3 eye=1 eventId=769 targetId=10 targetPtr=0xABC\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=0 targetId=10 targetPtr=0xABC\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=1 targetId=10 targetPtr=0xABC\n",
            encoding="utf-8",
        )
        failed = invoke(tool, log, output)
        assert failed.returncode != 0
        failures = json.loads(output.read_text(encoding="utf-8"))["failures"]
        assert any("shared" in failure for failure in failures)

        log.write_text(
            common
            + f"{PREFIX} matrices cameraId=3 selectedEye=0 halfIpd=0.032 leftTx=1.032000 rightTx=0.968000\n"
            + f"{PREFIX} matrices cameraId=3 selectedEye=1 halfIpd=0.032 leftTx=1.032000 rightTx=0.968000\n"
            + f"{PREFIX} dispatch cameraId=3 eye=0 eventId=768 targetId=10 targetPtr=0xABC\n"
            + f"{PREFIX} dispatch cameraId=3 eye=1 eventId=769 targetId=11 targetPtr=0xDEF\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=0 targetId=10 targetPtr=0xABC\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=1 targetId=11 targetPtr=0xDEF\n",
            encoding="utf-8",
        )
        weak_matrices = invoke(tool, log, output)
        assert weak_matrices.returncode != 0
        failures = json.loads(output.read_text(encoding="utf-8"))["failures"]
        assert any("matrix separation is too small" in failure for failure in failures)

        log.write_text(
            common
            + f"{PREFIX} matrices cameraId=3 selectedEye=0 halfIpd=0.150 leftTx=1.150000 rightTx=0.850000\n"
            + f"{PREFIX} matrices cameraId=3 selectedEye=1 halfIpd=0.150 leftTx=1.150000 rightTx=0.850000\n"
            + f"{PREFIX} dispatch cameraId=3 eye=0 eventId=768 targetId=10 targetPtr=0xABC\n"
            + f"{PREFIX} dispatch cameraId=3 eye=1 eventId=769 targetId=11 targetPtr=0xDEF\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=0 targetId=10 targetPtr=0xABC\n"
            + "[IMM_SYNTH_PRESENT_EYE_20260804] eye=1 targetId=10 targetPtr=0xABC\n",
            encoding="utf-8",
        )
        wrong_presentation = invoke(tool, log, output)
        assert wrong_presentation.returncode != 0
        failures = json.loads(output.read_text(encoding="utf-8"))["failures"]
        assert any("presented target" in failure for failure in failures)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
