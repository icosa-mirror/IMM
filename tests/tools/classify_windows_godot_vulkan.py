#!/usr/bin/env python3
"""Classify the Windows Godot Vulkan lane from its complete visual evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


RUNTIME_MARKERS = (
    "device lost",
    "device was lost",
    "vk_error_device_lost",
    "fatal signal",
    "signal 11",
    "immviewernode setup failed",
    "immviewercompositoreffect setup failed",
)


def read_json(path: Path) -> dict | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def metric_failures(label: str, value: dict) -> list[str]:
    failures = value.get("errors") or value.get("failures") or []
    if not isinstance(failures, list):
        failures = [failures]
    detail = "; ".join(str(item) for item in failures if str(item))
    return [f"{label} failed{f': {detail}' if detail else ''}"]


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
        result = "evidence_incomplete"
        failure_class = "evidence"
    elif not render_passed:
        result = "render_failed"
        failure_class = "rendering"
    elif not composition_passed:
        result = "composition_failed"
        failure_class = "compositing"
    else:
        result = "passed"
        failure_class = ""

    return {
        "schema": "imm-composition-status-v1",
        "result": result,
        "rendering": "success" if render_passed else "failed",
        "composition_mode": mode,
        "composition_contract": (
            "ordered_overlay" if mode == "ordered_overlay" else "depth_composition"
        ),
        "compositing": "success" if composition_passed else "failed",
        "ordered_overlay": (
            "success" if composition_passed else "failed"
        ) if mode == "ordered_overlay" else "not_tested",
        "depth_composition": (
            "not_claimed"
            if mode == "ordered_overlay"
            else ("success" if composition_passed else "failed")
        ),
        "depth_interleaving": (
            "not_claimed"
            if mode == "ordered_overlay"
            else ("success" if composition_passed else "failed")
        ),
        "expected": False,
        "failure_class": failure_class,
        "failures": failures,
    }


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def classify(root: Path, render_only_root: Path, overlay_root: Path) -> tuple[dict, dict, dict]:
    required_captures = {
        "Godot project Run-button capture": root / "godot-sample-play.png",
        "render-only capture": root / "godot-vulkan-render.ppm",
        "full-depth capture": root / "godot-vulkan-visual.ppm",
        "ordered-overlay capture": overlay_root / "godot-vulkan-ordered-overlay.ppm",
    }
    metric_paths = {
        "Godot project Run-button visual contract": root / "godot-sample-play-metrics.json",
        "render-only visual contract": root / "godot-vulkan-render-metrics.json",
        "full-depth visual contract": root / "godot-vulkan-full-depth-render-metrics.json",
        "ordered-overlay visual contract": overlay_root / "godot-vulkan-ordered-overlay-metrics.json",
    }
    metrics = {label: read_json(path) for label, path in metric_paths.items()}

    log_paths = sorted(
        path
        for search_root in (root, render_only_root, overlay_root)
        if search_root.exists()
        for path in search_root.rglob("*.log")
        if path.is_file()
    )
    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore") for path in log_paths
    ).lower()
    runtime_failures = [
        f"runtime failure marker: {marker}"
        for marker in RUNTIME_MARKERS
        if marker in log_text
    ]

    missing = [
        f"missing authoritative capture: {path.name}"
        for path in required_captures.values()
        if not path.is_file()
    ]
    missing.extend(
        f"missing or invalid visual evidence: {path.name}"
        for label, path in metric_paths.items()
        if metrics[label] is None
    )

    render_metric = metrics["render-only visual contract"]
    full_depth_metric = metrics["full-depth visual contract"]
    overlay_metric = metrics["ordered-overlay visual contract"]
    full_depth_missing = [
        item for item in missing
        if "render-only" in item or "full-depth" in item or "godot-vulkan-render" in item or "godot-vulkan-visual" in item
    ]
    overlay_missing = [
        item for item in missing
        if "render-only" in item or "ordered-overlay" in item or "godot-vulkan-render" in item
    ]
    full_depth_status = composition_status(
        "full_depth", render_metric, full_depth_metric, full_depth_missing
    )
    overlay_status = composition_status(
        "ordered_overlay", render_metric, overlay_metric, overlay_missing
    )

    warnings: list[str] = []
    if log_paths and "imm godot vulkan visual smoke passed" not in log_text:
        warnings.append(
            "redundant Godot visual-smoke success marker was absent; visual evidence is authoritative"
        )

    if runtime_failures:
        result = "runtime_failed"
        failure_class = "runtime"
        failures = runtime_failures
    elif missing:
        result = "evidence_incomplete"
        failure_class = "evidence"
        failures = missing
    else:
        render_failures: list[str] = []
        for label in (
            "Godot project Run-button visual contract",
            "render-only visual contract",
        ):
            value = metrics[label]
            assert value is not None
            if value.get("passed") is not True:
                render_failures.extend(metric_failures(label, value))
        composition_failures: list[str] = []
        for label in (
            "full-depth visual contract",
            "ordered-overlay visual contract",
        ):
            value = metrics[label]
            assert value is not None
            if value.get("passed") is not True:
                composition_failures.extend(metric_failures(label, value))

        if render_failures:
            result = "render_failed"
            failure_class = "rendering"
            failures = render_failures
        elif composition_failures:
            result = "composition_failed"
            failure_class = "compositing"
            failures = composition_failures
        else:
            result = "passed"
            failure_class = ""
            failures = []

    lane_status = {
        "schema": "imm-windows-godot-vulkan-status-v1",
        "result": result,
        "failure_class": failure_class,
        "failures": failures,
        "warnings": warnings,
    }
    return lane_status, full_depth_status, overlay_status


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--render-only-dir", type=Path, required=True)
    parser.add_argument("--ordered-overlay-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--full-depth-status-output", type=Path, required=True)
    parser.add_argument("--ordered-overlay-status-output", type=Path, required=True)
    args = parser.parse_args()

    lane, full_depth, overlay = classify(
        args.artifact_dir, args.render_only_dir, args.ordered_overlay_dir
    )
    write_json(args.output, lane)
    write_json(args.full_depth_status_output, full_depth)
    write_json(args.ordered_overlay_status_output, overlay)
    print(
        f"Windows Godot Vulkan status: {lane['result']} "
        f"({lane['failure_class'] or 'no failure class'})"
    )
    for failure in lane["failures"]:
        print(f"- {failure}")
    for warning in lane["warnings"]:
        print(f"- supporting diagnostic: {warning}")
    return 0 if lane["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
