"""Validate repeated native publication timings against matched fixture provenance.

This consumes owned viewer logs; it does not launch viewers or establish display,
GPU-completion, retirement or whole-SDK acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import platform
import re
import statistics

METRICS = ('queueMs', 'commitCallMs', 'commitToPreparingMs',
           'commitToFirstPreparedMs', 'commitToPresentedMs')
CASES = {'small': (10, 16, 16), 'medium': (400, 32, 16), 'large': (4000, 64, 50)}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def summarize(values):
    return {'median': statistics.median(values), 'p95': sorted(values)[math.ceil(len(values) * .95) - 1],
            'min': min(values), 'max': max(values), 'samples': values}


def parse_log(path, case, samples, warmup):
    entries = []
    text = path.read_text(encoding='utf-8', errors='replace')
    if '[IMM_LIVE_BENCHMARK_ERROR]' in text:
        raise ValueError(f'{case}: native benchmark reported a failure')
    for line in text.splitlines():
        if '[IMM_LIVE_BENCHMARK]' not in line:
            continue
        fields = dict(re.findall(r'(\w+)=([^\s]+)', line))
        try:
            entry = {key: int(fields[key]) for key in ('iteration', 'sample', 'warmup', 'points', 'layer', 'revision', 'status')}
            entry.update({key: float(fields[key]) for key in METRICS})
        except (KeyError, ValueError) as error:
            raise ValueError(f'{case}: incomplete benchmark record') from error
        if entry['status'] != 4 or entry['points'] != CASES[case][1] or entry['revision'] <= 0:
            raise ValueError(f'{case}: rejected/unexpected benchmark operation')
        if any(not math.isfinite(entry[key]) or entry[key] < 0 for key in METRICS):
            raise ValueError(f'{case}: missing or invalid timing')
        if not entry['commitToPreparingMs'] <= entry['commitToFirstPreparedMs'] <= entry['commitToPresentedMs']:
            raise ValueError(f'{case}: nonmonotonic publication transitions')
        entries.append(entry)
    if len(entries) != warmup + samples:
        raise ValueError(f'{case}: expected {warmup + samples} records, found {len(entries)}')
    for iteration, entry in enumerate(entries):
        if entry['iteration'] != iteration or entry['sample'] != iteration - warmup or entry['warmup'] != int(iteration < warmup):
            raise ValueError(f'{case}: warmup/sample sequence mismatch')
        if iteration and entry['revision'] <= entries[iteration - 1]['revision']:
            raise ValueError(f'{case}: revision did not advance')
    return entries[warmup:]


def validate_geometry_trace(text, entries):
    traces = set()
    all_traces = []
    for line in text.splitlines():
        if '[IMM_LIVE_EDIT_FRAME]' not in line:
            continue
        # Import-thread messages may share a line with this complete trace.
        # Parse only its ordered numeric record, not a later layer=Paint name.
        record = re.search(r'\[IMM_LIVE_EDIT_FRAME\] sample=\d+ layer=(\d+) revision=(\d+) '
                           r'rendererToken=\d+ geometryMatch=([01]) inFrustum=([01])', line)
        if record is None:
            raise ValueError('Malformed renderer geometry trace')
        fields = dict(zip(('layer', 'revision', 'geometryMatch', 'inFrustum'), record.groups()))
        if fields.get('geometryMatch') != '1':
            raise ValueError('A sampled model/renderer geometry mismatch was recorded')
        if fields.get('inFrustum') == '1':
            traces.add((int(fields['layer']), int(fields['revision'])))
        all_traces.append((int(fields['layer']), int(fields['revision'])))
    layers = {entry['layer'] for entry in entries}
    if len(layers) != 1 or any((entry['layer'], entry['revision']) not in traces for entry in entries):
        raise ValueError('A measured target-layer revision has no matching in-frustum geometry trace')
    target = next(iter(layers))
    if any(layer != target and revision != 0 for layer, revision in all_traces):
        raise ValueError('An unrelated selected drawing received an authoring revision')
    return target


def runtime_provenance(text, expected_backend):
    # Retain the renderer's actual strings, including its historical spelling.
    gpus = set(re.findall(r'^\s*GPU\s*:\s*(.+?)\s*$', text, re.MULTILINE))
    backends = set(re.findall(r'^\s*Rendering Back(?:end|ened)\s*:\s*(.+?)\s*$', text, re.MULTILINE))
    if len(gpus) != 1 or len(backends) != 1:
        raise ValueError('Missing or ambiguous logged GPU/backend provenance')
    gpu, backend = next(iter(gpus)), next(iter(backends))
    if backend.casefold() != expected_backend.casefold():
        raise ValueError(f'Logged renderer backend {backend} differs from requested {expected_backend}')
    return {'loggedGpu': gpu, 'loggedBackend': backend}


def validate_native_corpus(source, case):
    expected_drawings, points_per_stroke, _ = CASES[case]
    inventory = source.get('nativeCorpus', {})
    expected = {'layers': {'small': 1, 'medium': 4, 'large': 8}[case],
                'drawings': expected_drawings, 'strokes': expected_drawings,
                'points': expected_drawings * points_per_stroke, 'pointsPerStroke': points_per_stroke,
                'frames': {'small': 12, 'medium': 480, 'large': 4000}[case], 'allDrawingsReferenced': True,
                'brush': 'Circle', 'visibility': 'Always', 'fixtureSha256': source['fixtureSha256']}
    if any(inventory.get(key) != value for key, value in expected.items()) or not inventory.get('verificationOrigin'):
        raise ValueError(f'{case}: missing or mismatched decoded native corpus inventory')
    if source.get('case', {}).get('frames') != expected['frames'] // expected['layers']:
        raise ValueError(f'{case}: case frame metadata differs from decoded native inventory')
    return inventory


def validate_player_corpus(text, case):
    lines = [line for line in text.splitlines() if '[IMM_LIVE_BENCHMARK_CORPUS]' in line]
    if len(lines) != 1:
        raise ValueError(f'{case}: expected one resident player corpus record')
    fields = dict(re.findall(r'(\w+)=([^\s]+)', lines[0]))
    expected = {'paintLayers': {'small': 1, 'medium': 4, 'large': 8}[case],
                'drawings': CASES[case][0], 'frames': {'small': 12, 'medium': 480, 'large': 4000}[case],
                'loadedStrokes': CASES[case][0]}
    if any(fields.get(key) != str(value) for key, value in expected.items()):
        raise ValueError(f'{case}: resident player corpus differs from matched workload')
    playback = [line for line in text.splitlines() if '[IMM_LIVE_BENCHMARK_PLAYBACK]' in line]
    if len(playback) != 1:
        raise ValueError(f'{case}: missing paused-at-zero player evidence')
    clocks = dict(re.findall(r'(\w+)=([^\s]+)', playback[0]))
    if clocks.get('position') != '0' or clocks.get('pausedFrame') != '0':
        raise ValueError(f'{case}: player was not paused at frame zero')
    expected['playbackPosition'] = 0
    expected['pausedFrame'] = 0
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', action='append', required=True, metavar='CASE=PATH')
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--graphics-api', required=True)
    parser.add_argument('--samples', type=int, default=30)
    parser.add_argument('--warmup', type=int, default=5)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.samples <= 0 or args.warmup < 0:
        parser.error('samples must be positive and warmup nonnegative')
    baseline = json.loads(args.baseline.read_text(encoding='utf-8'))
    result = {'scope': 'Native internal Commit-entry to model/renderer publication; not displayed-frame, GPU-completion or retirement latency.',
              'baselineScope': 'Exporter stage-sum plus stroke-reader reload; excludes player GPU loading and displayed frames. Endpoint differs from publication timings.',
              'platform': platform.platform(), 'graphicsApi': args.graphics_api,
              'viewerSha256': sha256(args.viewer), 'baselineReportSha256': sha256(args.baseline),
              'samples': args.samples, 'warmup': args.warmup, 'cases': {}}
    for argument in args.log:
        case, separator, filename = argument.partition('=')
        if not separator or case not in CASES or case in result['cases']:
            parser.error('each --log must identify one unique small/medium/large case')
        source = baseline[case]
        expected_drawings, points, target = CASES[case]
        fixture = args.fixtures / f'{case}.imm'
        if source.get('workload') != 'matched-live-first-drawing-v2' or source.get('warmup') != args.warmup:
            raise ValueError(f'{case}: baseline workload/warmup is not matched')
        if source.get('fixtureSha256') != sha256(fixture) or source['drawings'] != expected_drawings:
            raise ValueError(f'{case}: baseline fixture/count mismatch')
        inventory = validate_native_corpus(source, case)
        changed = source.get('changedDrawing', {})
        if changed.get('points') != points or changed.get('allDrawingsReferenced') is not True or changed.get('pausedAtFrameZero') is not True:
            raise ValueError(f'{case}: baseline geometry/frame mapping is not matched')
        iterations = source['measuredIterations']
        if len(iterations) != args.samples or any(entry['iteration'] != args.warmup + index or
                not math.isclose(entry['firstDrawingY'], 1.52 if entry['iteration'] % 2 == 0 else 1.5, abs_tol=1e-9)
                for index, entry in enumerate(iterations)):
            raise ValueError(f'{case}: baseline edit parity/sample count mismatch')
        log = Path(filename)
        entries = parse_log(log, case, args.samples, args.warmup)
        text = log.read_text(encoding='utf-8', errors='replace')
        target_layer = validate_geometry_trace(text, entries)
        runtime = runtime_provenance(text, args.graphics_api)
        player_corpus = validate_player_corpus(text, case)
        metrics = {key: summarize([entry[key] for entry in entries]) for key in METRICS}
        result['cases'][case] = {'fixtureSha256': source['fixtureSha256'], 'logSha256': sha256(log),
            'exporterSha256': source['exporterSha256'], 'readerSha256': source['readerSha256'],
            'drawings': expected_drawings, 'pointsPerChangedDrawing': points, 'metrics': metrics,
            'targetLayer': target_layer, 'targetRevisionGeometryTrace': True, 'targetRevisionInFrustumTrace': True,
            'unrelatedSelectedRevisionsUnchanged': True,
            'nativeCorpus': inventory,
            'residentPlayerCorpus': player_corpus,
            **runtime,
            'editDocumentStageSum': source['editDocumentStageSum'], 'records': entries,
            'publicationTargetMs': target, 'publicationP95WithinTarget': metrics['commitToPresentedMs']['p95'] < target,
            'acceptanceLimit': 'Publication timing and sampled in-frustum selected model/renderer consistency. Rasterization, retirement and cross-backend acceptance require separate evidence.'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f"[IMM_LIVE_BENCHMARK_REPORT] validated {len(result['cases'])} cases; publication timing only; wrote {args.output}")


if __name__ == '__main__':
    main()
