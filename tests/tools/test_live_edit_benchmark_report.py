"""Small synthetic guards against accepting incomplete or inconsistent live timings."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'code/appImmUnity/tests'))
from live_edit_benchmark_report import parse_log, validate_geometry_trace, runtime_provenance, validate_native_corpus, validate_player_corpus


def valid_log():
    lines = []
    for iteration in range(3):
        lines.append(f'[IMM_LIVE_BENCHMARK] iteration={iteration} sample={iteration-1} '
            f'warmup={int(iteration<1)} points=16 layer=4 revision={iteration+1} status=4 '
            'queueMs=.01 commitCallMs=.01 commitToPreparingMs=.02 '
            'commitToFirstPreparedMs=.03 commitToPresentedMs=.04')
        lines.append(f'[IMM_LIVE_EDIT_FRAME] sample={iteration} layer=4 revision={iteration+1} '
                     'rendererToken=7 geometryMatch=1 inFrustum=1')
    return '\n'.join(lines)


class LiveEditBenchmarkReportGuards(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='imm-live-report-')
        self.path = Path(self.directory.name) / 'viewer.log'

    def tearDown(self):
        self.directory.cleanup()

    def read(self, text):
        self.path.write_text(text, encoding='utf-8')
        return parse_log(self.path, 'small', 2, 1)

    def test_valid_log_excludes_warmup_and_matches_target(self):
        text = valid_log()
        rows = self.read(text)
        self.assertEqual([row['sample'] for row in rows], [0, 1])
        self.assertEqual(validate_geometry_trace(text, rows), 4)

    def test_incomplete_records_rejected(self):
        text = valid_log()
        variants = [text.replace('commitToPresentedMs=.04', '', 1),
                    '\n'.join(line for line in text.splitlines() if 'iteration=2 ' not in line), '']
        for corrupt in variants:
            with self.subTest(corrupt=corrupt[:60]), self.assertRaises(ValueError):
                self.read(corrupt)

    def test_geometry_trace_ignores_appended_import_fields(self):
        text = valid_log().replace('inFrustum=1',
            'inFrustum=1IMM_IMPORT: Async ReadDrawing layer=Paint 5 drawingId=182 flipped=0')
        self.assertEqual(validate_geometry_trace(text, self.read(text)), 4)

    def test_invalid_timing_status_and_sequence_rejected(self):
        text = valid_log()
        replacements = [('commitToFirstPreparedMs=.03', 'commitToFirstPreparedMs=.01'),
            ('commitToPresentedMs=.04', 'commitToPresentedMs=-1'),
            ('queueMs=.01', 'queueMs=nan'), ('commitCallMs=.01', 'commitCallMs=inf'),
            ('status=4', 'status=3'), ('iteration=2 sample=1', 'iteration=3 sample=1'),
            ('warmup=1', 'warmup=0'), ('points=16', 'points=32'),
            ('layer=4 revision=3 status=4', 'layer=4 revision=2 status=4')]
        for before, after in replacements:
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.read(text.replace(before, after, 1))

    def test_target_geometry_evidence_required(self):
        text = valid_log()
        rows = self.read(text)
        target = 'layer=4 revision=3 rendererToken=7 geometryMatch=1 inFrustum=1'
        variants = [text.replace(target, 'layer=4 revision=3 rendererToken=7 geometryMatch=1 inFrustum=0'),
                    text.replace(target, 'layer=9 revision=3 rendererToken=7 geometryMatch=1 inFrustum=1'),
                    text.replace(target, 'layer=4 revision=9 rendererToken=7 geometryMatch=1 inFrustum=1'),
                    text.replace(target, 'layer=4 revision=3 rendererToken=7 geometryMatch=1'),
                    text + '\n[IMM_LIVE_EDIT_FRAME] layer=9 revision=3 geometryMatch=1 inFrustum=1',
                    text.replace('geometryMatch=1', 'geometryMatch=0', 1)]
        for corrupt in variants:
            with self.subTest(corrupt=corrupt[-100:]), self.assertRaises(ValueError):
                validate_geometry_trace(corrupt, rows)

    def test_injected_failure_rejected_even_with_complete_records(self):
        with self.assertRaises(ValueError):
            self.read(valid_log() + '\n[IMM_LIVE_BENCHMARK_ERROR] injected failure')

    def test_logged_backend_and_gpu_are_required(self):
        text = 'GPU   : Unkown, Test GPU\nRendering Backened: DirectX\n'
        self.assertEqual(runtime_provenance(text, 'DirectX'),
                         {'loggedGpu': 'Unkown, Test GPU', 'loggedBackend': 'DirectX'})
        for corrupt in ('', text.replace('DirectX', 'OpenGL'), text + 'Rendering Backend: OpenGL\n'):
            with self.subTest(corrupt=corrupt), self.assertRaises(ValueError):
                runtime_provenance(corrupt, 'DirectX')

    def test_native_inventory_must_retain_all_drawings(self):
        source = {'fixtureSha256': 'fixture', 'case': {'frames': 12}, 'nativeCorpus': {'layers': 1, 'drawings': 10, 'strokes': 10,
            'points': 160, 'pointsPerStroke': 16, 'brush': 'Circle', 'visibility': 'Always',
            'frames': 12, 'allDrawingsReferenced': True,
            'fixtureSha256': 'fixture', 'verificationOrigin': 'synthetic test'}}
        self.assertEqual(validate_native_corpus(source, 'small')['drawings'], 10)
        for key, value in [('drawings', 1), ('points', 16), ('brush', 'Segment'), ('visibility', 'FadePow2'),
                           ('fixtureSha256', 'other'), ('verificationOrigin', ''), ('allDrawingsReferenced', False), ('frames', 1)]:
            corrupt = {'fixtureSha256': 'fixture', 'case': {'frames': 12}, 'nativeCorpus': dict(source['nativeCorpus'], **{key: value})}
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_native_corpus(corrupt, 'small')
        with self.assertRaises(ValueError):
            validate_native_corpus({'fixtureSha256': 'fixture'}, 'small')
        with self.assertRaises(ValueError):
            validate_native_corpus(dict(source, case={'frames': 1}), 'small')

    def test_player_must_retain_loaded_corpus(self):
        text = '[IMM_LIVE_BENCHMARK_CORPUS] paintLayers=1 drawings=10 frames=12 loadedStrokes=10\n[IMM_LIVE_BENCHMARK_PLAYBACK] position=0 pausedFrame=0'
        self.assertEqual(validate_player_corpus(text, 'small')['loadedStrokes'], 10)
        for corrupt in ('', f'{text}\n{text}', text.replace('drawings=10', 'drawings=1'),
                        text.replace('loadedStrokes=10', 'loadedStrokes=1'), text.replace('frames=12', 'frames=1'),
                        text.replace('position=0', 'position=1'), text.split('\n')[0]):
            with self.subTest(corrupt=corrupt), self.assertRaises(ValueError):
                validate_player_corpus(corrupt, 'small')


if __name__ == '__main__':
    unittest.main()
