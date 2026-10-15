"""Windows regression: actual managed LoadFromFile P/Invoke -> UTF-8 decoder -> importer.

Uses a headless test DLL, not a graphics host. Native production targets should be built first.
Private fixtures are supplied at invocation, copied under ignored artifacts, and never versioned.
"""
import argparse
import hashlib
import os
import re
from pathlib import Path
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from exporter_bridge_smoke import runtime_dependencies

ROOT = Path(__file__).resolve().parents[3]


def run_logged(command: list[str], log: Path) -> None:
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}); see {log}')


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--msbuild', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    args = parser.parse_args()
    out = ROOT / 'artifacts/unicode-path-smoke'
    out.mkdir(parents=True, exist_ok=True)
    paths = [out / 'tortoise-and-rabbit.imm',
        out / '\u4e2d\u6587\u76ee\u5f55' / '\u4e2d\u534e\u5178\u6545\u300a\u9f9f\u5154\u8d5b\u8dd1\u300bTortoise and rabbit race.imm',
        out / '\u9f9f\u5154\U0001f422 100% complete.imm']
    expected = hashlib.sha256(args.fixture.read_bytes()).digest()
    for path in paths:
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.fixture, path)
        assert hashlib.sha256(path.read_bytes()).digest() == expected
    for dependency in runtime_dependencies(ROOT):
        shutil.copy2(dependency, out / dependency.name)

    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    ET.register_namespace('', ns['m'])
    tree = ET.parse(ROOT / 'code/appImmUnity/appImmUnity.vcxproj')
    project = tree.getroot()
    for group in project.findall('m:ItemGroup', ns):
        for node in list(group):
            if node.tag == f"{{{ns['m']}}}ClCompile": group.remove(node)
    items = ET.SubElement(project, f"{{{ns['m']}}}ItemGroup")
    ET.SubElement(items, f"{{{ns['m']}}}ClCompile", Include='tests/unicode_path_test.cpp')
    temporary = ROOT / f'code/appImmUnity/.unicode-path-{uuid.uuid4().hex}.vcxproj'
    tree.write(temporary, encoding='utf-8', xml_declaration=True)
    try:
        run_logged([str(args.msbuild), str(temporary), '/t:Build',
            '/p:Configuration=Release', '/p:Platform=x64', '/p:TargetName=ImmUnityPlugin',
            f'/p:SolutionDir={ROOT / "code/projects/windows"}{os.sep}',
            f'/p:OutDir={out}{os.sep}', f'/p:IntDir={out / "native-obj"}{os.sep}',
            '/p:PostBuildEventUseInBuild=false', '/v:quiet', '/nologo'], out / 'native-build.log')
    finally:
        temporary.unlink(missing_ok=True)
    # Use a modern .NET host for the production UTF-8 marshaling declaration.
    managed = out / 'test-managed'
    managed.mkdir(exist_ok=True)
    csproj = managed / 'UnicodePathSmoke.csproj'
    # Extract the production declaration verbatim so CI tests its marshaling
    # without needing a Unity Editor or ignored generated Unity project files.
    native_source = (ROOT / 'code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs').read_text(encoding='utf-8')
    declaration = re.search(r'\[DllImport\(DllName\)\]\s*public static extern int LoadFromFile\([^;]+;', native_source)
    if declaration is None:
        raise RuntimeError('Production LoadFromFile P/Invoke declaration not found')
    isolated = managed / 'NativeLoadFromFile.cs'
    isolated.write_text('using System.Runtime.InteropServices;\nnamespace ImmPlayer { public static class ImmNativePlugin {\n'
        'private const string DllName = "ImmUnityPlugin";\n' + declaration.group(0) + '\n} }\n', encoding='utf-8')
    csproj.write_text(f"""<Project Sdk="Microsoft.NET.Sdk">
      <PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework>
        <EnableDefaultCompileItems>false</EnableDefaultCompileItems></PropertyGroup>
      <ItemGroup><Compile Include="{ROOT / 'code/appImmUnity/tests/UnicodePathSmoke.cs'}" />
        <Compile Include="{isolated}" />
      </ItemGroup></Project>""", encoding='utf-8')
    run_logged(['dotnet', 'build', str(csproj), '--ignore-failed-sources', '--output', str(managed),
        '--verbosity', 'quiet'], out / 'test-build.log')
    shutil.copy2(out / 'ImmUnityPlugin.dll', managed / 'ImmUnityPlugin.dll')
    for dependency in runtime_dependencies(ROOT):
        shutil.copy2(dependency, managed / dependency.name)
    subprocess.run(['dotnet', str(managed / 'UnicodePathSmoke.dll'), *(str(path) for path in paths),
        str(out / '\u4e0d\u5b58\u5728\u7684\u9f9f\u5154.imm'), str(out / 'native.log')],
        cwd=ROOT, check=True, timeout=120)
    print('[IMM_UNICODE_PATH_TEST] all fixture SHA-256 hashes match')


if __name__ == '__main__':
    main()
