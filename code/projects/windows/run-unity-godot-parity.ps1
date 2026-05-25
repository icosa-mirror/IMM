param(
    [string]$UnityExe = $env:UNITY_EXE,
    [string]$GodotExe = $env:GODOT_EXE,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$DocumentPath = "",
    [string]$LogRoot = "artifacts\parity",
    [switch]$RequireExtension,
    [switch]$SyncBuiltUnityPlugins
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$godotLogDir = Join-Path $LogRoot "godot-smoke"
$unityLogDir = Join-Path $LogRoot "unity-parity"
$comparisonLog = Join-Path $LogRoot "unity-godot-parity-comparison.log"
$comparisonJson = Join-Path $LogRoot "unity-godot-parity-comparison.json"
New-Item -ItemType Directory -Force $LogRoot | Out-Null
$resolvedDocumentPath = ""

function Write-ParitySummary {
    param(
        [bool]$Passed,
        [string]$Phase,
        [object]$ComparisonExitCode = $null,
        [string]$DiagnosticsError = ""
    )

    @(
        "Passed=$Passed",
        "Phase=$Phase",
        "Configuration=$Configuration",
        "RequireExtension=$RequireExtension",
        "SyncBuiltUnityPlugins=$SyncBuiltUnityPlugins",
        "DocumentPath=$resolvedDocumentPath",
        "GodotLogDir=$godotLogDir",
        "UnityLogDir=$unityLogDir",
        "ComparisonLog=$comparisonLog",
        "ComparisonJson=$comparisonJson",
        "ComparisonExitCode=$ComparisonExitCode",
        "DiagnosticsError=$DiagnosticsError",
        "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))"
    ) | Set-Content -Path (Join-Path $LogRoot "unity-godot-parity-summary.txt") -Encoding UTF8

    [pscustomobject]@{
        passed = $Passed
        phase = $Phase
        configuration = $Configuration
        require_extension = [bool]$RequireExtension
        sync_built_unity_plugins = [bool]$SyncBuiltUnityPlugins
        document_path = $resolvedDocumentPath
        godot_log_dir = $godotLogDir
        unity_log_dir = $unityLogDir
        comparison_log = $comparisonLog
        comparison_json = $comparisonJson
        comparison_exit_code = $ComparisonExitCode
        diagnostics_error = $DiagnosticsError
        generated_utc = (Get-Date).ToUniversalTime().ToString('o')
    } |
        ConvertTo-Json -Depth 8 |
        Set-Content -Path (Join-Path $LogRoot "unity-godot-parity-summary.json") -Encoding UTF8
}

if ([string]::IsNullOrWhiteSpace($DocumentPath)) {
    $defaultDocumentPath = Join-Path $repoRoot "exampleImmFiles\sample1.imm"
    if (-not (Test-Path $defaultDocumentPath)) {
        $preflightError = "Default IMM parity document does not exist: $defaultDocumentPath"
        Write-ParitySummary -Passed $false -Phase "preflight" -DiagnosticsError $preflightError
        throw $preflightError
    }
    $resolvedDocumentPath = (Resolve-Path $defaultDocumentPath).Path
}
else {
    if (-not (Test-Path $DocumentPath)) {
        $preflightError = "IMM parity document does not exist: $DocumentPath"
        Write-ParitySummary -Passed $false -Phase "preflight" -DiagnosticsError $preflightError
        throw $preflightError
    }
    $resolvedDocumentPath = (Resolve-Path $DocumentPath).Path
}

$godotArgs = @(
    "-Configuration", $Configuration,
    "-GodotExe", $GodotExe,
    "-LogDir", $godotLogDir,
    "-DocumentPath", $resolvedDocumentPath
)
if ($RequireExtension) {
    $godotArgs += "-RequireExtension"
}
try {
    & (Join-Path $PSScriptRoot "run-godot-smoke.ps1") @godotArgs
}
catch {
    $errorMessage = $_.Exception.Message
    Write-ParitySummary -Passed $false -Phase "godot_smoke" -DiagnosticsError $errorMessage
    throw
}

$unityArgs = @(
    "-UnityExe", $UnityExe,
    "-LogDir", $unityLogDir,
    "-DocumentPath", $resolvedDocumentPath
)
if ($SyncBuiltUnityPlugins) {
    $unityArgs += "-SyncBuiltPlugins"
}
try {
    & (Join-Path $PSScriptRoot "capture-unity-parity.ps1") @unityArgs
}
catch {
    $errorMessage = $_.Exception.Message
    Write-ParitySummary -Passed $false -Phase "unity_capture" -DiagnosticsError $errorMessage
    throw
}

$unityDiagnostics = Join-Path $unityLogDir "unity-matrix-diagnostics.log"
$godotDiagnostics = Join-Path $godotLogDir "godot-matrix-diagnostics.json"
$comparisonOutput = python (Join-Path $PSScriptRoot "compare-matrix-diagnostics.py") `
    --unity $unityDiagnostics `
    --godot $godotDiagnostics `
    --require-extended `
    --summary-json $comparisonJson 2>&1
$comparisonExitCode = $LASTEXITCODE
$comparisonOutput | Tee-Object -FilePath $comparisonLog

if ($comparisonExitCode -ne 0) {
    Write-ParitySummary -Passed $false -Phase "comparison" -ComparisonExitCode $comparisonExitCode -DiagnosticsError "Unity/Godot parity comparison failed. See $comparisonLog"
    throw "Unity/Godot parity comparison failed. See $comparisonLog"
}

Write-ParitySummary -Passed $true -Phase "complete" -ComparisonExitCode $comparisonExitCode
Write-Host "Unity/Godot parity comparison passed: $comparisonLog"
Write-Host "Unity/Godot parity comparison summary: $comparisonJson"
Write-Host "Unity/Godot parity phase summary: $(Join-Path $LogRoot 'unity-godot-parity-summary.json')"
