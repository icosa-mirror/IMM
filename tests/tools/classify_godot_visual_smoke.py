#!/usr/bin/env python3
"""Classify Godot visual smoke output into render and compositing status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from composition_status import COMPOSITION_CONTRACTS, build_composition_fields, classification_succeeded


RENDER_FAILURE_MARKERS = [
    "visual smoke PNG was too flat",
    "visual smoke PNG had only",
    "visual smoke PNG content bounds were too small",
    "visual smoke PNG orientation check failed",
    "ImmViewer did not load",
    "ImmViewer sequence was not ready",
    "render diagnostics did not report IMM draw calls",
    "render diagnostics did not report foreground paint draw calls",
    "ImmViewerCompositorEffect did not start a Metal frame",
    "ImmViewerCompositorEffect did not start a Vulkan frame",
    "ImmGodot_RenderCamera returned",
]

RUNTIME_FAILURE_MARKERS = [
    "device lost",
    "device was lost",
    "vk_error_device_lost",
    "fatal signal",
    "signal 11",
    "immviewernode setup failed",
    "immviewercompositoreffect setup failed",
]


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--renderer", required=True, choices=["metal", "vulkan"])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--composition-mode", choices=sorted(COMPOSITION_CONTRACTS), default="full_depth")
    parser.add_argument("--render-capture", type=Path)
    parser.add_argument("--composition-capture", type=Path)
    parser.add_argument("--render-metrics", type=Path)
    parser.add_argument("--composition-metrics", type=Path)
    args = parser.parse_args()

    external_arguments = (
        args.render_capture,
        args.composition_capture,
        args.render_metrics,
        args.composition_metrics,
    )
    use_external_evidence = any(external_arguments)
    if use_external_evidence and not all(external_arguments):
        parser.error(
            "--render-capture, --composition-capture, --render-metrics, and "
            "--composition-metrics must be provided together"
        )

    text = args.log.read_text(encoding="utf-8", errors="ignore") if args.log.exists() else ""
    composition_failures = [
        line.strip().removeprefix("ERROR: ").strip()
        for line in text.splitlines()
        if "scene composition" in line and "probe failed" in line
    ]
    render_failures = [marker for marker in RENDER_FAILURE_MARKERS if marker in text]
    lowered_text = text.lower()
    runtime_failures = [
        marker for marker in RUNTIME_FAILURE_MARKERS if marker in lowered_text
    ]
    success_marker = f"IMM Godot {args.renderer.title()} visual smoke passed"
    rendering_succeeded = success_marker in text or (
        "visual smoke content diagnostics" in text
        and "visual smoke render diagnostics" in text
        and "visual smoke compositor diagnostics" in text
        and not render_failures
    )
    if rendering_succeeded and not composition_failures:
        has_scene_composition_diagnostics = (
            "visual smoke scene composition diagnostics" in text
            or "visual smoke PPM scene composition diagnostics" in text
        )
        if args.composition_mode == "full_depth" and not has_scene_composition_diagnostics:
            composition_failures.append("scene composition full depth probe missing failed")
        elif args.composition_mode == "ordered_overlay" and "ordered overlay IMM diagnostics" not in text:
            composition_failures.append("scene composition ordered overlay probe missing failed")

    warnings: list[str] = []
    evidence_failures: list[str] = []
    if use_external_evidence:
        assert args.render_capture is not None
        assert args.composition_capture is not None
        assert args.render_metrics is not None
        assert args.composition_metrics is not None
        render_metric = read_json(args.render_metrics)
        composition_metric = read_json(args.composition_metrics)
        evidence_failures.extend(
            f"missing authoritative capture: {path}"
            for path in (args.render_capture, args.composition_capture)
            if not path.is_file()
        )
        if render_metric is None:
            evidence_failures.append(f"missing or invalid visual evidence: {args.render_metrics}")
        if composition_metric is None:
            evidence_failures.append(
                f"missing or invalid visual evidence: {args.composition_metrics}"
            )
        rendering_succeeded = (
            args.render_capture.is_file()
            and render_metric is not None
            and render_metric.get("passed") is True
        )
        render_failures = []
        composition_failures = []
        if render_metric is not None and render_metric.get("passed") is not True:
            render_failures.extend(metric_failures("render visual contract", render_metric))
        if composition_metric is not None and composition_metric.get("passed") is not True:
            composition_failures.extend(
                metric_failures(f"{args.composition_mode} visual contract", composition_metric)
            )
        if not args.log.is_file():
            warnings.append("supporting Godot log is absent")
        elif not composition_failures:
            log_failures = [
                line.strip().removeprefix("ERROR: ").strip()
                for line in text.splitlines()
                if "scene composition" in line and "probe failed" in line
            ]
            warnings.extend(
                f"supporting diagnostic did not override passing visual evidence: {failure}"
                for failure in log_failures
            )

    composition_fields = build_composition_fields(args.composition_mode, rendering_succeeded, composition_failures)
    if not use_external_evidence and not args.log.exists():
        result = "evidence_incomplete"
        failure_class = "evidence"
        evidence_failures = [f"missing log: {args.log}"]
    elif runtime_failures:
        result = "runtime_failed"
        failure_class = "runtime"
    elif evidence_failures:
        result = "evidence_incomplete"
        failure_class = "evidence"
    elif render_failures or not rendering_succeeded:
        result = "render_failed"
        failure_class = "rendering"
        if not use_external_evidence:
            evidence_failures = []
    elif composition_failures:
        result = "composition_failed"
        failure_class = "compositing"
        if not use_external_evidence:
            evidence_failures = []
    else:
        result = "passed"
        failure_class = ""
        if not use_external_evidence:
            evidence_failures = []
    render_failure_details = (
        render_failures
        if use_external_evidence
        else [f"rendering failure marker: {marker}" for marker in render_failures]
    )
    status = {
        "schema": "imm-composition-status-v1",
        "result": result,
        "rendering": "success" if rendering_succeeded else "failed",
        "failure_class": failure_class,
        "failures": (
            evidence_failures
            + composition_failures
            + [f"runtime failure marker: {marker}" for marker in runtime_failures]
            + render_failure_details
        ),
        "warnings": warnings,
    }
    status.update(composition_fields)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Godot visual smoke status written: {args.output}")

    if result == "passed" and classification_succeeded(status):
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
