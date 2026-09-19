"""Compile the Unity project's generated C# projects without opening the Editor.

Unity writes .csproj files (and IMM Unity Test.sln) into the project root, referencing the
installed Editor's managed assemblies. That makes it possible to type-check the packages,
the sample scripts and the test assemblies with MSBuild alone - which matters here because
the Editor currently hangs while opening the project (it stops right after
"[Package Manager] Connected to IPC stream"), and because CI only compiles the small
SharpQuill adapter subset.

Outputs go to artifacts/csharp-check/ so nothing is written into the project while the
Editor may be holding it open.

The default set is the two package runtime assemblies, which reference only the Editor's own
managed DLLs and therefore compile even when the project has never been imported. The test
and sample assemblies additionally reference assemblies Unity generates into
Library/ScriptAssemblies from packages (nunit, the Unity test framework, XR Management,
TextMeshPro, ...). Those only exist after the Editor has completed an import, so --all
reports them as unresolved-reference errors on a project that has never been opened.

Usage:
    python tests/tools/verify_unity_csharp.py
    python tests/tools/verify_unity_csharp.py --all --keep-going
    python tests/tools/verify_unity_csharp.py --project Assembly-CSharp
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
UNITY_PROJECT = REPO_ROOT / "code/ImmUnitySampleProject"
MSBUILD_CANDIDATES = (
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"),
)

# Compile cleanly against the Editor's own managed assemblies alone.
DEFAULT_PROJECTS = (
    "ImmUnity.Runtime",
    "ImmStrokeReader.Runtime",
)
# Also need Library/ScriptAssemblies, so they require a completed Editor import.
PACKAGE_DEPENDENT_PROJECTS = (
    "ImmUnity.Runtime.Tests",
    "Assembly-CSharp",
    "Assembly-CSharp-Editor",
)


def find_msbuild() -> Path:
    for candidate in MSBUILD_CANDIDATES:
        if candidate.is_file():
            return candidate
    found = shutil.which("MSBuild.exe") or shutil.which("msbuild")
    if found:
        return Path(found)
    raise SystemExit("MSBuild not found; install the Visual Studio C++/C# workload or add MSBuild to PATH.")


def project_path(name: str) -> Path:
    return UNITY_PROJECT / f"{name}.csproj"


def compile_project(msbuild: Path, name: str, output_root: Path, verbose: bool) -> tuple[bool, list[str]]:
    """Return (succeeded, error lines) for one generated project."""
    project = project_path(name)
    if not project.is_file():
        return False, [f"{name}.csproj not found - open the project in Unity once to generate it"]

    out_dir = output_root / name
    obj_dir = output_root / f"obj-{name}"
    command = [
        str(msbuild),
        str(project),
        "-t:Build",
        "-p:Configuration=Debug",
        f"-p:OutputPath={out_dir}{os.sep}",
        f"-p:IntermediateOutputPath={obj_dir}{os.sep}",
        "-v:m",
        "-nologo",
    ]
    completed = subprocess.run(
        command,
        cwd=UNITY_PROJECT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    lines = (completed.stdout or "").splitlines() + (completed.stderr or "").splitlines()
    errors = [line.strip() for line in lines if ": error " in line]
    if verbose and not errors:
        print("\n".join(lines[-5:]))
    return completed.returncode == 0 and not errors, errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--project", action="append", default=None,
                        help="generated project name to compile (repeatable; default: the package runtimes)")
    parser.add_argument("--all", action="store_true",
                        help="also compile the test and sample assemblies (needs a completed Editor import)")
    parser.add_argument("--output", type=Path, default=REPO_ROOT / "artifacts/csharp-check",
                        help="directory for build outputs (default: artifacts/csharp-check)")
    parser.add_argument("--keep-going", action="store_true", help="compile every project even after a failure")
    parser.add_argument("--verbose", action="store_true", help="print the tail of each build log")
    arguments = parser.parse_args()

    msbuild = find_msbuild()
    projects = arguments.project or list(DEFAULT_PROJECTS) + (list(PACKAGE_DEPENDENT_PROJECTS) if arguments.all else [])
    arguments.output.mkdir(parents=True, exist_ok=True)

    print(f"msbuild: {msbuild}")
    print(f"project: {UNITY_PROJECT}\n")

    failures = 0
    for name in projects:
        succeeded, errors = compile_project(msbuild, name, arguments.output, arguments.verbose)
        print(f"{name:26s} {'ok' if succeeded else 'FAILED'}")
        for line in errors[:20]:
            print(f"    {line}")
        if not succeeded:
            failures += 1
            if not arguments.keep_going:
                break

    print()
    if failures:
        print(f"{failures} project(s) failed to compile")
        return 1
    print("every project compiled")
    return 0


if __name__ == "__main__":
    sys.exit(main())
