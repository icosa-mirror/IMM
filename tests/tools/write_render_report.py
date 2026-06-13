#!/usr/bin/env python3
"""Write a human-readable render comparison report with viewable PNG captures."""

from __future__ import annotations

import argparse
import json
import shutil
import struct
import zlib
from pathlib import Path


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as handle:
        magic = handle.readline().strip()
        if magic != b"P6":
            raise ValueError(f"{path} is not a binary PPM (P6)")

        tokens: list[bytes] = []
        while len(tokens) < 3:
            line = handle.readline()
            if not line:
                raise ValueError(f"{path} ended before PPM header was complete")
            line = line.split(b"#", 1)[0].strip()
            if line:
                tokens.extend(line.split())

        width, height, max_value = (int(token) for token in tokens[:3])
        if max_value != 255:
            raise ValueError(f"{path} has unsupported PPM max value {max_value}")
        pixels = handle.read()

    expected_bytes = width * height * 3
    if len(pixels) != expected_bytes:
        raise ValueError(f"{path} has {len(pixels)} pixel bytes, expected {expected_bytes}")
    return width, height, pixels


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(kind)
    crc = zlib.crc32(payload, crc)
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc & 0xFFFFFFFF)


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    rows = []
    stride = width * 3
    for y in range(height):
        rows.append(b"\x00" + rgb[y * stride : (y + 1) * stride])
    raw = b"".join(rows)
    payload = [
        b"\x89PNG\r\n\x1a\n",
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)),
        png_chunk(b"IDAT", zlib.compress(raw, level=6)),
        png_chunk(b"IEND", b""),
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"".join(payload))


def convert_capture(capture: Path, output_dir: Path) -> Path:
    if capture.suffix.lower() == ".ppm":
        width, height, pixels = read_ppm(capture)
        output = output_dir / f"{capture.stem}.png"
        write_png(output, width, height, pixels)
        return output
    if capture.suffix.lower() == ".png":
        output = output_dir / capture.name
        output.parent.mkdir(parents=True, exist_ok=True)
        if capture.resolve() != output.resolve():
            shutil.copy2(capture, output)
        return output
    raise ValueError(f"Unsupported capture format for report image: {capture.suffix}")


def rel_link(path: Path, report_path: Path) -> str:
    return path.resolve().relative_to(report_path.parent.resolve()).as_posix()


def metric_value(metrics: dict, section: str, key: str) -> str:
    value = metrics.get(section, {}).get(key)
    if isinstance(value, float):
        return f"{value:.3f}"
    if value is None:
        return ""
    return str(value)


def percentile_value(metrics: dict, section: str, key: str) -> str:
    value = metrics.get(section, {}).get("luma_percentiles", {}).get(key)
    if value is None:
        return ""
    return str(value)


def top_level_metric(metrics: dict, section: str, key: str) -> str:
    value = metrics.get(section, {}).get(key)
    if isinstance(value, float):
        return f"{value:.3f}"
    if value is None:
        return ""
    return str(value)


def write_report(metrics: dict, report_path: Path, images: list[tuple[str, Path]], status: dict | None = None) -> None:
    lines = [
        "# Render Validation Report",
        "",
        f"- Result: {'passed' if metrics.get('passed') else 'failed'}",
        "",
    ]
    if status:
        lines.extend(
            [
                "## Status",
                f"- Rendering: {status.get('rendering', '')}",
                f"- Compositing: {status.get('compositing', '')}",
                f"- Failure class: {status.get('failure_class', '')}",
                "",
            ]
        )
        failures = status.get("failures") or []
        if failures:
            lines.append("### Composition Failures")
            lines.extend(f"- {failure}" for failure in failures)
            lines.append("")
    errors = metrics.get("errors") or []
    if errors:
        lines.append("## Errors")
        lines.extend(f"- {error}" for error in errors)
        lines.append("")

    rows = [
        ["Metric", "Reference", "Candidate"],
        ["width", metric_value(metrics, "reference", "width"), metric_value(metrics, "candidate", "width")],
        ["height", metric_value(metrics, "reference", "height"), metric_value(metrics, "candidate", "height")],
        ["non_black_pixels", metric_value(metrics, "reference", "non_black_pixels"), metric_value(metrics, "candidate", "non_black_pixels")],
        ["near_visible_pixels", metric_value(metrics, "reference", "near_visible_pixels"), metric_value(metrics, "candidate", "near_visible_pixels")],
        ["luma_stddev", metric_value(metrics, "reference", "luma_stddev"), metric_value(metrics, "candidate", "luma_stddev")],
        ["luma_p01", percentile_value(metrics, "reference", "p01"), percentile_value(metrics, "candidate", "p01")],
        ["luma_p50", percentile_value(metrics, "reference", "p50"), percentile_value(metrics, "candidate", "p50")],
        ["luma_p95", percentile_value(metrics, "reference", "p95"), percentile_value(metrics, "candidate", "p95")],
        ["luma_p99", percentile_value(metrics, "reference", "p99"), percentile_value(metrics, "candidate", "p99")],
        ["visible_luma_mean", metric_value(metrics, "reference", "visible_luma_mean"), metric_value(metrics, "candidate", "visible_luma_mean")],
        ["visible_chroma_mean", metric_value(metrics, "reference", "visible_chroma_mean"), metric_value(metrics, "candidate", "visible_chroma_mean")],
    ]
    if metrics.get("spatial_luma_grid"):
        rows.extend(
            [
                ["spatial_grid_mean_abs_delta", "", top_level_metric(metrics, "spatial_luma_grid", "mean_abs_delta")],
                ["spatial_grid_rmse", "", top_level_metric(metrics, "spatial_luma_grid", "rmse")],
                ["spatial_grid_correlation", "", top_level_metric(metrics, "spatial_luma_grid", "correlation")],
            ]
        )
    widths = [max(len(row[index]) for row in rows) for index in range(len(rows[0]))]
    lines.append("## Metrics")
    lines.append("| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(rows[0])) + " |")
    lines.append("| " + " | ".join("-" * widths[index] for index in range(len(rows[0]))) + " |")
    for row in rows[1:]:
        lines.append("| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(row)) + " |")
    lines.append("")

    if images:
        lines.append("## Captures")
        for label, image in images:
            lines.append(f"### {label}")
            lines.append(f"![{label}]({rel_link(image, report_path)})")
            lines.append("")

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8", newline="\n")


def load_metrics(path: Path) -> dict:
    if not path.exists():
        return {
            "passed": False,
            "errors": [f"missing metrics JSON: {path}"],
            "reference": {},
            "candidate": {},
        }
    return json.loads(path.read_text(encoding="utf-8"))


def load_status(path: Path | None) -> dict | None:
    if path is None or not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def append_capture(images: list[tuple[str, Path]], label: str, capture: Path | None, output_dir: Path, errors: list[str]) -> None:
    if capture is None:
        return
    if not capture.exists():
        errors.append(f"missing {label.lower()} capture: {capture}")
        return
    images.append((label, convert_capture(capture, output_dir)))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metrics-json", type=Path, required=True)
    parser.add_argument("--candidate-capture", type=Path, required=True)
    parser.add_argument("--reference-capture", type=Path)
    parser.add_argument("--status-json", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    args = parser.parse_args()

    metrics = load_metrics(args.metrics_json)
    errors = metrics.setdefault("errors", [])
    images: list[tuple[str, Path]] = []
    append_capture(images, "Reference", args.reference_capture, args.output_dir, errors)
    append_capture(images, "Candidate", args.candidate_capture, args.output_dir, errors)
    if errors:
        metrics["passed"] = False
    write_report(metrics, args.markdown_output, images, load_status(args.status_json))
    print(f"Render report written: {args.markdown_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
