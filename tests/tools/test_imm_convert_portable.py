#!/usr/bin/env python3
"""Run the extracted self-contained CLI with global .NET discovery disabled."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import subprocess
import zipfile


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    archive = args.archive.resolve()
    source = args.source.resolve()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    report_path = out / 'report.json'
    report = {'passed': False, 'schema': 'imm-convert-portable-test-v1',
              'archiveSha256': digest(archive),
              'scope': 'extracted archive with isolated .NET discovery and actual local CLR/CRT/native module paths; not a clean VM or desktop/Unity acceptance'}
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    extracted = out / 'extracted Å with spaces'
    extracted.mkdir()
    with zipfile.ZipFile(archive) as bundle:
        for entry in bundle.infolist():
            name = entry.filename.replace('\\', '/')
            path = PurePosixPath(name)
            if path.is_absolute() or PureWindowsPath(name).drive or '..' in path.parts:
                raise RuntimeError(f'Unsafe archive path: {entry.filename}')
            if not extracted.joinpath(*path.parts).resolve().is_relative_to(extracted):
                raise RuntimeError(f'Archive path escapes extraction: {entry.filename}')
        bundle.extractall(extracted)
    application = extracted / 'imm-convert-win-x64'
    manifest_path = application / 'portable-manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    expected_files = set()
    for row in manifest['files']:
        path = (application / row['path']).resolve()
        if not path.is_relative_to(application): raise RuntimeError('Manifest path escapes package')
        assert path.stat().st_size == row['bytes'] and digest(path) == row['sha256'], row['path']
        expected_files.add(path.relative_to(application).as_posix())
    actual_files = {p.relative_to(application).as_posix() for p in application.rglob('*') if p.is_file()}
    assert actual_files - {'portable-manifest.json'} == expected_files
    config = json.loads((application / 'imm-convert.runtimeconfig.json').read_text(encoding='utf-8'))['runtimeOptions']
    assert config['includedFrameworks'] and 'framework' not in config and 'frameworks' not in config
    assert (application / 'coreclr.dll').is_file() and (application / 'vcruntime140.dll').is_file()
    project = out / 'Quill Å project'
    shutil.copytree(source, project)
    working = out / 'working directory'
    working.mkdir()
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith('DOTNET_')}
    absent = str(out / 'absent global runtime')
    env.update({'DOTNET_ROOT': absent, 'DOTNET_ROOT_X64': absent, 'DOTNET_MULTILEVEL_LOOKUP': '0',
                'DOTNET_ROLL_FORWARD': 'Disable', 'COREHOST_TRACE': '1',
                'COREHOST_TRACEFILE': str(out / 'host-trace.log'),
                'PATH': os.pathsep.join([str(Path(os.environ['SystemRoot']) / 'System32'), os.environ['SystemRoot']])})
    executable = application / 'imm-convert.exe'
    conversion_report = out / 'conversion.json'
    output = out / 'converted.imm'
    for name, arguments in [('help', ['--help']), ('convert', [str(project), str(output), '--report', str(conversion_report)])]:
        with (out / f'{name}.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(executable), *arguments], cwd=working, env=env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=60)
        if result.returncode != 0: raise RuntimeError(f'{name} failed with exit {result.returncode}')
    conversion = json.loads(conversion_report.read_text(encoding='utf-8'))
    assert conversion['succeeded'] and conversion['outputPublished'] and not conversion['partial']
    assert conversion['bytesWritten'] == output.stat().st_size
    runtime = conversion['runtime']
    assert Path(runtime['directory']).resolve() == application.resolve()
    assert runtime['moduleInspectionError'] is None, runtime['moduleInspectionError']
    for name in ('coreclr.dll', 'vcruntime140.dll', 'immunityplugin.dll'):
        assert Path(runtime['modules'][name]).resolve() == (application / name).resolve(), name
    # Windows .NET host tracing uses the native ANSI code page. Python UTF-8
    # mode changes read_text's default, not the encoding written by that host.
    trace = (out / 'host-trace.log').read_text(encoding='mbcs')
    assert str(application).casefold() in trace.casefold()
    assert not list(out.rglob('.imm-convert-*.imm'))
    report.update({'passed': True, 'sourceRevision': manifest['sourceRevision'],
                   'workingTreeDirty': manifest['workingTreeDirty'], 'runtime': runtime,
                   'verifiedFiles': len(expected_files), 'nativeFiles': manifest['nativeFiles'],
                   'outputSha256': digest(output)})
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'[IMM_CONVERT_PORTABLE_TEST] {len(expected_files)} files verified; extracted apphost and local CLR/CRT/native conversion passed')


if __name__ == '__main__':
    main()
