"""Focused degenerate-case checks; these do not substitute for runtime evidence."""
import copy
import unittest

from verify_frame_sample import verify


def sample(fps=60):
    frames = fps * 5
    return dict(schema="imm-frame-sample-v1", product="standalone", graphics_api="Vulkan",
                width=1280, height=720, warmup_seconds=2, requested_seconds=5,
                observed_seconds=5, engine_frame_count=300,
                imm_rendered_frame_count=frames, intervals_ms=[1000 / fps] * frames, errors=[])


class DegenerateCases(unittest.TestCase):
    def test_normal_and_ten_fps_are_both_valid_but_only_normal_meets_loose_floor(self):
        self.assertTrue(verify(sample(), 20)["threshold_passed"])
        slow = verify(sample(10), 20)
        self.assertTrue(slow["valid"])
        self.assertFalse(slow["threshold_passed"])
        self.assertTrue(verify(sample(10))["valid"])  # report-only preserves slow evidence

    def test_updates_without_rendering_are_invalid(self):
        data = sample()
        data.update(imm_rendered_frame_count=0, intervals_ms=[])
        self.assertFalse(verify(data)["valid"])

    def test_trailing_render_stall_counts_in_mean(self):
        data = sample()
        data["observed_seconds"] = 30
        result = verify(data, 20)
        self.assertEqual(result["metrics"]["mean_fps"], 10)
        self.assertFalse(result["threshold_passed"])

    def test_invalid_samples_cannot_pass(self):
        for changes in (
            dict(width=1), dict(observed_seconds=1), dict(intervals_ms=[float("nan")] * 300),
            dict(imm_rendered_frame_count=600), dict(errors=["Viewer stopped early"]),
        ):
            with self.subTest(changes=next(iter(changes))):
                data = copy.deepcopy(sample())
                data.update(changes)
                self.assertFalse(verify(data, 20)["valid"])


if __name__ == "__main__":
    unittest.main()
