#!/usr/bin/env python3
"""Focused checks for runner preflight diagnostics."""

from __future__ import annotations

import preflight_runner


def main() -> int:
    output = "\n".join(
        [
            "List of devices attached",
            "1WMHH000000000 device product:hollywood model:Quest_3 device:eureka transport_id:2",
            "emulator-5554 offline transport_id:1",
            "",
        ]
    )
    devices = preflight_runner.parse_adb_devices(output)
    assert devices == [
        {
            "serial": "1WMHH000000000",
            "state": "device",
            "details": ["product:hollywood", "model:Quest_3", "device:eureka", "transport_id:2"],
        },
        {"serial": "emulator-5554", "state": "offline", "details": ["transport_id:1"]},
    ]
    assert any(device["state"] == "device" for device in devices)
    print("Preflight parser tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
