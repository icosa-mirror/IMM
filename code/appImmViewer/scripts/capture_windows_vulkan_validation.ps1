param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$OutputPath = "",
    [string]$SamplePath = "",
    [int]$PlayerFrame = 60,
    [int]$MinPictureDrawCalls = 1,
    [int]$MinPicture360DrawCalls = 1,
    [int]$MinPicture360EquirectDrawCalls = 1,
    [int]$TimeoutSeconds = 90,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")

if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot "build\baseline-captures\windows-vulkan-validation.ppm"
}

$baselineArgs = @{
    Configuration = $Configuration
    Platform = $Platform
    SettingsPath = Join-Path $repoRoot "code\appImmViewer\exe\settings-vulkan-smoke.json"
    OutputPath = $OutputPath
    PlayerFrame = $PlayerFrame
    TimeoutSeconds = $TimeoutSeconds
    MinPictureDrawCalls = $MinPictureDrawCalls
    MinPicture360DrawCalls = $MinPicture360DrawCalls
    MinPicture360EquirectDrawCalls = $MinPicture360EquirectDrawCalls
}
if ($SamplePath) {
    $baselineArgs.SamplePath = $SamplePath
}
if ($SkipBuild) {
    $baselineArgs.SkipBuild = $true
}

& (Join-Path $scriptDir "capture_windows_directx_baseline.ps1") @baselineArgs
