#!/usr/bin/env python3
"""Ensure every committed host copy uses the canonical diagnostic fixture."""

from __future__ import annotations

import hashlib
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    canonical = root / "exampleImmFiles" / "face-orientation.imm"
    copies = [
        root / "code" / "ImmUnitySampleProject" / "Assets" / "StreamingAssets" / "face-orientation.imm",
        root / "code" / "ImmGodotSampleProject" / "face-orientation.imm",
        root / "code" / "ImmGodotXRSampleProject" / "face-orientation.imm",
    ]
    expected = digest(canonical)
    for copy in copies:
        if not copy.is_file():
            raise FileNotFoundError(f"Missing face-orientation fixture copy: {copy}")
        if digest(copy) != expected:
            raise RuntimeError(f"Face-orientation fixture is out of sync: {copy}")
    print("Face-orientation fixture copies are in sync")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
