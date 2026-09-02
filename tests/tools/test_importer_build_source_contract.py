#!/usr/bin/env python3
"""Guard shared importer source coverage across native build definitions."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    paint_geometry_source = "libImmImporter/src/document/layerPaint/paintGeometry.cpp"

    for project in ["macos", "ios"]:
        cmake = (ROOT / f"code/projects/{project}/CMakeLists.txt").read_text(encoding="utf-8")
        assert paint_geometry_source in cmake, f"{project} omits {paint_geometry_source}"

    android_cmake = (ROOT / "code/libImmImporter/CMakeLists.txt").read_text(encoding="utf-8")
    assert "${libImm-dir}/src/document/layerPaint/paintGeometry.cpp" in android_cmake

    windows_project = (ROOT / "code/libImmImporter/libImmImporter.vcxproj").read_text(
        encoding="utf-8"
    )
    assert 'ClCompile Include="src\\document\\layerPaint\\paintGeometry.cpp"' in windows_project

    for suffix in ["h", "cpp"]:
        source = (
            ROOT / f"code/libImmImporter/src/document/layerPaint/paintGeometry.{suffix}"
        ).read_text(encoding="utf-8")
        assert "namespace ImmImporter::PaintGeometry" not in source

    print("Importer native build source contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
