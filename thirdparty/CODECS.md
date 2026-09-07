# Bundled source codecs

Android, iOS, and macOS CMake builds use these upstream source releases:

| Dependency | Source directory | Download | Archive SHA-256 |
| --- | --- | --- | --- |
| libpng 1.6.58 | `libpng-1.6.58` | https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.58.tar.gz | `a9d4df463d36a6e5f9c29bd6f4967312d17e996c1854f3511f833924eb1993cf` |
| Vorbis 1.3.7 | `ogg/libvorbis-1.3.7` | https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz | `b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b` |

The source trees are unmodified upstream distributions. Licenses are in
`libpng-1.6.58/LICENSE.md` and `ogg/libvorbis-1.3.7/COPYING`.
The Vorbis archive checksum matches the Xiph release announcement:
https://lists.xiph.org/pipermail/vorbis/2020-July/027820.html.

These replace libpng 1.6.37 and Vorbis 1.3.5. libpng 1.6.58 includes the
CVE-2026-33416 fix and subsequent 1.6.57/1.6.58 corrections to the chunk setter
and palette APIs. See https://www.libpng.org/pub/png/libpng.html and the bundled
release notes. Vorbis 1.3.7 meets the Android scanner's minimum fixed version.

`libImmStrokeReader.so` statically links both libraries, so applications must
ship a rebuilt native plugin to receive the fixes. CI checks the built Android
plugin's upstream release markers with `tests/tools/verify_android_codec_versions.py`.
This guards against stale build outputs; it is not a general vulnerability scan.
Update that check when upgrading either source release.

Windows projects use separate prebuilt dependencies in `libpng`, `libvorbis`,
and `libogg`; these source updates do not rebuild those Windows DLLs.
