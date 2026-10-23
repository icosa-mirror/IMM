"""Check additive key layouts and C ABI validation without initializing a player.

All calls deliberately reject before accessing the player's document table.
Loaded-document mutation and rendered held selection are separate integration tests.
"""
import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

from exporter_bridge_smoke import runtime_dependencies


ROOT = Path(__file__).resolve().parents[3]


class KeyV1(c.Structure):
    _fields_ = [('structSize', c.c_uint32), ('structVersion', c.c_uint32),
                ('property', c.c_int32), ('interpolation', c.c_int32), ('timeSeconds', c.c_double),
                ('boolValue', c.c_int32), ('floatValue', c.c_float),
                *[(name, c.c_float) for name in ('tx', 'ty', 'tz', 'qx', 'qy', 'qz', 'qw', 'scale')],
                ('reserved', c.c_uint32 * 4)]


class KeyV2(c.Structure):
    _fields_ = [('base', KeyV1), ('uintValue', c.c_uint32), ('reserved3', c.c_uint32 * 3)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, default=ROOT / 'code/appImmUnity/exe/ImmUnityPlugin.dll')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows native plugin required')
    assert c.sizeof(KeyV1) == 80 and c.sizeof(KeyV2) == 96 and KeyV2.uintValue.offset == 80
    output = ROOT / 'artifacts/authoring-key-abi'
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / 'report.json'
    results = []
    report = {'passed': False, 'pluginSha256': hashlib.sha256(args.plugin.read_bytes()).hexdigest(),
              'v1Size': c.sizeof(KeyV1), 'v2Size': c.sizeof(KeyV2), 'uintValueOffset': KeyV2.uintValue.offset,
              'scope': 'ABI layout and pre-player validation only', 'cases': results}
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    staging = Path(tempfile.mkdtemp(prefix='imm-authoring-key-abi-'))
    for path in (args.plugin, *runtime_dependencies(ROOT)):
        shutil.copy2(path, staging / path.name)
    # Loaded codec dependencies remain mapped until this short-lived process exits.
    c.windll.kernel32.SetDllDirectoryW(str(staging))
    plugin = c.CDLL(str(staging / args.plugin.name))
    key_set = plugin.ImmAuthoring_KeySet
    key_set.argtypes = [c.c_int32, c.c_int32, c.c_void_p]
    key_set.restype = c.c_int32

    def check(name, key, expected):
        actual = key_set(0, 0, c.byref(key))
        results.append({'case': name, 'expected': expected, 'actual': actual})
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
        if actual != expected:
            raise AssertionError(f'{name}: expected {expected}, got {actual}')

    def v1(prop=99):
        key = KeyV1()
        key.structSize, key.structVersion, key.property = c.sizeof(KeyV1), 1, prop
        return key

    def v2(prop=99):
        key = KeyV2()
        key.base.structSize, key.base.structVersion, key.base.property = c.sizeof(KeyV2), 2, prop
        return key

    check('V1 unchanged layout accepted before unsupported property', v1(), -3)
    check('V2 prefix accepted before unsupported property', v2(), -3)
    check('V1 Offset cannot reinterpret reserved or float fields', v1(8), -3)
    key = v2(8)
    key.base.structSize = c.sizeof(KeyV1)
    check('V2 truncated extension', key, -2)
    key = v2(8)
    key.base.structVersion = 3
    check('unknown version', key, -3)
    key = v1(7)
    key.boolValue = 2
    check('Loop V1 strict boolean', key, -2)
    key = v1(7)
    key.interpolation = 1
    check('Loop V1 step only', key, -2)
    key = v2(7)
    key.base.boolValue = -1
    check('Loop V2 strict boolean', key, -2)
    key = v2(8)
    key.uintValue, key.base.interpolation = 0xFFFFFFFF, 1
    check('Offset V2 step only with full-width unsigned payload', key, -2)
    for index in range(4):
        key = v2(8)
        key.base.reserved[index] = 1
        check(f'V1 reserved slot {index} stays reserved in V2', key, -2)
    for index in range(3):
        key = v2(8)
        key.reserved3[index] = 1
        check(f'V2 reserved slot {index}', key, -2)
    repeat_set = plugin.ImmAuthoring_PaintSetMaxRepeatCount
    repeat_set.argtypes = [c.c_int32, c.c_int32, c.c_uint32]
    repeat_set.restype = c.c_int32
    repeat_get = plugin.ImmAuthoring_PaintGetMaxRepeatCount
    repeat_get.argtypes = [c.c_int32, c.c_int32, c.POINTER(c.c_uint32)]
    repeat_get.restype = c.c_int32
    sentinel = c.c_uint32(123)
    for name, actual in (
        ('repeat setter negative layer', repeat_set(0, -1, 0)),
        ('repeat getter negative layer', repeat_get(0, -1, c.byref(sentinel))),
        ('repeat getter null output', repeat_get(0, 0, None)),
    ):
        results.append({'case': name, 'expected': -2, 'actual': actual})
        assert actual == -2, f'{name}: expected -2, got {actual}'
    assert sentinel.value == 123, 'rejected getter must not modify caller output'
    report['passed'] = True
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'[IMM_AUTHORING_KEY_ABI] passed {len(results)} additive layout/validation checks')


if __name__ == '__main__':
    main()
