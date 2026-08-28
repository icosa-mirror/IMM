#!/usr/bin/env python3
"""Contract tests for the face-orientation image classifier."""

from __future__ import annotations

import unittest

from classify_face_orientation_capture import REGIONS, classify, pixel_bounds


class FaceOrientationClassifierTests(unittest.TestCase):
    def make_capture(self, expose_backfaces: bool = False) -> tuple[int, int, bytes]:
        width, height = 200, 100
        pixels = bytearray((24, 29, 40) * (width * height))
        colors = ((40, 195, 235), (235, 55, 20), (45, 225, 90))
        for index, (region, color) in enumerate(zip(REGIONS, colors)):
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


if __name__ == "__main__":
    unittest.main()
