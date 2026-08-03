#!/usr/bin/env python3
"""Verify that Unity dispatched two distinct Vulkan eye targets for one camera."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


PREFIX = "[IMM_UNITY_VK_SYNTH_STEREO_20260803]"
DISPATCH = re.compile(
    re.escape(PREFIX)
    + r" dispatch cameraId=(?P<camera>\d+) eye=(?P<eye>[01]) "
    + r"eventId=(?P<event>\d+) targetId=(?P<target>-?\d+) targetPtr=0x(?P<pointer>[0-9A-Fa-f]+)"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    latest_by_eye: dict[int, dict[str, int]] = {}
    for match in DISPATCH.finditer(text):
        values = {key: int(value, 16 if key == "pointer" else 10) for key, value in match.groupdict().items()}
        latest_by_eye[values["eye"]] = values

    failures: list[str] = []
    for eye in (0, 1):
        if eye not in latest_by_eye:
            failures.append(f"missing dispatch for eye {eye}")

    if not failures:
        left = latest_by_eye[0]
        right = latest_by_eye[1]
        if left["camera"] != right["camera"]:
            failures.append(f"eyes used different camera IDs: {left['camera']} and {right['camera']}")
        if right["event"] != left["event"] + 1:
            failures.append(f"eye event IDs are not adjacent: {left['event']} and {right['event']}")
        if left["target"] == 0 or right["target"] == 0 or left["target"] == right["target"]:
            failures.append(f"eye target IDs are invalid or shared: {left['target']} and {right['target']}")
        if left["pointer"] == 0 or right["pointer"] == 0 or left["pointer"] == right["pointer"]:
            failures.append(
                f"eye native pointers are invalid or shared: 0x{left['pointer']:X} and 0x{right['pointer']:X}"
            )

    required_markers = [
        "[IMM_UNITY_SMOKE] graphics api expected=Vulkan actual=Vulkan",
        f"{PREFIX} passed",
        "[IMM_UNITY_SMOKE] synthetic stereo Vulkan capture=",
    ]
    for marker in required_markers:
        if marker not in text:
            failures.append(f"missing marker: {marker}")

    result = {
        "schema": "imm-unity-vulkan-synthetic-stereo-log-v1",
        "status": "failed" if failures else "passed",
        "dispatches": latest_by_eye,
        "failures": failures,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
