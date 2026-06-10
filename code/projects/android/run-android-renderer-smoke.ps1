param(
    [Alias("?")]
    [switch]$Help,

    [ValidateSet("Vulkan", "GLES")]
    [string]$RendererApi = "Vulkan",

    [string]$Adb = $env:ADB,

    [int]$WaitSeconds = 20,

    [string]$LogDir = "",

    [string]$BuildDir = "",

    [switch]$UseIntentRendererExtra,

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Usage: ./run-android-renderer-smoke.ps1 [-RendererApi Vulkan|GLES] [-Adb <adb>] [-WaitSeconds <seconds>] [-LogDir <path>] [-BuildDir <dir>] [-UseIntentRendererExtra] [-SkipBuild]"
    return
}

function Resolve-Adb([string]$Requested) {
    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $cmd = Get-Command adb -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $localProperties = Join-Path $PSScriptRoot "local.properties"
    if (Test-Path $localProperties) {
        $sdkLine = Get-Content $localProperties | Where-Object { $_ -match "^sdk\.dir=" } | Select-Object -First 1
        if ($sdkLine) {
            $sdkDir = ($sdkLine -replace "^sdk\.dir=", "").Replace("\\", "\")
            $candidate = Join-Path $sdkDir "platform-tools\adb.exe"
            if (Test-Path $candidate) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    if ($env:ANDROID_SDK_ROOT) {
        $candidate = Join-Path $env:ANDROID_SDK_ROOT "platform-tools\adb.exe"
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "adb not found. Pass -Adb, set ADB, put adb on PATH, configure local.properties sdk.dir, or set ANDROID_SDK_ROOT."
}

function Require-Marker([string]$Log, [string]$Marker) {
    if (-not $Log.Contains($Marker)) {
        throw "Android $RendererApi smoke log did not contain required marker: $Marker"
    }
}

function Write-AdbSnapshot([string]$Directory, [string]$Name) {
    & $adbPath shell dumpsys power | Out-File -FilePath (Join-Path $Directory "power_$Name.txt") -Encoding utf8
    & $adbPath shell dumpsys activity activities | Out-File -FilePath (Join-Path $Directory "activity_$Name.txt") -Encoding utf8
}

function Test-BlockedLaunchState([string]$Log, [string]$ActivityText, [string]$PowerText) {
    if ($Log.Contains("Launch is blocked because: a Reprojected OS dialog is currently showing") -or
        $ActivityText.Contains("com.oculus.os.vrlockscreen") -or
        $ActivityText.Contains("com.oculus.guardian")) {
        throw "Android $RendererApi smoke launch is blocked by Quest OS focus state (VR lockscreen/Guardian/reprojected OS dialog). Wake/unlock the headset and clear Guardian/dialogs, then rerun. Log: $logPath"
    }

    if ($PowerText.Contains("mWakefulness=Asleep") -or $PowerText.Contains("mScreenState=OFF")) {
        throw "Android $RendererApi smoke launch is blocked because the Quest display is asleep/off. Wake the headset and keep it active, then rerun. Log: $logPath"
    }
}

if (-not $BuildDir) {
    $BuildDir = if ($RendererApi -eq "Vulkan") { "build_vulkan" } else { "build_gles_fallback" }
}

if (-not $LogDir) {
    $LogDir = "logs/android-$($RendererApi.ToLowerInvariant())-smoke"
}

$adbPath = Resolve-Adb $Adb
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$apk = Join-Path $PSScriptRoot "appImmViewer\$BuildDir\outputs\apk\debug\appImmViewer-debug.apk"

if (-not $SkipBuild) {
    Push-Location $PSScriptRoot
    try {
        & .\gradlew.bat :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug :appImmViewer:assembleDebug -PimmNonVr=ON "-PimmRendererApi=$RendererApi" "-PimmBuildDir=$BuildDir"
        if ($LASTEXITCODE -ne 0) {
            throw "Android $RendererApi APK build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $apk)) {
    throw "Android $RendererApi APK not found: $apk"
}

$devicesOutput = & $adbPath devices
$deviceLines = @($devicesOutput | Where-Object { $_ -match "`tdevice$" })
if ($deviceLines.Count -le 0) {
    throw "No Android device or emulator is attached for $RendererApi runtime smoke."
}

Write-AdbSnapshot $logDirectory "before"

& $adbPath logcat -c
& $adbPath install --no-incremental -r $apk
if ($LASTEXITCODE -ne 0) {
    throw "adb install failed with exit code $LASTEXITCODE"
}

& $adbPath shell am force-stop org.linuxfoundation.imm.player | Out-Null
if ($UseIntentRendererExtra) {
    & $adbPath shell am start -n org.linuxfoundation.imm.player/.MainActivity --es RenderingAPI $RendererApi | Out-Null
} else {
    & $adbPath shell am start -n org.linuxfoundation.imm.player/.MainActivity | Out-Null
}
Start-Sleep -Seconds $WaitSeconds

$logPath = Join-Path $logDirectory "logcat.txt"
$pidPath = Join-Path $logDirectory "pidof_after.txt"
$activityPath = Join-Path $logDirectory "activity_after.txt"
$powerPath = Join-Path $logDirectory "power_after.txt"
$screencapPath = Join-Path $logDirectory "screencap_after.png"
$deviceScreencapPath = "/sdcard/imm_$($RendererApi.ToLowerInvariant())_smoke_screencap.png"

& $adbPath logcat -d | Out-File -FilePath $logPath -Encoding utf8
& $adbPath shell pidof org.linuxfoundation.imm.player | Out-File -FilePath $pidPath -Encoding utf8
& $adbPath shell dumpsys activity activities | Out-File -FilePath $activityPath -Encoding utf8
& $adbPath shell dumpsys power | Out-File -FilePath $powerPath -Encoding utf8
& $adbPath shell screencap -p $deviceScreencapPath | Out-Null
& $adbPath pull $deviceScreencapPath $screencapPath | Out-Null
& $adbPath shell rm $deviceScreencapPath | Out-Null
if (-not (Test-Path $screencapPath) -or (Get-Item $screencapPath).Length -le 0) {
    throw "Android $RendererApi smoke screenshot was not captured: $screencapPath"
}

$log = Get-Content $logPath -Raw
$activityText = Get-Content $activityPath -Raw
$powerText = Get-Content $powerPath -Raw
Test-BlockedLaunchState $log $activityText $powerText

$pidText = [string](Get-Content $pidPath -Raw)
$pidText = $pidText.Trim()
if (-not $pidText) {
    throw "Android $RendererApi smoke process was not running after $WaitSeconds seconds. Log: $logPath"
}

$requiredMarkers = @(
    "IMM Android renderer API: $RendererApi",
    "IMMAVAL loadPath result=1",
    "Loaded in CPU",
    "Loaded in GPU"
)

foreach ($marker in $requiredMarkers) {
    Require-Marker $log $marker
}

if ($RendererApi -eq "Vulkan") {
    $vulkanMarkers = @(
        "Vulkan renderer created Android surface",
        "Vulkan renderer initialized with owned device",
        "Vulkan renderer submitted picture draw commands",
        "Vulkan renderer submitted static paint draw commands"
    )

    foreach ($marker in $vulkanMarkers) {
        Require-Marker $log $marker
    }
}

$forbiddenMarkers = @(
    "Vulkan renderer failed",
    "Vulkan renderer could not",
    "Vulkan draw submission is not implemented yet",
    "Could not initialize piRenderer",
    "Failed to load IMM"
)

foreach ($marker in $forbiddenMarkers) {
    if ($log.Contains($marker)) {
        throw "Android $RendererApi smoke log contained failure marker: $marker"
    }
}

Write-Host "Android $RendererApi smoke passed"
Write-Host "Log: $logPath"
Write-Host "Activity dump: $activityPath"
Write-Host "Screenshot: $screencapPath"
