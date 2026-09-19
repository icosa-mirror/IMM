"""One command for the player-facing regression set.

The live-document work must not change how the player behaves for documents that are simply
loaded and playing. This runs the checks that would catch a regression, in the order that
gives the most useful failure first:

    1. exporter bridge round trip      (native ABI, export -> reload through the reader)
    2. stroke reader adapter           (managed C# compiles and parses the same files)
    3. IMM content baseline            (the committed sample file still parses identically)
    4. plugin export surface           (declared P/Invokes still exist in the binaries)
    5. Unity package C# compile        (the packages still build against the Editor's DLLs)

Usage:
    python tests/tools/run_player_regressions.py
    python tests/tools/run_player_regressions.py --skip adapter,unity
    python tests/tools/run_player_regressions.py --json artifacts/player-regressions.json
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

CHECKS = (
    {
        "name": "bridge",
        "title": "Exporter bridge round trip",
        "command": [sys.executable, "code/appImmUnity/tests/exporter_bridge_smoke.py"],
    },
    {
        "name": "adapter",
        "title": "Stroke reader adapter regression",
        "command": ["dotnet", "run", "--project",
                    "code/appImmStrokeReader/tests/SharpQuillAdapter/SharpQuillAdapter.csproj", "--",
                    "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader"
                    "/Plugins/x86_64/ImmStrokeReader.dll",
                    "exampleImmFiles/sample1.imm",
                    "artifacts/sharpquill-adapter-native.log"],
    },
    {
        "name": "baseline",
        "title": "IMM content baseline",
        "command": [sys.executable, "tests/tools/verify_imm_baseline.py", "exampleImmFiles/sample1.imm",
                    "--baseline", "tests/baselines/content/sample1.json",
                    "--write-actual", "artifacts/baseline-content/sample1.actual.json"],
    },
    {
        "name": "exports",
        "title": "Plugin export surface",
        "command": [sys.executable, "tests/tools/verify_unity_plugin_exports.py"],
    },
    {
        "name": "unity",
        "title": "Unity package C# compile",
        "command": [sys.executable, "tests/tools/verify_unity_csharp.py"],
    },
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--skip", default="", help="comma-separated check names to skip")
    parser.add_argument("--json", type=Path, default=None, help="write the result summary as JSON")
    arguments = parser.parse_args()

    skipped = {name.strip() for name in arguments.skip.split(",") if name.strip()}
    results = []
    failures = 0

    for check in CHECKS:
        if check["name"] in skipped:
            print(f"{check['title']:38s} skipped")
            results.append({"name": check["name"], "status": "skipped", "seconds": 0.0, "detail": ""})
            continue

        start = time.perf_counter()
        completed = subprocess.run(
            check["command"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        elapsed = time.perf_counter() - start
        output = ((completed.stdout or "") + (completed.stderr or "")).strip()
        detail = output.splitlines()[-1] if output else ""
        if completed.returncode == 0:
            print(f"{check['title']:38s} ok       {elapsed:6.1f}s")
        else:
            failures += 1
            print(f"{check['title']:38s} FAILED   {elapsed:6.1f}s")
            for line in output.splitlines()[-12:]:
                print(f"    {line}")
        results.append({
            "name": check["name"],
            "status": "ok" if completed.returncode == 0 else "failed",
            "seconds": round(elapsed, 2),
            "detail": detail,
        })

    print()
    if failures:
        print(f"{failures} player regression check(s) failed")
    else:
        print("player regression set passed")

    if arguments.json is not None:
        arguments.json.parent.mkdir(parents=True, exist_ok=True)
        arguments.json.write_text(json.dumps({"checks": results}, indent=2), encoding="utf-8")
        print(f"wrote {arguments.json}")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
