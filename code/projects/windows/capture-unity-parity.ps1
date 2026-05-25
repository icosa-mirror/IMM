param(
    [string]$UnityExe = $env:UNITY_EXE,
    [string]$DocumentPath = "",
    [string]$LogDir = "artifacts\unity-parity",
    [switch]$SyncBuiltPlugins
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force $LogDir | Out-Null

function Write-UnityParitySummary {
    param(
        [bool]$Passed,
        [string]$Phase,
        [string]$UnityExeValue = "",
        [string]$DocumentPathValue = "",
        [object]$ExitCode = $null,
        [bool]$HasDiagnostics = $false,
        [bool]$SyncedPlugins = $false,
        [string]$UnityVersionValue = "",
        [string]$DiagnosticsError = ""
    )

    @(
        "Passed=$Passed",
        "Phase=$Phase",
        "UnityExe=$UnityExeValue",
        "DocumentPath=$DocumentPathValue",
        "ExitCode=$ExitCode",
        "HasDiagnostics=$HasDiagnostics",
        "SyncedPlugins=$SyncedPlugins",
        "UnityVersion=$UnityVersionValue",
        "DiagnosticsError=$DiagnosticsError",
        "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))"
    ) | Set-Content -Path (Join-Path $LogDir "unity-parity-summary.txt") -Encoding UTF8

    [pscustomobject]@{
        passed = $Passed
        phase = $Phase
        unity_exe = $UnityExeValue
        document_path = $DocumentPathValue
        exit_code = $ExitCode
        has_diagnostics = $HasDiagnostics
        synced_plugins = $SyncedPlugins
        unity_version = $UnityVersionValue
        diagnostics_error = $DiagnosticsError
        generated_utc = (Get-Date).ToUniversalTime().ToString('o')
    } |
        ConvertTo-Json -Depth 8 |
        Set-Content -Path (Join-Path $LogDir "unity-parity-summary.json") -Encoding UTF8
}

if ([string]::IsNullOrWhiteSpace($UnityExe)) {
    $candidates = @(
        "${env:ProgramFiles}\Unity\Hub\Editor\*\Editor\Unity.exe",
        "${env:ProgramFiles(x86)}\Unity\Hub\Editor\*\Editor\Unity.exe"
    )
    $UnityExe = Get-ChildItem $candidates -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if ([string]::IsNullOrWhiteSpace($UnityExe) -or -not (Test-Path $UnityExe)) {
    $preflightError = "Set UNITY_EXE or pass -UnityExe with the Unity editor executable path."
    Write-UnityParitySummary -Passed $false -Phase "preflight" -UnityExeValue $UnityExe -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $preflightError
    throw $preflightError
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$projectDir = Join-Path $repoRoot "code\ImmUnitySampleProject"
if ($SyncBuiltPlugins) {
    $unityPkgWin = Join-Path $projectDir "Packages\com.immersive-foundation.imm-unity\Plugins\x86_64"
    New-Item -ItemType Directory -Force $unityPkgWin | Out-Null

    $unityPluginCandidates = @(
        (Join-Path $repoRoot "code\appImmUnity\exe\ImmUnityPlugin.dll"),
        (Join-Path $repoRoot "code\appImmUnity\exe\Release\ImmUnityPlugin.dll")
    )
    $unityPluginSource = $unityPluginCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $unityPluginSource) {
        $pluginError = "ImmUnityPlugin.dll was not found for parity capture plugin sync. Checked: $($unityPluginCandidates -join ', ')"
        Write-UnityParitySummary -Passed $false -Phase "plugin_sync" -UnityExeValue $UnityExe -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $pluginError
        throw $pluginError
    }

    $unityDllMap = @{
        "ImmUnityPlugin.dll" = $unityPluginSource
        "Audio360.dll"       = (Join-Path $repoRoot "thirdparty\audio360-sdk\Audio360\Windows\x64\Audio360.dll")
        "opus.dll"           = (Join-Path $repoRoot "thirdparty\opus\bin\opus.dll")
        "opusenc.dll"        = (Join-Path $repoRoot "thirdparty\libopusenc\bin\opusenc.dll")
        "vorbisenc.dll"      = (Join-Path $repoRoot "thirdparty\libvorbis\bin\vorbisenc.dll")
    }

    foreach ($name in $unityDllMap.Keys) {
        $src = $unityDllMap[$name]
        if (-not (Test-Path $src)) {
            $pluginError = "Required Unity parity plugin DLL source is missing: $src"
            Write-UnityParitySummary -Passed $false -Phase "plugin_sync" -UnityExeValue $UnityExe -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $pluginError
            throw $pluginError
        }
        Copy-Item $src (Join-Path $unityPkgWin $name) -Force
    }
    Write-Host "Synced Unity parity plugin DLLs to: $unityPkgWin"
}
if ([string]::IsNullOrWhiteSpace($DocumentPath)) {
    $DocumentPath = Join-Path $projectDir "Assets\StreamingAssets\sample1.imm"
}
if (-not (Test-Path $DocumentPath)) {
    $preflightError = "Unity parity document does not exist: $DocumentPath"
    Write-UnityParitySummary -Passed $false -Phase "preflight" -UnityExeValue $UnityExe -DocumentPathValue $DocumentPath -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $preflightError
    throw $preflightError
}

$logPath = Join-Path $LogDir "unity-parity-output.log"
$diagnosticsPath = Join-Path $LogDir "unity-matrix-diagnostics.log"
$resolvedDocumentPath = (Resolve-Path $DocumentPath).Path

$env:IMM_UNITY_PARITY_DOCUMENT = $resolvedDocumentPath
try {
    & $UnityExe `
        -batchmode `
        -quit `
        -nographics `
        -projectPath $projectDir `
        -executeMethod ImmPlayer.Editor.BuildAutomation.CaptureDeterministicParityDiagnostics `
        -logFile $logPath
    $exitCode = $LASTEXITCODE
}
finally {
    Remove-Item Env:\IMM_UNITY_PARITY_DOCUMENT -ErrorAction SilentlyContinue
}

if ($exitCode -ne 0) {
    $diagnosticsError = "Unity parity diagnostics capture failed with exit code $exitCode. See $logPath"
    Write-UnityParitySummary -Passed $false -Phase "unity_batch" -UnityExeValue $UnityExe -DocumentPathValue $resolvedDocumentPath -ExitCode $exitCode -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $diagnosticsError
    throw $diagnosticsError
}
if (-not (Test-Path $logPath)) {
    $diagnosticsError = "Unity parity diagnostics log was not written: $logPath"
    Write-UnityParitySummary -Passed $false -Phase "unity_batch" -UnityExeValue $UnityExe -DocumentPathValue $resolvedDocumentPath -ExitCode $exitCode -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $diagnosticsError
    throw $diagnosticsError
}

$logText = Get-Content $logPath -Raw
$diagnosticsLine = ($logText -split "`r?`n" | Where-Object { $_ -match "IMM_UNITY_MATRIX_DIAGNOSTICS_JSON " } | Select-Object -Last 1)
if ($null -eq $diagnosticsLine) {
    $diagnosticsError = "Unity parity diagnostics did not print IMM_UNITY_MATRIX_DIAGNOSTICS_JSON. See $logPath"
    Write-UnityParitySummary -Passed $false -Phase "diagnostics" -UnityExeValue $UnityExe -DocumentPathValue $resolvedDocumentPath -ExitCode $exitCode -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $diagnosticsError
    throw $diagnosticsError
}

$unityDiagnostics = $null
try {
    $diagnosticsJson = $diagnosticsLine -replace "^.*IMM_UNITY_MATRIX_DIAGNOSTICS_JSON ", ""
    $unityDiagnostics = $diagnosticsJson | ConvertFrom-Json
    if ([string]::IsNullOrWhiteSpace([string]$unityDiagnostics.unity_version)) {
        throw "Unity matrix diagnostics must contain unity_version."
    }
}
catch {
    $diagnosticsError = $_.Exception.Message
    Write-UnityParitySummary -Passed $false -Phase "diagnostics" -UnityExeValue $UnityExe -DocumentPathValue $resolvedDocumentPath -ExitCode $exitCode -HasDiagnostics $true -SyncedPlugins ([bool]$SyncBuiltPlugins) -DiagnosticsError $diagnosticsError
    throw $diagnosticsError
}

$diagnosticsLine | Set-Content -Path $diagnosticsPath -Encoding UTF8
Write-UnityParitySummary -Passed $true -Phase "complete" -UnityExeValue $UnityExe -DocumentPathValue $resolvedDocumentPath -ExitCode $exitCode -HasDiagnostics $true -SyncedPlugins ([bool]$SyncBuiltPlugins) -UnityVersionValue $unityDiagnostics.unity_version
Write-Host "Unity parity diagnostics: $diagnosticsPath"
