#!/usr/bin/env python3
"""Classify Unity iOS Metal simulator render and depth evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from composition_status import build_composition_fields


RUNTIME_MARKERS = (
    "[imm_unity_smoke] graphics api probe failed",
    "[imm_diag] failed to load",
    "failed to load from streamingassets",
    "symbol not found",
    "dyld:",
    "crash!!!",
    "segmentation fault",
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


def visual_status(mode: str, render_metric: dict | None, composition_metric: dict | None, missing: list[str]) -> dict:
    render_passed = render_metric is not None and render_metric.get("passed") is True
    composition_passed = composition_metric is not None and composition_metric.get("passed") is True
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
    status.update(build_composition_fields(mode, render_passed, [] if composition_passed else failures))
    return status


def classify(root: Path, simulator_outcome: str) -> tuple[dict, dict, dict]:
    captures = {
        "render": root / "unity-ios-metal-render.png",
        "full_depth": root / "unity-ios-metal-full-depth.png",
        "ordered_overlay": root / "unity-ios-metal-ordered-overlay.png",
    }
    metric_paths = {
        "render": root / "unity-ios-metal-render-metrics.json",
        "full_depth": root / "unity-ios-metal-full-depth-metrics.json",
        "ordered_overlay": root / "unity-ios-metal-ordered-overlay-metrics.json",
    }
    metrics = {name: read_json(path) for name, path in metric_paths.items()}
    missing = [
        f"missing authoritative capture: {path.name}"
        for path in captures.values()
        if not path.is_file()
    ]
    missing.extend(
        f"missing or invalid visual evidence: {path.name}"
        for name, path in metric_paths.items()
        if metrics[name] is None
    )
    logs = sorted(root.glob("unity-ios-*-player.log"))
    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore") for path in logs
    ).lower()
    runtime_failures = [
        f"runtime failure marker: {marker}"
        for marker in RUNTIME_MARKERS
        if marker in log_text
    ]
    if logs and "[imm_unity_smoke] graphics api expected=metal actual=metal" not in log_text:
        runtime_failures.append("missing Unity Metal graphics API runtime evidence")

    full_missing = [item for item in missing if "render" in item or "full-depth" in item]
    overlay_missing = [item for item in missing if "render" in item or "ordered-overlay" in item]
    full_status = visual_status("full_depth", metrics["render"], metrics["full_depth"], full_missing)
    overlay_status = visual_status("ordered_overlay", metrics["render"], metrics["ordered_overlay"], overlay_missing)

    render_failures = [] if metrics["render"] is None or metrics["render"].get("passed") is True else metric_failures("render visual contract", metrics["render"])
    composition_failures: list[str] = []
    for name in ("full_depth", "ordered_overlay"):
        metric = metrics[name]
        if metric is not None and metric.get("passed") is not True:
            composition_failures.extend(metric_failures(f"{name} visual contract", metric))

    warnings: list[str] = []
    if simulator_outcome != "success" and not missing:
        warnings.append(f"simulator execution step ended as {simulator_outcome} after complete visual evidence")
    if runtime_failures:
        result, failure_class, failures = "runtime_failed", "runtime", runtime_failures
    elif missing:
        result, failure_class, failures = "evidence_incomplete", "evidence", missing
    elif render_failures:
        result, failure_class, failures = "render_failed", "rendering", render_failures
    elif composition_failures:
        result, failure_class, failures = "composition_failed", "compositing", composition_failures
    elif simulator_outcome != "success":
        result, failure_class = "runtime_failed", "runtime-launch"
        failures = [f"simulator execution step ended as {simulator_outcome}"]
    else:
        result, failure_class, failures = "passed", "", []

    lane = {
        "schema": "imm-unity-ios-metal-status-v1",
        "result": result,
        "failure_class": failure_class,
        "failures": failures,
        "warnings": warnings,
    }
    return lane, full_status, overlay_status


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--simulator-outcome", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--full-depth-status-output", type=Path, required=True)
    parser.add_argument("--ordered-overlay-status-output", type=Path, required=True)
    args = parser.parse_args()
    lane, full_depth, overlay = classify(args.artifact_dir, args.simulator_outcome)
    write_json(args.output, lane)
    write_json(args.full_depth_status_output, full_depth)
    write_json(args.ordered_overlay_status_output, overlay)
    print(f"Unity iOS Metal status written: {args.output}")
    return 0 if lane["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
