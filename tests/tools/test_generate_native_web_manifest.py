import ctypes
import importlib.util
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).with_name("generate_native_web_manifest.py")
SPEC = importlib.util.spec_from_file_location("generate_native_web_manifest", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class NativeWebManifestTests(unittest.TestCase):
    def test_transform_dict_preserves_web_contract_fields(self) -> None:
        transform = MODULE.StrokeLayerTransform()
        transform.rotation[:] = (0.0, 0.5, 0.0, 1.0)
        transform.scale = 2.0
        transform.flip = 3
        transform.translation[:] = (1.0, 2.0, 3.0)

        self.assertEqual(
            MODULE._transform_dict(transform),
            {
                "rotation": [0.0, 0.5, 0.0, 1.0],
                "scale": 2.0,
                "flip": 3,
                "translation": [1.0, 2.0, 3.0],
            },
        )

    def test_ctypes_structs_match_native_field_sizes(self) -> None:
        self.assertEqual(ctypes.sizeof(MODULE.StrokeLayerInfo), 316)
        self.assertEqual(ctypes.sizeof(MODULE.StrokeLayerTransform), 36)
        self.assertEqual(ctypes.sizeof(MODULE.StrokeInfo), 36)
        self.assertEqual(ctypes.sizeof(MODULE.StrokePoint), 56)
        self.assertEqual(ctypes.sizeof(MODULE.StrokePictureInfo), 28)


if __name__ == "__main__":
    unittest.main()
