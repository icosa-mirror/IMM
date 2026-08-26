#!/usr/bin/env python3
"""Verify that Unity emitted a passing NUnit result containing required tests."""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def parse_count(root: ET.Element, names: tuple[str, ...]) -> int | None:
    for name in names:
        value = root.get(name)
        if value is not None:
            try:
                return int(value)
            except ValueError:
                return None
    return None


def verify_results(path: Path, required_tests: list[str]) -> list[str]:
    errors: list[str] = []
    if not path.is_file():
        return [f"Unity test result XML is missing: {path}"]

    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as exc:
        return [f"Could not parse Unity test result XML {path}: {exc}"]

    test_cases = list(root.iter("test-case"))
    if not test_cases:
        errors.append(f"Unity test result XML contains no test cases: {path}")

    failed = parse_count(root, ("failed", "failures"))
    errors_count = parse_count(root, ("errors",))
    if root.get("result", "").lower() in {"failed", "failure", "error"}:
        errors.append(f"Unity test run result was {root.get('result')}")
    if failed not in (None, 0):
        errors.append(f"Unity test run reported {failed} failed test(s)")
    if errors_count not in (None, 0):
        errors.append(f"Unity test run reported {errors_count} error(s)")

    executed_names = {
        value
        for test_case in test_cases
        for value in (test_case.get("fullname"), test_case.get("name"))
        if value
    }
    for required in required_tests:
        if not any(required in executed for executed in executed_names):
            errors.append(f"Required Unity test did not run: {required}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--require-test", action="append", default=[])
    args = parser.parse_args()

    errors = verify_results(args.results, args.require_test)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Unity test results verified: {args.results}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
