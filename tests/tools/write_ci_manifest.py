#!/usr/bin/env python3
"""Write a compact manifest for a CI matrix leg or local verification run."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_file(path: Path, root: Path) -> dict:
    stat = path.stat()
    try:
        display_path = path.relative_to(root).as_posix()
    except ValueError:
        display_path = path.as_posix()
    return {
        "path": display_path,
        "byte_size": stat.st_size,
        "sha256": sha256_file(path),
    }


def run_version(command: list[str], timeout_seconds: int = 10) -> dict:
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
            "stdout": completed.stdout.strip()[-2000:],
            "stderr": completed.stderr.strip()[-2000:],
        }
    except Exception as exc:
        return {
            "command": command,
            "exit_code": None,
            "error": f"{type(exc).__name__}: {exc}",
        }


def tool_info(name: str, command: list[str] | None = None) -> dict:
    path = shutil.which(name)
    info = {"name": name, "path": path, "found": path is not None}
    if path and command:
        info["version"] = run_version(command)
    return info


def collect_tool_versions() -> dict:
    tools = {
        "python": {"path": sys.executable, "version": run_version([sys.executable, "--version"])},
        "git": tool_info("git", ["git", "--version"]),
        "cmake": tool_info("cmake", ["cmake", "--version"]),
        "java": tool_info("java", ["java", "-version"]),
        "gradle": tool_info("gradle", ["gradle", "--version"]),
        "adb": tool_info("adb", ["adb", "version"]),
        "xcodebuild": tool_info("xcodebuild", ["xcodebuild", "-version"]),
        "xcrun": tool_info("xcrun", ["xcrun", "--version"]),
        "msbuild": tool_info("msbuild", ["msbuild", "-version"]),
        "glslangValidator": tool_info("glslangValidator", ["glslangValidator", "--version"]),
        "spirv-val": tool_info("spirv-val", ["spirv-val", "--version"]),
        "scons": tool_info("scons", ["scons", "--version"]),
        "godot": tool_info("godot", ["godot", "--version"]),
    }
    for env_name, key in [("UNITY_EXE", "unity"), ("GODOT_EXE", "godot_env"), ("ANDROID_HOME", "android_home"), ("ANDROID_SDK_ROOT", "android_sdk_root"), ("ANDROID_NDK_HOME", "android_ndk_home")]:
        value = os.environ.get(env_name)
        tools[key] = {"env": env_name, "set": value is not None, "value": value or ""}
    return tools


def collect_fixtures(paths: list[Path], root: Path) -> list[dict]:
    fixtures = []
    seen: set[Path] = set()
    for path in paths:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file():
            fixtures.append(collect_file(resolved, root))
    return fixtures


def default_fixtures(root: Path) -> list[Path]:
    sample = root / "exampleImmFiles" / "sample1.imm"
    return [sample] if sample.exists() else []


def normalize_status(status: str) -> str:
    normalized = status.strip().lower()
    if normalized in {"", "success", "succeeded", "pass"}:
        return "passed"
    if normalized in {"failure", "failed", "error"}:
        return "failed"
    if normalized in {"cancelled", "canceled"}:
        return "cancelled"
    if normalized == "skipped":
        return "skipped"
    if normalized in {"expected_failed", "expected-failed", "expected_failure", "expected-failure"}:
        return "expected_failed"
    return normalized


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--product", required=True)
    parser.add_argument("--platform-name", required=True)
    parser.add_argument("--mode", required=True)
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--status", default="passed")
    parser.add_argument("--failure-class", default="", choices=["", "build", "packaging", "api", "content-parse", "visual", "rendering", "compositing", "audio", "runtime", "runtime-launch", "infrastructure", "vr-device-infrastructure", "evidence", "release-validation", "unknown"])
    parser.add_argument(
        "--classification-json",
        type=Path,
        help="Status JSON containing result and failure_class from an evidence classifier",
    )
    parser.add_argument("--fixture", action="append", default=[], help="Fixture file to hash into the manifest")
    parser.add_argument("--include", action="append", default=[], help="File or directory to hash into the manifest")
    args = parser.parse_args()

    root = args.repo_root.resolve()
    files = []
    for item in args.include:
        path = Path(item).resolve()
        if path.is_file():
            files.append(collect_file(path, root))
        elif path.is_dir():
            for child in sorted(p for p in path.rglob("*") if p.is_file()):
                files.append(collect_file(child, root))

    fixture_paths = [Path(item) for item in args.fixture] if args.fixture else default_fixtures(root)
    status = normalize_status(args.status)
    failure_class = args.failure_class
    classifier_details: dict = {}
    if args.classification_json:
        try:
            classifier = json.loads(args.classification_json.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            parser.error(f"could not read --classification-json: {exc}")
        if not isinstance(classifier, dict):
            parser.error("--classification-json must contain a JSON object")
        classifier_result = str(classifier.get("result") or "").strip()
        classifier_failure_class = str(classifier.get("failure_class") or "").strip()
        for key in ("failures", "warnings"):
            value = classifier.get(key)
            if isinstance(value, list):
                classifier_details[key] = [str(item) for item in value if str(item)]
        for key in (
            "rendering",
            "compositing",
            "composition_mode",
            "composition_contract",
            "depth_composition",
            "depth_interleaving",
            "ordered_overlay",
        ):
            value = classifier.get(key)
            if value is not None:
                classifier_details[key] = value
        if classifier_result == "passed" and status == "passed":
            failure_class = ""
        elif classifier_result == "skipped" and status == "passed":
            status = "skipped"
            failure_class = ""
        elif classifier_result != "passed":
            status = "failed" if status == "passed" else status
            failure_class = classifier_failure_class or failure_class or "unknown"
        elif status != "passed":
            failure_class = failure_class or "infrastructure"
    if status in {"passed", "skipped"}:
        failure_class = ""
    elif not failure_class:
        failure_class = "unknown"

    manifest = {
        "schema": "imm-ci-artifact-manifest-v1",
        "product": args.product,
        "platform": args.platform_name,
        "mode": args.mode,
        "renderer": args.renderer,
        "status": status,
        "classification": {
            "result": status,
            "failure_class": failure_class,
            **classifier_details,
        },
        "matrix": {
            "product": args.product,
            "platform": args.platform_name,
            "mode": args.mode,
            "renderer": args.renderer,
            "github_job": os.environ.get("GITHUB_JOB", ""),
            "workflow": os.environ.get("GITHUB_WORKFLOW", ""),
        },
        "git": {
            "sha": os.environ.get("GITHUB_SHA", ""),
            "ref": os.environ.get("GITHUB_REF", ""),
            "workflow": os.environ.get("GITHUB_WORKFLOW", ""),
            "job": os.environ.get("GITHUB_JOB", ""),
            "run_id": os.environ.get("GITHUB_RUN_ID", ""),
            "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", ""),
            "repository": os.environ.get("GITHUB_REPOSITORY", ""),
        },
        "runner": {
            "os": os.environ.get("RUNNER_OS", platform.system()),
            "arch": os.environ.get("RUNNER_ARCH", platform.machine()),
            "name": os.environ.get("RUNNER_NAME", ""),
            "labels": os.environ.get("RUNNER_LABELS", ""),
        },
        "tool_versions": collect_tool_versions(),
        "fixtures": collect_fixtures(fixture_paths, root),
        "files": files,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote CI manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
