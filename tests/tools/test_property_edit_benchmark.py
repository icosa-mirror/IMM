"""Reject incomplete timing evidence and property captures with hidden failures."""
import importlib.util
from pathlib import Path
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'code/appImmUnity/tests'))
SPEC = importlib.util.spec_from_file_location(
    'property_edit_benchmark', ROOT / 'code/appImmUnity/tests/property_edit_benchmark.py')
BENCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCH)


def rejects(operation):
    try:
        operation()
    except ValueError:
        return
    raise AssertionError('Invalid evidence was accepted')


def write_capture(path, red_x=0, red=200, blue=200):
    width, height = 32, 8
    pixels = bytearray(width * height * 3)
    for y in range(4):
        for x in range(4):
            if red:
                offset = (y * width + x + red_x) * 3
                pixels[offset:offset + 3] = bytes((red, 0, 0))
            offset = (y * width + x + 24) * 3
            pixels[offset:offset + 3] = bytes((0, 0, blue))
    path.write_bytes(f'P6\n{width} {height}\n255\n'.encode() + pixels)


def main():
    with tempfile.TemporaryDirectory() as directory:
        folder = Path(directory)
        control, changed = folder / 'control.ppm', folder / 'changed.ppm'
        write_capture(control)
        write_capture(changed, red=0)
        BENCH.compare_captures(control, changed, 'visibility')
        write_capture(changed, red=70)
        BENCH.compare_captures(control, changed, 'opacity')
        write_capture(changed, red_x=4)
        BENCH.compare_captures(control, changed, 'transform')
        write_capture(changed)
        for prop in BENCH.PROPERTIES:
            rejects(lambda: BENCH.compare_captures(control, changed, prop))
        write_capture(changed, red=0, blue=190)
        rejects(lambda: BENCH.compare_captures(control, changed, 'visibility'))
        changed.write_bytes(b'P6\n32 8\n255\nshort')
        rejects(lambda: BENCH.compare_captures(control, changed, 'visibility'))

        log = folder / 'debug.txt'
        valid = (
            '[IMM_LIVE_PROPERTY_CORPUS] property=opacity paintLayers=2 drawings=10 frames=24 loadedStrokes=10 points=16\n'
            '[IMM_LIVE_EDIT_FRAME] sample=10 layer=1 revision=0 rendererToken=0 geometryMatch=1 inFrustum=1\n'
            '[IMM_LIVE_EDIT_FRAME] sample=10 layer=2 revision=0 rendererToken=5 geometryMatch=1 inFrustum=1\n'
            '[IMM_LIVE_PROPERTY_SETUP] property=opacity visibilityKeyRemoved=1 overrides=0 pausedFrame=0 paintDrawCalls=2 triangles=32\n'
            '[IMM_LIVE_EDIT_FRAME] sample=11 layer=1 revision=0 rendererToken=0 geometryMatch=1 inFrustum=1\n'
            '[IMM_LIVE_EDIT_FRAME] sample=11 layer=2 revision=0 rendererToken=5 geometryMatch=1 inFrustum=1\n'
            '[IMM_LIVE_PROPERTY_BENCHMARK] iteration=0 sample=0 warmup=0 points=16 '
            'layer=1 revision=2 status=4 property=opacity control=0 queueMs=0.01 '
            'commitCallMs=0.02 commitToPreparingMs=0.03 commitToFirstPreparedMs=0.04 '
            'commitToPresentedMs=0.05 effective=1 stableGeometry=1 siblingStable=1 paintDrawCalls=2 triangles=32 changed=1\n'
            '[IMM_LIVE_PROPERTY_BENCHMARK_DONE] property=opacity control=0 samples=1 warmup=0 effect=changed stableGeometry=1 siblingStable=1 effective=1\n'
            'IMM GL validation: frame=65 paintDrawCalls=2 triangles=32\n')
        log.write_text(valid)
        entries = BENCH.parse_log(log, 'small', 'opacity', 1, 0)
        assert BENCH.summarize(entries)['nativePublicationP95MeetsTarget']
        for broken in (
                valid.replace('stableGeometry=1', 'stableGeometry=0'),
                valid.replace('commitToPresentedMs=0.05', 'commitToPresentedMs=nan'),
                valid.replace('commitToFirstPreparedMs=0.04', 'commitToFirstPreparedMs=0.06'),
                valid.replace('property=opacity', 'property=visibility'),
                valid.replace('paintDrawCalls=2', 'paintDrawCalls=0'),
                valid.replace('control=0', 'control=1'),
                valid.replace('loadedStrokes=10', 'loadedStrokes=9'),
                valid.replace('samples=1', 'samples=2'),
                valid.replace('geometryMatch=1', 'geometryMatch=0'),
                valid.replace('effective=1 stableGeometry=1 siblingStable=1 paintDrawCalls=2', 'paintDrawCalls=2'),
                valid.replace('sample=11 layer=2 revision=0 rendererToken=5',
                              'sample=11 layer=2 revision=0 rendererToken=6'),
                valid.replace('[IMM_LIVE_EDIT_FRAME] sample=11 layer=1 revision=0 rendererToken=0 geometryMatch=1 inFrustum=1\n', ''),
                valid + '[IMM_LIVE_PROPERTY_BENCHMARK_ERROR] render mismatch\n'):
            log.write_text(broken)
            rejects(lambda: BENCH.parse_log(log, 'small', 'opacity', 1, 0))
    print('Property benchmark evidence checks passed')


if __name__ == '__main__':
    main()
