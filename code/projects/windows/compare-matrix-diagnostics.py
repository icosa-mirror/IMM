#!/usr/bin/env python3
"""Compare Unity and Godot IMM matrix diagnostics JSON payloads."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any


UNITY_PREFIX = "IMM_UNITY_MATRIX_DIAGNOSTICS_JSON "
GODOT_PREFIX = "IMM_GODOT_MATRIX_DIAGNOSTICS_JSON "
UNITY_SCHEMA = "imm_unity_matrix_diagnostics_v1"
GODOT_SCHEMA = "imm_godot_matrix_diagnostics_v1"
EXTENDED_FLOAT_FIELDS = {
    "document_to_world": 16,
    "background_color": 4,
    "bounding_box_min": 3,
    "bounding_box_max": 3,
}
EXTENDED_SCALAR_FIELDS = [
    "document_name",
    "document_size_bytes",
    "document_loading_state",
    "document_playback_state",
    "bounding_box_valid",
    "spawn_area_count",
    "active_spawn_area_index",
    "active_spawn_area_id",
]
ENGINE_VERSION_FIELDS = {
    "unity": "unity_version",
    "godot": "godot_version",
}


def load_payload(path: Path, prefix: str) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8").strip()
    for line in reversed(text.splitlines()):
        stripped = line.strip()
        if stripped.startswith(prefix):
            return json.loads(stripped[len(prefix):])
        if stripped.startswith("{"):
            return json.loads(stripped)
    raise ValueError(f"{path} does not contain JSON or a {prefix.strip()} line")


def get_matrix(payload: dict[str, Any], key: str) -> list[float]:
    values = payload.get(key)
    if not isinstance(values, list) or len(values) != 16:
        raise ValueError(f"{key} must be a 16-float array")
    return [float(value) for value in values]


def get_float_array(payload: dict[str, Any], key: str, size: int) -> list[float] | None:
    values = payload.get(key)
    if values is None:
        return None
    if not isinstance(values, list) or len(values) != size:
        raise ValueError(f"{key} must be a {size}-float array")
    return [float(value) for value in values]


def require_schema(payload: dict[str, Any], expected: str, label: str) -> None:
    actual = payload.get("schema")
    if actual != expected:
        raise ValueError(f"{label} diagnostics schema must be {expected!r}, got {actual!r}")


def get_camera_id(payload: dict[str, Any], *keys: str) -> int | None:
    for key in keys:
        value = payload.get(key)
        if value is not None:
            return int(value)
    return None


def compare_matrix(name: str, unity: list[float], godot: list[float], tolerance: float) -> tuple[bool, str]:
    worst_index = -1
    worst_delta = -1.0
    for index, (unity_value, godot_value) in enumerate(zip(unity, godot)):
        delta = abs(unity_value - godot_value)
        if delta > worst_delta:
            worst_delta = delta
            worst_index = index

    ok = worst_delta <= tolerance or math.isclose(worst_delta, tolerance)
    status = "ok" if ok else "mismatch"
    return ok, f"{name}: {status}, max_delta={worst_delta:.9g}, index={worst_index}, unity={unity[worst_index]:.9g}, godot={godot[worst_index]:.9g}"


def parse_check_message(message: str) -> dict[str, Any]:
    name, _, detail = message.partition(": ")
    result: dict[str, Any] = {"name": name, "message": message}
    for part in detail.split(", "):
        key, separator, value = part.partition("=")
        if not separator:
            if part in {"ok", "mismatch", "missing"}:
                result["status"] = part
            continue
        try:
            if any(character in value for character in [".", "e", "E"]):
                result[key] = float(value)
            else:
                result[key] = int(value)
        except ValueError:
            result[key] = value
    return result


def compare_float_array(name: str, unity: list[float], godot: list[float], tolerance: float) -> tuple[bool, str]:
    if len(unity) != len(godot):
        return False, f"{name}: mismatch, unity_len={len(unity)}, godot_len={len(godot)}"
    worst_index = -1
    worst_delta = -1.0
    for index, (unity_value, godot_value) in enumerate(zip(unity, godot)):
        delta = abs(unity_value - godot_value)
        if delta > worst_delta:
            worst_delta = delta
            worst_index = index
    ok = worst_delta <= tolerance or math.isclose(worst_delta, tolerance)
    status = "ok" if ok else "mismatch"
    return ok, f"{name}: {status}, max_delta={worst_delta:.9g}, index={worst_index}, unity={unity[worst_index]:.9g}, godot={godot[worst_index]:.9g}"


def compare_optional_float_array(unity: dict[str, Any], godot: dict[str, Any], key: str, size: int, tolerance: float) -> tuple[bool, str] | None:
    unity_values = get_float_array(unity, key, size)
    godot_values = get_float_array(godot, key, size)
    if unity_values is None or godot_values is None:
        return None
    return compare_float_array(key, unity_values, godot_values, tolerance)


def compare_optional_scalar(unity: dict[str, Any], godot: dict[str, Any], key: str) -> tuple[bool, str] | None:
    if key not in unity or key not in godot:
        return None
    ok = unity[key] == godot[key]
    status = "ok" if ok else "mismatch"
    return ok, f"{key}: {status}, unity={unity[key]}, godot={godot[key]}"


def require_extended_fields(unity: dict[str, Any], godot: dict[str, Any]) -> list[tuple[bool, str]]:
    checks: list[tuple[bool, str]] = []
    for key, size in EXTENDED_FLOAT_FIELDS.items():
        for label, payload in [("unity", unity), ("godot", godot)]:
            values = payload.get(key)
            ok = isinstance(values, list) and len(values) == size
            status = "ok" if ok else "missing"
            checks.append((ok, f"{key}.{label}: {status}"))
    for key in EXTENDED_SCALAR_FIELDS:
        for label, payload in [("unity", unity), ("godot", godot)]:
            ok = key in payload
            status = "ok" if ok else "missing"
            checks.append((ok, f"{key}.{label}: {status}"))
    for label, key in ENGINE_VERSION_FIELDS.items():
        payload = unity if label == "unity" else godot
        ok = isinstance(payload.get(key), str) and bool(payload.get(key).strip())
        status = "ok" if ok else "missing"
        checks.append((ok, f"{key}.{label}: {status}"))
    return checks


def write_summary(
    path: Path,
    *,
    passed: bool,
    tolerance: float,
    require_extended: bool,
    unity: dict[str, Any] | None = None,
    godot: dict[str, Any] | None = None,
    checks: list[tuple[bool, str]] | None = None,
    camera_ok: bool | None = None,
    unity_camera: int | None = None,
    godot_camera: int | None = None,
    error: str | None = None,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload: dict[str, Any] = {
        "passed": passed,
        "tolerance": tolerance,
        "require_extended": require_extended,
        "unity_schema": unity.get("schema") if unity is not None else None,
        "godot_schema": godot.get("schema") if godot is not None else None,
        "unity_version": unity.get("unity_version") if unity is not None else None,
        "godot_version": godot.get("godot_version") if godot is not None else None,
        "unity_camera_id": unity_camera,
        "godot_camera_id": godot_camera,
        "checks": [
            {"ok": ok, **parse_check_message(message)}
            for ok, message in checks or []
        ],
        "camera_check": {
            "ok": camera_ok,
            "unity": unity_camera,
            "godot": godot_camera,
        },
    }
    if error is not None:
        payload["error"] = error
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unity", required=True, type=Path, help="Unity diagnostics JSON file or log containing IMM_UNITY_MATRIX_DIAGNOSTICS_JSON")
    parser.add_argument("--godot", required=True, type=Path, help="Godot diagnostics JSON file or log containing IMM_GODOT_MATRIX_DIAGNOSTICS_JSON")
    parser.add_argument("--tolerance", type=float, default=1e-4)
    parser.add_argument("--require-extended", action="store_true", help="Require and compare document/background/bounds/spawn parity fields")
    parser.add_argument("--summary-json", type=Path, help="Optional path for a machine-readable comparison summary")
    args = parser.parse_args()

    try:
        unity = load_payload(args.unity, UNITY_PREFIX)
        godot = load_payload(args.godot, GODOT_PREFIX)
        require_schema(unity, UNITY_SCHEMA, "Unity")
        require_schema(godot, GODOT_SCHEMA, "Godot")
    except Exception as exc:
        if args.summary_json is not None:
            write_summary(
                args.summary_json,
                passed=False,
                tolerance=args.tolerance,
                require_extended=args.require_extended,
                error=str(exc),
            )
        print(f"error: {exc}", file=sys.stderr)
        return 1

    unity_camera: int | None = None
    godot_camera: int | None = None
    try:
        unity_camera = get_camera_id(unity, "camera_id")
        godot_camera = get_camera_id(godot, "camera_id", "last_matrix_camera_id")
        checks = [
            compare_matrix("world_to_head", get_matrix(unity, "world_to_head"), get_matrix(godot, "world_to_head"), args.tolerance),
            compare_matrix("projection", get_matrix(unity, "projection"), get_matrix(godot, "projection"), args.tolerance),
        ]
        if args.require_extended:
            checks.extend(require_extended_fields(unity, godot))

        for key, size in EXTENDED_FLOAT_FIELDS.items():
            optional_check = compare_optional_float_array(unity, godot, key, size, args.tolerance)
            if optional_check is not None:
                checks.append(optional_check)
        for key in EXTENDED_SCALAR_FIELDS:
            optional_check = compare_optional_scalar(unity, godot, key)
            if optional_check is not None:
                checks.append(optional_check)
    except Exception as exc:
        if args.summary_json is not None:
            write_summary(
                args.summary_json,
                passed=False,
                tolerance=args.tolerance,
                require_extended=args.require_extended,
                unity=unity,
                godot=godot,
                camera_ok=None,
                unity_camera=unity_camera,
                godot_camera=godot_camera,
                error=str(exc),
            )
        print(f"error: {exc}", file=sys.stderr)
        return 1

    for _, message in checks:
        print(message)

    camera_ok = True
    if unity_camera is not None and godot_camera is not None and int(unity_camera) != int(godot_camera):
        print(f"camera_id: mismatch, unity={unity_camera}, godot={godot_camera}")
        camera_ok = False

    passed = all(ok for ok, _ in checks) and camera_ok
    if args.summary_json is not None:
        write_summary(
            args.summary_json,
            passed=passed,
            tolerance=args.tolerance,
            require_extended=args.require_extended,
            unity=unity,
            godot=godot,
            checks=checks,
            camera_ok=camera_ok,
            unity_camera=unity_camera,
            godot_camera=godot_camera,
        )

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
