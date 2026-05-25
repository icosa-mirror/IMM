param(
    [string]$GodotExe = $env:GODOT_EXE,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$DocumentPath = "",
    [string]$LogDir = "",
    [switch]$RequireExtension
)

$ErrorActionPreference = "Stop"

function Assert-FloatArrayClose {
    param(
        [string]$Name,
        [object[]]$Actual,
        [double[]]$Expected,
        [double]$Tolerance = 0.0001
    )

    if ($Actual.Count -ne $Expected.Count) {
        throw "Godot matrix diagnostics $Name must contain $($Expected.Count) floats, got $($Actual.Count)."
    }

    $maxDelta = 0.0
    $maxIndex = -1
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        $actualValue = [double]$Actual[$index]
        $delta = [Math]::Abs($actualValue - $Expected[$index])
        if ($delta -gt $maxDelta) {
            $maxDelta = $delta
            $maxIndex = $index
        }
    }

    if ($maxDelta -gt $Tolerance) {
        throw "Godot matrix diagnostics $Name mismatch at index $maxIndex. MaxDelta=$maxDelta Tolerance=$Tolerance"
    }

    return [pscustomobject]@{
        Name = $Name
        MaxDelta = $maxDelta
        MaxIndex = $maxIndex
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$projectDir = Join-Path $repoRoot "code\ImmGodotSampleProject"
$extensionDir = Join-Path $projectDir "bin\windows\$($Configuration.ToLowerInvariant())"
$resolvedDocumentPath = ""

function Copy-GodotSmokeProjectArtifacts {
    param([string]$TargetLogDir)

    if ([string]::IsNullOrWhiteSpace($TargetLogDir)) {
        return
    }
    New-Item -ItemType Directory -Force $TargetLogDir | Out-Null
    $artifacts = @{
        "project.godot" = (Join-Path $projectDir "project.godot")
        "imm_viewer.gdextension" = (Join-Path $projectDir "addons\imm_viewer\imm_viewer.gdextension")
        "NativeSmokeScene.tscn" = (Join-Path $projectDir "scenes\NativeSmokeScene.tscn")
    }
    foreach ($artifact in $artifacts.GetEnumerator()) {
        if (Test-Path $artifact.Value) {
            Copy-Item $artifact.Value (Join-Path $TargetLogDir $artifact.Key) -Force
        }
    }
}

function Write-GodotSmokePreflightSummary {
    param(
        [string]$DiagnosticsError,
        [string]$DocumentPathValue = "",
        [string[]]$MissingRuntimeDlls = @()
    )

    if ([string]::IsNullOrWhiteSpace($LogDir)) {
        return
    }
    Copy-GodotSmokeProjectArtifacts -TargetLogDir $LogDir
    @(
        "Configuration=$Configuration",
        "RequireExtension=$RequireExtension",
        "DocumentPath=$DocumentPathValue",
        "ExitCode=",
        "HasSuccessMarker=False",
        "HasLifecycleMarker=False",
        "HasMatrixDiagnostics=False",
        "MissingRuntimeDlls=$($MissingRuntimeDlls -join ';')",
        "DiagnosticsError=$DiagnosticsError",
        "GodotExe=$GodotExe",
        "ProjectDir=$projectDir",
        "ExtensionDir=$extensionDir",
        "ProjectArtifacts=project.godot;imm_viewer.gdextension;NativeSmokeScene.tscn",
        "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))"
    ) | Set-Content -Path (Join-Path $LogDir "godot-smoke-summary.txt") -Encoding UTF8

    [pscustomobject]@{
        passed = $false
        phase = "preflight"
        configuration = $Configuration
        require_extension = [bool]$RequireExtension
        document_path = $DocumentPathValue
        has_success_marker = $false
        has_lifecycle_marker = $false
        has_matrix_diagnostics = $false
        missing_runtime_dlls = $MissingRuntimeDlls
        diagnostics_error = $DiagnosticsError
        godot_exe = $GodotExe
        project_dir = $projectDir
        extension_dir = $extensionDir
        project_artifacts = @("project.godot", "imm_viewer.gdextension", "NativeSmokeScene.tscn")
        generated_utc = (Get-Date).ToUniversalTime().ToString('o')
    } |
        ConvertTo-Json -Depth 8 |
        Set-Content -Path (Join-Path $LogDir "godot-smoke-summary.json") -Encoding UTF8
}

if ([string]::IsNullOrWhiteSpace($GodotExe)) {
    $preflightError = "Set GODOT_EXE or pass -GodotExe with the Godot 4 executable path."
    Write-GodotSmokePreflightSummary -DiagnosticsError $preflightError
    throw $preflightError
}
if (-not (Test-Path $GodotExe)) {
    $preflightError = "Godot executable does not exist: $GodotExe"
    Write-GodotSmokePreflightSummary -DiagnosticsError $preflightError
    throw $preflightError
}
if (-not [string]::IsNullOrWhiteSpace($DocumentPath)) {
    if (-not (Test-Path $DocumentPath)) {
        $preflightError = "Godot smoke document does not exist: $DocumentPath"
        Write-GodotSmokePreflightSummary -DiagnosticsError $preflightError -DocumentPathValue $DocumentPath
        throw $preflightError
    }
    $resolvedDocumentPath = (Resolve-Path $DocumentPath).Path
}
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    Copy-GodotSmokeProjectArtifacts -TargetLogDir $LogDir
}

if ($RequireExtension) {
    $requiredDlls = @(
        "imm_godot_extension.dll",
        "ImmGodotPlugin.dll",
        "Audio360.dll",
        "opus.dll",
        "opusenc.dll",
        "vorbisenc.dll",
        "zlib1.dll",
        "jpeg62.dll",
        "libpng16.dll",
        "ogg.dll",
        "vorbis.dll"
    )
    $missingRuntimeDlls = @()
    foreach ($dll in $requiredDlls) {
        $path = Join-Path $extensionDir $dll
        if (-not (Test-Path $path)) {
            $missingRuntimeDlls += $dll
        }
    }
    if ($missingRuntimeDlls.Count -gt 0) {
        if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
            $preflightError = "Godot GDExtension runtime DLLs are missing: $($missingRuntimeDlls -join ', ')"
            Write-GodotSmokePreflightSummary -DiagnosticsError $preflightError -DocumentPathValue $resolvedDocumentPath -MissingRuntimeDlls $missingRuntimeDlls
        }
        throw "Godot GDExtension runtime DLLs are missing: $($missingRuntimeDlls -join ', ')"
    }

    if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
        $manifestPath = Join-Path $LogDir "godot-extension-dlls.txt"
        $manifest = @(
            "Configuration=$Configuration",
            "ExtensionDir=$extensionDir",
            "ExpectedDllCount=$($requiredDlls.Count)",
            "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))",
            "DLLs:"
        )
        foreach ($dll in $requiredDlls) {
            $path = Join-Path $extensionDir $dll
            $item = Get-Item $path
            $hash = Get-FileHash -Algorithm SHA256 -Path $path
            $manifest += "FOUND`t$dll`t$($item.Length)`t$($item.LastWriteTimeUtc.ToString('o'))`tSHA256=$($hash.Hash)"
        }
        $manifest | Set-Content -Path $manifestPath -Encoding UTF8
    }
}

if ($RequireExtension) {
    $env:IMM_GODOT_REQUIRE_EXTENSION = "1"
}
else {
    Remove-Item Env:\IMM_GODOT_REQUIRE_EXTENSION -ErrorAction SilentlyContinue
}
if (-not [string]::IsNullOrWhiteSpace($resolvedDocumentPath)) {
    $env:IMM_GODOT_SMOKE_DOCUMENT = $resolvedDocumentPath
}
else {
    Remove-Item Env:\IMM_GODOT_SMOKE_DOCUMENT -ErrorAction SilentlyContinue
}

try {
    $output = & $GodotExe --headless --path $projectDir --script res://scripts/smoke_test_runner.gd 2>&1
    $exitCode = $LASTEXITCODE
}
finally {
    Remove-Item Env:\IMM_GODOT_REQUIRE_EXTENSION -ErrorAction SilentlyContinue
    Remove-Item Env:\IMM_GODOT_SMOKE_DOCUMENT -ErrorAction SilentlyContinue
}
$output | Write-Host
$outputText = $output -join "`n"
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    $outputText | Set-Content -Path (Join-Path $LogDir "godot-smoke-output.log") -Encoding UTF8
}
$hasSuccessMarker = $outputText -match "IMM Godot smoke test passed"
$hasLifecycleMarker = $outputText -match "IMM Godot smoke lifecycle cycles: 2"
$matrixDiagnosticsLine = ($output | Where-Object { $_ -match "^IMM_GODOT_MATRIX_DIAGNOSTICS_JSON " } | Select-Object -Last 1)
$matrixDiagnostics = $null
$documentToWorldCheck = $null
$worldToHeadCheck = $null
$projectionCheck = $null
$diagnosticsError = $null
if ($null -ne $matrixDiagnosticsLine) {
    try {
        $matrixDiagnosticsJson = $matrixDiagnosticsLine -replace "^IMM_GODOT_MATRIX_DIAGNOSTICS_JSON ", ""
        $matrixDiagnostics = $matrixDiagnosticsJson | ConvertFrom-Json

        if ($matrixDiagnostics.schema -ne "imm_godot_matrix_diagnostics_v1") {
            throw "Godot matrix diagnostics schema mismatch: $($matrixDiagnostics.schema)"
        }
        if ($matrixDiagnostics.camera_id -ne 1 -or $matrixDiagnostics.last_matrix_camera_id -ne 1) {
            throw "Godot matrix diagnostics camera id mismatch: camera_id=$($matrixDiagnostics.camera_id), last_matrix_camera_id=$($matrixDiagnostics.last_matrix_camera_id)"
        }
        if ($matrixDiagnostics.background_color.Count -ne 4) {
            throw "Godot matrix diagnostics must contain a 4-float background_color array."
        }
        if ($matrixDiagnostics.document_loading_state -lt 0 -or $matrixDiagnostics.document_playback_state -lt 0) {
            throw "Godot matrix diagnostics contain invalid document state."
        }
        if ($matrixDiagnostics.bounding_box_min.Count -ne 3 -or $matrixDiagnostics.bounding_box_max.Count -ne 3) {
            throw "Godot matrix diagnostics must contain 3-float bounding_box_min and bounding_box_max arrays."
        }
        if ($matrixDiagnostics.spawn_area_count -lt 0 -or $matrixDiagnostics.active_spawn_area_index -lt -1 -or $matrixDiagnostics.active_spawn_area_id -lt -1) {
            throw "Godot matrix diagnostics contain invalid spawn area state."
        }
        if ($matrixDiagnostics.document_to_world.Count -ne 16 -or $matrixDiagnostics.world_to_head.Count -ne 16 -or $matrixDiagnostics.projection.Count -ne 16) {
            throw "Godot matrix diagnostics must contain 16-float document_to_world, world_to_head, and projection arrays."
        }
        if ([string]::IsNullOrWhiteSpace([string]$matrixDiagnostics.document_name) -or [int64]$matrixDiagnostics.document_size_bytes -le 0) {
            throw "Godot matrix diagnostics must contain document_name and positive document_size_bytes."
        }
        if ([string]::IsNullOrWhiteSpace([string]$matrixDiagnostics.godot_version)) {
            throw "Godot matrix diagnostics must contain godot_version."
        }
        if ($matrixDiagnostics.rendering_method -ne "gl_compatibility") {
            throw "Godot smoke must run the Compatibility renderer path, got '$($matrixDiagnostics.rendering_method)'."
        }

        $expectedDocumentToWorld = @(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.5, 0.0, -0.25, 1.0
        )
        $expectedWorldToHead = @(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.25, 1.6, 6.0, 1.0
        )
        $expectedProjection = @(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, -1.0, -1.0,
            0.0, 0.0, -0.1, 0.0
        )
        $documentToWorldCheck = Assert-FloatArrayClose -Name "document_to_world" -Actual $matrixDiagnostics.document_to_world -Expected $expectedDocumentToWorld
        $worldToHeadCheck = Assert-FloatArrayClose -Name "world_to_head" -Actual $matrixDiagnostics.world_to_head -Expected $expectedWorldToHead
        $projectionCheck = Assert-FloatArrayClose -Name "projection" -Actual $matrixDiagnostics.projection -Expected $expectedProjection
    }
    catch {
        $diagnosticsError = $_.Exception.Message
    }
}
$nativeLogCandidates = @(
    (Join-Path $repoRoot "imm_player_log.txt"),
    (Join-Path $projectDir "imm_player_log.txt")
)
if (-not [string]::IsNullOrWhiteSpace($env:APPDATA)) {
    $nativeLogCandidates += (Join-Path $env:APPDATA "Godot\app_userdata\IMM Godot Sample\imm_godot_log.txt")
}
$nativeLogFiles = @()
foreach ($candidate in $nativeLogCandidates) {
    if (Test-Path $candidate) {
        $nativeLogFiles += (Resolve-Path $candidate).Path
    }
}

if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    if ($null -ne $matrixDiagnostics) {
        $matrixDiagnostics |
            ConvertTo-Json -Depth 8 |
            Set-Content -Path (Join-Path $LogDir "godot-matrix-diagnostics.json") -Encoding UTF8
    }
    foreach ($nativeLog in $nativeLogFiles) {
        Copy-Item $nativeLog (Join-Path $LogDir (Split-Path $nativeLog -Leaf)) -Force
    }
    @(
        "Configuration=$Configuration",
        "RequireExtension=$RequireExtension",
        "DocumentPath=$resolvedDocumentPath",
        "ExitCode=$exitCode",
        "HasSuccessMarker=$hasSuccessMarker",
        "HasLifecycleMarker=$hasLifecycleMarker",
        "HasMatrixDiagnostics=$($null -ne $matrixDiagnosticsLine)",
        "MatrixDiagnosticsSchema=$($matrixDiagnostics.schema)",
        "MatrixDiagnosticsCameraId=$($matrixDiagnostics.camera_id)",
        "MatrixDiagnosticsDocumentName=$($matrixDiagnostics.document_name)",
        "MatrixDiagnosticsDocumentSizeBytes=$($matrixDiagnostics.document_size_bytes)",
        "GodotVersion=$($matrixDiagnostics.godot_version)",
        "RenderingMethod=$($matrixDiagnostics.rendering_method)",
        "DocumentToWorldMaxDelta=$($documentToWorldCheck.MaxDelta)",
        "WorldToHeadMaxDelta=$($worldToHeadCheck.MaxDelta)",
        "ProjectionMaxDelta=$($projectionCheck.MaxDelta)",
        "DocumentLoadingState=$($matrixDiagnostics.document_loading_state)",
        "DocumentPlaybackState=$($matrixDiagnostics.document_playback_state)",
        "BoundingBoxValid=$($matrixDiagnostics.bounding_box_valid)",
        "SpawnAreaCount=$($matrixDiagnostics.spawn_area_count)",
        "ActiveSpawnAreaIndex=$($matrixDiagnostics.active_spawn_area_index)",
        "ActiveSpawnAreaId=$($matrixDiagnostics.active_spawn_area_id)",
        "NativeLogFiles=$($nativeLogFiles -join ';')",
        "DiagnosticsError=$diagnosticsError",
        "GodotExe=$GodotExe",
        "ProjectDir=$projectDir",
        "ExtensionDir=$extensionDir",
        "ProjectArtifacts=project.godot;imm_viewer.gdextension;NativeSmokeScene.tscn",
        "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))"
    ) | Set-Content -Path (Join-Path $LogDir "godot-smoke-summary.txt") -Encoding UTF8

    $smokePassed = ($exitCode -eq 0) -and $hasSuccessMarker -and $hasLifecycleMarker -and ($null -eq $diagnosticsError) -and ((-not $RequireExtension) -or ($null -ne $matrixDiagnosticsLine))
    [pscustomobject]@{
        passed = $smokePassed
        configuration = $Configuration
        require_extension = [bool]$RequireExtension
        document_path = $resolvedDocumentPath
        exit_code = $exitCode
        has_success_marker = $hasSuccessMarker
        has_lifecycle_marker = $hasLifecycleMarker
        has_matrix_diagnostics = ($null -ne $matrixDiagnosticsLine)
        matrix_diagnostics_schema = $matrixDiagnostics.schema
        matrix_diagnostics_camera_id = $matrixDiagnostics.camera_id
        matrix_diagnostics_document_name = $matrixDiagnostics.document_name
        matrix_diagnostics_document_size_bytes = $matrixDiagnostics.document_size_bytes
        godot_version = $matrixDiagnostics.godot_version
        rendering_method = $matrixDiagnostics.rendering_method
        document_to_world_max_delta = $documentToWorldCheck.MaxDelta
        world_to_head_max_delta = $worldToHeadCheck.MaxDelta
        projection_max_delta = $projectionCheck.MaxDelta
        document_loading_state = $matrixDiagnostics.document_loading_state
        document_playback_state = $matrixDiagnostics.document_playback_state
        bounding_box_valid = $matrixDiagnostics.bounding_box_valid
        spawn_area_count = $matrixDiagnostics.spawn_area_count
        active_spawn_area_index = $matrixDiagnostics.active_spawn_area_index
        active_spawn_area_id = $matrixDiagnostics.active_spawn_area_id
        native_log_files = $nativeLogFiles
        diagnostics_error = $diagnosticsError
        godot_exe = $GodotExe
        project_dir = $projectDir
        extension_dir = $extensionDir
        project_artifacts = @("project.godot", "imm_viewer.gdextension", "NativeSmokeScene.tscn")
        generated_utc = (Get-Date).ToUniversalTime().ToString('o')
    } |
        ConvertTo-Json -Depth 8 |
        Set-Content -Path (Join-Path $LogDir "godot-smoke-summary.json") -Encoding UTF8
}

if ($exitCode -ne 0) {
    throw "Godot smoke test failed with exit code $exitCode"
}
if (-not $hasSuccessMarker) {
    throw "Godot smoke test did not print the success marker."
}
if (-not $hasLifecycleMarker) {
    throw "Godot smoke test did not print the lifecycle coverage marker."
}
if ($RequireExtension -and $null -eq $matrixDiagnosticsLine) {
    throw "Godot native smoke did not print matrix diagnostics."
}
if ($null -ne $diagnosticsError) {
    throw $diagnosticsError
}
