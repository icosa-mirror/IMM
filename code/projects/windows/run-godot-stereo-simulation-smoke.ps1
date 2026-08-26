param(
    [string]$GodotExe = $env:GODOT_EXE,

    [string]$ProjectPath,

    [string]$OutputDirectory,

    [string]$ReplayPath,

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
if (-not $ProjectPath) {
    $ProjectPath = Join-Path $repoRoot "code\ImmGodotSampleProject"
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot "artifacts\godot-stereo-simulation"
}
if (-not $GodotExe -or -not (Test-Path -LiteralPath $GodotExe)) {
    throw "Godot executable not found. Pass -GodotExe or set GODOT_EXE."
}

$godot = (Resolve-Path -LiteralPath $GodotExe).Path
$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$output = (New-Item -ItemType Directory -Force -Path $OutputDirectory).FullName
$stdoutPath = Join-Path $output "godot.stdout.log"
$stderrPath = Join-Path $output "godot.stderr.log"
$resultPath = Join-Path $output "result.json"
$expectedFiles = @(
    $resultPath,
    (Join-Path $output "mono.png"),
    (Join-Path $output "left.png"),
    (Join-Path $output "right.png"),
    (Join-Path $output "summary.log"),
    (Join-Path $output "native.log")
)
foreach ($path in $expectedFiles + @($stdoutPath, $stderrPath)) {
    Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
}

$previousOutputDirectory = $env:IMM_GODOT_STEREO_SIM_OUTPUT_DIR
$previousNativeLog = $env:IMM_GODOT_LOG_FILE
$previousReplayPath = $env:IMM_GODOT_STEREO_REPLAY_PATH
$process = $null
try {
    $env:IMM_GODOT_STEREO_SIM_OUTPUT_DIR = $output
    $env:IMM_GODOT_LOG_FILE = Join-Path $output "native.log"
    if ($ReplayPath) {
        $env:IMM_GODOT_STEREO_REPLAY_PATH = (Resolve-Path -LiteralPath $ReplayPath).Path
    }
    else {
        Remove-Item Env:IMM_GODOT_STEREO_REPLAY_PATH -ErrorAction SilentlyContinue
    }
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $godot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @(
        "--path", $project,
        "--xr-mode", "off",
        "--display-driver", "windows",
        "--audio-driver", "Dummy",
        "--log-file", (Join-Path $output "godot-engine.log"),
        "--rendering-driver", "vulkan",
        "--rendering-method", "forward_plus",
        "--resolution", "1280x720",
        "--fixed-fps", "30",
        "--scene", "res://scenes/StereoSimulationSmokeScene.tscn"
    )) {
        $startInfo.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Godot stereo simulation process did not start"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        if (-not $process.HasExited) {
            $process.Kill($true)
        }
        throw "Godot stereo simulation timed out after $TimeoutSeconds seconds"
    }
    $stdoutTask.GetAwaiter().GetResult() | Set-Content -LiteralPath $stdoutPath
    $stderrTask.GetAwaiter().GetResult() | Set-Content -LiteralPath $stderrPath
    if ($process.ExitCode -ne 0) {
        throw "Godot stereo simulation failed with exit code $($process.ExitCode); inspect $resultPath and $stderrPath"
    }
}
finally {
    if ($null -eq $previousOutputDirectory) {
        Remove-Item Env:IMM_GODOT_STEREO_SIM_OUTPUT_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:IMM_GODOT_STEREO_SIM_OUTPUT_DIR = $previousOutputDirectory
    }
    if ($null -eq $previousNativeLog) {
        Remove-Item Env:IMM_GODOT_LOG_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:IMM_GODOT_LOG_FILE = $previousNativeLog
    }
    if ($null -eq $previousReplayPath) {
        Remove-Item Env:IMM_GODOT_STEREO_REPLAY_PATH -ErrorAction SilentlyContinue
    }
    else {
        $env:IMM_GODOT_STEREO_REPLAY_PATH = $previousReplayPath
    }
}

foreach ($path in $expectedFiles) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Godot stereo simulation did not write expected artifact: $path"
    }
}
$result = Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
if ($result.status -ne "passed") {
    throw "Godot stereo simulation reported failure: $($result.failures -join '; ')"
}

Write-Host "Godot stereo simulation passed: $resultPath"
Write-Host "Left orientation ratio: $($result.left_orientation.normal_to_flipped_ratio)"
Write-Host "Right orientation ratio: $($result.right_orientation.normal_to_flipped_ratio)"
Write-Host "Stereo mean difference: $($result.stereo_mean_difference)"
