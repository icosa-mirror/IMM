#!/usr/bin/env python3
"""Exercise the production converter CLI with real staged Windows native DLLs."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    cli = args.cli.resolve()
    source = args.source.resolve()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    report_path = out / 'report.json'
    rows = []
    report = {'passed': False, 'scope': 'production CLI with real Windows native conversion; no portable/Unity/device acceptance',
              'cliSha256': hashlib.sha256(cli.read_bytes()).hexdigest(), 'cases': rows}
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    native = out / 'native files'
    native.mkdir()
    for package in ('com.immersive-foundation.imm-unity', 'com.immersive-foundation.imm-stroke-reader'):
        directory = ROOT / 'code/ImmUnitySampleProject/Packages' / package / 'Plugins/x86_64'
        for dll in directory.glob('*.dll'):
            shutil.copy2(dll, native / dll.name)
    native_option = ['--native-directory', str(native)]
    project = out / 'project Å with spaces'
    shutil.copytree(source, project)
    original = {name: (project / name).read_bytes() for name in ('Quill.json', 'Quill.qbin')}
    working = out / 'unrelated working directory'
    working.mkdir()

    def run(name, arguments, expected, verdict=None, cli_path=cli):
        with (out / f'{name}.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(['dotnet', str(cli_path), *map(str, arguments)],
                                    cwd=working, stdout=log, stderr=subprocess.STDOUT, timeout=60)
        if result.returncode != expected:
            raise RuntimeError(f'{name}: expected exit {expected}, got {result.returncode}; inspect {name}.log')
        evidence = json.loads(verdict.read_text(encoding='utf-8')) if verdict else None
        if evidence:
            assert evidence['schema'] == 'imm-quill-conversion-report-v1'
            assert evidence['exitCode'] == expected and evidence['toolVersion']
        rows.append({'case': name, 'exitCode': result.returncode})
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
        return evidence

    run('help', ['--help'], 0)
    run('unknown-option', [project, out / 'unknown.imm', '--unknown'], 2)
    run('duplicate-option', [project, out / 'duplicate.imm', '--overwrite', '--overwrite'], 2)
    run('invalid-limit', [project, out / 'limit.imm', '--input-mib', '-1'], 2)
    run('overflow-limit', [project, out / 'overflow.imm', '--input-mib', str(2**63)], 2)
    missing = out / 'missing native'
    missing.mkdir()
    missing_report = out / 'missing-native.json'
    evidence = run('missing-native', [project, out / 'missing.imm', '--native-directory', missing,
                                    '--report', missing_report], 3, missing_report)
    assert not evidence['succeeded'] and 'zlib1.dll' in evidence['error']
    assert not (out / 'missing.imm').exists()

    output = out / 'complete output.imm'
    complete_report = out / 'complete.json'
    evidence = run('complete', [project, output, *native_option, '--report', complete_report], 0, complete_report)
    assert evidence['succeeded'] and not evidence['partial'] and evidence['bytesWritten'] == output.stat().st_size
    assert evidence['outputPublished']
    application = out / 'application files'
    application.mkdir()
    for path in cli.parent.iterdir():
        if path.is_file(): shutil.copy2(path, application / path.name)
    for path in native.glob('*.dll'): shutil.copy2(path, application / path.name)
    run('application-directory', [project, out / 'default-native-directory.imm'], 0,
        cli_path=application / cli.name)
    before = output.read_bytes()
    run('existing-output-refused', [project, output, *native_option], 2)
    assert output.read_bytes() == before
    run('overwrite', [project, output, *native_option, '--overwrite'], 0)
    run('report-output-collision', [project, out / 'collision.imm', '--report', out / 'collision.imm'], 2)
    run('report-source-collision', [project, out / 'source-collision.imm', '--report', project / 'Quill.json'], 2)

    bad = out / 'malformed project'
    shutil.copytree(source, bad)
    (bad / 'Quill.json').write_text('not JSON', encoding='utf-8')
    preserved = out / 'preserved.imm'
    marker = b'preserve existing output'
    preserved.write_bytes(marker)
    failure_report = out / 'conversion-failure.json'
    evidence = run('conversion-failure', [bad, preserved, *native_option, '--overwrite', '--report', failure_report],
                   1, failure_report)
    assert not evidence['succeeded'] and evidence['issues'] and preserved.read_bytes() == marker

    partial = out / 'partial project'
    shutil.copytree(source, partial)
    metadata = json.loads((partial / 'Quill.json').read_text(encoding='utf-8'))
    root = metadata['Sequence']['RootLayer']
    unsupported = json.loads(json.dumps(root['Implementation']['Children'][0]))
    unsupported['Name'] = 'Unsupported model'
    unsupported['Type'] = 'Model'
    unsupported['Implementation'] = {}
    root['Implementation']['Children'].append(unsupported)
    (partial / 'Quill.json').write_text(json.dumps(metadata), encoding='utf-8')
    rejected_report = out / 'unsupported-rejected.json'
    rejected = out / 'unsupported.imm'
    evidence = run('unsupported-rejected', [partial, rejected, *native_option, '--report', rejected_report],
                   1, rejected_report)
    assert not evidence['succeeded'] and not rejected.exists()
    partial_report = out / 'partial.json'
    evidence = run('partial-explicit', [partial, out / 'partial.imm', *native_option,
                                      '--allow-omissions', '--report', partial_report], 0, partial_report)
    assert evidence['succeeded'] and evidence['partial']
    assert any(issue['code'] == 'OmittedLayer' for issue in evidence['issues'])
    assert all((project / name).read_bytes() == data for name, data in original.items())
    assert not list(out.rglob('.imm-convert-*.imm')), 'CLI staging files were not removed'
    report['passed'] = True
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'[IMM_CONVERT_CLI_TEST] {len(rows)} real CLI cases passed')


if __name__ == '__main__':
    main()
