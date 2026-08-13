#!/usr/bin/env python3
"""Select a compatible available iPhone from `simctl list devices -j`."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


IOS_RUNTIME_RE = re.compile(r"\.SimRuntime\.iOS-(?P<version>[0-9-]+)$")


def runtime_version(identifier: str) -> tuple[int, ...] | None:
    match = IOS_RUNTIME_RE.search(identifier)
    if not match:
        return None
    return tuple(int(part) for part in match.group("version").split("-"))


def select(payload: dict) -> dict:
    groups = payload.get("devices")
    if not isinstance(groups, dict):
        raise ValueError("simctl payload is missing the devices object")
    candidates: list[tuple[tuple[int, ...], str, str, str]] = []
    for runtime_id, devices in groups.items():
        version = runtime_version(str(runtime_id))
        if version is None or not isinstance(devices, list):
            continue
        for device in devices:
            if not isinstance(device, dict):
                continue
            name = str(device.get("name") or "")
            udid = str(device.get("udid") or "")
            if device.get("isAvailable") is True and name.startswith("iPhone") and udid:
                candidates.append((version, name, udid, str(runtime_id)))
    if not candidates:
        raise ValueError("no available iPhone simulator belongs to an installed iOS runtime")
    version, name, udid, runtime_id = max(candidates)
    return {
        "schema": "imm-ios-simulator-selection-v1",
        "runtime_id": runtime_id,
        "runtime_version": ".".join(str(part) for part in version),
        "device_name": name,
        "udid": udid,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    payload = json.loads(args.input.read_text(encoding="utf-8"))
    selected = select(payload)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(selected, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"Selected {selected['device_name']} on {selected['runtime_version']}: {selected['udid']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
