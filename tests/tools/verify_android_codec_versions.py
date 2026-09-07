"""Reject stale codec dependencies in the packaged Android stroke reader."""

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    args = parser.parse_args()
    binary = args.library.read_bytes()
    if not binary.startswith(b"\x7fELF"):
        parser.error(f"{args.library} is not an ELF library")

    # Match the upstream runtime release markers, not source paths or filenames.
    # Update these alongside the vendored releases documented in thirdparty/CODECS.md.
    expected = {
        "libpng 1.6.58": b"libpng version 1.6.58",
        "Vorbis 1.3.7": b"Xiph.Org libVorbis I 20200704 (Reducing Environment)",
    }
    for name, marker in expected.items():
        if marker not in binary:
            parser.error(f"{args.library} is missing the {name} release marker; rebuild its native dependencies")
        print(f"[IMM-CODEC-VERIFY] Found {name} in {args.library.name}")


if __name__ == "__main__":
    main()
