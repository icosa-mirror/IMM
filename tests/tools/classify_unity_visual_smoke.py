#!/usr/bin/env python3
"""Classify Unity visual smoke output into render and compositing status."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from composition_status import COMPOSITION_CONTRACTS, build_composition_fields, classification_succeeded


RENDER_FAILURE_MARKERS = [
    "[IMM_EDITOR_SMOKE] timed out waiting for capture",
    "[IMM_EDITOR_SMOKE] failed:",
    "[IMM_UNITY_SMOKE] invalid screen size",
    "[IMM_UNITY_SMOKE] graphics api probe failed",
    "[IMM_DIAG] Failed to load",
    "Failed to load from StreamingAssets",
]

RUNTIME_FAILURE_MARKERS = [
    "[IMM_UNITY_SMOKE] graphics api probe failed",
    "[IMM_DIAG] Failed to load",
    "Failed to load from StreamingAssets",
]

CAPTURE_METRICS_RE = re.compile(r"\[IMM_UNITY_SMOKE\] capture=.*?\bnonZero=(?P<non_zero>\d+)\b.*?\bcolorBuckets=(?P<color_buckets>\d+)\b")


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


def capture_metric_failures(text: str) -> list[str]:
    failures: list[str] = []
    matches = list(CAPTURE_METRICS_RE.finditer(text))
    if not matches:
        return failures

    # The last capture line is the authoritative one for the run.
    match = matches[-1]
    non_zero = int(match.group("non_zero"))
    color_buckets = int(match.group("color_buckets"))
    if non_zero <= 0:
        failures.append("capture has no non-zero pixels")
    if color_buckets <= 1:
        failures.append(f"capture has only {color_buckets} color bucket(s)")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--composition-mode", choices=sorted(COMPOSITION_CONTRACTS), default="full_depth")
    parser.add_argument("--render-capture", type=Path)
    parser.add_argument("--render-metrics", type=Path)
    parser.add_argument("--composition-metrics", type=Path)
    parser.add_argument("--expected-graphics-api")
    args = parser.parse_args()

    external_arguments = (
        args.render_capture,
        args.render_metrics,
        args.composition_metrics,
    )
    use_external_evidence = any(external_arguments)
    if use_external_evidence and not all(external_arguments):
        parser.error(
            "--render-capture, --render-metrics, and --composition-metrics must be provided together"
        )

    text = args.log.read_text(encoding="utf-8", errors="ignore") if args.log.exists() else ""
    composition_failures = [
        line.strip()
        for line in text.splitlines()
        if "[IMM_UNITY_SMOKE] scene composition" in line and "failed" in line
    ]
    runtime_failures = [marker for marker in RUNTIME_FAILURE_MARKERS if marker in text]
    render_failures = [
        marker for marker in RENDER_FAILURE_MARKERS
        if marker in text and marker not in RUNTIME_FAILURE_MARKERS
    ]
    render_failures.extend(capture_metric_failures(text))
    rendering_succeeded = (
        ("[IMM_EDITOR_SMOKE] passed:" in text or "[IMM_UNITY_SMOKE] capture=" in text)
        and args.capture.exists()
        and not render_failures
        and not runtime_failures
    )
    if rendering_succeeded and not composition_failures:
        if args.composition_mode == "full_depth" and "[IMM_UNITY_SMOKE] scene composition probe passed" not in text:
            composition_failures.append("scene composition full depth probe missing failed")
        elif args.composition_mode == "ordered_overlay" and "[IMM_UNITY_SMOKE] scene composition overlay probe passed" not in text:
            composition_failures.append("scene composition ordered overlay probe missing failed")

    warnings: list[str] = []
    evidence_failures = []
    if use_external_evidence:
        assert args.render_capture is not None
        assert args.render_metrics is not None
        assert args.composition_metrics is not None
        render_metric = read_json(args.render_metrics)
        composition_metric = read_json(args.composition_metrics)
        evidence_failures = [
            f"missing authoritative capture: {path}"
            for path in (args.render_capture, args.capture)
            if not path.is_file()
        ]
        if render_metric is None:
            evidence_failures.append(f"missing or invalid visual evidence: {args.render_metrics}")
        if composition_metric is None:
            evidence_failures.append(
                f"missing or invalid visual evidence: {args.composition_metrics}"
            )
        if args.expected_graphics_api:
            api_marker = (
                f"[IMM_UNITY_SMOKE] graphics api expected={args.expected_graphics_api} "
                f"actual={args.expected_graphics_api}"
            )
            if not args.log.is_file():
                evidence_failures.append(
                    f"missing graphics API evidence log: {args.log}"
                )
            elif api_marker not in text:
                evidence_failures.append(
                    f"missing requested graphics API evidence: {api_marker}"
                )

        rendering_succeeded = (
            args.render_capture.is_file()
            and render_metric is not None
            and render_metric.get("passed") is True
        )
        composition_failures = []
        render_failures = []
        if render_metric is not None and render_metric.get("passed") is not True:
            render_failures.extend(metric_failures("render visual contract", render_metric))
        if composition_metric is not None and composition_metric.get("passed") is not True:
            composition_failures.extend(
                metric_failures(f"{args.composition_mode} visual contract", composition_metric)
            )
        log_composition_failures = [
            line.strip()
            for line in text.splitlines()
            if "[IMM_UNITY_SMOKE] scene composition" in line and "failed" in line
        ]
        if not args.log.is_file():
            warnings.append("supporting Unity player log is absent")
        elif log_composition_failures and not composition_failures:
            warnings.extend(
                f"supporting diagnostic did not override passing visual evidence: {failure}"
                for failure in log_composition_failures
            )

    composition_fields = build_composition_fields(args.composition_mode, rendering_succeeded, composition_failures)
    if not use_external_evidence and not args.log.exists():
        evidence_failures.append(f"missing log: {args.log}")
    if not args.capture.exists():
        evidence_failures.append(f"missing capture: {args.capture}")

    if runtime_failures:
        result = "runtime_failed"
        failure_class = "runtime"
    elif evidence_failures:
        result = "evidence_incomplete"
        failure_class = "evidence"
    elif render_failures or not rendering_succeeded:
        result = "render_failed"
        failure_class = "rendering"
    elif composition_failures:
        result = "composition_failed"
        failure_class = "compositing"
    else:
        result = "passed"
        failure_class = ""

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
    print(f"Unity visual smoke status written: {args.output}")

    if result == "passed" and classification_succeeded(status):
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
