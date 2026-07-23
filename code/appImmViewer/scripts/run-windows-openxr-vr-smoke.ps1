param(
    [string]$Configuration = "Release",
    [string]$LogDir = "artifacts/windows-standalone-openxr-vr",
    [int]$TimeoutSeconds = 45,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$projectDir = Join-Path $repoRoot "code\projects\windows"
$exeDir = Join-Path $repoRoot "code\appImmViewer\exe"
$logDirectory = (New-Item -ItemType Directory -Force $LogDir).FullName
$debugLog = Join-Path $exeDir "debug.txt"
$capturedLog = Join-Path $logDirectory "debug.txt"
$openXrDepsLog = Join-Path $logDirectory "openxr-deps.txt"

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

& (Join-Path $scriptDir "check-openxr-deps.ps1") 2>&1 | Out-File -FilePath $openXrDepsLog -Encoding utf8
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR dependency check failed. See $openXrDepsLog"
}

$exeName = if ($Configuration -ieq "Debug") { "appImmViewer_Debug.exe" } else { "appImmViewer_Release.exe" }
$viewerExe = Join-Path $exeDir $exeName
if (-not (Test-Path $viewerExe)) {
    throw "Viewer executable was not found: $viewerExe"
}

Remove-Item -LiteralPath $debugLog -Force -ErrorAction SilentlyContinue
$process = Start-Process -FilePath $viewerExe -ArgumentList @("settings-openxr-probe.json") -WorkingDirectory $exeDir -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "Windows standalone OpenXR VR smoke timed out after $TimeoutSeconds seconds"
}

if (-not (Test-Path $debugLog)) {
    throw "Windows standalone OpenXR VR smoke did not write debug log: $debugLog"
}
Copy-Item -Force $debugLog $capturedLog

$log = Get-Content $capturedLog -Raw
$requiredMarkers = @(
    "XR Runtime: OpenXR",
    "Rendering in VR: yes",
    "IMM_OPENXR_STANDALONE createInstanceResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE getHmdSystemResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE enumerateStereoViewsFillResult=0",
    "IMM_OPENXR_STANDALONE createSessionResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE enumerateSwapchainFormatsFillResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE createSwapchainResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE enumerateSwapchainImagesFillResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE beginSessionResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE waitFrameResult=0 resultName=XR_SUCCESS shouldRender=1",
    "IMM_OPENXR_STANDALONE beginFrameResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE acquireSwapchainImageResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE waitSwapchainImageResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE releaseSwapchainImageResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE endFrameResult=0 resultName=XR_SUCCESS layerCount=1 projectionViews=2",
    "IMM_OPENXR_STANDALONE endSessionResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE destroySwapchainResult=0 resultName=XR_SUCCESS",
    "IMM_OPENXR_STANDALONE destroySessionResult=0 resultName=XR_SUCCESS",
    "OpenXR standalone startup probe passed"
)

foreach ($marker in $requiredMarkers) {
    if (-not $log.Contains($marker)) {
        throw "Windows standalone OpenXR VR smoke missing marker: $marker"
    }
}

$forbiddenMarkers = @(
    "OpenXR standalone startup probe failed",
    "IMM_OPENXR_STANDALONE missing=loaderPath",
    "IMM_OPENXR_STANDALONE missing=requiredExport",
    "Cannot do VR"
)

foreach ($marker in $forbiddenMarkers) {
    if ($log.Contains($marker)) {
        throw "Windows standalone OpenXR VR smoke contained forbidden marker: $marker"
    }
}

Write-Host "Windows standalone OpenXR VR smoke passed"
Write-Host "Log: $capturedLog"
Write-Host "OpenXR deps: $openXrDepsLog"
