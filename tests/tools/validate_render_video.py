#!/usr/bin/env python3
"""Require a stable run of baseline-matching frames in a device video."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import compare_render_metrics


def first_stable_run(passed: list[bool], minimum: int) -> tuple[int, int] | None:
    start = 0
    count = 0
    for index, value in enumerate(passed):
        if value:
            if count == 0:
                start = index
            count += 1
            if count >= minimum:
                return start, index
        else:
            count = 0
    return None


def extract_frames(video: Path, destination: Path, sample_interval_seconds: float) -> list[Path]:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg is required for physical-device video validation")
    output_pattern = destination / "frame-%04d.png"
    completed = subprocess.run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(video),
            "-vf",
            f"fps=1/{sample_interval_seconds:g}",
            str(output_pattern),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"ffmpeg frame extraction failed: {completed.stderr.strip()}")
    return sorted(destination.glob("frame-*.png"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("video", type=Path)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--capture-output", type=Path, required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--sample-interval-seconds", type=float, default=2.0)
    parser.add_argument("--minimum-consecutive-frames", type=int, default=5)
    args = parser.parse_args()

    if args.sample_interval_seconds <= 0:
        parser.error("--sample-interval-seconds must be positive")
    if args.minimum_consecutive_frames <= 0:
        parser.error("--minimum-consecutive-frames must be positive")

    frame_results: list[dict] = []
    selected: Path | None = None
    stable_run: tuple[int, int] | None = None
    error = ""
    try:
        with tempfile.TemporaryDirectory(prefix="imm-video-validation-") as temp:
            frames = extract_frames(args.video, Path(temp), args.sample_interval_seconds)
            if not frames:
                raise RuntimeError("device video contains no extractable frames")
            passed: list[bool] = []
            for frame in frames:
                result = compare_render_metrics.evaluate_capture(frame, args.reference, args.contract)
                passed.append(bool(result["passed"]))
                frame_results.append(
                    {
                        "frame": frame.name,
                        "time_seconds": len(passed) * args.sample_interval_seconds,
                        "passed": result["passed"],
                        "errors": result["errors"],
                        "candidate": result["candidate"],
                        "spatial_luma_grid": result.get("spatial_luma_grid"),
                        "color_component_probes": result.get("color_component_probes"),
                    }
                )
                stable_run = first_stable_run(passed, args.minimum_consecutive_frames)
                if stable_run is not None:
                    selected_index = (stable_run[0] + stable_run[1]) // 2
                    selected = frames[selected_index]
                    args.capture_output.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(selected, args.capture_output)
                    break
    except Exception as exc:
        error = str(exc)

    output = {
        "schema": "imm-render-video-validation-v1",
        "passed": selected is not None,
        "video": args.video.as_posix(),
        "reference": args.reference.as_posix(),
        "contract": args.contract.as_posix(),
        "sample_interval_seconds": args.sample_interval_seconds,
        "minimum_consecutive_frames": args.minimum_consecutive_frames,
        "stable_run": (
            {
                "first_sample": stable_run[0] + 1,
                "last_sample": stable_run[1] + 1,
                "first_time_seconds": (stable_run[0] + 1) * args.sample_interval_seconds,
                "last_time_seconds": (stable_run[1] + 1) * args.sample_interval_seconds,
            }
            if stable_run is not None
            else None
        ),
        "selected_capture": args.capture_output.as_posix() if selected is not None else None,
        "frames": frame_results,
        "errors": [] if selected is not None else [error or "no stable baseline-matching frame run found"],
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if selected is None:
        print(output["errors"][0], file=sys.stderr)
        return 1
    print(
        f"Physical-device video has {args.minimum_consecutive_frames} consecutive matching samples; "
        f"selected {args.capture_output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
