#!/usr/bin/env python3
"""Collect a short sample from an installed CI simulator app, then terminate it."""
import argparse
import json
import plistlib
import subprocess
import sys
from pathlib import Path


def collect_launch_diagnostics(udid: str, executable: str, output: Path) -> None:
    """Retain launch evidence even when simctl never attaches a console."""
    output.mkdir(parents=True, exist_ok=True)
    commands = {
        "simulator-devices.json": ["xcrun", "simctl", "list", "devices", "--json"],
        "simulator-processes.txt": ["ps", "-axo", "pid,ppid,state,%cpu,etime,command"],
        "simulator-system.log": ["xcrun", "simctl", "spawn", udid, "log", "show",
                                 "--style", "compact", "--last", "5m", "--predicate",
                                 f'process == "{executable}" OR process == "SpringBoard"'],
    }
    for name, command in commands.items():
        with (output / name).open("wb") as stream:
            try:
                subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                               timeout=15, check=False)
            except subprocess.TimeoutExpired:
                stream.write(b"\n[IMM-FPS-IOS] Diagnostic command timed out.\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--udid", required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--product", choices=("standalone", "unity", "godot"), required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    with (args.app / "Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    bundle = info["CFBundleIdentifier"]
    devices = json.loads(subprocess.check_output(["xcrun", "simctl", "list", "devices", "--json"], text=True))
    device = next(device for group in devices["devices"].values() for device in group if device["udid"] == args.udid)
    booted_here = device["state"] != "Booted"
    if booted_here:
        subprocess.run(["xcrun", "simctl", "boot", args.udid], check=True)
    subprocess.run(["xcrun", "simctl", "bootstatus", args.udid, "-b"], check=True)
    container = Path(subprocess.check_output(
        ["xcrun", "simctl", "get_app_container", args.udid, bundle, "data"], text=True).strip())
    runner = Path(__file__).with_name("run_frame_sample.py")
    command = [sys.executable, str(runner), "--product", args.product, "--graphics-api", "Metal",
               "--platform", "ios", "--runtime-kind", "simulator", "--env-prefix", "SIMCTL_CHILD_",
               "--sample-path", str(container / "Documents/imm-performance/performance-measurements-raw.json"),
               "--executable-artifact", str(args.app / info["CFBundleExecutable"]),
               "--output-dir", str(args.output_dir), "--timeout", "120", "--stop-after-sample"]
    if args.product == "unity":
        command.append("--render-requests")
    command.extend(["--", "xcrun", "simctl", "launch", "--console", "--terminate-running-process", args.udid, bundle])
    if args.product == "godot":
        # The simulator lacks presentDrawable:afterMinimumDuration:. Match the
        # existing Metal correctness launch; retain real wall-clock sampling.
        command.extend(["--disable-vsync", "--rendering-driver", "metal", "--rendering-method", "mobile",
                        "--", "--imm-godot-ci-scene=sample"])
    try:
        status = subprocess.run(command, check=False).returncode
        if status:
            collect_launch_diagnostics(args.udid, info["CFBundleExecutable"], args.output_dir)
        return status
    finally:
        subprocess.run(["xcrun", "simctl", "terminate", args.udid, bundle],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        if booted_here:
            subprocess.run(["xcrun", "simctl", "shutdown", args.udid], check=False)


if __name__ == "__main__":
    raise SystemExit(main())
