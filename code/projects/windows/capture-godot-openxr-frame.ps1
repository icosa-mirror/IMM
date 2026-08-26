param(
    [string]$GodotExe = $env:GODOT_EXE,

    [string]$ProjectPath,

    [string]$OutputDirectory,

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
if (-not $ProjectPath) {
    $ProjectPath = Join-Path $repoRoot "code\ImmGodotXRSampleProject"
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot "artifacts\godot-xr-matrix-replay"
}
if (-not $GodotExe -or -not (Test-Path -LiteralPath $GodotExe)) {
    throw "Godot executable not found. Pass -GodotExe or set GODOT_EXE."
}

$godot = (Resolve-Path -LiteralPath $GodotExe).Path
$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$output = (New-Item -ItemType Directory -Force -Path $OutputDirectory).FullName
$capturePath = Join-Path $output "xr-frame.json"
$stdoutPath = Join-Path $output "godot.stdout.log"
$stderrPath = Join-Path $output "godot.stderr.log"
$engineLogPath = Join-Path $output "godot-engine.log"
$nativeLogPath = Join-Path $output "native.log"
$mirrorPath = Join-Path $output "xr-mirror.png"
foreach ($path in @($capturePath, $stdoutPath, $stderrPath, $engineLogPath, $nativeLogPath, $mirrorPath)) {
    Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
}

$previousCapturePath = $env:IMM_GODOT_XR_FRAME_CAPTURE_PATH
$previousMirrorPath = $env:IMM_GODOT_XR_MIRROR_CAPTURE_PATH
$previousNativeLog = $env:IMM_GODOT_LOG_FILE
try {
    $env:IMM_GODOT_XR_FRAME_CAPTURE_PATH = $capturePath
    $env:IMM_GODOT_XR_MIRROR_CAPTURE_PATH = $mirrorPath
    $env:IMM_GODOT_LOG_FILE = $nativeLogPath
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $godot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @(
        "--path", $project,
        "--xr-mode", "on",
        "--display-driver", "windows",
        "--audio-driver", "Dummy",
        "--log-file", $engineLogPath,
        "--rendering-driver", "vulkan",
        "--rendering-method", "forward_plus",
        "--fixed-fps", "30",
        "--scene", "res://scenes/XRSampleScene.tscn"
    )) {
        $startInfo.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Godot OpenXR capture process did not start"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        if (-not $process.HasExited) {
            $process.Kill($true)
        }
        throw "Godot OpenXR frame capture timed out after $TimeoutSeconds seconds"
    }
    $stdoutTask.GetAwaiter().GetResult() | Set-Content -LiteralPath $stdoutPath
    $stderrTask.GetAwaiter().GetResult() | Set-Content -LiteralPath $stderrPath
    if ($process.ExitCode -ne 0) {
        throw "Godot OpenXR frame capture failed with exit code $($process.ExitCode); inspect $engineLogPath"
    }
}
finally {
    if ($null -eq $previousCapturePath) {
        Remove-Item Env:IMM_GODOT_XR_FRAME_CAPTURE_PATH -ErrorAction SilentlyContinue
    }
    else {
        $env:IMM_GODOT_XR_FRAME_CAPTURE_PATH = $previousCapturePath
    }
    if ($null -eq $previousMirrorPath) {
        Remove-Item Env:IMM_GODOT_XR_MIRROR_CAPTURE_PATH -ErrorAction SilentlyContinue
    }
    else {
        $env:IMM_GODOT_XR_MIRROR_CAPTURE_PATH = $previousMirrorPath
    }
    if ($null -eq $previousNativeLog) {
        Remove-Item Env:IMM_GODOT_LOG_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:IMM_GODOT_LOG_FILE = $previousNativeLog
    }
}

if (-not (Test-Path -LiteralPath $capturePath)) {
    throw "Godot OpenXR did not write the frame capture: $capturePath"
}
if (-not (Test-Path -LiteralPath $mirrorPath)) {
    throw "Godot OpenXR did not write the mirror capture: $mirrorPath"
}
$capture = Get-Content -Raw -LiteralPath $capturePath | ConvertFrom-Json
if ($capture.status -ne "captured" -or -not $capture.available) {
    throw "Godot OpenXR frame capture is invalid: $capturePath"
}
foreach ($matrixName in @(
    "world_to_head",
    "head_projection",
    "world_to_left_eye",
    "left_eye_projection",
    "world_to_right_eye",
    "right_eye_projection"
)) {
    if ($capture.$matrixName.Count -ne 16) {
        throw "Godot OpenXR frame capture matrix $matrixName does not contain 16 values"
    }
}

Write-Host "Godot OpenXR frame captured: $capturePath"
Write-Host "Godot OpenXR mirror captured: $mirrorPath"
