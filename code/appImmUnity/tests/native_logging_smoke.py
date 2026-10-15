"""Windows headless diagnostic policy regression; never copies into Unity packages."""
import argparse
import ctypes as c
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from exporter_bridge_smoke import runtime_dependencies
ROOT = Path(__file__).resolve().parents[3]

def run(command, log):
    with log.open('w',encoding='utf-8') as output:
        result=subprocess.run(command,cwd=ROOT,stdout=output,stderr=subprocess.STDOUT)
    if result.returncode: raise RuntimeError(f'Command failed; see {log}')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--msbuild',type=Path,required=True)
    parser.add_argument('--reader',type=Path,default=ROOT/'code/appImmStrokeReader/exe/ImmStrokeReader.dll')
    args=parser.parse_args()
    out=ROOT/'artifacts/native-logging-smoke'/uuid.uuid4().hex
    out.mkdir(parents=True)
    for dependency in runtime_dependencies(ROOT): shutil.copy2(dependency,out/dependency.name)
    shutil.copy2(args.reader,out/'ImmStrokeReader.dll')
    ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
    ET.register_namespace('',ns['m'])
    tree=ET.parse(ROOT/'code/appImmUnity/appImmUnity.vcxproj'); project=tree.getroot()
    for node in project.findall('.//m:ConfigurationType',ns): node.text='Application'
    for node in project.findall('.//m:SubSystem',ns): node.text='Console'
    for group in project.findall('m:ItemGroup',ns):
        for node in list(group):
            if node.tag==f"{{{ns['m']}}}ClCompile": group.remove(node)
    items=ET.SubElement(project,f"{{{ns['m']}}}ItemGroup")
    for source in ['tests/native_logging_test.cpp','../appImmShared/src/imm_engine_bridge.cpp']:
        ET.SubElement(items,f"{{{ns['m']}}}ClCompile",Include=source)
    temporary=ROOT/f'code/appImmUnity/.native-logging-{uuid.uuid4().hex}.vcxproj'
    tree.write(temporary,encoding='utf-8',xml_declaration=True)
    try:
        run([str(args.msbuild),str(temporary),'/t:Build','/p:Configuration=Release','/p:Platform=x64',
            '/p:TargetName=NativeLoggingTest',f'/p:SolutionDir={ROOT/"code/projects/windows"}{os.sep}',
            f'/p:OutDir={out}{os.sep}',f'/p:IntDir={out/"obj"}{os.sep}',
            '/p:PostBuildEventUseInBuild=false','/v:quiet','/nologo'],out/'native-build.log')
    finally: temporary.unlink(missing_ok=True)
    subprocess.run([str(out/'NativeLoggingTest.exe'),str(out/'\u4e2d\u6587-explicit.log'),
        str(out/'missing-parent/failure.log')],cwd=out,check=True,timeout=30)
    os.chdir(out)
    c.windll.kernel32.SetDllDirectoryW(str(out))
    dll=c.CDLL(str(out/'ImmStrokeReader.dll'))
    dll.StrokeReader_Init.argtypes=[c.c_char_p];dll.StrokeReader_Init.restype=c.c_int
    dll.StrokeReader_LoadFromFile.argtypes=[c.c_char_p];dll.StrokeReader_LoadFromFile.restype=c.c_int
    dll.StrokeReader_GetLogMessage.argtypes=[c.c_void_p,c.c_int,c.POINTER(c.c_int)]
    dll.StrokeReader_GetLogMessage.restype=c.c_int
    dll.StrokeReader_End.argtypes=[];dll.StrokeReader_End.restype=None
    buffer=c.create_string_buffer(32769);severity=c.c_int()
    assert dll.StrokeReader_Init(None)==0
    assert not (out/'imm_stroke_reader_log.txt').exists()
    assert dll.StrokeReader_LoadFromFile(str(out/'missing.imm').encode('utf-8'))<0
    warnings=[]
    while dll.StrokeReader_GetLogMessage(buffer,len(buffer),c.byref(severity))>0: warnings.append(buffer.value.decode('utf-8'))
    assert any('[IMM_FILE_OPEN]' in text for text in warnings)
    dll.StrokeReader_End()
    assert dll.StrokeReader_Init(str(out/'missing-parent/failure.log').encode('utf-8'))==0
    assert dll.StrokeReader_GetLogMessage(buffer,len(buffer),c.byref(severity))>0
    assert severity.value==2 and b'continuing without it' in buffer.value
    dll.StrokeReader_End()
    print('[IMM_NATIVE_LOG_TEST] real stroke-reader default-off and nonfatal file failure passed')
    source=(ROOT/'code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs').read_text(encoding='utf-8')
    start=source.index('    public static class ImmNativeLogging')
    end=source.index('    /// <summary>\n    /// Native P/Invoke declarations',start)
    policy=out/'Policy.cs'
    policy.write_text(f'using System;\nusing System.IO;\nusing System.Collections.Concurrent;\nusing System.Threading;\nusing System.Text;\nusing UnityEngine;\nnamespace ImmPlayer {{\n{source[start:end]}\n}}\n',encoding='utf-8')
    csproj=out/'NativeLoggingPolicySmoke.csproj'
    csproj.write_text(f"""<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework>
      <OutputType>Exe</OutputType><DefineConstants>UNITY_EDITOR</DefineConstants>
      <EnableDefaultCompileItems>false</EnableDefaultCompileItems></PropertyGroup><ItemGroup>
      <Compile Include="{policy}"/><Compile Include="{ROOT/'code/appImmUnity/tests/NativeLoggingPolicySmoke.cs'}"/>
      </ItemGroup></Project>""",encoding='utf-8')
    run(['dotnet','build',str(csproj),'--ignore-failed-sources','--output',str(out/'managed'),'--verbosity','quiet'],out/'managed-build.log')
    subprocess.run(['dotnet',str(out/'managed/NativeLoggingPolicySmoke.dll'),str(out/'managed-cache')],cwd=ROOT,check=True,timeout=30)

if __name__=='__main__': main()
