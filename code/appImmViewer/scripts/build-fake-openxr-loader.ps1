param(
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$source = Join-Path $scriptDir "fake_openxr_loader.cpp"
if (-not $OutputDir) {
    $OutputDir = Join-Path $repoRoot "build\openxr-fake-loader"
}
$outputFullDir = (New-Item -ItemType Directory -Force $OutputDir).FullName
$dllPath = Join-Path $outputFullDir "openxr_loader.dll"

function Find-VsDevCmd {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
        if ($installPath) {
            $candidate = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $known = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    if (Test-Path $known) {
        return $known
    }

    return ""
}

$vsDevCmd = Find-VsDevCmd
if (-not $vsDevCmd) {
    throw "VsDevCmd.bat was not found. Install Visual Studio C++ tools."
}

$cmd = "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /EHsc /LD `"$source`" /Fe:`"$dllPath`" /Fo:`"$outputFullDir\fake_openxr_loader.obj`" /link /NOLOGO"
cmd.exe /d /s /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "Fake OpenXR loader build failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $dllPath)) {
    throw "Fake OpenXR loader was not produced: $dllPath"
}

Write-Host "IMM_FAKE_OPENXR_LOADER dll=$dllPath"
