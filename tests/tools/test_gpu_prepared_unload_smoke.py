"""Reject incomplete uploaded-replacement cancellation evidence."""
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'code/appImmUnity/tests'))
from gpu_prepared_unload_smoke import validate_gpu_log


def rejects(text, held_capture=True):
    try:
        validate_gpu_log(text, held_capture)
    except ValueError:
        return
    raise AssertionError('Invalid cancellation evidence was accepted')


def main():
    baseline = '[IMM_LIVE_EDIT_FRAME] sample=10 layer=1 revision=0 rendererToken=2 geometryMatch=1 inFrustum=1\n'
    queued = '[IMM_LIVE_GPU_UNLOAD] queued=1 layer=1 revision=1 oldPresented=0\n'
    prepared = '[IMM_LIVE_GPU_PREPARED] revision=1 object=1 token=3 points=112 indices=630\n'
    held = ''
    for frame in range(1, 4):
        held = f'{held}[IMM_LIVE_EDIT_FRAME] sample={10 + frame} layer=1 revision=0 rendererToken=2 geometryMatch=1 inFrustum=1\n'
        held = f'{held}[IMM_LIVE_GPU_UNLOAD] stage=3 heldFrames={frame} oldState=1 prepared=1 presented=0 layer=1\n'
    capture = '[IMM_LIVE_GPU_UNLOAD] pass=1 stage=3 heldCapture=1 uploadedUnpublished=1 heldFrames=3 stableSibling=1\n'
    cancelled = '[IMM_LIVE_EDIT_CANCEL] completedToken=3\n'
    valid = f'{baseline}{queued}{prepared}{held}{capture}{cancelled}'
    assert validate_gpu_log(valid, True)['exactlyOneCancellation']
    for broken in (
            valid.replace('points=112', 'points=0'),
            valid.replace('indices=630', 'indices=0'),
            valid.replace('heldFrames=2', 'heldFrames=1'),
            valid.replace('sample=12', 'sample=11').replace('sample=13', 'sample=11'),
            valid.replace('presented=0 layer=1', 'presented=1 layer=1'),
            valid.replace(cancelled, ''),
            f'{valid}{cancelled}',
            valid.replace('completedToken=3', 'completedToken=2'),
            valid.replace('geometryMatch=1', 'geometryMatch=0'),
            valid.replace('stableSibling=1', 'stableSibling=0'),
            valid.replace(prepared, f'{prepared}renderer swap oldToken=2 newToken=3\n'),
            valid.replace(queued, '[IMM_LIVE_GPU_UNLOAD] queued=1 layer=1 revision=1 oldPresented=1\n')):
        rejects(broken)
    unload = '[IMM_LIVE_GPU_UNLOAD] unloadQueued=1 revision=1\n'
    recovery = (
        '[IMM_LIVE_EDIT_FRAME] sample=20 layer=1 revision=2 rendererToken=3 geometryMatch=1 inFrustum=1\n'
        '[IMM_LIVE_GPU_UNLOAD] pass=1 stage=6 heldCapture=0 uploadedUnpublished=1 sessionInvalid=1 '
        'reloaded=1 healthyPresented=1 stableSibling=1 layer=1 revision=2\n')
    valid_recovery = f'{baseline}{queued}{prepared}{held}{unload}{cancelled}{recovery}'
    assert validate_gpu_log(valid_recovery, False)['exactlyOneCancellation']
    rejects(valid_recovery.replace('sessionInvalid=1', 'sessionInvalid=0'), False)
    rejects(valid_recovery.replace(unload, ''), False)
    rejects(valid_recovery.replace('sample=20 layer=1 revision=2', 'sample=20 layer=1 revision=0'), False)
    print('GPU-prepared cancellation evidence checks passed')


if __name__ == '__main__':
    main()
