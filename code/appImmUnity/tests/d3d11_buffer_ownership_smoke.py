"""Build the current D3D11 renderer source and check real unbound COM ownership."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import uuid
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--msbuild', type=Path, required=True)
    args = parser.parse_args()
    output = ROOT / 'artifacts/d3d11-buffer-ownership'
    output.mkdir(parents=True, exist_ok=True)
    sources = ('code/libImmCore/src/libRender/directx11/piDX11_Renderer.cpp',
               'code/libImmCore/src/libRender/directx11/piDX11_Renderer.h',
               'code/libImmCore/tests/d3d11/buffer_ownership.cpp',
               'code/appImmUnity/tests/d3d11_buffer_ownership_smoke.py')
    report = {'passed': False, 'backend': 'D3D11 WARP', 'iterations': [],
              'scope': 'unbound COM view and buffer ownership; no in-flight completion claim',
              'sourceSha256': {source: hashlib.sha256((ROOT / source).read_bytes()).hexdigest()
                               for source in sources}}
    report_path = output / 'report.json'
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    (output / 'run.log').write_text('', encoding='utf-8')
    namespace = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    ET.register_namespace('', namespace['m'])
    tree = ET.parse(ROOT / 'code/appImmUnity/appImmUnity.vcxproj')
    project = tree.getroot()
    for node in project.findall('.//m:ConfigurationType', namespace):
        node.text = 'Application'
    for node in project.findall('.//m:SubSystem', namespace):
        node.text = 'Console'
    for group in project.findall('m:ItemGroup', namespace):
        for node in list(group):
            if node.tag == f"{{{namespace['m']}}}ClCompile":
                group.remove(node)
    items = ET.SubElement(project, f"{{{namespace['m']}}}ItemGroup")
    # Compile the production renderer directly so the test never silently uses
    # a stale libImmCore renderer implementation or changes shared build outputs.
    for source in ('../libImmCore/tests/d3d11/buffer_ownership.cpp',
                   '../libImmCore/src/libRender/directx11/piDX11_Renderer.cpp'):
        ET.SubElement(items, f"{{{namespace['m']}}}ClCompile", Include=source)
    temporary = ROOT / f'code/appImmUnity/.d3d11-ownership-{uuid.uuid4().hex}.vcxproj'
    tree.write(temporary, encoding='utf-8', xml_declaration=True)
    try:
        with (output / 'build.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(args.msbuild), str(temporary), '/t:Build',
                '/p:Configuration=Release', '/p:Platform=x64', '/p:TargetName=D3D11BufferOwnership',
                f'/p:SolutionDir={ROOT / "code/projects/windows"}{os.sep}',
                f'/p:OutDir={output}{os.sep}', f'/p:IntDir={output / "obj"}{os.sep}',
                '/p:PostBuildEventUseInBuild=false', '/v:minimal', '/nologo'],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f'Native build failed; inspect {output / "build.log"}')
        with (output / 'run.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(output / 'D3D11BufferOwnership.exe')], cwd=output,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=30)
        text = (output / 'run.log').read_text(encoding='utf-8')
        samples = re.findall(r'\[IMM_D3D11_BUFFER_OWNERSHIP_SAMPLE\] iteration=(\d+) '
                             r'viewReferences=(\d+) resourceReferences=(\d+)', text)
        report['iterations'] = [{'iteration': int(index), 'viewReferences': int(view),
                                 'resourceReferences': int(resource)} for index, view, resource in samples]
        report['executableSha256'] = hashlib.sha256((output / 'D3D11BufferOwnership.exe').read_bytes()).hexdigest()
        report['passed'] = (result.returncode == 0 and len(samples) == 8 and
                            report['iterations'] == [{'iteration': index, 'viewReferences': 0,
                                                      'resourceReferences': 0} for index in range(8)] and
                            '[IMM_D3D11_BUFFER_OWNERSHIP] passed iterations=8 backend=WARP' in text)
        report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
        if not report['passed']:
            raise RuntimeError(f'D3D11 ownership check failed; inspect {output / "run.log"}')
        print('[IMM_D3D11_BUFFER_OWNERSHIP] passed 8 real COM view/resource release checks; artifacts retained')
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
