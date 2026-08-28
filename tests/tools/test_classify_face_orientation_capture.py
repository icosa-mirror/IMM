#!/usr/bin/env python3
"""Contract tests for the face-orientation image classifier."""

from __future__ import annotations

import unittest

from classify_face_orientation_capture import (
    LAYOUT_NORMAL,
    LAYOUT_REVERSED_HORIZONTAL,
    classify,
    is_cyan,
    pixel_bounds,
    regions_for_layout,
)


class FaceOrientationClassifierTests(unittest.TestCase):
    def test_cyan_accepts_mobile_channel_clipping_without_matching_green(self) -> None:
        self.assertTrue(is_cyan(89, 255, 255))
        self.assertFalse(is_cyan(123, 255, 176))

    def make_capture(
        self,
        expose_backfaces: bool = False,
        layout: str = LAYOUT_NORMAL,
    ) -> tuple[int, int, bytes]:
        width, height = 200, 100
        pixels = bytearray((24, 29, 40) * (width * height))
        colors = ((40, 195, 235), (235, 55, 20), (45, 225, 90))
        for index, (region, color) in enumerate(zip(regions_for_layout(layout), colors)):
            if index == 1 and not expose_backfaces:
                continue
            x0, y0, x1, y1 = pixel_bounds(region, width, height)
            inset_x = max(1, (x1 - x0) // 4)
            inset_y = max(1, (y1 - y0) // 4)
            for y in range(y0 + inset_y, y1 - inset_y):
                for x in range(x0 + inset_x, x1 - inset_x):
                    offset = (y * width + x) * 3
                    pixels[offset : offset + 3] = bytes(color)
        return width, height, bytes(pixels)

    def test_expected_front_faces_pass(self) -> None:
        status = classify(*self.make_capture())
        self.assertEqual("passed", status["result"])
        self.assertTrue(status["checks"]["backface_interior_hidden"]["passed"])

    def test_exposed_backfaces_fail(self) -> None:
        status = classify(*self.make_capture(expose_backfaces=True))
        self.assertEqual("render_failed", status["result"])
        self.assertEqual(["backface_interior_hidden"], status["failures"])

    def test_reversed_horizontal_capture_passes_and_records_layout(self) -> None:
        status = classify(*self.make_capture(layout=LAYOUT_REVERSED_HORIZONTAL))
        self.assertEqual("passed", status["result"])
        self.assertEqual(LAYOUT_REVERSED_HORIZONTAL, status["capture_layout"])

    def test_reversed_horizontal_capture_still_detects_backfaces(self) -> None:
        status = classify(
            *self.make_capture(expose_backfaces=True, layout=LAYOUT_REVERSED_HORIZONTAL)
        )
        self.assertEqual("render_failed", status["result"])
        self.assertEqual(["backface_interior_hidden"], status["failures"])


if __name__ == "__main__":
    unittest.main()
