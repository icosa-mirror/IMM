param(
    [Alias("?")]
    [switch]$Help,

    [string]$Adb = $env:ADB,

    [int]$WaitSeconds = 20,

    [string]$LogDir = "logs/android-gles-smoke",

    [string]$BuildDir = "build_gles_fallback",

    [switch]$UseIntentRendererExtra,

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Usage: ./run-android-gles-smoke.ps1 [-Adb <adb>] [-WaitSeconds <seconds>] [-LogDir <path>] [-BuildDir <dir>] [-UseIntentRendererExtra] [-SkipBuild]"
    return
}

& (Join-Path $PSScriptRoot "run-android-renderer-smoke.ps1") `
    -RendererApi GLES `
    -Adb $Adb `
    -WaitSeconds $WaitSeconds `
    -LogDir $LogDir `
    -BuildDir $BuildDir `
    -UseIntentRendererExtra:$UseIntentRendererExtra `
    -SkipBuild:$SkipBuild
