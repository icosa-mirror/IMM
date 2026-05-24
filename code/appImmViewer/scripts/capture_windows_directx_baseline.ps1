param(
    [string]$ViewerExe = "",
    [string]$SettingsPath = "",
    [string]$SamplePath = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

function Resolve-RequiredFile([string]$PathValue, [string]$Description) {
    if (-not $PathValue) {
        throw "$Description path is empty"
    }
    $resolved = Resolve-Path -LiteralPath $PathValue -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "$Description is not a file: $PathValue"
    }
    return $resolved.Path
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRootCandidate = Join-Path $scriptDir "..\..\.."
$repoRoot = ""
if (Test-Path -LiteralPath (Join-Path $repoRootCandidate "code\appImmViewer\exe") -PathType Container) {
    $repoRoot = (Resolve-Path -LiteralPath $repoRootCandidate).Path
}

if (-not $ViewerExe) {
    if ($repoRoot) {
        $ViewerExe = Join-Path $repoRoot "code\appImmViewer\exe\appImmViewer_Release.exe"
    }
    if (-not $ViewerExe -or -not (Test-Path -LiteralPath $ViewerExe -PathType Leaf)) {
        $artifactViewer = Join-Path $scriptDir "appImmViewer_Release.exe"
        $cwdViewer = Join-Path (Get-Location) "appImmViewer_Release.exe"
        if (Test-Path -LiteralPath $artifactViewer -PathType Leaf) {
            $ViewerExe = $artifactViewer
        } elseif (Test-Path -LiteralPath $cwdViewer -PathType Leaf) {
            $ViewerExe = $cwdViewer
        }
    }
}

if (-not $SettingsPath) {
    if ($repoRoot) {
        $SettingsPath = Join-Path $repoRoot "code\appImmViewer\exe\settings-baseline-directx-sample1.json"
    }
    if (-not $SettingsPath -or -not (Test-Path -LiteralPath $SettingsPath -PathType Leaf)) {
        $artifactSettings = Join-Path $scriptDir "settings-baseline-directx-sample1.json"
        $cwdSettings = Join-Path (Get-Location) "settings-baseline-directx-sample1.json"
        if (Test-Path -LiteralPath $artifactSettings -PathType Leaf) {
            $SettingsPath = $artifactSettings
        } elseif (Test-Path -LiteralPath $cwdSettings -PathType Leaf) {
            $SettingsPath = $cwdSettings
        }
    }
}

if (-not $SamplePath) {
    if ($repoRoot) {
        $SamplePath = Join-Path $repoRoot "exampleImmFiles\sample1.imm"
    }
    if (-not $SamplePath -or -not (Test-Path -LiteralPath $SamplePath -PathType Leaf)) {
        $artifactSample = Join-Path $scriptDir "sample1.imm"
        $cwdSample = Join-Path (Get-Location) "sample1.imm"
        if (Test-Path -LiteralPath $artifactSample -PathType Leaf) {
            $SamplePath = $artifactSample
        } elseif (Test-Path -LiteralPath $cwdSample -PathType Leaf) {
            $SamplePath = $cwdSample
        }
    }
}

if (-not $OutputPath) {
    if ($repoRoot) {
        $OutputPath = Join-Path $repoRoot "build\baseline-captures\windows-directx-static.png"
    } else {
        $OutputPath = Join-Path $scriptDir "baseline-captures\windows-directx-static.png"
    }
}

$ViewerExe = Resolve-RequiredFile $ViewerExe "Viewer executable"
$SettingsPath = Resolve-RequiredFile $SettingsPath "DirectX baseline settings"
$SamplePath = Resolve-RequiredFile $SamplePath "sample1.imm"

$outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
$outputDir = Split-Path -Parent $outputFullPath
New-Item -ItemType Directory -Force $outputDir | Out-Null
if (Test-Path -LiteralPath $outputFullPath) {
    Remove-Item -LiteralPath $outputFullPath -Force
}

$runtimeSettingsPath = Join-Path $outputDir "settings-baseline-directx-sample1.runtime.json"
$runtimeSettings = Get-Content -LiteralPath $SettingsPath -Raw | ConvertFrom-Json
if (-not $runtimeSettings.PSObject.Properties["File"]) {
    $runtimeSettings | Add-Member -MemberType NoteProperty -Name "File" -Value ([pscustomobject]@{})
}
if ($runtimeSettings.File.PSObject.Properties["Load"]) {
    $runtimeSettings.File.Load = @($SamplePath)
} else {
    $runtimeSettings.File | Add-Member -MemberType NoteProperty -Name "Load" -Value @($SamplePath)
}
$runtimeSettings | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $runtimeSettingsPath -Encoding UTF8

$env:IMM_VIEWER_VALIDATE_FRAME = "1"
$env:IMM_VIEWER_VALIDATE_MAX_FRAME = "240"
$env:IMM_VIEWER_VALIDATE_MIN_NONZERO = "16"
$env:IMM_VIEWER_VALIDATE_MIN_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_TRIANGLES = "1"
$env:IMM_VIEWER_VALIDATE_CAPTURE_PATH = $outputFullPath

Write-Host "Running Windows DirectX baseline capture..."
Write-Host "Viewer:   $ViewerExe"
Write-Host "Settings: $runtimeSettingsPath"
Write-Host "Sample:   $SamplePath"
Write-Host "Output:   $outputFullPath"

& $ViewerExe $runtimeSettingsPath
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "Windows DirectX baseline capture failed with exit code $exitCode"
}

if (-not (Test-Path -LiteralPath $outputFullPath -PathType Leaf)) {
    throw "Windows DirectX baseline capture did not produce output: $outputFullPath"
}

Write-Host "Windows DirectX baseline capture written: $outputFullPath"
