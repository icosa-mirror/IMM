param(
    [Parameter(Mandatory = $true)][string]$Apk,
    [string]$Adb = 'adb',
    [string]$Serial,
    [string]$PackageName = 'com.ImmersiveFoundation.IMMUnityTest',
    [string]$OutputDir = 'artifacts/unity-quest-urp',
    [int]$WaitSeconds = 180
)

$ErrorActionPreference = 'Stop'
if ($PackageName -notmatch '^[A-Za-z0-9_.]+$') { throw 'Invalid Android package name' }
if ($WaitSeconds -lt 1) { throw 'WaitSeconds must be positive' }
$apkPath = (Resolve-Path -LiteralPath $Apk).Path
$adbPath = (Get-Command $Adb -ErrorAction Stop).Source
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$evidenceDir = (Resolve-Path -LiteralPath $OutputDir).Path
$deviceArguments = @()

function Invoke-ProbeAdb([string[]]$AdbArguments, [switch]$AllowFailure) {
    $commandOutput = & $adbPath @deviceArguments @AdbArguments 2>&1
    $commandExit = $LASTEXITCODE
    $commandOutput | Add-Content -LiteralPath (Join-Path $evidenceDir 'adb.log')
    if ($commandExit -ne 0 -and !$AllowFailure) {
        throw "ADB $($AdbArguments[0]) failed with exit code $commandExit; see adb.log"
    }
    return @($commandOutput | ForEach-Object { $_.ToString() })
}

$connected = @(Invoke-ProbeAdb -AdbArguments @('devices') | Where-Object { $_ -match '^\S+\s+device\s*$' } |
    ForEach-Object { ($_ -split '\s+')[0] })
if ($Serial) {
    if ($Serial -notin $connected) { throw 'Requested device is not connected and authorized' }
} elseif ($connected.Count -eq 1) {
    $Serial = $connected[0]
} else {
    throw 'Connect one authorized Quest or supply -Serial to choose the device'
}
$deviceArguments = @('-s', $Serial)
$existing = (Invoke-ProbeAdb -AdbArguments @('shell', 'pidof', $PackageName) -AllowFailure) -join ''
if ($existing.Trim()) { throw 'The Unity sample is already running; stop it before starting this probe' }

Invoke-ProbeAdb -AdbArguments @('install', '--no-incremental', '-r', $apkPath) | Out-Null
$resolvedActivity = @(Invoke-ProbeAdb -AdbArguments @('shell', 'cmd', 'package', 'resolve-activity', '--brief',
    '-a', 'android.intent.action.MAIN',
    '-c', 'android.intent.category.LAUNCHER', $PackageName))
$component = $resolvedActivity | Where-Object { $_ -match '^[A-Za-z0-9_.]+/[A-Za-z0-9_.]+$' } | Select-Object -Last 1
if (!$component -or !($component.StartsWith("$PackageName/"))) { throw 'Could not resolve the Unity sample activity' }

$runId = [Guid]::NewGuid().ToString('N')
$captureOnDevice = "/sdcard/Android/data/$PackageName/files/imm-urp-xr-smoke.png"
$unityArguments = "-immUrpXrSmoke -immUrpXrRunId $runId -immUrpXrCapturePath $captureOnDevice"
$logPath = Join-Path $evidenceDir 'logcat.txt'
$ownedLaunch = $false
try {
    Invoke-ProbeAdb -AdbArguments @('shell', "am start -a android.intent.action.MAIN -n $component --es unity '$unityArguments'") | Out-Null
    $ownedLaunch = $true
    $deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds)
    $passed = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        # Preserve shared log buffers. Correlation excludes stale runs without logcat -c.
        & $adbPath @deviceArguments logcat -d -v threadtime > $logPath
        if ($LASTEXITCODE -ne 0) { throw 'Could not read Quest logcat' }
        $failed = Select-String -LiteralPath $logPath -SimpleMatch "[IMM_URP_XR_SMOKE] FAIL run=$runId "
        if ($failed) { throw 'Unity Quest URP probe failed; see logcat.txt' }
        $completion = Select-String -LiteralPath $logPath -Pattern "\[IMM_URP_XR_SMOKE\] PASS api=Vulkan .*run=$runId\b"
        if ($completion) { $passed = $true; break }
        Start-Sleep -Seconds 2
    }
    if (!$passed) { throw 'Unity Quest URP probe timed out; see logcat.txt and headset focus/tracking state' }
    Select-String -LiteralPath $logPath -Pattern "\[IMM_URP_XR_SMOKE\].*run=$runId\b" |
        ForEach-Object { $_.Line } | Set-Content -LiteralPath (Join-Path $evidenceDir 'probe.log')
    $capturePath = Join-Path $evidenceDir 'quest-mirror.png'
    Invoke-ProbeAdb -AdbArguments @('pull', $captureOnDevice, $capturePath) | Out-Null
    if (!(Test-Path -LiteralPath $capturePath) -or (Get-Item -LiteralPath $capturePath).Length -eq 0) {
        throw 'Unity Quest URP mirror capture is missing'
    }
    $eyeDirectoryName = "imm-urp-xr-$runId"
    $eyeDirectory = Join-Path $evidenceDir $eyeDirectoryName
    Invoke-ProbeAdb -AdbArguments @('pull', "/sdcard/Android/data/$PackageName/files/$eyeDirectoryName", $eyeDirectory) | Out-Null
    foreach ($phase in 0..4) {
        foreach ($eye in 0..1) {
            $eyePath = Join-Path $eyeDirectory "depth-$phase-eye-$eye.png"
            if (!(Test-Path -LiteralPath $eyePath) -or (Get-Item -LiteralPath $eyePath).Length -eq 0) {
                throw "Unity Quest URP per-eye capture is missing: depth-$phase-eye-$eye.png"
            }
        }
    }
    [ordered]@{ result = 'success'; run_id = $runId; renderer = 'Vulkan';
        capture = 'quest-mirror.png'; per_eye_directory = $eyeDirectoryName; log = 'logcat.txt' } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidenceDir 'probe-result.json')
    Write-Output "Unity Quest URP submission/allocation/stereo-depth probe passed: $evidenceDir"
} finally {
    # Only stop the package instance launched above, never an unrelated app or device.
    if ($ownedLaunch) {
        Invoke-ProbeAdb -AdbArguments @('shell', 'am', 'force-stop', $PackageName) -AllowFailure | Out-Null
    }
}
