param(
    [string]$Adb = $env:ADB,

    [int]$WaitSeconds = 20,

    [string]$LogDir = "logs/android-vulkan-smoke",

    [switch]$SkipBuild
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

$adbPath = Resolve-Adb $Adb
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$apk = Join-Path $PSScriptRoot "appImmViewer\build_vulkan\outputs\apk\debug\appImmViewer-debug.apk"

if (-not $SkipBuild) {
    Push-Location $PSScriptRoot
    try {
        & .\gradlew.bat :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug :appImmViewer:assembleDebug -PimmNonVr=ON -PimmRendererApi=Vulkan -PimmBuildDir=build_vulkan
        if ($LASTEXITCODE -ne 0) {
            throw "Android Vulkan APK build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $apk)) {
    throw "Android Vulkan APK not found: $apk"
}

$devicesOutput = & $adbPath devices
$deviceLines = @($devicesOutput | Where-Object { $_ -match "`tdevice$" })
if ($deviceLines.Count -le 0) {
    throw "No Android device or emulator is attached for Vulkan runtime smoke."
}

& $adbPath logcat -c
& $adbPath install -r $apk
if ($LASTEXITCODE -ne 0) {
    throw "adb install failed with exit code $LASTEXITCODE"
}

& $adbPath shell am force-stop org.linuxfoundation.imm.player | Out-Null
& $adbPath shell am start -n org.linuxfoundation.imm.player/.MainActivity | Out-Null
Start-Sleep -Seconds $WaitSeconds

$logPath = Join-Path $logDirectory "logcat.txt"
& $adbPath logcat -d | Out-File -FilePath $logPath -Encoding utf8

$log = Get-Content $logPath -Raw
$requiredMarkers = @(
    "IMM Android renderer API: Vulkan",
    "Vulkan renderer created Android surface",
    "Vulkan renderer initialized with owned device",
    "Loaded in CPU",
    "Loaded in GPU",
    "Vulkan renderer submitted picture draw commands",
    "Vulkan renderer submitted static paint draw commands"
)

foreach ($marker in $requiredMarkers) {
    if (-not $log.Contains($marker)) {
        throw "Android Vulkan smoke log did not contain required marker: $marker"
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
        throw "Android Vulkan smoke log contained failure marker: $marker"
    }
}

Write-Host "Android Vulkan smoke passed"
Write-Host "Log: $logPath"
