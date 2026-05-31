param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$GodotCppPath = $env:GODOT_CPP_PATH,

    [string]$GodotCppLib = $env:GODOT_CPP_LIB,

    [string]$GodotCppRef = $env:GODOT_CPP_REF,

    [string]$AndroidSdkRoot = $env:ANDROID_SDK_ROOT,

    [string]$AndroidNdkRoot = $env:ANDROID_NDK_ROOT,

    [switch]$BootstrapGodotCpp,

    [switch]$BuildGodotCpp,

    [switch]$PreflightOnly
)

$ErrorActionPreference = "Stop"

if (-not $GodotCppRef) {
    $GodotCppRef = "godot-4.5-stable"
}

function Resolve-Git {
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        return $git.Source
    }
    throw "git not found. Install Git or pass an existing -GodotCppPath."
}

function Resolve-Python {
    foreach ($name in @("python", "python3")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }
    throw "Python not found. Install Python or make python/python3 available on PATH."
}

function Resolve-SCons([string]$python) {
    $cmd = Get-Command scons -ErrorAction SilentlyContinue
    if ($cmd) {
        return @($cmd.Source)
    }
    return @($python, "-m", "SCons")
}

function Resolve-AndroidSdk([string]$requested) {
    if ($requested -and (Test-Path $requested)) {
        return (Resolve-Path $requested).Path
    }
    if ($env:ANDROID_HOME -and (Test-Path $env:ANDROID_HOME)) {
        return (Resolve-Path $env:ANDROID_HOME).Path
    }
    $local = Join-Path $env:LOCALAPPDATA "Android\Sdk"
    if ($local -and (Test-Path $local)) {
        return (Resolve-Path $local).Path
    }
    throw "Android SDK not found. Set ANDROID_SDK_ROOT or pass -AndroidSdkRoot."
}

function Resolve-AndroidNdk([string]$requested, [string]$sdkRoot) {
    if ($requested -and (Test-Path $requested)) {
        return (Resolve-Path $requested).Path
    }
    foreach ($version in @("28.1.13356709", "26.1.10909125")) {
        $preferred = Join-Path $sdkRoot "ndk\$version"
        if (Test-Path $preferred) {
            return (Resolve-Path $preferred).Path
        }
    }
    $ndkRoot = Join-Path $sdkRoot "ndk"
    if (Test-Path $ndkRoot) {
        $latest = Get-ChildItem -Path $ndkRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($latest) {
            return $latest.FullName
        }
    }
    throw "Android NDK not found. Install ndk;26.1.10909125 or pass -AndroidNdkRoot."
}

function Resolve-GodotCppPath([string]$requested, [string]$repoRoot, [bool]$bootstrap, [string]$ref, [bool]$preflight) {
    if ($requested -and (Test-Path $requested)) {
        return (Resolve-Path $requested).Path
    }

    $defaultPath = Join-Path $repoRoot "thirdparty\godot-cpp"
    if (Test-Path $defaultPath) {
        return (Resolve-Path $defaultPath).Path
    }

    if ($bootstrap -and $preflight) {
        Write-Warning "godot-cpp checkout not found at $defaultPath. A full run with -BootstrapGodotCpp will clone ref $ref."
        return $defaultPath
    }

    if (-not $bootstrap) {
        throw "godot-cpp checkout not found. Pass -GodotCppPath, set GODOT_CPP_PATH, place it at thirdparty\godot-cpp, or pass -BootstrapGodotCpp."
    }

    $git = Resolve-Git
    $targetParent = Split-Path -Parent $defaultPath
    if ($targetParent -and -not (Test-Path $targetParent)) {
        New-Item -ItemType Directory -Force $targetParent | Out-Null
    }
    Write-Host "Cloning godot-cpp $ref to $defaultPath"
    & $git clone --depth 1 --branch $ref https://github.com/godotengine/godot-cpp.git $defaultPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to clone godot-cpp ref $ref"
    }
    return (Resolve-Path $defaultPath).Path
}

function Find-GodotCppAndroidLib([string]$godotCpp, [string]$target) {
    $shortTarget = if ($target.EndsWith("debug")) { "debug" } else { "release" }
    $candidates = @(
        (Join-Path $godotCpp "bin\libgodot-cpp.android.$target.arm64.a"),
        (Join-Path $godotCpp "bin\libgodot-cpp.android.$target.dev.arm64.a"),
        (Join-Path $godotCpp "bin\libgodot-cpp.android.$shortTarget.arm64.a")
    )
    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Invoke-Tool([string[]]$command, [string[]]$arguments, [string]$workingDirectory) {
    Push-Location $workingDirectory
    try {
        if (-not $env:PROCESSOR_ARCHITECTURE) {
            $env:PROCESSOR_ARCHITECTURE = "AMD64"
        }
        $toolArgs = @()
        if ($command.Length -gt 1) {
            $toolArgs += $command[1..($command.Length - 1)]
        }
        $toolArgs += $arguments
        & $command[0] @toolArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code ${LASTEXITCODE}: $($command -join ' ') $($arguments -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$python = Resolve-Python
$scons = Resolve-SCons $python
$sdkRoot = Resolve-AndroidSdk $AndroidSdkRoot
$ndkRoot = Resolve-AndroidNdk $AndroidNdkRoot $sdkRoot
$ndkVersion = Split-Path -Leaf $ndkRoot
$godotCpp = Resolve-GodotCppPath $GodotCppPath $repoRoot $BootstrapGodotCpp.IsPresent $GodotCppRef $PreflightOnly.IsPresent
$target = if ($Configuration -eq "Debug") { "template_debug" } else { "template_release" }
$variant = if ($Configuration -eq "Debug") { "debug" } else { "release" }

Write-Host "Using Android SDK: $sdkRoot"
Write-Host "Using Android NDK: $ndkRoot"
Write-Host "Using godot-cpp: $godotCpp"
Write-Host "Using godot-cpp ref: $GodotCppRef"
Write-Host "Using SCons: $($scons -join ' ')"

if (-not $GodotCppLib) {
    $GodotCppLib = Find-GodotCppAndroidLib $godotCpp $target
}

$generatedHeader = Join-Path $godotCpp "gen\include\godot_cpp\classes\camera3d.hpp"
if (-not $GodotCppLib -or -not (Test-Path $generatedHeader)) {
    if (-not $BuildGodotCpp) {
        Write-Warning "godot-cpp Android library or generated bindings are missing. Pass -BuildGodotCpp or provide -GodotCppLib."
    }
    elseif (-not $PreflightOnly) {
        $previousAndroidNdkRoot = $env:ANDROID_NDK_ROOT
        $previousAndroidNdkHome = $env:ANDROID_NDK_HOME
        $previousAndroidSdkRoot = $env:ANDROID_SDK_ROOT
        $previousAndroidHome = $env:ANDROID_HOME
        $env:ANDROID_NDK_ROOT = $ndkRoot
        $env:ANDROID_NDK_HOME = $ndkRoot
        $env:ANDROID_SDK_ROOT = $sdkRoot
        $env:ANDROID_HOME = $sdkRoot
        try {
            Invoke-Tool $scons @("platform=android", "target=$target", "arch=arm64", "generate_bindings=yes", "android_api_level=26", "ndk_version=$ndkVersion") $godotCpp
        }
        finally {
            $env:ANDROID_NDK_ROOT = $previousAndroidNdkRoot
            $env:ANDROID_NDK_HOME = $previousAndroidNdkHome
            $env:ANDROID_SDK_ROOT = $previousAndroidSdkRoot
            $env:ANDROID_HOME = $previousAndroidHome
        }
        $GodotCppLib = Find-GodotCppAndroidLib $godotCpp $target
    }
}

if ($PreflightOnly) {
    Write-Host "Android Godot extension preflight passed."
    return
}

if (-not $GodotCppLib -or -not (Test-Path $GodotCppLib)) {
    throw "godot-cpp Android library not found. Build godot-cpp for platform=android arch=arm64 or pass -GodotCppLib."
}
if (-not (Test-Path $generatedHeader)) {
    throw "godot-cpp generated bindings not found: $generatedHeader"
}

$gradlew = if ($IsWindows -or $PSVersionTable.PSEdition -eq "Desktop") {
    Join-Path $scriptDir "gradlew.bat"
}
else {
    Join-Path $scriptDir "gradlew"
}
if (-not (Test-Path $gradlew)) {
    $gradlew = Join-Path $scriptDir "gradlew"
}
if (-not (Test-Path $gradlew)) {
    throw "Gradle wrapper not found under $scriptDir"
}

$previousAndroidSdkRoot = $env:ANDROID_SDK_ROOT
$previousAndroidHome = $env:ANDROID_HOME
$env:ANDROID_SDK_ROOT = $sdkRoot
$env:ANDROID_HOME = $sdkRoot
try {
    Push-Location $scriptDir
    try {
        & $gradlew :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug :appImmGodot:assembleDebug `
            "-PimmBuildDir=build_godot" `
            "-PimmGodotVariant=$variant" `
            "-PimmGodotCppPath=$godotCpp" `
            "-PimmGodotCppLib=$GodotCppLib"
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle Android Godot extension build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    $env:ANDROID_SDK_ROOT = $previousAndroidSdkRoot
    $env:ANDROID_HOME = $previousAndroidHome
}

$outputDir = Join-Path $repoRoot "code\ImmGodotSampleProject\addons\imm_viewer\bin\android\$variant"
$requiredOutputs = @(
    "libimm_godot_extension.arm64.so",
    "libImmGodotPlugin.so"
)
$missingOutputs = @()
foreach ($name in $requiredOutputs) {
    $candidate = Join-Path $outputDir $name
    if (-not (Test-Path $candidate)) {
        $missingOutputs += $candidate
    }
}
if ($missingOutputs.Count -gt 0) {
    throw "Android Godot staged output libraries are missing:`n  $($missingOutputs -join "`n  ")"
}

$manifest = @(
    "Configuration=$Configuration",
    "OutputDir=$outputDir",
    "ExpectedLibraryCount=$($requiredOutputs.Count)",
    "GodotCppLib=$GodotCppLib",
    "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString("o"))",
    "Libraries:"
)
foreach ($name in $requiredOutputs) {
    $candidate = Join-Path $outputDir $name
    $item = Get-Item $candidate
    $manifest += ("FOUND`t{0}`t{1}`t{2:o}" -f $item.Name, $item.Length, $item.LastWriteTimeUtc)
}
$manifest | Out-File -FilePath (Join-Path $outputDir "godot-extension-android-libs.txt") -Encoding utf8

Write-Host "Updated Android Godot sample binaries:"
Write-Host "  $outputDir"
Write-Host "Verified staged Android Godot library set: $($requiredOutputs.Count) files"
