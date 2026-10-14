"""Build and run the CPU-only viewpoint policy regression on Windows.

Build appImmUnity/libImmPlayer Release first. No Unity Editor or renderer is started.
"""
from pathlib import Path
import argparse
import ctypes as c
import os
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from exporter_bridge_smoke import Transform, bind, require, runtime_dependencies

ROOT = Path(__file__).resolve().parents[3]


def fixture(plugin: Path, output: Path) -> None:
    handles = [os.add_dll_directory(str(p.parent)) for p in runtime_dependencies(ROOT)]
    c.windll.kernel32.SetDllDirectoryW(str(plugin.parent))
    library = c.CDLL(str(plugin))
    create = bind(library, 'ImmExporter_CreateSequence', c.c_void_p,
        c.c_int, c.c_int, c.c_float, c.c_float, c.c_float, c.c_uint32,
        c.c_int64, c.c_int64, c.c_int64, c.c_int64)
    group = bind(library, 'ImmExporter_CreateGroupLayer', c.c_void_p,
        c.c_void_p, c.c_void_p, c.c_char_p, c.c_int, c.c_float,
        c.POINTER(Transform), c.POINTER(Transform), c.c_int, c.c_int64, c.c_uint32)
    spawn = bind(library, 'ImmExporter_CreateSpawnAreaLayer', c.c_void_p,
        c.c_void_p, c.c_void_p, c.c_char_p, c.POINTER(Transform), c.c_int)
    key = bind(library, 'ImmExporter_LayerAddAnimationKey', c.c_bool,
        c.c_void_p, c.c_int, c.c_int64, c.c_int, c.c_int, c.c_uint32,
        c.c_float, c.c_double, c.POINTER(Transform))
    default = bind(library, 'ImmExporter_SetInitialSpawnArea', c.c_bool, c.c_void_p, c.c_void_p)
    export = bind(library, 'ImmExporter_ExportToFile', c.c_bool, c.c_void_p, c.c_char_p, c.c_int, c.c_int)
    destroy = bind(library, 'ImmExporter_DestroySequence', None, c.c_void_p)
    identity = Transform(0, 0, 0, 0, 0, 0, 1, 1)
    moved = Transform(5, 0, 0, 0, 0, 0, 1, 1)
    seq = require(create(1, 0, 0, 0, 0, 30, 0, 0, 0, 0), 'create fixture')
    try:
        parent = require(group(seq, None, b'chapter-group', 1, 1,
            c.byref(identity), c.byref(identity), 0, 0, 1), 'parent group')
        nested = require(spawn(seq, parent, b'nested', c.byref(identity), 0), 'nested viewpoint')
        visible = require(spawn(seq, None, b'visible', c.byref(identity), 0), 'visible viewpoint')
        hidden = require(spawn(seq, None, b'self-hidden', c.byref(identity), 0), 'self-hidden viewpoint')
        require(default(seq, visible), 'initial viewpoint')
        def add(layer, prop, ticks, boolean=0, integer=0, transform=identity):
            require(key(layer, prop, ticks, 0, boolean, integer, 0, 0, c.byref(transform)), 'fixture key')
        add(parent, 0, 0, 0)
        add(parent, 0, 2 * 12600, 1)
        add(parent, 9, 0, transform=identity)
        add(parent, 9, 2 * 12600, transform=moved)
        add(hidden, 0, 0, 0)
        add(hidden, 0, 12600, 1)
        add(nested, 6, 2 * 12600, integer=3)  # MakeDefault
        require(export(seq, os.fsencode(output), 96000, 0), 'export fixture')
    finally:
        destroy(seq)
        # Keep dependency handles alive until all exporter calls finish.
        for handle in handles:
            handle.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--msbuild', type=Path, required=True)
    args = parser.parse_args()
    out = ROOT / 'artifacts/viewpoint-policy'
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
    ET.SubElement(items, f"{{{ns['m']}}}ClCompile", Include='tests/viewpoint_policy_test.cpp')
    temporary = ROOT / f'code/appImmUnity/.viewpoint-policy-{uuid.uuid4().hex}.vcxproj'
    tree.write(temporary, encoding='utf-8', xml_declaration=True)
    try:
        with (out / 'build.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(args.msbuild), str(temporary), '/t:Build',
                '/p:Configuration=Release', '/p:Platform=x64', '/p:TargetName=ViewpointPolicyTest',
                f'/p:SolutionDir={ROOT / "code/projects/windows"}{os.sep}',
                f'/p:OutDir={out}{os.sep}', f'/p:IntDir={out / "obj"}{os.sep}',
                '/p:PostBuildEventUseInBuild=false', '/v:minimal', '/nologo'],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        require(result.returncode == 0, f'build (see {out / "build.log"})')
        subprocess.run([str(out / 'ViewpointPolicyTest.exe'), str(out / 'fixture.imm')],
            cwd=ROOT, check=True, timeout=30)
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
