#!/usr/bin/env python3
"""Classify native Godot iOS-on-Mac Metal rendering and depth evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from composition_status import build_composition_fields


VISUAL_RESULT_PREFIX = "[imm_godot_visual_result_20260813]"
SAMPLE_RESULT_PREFIX = "[imm_godot_sample_play_20260803]"
RUNTIME_FAILURE_MARKERS = (
    "device lost",
    "device was lost",
    "fatal signal",
    "signal 11",
    "segmentation fault",
    "crash",
    "immviewernode setup failed",
    "immviewercompositoreffect setup failed",
)


def read_json(path: Path) -> dict | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def metric_failures(label: str, metric: dict) -> list[str]:
    details = metric.get("errors") or metric.get("failures") or []
    if not isinstance(details, list):
        details = [details]
    suffix = "; ".join(str(item) for item in details if str(item))
    return [f"{label} failed{f': {suffix}' if suffix else ''}"]


def composition_status(
    mode: str,
    render_metric: dict | None,
    composition_metric: dict | None,
    missing: list[str],
) -> dict:
    render_passed = render_metric is not None and render_metric.get("passed") is True
    composition_passed = (
        composition_metric is not None and composition_metric.get("passed") is True
    )
    failures = list(missing)
    if render_metric is not None and not render_passed:
        failures.extend(metric_failures("render visual contract", render_metric))
    if composition_metric is not None and not composition_passed:
        failures.extend(metric_failures(f"{mode} visual contract", composition_metric))
    if missing:
        result, failure_class = "evidence_incomplete", "evidence"
    elif not render_passed:
        result, failure_class = "render_failed", "rendering"
    elif not composition_passed:
        result, failure_class = "composition_failed", "compositing"
    else:
        result, failure_class = "passed", ""
    status = {
        "schema": "imm-composition-status-v1",
        "result": result,
        "rendering": "success" if render_passed else "failed",
        "failure_class": failure_class,
        "failures": failures,
    }
    status.update(
        build_composition_fields(
            mode,
            render_passed,
            [] if composition_passed else failures,
        )
    )
    return status


def classify(root: Path, runtime_outcome: str) -> tuple[dict, dict, dict]:
    capture_paths = {
        "render": root / "godot-ios-metal-render.png",
        "full_depth": root / "godot-ios-metal-full-depth.png",
        "ordered_overlay": root / "godot-ios-metal-ordered-overlay.png",
        "sample_play": root / "godot-ios-sample-play.png",
    }
    metric_paths = {
        "render": root / "godot-ios-metal-render-metrics.json",
        "full_depth": root / "godot-ios-metal-full-depth-metrics.json",
        "ordered_overlay": root / "godot-ios-metal-ordered-overlay-metrics.json",
        "sample_play": root / "godot-ios-sample-play-metrics.json",
    }
    result_logs = {
        "full_depth": root / "godot-ios-full-depth-result.log",
        "ordered_overlay": root / "godot-ios-ordered-overlay-result.log",
        "sample_play": root / "godot-ios-sample-play.log",
    }
    metrics = {name: read_json(path) for name, path in metric_paths.items()}

    missing = [
        f"missing authoritative capture: {path.name}"
        for path in capture_paths.values()
        if not path.is_file()
    ]
    missing.extend(
        f"missing or invalid visual evidence: {path.name}"
        for name, path in metric_paths.items()
        if metrics[name] is None
    )
    missing.extend(
        f"missing runtime result: {path.name}"
        for path in result_logs.values()
        if not path.is_file()
    )

    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in result_logs.values()
        if path.is_file()
    ).lower()
    runtime_failures = [
        f"runtime failure marker: {marker}"
        for marker in RUNTIME_FAILURE_MARKERS
        if marker in log_text
    ]
    for mode in ("full_depth", "ordered_overlay"):
        path = result_logs[mode]
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore").lower()
        if not all(
            token in text
            for token in (VISUAL_RESULT_PREFIX, "passed", "renderer=metal", "os=ios")
        ):
            runtime_failures.append(
                f"missing native iOS Metal success contract: {path.name}"
            )
    sample_log = result_logs["sample_play"]
    if sample_log.is_file():
        text = sample_log.read_text(encoding="utf-8", errors="ignore").lower()
        if not all(token in text for token in (SAMPLE_RESULT_PREFIX, "passed", "os=ios")):
            runtime_failures.append(
                f"missing ordinary iOS sample Run-button contract: {sample_log.name}"
            )

    full_missing = [
        item for item in missing if "render" in item or "full-depth" in item
    ]
    overlay_missing = [
        item for item in missing if "render" in item or "ordered-overlay" in item
    ]
    full_status = composition_status(
        "full_depth", metrics["render"], metrics["full_depth"], full_missing
    )
    overlay_status = composition_status(
        "ordered_overlay", metrics["render"], metrics["ordered_overlay"], overlay_missing
    )

    render_failures: list[str] = []
    for name in ("render", "sample_play"):
        metric = metrics[name]
        if metric is not None and metric.get("passed") is not True:
            render_failures.extend(metric_failures(f"{name} visual contract", metric))
    composition_failures: list[str] = []
    for name in ("full_depth", "ordered_overlay"):
        metric = metrics[name]
        if metric is not None and metric.get("passed") is not True:
            composition_failures.extend(metric_failures(f"{name} visual contract", metric))

    if runtime_failures:
        result, failure_class, failures = "runtime_failed", "runtime", runtime_failures
    elif missing:
        result, failure_class, failures = "evidence_incomplete", "evidence", missing
    elif render_failures:
        result, failure_class, failures = "render_failed", "rendering", render_failures
    elif composition_failures:
        result, failure_class, failures = (
            "composition_failed",
            "compositing",
            composition_failures,
        )
    elif runtime_outcome != "success":
        result, failure_class = "runtime_failed", "runtime-launch"
        failures = [f"native iOS-on-Mac execution step ended as {runtime_outcome}"]
    else:
        result, failure_class, failures = "passed", "", []

    lane = {
        "schema": "imm-godot-ios-metal-status-v1",
        "result": result,
        "failure_class": failure_class,
        "failures": failures,
        "warnings": [],
        "rendering": (
            "success"
            if all(
                metrics[name] is not None and metrics[name].get("passed") is True
                for name in ("render", "sample_play")
            )
            else "failed"
        ),
        "compositing": (
            "success"
            if full_status.get("compositing") == "success"
            and overlay_status.get("compositing") == "success"
            else "failed"
        ),
        "composition_mode": "full_depth",
        "depth_composition": full_status.get("depth_composition", "failed"),
        "ordered_overlay": overlay_status.get("ordered_overlay", "failed"),
        "run_button": (
            "success"
            if metrics["sample_play"] is not None
            and metrics["sample_play"].get("passed") is True
            and sample_log.is_file()
            else "failed"
        ),
    }
    return lane, full_status, overlay_status


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--runtime-outcome", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--full-depth-status-output", type=Path, required=True)
    parser.add_argument("--ordered-overlay-status-output", type=Path, required=True)
    args = parser.parse_args()
    lane, full_depth, overlay = classify(args.artifact_dir, args.runtime_outcome)
    write_json(args.output, lane)
    write_json(args.full_depth_status_output, full_depth)
    write_json(args.ordered_overlay_status_output, overlay)
    print(f"Godot iOS Metal status written: {args.output}")
    return 0 if lane["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
