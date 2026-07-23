#!/usr/bin/env python3
"""Verify an IMM fixture against a committed content baseline."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from generate_imm_baseline import generate_baseline


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--write-actual", type=Path)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    expected = json.loads(args.baseline.read_text(encoding="utf-8"))
    actual = generate_baseline(args.fixture.resolve(), repo_root)

    if args.write_actual:
        args.write_actual.parent.mkdir(parents=True, exist_ok=True)
        args.write_actual.write_text(json.dumps(actual, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")

    if actual != expected:
        print("IMM baseline mismatch.", file=sys.stderr)
        print(f"Expected: {args.baseline}", file=sys.stderr)
        if args.write_actual:
            print(f"Actual:   {args.write_actual}", file=sys.stderr)
        else:
            print(json.dumps(actual, indent=2, sort_keys=True), file=sys.stderr)
        return 1

    print(f"IMM baseline verified: {args.fixture}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
