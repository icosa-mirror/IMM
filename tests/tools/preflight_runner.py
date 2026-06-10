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
    parser.add_argument("--profile", required=True, choices=["android-device", "macos-godot-metal", "windows-gpu-vulkan", "windows-godot-vulkan", "unity"])
    parser.add_argument("--require-command", action="append", default=[])
    parser.add_argument("--require-env", action="append", default=[])
    parser.add_argument("--adb-devices", action="store_true")
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
            if adb_result.get("exit_code") != 0:
                errors.append("adb devices failed")
            elif "\tdevice" not in adb_result.get("stdout", ""):
                errors.append("adb reported no attached device in device state")
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
