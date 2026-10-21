"""Build/run isolated CPU native evaluated-state comparison; no GPU or Editor.

Native Release libraries must already be built. Logs remain under artifacts.
"""
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from exporter_bridge_smoke import runtime_dependencies

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--msbuild', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--ticks', default='0,1,262,524,525,526,787,1050,1312,1574,1575,1576,1837,2362,3149,3150,3151,6299,6300,6301,12599,12600,12601')
    parser.add_argument('--scope', choices=['paint-groups-root'], required=True)
    parser.add_argument('--skip-build', action='store_true', help='Reuse the existing isolated executable explicitly.')
    parser.add_argument('--expect-mismatch', action='store_true', help='Require a deliberate pair difference with zero history errors.')
    args = parser.parse_args()
    for path in (args.reference, args.candidate):
        if not path.is_file():
            parser.error(f'Input does not exist: {path}')
    out = ROOT / 'artifacts/evaluated-state-compare'
    out.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        for dependency in runtime_dependencies(ROOT):
            shutil.copy2(dependency, out / dependency.name)
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
                if node.tag == f"{{{ns['m']}}}ClCompile":
                    group.remove(node)
        items = ET.SubElement(project, f"{{{ns['m']}}}ItemGroup")
        ET.SubElement(items, f"{{{ns['m']}}}ClCompile", Include='tests/evaluated_state_compare.cpp')
        temporary = ROOT / f'code/appImmUnity/.evaluated-state-{uuid.uuid4().hex}.vcxproj'
        tree.write(temporary, encoding='utf-8', xml_declaration=True)
        try:
            with (out / 'build.log').open('w', encoding='utf-8') as log:
                result = subprocess.run([str(args.msbuild), str(temporary), '/t:Build',
                    '/p:Configuration=Release', '/p:Platform=x64', '/p:TargetName=EvaluatedStateCompare',
                    f'/p:SolutionDir={ROOT / "code/projects/windows"}{os.sep}',
                    f'/p:OutDir={out}{os.sep}', f'/p:IntDir={out / "obj"}{os.sep}',
                    '/p:PostBuildEventUseInBuild=false', '/p:TrackFileAccess=false', '/nr:false', '/v:minimal', '/nologo'],
                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                raise SystemExit(f'Native comparator build failed; inspect {out / "build.log"}')
        finally:
            temporary.unlink(missing_ok=True)
    result = subprocess.run([str(out / 'EvaluatedStateCompare.exe'), str(args.reference.resolve()),
        str(args.candidate.resolve()), args.ticks, args.scope], cwd=ROOT, timeout=120,
        capture_output=args.expect_mismatch, text=args.expect_mismatch)
    if args.expect_mismatch:
        print(result.stdout, end='')
        print(result.stderr, end='')
        counts = re.search(r'pair-errors=(\d+) history-errors=(\d+)', result.stdout)
        if result.returncode != 1 or not counts or int(counts[1]) == 0 or int(counts[2]) != 0:
            raise SystemExit('Deliberate mismatch did not produce a clean pair-only rejection.')
        print('[IMM_EVALUATED_COMPARE] deliberate pair mismatch correctly rejected')
        return
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
