import json
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BASELINE_PATH = REPO_ROOT / "tests" / "baselines" / "web" / "sample1-native-decoder.json"


class NativeWebManifestBaselineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads(BASELINE_PATH.read_text(encoding="utf-8"))

    def test_fixture_identity_and_schema(self) -> None:
        self.assertEqual(self.manifest["schema"], "imm-native-decoder-manifest-v1")
        self.assertEqual(self.manifest["fixture"]["byte_size"], 5_831_101)
        self.assertEqual(
            self.manifest["fixture"]["sha256"],
            "baa05c125ed0ca130760f1b76a8cde39a475f66a60e5b4e96644a1839b0b2095",
        )

    def test_sample1_decoded_content_summary(self) -> None:
        layers = self.manifest["document"]["layers"]
        drawings = [drawing for layer in layers for drawing in layer["drawings"]]

        self.assertEqual(self.manifest["document"]["chapter_count"], 1)
        self.assertEqual(len(layers), 31)
        self.assertEqual(len(drawings), 30)
        self.assertEqual(sum(drawing["stroke_count"] for drawing in drawings), 1_171)
        self.assertEqual(sum(drawing["point_count"] for drawing in drawings), 58_405)
        self.assertEqual(sum(layer["picture"] is not None for layer in layers), 1)


if __name__ == "__main__":
    unittest.main()
