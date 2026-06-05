param(
    [string]$OpenXrSdkPath = "",
    [string]$OpenXrSdkRef = "release-1.1.60",
    [switch]$Force,
    [switch]$BuildLoader
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $scriptDir = Split-Path -Parent $MyInvocation.ScriptName
    return Resolve-Path (Join-Path $scriptDir "..\..\..")
}

function Resolve-MSBuild {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "MSBuild was not found. Install Visual Studio Build Tools or pass a shell with msbuild on PATH."
}

$repoRoot = Resolve-RepoRoot
if ([string]::IsNullOrWhiteSpace($OpenXrSdkPath)) {
    $OpenXrSdkPath = Join-Path $repoRoot "thirdparty\openxr-sdk"
}

$git = Get-Command git -ErrorAction Stop
$cmake = Get-Command cmake -ErrorAction Stop
$openXrSdkFullPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OpenXrSdkPath)
$parent = Split-Path -Parent $openXrSdkFullPath
if (-not (Test-Path $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}

if (Test-Path $openXrSdkFullPath) {
    $hasHeader = Test-Path (Join-Path $openXrSdkFullPath "include\openxr\openxr.h")
    if (-not $hasHeader) {
        if (-not $Force) {
            throw "Existing OpenXR SDK path does not contain include\openxr\openxr.h: $openXrSdkFullPath. Pass -Force to replace it."
        }
        Remove-Item -LiteralPath $openXrSdkFullPath -Recurse -Force
    }
} else {
    Write-Host "IMM_OPENXR_BOOTSTRAP clone=$OpenXrSdkRef target=$openXrSdkFullPath"
    & $git clone --depth 1 --branch $OpenXrSdkRef https://github.com/KhronosGroup/OpenXR-SDK.git $openXrSdkFullPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to clone OpenXR-SDK ref $OpenXrSdkRef"
    }
}

if (-not (Test-Path (Join-Path $openXrSdkFullPath "include\openxr\openxr.h"))) {
    throw "OpenXR header not found after bootstrap: $openXrSdkFullPath"
}

if (-not $BuildLoader) {
    Write-Host "IMM_OPENXR_BOOTSTRAP header=$(Join-Path $openXrSdkFullPath 'include\openxr\openxr.h')"
    Write-Host "IMM_OPENXR_BOOTSTRAP skippedLoaderBuild=1"
    Write-Host "IMM_OPENXR_BOOTSTRAP note=dynamic loader path does not require openxr_loader.lib"
    exit 0
}

$buildDir = Join-Path $openXrSdkFullPath "build\windows-x64"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Write-Host "IMM_OPENXR_BOOTSTRAP configure=$buildDir"
& $cmake -S $openXrSdkFullPath -B $buildDir -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_API_LAYERS=OFF -DBUILD_CONFORMANCE_TESTS=OFF -DBUILD_ALL_EXTENSIONS=ON
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR SDK CMake configure failed"
}

$msbuild = Resolve-MSBuild
Write-Host "IMM_OPENXR_BOOTSTRAP buildTarget=openxr_loader"
& $cmake --build $buildDir --config Release --target openxr_loader -- /m
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR loader build failed"
}

$loaderLib = Get-ChildItem -Path $buildDir -Recurse -Filter "openxr_loader.lib" -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $loaderLib) {
    throw "OpenXR loader import library was not produced under $buildDir"
}

Write-Host "IMM_OPENXR_BOOTSTRAP header=$(Join-Path $openXrSdkFullPath 'include\openxr\openxr.h')"
Write-Host "IMM_OPENXR_BOOTSTRAP loaderLib=$($loaderLib.FullName)"
