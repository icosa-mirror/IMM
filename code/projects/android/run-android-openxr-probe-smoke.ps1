param(
    [string]$Adb = $env:ADB,
    [int]$WaitSeconds = 20,
    [string]$LogDir = "logs/android-openxr-probe-smoke",
    [string]$BuildDir = "build_openxr_probe",
    [switch]$VrManifest,
    [switch]$SkipBuild,
    [switch]$AttemptQuestUnblock
)

$ErrorActionPreference = "Stop"

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
        throw "Android OpenXR probe log did not contain required marker: $Marker"
    }
}

function Write-AdbSnapshot([string]$Directory, [string]$Name) {
    & $adbPath shell dumpsys power | Out-File -FilePath (Join-Path $Directory "power_$Name.txt") -Encoding utf8
    & $adbPath shell dumpsys activity activities | Out-File -FilePath (Join-Path $Directory "activity_$Name.txt") -Encoding utf8
}

function Invoke-QuestUnblockAttempt([string]$Directory) {
    $attemptPath = Join-Path $Directory "quest_unblock_attempt.txt"
    "attempt=WAKEUP,MENU,BACK,BACK,BACK,DPAD_CENTER" | Out-File -FilePath $attemptPath -Encoding utf8
    & $adbPath shell input keyevent KEYCODE_WAKEUP 2>&1 | Out-File -FilePath $attemptPath -Append -Encoding utf8
    Start-Sleep -Milliseconds 500
    & $adbPath shell input keyevent KEYCODE_MENU 2>&1 | Out-File -FilePath $attemptPath -Append -Encoding utf8
    Start-Sleep -Milliseconds 500
    foreach ($i in 1..3) {
        & $adbPath shell input keyevent KEYCODE_BACK 2>&1 | Out-File -FilePath $attemptPath -Append -Encoding utf8
        Start-Sleep -Milliseconds 500
    }
    & $adbPath shell input keyevent KEYCODE_DPAD_CENTER 2>&1 | Out-File -FilePath $attemptPath -Append -Encoding utf8
    Start-Sleep -Seconds 2
}

function Test-BlockedLaunchState([string]$Log, [string]$ActivityText, [string]$PowerText) {
    if ($Log.Contains("Launch is blocked because: a Reprojected OS dialog is currently showing")) {
        throw "Android OpenXR probe launch is blocked by a reprojected Quest OS dialog. Clear the visible headset dialog, then rerun. Log: $logPath"
    }

    if ($Log.Contains("RequiresControllersLaunchInterceptor") -or
        $Log.Contains("REQUIRES_CONTROLLERS_LAUNCH_CHECK") -or
        $ActivityText.Contains("LaunchCheckControllerRequiredDialogActivity")) {
        throw "Android OpenXR probe launch is blocked by Quest controller-required launch check. Wake/connect controllers or use a hand-tracking-capable VR manifest, then rerun. Log: $logPath"
    }

    if ($ActivityText.Contains("com.oculus.os.vrlockscreen") -or
        $ActivityText.Contains("com.oculus.guardian")) {
        throw "Android OpenXR probe launch is blocked by Quest OS focus state (VR lockscreen/Guardian). Wake/unlock the headset and clear Guardian/dialogs, then rerun. Log: $logPath"
    }

    if ($PowerText.Contains("mWakefulness=Asleep") -or $PowerText.Contains("mScreenState=OFF")) {
        throw "Android OpenXR probe launch is blocked because the Quest display is asleep/off. Wake the headset and keep it active, then rerun. Log: $logPath"
    }
}

function Start-OpenXrProbeActivity {
    if ($VrManifest) {
        & $adbPath shell am start -a android.intent.action.MAIN -c com.oculus.intent.category.VR -n org.linuxfoundation.imm.player/.MainActivity | Out-Null
    } else {
        & $adbPath shell am start -n org.linuxfoundation.imm.player/.MainActivity | Out-Null
    }
}

$adbPath = Resolve-Adb $Adb
if ($VrManifest -and $BuildDir -eq "build_openxr_probe") {
    $BuildDir = "build_openxr_probe_vrmanifest"
}
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$apk = Join-Path $PSScriptRoot "appImmViewer\$BuildDir\outputs\apk\debug\appImmViewer-debug.apk"

if (-not $SkipBuild) {
    Push-Location $PSScriptRoot
    try {
        $manifestArg = if ($VrManifest) { "-PimmManifest=vr" } else { "-PimmManifest=nonVr" }
        & .\gradlew.bat :appImmViewer:assembleDebug -PimmNonVr=ON "-PimmBuildDir=$BuildDir" -PimmRendererApi=Vulkan -PimmXrRuntime=OpenXR $manifestArg
        if ($LASTEXITCODE -ne 0) {
            throw "Android OpenXR probe APK build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $apk)) {
    throw "Android OpenXR probe APK not found: $apk"
}

$devicesOutput = & $adbPath devices
$deviceLines = @($devicesOutput | Where-Object { $_ -match "`tdevice$" })
if ($deviceLines.Count -le 0) {
    throw "No Android device or emulator is attached for OpenXR probe runtime smoke."
}

Write-AdbSnapshot $logDirectory "before"

& $adbPath shell input keyevent KEYCODE_WAKEUP | Out-Null
if ($AttemptQuestUnblock) {
    Invoke-QuestUnblockAttempt $logDirectory
    Write-AdbSnapshot $logDirectory "after_unblock_attempt"
}
& $adbPath logcat -c
& $adbPath install --no-incremental -r $apk
if ($LASTEXITCODE -ne 0) {
    throw "adb install failed with exit code $LASTEXITCODE"
}

& $adbPath shell am force-stop org.linuxfoundation.imm.player | Out-Null
Start-OpenXrProbeActivity
if ($AttemptQuestUnblock) {
    Start-Sleep -Seconds 2
    Invoke-QuestUnblockAttempt $logDirectory
    Write-AdbSnapshot $logDirectory "after_post_launch_unblock_attempt"
    Start-OpenXrProbeActivity
}
Start-Sleep -Seconds $WaitSeconds

$logPath = Join-Path $logDirectory "logcat.txt"
$pidPath = Join-Path $logDirectory "pidof_after.txt"
$activityPath = Join-Path $logDirectory "activity_after.txt"
$powerPath = Join-Path $logDirectory "power_after.txt"

& $adbPath logcat -d | Out-File -FilePath $logPath -Encoding utf8
& $adbPath shell pidof org.linuxfoundation.imm.player | Out-File -FilePath $pidPath -Encoding utf8
& $adbPath shell dumpsys activity activities | Out-File -FilePath $activityPath -Encoding utf8
& $adbPath shell dumpsys power | Out-File -FilePath $powerPath -Encoding utf8

$log = Get-Content $logPath -Raw
$activityText = Get-Content $activityPath -Raw
$powerText = Get-Content $powerPath -Raw
Test-BlockedLaunchState $log $activityText $powerText

$requiredMarkers = @(
    "IMM_ANDROID_OPENXR_PROBE begin",
    "IMM_ANDROID_OPENXR_PROBE initializeLoaderResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE enumerateExtensionsResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE enumerateExtensionsFillResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE createInstanceResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE getHmdSystemResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE enumerateStereoViewsFillResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE destroyInstanceResult=0 resultName=XR_SUCCESS",
    "IMM_ANDROID_OPENXR_PROBE end"
)

foreach ($marker in $requiredMarkers) {
    Require-Marker $log $marker
}

Write-Host "Android OpenXR probe smoke passed"
Write-Host "Log: $logPath"
