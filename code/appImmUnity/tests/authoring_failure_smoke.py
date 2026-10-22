"""Build and run the CPU-only authoring cancellation regression on Windows.

Build appImmUnity/libImmPlayer Release first. No Unity Editor or renderer is started.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from exporter_bridge_smoke import require, runtime_dependencies

from viewpoint_policy_smoke import fixture

ROOT = Path(__file__).resolve().parents[3]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--msbuild', type=Path, required=True)
    args = parser.parse_args()
    out = ROOT / 'artifacts/authoring-failure'
    out.mkdir(parents=True, exist_ok=True)
    plugin = ROOT / 'code/appImmUnity/exe/ImmUnityPlugin.dll'
    for dependency in runtime_dependencies(ROOT):
        shutil.copy2(dependency, out / dependency.name)
    staged_plugin = out / plugin.name
    shutil.copy2(plugin, staged_plugin)
    fixture(staged_plugin, out / 'fixture.imm')
    # Reuse the native target's real link configuration, without post-build Unity copying.
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    ET.register_namespace('', ns['m'])
    tree = ET.parse(ROOT / 'code/appImmUnity/appImmUnity.vcxproj')
    project = tree.getroot()
    for node in project.findall('.//m:ConfigurationType', ns): node.text = 'Application'
    for node in project.findall('.//m:SubSystem', ns): node.text = 'Console'
    for group_node in project.findall('m:ItemGroup', ns):
        for node in list(group_node):
            if node.tag == f"{{{ns['m']}}}ClCompile": group_node.remove(node)
    items = ET.SubElement(project, f"{{{ns['m']}}}ItemGroup")
    ET.SubElement(items, f"{{{ns['m']}}}ClCompile", Include='tests/authoring_failure_test.cpp')
    temporary = ROOT / f'code/appImmUnity/.authoring-failure-{uuid.uuid4().hex}.vcxproj'
    tree.write(temporary, encoding='utf-8', xml_declaration=True)
    try:
        with (out / 'build.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(args.msbuild), str(temporary), '/t:Build',
                '/p:Configuration=Release', '/p:Platform=x64', '/p:TargetName=AuthoringFailureTest',
                f'/p:SolutionDir={ROOT / "code/projects/windows"}{os.sep}',
                f'/p:OutDir={out}{os.sep}', f'/p:IntDir={out / "obj"}{os.sep}',
                '/p:PostBuildEventUseInBuild=false', '/v:minimal', '/nologo'],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        require(result.returncode == 0, f'build (see {out / "build.log"})')
        subprocess.run([str(out / 'AuthoringFailureTest.exe'), str(out / 'fixture.imm')],
            cwd=ROOT, check=True, timeout=30)
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    main()


