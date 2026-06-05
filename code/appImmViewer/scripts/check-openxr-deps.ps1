param(
    [string]$OpenXrSdk = $env:OPENXR_SDK
)

$ErrorActionPreference = "Stop"

function Find-FirstExistingFile {
    param(
        [string[]]$Roots,
        [string]$Filter
    )

    foreach ($root in $Roots) {
        if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path $root)) {
            continue
        }

        $match = Get-ChildItem -Path $root -Recurse -Filter $Filter -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) {
            return $match.FullName
        }
    }

    return $null
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$roots = @(
    $OpenXrSdk,
    $env:VULKAN_SDK,
    (Join-Path $repoRoot "thirdparty"),
    "C:\OpenXR-SDK",
    "C:\Program Files\OpenXR-SDK",
    "C:\Program Files (x86)\OpenXR-SDK"
)

$header = Find-FirstExistingFile -Roots $roots -Filter "openxr.h"
$loaderLib = Find-FirstExistingFile -Roots $roots -Filter "openxr_loader.lib"
$loaderDll = Find-FirstExistingFile -Roots $roots -Filter "openxr_loader.dll"

$runtimeJson = $null
$runtimeKeys = @(
    "HKCU:\SOFTWARE\Khronos\OpenXR\1",
    "HKLM:\SOFTWARE\Khronos\OpenXR\1",
    "HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1"
)

foreach ($key in $runtimeKeys) {
    if (-not (Test-Path $key)) {
        continue
    }

    $props = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
    if ($props -and $props.ActiveRuntime) {
        $runtimeJson = $props.ActiveRuntime
        break
    }
}

if (-not $loaderDll -and $runtimeJson -and (Test-Path $runtimeJson)) {
    $runtimeDir = Split-Path -Parent $runtimeJson
    $runtimeLoader = Join-Path $runtimeDir "bin\win64\openxr_loader.dll"
    if (Test-Path $runtimeLoader) {
        $loaderDll = $runtimeLoader
    }
}

$ok = $true
Write-Host "IMM_OPENXR_DEPS repoRoot=$repoRoot"

if ($header) {
    Write-Host "IMM_OPENXR_DEPS header=$header"
} else {
    Write-Host "IMM_OPENXR_DEPS missing=openxr.h"
    $ok = $false
}

if ($loaderLib) {
    Write-Host "IMM_OPENXR_DEPS loaderLib=$loaderLib"
} else {
    Write-Host "IMM_OPENXR_DEPS missingOptional=openxr_loader.lib"
    Write-Host "IMM_OPENXR_DEPS note=dynamic loader path can proceed without import library"
}

if ($loaderDll) {
    Write-Host "IMM_OPENXR_DEPS loaderDll=$loaderDll"
} else {
    Write-Host "IMM_OPENXR_DEPS missing=openxr_loader.dll"
    $ok = $false
}

if ($runtimeJson) {
    if (Test-Path $runtimeJson) {
        Write-Host "IMM_OPENXR_DEPS activeRuntime=$runtimeJson"
    } else {
        Write-Host "IMM_OPENXR_DEPS activeRuntimeMissingFile=$runtimeJson"
        $ok = $false
    }
} else {
    Write-Host "IMM_OPENXR_DEPS missing=activeRuntimeRegistry"
    $ok = $false
}

if (-not $ok) {
    throw "OpenXR native dependencies are not ready. Install or vendor the OpenXR SDK/loader and configure an active OpenXR runtime."
}

Write-Host "IMM_OPENXR_DEPS ready"
