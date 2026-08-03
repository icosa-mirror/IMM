#!/usr/bin/env python3
"""Contract tests for stable physical-device video validation."""

from __future__ import annotations

from pathlib import Path

import validate_render_video


def main() -> int:
    assert validate_render_video.first_stable_run([False, True, True, True], 3) == (1, 3)
    assert validate_render_video.first_stable_run([True, True, False, True, True], 3) is None
    assert validate_render_video.first_stable_run([False, True, True, False, True, True, True], 3) == (4, 6)
    source = Path(validate_render_video.__file__).read_text(encoding="utf-8")
    assert '"color_component_probes": result.get("color_component_probes")' in source
    print("Render video validation contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
