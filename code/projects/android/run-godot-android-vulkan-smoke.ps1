param(
    [string]$GodotExe = $env:GODOT_EXE,

    [string]$Adb = $env:ADB,

    [int]$WaitSeconds = 40,

    [string]$LogDir = "logs/android-godot-vulkan-smoke"
)

$ErrorActionPreference = "Stop"

function Resolve-Tool([string]$Requested, [string]$CommandName, [string]$Description) {
    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "$Description not found. Pass the explicit path or make $CommandName available on PATH."
}

function Resolve-Adb([string]$Requested) {
    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $adbExecutable = if ($IsWindows) { "adb.exe" } else { "adb" }

    $cmd = Get-Command adb -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $localProperties = Join-Path $PSScriptRoot "local.properties"
    if (Test-Path $localProperties) {
        $sdkLine = Get-Content $localProperties | Where-Object { $_ -match "^sdk\.dir=" } | Select-Object -First 1
        if ($sdkLine) {
            $sdkDir = ($sdkLine -replace "^sdk\.dir=", "").Replace("\\", [IO.Path]::DirectorySeparatorChar)
            $candidate = Join-Path $sdkDir (Join-Path "platform-tools" $adbExecutable)
            if (Test-Path $candidate) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    if ($env:ANDROID_SDK_ROOT) {
        $candidate = Join-Path $env:ANDROID_SDK_ROOT (Join-Path "platform-tools" $adbExecutable)
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "adb not found. Pass -Adb, set ADB, put adb on PATH, configure local.properties sdk.dir, or set ANDROID_SDK_ROOT."
}

function Assert-LogContains([string]$Log, [string]$Marker) {
    if (-not $Log.Contains($Marker)) {
        throw "Android Godot Vulkan smoke log did not contain required marker: $Marker"
    }
}

function Wait-ForDevice([string]$AdbPath, [int]$TimeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $devicesOutput = & $AdbPath devices
        $deviceLines = @($devicesOutput | Where-Object { $_ -match "`tdevice$" })
        if ($deviceLines.Count -gt 0) {
            return
        }
        Start-Sleep -Seconds 2
    } while ((Get-Date) -lt $deadline)

    throw "No Android device or emulator is attached for Godot Vulkan runtime smoke."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$project = Join-Path $repoRoot (Join-Path "code" "ImmGodotSampleProject")
$sampleSource = Join-Path $repoRoot (Join-Path "exampleImmFiles" "sample1.imm")
$sampleTarget = Join-Path $project "sample1.imm"
$apk = Join-Path $project (Join-Path "build" "android" "imm-godot-sample-debug.apk")
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$logPath = Join-Path $logDirectory "logcat.txt"
$pngPath = Join-Path $logDirectory "vulkan_visual_smoke.png"
$faceOrientationPngPath = Join-Path $logDirectory "godot-android-vulkan-face-orientation.png"
$devicePngPath = "/sdcard/Android/data/org.linuxfoundation.imm.godot.sample/files/imm-ftl/vulkan_visual_smoke.png"
$deviceFaceOrientationPngPath = "/sdcard/Android/data/org.linuxfoundation.imm.godot.sample/files/imm-ftl/godot-android-vulkan-face-orientation.png"

$godotPath = Resolve-Tool $GodotExe "godot" "Godot"
$adbPath = Resolve-Adb $Adb

if (-not (Test-Path $sampleSource)) {
    throw "Sample IMM not found: $sampleSource"
}

$hadSample = Test-Path $sampleTarget
if (-not $hadSample) {
    Copy-Item -LiteralPath $sampleSource -Destination $sampleTarget
}

try {
    & $godotPath --headless --path $project --import
    if ($LASTEXITCODE -ne 0) {
        throw "Godot import failed with exit code $LASTEXITCODE"
    }

    & $godotPath --headless --path $project --export-debug "Android Debug" $apk
    if ($LASTEXITCODE -ne 0) {
        throw "Godot Android export failed with exit code $LASTEXITCODE"
    }
}
finally {
    if (-not $hadSample -and (Test-Path $sampleTarget)) {
        Remove-Item -LiteralPath $sampleTarget
    }
}

if (-not (Test-Path $apk)) {
    throw "Android Godot APK not found: $apk"
}

Wait-ForDevice $adbPath 30

& $adbPath logcat -c
& $adbPath install --no-incremental -r $apk
if ($LASTEXITCODE -ne 0) {
    throw "adb install failed with exit code $LASTEXITCODE"
}

& $adbPath shell am force-stop org.linuxfoundation.imm.godot.sample | Out-Null
# Godot 4.7 launches through an exported activity alias; GodotApp itself is private.
$launchOutput = & $adbPath shell am start -W -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -p org.linuxfoundation.imm.godot.sample
if ($LASTEXITCODE -ne 0 -or ($launchOutput -join "`n") -notmatch "Status:\s+ok") {
    throw "Android Godot launcher intent failed: $($launchOutput -join ' ')"
}
Start-Sleep -Seconds $WaitSeconds

& $adbPath logcat -d | Out-File -FilePath $logPath -Encoding utf8
& $adbPath pull $devicePngPath $pngPath | Out-Null
& $adbPath pull $deviceFaceOrientationPngPath $faceOrientationPngPath | Out-Null
& $adbPath shell am force-stop org.linuxfoundation.imm.godot.sample | Out-Null

$log = Get-Content $logPath -Raw
foreach ($marker in @(
    "IMM Godot Vulkan visual smoke passed",
    "Loaded in CPU",
    "Loaded in GPU",
    "Vulkan renderer submitted picture draw commands",
    "Vulkan renderer submitted static paint draw commands",
    "last_vulkan_frame_started"
)) {
    Assert-LogContains $log $marker
}

foreach ($marker in @("Fatal signal", "F DEBUG", "piFile::Open failed", "Could not load IMM from disk")) {
    if ($log.Contains($marker)) {
        throw "Android Godot Vulkan smoke log contained failure marker: $marker"
    }
}

if (-not (Test-Path $pngPath) -or (Get-Item $pngPath).Length -le 0) {
    throw "Android Godot Vulkan smoke PNG was not captured: $pngPath"
}
if (-not (Test-Path $faceOrientationPngPath) -or (Get-Item $faceOrientationPngPath).Length -le 0) {
    throw "Android Godot Vulkan face-orientation PNG was not captured: $faceOrientationPngPath"
}

Write-Host "Android Godot Vulkan smoke passed"
Write-Host "Log: $logPath"
Write-Host "PNG: $pngPath"
Write-Host "Face orientation PNG: $faceOrientationPngPath"
