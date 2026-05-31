param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [int]$DurationSeconds = 45,
    [int]$MinNonblackPixels = 20000,
    [int]$MinNearVisiblePixels = 10000,
    [switch]$SkipBuild,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
$viewerExeDir = Join-Path $repoRoot "code\appImmViewer\exe"
$debugLog = Join-Path $viewerExeDir "debug.txt"
$capturePath = Join-Path $viewerExeDir "vulkan_sample1_smoke.ppm"
$captureTmpPath = "$capturePath.tmp"
$settingsPath = Join-Path $viewerExeDir "settings-vulkan-smoke.json"
$exeName = if ($Configuration -ieq "Debug") { "appImmViewer_Debug.exe" } else { "appImmViewer_Release.exe" }
$viewerExe = Join-Path $viewerExeDir $exeName

function Find-MSBuild {
    if ($env:MSBUILD_EXE_PATH -and (Test-Path $env:MSBUILD_EXE_PATH)) {
        return $env:MSBUILD_EXE_PATH
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\Current\Bin\amd64\MSBuild.exe" | Select-Object -First 1
        if ($path -and (Test-Path $path)) {
            return $path
        }
    }

    $knownPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    if (Test-Path $knownPath) {
        return $knownPath
    }

    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "MSBuild.exe was not found. Set MSBUILD_EXE_PATH or install Visual Studio Build Tools."
}

function Read-PpmStats {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        throw "Capture was not written: $Path"
    }

    $script:ppmBytes = [System.IO.File]::ReadAllBytes((Resolve-Path $Path))
    $script:ppmIdx = 0
    function Read-Token {
        while ($script:ppmIdx -lt $script:ppmBytes.Length -and [char]$script:ppmBytes[$script:ppmIdx] -match "\s") { $script:ppmIdx++ }
        $start = $script:ppmIdx
        while ($script:ppmIdx -lt $script:ppmBytes.Length -and -not ([char]$script:ppmBytes[$script:ppmIdx] -match "\s")) { $script:ppmIdx++ }
        [System.Text.Encoding]::ASCII.GetString($script:ppmBytes, $start, $script:ppmIdx - $start)
    }

    $magic = Read-Token
    $width = [int](Read-Token)
    $height = [int](Read-Token)
    $maxValue = [int](Read-Token)
    if ($script:ppmIdx -lt $script:ppmBytes.Length -and [char]$script:ppmBytes[$script:ppmIdx] -match "\s") { $script:ppmIdx++ }

    $expectedPayload = $width * $height * 3
    $payload = $script:ppmBytes.Length - $script:ppmIdx
    $nonblack = 0
    $nearVisible = 0
    $maxR = 0
    $maxG = 0
    $maxB = 0

    for ($i = 0; $i -lt [Math]::Min($expectedPayload, $payload); $i += 3) {
        $r = $script:ppmBytes[$script:ppmIdx + $i]
        $g = $script:ppmBytes[$script:ppmIdx + $i + 1]
        $b = $script:ppmBytes[$script:ppmIdx + $i + 2]
        if (($r -ne 0) -or ($g -ne 0) -or ($b -ne 0)) { $nonblack++ }
        if (($r -gt 32) -or ($g -gt 32) -or ($b -gt 32)) { $nearVisible++ }
        if ($r -gt $maxR) { $maxR = $r }
        if ($g -gt $maxG) { $maxG = $g }
        if ($b -gt $maxB) { $maxB = $b }
    }

    [pscustomobject]@{
        Magic = $magic
        Width = $width
        Height = $height
        MaxValue = $maxValue
        Payload = $payload
        ExpectedPayload = $expectedPayload
        Complete = ($payload -eq $expectedPayload)
        Nonblack = $nonblack
        NearVisible = $nearVisible
        MaxR = $maxR
        MaxG = $maxG
        MaxB = $maxB
    }
}

if (-not $SkipBuild) {
    $msbuild = Find-MSBuild
    & $msbuild (Join-Path $repoRoot "code\projects\windows\imm.sln") `
        -t:appImmViewer `
        -p:Configuration=$Configuration `
        -p:Platform=$Platform `
        -p:TrackFileAccess=false `
        -p:CL_MPCount=1 `
        -nr:false `
        -v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path $viewerExe)) {
    throw "Viewer executable was not found: $viewerExe"
}
if (-not (Test-Path $settingsPath)) {
    throw "Vulkan smoke settings were not found: $settingsPath"
}

Remove-Item -LiteralPath $debugLog, $capturePath, $captureTmpPath -Force -ErrorAction SilentlyContinue

$env:IMM_VULKAN_CPU_CAPTURE_PATH = $capturePath
$env:IMM_VULKAN_CPU_CAPTURE_OVERWRITE = "1"

$process = Start-Process -FilePath $viewerExe -ArgumentList @("settings-vulkan-smoke.json") -WorkingDirectory $viewerExeDir -WindowStyle Hidden -PassThru
Start-Sleep -Seconds $DurationSeconds
if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    Start-Sleep -Seconds 1
}

if (-not (Test-Path $debugLog)) {
    throw "Viewer did not write debug log: $debugLog"
}

$log = Get-Content $debugLog -Raw
if ($log -notmatch "Loaded in CPU") {
    throw "Smoke log does not show CPU load completion."
}
if ($log -notmatch "Loaded in GPU \[0\]") {
    throw "Smoke log does not show GPU load completion."
}
if ($log -notmatch "Vulkan renderer presented swapchain direct GPU texture frame") {
    throw "Smoke log does not show direct GPU Vulkan presentation."
}
if ($log -notmatch "Vulkan renderer read back static paint GPU target nonblack=") {
    throw "Smoke log does not show Vulkan GPU target readback."
}

$badPatterns = @(
    "Vulkan renderer failed",
    "Vulkan draw submission is not implemented yet",
    "Vulkan render target .*not implemented",
    "placeholder"
)
foreach ($pattern in $badPatterns) {
    if ($log -match $pattern) {
        throw "Smoke log matched failure pattern: $pattern"
    }
}

$stats = Read-PpmStats $capturePath
if ($stats.Magic -ne "P6" -or $stats.Width -ne 1280 -or $stats.Height -ne 720 -or $stats.MaxValue -ne 255) {
    throw "Unexpected PPM header: $($stats.Magic) $($stats.Width)x$($stats.Height) max=$($stats.MaxValue)"
}
if (-not $stats.Complete) {
    throw "PPM payload is incomplete: $($stats.Payload) of $($stats.ExpectedPayload)"
}
if ($stats.Nonblack -lt $MinNonblackPixels) {
    throw "PPM nonblack count $($stats.Nonblack) is below threshold $MinNonblackPixels"
}
if ($stats.NearVisible -lt $MinNearVisiblePixels) {
    throw "PPM near-visible count $($stats.NearVisible) is below threshold $MinNearVisiblePixels"
}

Write-Host "Vulkan sample1 smoke passed"
Write-Host "Capture: $capturePath"
Write-Host "Pixels: nonblack=$($stats.Nonblack) nearVisible=$($stats.NearVisible) maxRGB=$($stats.MaxR),$($stats.MaxG),$($stats.MaxB)"

if (-not $KeepArtifacts) {
    Remove-Item -LiteralPath $capturePath, $captureTmpPath -Force -ErrorAction SilentlyContinue
}
