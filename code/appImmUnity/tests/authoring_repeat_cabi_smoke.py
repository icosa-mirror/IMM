"""Exercise production C export definitions in an owned Windows WARP host.

This is linked native delivery, not Unity/PInvoke marshalling or visual acceptance.
The separate dynamic-DLL ABI and rendered-viewer tests retain those scopes.
"""
from pathlib import Path
import argparse
import ctypes as c
import hashlib
import json
import os
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET

from exporter_bridge_smoke import require, runtime_dependencies
from paint_playback_keys_smoke import fixture

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--msbuild', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows WARP host required')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'scope': __doc__.strip()}
    report_path = out / 'report.json'
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    plugin = ROOT / 'code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll'
    for path in (plugin, *runtime_dependencies(ROOT)):
        shutil.copy2(path, out / path.name)
    c.windll.kernel32.SetDllDirectoryW(str(out))
    source = out / 'paint.imm'
    fixture(out / plugin.name, source)
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    ET.register_namespace('', ns['m'])
    tree = ET.parse(ROOT / 'code/appImmUnity/appImmUnity.vcxproj')
    project = tree.getroot()
    for node in project.findall('.//m:ConfigurationType', ns):
        node.text = 'Application'
    for node in project.findall('.//m:SubSystem', ns):
        node.text = 'Console'
    for group in project.findall('m:ItemGroup', ns):
        for node in list(group):
            if node.tag == f"{{{ns['m']}}}ClCompile" and node.get('Include', '').replace('\\', '/') == 'src/main.cpp':
                group.remove(node)
    items = ET.SubElement(project, f"{{{ns['m']}}}ItemGroup")
    ET.SubElement(items, f"{{{ns['m']}}}ClCompile", Include='tests/authoring_repeat_cabi_test.cpp')
    temporary = ROOT / f'code/appImmUnity/.repeat-cabi-{uuid.uuid4().hex}.vcxproj'
    tree.write(temporary, encoding='utf-8', xml_declaration=True)
    try:
        with (out / 'build.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(args.msbuild), str(temporary), '/t:Build',
                '/p:Configuration=Release', '/p:Platform=x64', '/p:TargetName=RepeatCabiTest',
                f'/p:SolutionDir={ROOT / "code/projects/windows"}{os.sep}',
                f'/p:OutDir={out}{os.sep}', f'/p:IntDir={out / "obj"}{os.sep}',
                '/p:PostBuildEventUseInBuild=false', '/v:minimal', '/nologo'],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=300)
        require(result.returncode == 0, f'build (see {out / "build.log"})')
        executable = out / 'RepeatCabiTest.exe'
        report['executableSha256'] = hashlib.sha256(executable.read_bytes()).hexdigest()
        report['fixtureSha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
        report['fixtureExporterSha256'] = hashlib.sha256((out / plugin.name).read_bytes()).hexdigest()
        with (out / 'run.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(executable), str(source)], cwd=out,
                stdout=log, stderr=subprocess.STDOUT, timeout=90)
        report['exitCode'] = result.returncode
        report['passed'] = result.returncode == 0 and '[IMM_REPEAT_CABI_TEST] pass=1' in (out / 'run.log').read_text()
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
        require(report['passed'], f'native delivery (see {out / "run.log"})')
        print('[IMM_REPEAT_CABI_TEST] production exports passed in a loaded-document WARP host')
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
