#!/usr/bin/env python3
import argparse
import json
import math
import struct
import sys
import zlib
from pathlib import Path


def read_ppm(path):
    data = Path(path).read_bytes()
    pos = 0

    def token():
        nonlocal pos
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if pos < len(data) and data[pos] == ord("#"):
            while pos < len(data) and data[pos] not in b"\r\n":
                pos += 1
            return token()
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        return data[start:pos]

    magic = token()
    if magic != b"P6":
        raise ValueError(f"{path}: expected P6 PPM")
    width = int(token())
    height = int(token())
    max_value = int(token())
    if max_value != 255:
        raise ValueError(f"{path}: expected 8-bit PPM, max value is {max_value}")
    if pos < len(data) and data[pos] in b" \t\r\n":
        pos += 1
    expected = width * height * 3
    pixels = data[pos:pos + expected]
    if len(pixels) != expected:
        raise ValueError(f"{path}: expected {expected} pixel bytes, found {len(pixels)}")
    return width, height, pixels


def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png(path):
    data = Path(path).read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"{path}: expected PNG signature")

    pos = 8
    width = height = color_type = bit_depth = interlace = None
    compressed = bytearray()

    while pos < len(data):
        if pos + 8 > len(data):
            raise ValueError(f"{path}: truncated PNG chunk header")
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        pos += 8
        chunk = data[pos:pos + length]
        pos += length + 4

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", chunk)
            if bit_depth != 8:
                raise ValueError(f"{path}: only 8-bit PNG is supported")
            if color_type not in (2, 6):
                raise ValueError(f"{path}: only RGB/RGBA PNG is supported")
            if compression != 0 or filter_method != 0 or interlace != 0:
                raise ValueError(f"{path}: unsupported PNG compression/filter/interlace settings")
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise ValueError(f"{path}: missing IHDR")

    channels = 3 if color_type == 2 else 4
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))
    expected = (stride + 1) * height
    if len(raw) != expected:
        raise ValueError(f"{path}: expected {expected} decompressed bytes, found {len(raw)}")

    recon = bytearray(height * stride)
    src = 0
    for y in range(height):
        filter_type = raw[src]
        src += 1
        row_start = y * stride
        prev_start = row_start - stride
        for x in range(stride):
            value = raw[src + x]
            left = recon[row_start + x - channels] if x >= channels else 0
            up = recon[prev_start + x] if y > 0 else 0
            up_left = recon[prev_start + x - channels] if y > 0 and x >= channels else 0
            if filter_type == 0:
                recon[row_start + x] = value
            elif filter_type == 1:
                recon[row_start + x] = (value + left) & 255
            elif filter_type == 2:
                recon[row_start + x] = (value + up) & 255
            elif filter_type == 3:
                recon[row_start + x] = (value + ((left + up) // 2)) & 255
            elif filter_type == 4:
                recon[row_start + x] = (value + paeth(left, up, up_left)) & 255
            else:
                raise ValueError(f"{path}: unsupported PNG row filter {filter_type}")
        src += stride

    if channels == 3:
        return width, height, bytes(recon)

    rgb = bytearray(width * height * 3)
    for i in range(width * height):
        rgb[3 * i:3 * i + 3] = recon[4 * i:4 * i + 3]
    return width, height, bytes(rgb)


def read_image(path):
    suffix = Path(path).suffix.lower()
    if suffix == ".ppm":
        return read_ppm(path)
    if suffix == ".png":
        return read_png(path)
    raise ValueError(f"{path}: unsupported extension; use .png or .ppm")


def png_chunk(chunk_type, payload):
    return (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", zlib.crc32(chunk_type + payload) & 0xFFFFFFFF)
    )


def write_png_rgb(path, width, height, pixels):
    if len(pixels) != width * height * 3:
        raise ValueError(f"{path}: expected {width * height * 3} RGB bytes, got {len(pixels)}")

    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        row_start = y * stride
        raw.extend(pixels[row_start:row_start + stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + png_chunk(b"IEND", b"")
    )
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(data)


def compare_pixels(ref_w, ref_h, ref_pixels, cand_w, cand_h, cand_pixels):
    if (ref_w, ref_h) != (cand_w, cand_h):
        raise ValueError(f"dimension mismatch: reference={ref_w}x{ref_h} candidate={cand_w}x{cand_h}")

    total_abs = 0
    total_sq = 0
    max_abs = 0
    differing_pixels = 0
    channel_count = len(ref_pixels)
    pixel_count = ref_w * ref_h

    for i in range(0, channel_count, 3):
        pixel_differs = False
        for c in range(3):
            diff = abs(ref_pixels[i + c] - cand_pixels[i + c])
            total_abs += diff
            total_sq += diff * diff
            max_abs = max(max_abs, diff)
            if diff:
                pixel_differs = True
        if pixel_differs:
            differing_pixels += 1

    mean_abs = total_abs / channel_count
    rms = math.sqrt(total_sq / channel_count)
    differing_percent = 100.0 * differing_pixels / pixel_count
    return {
        "width": ref_w,
        "height": ref_h,
        "pixels": pixel_count,
        "channels": channel_count,
        "mean_abs": mean_abs,
        "rms": rms,
        "max_abs": max_abs,
        "differing_pixels": differing_pixels,
        "differing_percent": differing_percent,
    }


def compare(reference, candidate):
    ref_w, ref_h, ref_pixels = read_image(reference)
    cand_w, cand_h, cand_pixels = read_image(candidate)
    return compare_pixels(ref_w, ref_h, ref_pixels, cand_w, cand_h, cand_pixels)


def create_diff_pixels(ref_pixels, cand_pixels, scale):
    diff_pixels = bytearray(len(ref_pixels))
    for i in range(len(ref_pixels)):
        diff_pixels[i] = min(255, abs(ref_pixels[i] - cand_pixels[i]) * scale)
    return bytes(diff_pixels)


def create_contact_sheet(ref_pixels, cand_pixels, diff_pixels, width, height):
    gutter = 8
    label_height = 0
    sheet_width = width * 3 + gutter * 2
    sheet_height = height + label_height
    sheet = bytearray([24, 24, 24] * sheet_width * sheet_height)

    panels = (ref_pixels, cand_pixels, diff_pixels)
    for panel_index, panel_pixels in enumerate(panels):
        dst_x = panel_index * (width + gutter)
        for y in range(height):
            src_row = y * width * 3
            dst_row = y * sheet_width * 3 + dst_x * 3
            sheet[dst_row:dst_row + width * 3] = panel_pixels[src_row:src_row + width * 3]
    return sheet_width, sheet_height, bytes(sheet)


def main():
    parser = argparse.ArgumentParser(description="Compare IMM validation captures in PNG or PPM format.")
    parser.add_argument("reference")
    parser.add_argument("candidate")
    parser.add_argument("--max-mean-abs", type=float)
    parser.add_argument("--max-rms", type=float)
    parser.add_argument("--max-channel-diff", type=int)
    parser.add_argument("--max-differing-percent", type=float)
    parser.add_argument("--json-output", help="Optional path for machine-readable comparison metrics.")
    parser.add_argument("--diff-output", help="Optional path for an RGB PNG absolute-difference image.")
    parser.add_argument("--diff-scale", type=int, default=4, help="Multiplier for visual diff pixels before clamping (default: 4).")
    parser.add_argument("--contact-sheet-output", help="Optional path for an RGB PNG containing reference, candidate, and amplified diff side by side.")
    args = parser.parse_args()

    try:
        ref_w, ref_h, ref_pixels = read_image(args.reference)
        cand_w, cand_h, cand_pixels = read_image(args.candidate)
        metrics = compare_pixels(ref_w, ref_h, ref_pixels, cand_w, cand_h, cand_pixels)
    except Exception as exc:
        print(f"capture comparison failed: {exc}", file=sys.stderr)
        return 2

    print(
        "capture comparison: "
        f"{metrics['width']}x{metrics['height']} "
        f"meanAbs={metrics['mean_abs']:.6f} "
        f"rms={metrics['rms']:.6f} "
        f"maxChannelDiff={metrics['max_abs']} "
        f"differingPixels={metrics['differing_pixels']}/{metrics['pixels']} "
        f"({metrics['differing_percent']:.6f}%)"
    )

    if args.json_output:
        output_path = Path(args.json_output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output = {
            "reference": str(Path(args.reference)),
            "candidate": str(Path(args.candidate)),
            "metrics": metrics,
            "limits": {
                "max_mean_abs": args.max_mean_abs,
                "max_rms": args.max_rms,
                "max_channel_diff": args.max_channel_diff,
                "max_differing_percent": args.max_differing_percent,
            },
        }
        output_path.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n")

    if args.diff_scale < 1:
        print("--diff-scale must be >= 1", file=sys.stderr)
        return 2

    diff_pixels = None
    if args.diff_output or args.contact_sheet_output:
        try:
            diff_pixels = create_diff_pixels(ref_pixels, cand_pixels, args.diff_scale)
        except Exception as exc:
            print(f"diff image generation failed: {exc}", file=sys.stderr)
            return 2

    if args.diff_output:
        try:
            write_png_rgb(args.diff_output, metrics["width"], metrics["height"], diff_pixels)
        except Exception as exc:
            print(f"diff image write failed: {exc}", file=sys.stderr)
            return 2

    if args.contact_sheet_output:
        try:
            sheet_width, sheet_height, sheet_pixels = create_contact_sheet(ref_pixels, cand_pixels, diff_pixels, metrics["width"], metrics["height"])
            write_png_rgb(args.contact_sheet_output, sheet_width, sheet_height, sheet_pixels)
        except Exception as exc:
            print(f"contact sheet write failed: {exc}", file=sys.stderr)
            return 2

    failed = False
    if args.max_mean_abs is not None and metrics["mean_abs"] > args.max_mean_abs:
        print(f"meanAbs exceeds limit: {metrics['mean_abs']:.6f} > {args.max_mean_abs}", file=sys.stderr)
        failed = True
    if args.max_rms is not None and metrics["rms"] > args.max_rms:
        print(f"rms exceeds limit: {metrics['rms']:.6f} > {args.max_rms}", file=sys.stderr)
        failed = True
    if args.max_channel_diff is not None and metrics["max_abs"] > args.max_channel_diff:
        print(f"maxChannelDiff exceeds limit: {metrics['max_abs']} > {args.max_channel_diff}", file=sys.stderr)
        failed = True
    if args.max_differing_percent is not None and metrics["differing_percent"] > args.max_differing_percent:
        print(
            f"differingPercent exceeds limit: {metrics['differing_percent']:.6f} > {args.max_differing_percent}",
            file=sys.stderr,
        )
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
