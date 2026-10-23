#!/usr/bin/env python3
"""Run native Editor tests with an owned activate/test/return licence lifecycle."""
from __future__ import annotations

import argparse
import base64
import json
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from verify_unity_test_results import verify_results

TEST_CLASS = 'ImmPlayer.Editor.Tests.QuillToImmEditorTests'
REQUIRED_TESTS = [f'{TEST_CLASS}.{name}' for name in (
    'OutputMustBeAnImmFileInsideAssets',
    'ExistingOutputAndPreCancellationArePreserved',
    'NativeProjectConversionImportsAnAssetAndProtectsExistingOutput',
)]


def license_serial(env: dict[str, str]) -> str:
    serial = env.get('UNITY_SERIAL', '').strip()
    if not serial:
        try:
            root = ET.fromstring(env.get('UNITY_LICENSE', ''))
            node = root.find('.//DeveloperData')
            value = node.get('Value', '') if node is not None else ''
            decoded = base64.b64decode(value, validate=True)
            if len(decoded) <= 4:
                raise ValueError('missing serial')
            serial = decoded[4:].decode('latin-1').strip()
        except (ET.ParseError, ValueError) as error:
            raise ValueError('UNITY_LICENSE must contain valid DeveloperData or provide UNITY_SERIAL') from error
    if not serial or any(not char.isprintable() or char.isspace() for char in serial):
        raise ValueError('Unity serial is empty or malformed')
    return serial


def run_tests(editor: Path, project: Path, output: Path, env: dict[str, str],
              runner=subprocess.run, timeout: int = 240) -> dict:
    serial = license_serial(env)
    email, password = env.get('UNITY_EMAIL', ''), env.get('UNITY_PASSWORD', '')
    if not email or not password:
        raise ValueError('UNITY_EMAIL and UNITY_PASSWORD are required for native Editor activation')
    output.mkdir(parents=True, exist_ok=True)
    result_path = output / 'quill-editor-tests.xml'
    if result_path.exists():
        raise ValueError('Native Editor result XML already exists; use a fresh output directory')
    secrets = [serial, email, password, env.get('UNITY_LICENSE', '')]
    report = {'passed': False, 'scope': 'real native Editor tests with independent serial activation/return', 'phases': {}}

    def redact(path: Path) -> None:
        if path.is_file():
            text = path.read_text(encoding='utf-8', errors='replace')
            for secret in sorted(filter(None, secrets), key=len, reverse=True):
                text = text.replace(secret, '[redacted]')
            path.write_text(text, encoding='utf-8')

    def phase(name: str, arguments: list[str], cwd: Path) -> bool:
        log = output / f'quill-editor-{name}.log'
        console = output / f'quill-editor-{name}-console.log'
        state = {'exitCode': None, 'timedOut': False}
        report['phases'][name] = state
        try:
            with console.open('w', encoding='utf-8') as stream:
                process = runner([str(editor), *arguments, '-logFile', str(log)],
                                 cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                 timeout=timeout, env=env)
            state['exitCode'] = process.returncode
        except subprocess.TimeoutExpired:
            state['timedOut'] = True
        except OSError as error:
            state['errorType'] = type(error).__name__
        finally:
            redact(log)
            redact(console)
        return state['exitCode'] == 0

    with tempfile.TemporaryDirectory(prefix='imm-editor-license-') as temporary:
        activation_project = Path(temporary)
        common = ['-batchmode', '-nographics', '-quit', '-username', email,
                  '-password', password, '-projectPath', str(activation_project)]
        try:
            if phase('activation', [*common, '-serial', serial], activation_project):
                tests_ok = phase('tests', ['-batchmode', '-nographics', '-projectPath', str(project),
                    '-runTests', '-testPlatform', 'EditMode', '-testFilter', TEST_CLASS,
                    '-testResults', str(result_path)], project)
                errors = verify_results(result_path, REQUIRED_TESTS)
                report['testResultErrors'] = errors
                report['testsPassed'] = tests_ok and not errors
        finally:
            returned = phase('return', [*common, '-returnlicense'], activation_project)
            redact(result_path)
            report['passed'] = report.get('testsPassed', False) and returned
            (output / 'quill-editor-runner.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = run_tests(args.editor.resolve(), args.project.resolve(), args.output_dir.resolve(), dict(os.environ))
    except ValueError:
        print('Native Editor test configuration is invalid; check licence and credentials or stale result XML.')
        return 1
    print(f'Native Editor tests passed={result["passed"]}; sanitized phase logs and strict results retained.')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
