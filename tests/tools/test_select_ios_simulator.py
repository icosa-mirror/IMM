#!/usr/bin/env python3
"""Tests for compatible iOS Simulator selection."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/tools"))

from select_ios_simulator import select  # noqa: E402


def main() -> int:
    payload = {
        "devices": {
            "com.apple.CoreSimulator.SimRuntime.iOS-17-5": [
                {"name": "iPhone 15", "udid": "IOS17-PHONE", "isAvailable": True},
            ],
            "com.apple.CoreSimulator.SimRuntime.iOS-18-2": [
                {"name": "iPad Pro", "udid": "IOS18-IPAD", "isAvailable": True},
                {"name": "iPhone 16", "udid": "IOS18-PHONE", "isAvailable": True},
                {"name": "iPhone 16 Pro", "udid": "UNAVAILABLE", "isAvailable": False},
            ],
            "com.apple.CoreSimulator.SimRuntime.tvOS-18-2": [
                {"name": "Apple TV", "udid": "TV", "isAvailable": True},
            ],
        }
    }
    selected = select(payload)
    assert selected["runtime_id"].endswith("iOS-18-2")
    assert selected["udid"] == "IOS18-PHONE"
    assert selected["device_name"] == "iPhone 16"

    try:
        select({"devices": {"com.apple.CoreSimulator.SimRuntime.iOS-18-2": []}})
    except ValueError as exc:
        assert "no available iPhone simulator" in str(exc)
    else:
        raise AssertionError("missing compatible simulator must fail closed")

    print("iOS Simulator selector tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
