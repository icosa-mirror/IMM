#!/usr/bin/env python3
import argparse
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


def compare(reference, candidate):
    ref_w, ref_h, ref_pixels = read_image(reference)
    cand_w, cand_h, cand_pixels = read_image(candidate)
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


def main():
    parser = argparse.ArgumentParser(description="Compare IMM validation captures in PNG or PPM format.")
    parser.add_argument("reference")
    parser.add_argument("candidate")
    parser.add_argument("--max-mean-abs", type=float)
    parser.add_argument("--max-rms", type=float)
    parser.add_argument("--max-channel-diff", type=int)
    parser.add_argument("--max-differing-percent", type=float)
    args = parser.parse_args()

    try:
        metrics = compare(args.reference, args.candidate)
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
