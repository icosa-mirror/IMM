#!/usr/bin/env python3
"""Classify Unity macOS Metal from required Editor and player visual evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


RUNTIME_MARKERS = (
    "[imm_unity_smoke] graphics api probe failed",
    "[imm_diag] failed to load",
    "failed to load from streamingassets",
    "segmentation fault",
    "crash!!!",
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


def metric_failures(label: str, value: dict) -> list[str]:
    failures = value.get("errors") or value.get("failures") or []
    if not isinstance(failures, list):
        failures = [failures]
    detail = "; ".join(str(item) for item in failures if str(item))
    return [f"{label} failed{f': {detail}' if detail else ''}"]


def visual_status(
    mode: str,
    render_metric: dict | None,
    composition_metric: dict | None = None,
    missing: list[str] | None = None,
) -> dict:
    missing = missing or []
    render_passed = render_metric is not None and render_metric.get("passed") is True
    composition_tested = composition_metric is not None
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
    elif composition_tested and not composition_passed:
        result = "composition_failed"
        failure_class = "compositing"
    else:
        result = "passed"
        failure_class = ""

    status = {
        "schema": "imm-composition-status-v1",
        "result": result,
        "rendering": "success" if render_passed else "failed",
        "failure_class": failure_class,
        "failures": failures,
    }
    if composition_tested:
        status.update(
            {
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
            }
        )
    return status


def classify(
    root: Path,
    player_build_outcome: str,
    editor_play_outcome: str,
) -> tuple[dict, dict, dict, dict]:
    capture_paths = {
        "Editor Play capture": root / "unity-macos-metal-editor-play.png",
        "render-only capture": root / "unity-macos-metal-render.png",
        "full-depth capture": root / "unity-macos-metal-full-depth.png",
        "ordered-overlay capture": root / "unity-macos-metal-ordered-overlay.png",
    }
    metric_paths = {
        "Editor Play visual contract": root / "unity-macos-metal-editor-play-metrics.json",
        "render-only visual contract": root / "unity-macos-metal-render-metrics.json",
        "full-depth visual contract": root / "unity-macos-metal-full-depth-metrics.json",
        "ordered-overlay visual contract": root / "unity-macos-metal-ordered-overlay-metrics.json",
    }
    metrics = {label: read_json(path) for label, path in metric_paths.items()}
    missing = [
        f"missing authoritative capture: {path.name}"
        for path in capture_paths.values()
        if not path.is_file()
    ]
    missing.extend(
        f"missing or invalid visual evidence: {path.name}"
        for label, path in metric_paths.items()
        if metrics[label] is None
    )

    log_paths = sorted(path for path in root.rglob("*.log") if path.is_file())
    log_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore") for path in log_paths
    ).lower()
    runtime_failures = [
        f"runtime failure marker: {marker}"
        for marker in RUNTIME_MARKERS
        if marker in log_text
    ]

    warnings: list[str] = []
    if player_build_outcome != "success":
        if missing:
            runtime_failures.append(
                f"Unity macOS Metal player build step ended as {player_build_outcome}"
            )
        else:
            warnings.append(
                f"player build step ended as {player_build_outcome} after complete visual evidence"
            )
    if editor_play_outcome != "success":
        editor_missing = any("Editor Play" in item or "editor-play" in item for item in missing)
        if editor_missing:
            runtime_failures.append(
                f"Unity Editor Play invocation ended as {editor_play_outcome} without complete evidence"
            )
        else:
            warnings.append(
                f"Editor Play invocation ended as {editor_play_outcome} after its image passed"
            )

    render_metric = metrics["render-only visual contract"]
    editor_metric = metrics["Editor Play visual contract"]
    full_depth_metric = metrics["full-depth visual contract"]
    overlay_metric = metrics["ordered-overlay visual contract"]
    editor_missing = [item for item in missing if "Editor Play" in item or "editor-play" in item]
    full_depth_missing = [
        item for item in missing
        if "render-only" in item or "full-depth" in item or "metal-render" in item
    ]
    overlay_missing = [
        item for item in missing
        if "render-only" in item or "ordered-overlay" in item or "metal-render" in item
    ]
    editor_status = visual_status("render_only", editor_metric, missing=editor_missing)
    full_depth_status = visual_status(
        "full_depth", render_metric, full_depth_metric, full_depth_missing
    )
    overlay_status = visual_status(
        "ordered_overlay", render_metric, overlay_metric, overlay_missing
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
        for label in ("Editor Play visual contract", "render-only visual contract"):
            value = metrics[label]
            assert value is not None
            if value.get("passed") is not True:
                render_failures.extend(metric_failures(label, value))
        composition_failures: list[str] = []
        for label in ("full-depth visual contract", "ordered-overlay visual contract"):
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
        "schema": "imm-unity-macos-metal-status-v1",
        "result": result,
        "failure_class": failure_class,
        "failures": failures,
        "warnings": warnings,
    }
    return lane_status, editor_status, full_depth_status, overlay_status


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--player-build-outcome", required=True)
    parser.add_argument("--editor-play-outcome", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--editor-play-status-output", type=Path, required=True)
    parser.add_argument("--full-depth-status-output", type=Path, required=True)
    parser.add_argument("--ordered-overlay-status-output", type=Path, required=True)
    args = parser.parse_args()

    lane, editor, full_depth, overlay = classify(
        args.artifact_dir,
        args.player_build_outcome.strip().lower(),
        args.editor_play_outcome.strip().lower(),
    )
    write_json(args.output, lane)
    write_json(args.editor_play_status_output, editor)
    write_json(args.full_depth_status_output, full_depth)
    write_json(args.ordered_overlay_status_output, overlay)
    print(
        f"Unity macOS Metal status: {lane['result']} "
        f"({lane['failure_class'] or 'no failure class'})"
    )
    for failure in lane["failures"]:
        print(f"- {failure}")
    for warning in lane["warnings"]:
        print(f"- supporting diagnostic: {warning}")
    return 0 if lane["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
