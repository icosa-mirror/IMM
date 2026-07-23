param(
    [Alias("?")]
    [switch]$Help,

    [string]$Adb = $env:ADB,

    [int]$WaitSeconds = 20,

    [string]$LogDir = "logs/android-vulkan-smoke",

    [string]$BuildDir = "build_vulkan",

    [switch]$UseIntentRendererExtra,

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Usage: ./run-android-vulkan-smoke.ps1 [-Adb <adb>] [-WaitSeconds <seconds>] [-LogDir <path>] [-BuildDir <dir>] [-UseIntentRendererExtra] [-SkipBuild]"
    return
}

& (Join-Path $PSScriptRoot "run-android-renderer-smoke.ps1") `
    -RendererApi Vulkan `
    -Adb $Adb `
    -WaitSeconds $WaitSeconds `
    -LogDir $LogDir `
    -BuildDir $BuildDir `
    -UseIntentRendererExtra:$UseIntentRendererExtra `
    -SkipBuild:$SkipBuild
