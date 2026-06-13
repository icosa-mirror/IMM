param(
    [string]$Configuration = "Release",
    [string]$LogDir = "artifacts/windows-standalone-opengl-vr",
    [int]$TimeoutSeconds = 45,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Require-Marker([string]$Log, [string]$Marker) {
    if (-not $Log.Contains($Marker)) {
        throw "Windows standalone OpenGL VR smoke missing marker: $Marker"
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$projectDir = Join-Path $repoRoot "code\projects\windows"
$exeDir = Join-Path $repoRoot "code\appImmViewer\exe"
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$debugLog = Join-Path $exeDir "debug.txt"
$capturedLog = Join-Path $logDirectory "debug.txt"

if (-not $SkipBuild) {
    Push-Location $projectDir
    try {
        & msbuild imm.sln /t:appImmViewer /p:Configuration=$Configuration /p:Platform=x64 /m
        if ($LASTEXITCODE -ne 0) {
            throw "Windows viewer build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

$exeName = if ($Configuration -ieq "Debug") { "appImmViewer_Debug.exe" } else { "appImmViewer_Release.exe" }
$viewerExe = Join-Path $exeDir $exeName
if (-not (Test-Path $viewerExe)) {
    throw "Viewer executable was not found: $viewerExe"
}

Remove-Item -LiteralPath $debugLog -Force -ErrorAction SilentlyContinue
$env:IMM_VIEWER_VALIDATE_DISABLE_AUDIO = "1"
$process = Start-Process -FilePath $viewerExe -ArgumentList @("settings-opengl-vr.json") -WorkingDirectory $exeDir -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$log = ""
try {
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path $debugLog) {
            $log = Get-Content $debugLog -Raw
            if ($log.Contains("IMM_LEGACY_VR_SMOKE frame_submitted")) {
                break
            }
        }
        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 500
    }
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(5000) | Out-Null
    }
    Remove-Item Env:\IMM_VIEWER_VALIDATE_DISABLE_AUDIO -ErrorAction SilentlyContinue
}

if (-not (Test-Path $debugLog)) {
    throw "Windows standalone OpenGL VR smoke did not write debug log: $debugLog"
}
Copy-Item -Force $debugLog $capturedLog
$log = Get-Content $capturedLog -Raw

foreach ($marker in @(
    "Rendering Backened: OpenGL",
    "XR Runtime: Legacy",
    "Rendering in VR: yes",
    "IMM_LEGACY_VR_SMOKE hmd_initialized",
    "Viewer initialized",
    "IMM_LEGACY_VR_SMOKE frame_submitted"
)) {
    Require-Marker $log $marker
}

foreach ($marker in @(
    "Cannot do VR",
    "Viewer init failed",
    "Failed to create render target",
    "Failed to create color render texture",
    "Failed to create depth render texture"
)) {
    if ($log.Contains($marker)) {
        throw "Windows standalone OpenGL VR smoke contained forbidden marker: $marker"
    }
}

Write-Host "Windows standalone OpenGL VR smoke passed"
Write-Host "Log: $capturedLog"

