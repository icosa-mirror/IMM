#!/usr/bin/env python3
"""Run one real workload and retain frame evidence, logs and input hashes."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import time
from pathlib import Path

from verify_frame_sample import verify


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--product", choices=("standalone", "unity", "godot"), required=True)
    parser.add_argument("--fixture", type=Path, default=Path("exampleImmFiles/sample1.imm"))
    parser.add_argument("--graphics-api", required=True)
    parser.add_argument("--cwd", type=Path)
    parser.add_argument("--sample-path", type=Path, help="Producer location when collecting from a simulator container")
    parser.add_argument("--env-prefix", default="")
    parser.add_argument("--executable-artifact", type=Path, help="Actual app binary when the command is a launcher")
    parser.add_argument("--platform", dest="target_platform")
    parser.add_argument("--runtime-kind", choices=("host", "simulator"), default="host")
    parser.add_argument("--render-requests", action="store_true", help="Unity: one labelled full-size request per engine tick for hidden CI windows")
    parser.add_argument("--minimum-fps", type=float)
    parser.add_argument("--stop-after-sample", action="store_true", help="Stop this owned test process after its evidence is complete")
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("a workload command is required after --")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    raw_path = output / "performance-measurements-raw.json"
    raw_path.unlink(missing_ok=True)  # An old sample must never pass a new run.
    producer_path = args.sample_path.resolve() if args.sample_path else raw_path
    producer_path.parent.mkdir(parents=True, exist_ok=True)
    producer_path.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment[f"{args.env_prefix}IMM_PERFORMANCE_OUTPUT"] = str(producer_path)
    environment[f"{args.env_prefix}IMM_UNITY_DOCUMENT_NAME"] = args.fixture.name
    if args.render_requests:
        environment[f"{args.env_prefix}IMM_PERFORMANCE_RENDER_REQUESTS"] = "1"
    for flag in ("IMM_UNITY_URP_SMOKE", "IMM_UNITY_SMOKE_CAPTURE_PATH",
                 "IMM_GODOT_VISUAL_SMOKE", "IMM_GODOT_SAMPLE_PLAY_SMOKE"):
        environment.pop(flag, None)
        environment.pop(f"{args.env_prefix}{flag}", None)
    started = time.monotonic()
    error = None
    with (output / "runtime.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=args.cwd, env=environment, stdout=log, stderr=subprocess.STDOUT)
        try:
            while process.poll() is None:
                if args.stop_after_sample and producer_path.exists():
                    try:
                        json.loads(producer_path.read_text(encoding="utf-8"))
                        break
                    except (OSError, ValueError):
                        pass  # The producer may still be finishing its write.
                if time.monotonic() - started > args.timeout:
                    error = f"Workload exceeded {args.timeout:g} seconds"
                    break
                time.sleep(0.25)
        finally:
            if process.poll() is None:
                process.terminate()  # Only the process created above; no shared Editor.
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        if not args.stop_after_sample and process.returncode != 0:
            error = error or f"Workload exited with {process.returncode}"
    try:
        if producer_path != raw_path:
            shutil.copy2(producer_path, raw_path)
        sample = json.loads(raw_path.read_text(encoding="utf-8"))
        if not isinstance(sample, dict):
            raise ValueError("Sample must be an object")
        result = verify(sample, args.minimum_fps)
        if sample.get("product") != args.product or sample.get("graphics_api", "").lower() != args.graphics_api.lower():
            error = "Measured host or graphics API does not match the requested workload"
        result["sample_sha256"] = sha256(raw_path)
    except (OSError, ValueError) as exception:
        result = {"valid": False, "validity_errors": [str(exception)], "threshold_passed": None}
    if error:
        result["valid"] = False
        result["validity_errors"].append(error)
    result["inputs"] = {
        "product": args.product, "platform": args.target_platform or platform.system(),
        "runtime_kind": args.runtime_kind,
        "build_commit": environment.get("GITHUB_SHA") or subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True).strip(),
        "fixture": args.fixture.name, "fixture_sha256": sha256(args.fixture),
        "executable_sha256": sha256(args.executable_artifact or Path(shutil.which(command[0]) or command[0])),
    }
    (output / "performance-verdict.json").write_text(
        f"{json.dumps(result, indent=2, allow_nan=False)}\n", encoding="utf-8")
    failed = not result["valid"] or result["threshold_passed"] is False
    fps = result.get("metrics", {}).get("mean_fps")
    print(f"[IMM-FPS] {args.product}/{platform.system()}/{args.graphics_api}: "
          f"{'FAIL' if failed else 'valid'}; mean_fps={fps}; evidence={output}")
    for message in result["validity_errors"]:
        print(f"[IMM-FPS] {message}")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
