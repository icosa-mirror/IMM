param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$GodotExe = $env:GODOT_EXE,

    [string]$ReferencePath = "",

    [string]$CapturePath = "",

    [string]$RenderCapturePath = "",

    [string]$LogDir = "",

    [ValidateSet("render_only", "full_depth", "ordered_overlay")]
    [string]$CompositionMode = "full_depth",

    [int]$PlayerFrame = 60,

    [double]$MaxMeanAbsoluteError = 35.0,

    [double]$MaxRootMeanSquareError = 50.0,

    [double]$MinVisibleOverlap = 0.95,

    [int]$TimeoutSeconds = 90,

    [switch]$GenerateReference,

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Resolve-GodotExe([string]$requested) {
    if ($requested -and (Test-Path $requested)) {
        return (Resolve-Path $requested).Path
    }

    foreach ($name in @("godot.exe", "godot")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }

    throw "Godot executable not found. Pass -GodotExe, set GODOT_EXE, or add Godot to PATH."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$sampleProject = Join-Path $repoRoot "code\ImmGodotSampleProject"
$variant = if ($Configuration -eq "Debug") { "debug" } else { "release" }
$extensionDir = Join-Path $sampleProject "addons\imm_viewer\bin\windows\$variant"
$editorExtensionDir = Join-Path $sampleProject "addons\imm_viewer\bin\windows\debug"

if (-not $ReferencePath) {
    $ReferencePath = Join-Path $repoRoot "build\baseline-captures\windows-directx-static.ppm"
}
if (-not $LogDir) {
    $LogDir = Join-Path $repoRoot "build\logs\godot-vulkan-visual-baseline-smoke"
}
$LogDir = (New-Item -ItemType Directory -Force $LogDir).FullName
if (-not $CapturePath) {
    $CapturePath = Join-Path $LogDir "godot-vulkan-visual.ppm"
}
if (-not $RenderCapturePath) {
    $RenderCapturePath = Join-Path $LogDir "godot-vulkan-render.ppm"
}
$ReferencePath = [System.IO.Path]::GetFullPath($ReferencePath)
$CapturePath = [System.IO.Path]::GetFullPath($CapturePath)
$RenderCapturePath = [System.IO.Path]::GetFullPath($RenderCapturePath)

if (-not $SkipBuild) {
    & (Join-Path $scriptDir "build-godot-extension.ps1") -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Godot extension build failed with exit code $LASTEXITCODE"
    }
}

if ($GenerateReference -or -not (Test-Path -LiteralPath $ReferencePath -PathType Leaf)) {
    & (Join-Path $repoRoot "code\appImmViewer\scripts\capture_windows_directx_baseline.ps1") `
        -Configuration "Release" `
        -OutputPath $ReferencePath `
        -PlayerFrame $PlayerFrame `
        -SkipBuild:$SkipBuild
}

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
foreach ($dll in $requiredDlls) {
    $candidate = Join-Path $extensionDir $dll
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Godot GDExtension runtime DLL is missing: $candidate"
    }
}
if ($editorExtensionDir -ne $extensionDir) {
    New-Item -ItemType Directory -Force $editorExtensionDir | Out-Null
    foreach ($dll in $requiredDlls) {
        Copy-Item -Force (Join-Path $extensionDir $dll) (Join-Path $editorExtensionDir $dll)
    }
}

Remove-Item -LiteralPath $CapturePath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $RenderCapturePath -Force -ErrorAction SilentlyContinue

$godot = Resolve-GodotExe $GodotExe
$outputPath = Join-Path $LogDir "godot-visual-baseline-output.log"
$compositionStatusPath = Join-Path $LogDir "composition-status.json"
$previousEnv = @{
    PATH = $env:PATH
    IMM_GODOT_VISUAL_SMOKE = $env:IMM_GODOT_VISUAL_SMOKE
    IMM_GODOT_VISUAL_RENDERER_API = $env:IMM_GODOT_VISUAL_RENDERER_API
    IMM_GODOT_VISUAL_SMOKE_PPM = $env:IMM_GODOT_VISUAL_SMOKE_PPM
    IMM_GODOT_VISUAL_SMOKE_RENDER_PPM = $env:IMM_GODOT_VISUAL_SMOKE_RENDER_PPM
    IMM_GODOT_VISUAL_SMOKE_PLAYER_FRAME = $env:IMM_GODOT_VISUAL_SMOKE_PLAYER_FRAME
    IMM_GODOT_VISUAL_SMOKE_USE_SPAWN_AREA = $env:IMM_GODOT_VISUAL_SMOKE_USE_SPAWN_AREA
    IMM_GODOT_VISUAL_SMOKE_COMPOSITION_MODE = $env:IMM_GODOT_VISUAL_SMOKE_COMPOSITION_MODE
    IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES = $env:IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES
    IMM_GODOT_DIRECT_VULKAN_DEPTH_COMPOSITION = $env:IMM_GODOT_DIRECT_VULKAN_DEPTH_COMPOSITION
    IMM_GODOT_RENDER_GRAPH_VULKAN_DEPTH_COMPOSITION = $env:IMM_GODOT_RENDER_GRAPH_VULKAN_DEPTH_COMPOSITION
    IMM_VIEWER_VALIDATE_FIXED_DT = $env:IMM_VIEWER_VALIDATE_FIXED_DT
    IMM_VIEWER_VALIDATE_PLAYER_FRAME = $env:IMM_VIEWER_VALIDATE_PLAYER_FRAME
}

try {
    $env:PATH = "$extensionDir;$env:PATH"
    $env:IMM_GODOT_VISUAL_SMOKE = "1"
    $env:IMM_GODOT_VISUAL_RENDERER_API = "5"
    $env:IMM_GODOT_VISUAL_SMOKE_PPM = $CapturePath
    $env:IMM_GODOT_VISUAL_SMOKE_RENDER_PPM = $RenderCapturePath
    $env:IMM_GODOT_VISUAL_SMOKE_PLAYER_FRAME = "$PlayerFrame"
    $env:IMM_GODOT_VISUAL_SMOKE_USE_SPAWN_AREA = "1"
    $env:IMM_GODOT_VISUAL_SMOKE_COMPOSITION_MODE = $CompositionMode
    $env:IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES = "0"
    if ($CompositionMode -eq "full_depth") {
        Remove-Item -Path "env:IMM_GODOT_DIRECT_VULKAN_DEPTH_COMPOSITION" -ErrorAction SilentlyContinue
        $env:IMM_GODOT_RENDER_GRAPH_VULKAN_DEPTH_COMPOSITION = "1"
    }
    else {
        Remove-Item -Path "env:IMM_GODOT_DIRECT_VULKAN_DEPTH_COMPOSITION" -ErrorAction SilentlyContinue
        Remove-Item -Path "env:IMM_GODOT_RENDER_GRAPH_VULKAN_DEPTH_COMPOSITION" -ErrorAction SilentlyContinue
    }
    $env:IMM_VIEWER_VALIDATE_FIXED_DT = "0.0333333333333333"
    $env:IMM_VIEWER_VALIDATE_PLAYER_FRAME = "$PlayerFrame"

    $godotArgs = @(
        "--resolution", "1280x720",
        "--fixed-fps", "30",
        "--path", $sampleProject,
        "res://scenes/VisualSmokeScene.tscn"
    )
    $stdoutPath = Join-Path $LogDir "godot-visual-baseline.stdout.log"
    $stderrPath = Join-Path $LogDir "godot-visual-baseline.stderr.log"
    $process = Start-Process -FilePath $godot `
        -ArgumentList $godotArgs `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            Start-Sleep -Seconds 1
        }
        throw "Godot Vulkan visual baseline smoke timed out after $TimeoutSeconds seconds"
    }
    $exitCode = $process.ExitCode
    $output = @()
    if (Test-Path -LiteralPath $stdoutPath) {
        $output += Get-Content -LiteralPath $stdoutPath
    }
    if (Test-Path -LiteralPath $stderrPath) {
        $output += Get-Content -LiteralPath $stderrPath
    }
}
finally {
    foreach ($entry in $previousEnv.GetEnumerator()) {
        if ($null -eq $entry.Value) {
            Remove-Item -Path "env:$($entry.Key)" -ErrorAction SilentlyContinue
        }
        else {
            Set-Item -Path "env:$($entry.Key)" -Value $entry.Value
        }
    }
}

$output | Tee-Object -FilePath $outputPath
$outputText = $output -join "`n"
$compositionFailures = @($output | Where-Object { $_ -match "scene composition .* failed" })
$hasSceneCompositionDiagnostics = $outputText.Contains("visual smoke scene composition diagnostics") -or $outputText.Contains("visual smoke PPM scene composition diagnostics")
$hasOrderedOverlayDiagnostics = $outputText.Contains("ordered overlay IMM diagnostics")
if ($CompositionMode -eq "full_depth" -and $compositionFailures.Count -eq 0 -and -not $hasSceneCompositionDiagnostics) {
    $compositionFailures += "scene composition full depth probe missing failed"
}
if ($CompositionMode -eq "ordered_overlay" -and $compositionFailures.Count -eq 0 -and -not $hasOrderedOverlayDiagnostics) {
    $compositionFailures += "scene composition ordered overlay probe missing failed"
}
$compositionContract = if ($CompositionMode -eq "ordered_overlay") {
    "ordered_overlay"
} elseif ($CompositionMode -eq "full_depth") {
    "depth_composition"
} else {
    "render_only"
}
$hasRequiredCaptures = (Test-Path -LiteralPath $CapturePath -PathType Leaf) -and
    (Test-Path -LiteralPath $RenderCapturePath -PathType Leaf)
# Godot returns a non-zero process code when an in-scene composition probe
# fails. That is not evidence that the render itself failed: the two captures
# are the render evidence and are checked by the shared visual metrics step.
$renderingStatus = if ($hasRequiredCaptures) { "success" } else { "failed" }
$compositingStatus = if ($CompositionMode -eq "render_only") {
    "not_tested"
} elseif ($compositionFailures.Count -gt 0) {
    "failed"
} elseif ($renderingStatus -eq "success") {
    "success"
} else {
    "unknown"
}

$compositionStatus = [ordered]@{
    schema = "imm-composition-status-v1"
    rendering = $renderingStatus
    composition_mode = $CompositionMode
    composition_contract = $compositionContract
    compositing = $compositingStatus
    ordered_overlay = if ($CompositionMode -eq "ordered_overlay") { $compositingStatus } else { "not_tested" }
    depth_composition = if ($CompositionMode -eq "render_only") { "not_tested" } elseif ($CompositionMode -eq "ordered_overlay") { "not_claimed" } elseif ($compositionFailures.Count -gt 0) { "failed" } else { "success" }
    depth_interleaving = if ($CompositionMode -eq "render_only") { "not_claimed" } elseif ($CompositionMode -eq "ordered_overlay") { "not_claimed" } elseif ($compositionFailures.Count -gt 0) { "failed" } else { "success" }
    expected = $false
    failure_class = if ($compositionFailures.Count -gt 0) { "compositing" } else { "" }
    failures = $compositionFailures
}
$compositionStatus | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 -LiteralPath $compositionStatusPath

if ($exitCode -ne 0) {
    throw "Godot Vulkan visual baseline smoke failed with exit code $exitCode"
}
if (-not $outputText.Contains("IMM Godot Vulkan visual smoke passed")) {
    throw "Godot Vulkan visual baseline smoke did not print the success marker."
}
if (-not (Test-Path -LiteralPath $CapturePath -PathType Leaf)) {
    throw "Godot Vulkan visual baseline smoke did not write capture: $CapturePath"
}
if (-not (Test-Path -LiteralPath $RenderCapturePath -PathType Leaf)) {
    throw "Godot Vulkan visual baseline smoke did not write render baseline capture: $RenderCapturePath"
}

if ($CompositionMode -eq "render_only") {
    Write-Output "Render-only baseline comparison is performed by the shared render-metrics contract."
}
else {
    Write-Output "Composition validation for $CompositionMode uses scene probes; render fidelity is validated from the separate render-only capture."
}
