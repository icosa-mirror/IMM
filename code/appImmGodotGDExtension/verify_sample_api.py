#!/usr/bin/env python3
"""Verify that the Godot sample only calls APIs exposed by ImmViewerNode.

This is a lightweight guard for environments where the Godot editor/CLI is not
available. It does not replace a real GDExtension build or Godot project parse.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CODE_ROOT = ROOT / "code"
SAMPLE_CONTROLLER = CODE_ROOT / "ImmGodotSampleProject/scripts/sample_scene_controller.gd"
SCRIPT_STUB = CODE_ROOT / "ImmGodotSampleProject/addons/imm_viewer/imm_viewer_node.gd"
NATIVE_HEADER = CODE_ROOT / "appImmGodotGDExtension/src/imm_viewer_node.h"
NATIVE_CPP = CODE_ROOT / "appImmGodotGDExtension/src/imm_viewer_node.cpp"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def public_native_methods(header: str) -> set[str]:
    return set(
        re.findall(
            r"\b(?:void|bool|int|int64_t|double|float|String|NodePath|Color|Dictionary|Transform3D|PackedFloat32Array|PackedInt32Array)"
            r"\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(",
            header,
        )
    )


def script_methods(script: str) -> set[str]:
    return set(re.findall(r"^func\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(", script, re.MULTILINE))


def native_bound_methods(cpp: str) -> set[str]:
    return set(re.findall(r'ClassDB::bind_method\(D_METHOD\("([^"]+)"', cpp))


def native_properties(cpp: str) -> set[str]:
    return set(re.findall(r'ADD_PROPERTY\(PropertyInfo\([^,]+,\s*"([^"]+)"', cpp))


def native_signals(cpp: str) -> set[str]:
    return set(re.findall(r'ADD_SIGNAL\(MethodInfo\("([^"]+)"', cpp))


def script_properties(script: str) -> set[str]:
    exported = set(re.findall(r"^@export(?:_[a-z_]+)?(?:\([^)]*\))?\s+var\s+([A-Za-z_][A-Za-z0-9_]*)", script, re.MULTILINE))
    plain = set(re.findall(r"^var\s+([A-Za-z_][A-Za-z0-9_]*)", script, re.MULTILINE))
    return exported | plain


def script_signals(script: str) -> set[str]:
    return set(re.findall(r"^signal\s+([A-Za-z_][A-Za-z0-9_]*)", script, re.MULTILINE))


def sample_calls(sample: str) -> set[str]:
    return set(re.findall(r"\bviewer\.([A-Za-z_][A-Za-z0-9_]*)\s*\(", sample))


def sample_properties(sample: str) -> set[str]:
    names = set(re.findall(r"\bviewer\.([A-Za-z_][A-Za-z0-9_]*)\b", sample))
    return names - sample_calls(sample)


def main() -> int:
    sample = read(SAMPLE_CONTROLLER)
    script = read(SCRIPT_STUB)
    header = read(NATIVE_HEADER)
    cpp = read(NATIVE_CPP)

    calls = sample_calls(sample)
    properties = sample_properties(sample)

    native_methods = public_native_methods(header)
    stub_methods = script_methods(script)
    bound_methods = native_bound_methods(cpp)
    native_props = native_properties(cpp)
    stub_props = script_properties(script)
    native_sigs = native_signals(cpp)
    stub_sigs = script_signals(script)

    failures: list[str] = []

    for name in sorted(native_sigs - stub_sigs):
        failures.append(f"native class exposes signal {name}, but script stub does not define it")
    for name in sorted(stub_sigs - native_sigs):
        failures.append(f"script stub defines signal {name}, but native class does not expose it")

    for name in sorted(calls - native_methods):
        failures.append(f"sample calls viewer.{name}(), but native header does not declare it")
    for name in sorted(calls - bound_methods):
        failures.append(f"sample calls viewer.{name}(), but native class does not bind it")
    for name in sorted(calls - stub_methods):
        failures.append(f"sample calls viewer.{name}(), but script stub does not define it")

    for name in sorted(properties):
        if name in native_sigs or name in stub_sigs:
            continue
        if name not in native_props:
            failures.append(f"sample reads viewer.{name}, but native class does not expose it as a property or signal")
        if name not in stub_props:
            failures.append(f"sample reads viewer.{name}, but script stub does not expose it as a property or signal")

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print("Sample ImmViewerNode API parity ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
