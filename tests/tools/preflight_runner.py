#!/usr/bin/env python3
"""Collect CI runner preflight diagnostics before hardware smoke jobs."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def run_command(command: list[str], timeout_seconds: int = 20) -> dict:
    try:
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
        return {
            "command": command,
            "exit_code": completed.returncode,
            "stdout": completed.stdout[-4000:],
            "stderr": completed.stderr[-4000:],
        }
    except Exception as exc:
        return {
            "command": command,
            "exit_code": None,
            "error": f"{type(exc).__name__}: {exc}",
        }


def parse_adb_devices(output: str) -> list[dict]:
    devices = []
    for line in output.splitlines():
        line = line.strip()
        if not line or line.startswith("List of devices"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        devices.append({"serial": parts[0], "state": parts[1], "details": parts[2:]})
    return devices


def adb_shell(adb: str, args: list[str], timeout_seconds: int = 20) -> dict:
    return run_command([adb, "shell", *args], timeout_seconds=timeout_seconds)


def collect_adb_health(adb: str) -> dict:
    health = {
        "battery": adb_shell(adb, ["dumpsys", "battery"]),
        "power": adb_shell(adb, ["dumpsys", "power"]),
        "activity": adb_shell(adb, ["dumpsys", "activity", "activities"]),
        "packages": {},
    }
    for package in ["org.linuxfoundation.imm.player", "org.linuxfoundation.imm.godot.sample"]:
        health["packages"][package] = adb_shell(adb, ["pm", "path", package])
    return health


def command_info(name: str) -> dict:
    path = shutil.which(name)
    info = {"name": name, "path": path, "found": path is not None}
    if path:
        version_args = {
            "adb": ["adb", "version"],
            "cmake": ["cmake", "--version"],
            "git": ["git", "--version"],
            "glslangValidator": ["glslangValidator", "--version"],
            "godot": ["godot", "--version"],
            "msbuild": ["msbuild", "-version"],
            "python": [sys.executable, "--version"],
            "spirv-val": ["spirv-val", "--version"],
        }.get(name)
        if version_args:
            info["version"] = run_command(version_args)
    return info


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--profile",
        required=True,
        choices=[
            "android-device",
            "ios-device",
            "macos-godot-metal",
            "windows-gpu-vulkan",
            "windows-godot-vulkan",
            "windows-godot-openxr-vr",
            "windows-opengl-vr",
            "windows-openxr-vr",
            "unity",
            "unity-windows-directx",
            "unity-windows-openxr-vr",
        ],
    )
    parser.add_argument("--require-command", action="append", default=[])
    parser.add_argument("--require-env", action="append", default=[])
    parser.add_argument("--adb-devices", action="store_true")
    parser.add_argument("--adb-health", action="store_true")
    args = parser.parse_args()

    commands = [command_info(name) for name in args.require_command]
    env = {name: os.environ.get(name) for name in args.require_env}
    diagnostics = {
        "schema": "imm-runner-preflight-v1",
        "profile": args.profile,
        "runner": {
            "os": os.environ.get("RUNNER_OS", platform.system()),
            "arch": os.environ.get("RUNNER_ARCH", platform.machine()),
            "name": os.environ.get("RUNNER_NAME", ""),
            "labels": os.environ.get("RUNNER_LABELS", ""),
        },
        "host": {
            "platform": platform.platform(),
            "python": sys.version,
        },
        "commands": commands,
        "environment": {name: {"set": value is not None, "value": value or ""} for name, value in env.items()},
        "checks": {},
    }

    errors: list[str] = []
    for info in commands:
        if not info["found"]:
            errors.append(f"Required command not found on PATH: {info['name']}")
    for name, value in env.items():
        if not value:
            errors.append(f"Required environment variable is not set: {name}")

    if args.adb_devices:
        adb = shutil.which("adb")
        if adb:
            adb_result = run_command([adb, "devices", "-l"])
            diagnostics["checks"]["adb_devices"] = adb_result
            diagnostics["checks"]["adb_device_list"] = parse_adb_devices(adb_result.get("stdout", ""))
            if adb_result.get("exit_code") != 0:
                errors.append("adb devices failed")
            elif not any(device["state"] == "device" for device in diagnostics["checks"]["adb_device_list"]):
                errors.append("adb reported no attached device in device state")
            if args.adb_health:
                diagnostics["checks"]["adb_health"] = collect_adb_health(adb)
        else:
            errors.append("adb device check requested but adb is missing")

    diagnostics["passed"] = not errors
    diagnostics["errors"] = errors
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(diagnostics, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        print(f"Preflight diagnostics written: {args.output}", file=sys.stderr)
        return 1

    print(f"Preflight diagnostics written: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
