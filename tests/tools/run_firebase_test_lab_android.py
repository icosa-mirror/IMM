#!/usr/bin/env python3
"""Run an Android Firebase Test Lab leg and collect result evidence."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


TEXT_SUFFIXES = {"", ".log", ".txt", ".xml", ".json"}


def run(command: list[str], stdout_path: Path, stderr_path: Path) -> int:
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    with stdout_path.open("w", encoding="utf-8", newline="\n") as stdout, stderr_path.open("w", encoding="utf-8", newline="\n") as stderr:
        completed = subprocess.run(command, check=False, stdout=stdout, stderr=stderr, text=True)
    return completed.returncode


def run_capture(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def decode_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return ""


def find_text_with_markers(root: Path, markers: list[str]) -> tuple[dict[str, list[str]], list[str]]:
    matches = {marker: [] for marker in markers}
    searched: list[str] = []
    if not root.exists():
        return matches, searched
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        if path.stat().st_size > 25 * 1024 * 1024:
            continue
        if path.suffix.lower() not in TEXT_SUFFIXES and "log" not in path.name.lower():
            continue
        text = decode_text(path)
        if not text:
            continue
        searched.append(path.relative_to(root).as_posix())
        for marker in markers:
            if marker in text:
                matches[marker].append(path.relative_to(root).as_posix())
    return matches, searched


def copy_results(bucket: str, results_dir: str, output_dir: Path) -> dict:
    destination = output_dir / "ftl-results"
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    source = f"gs://{bucket}/{results_dir}"
    completed = run_capture(["gcloud", "storage", "cp", "--recursive", source, str(destination)])
    (output_dir / "gcloud-storage-cp.stdout.txt").write_text(completed.stdout, encoding="utf-8", newline="\n")
    (output_dir / "gcloud-storage-cp.stderr.txt").write_text(completed.stderr, encoding="utf-8", newline="\n")
    return {
        "source": source,
        "destination": destination.as_posix(),
        "exit_code": completed.returncode,
    }


def is_tool_results_api_disabled(stderr_path: Path) -> bool:
    text = decode_text(stderr_path)
    return "toolresults.googleapis.com" in text and "SERVICE_DISABLED" in text


def is_firebase_infrastructure_failure(stderr_path: Path) -> bool:
    text = decode_text(stderr_path)
    return (
        "Firebase Test Lab infrastructure failure" in text
        or "An infrastructure error occurred. Attempts exhausted." in text
    )


def write_summary(
    args: argparse.Namespace,
    gcloud_exit: int,
    gcloud_exit_ignored: bool,
    copy_info: dict,
    errors: list[str],
    marker_matches: dict[str, list[str]],
    searched_logs: list[str],
    captures: list[Path],
) -> Path:
    summary = {
        "schema": "imm-firebase-test-lab-result-v1",
        "passed": (gcloud_exit == 0 or gcloud_exit_ignored) and not errors,
        "gcloud_exit_code": gcloud_exit,
        "gcloud_exit_ignored": gcloud_exit_ignored,
        "test_type": args.test_type,
        "project": args.project,
        "device": args.device,
        "results_bucket": args.results_bucket,
        "results_dir": args.results_dir,
        "copy_results": copy_info,
        "required_markers": marker_matches,
        "searched_logs": searched_logs,
        "captures": [path.relative_to(args.artifact_dir).as_posix() for path in captures],
        "errors": errors,
    }
    path = args.artifact_dir / "firebase-test-lab-result.json"
    path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Firebase Test Lab summary written: {path}")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", required=True)
    parser.add_argument("--results-bucket", required=True)
    parser.add_argument("--results-dir", required=True)
    parser.add_argument("--device", required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--test", type=Path)
    parser.add_argument("--test-type", choices=["instrumentation", "robo"], default="instrumentation")
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--timeout", default="5m")
    parser.add_argument("--environment-variable", action="append", default=[])
    parser.add_argument("--directory-to-pull", action="append", default=[])
    parser.add_argument("--required-marker", action="append", default=[])
    parser.add_argument("--required-capture-name", action="append", default=[])
    parser.add_argument("--client-label", default="")
    parser.add_argument("--gcloud-attempts", type=int, default=2)
    args = parser.parse_args()

    args.artifact_dir.mkdir(parents=True, exist_ok=True)
    errors: list[str] = []

    def build_command(results_dir: str) -> list[str]:
        command = [
            "gcloud",
            "firebase",
            "test",
            "android",
            "run",
            "--project",
            args.project,
            "--type",
            args.test_type,
            "--app",
            args.app.as_posix(),
            "--device",
            args.device,
            "--timeout",
            args.timeout,
            "--results-bucket",
            args.results_bucket,
            "--results-dir",
            results_dir,
            "--format",
            "json",
        ]
        if args.test:
            command.extend(["--test", args.test.as_posix()])
        for value in args.environment_variable:
            command.extend(["--environment-variables", value])
        for value in args.directory_to_pull:
            command.extend(["--directories-to-pull", value])
        if args.client_label:
            command.extend(["--client-details", f"matrixLabel={args.client_label}"])
        return command

    gcloud_exit = 1
    stderr_path = args.artifact_dir / "gcloud-firebase-test.stderr.txt"
    max_attempts = max(1, args.gcloud_attempts)
    for attempt in range(1, max_attempts + 1):
        attempt_results_dir = args.results_dir if attempt == 1 else f"{args.results_dir}-retry{attempt}"
        command = build_command(attempt_results_dir)
        (args.artifact_dir / f"gcloud-command.attempt{attempt}.json").write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8", newline="\n")
        (args.artifact_dir / "gcloud-command.json").write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8", newline="\n")
        stdout_path = args.artifact_dir / f"gcloud-firebase-test.attempt{attempt}.stdout.json"
        stderr_path = args.artifact_dir / f"gcloud-firebase-test.attempt{attempt}.stderr.txt"
        gcloud_exit = run(command, stdout_path, stderr_path)
        shutil.copyfile(stdout_path, args.artifact_dir / "gcloud-firebase-test.stdout.json")
        shutil.copyfile(stderr_path, args.artifact_dir / "gcloud-firebase-test.stderr.txt")
        args.results_dir = attempt_results_dir
        if gcloud_exit == 0:
            break
        if attempt == max_attempts or not is_firebase_infrastructure_failure(stderr_path):
            break
        print(f"Firebase Test Lab infrastructure failure on attempt {attempt}; retrying with results-dir {args.results_dir}-retry{attempt + 1}", file=sys.stderr)

    copy_info = copy_results(args.results_bucket, args.results_dir, args.artifact_dir)
    if copy_info["exit_code"] != 0:
        errors.append(f"Failed to copy Firebase Test Lab results from {copy_info['source']}")

    results_root = args.artifact_dir / "ftl-results"
    marker_matches, searched_logs = find_text_with_markers(results_root, args.required_marker)
    for marker, paths in marker_matches.items():
        if not paths:
            errors.append(f"Missing required Firebase Test Lab log marker: {marker}")

    captures: list[Path] = []
    for name in args.required_capture_name:
        found = sorted(path for path in results_root.rglob(name) if path.is_file())
        if not found:
            errors.append(f"Missing required Firebase Test Lab capture: {name}")
        else:
            captures.extend(found)

    gcloud_exit_ignored = gcloud_exit != 0 and is_tool_results_api_disabled(stderr_path) and not errors
    write_summary(args, gcloud_exit, gcloud_exit_ignored, copy_info, errors, marker_matches, searched_logs, captures)
    if gcloud_exit != 0 and not gcloud_exit_ignored:
        errors.append(f"gcloud firebase test android run exited with {gcloud_exit}")
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
