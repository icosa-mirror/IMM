#!/usr/bin/env python3
"""Replace one native library in an APK before zip-aligning and signing it."""

from __future__ import annotations

import argparse
import shutil
import tempfile
import zipfile
from pathlib import Path, PurePosixPath


SIGNATURE_SUFFIXES = (".SF", ".RSA", ".DSA", ".EC")


def is_signature_entry(name: str) -> bool:
    path = PurePosixPath(name)
    return (
        len(path.parts) == 2
        and path.parts[0].upper() == "META-INF"
        and path.name.upper().endswith(SIGNATURE_SUFFIXES)
    )


def repack(input_apk: Path, library: Path, entry: str, output_apk: Path) -> None:
    if not input_apk.is_file():
        raise FileNotFoundError(f"input APK not found: {input_apk}")
    if not library.is_file():
        raise FileNotFoundError(f"native library not found: {library}")

    normalized_entry = PurePosixPath(entry).as_posix().lstrip("/")
    output_apk.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(dir=output_apk.parent) as temp_dir:
        temporary_output = Path(temp_dir) / output_apk.name
        replaced = False
        with zipfile.ZipFile(input_apk, "r") as source, zipfile.ZipFile(
            temporary_output, "w", allowZip64=True
        ) as destination:
            for info in source.infolist():
                if info.filename == normalized_entry:
                    replacement = zipfile.ZipInfo(info.filename, info.date_time)
                    replacement.compress_type = zipfile.ZIP_STORED
                    replacement.comment = info.comment
                    replacement.extra = info.extra
                    replacement.create_system = info.create_system
                    replacement.external_attr = info.external_attr
                    replacement.flag_bits = info.flag_bits & ~0x08
                    destination.writestr(replacement, library.read_bytes())
                    replaced = True
                elif not is_signature_entry(info.filename):
                    destination.writestr(info, source.read(info.filename))

        if not replaced:
            raise ValueError(f"APK does not contain native library entry: {normalized_entry}")
        shutil.move(temporary_output, output_apk)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-apk", required=True, type=Path)
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--entry", required=True)
    parser.add_argument("--output-apk", required=True, type=Path)
    args = parser.parse_args()

    repack(args.input_apk, args.library, args.entry, args.output_apk)
    print(f"Repacked {args.entry} into {args.output_apk}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
