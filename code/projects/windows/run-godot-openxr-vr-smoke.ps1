param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$GodotExe = $env:GODOT_EXE,

    [string]$LogDir = "artifacts/godot-openxr-vr",

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Resolve-GodotExe([string]$Requested) {
    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    foreach ($name in @("godot.exe", "godot")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }

    throw "Godot executable not found. Pass -GodotExe, set GODOT_EXE, or add Godot to PATH."
}

function Require-Marker([string]$Log, [string]$Marker) {
    if (-not $Log.Contains($Marker)) {
        throw "Godot OpenXR VR smoke log did not contain required marker: $Marker"
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$sampleProject = Join-Path $repoRoot "code\ImmGodotSampleProject"
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$godot = Resolve-GodotExe $GodotExe
$variant = if ($Configuration -eq "Debug") { "debug" } else { "release" }
$extensionDir = Join-Path $sampleProject "addons\imm_viewer\bin\windows\$variant"
$requiredFiles = @(
    "imm_godot_extension.dll",
    "ImmGodotPlugin.dll",
    "Audio360.dll",
    "opus.dll",
    "opusenc.dll",
    "vorbisenc.dll",
    "zlib1.dll",
    "jpeg62.dll",
    "libpng16.dll",
    "ogg.dll",
    "vorbis.dll"
)

if (-not $SkipBuild) {
    & (Join-Path $scriptDir "build-godot-extension.ps1") -Configuration $Configuration -BootstrapGodotCpp -BuildGodotCpp
    if ($LASTEXITCODE -ne 0) {
        throw "Godot extension build failed with exit code $LASTEXITCODE"
    }
}

$missingFiles = @()
$inventory = @("Expected staged extension files:")
foreach ($file in $requiredFiles) {
    $candidate = Join-Path $extensionDir $file
    if (Test-Path $candidate) {
        $item = Get-Item $candidate
        $inventory += ("FOUND`t{0}`t{1}`t{2:o}" -f $item.Name, $item.Length, $item.LastWriteTimeUtc)
    }
    else {
        $missingFiles += $candidate
        $inventory += ("MISSING`t{0}" -f $file)
    }
}
$inventory | Out-File -FilePath (Join-Path $logDirectory "godot-openxr-extension-files.txt") -Encoding utf8
if ($missingFiles.Count -gt 0) {
    throw "Godot OpenXR VR smoke runtime files are missing:`n  $($missingFiles -join "`n  ")"
}

$previousPath = $env:PATH
$env:PATH = "$extensionDir;$env:PATH"
$env:IMM_GODOT_OPENXR_SMOKE = "1"
$outputPath = Join-Path $logDirectory "godot-openxr-vr-output.log"
$summaryPath = Join-Path $logDirectory "godot-openxr-vr-summary.txt"
try {
    $godotArgs = @(
        "--path", $sampleProject,
        "--rendering-driver", "vulkan",
        "--rendering-method", "forward_plus",
        "--xr-mode", "on",
        "--scene", "res://scenes/OpenXRSmokeScene.tscn",
        "--fixed-fps", "30"
    )
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $godot @godotArgs 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}
finally {
    $env:PATH = $previousPath
}

$output | ForEach-Object { Write-Host $_ }
$output | Out-File -FilePath $outputPath -Encoding utf8
$log = $output -join "`n"
$successMarker = "IMM Godot OpenXR VR smoke passed"
$hasSuccessMarker = $log.Contains($successMarker)
@(
    "Godot=$godot",
    "SampleProject=$sampleProject",
    "Configuration=$Configuration",
    "ExtensionDir=$extensionDir",
    "SuccessMarker=$successMarker",
    "HasSuccessMarker=$hasSuccessMarker",
    "ExitCode=$exitCode"
) | Out-File -FilePath $summaryPath -Encoding utf8

if ($exitCode -ne 0) {
    throw "Godot OpenXR VR smoke failed with exit code $exitCode. Log: $outputPath"
}

foreach ($marker in @(
    "IMM_GODOT_OPENXR_SMOKE begin",
    "IMM_GODOT_OPENXR_SMOKE interface_initialized",
    "IMM_GODOT_OPENXR_SMOKE viewport_use_xr=1",
    "IMM_GODOT_OPENXR_SMOKE primary_interface=OpenXR",
    "IMM_GODOT_OPENXR_SMOKE viewer_initialized",
    "IMM_GODOT_OPENXR_SMOKE load_document_result=0",
    "IMM_GODOT_OPENXR_SMOKE document_ready",
    "IMM_GODOT_OPENXR_SMOKE frame_submitted",
    "IMM Godot OpenXR VR smoke passed"
)) {
    Require-Marker $log $marker
}

foreach ($marker in @(
    "IMM_GODOT_OPENXR_SMOKE failed",
    "OpenXR interface was not found",
    "OpenXR interface initialize() returned false",
    "ImmViewer did not load",
    "ImmViewer sequence was not ready"
)) {
    if ($log.Contains($marker)) {
        throw "Godot OpenXR VR smoke log contained failure marker: $marker"
    }
}

Write-Host "Godot OpenXR VR smoke passed"
Write-Host "Log: $outputPath"
