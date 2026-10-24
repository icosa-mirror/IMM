"""Check the native key layout and C ABI validation without initializing a player.

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


class Key(c.Structure):
    _fields_ = [('structSize', c.c_uint32), ('structVersion', c.c_uint32),
                ('property', c.c_int32), ('interpolation', c.c_int32), ('timeSeconds', c.c_double),
                ('boolValue', c.c_int32), ('floatValue', c.c_float),
                *[(name, c.c_float) for name in ('tx', 'ty', 'tz', 'qx', 'qy', 'qz', 'qw', 'scale')],
                ('uintValue', c.c_uint32), ('transformFlip', c.c_int32), ('doubleValue', c.c_double)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, default=ROOT / 'code/appImmUnity/exe/ImmUnityPlugin.dll')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows native plugin required')
    assert c.sizeof(Key) == 80 and Key.uintValue.offset == 64 and Key.transformFlip.offset == 68 and Key.doubleValue.offset == 72
    output = ROOT / 'artifacts/authoring-key-abi'
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / 'report.json'
    results = []
    report = {'passed': False, 'pluginSha256': hashlib.sha256(args.plugin.read_bytes()).hexdigest(),
              'keySize': c.sizeof(Key), 'uintValueOffset': Key.uintValue.offset,
              'doubleValueOffset': Key.doubleValue.offset,
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

    def descriptor(prop=99):
        key = Key()
        key.structSize, key.structVersion, key.property = c.sizeof(Key), 1, prop
        return key

    check('layout accepted before unsupported property', descriptor(), -3)
    key = descriptor(8)
    key.structSize = c.sizeof(Key) - 4
    check('truncated key', key, -2)
    key = descriptor(8)
    key.structVersion = 2
    check('unknown version', key, -3)
    key = descriptor(7)
    key.boolValue = 2
    check('Loop strict boolean', key, -2)
    key = descriptor(7)
    key.interpolation = 1
    check('Loop step only', key, -2)
    key = descriptor(8)
    key.uintValue, key.interpolation = 0xFFFFFFFF, 1
    check('Offset step only with full-width unsigned payload', key, -2)
    key = descriptor()
    key.transformFlip = 4
    check('invalid transform reflection', key, -2)
    for index in range(2):
        key = descriptor(8)
        c.c_uint32.from_buffer(key, Key.doubleValue.offset + index * 4).value = 1
        check(f'non-DrawIn double payload word {index}', key, -2)
    for value in (-1.0, float('nan'), float('inf')):
        key = descriptor(5)
        key.doubleValue = value
        check(f'invalid DrawIn value {value}', key, -2)
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
    name_set = plugin.ImmAuthoring_LayerSetName
    name_set.argtypes = [c.c_int32, c.c_int32, c.c_char_p, c.c_uint32]
    name_set.restype = c.c_int32
    name_get = plugin.ImmAuthoring_LayerGetName
    name_get.argtypes = [c.c_int32, c.c_int32, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint32)]
    name_get.restype = c.c_int32
    for name, actual in (
        ('name setter negative layer', name_set(0, -1, b'x', 1)),
        ('name setter null input', name_set(0, 0, None, 1)),
        ('name setter empty input', name_set(0, 0, b'', 0)),
        ('name setter oversized input', name_set(0, 0, b'x', 4097)),
        ('name setter malformed UTF8', name_set(0, 0, b'\xc0\xaf', 2)),
        ('name setter embedded NUL', name_set(0, 0, b'a\0b', 3)),
        ('name getter negative layer', name_get(0, -1, None, 0, c.byref(sentinel))),
        ('name getter null size output', name_get(0, 0, None, 0, None)),
        ('name getter invalid null capacity', name_get(0, 0, None, 1, c.byref(sentinel))),
    ):
        results.append({'case': name, 'expected': -2, 'actual': actual})
        assert actual == -2, f'{name}: expected -2, got {actual}'
    assert sentinel.value == 123, 'rejected name getter must preserve size output'
    report['passed'] = True
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'[IMM_AUTHORING_KEY_ABI] passed {len(results)} layout/validation checks')


if __name__ == '__main__':
    main()
