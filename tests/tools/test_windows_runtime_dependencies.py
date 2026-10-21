"""Check Windows test staging against the committed package dependency files."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "code/appImmUnity/tests"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from exporter_bridge_smoke import runtime_dependencies as bridge_dependencies
from generate_face_orientation_fixture import runtime_dependencies as fixture_dependencies


class WindowsRuntimeDependencies(unittest.TestCase):
    def test_staging_uses_existing_shared_dependencies(self):
        bridge = bridge_dependencies(ROOT)
        fixture = fixture_dependencies(ROOT)
        self.assertEqual(set(bridge), set(fixture))
        for dependencies in (bridge, fixture):
            self.assertEqual(len(dependencies), len({path.name for path in dependencies}))
            for path in dependencies:
                self.assertTrue(path.is_file(), f"Missing staged dependency: {path.relative_to(ROOT)}")
            opus = next(path for path in dependencies if path.name == "opus.dll")
            self.assertIn("com.immersive-foundation.imm-stroke-reader", opus.parts)


if __name__ == "__main__":
    unittest.main()
