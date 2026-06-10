#!/usr/bin/env python3
"""Write a human-readable render comparison report with viewable PNG captures."""

from __future__ import annotations

import argparse
import json
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
        return capture
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


def write_report(metrics: dict, report_path: Path, images: list[tuple[str, Path]]) -> None:
    lines = [
        "# Render Validation Report",
        "",
        f"- Result: {'passed' if metrics.get('passed') else 'failed'}",
        "",
    ]
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
        ["visible_luma_mean", metric_value(metrics, "reference", "visible_luma_mean"), metric_value(metrics, "candidate", "visible_luma_mean")],
        ["visible_chroma_mean", metric_value(metrics, "reference", "visible_chroma_mean"), metric_value(metrics, "candidate", "visible_chroma_mean")],
    ]
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metrics-json", type=Path, required=True)
    parser.add_argument("--candidate-capture", type=Path, required=True)
    parser.add_argument("--reference-capture", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    args = parser.parse_args()

    metrics = json.loads(args.metrics_json.read_text(encoding="utf-8"))
    images: list[tuple[str, Path]] = []
    if args.reference_capture:
        images.append(("Reference", convert_capture(args.reference_capture, args.output_dir)))
    images.append(("Candidate", convert_capture(args.candidate_capture, args.output_dir)))
    write_report(metrics, args.markdown_output, images)
    print(f"Render report written: {args.markdown_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
