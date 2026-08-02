#!/usr/bin/env python3

from __future__ import annotations

import tempfile
import zipfile
from pathlib import Path

from repack_android_native_library import repack


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        source = root / "source.apk"
        output = root / "output.apk"
        library = root / "libImmUnityPlugin.so"
        entry = "lib/arm64-v8a/libImmUnityPlugin.so"
        library.write_bytes(b"new-library")

        with zipfile.ZipFile(source, "w") as apk:
            apk.writestr("AndroidManifest.xml", b"manifest")
            apk.writestr(entry, b"old-library", compress_type=zipfile.ZIP_STORED)
            apk.writestr("META-INF/OLD.SF", b"stale-signature")
            apk.writestr("META-INF/OLD.RSA", b"stale-signature")

        repack(source, library, entry, output)

        with zipfile.ZipFile(output, "r") as apk:
            assert apk.read("AndroidManifest.xml") == b"manifest"
            assert apk.read(entry) == b"new-library"
            assert apk.getinfo(entry).compress_type == zipfile.ZIP_STORED
            assert "META-INF/OLD.SF" not in apk.namelist()
            assert "META-INF/OLD.RSA" not in apk.namelist()

    print("Android native-library APK repack verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
