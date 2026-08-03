param(
    [string]$GodotExe = $env:GODOT_EXE,

    [string]$ProjectPath,

    [string]$CapturePath,

    [string]$LogDir,

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
if (-not $ProjectPath) {
    $ProjectPath = Join-Path $repoRoot "code\ImmGodotSampleProject"
}
if (-not $LogDir) {
    $LogDir = Join-Path $repoRoot "artifacts\godot-sample-play"
}

$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$logs = (New-Item -ItemType Directory -Force -Path $LogDir).FullName
if (-not $CapturePath) {
    $CapturePath = Join-Path $logs "godot-sample-play.png"
}
$capture = [System.IO.Path]::GetFullPath($CapturePath)
$captureDir = Split-Path -Parent $capture
New-Item -ItemType Directory -Force -Path $captureDir | Out-Null

if (-not $GodotExe -or -not (Test-Path -LiteralPath $GodotExe)) {
    throw "Godot executable not found. Pass -GodotExe or set GODOT_EXE."
}
$godot = (Resolve-Path -LiteralPath $GodotExe).Path

# Opening a freshly cloned project in the editor builds Godot's resource UID
# cache and discovers GDExtensions before the user presses Run. Reproduce that
# initialization when CI has no project cache yet.
$extensionList = Join-Path $project ".godot\extension_list.cfg"
if (-not (Test-Path -LiteralPath $extensionList)) {
    Write-Host "[IMM_GODOT_SAMPLE_PLAY_20260803] importing fresh Godot project"
    & $godot --editor --headless --path $project --quit
    if ($LASTEXITCODE -ne 0) {
        throw "Godot sample project import failed with exit code $LASTEXITCODE"
    }
    if (-not (Test-Path -LiteralPath $extensionList)) {
        throw "Godot sample project import did not discover its GDExtension"
    }
}

$stdoutPath = Join-Path $logs "godot-sample-play.stdout.log"
$stderrPath = Join-Path $logs "godot-sample-play.stderr.log"
$controllerLog = Join-Path $logs "godot-sample-play-controller.log"
$nativeLog = Join-Path $logs "godot-sample-play-native.log"
foreach ($path in @($capture, $stdoutPath, $stderrPath, $controllerLog, $nativeLog)) {
    Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
}

$environmentNames = @(
    "IMM_GODOT_SAMPLE_PLAY_SMOKE",
    "IMM_GODOT_SAMPLE_PLAY_CAPTURE",
    "IMM_GODOT_SAMPLE_PLAY_LOG",
    "IMM_GODOT_LOG_FILE"
)
$previousEnvironment = @{}
foreach ($name in $environmentNames) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name)
}

$output = @()
$exitCode = -1
$timedOut = $false
try {
    $env:IMM_GODOT_SAMPLE_PLAY_SMOKE = "1"
    $env:IMM_GODOT_SAMPLE_PLAY_CAPTURE = $capture
    $env:IMM_GODOT_SAMPLE_PLAY_LOG = $controllerLog
    $env:IMM_GODOT_LOG_FILE = $nativeLog

    # Do not pass --scene or --script: Godot must use project.godot's
    # run/main_scene, exactly like the editor Run button.
    $godotArgs = @(
        "--path", $project,
        "--rendering-driver", "vulkan",
        "--rendering-method", "forward_plus",
        "--resolution", "1280x720",
        "--fixed-fps", "30"
    )
    $process = Start-Process -FilePath $godot `
        -ArgumentList $godotArgs `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
        $timedOut = $true
    }
    if (-not $timedOut) {
        $exitCode = $process.ExitCode
    }
}
finally {
    foreach ($entry in $previousEnvironment.GetEnumerator()) {
        if ($null -eq $entry.Value) {
            Remove-Item -Path "env:$($entry.Key)" -ErrorAction SilentlyContinue
        }
        else {
            Set-Item -Path "env:$($entry.Key)" -Value $entry.Value
        }
    }
    if (Test-Path -LiteralPath $stdoutPath) {
        $output += Get-Content -LiteralPath $stdoutPath
    }
    if (Test-Path -LiteralPath $stderrPath) {
        $output += Get-Content -LiteralPath $stderrPath
    }
}

$output | ForEach-Object { Write-Host $_ }
$outputText = $output -join "`n"
if ($timedOut) {
    foreach ($diagnosticPath in @($controllerLog, $nativeLog)) {
        if (Test-Path -LiteralPath $diagnosticPath) {
            Write-Host "$([string]$diagnosticPath):"
            Get-Content -LiteralPath $diagnosticPath | ForEach-Object { Write-Host $_ }
        }
    }
    throw "Godot sample Play smoke timed out after $TimeoutSeconds seconds"
}
if ($exitCode -ne 0) {
    throw "Godot sample Play smoke failed with exit code $exitCode"
}
if (-not (Test-Path -LiteralPath $capture)) {
    throw "Godot sample Play smoke did not write capture: $capture"
}
if (-not (Test-Path -LiteralPath $controllerLog)) {
    throw "Godot sample Play smoke did not write controller log: $controllerLog"
}
if (-not (Test-Path -LiteralPath $nativeLog)) {
    throw "Godot sample Play smoke did not write native log: $nativeLog"
}
$controllerText = Get-Content -Raw -LiteralPath $controllerLog
if (-not $controllerText.Contains("[IMM_GODOT_SAMPLE_PLAY_20260803] passed")) {
    throw "Godot sample Play smoke did not write its success marker"
}
$fatalText = $outputText
if (Test-Path -LiteralPath $nativeLog) {
    $fatalText += "`n" + (Get-Content -Raw -LiteralPath $nativeLog)
}
foreach ($forbidden in @("VK_ERROR_DEVICE_LOST", "Device Lost", "signal 11", "Segmentation fault", "IMM_ASSERT_")) {
    if ($fatalText.Contains($forbidden)) {
        throw "Godot sample Play smoke logs contain fatal marker: $forbidden"
    }
}

Write-Host "Godot sample Play smoke passed: $capture"
