"""Exercise licence cleanup and strict result handling without Unity or credentials."""
import base64
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

from run_unity_editor_native_tests import REQUIRED_TESTS, license_serial, run_tests


class NativeEditorRunnerTests(unittest.TestCase):
    def exercise(self, fail=None):
        serial, password = 'F4-FAKE-TEST', 'fake-test-password'
        data = base64.b64encode(b'\x01\x00\x00\x00' + serial.encode()).decode()
        env = {'UNITY_LICENSE': f'<root><License><DeveloperData Value="{data}"/></License></root>',
               'UNITY_EMAIL': 'fake@example.invalid', 'UNITY_PASSWORD': password}
        calls = []
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project = root / 'project Å with spaces'
            project.mkdir()
            output = root / 'output'

            def runner(arguments, **kwargs):
                phase = 'activation' if '-serial' in arguments else 'return' if '-returnlicense' in arguments else 'tests'
                calls.append(phase)
                log = Path(arguments[arguments.index('-logFile') + 1])
                log.write_text(f'{serial} {password} {env["UNITY_EMAIL"]}')
                kwargs['stdout'].write(f'{serial} {password}')
                if phase == 'tests':
                    self.assertNotIn('-quit', arguments)
                    self.assertEqual(arguments[arguments.index('-projectPath') + 1], str(project))
                    if fail == 'timeout':
                        raise subprocess.TimeoutExpired(arguments, 1)
                    document = ET.Element('test-run', result='Passed', total='3', passed='3', failed='0')
                    for name in REQUIRED_TESTS:
                        ET.SubElement(document, 'test-case', fullname=name, result='Passed')
                    if fail == 'missing-test':
                        document.remove(document[-1])
                    if fail == 'skipped-test':
                        for case in document:
                            case.set('result', 'Skipped')
                    ET.ElementTree(document).write(arguments[arguments.index('-testResults') + 1])
                return subprocess.CompletedProcess(arguments, 1 if fail == phase else 0)

            report = run_tests(Path('fake-editor'), project, output, env, runner=runner)
            for path in output.iterdir():
                text = path.read_text()
                self.assertNotIn(serial, text)
                self.assertNotIn(password, text)
                self.assertNotIn(env['UNITY_EMAIL'], text)
            self.assertEqual(json.loads((output / 'quill-editor-runner.json').read_text()), report)
            return report, calls

    def test_successful_activation_test_and_return(self):
        report, calls = self.exercise()
        self.assertTrue(report['passed'])
        self.assertEqual(calls, ['activation', 'tests', 'return'])

    def test_failure_and_timeout_always_return_license(self):
        for failure in ['activation', 'tests', 'return', 'timeout', 'missing-test', 'skipped-test']:
            with self.subTest(failure=failure):
                report, calls = self.exercise(failure)
                self.assertFalse(report['passed'])
                self.assertEqual(calls[-1], 'return')
                self.assertEqual('tests' in calls, failure != 'activation')

    def test_invalid_license_rejects_without_exposing_contents(self):
        for value in ['', '<root/>', '<root><DeveloperData Value="!bad"/></root>']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                license_serial({'UNITY_LICENSE': value})


if __name__ == '__main__':
    unittest.main()
