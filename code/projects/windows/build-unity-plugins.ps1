param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

function Resolve-MsBuildPath {
    function Test-CppToolchainForMsBuild([string]$msbuildPath) {
        if (-not $msbuildPath -or -not (Test-Path $msbuildPath)) {
            return $false
        }

        $msbuildDir = Split-Path -Parent $msbuildPath
        $msbuildRoot = Resolve-Path (Join-Path $msbuildDir "..\..")
        $cppProps = Join-Path $msbuildRoot "Microsoft\VC\v170\Microsoft.Cpp.Default.props"
        return Test-Path $cppProps
    }

    $preferredPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($preferred in $preferredPaths) {
        if (Test-CppToolchainForMsBuild $preferred) {
            return $preferred
        }
    }

    if ($env:MSBUILD_EXE_PATH -and (Test-Path $env:MSBUILD_EXE_PATH)) {
        if (Test-CppToolchainForMsBuild $env:MSBUILD_EXE_PATH) {
            return $env:MSBUILD_EXE_PATH
        }

        throw "MSBUILD_EXE_PATH is set but missing C++ build props: $env:MSBUILD_EXE_PATH"
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $candidates = & $vswhere -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"
        foreach ($candidate in $candidates) {
            if (Test-CppToolchainForMsBuild $candidate) {
                return $candidate
            }
        }
    }

    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd -and (Test-CppToolchainForMsBuild $cmd.Source)) {
        return $cmd.Source
    }

    throw "MSBuild.exe not found. Install Visual Studio Build Tools (Desktop development with C++) or set MSBUILD_EXE_PATH."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$solution = Join-Path $scriptDir "imm.sln"

$msbuild = Resolve-MsBuildPath
Write-Host "Using MSBuild: $msbuild"

& $msbuild $solution "/t:appImmUnity:Rebuild;appImmStrokeReader:Rebuild" "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PostBuildEventUseInBuild=false" "/m"
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

$unityPkgWin = Join-Path $repoRoot "code\ImmUnitySampleProject\Packages\com.immersive-foundation.imm-unity\Plugins\x86_64"
$strokePkgWin = Join-Path $repoRoot "code\ImmUnitySampleProject\Packages\com.immersive-foundation.imm-stroke-reader\Plugins\x86_64"

New-Item -ItemType Directory -Path $unityPkgWin -Force | Out-Null
New-Item -ItemType Directory -Path $strokePkgWin -Force | Out-Null

$unityPluginCandidates = @(
    (Join-Path $repoRoot "code\appImmUnity\exe\ImmUnityPlugin.dll"),
    (Join-Path $repoRoot "code\appImmUnity\exe\$Configuration\ImmUnityPlugin.dll")
)

$unityPluginSource = $unityPluginCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $unityPluginSource) {
    throw "ImmUnityPlugin.dll was not found after build. Checked: $($unityPluginCandidates -join ', ')"
}

$strokePluginSource = Join-Path $repoRoot "code\appImmStrokeReader\exe\ImmStrokeReader.dll"
if (-not (Test-Path $strokePluginSource)) {
    throw "ImmStrokeReader.dll was not found after build at $strokePluginSource"
}

$unityDllMap = @{
    "ImmUnityPlugin.dll" = $unityPluginSource
    "Audio360.dll"       = (Join-Path $repoRoot "thirdparty\audio360-sdk\Audio360\Windows\x64\Audio360.dll")
    "jpeg62.dll"         = (Join-Path $repoRoot "thirdparty\libjpeg-turbo\bin\jpeg62.dll")
    "libpng16.dll"       = (Join-Path $repoRoot "thirdparty\libpng\bin\libpng16.dll")
    "ogg.dll"            = (Join-Path $repoRoot "thirdparty\libogg\bin\ogg.dll")
    "opus.dll"           = (Join-Path $repoRoot "thirdparty\opus\bin\opus.dll")
    "opusenc.dll"        = (Join-Path $repoRoot "thirdparty\libopusenc\bin\opusenc.dll")
    "vorbis.dll"         = (Join-Path $repoRoot "thirdparty\libvorbis\bin\vorbis.dll")
    "vorbisenc.dll"      = (Join-Path $repoRoot "thirdparty\libvorbis\bin\vorbisenc.dll")
    "zlib1.dll"          = (Join-Path $repoRoot "thirdparty\zlib\bin\zlib1.dll")
}

foreach ($name in $unityDllMap.Keys) {
    $src = $unityDllMap[$name]
    if (-not (Test-Path $src)) {
        throw "Required DLL source is missing: $src"
    }

    Copy-Item $src (Join-Path $unityPkgWin $name) -Force
}

Copy-Item $strokePluginSource (Join-Path $strokePkgWin "ImmStrokeReader.dll") -Force

Write-Host "Updated Unity sample packages:"
Write-Host "  $unityPkgWin"
Write-Host "  $strokePkgWin"
