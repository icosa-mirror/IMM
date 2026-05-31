param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$GodotExe = $env:GODOT_EXE,

    [string]$SmokeScene = $env:IMM_GODOT_SMOKE_SCENE,

    [string]$LogDir = $env:IMM_GODOT_SMOKE_LOG_DIR,

    [int]$LoadUnloadCycles = 0,

    [ValidateRange(0, 5)]
    [int]$RendererApi = 0,

    [switch]$RequireExtension,

    [switch]$PreflightOnly
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
$smokeScript = "res://scripts/smoke_test_runner.gd"
$variant = if ($Configuration -eq "Debug") { "debug" } else { "release" }
$extensionDir = Join-Path $sampleProject "addons\imm_viewer\bin\windows\$variant"
$extensionDll = Join-Path $extensionDir "imm_godot_extension.dll"
$editorVariant = if ($Configuration -eq "Release") { "debug" } else { $variant }
$editorExtensionDir = Join-Path $sampleProject "addons\imm_viewer\bin\windows\$editorVariant"
$editorExtensionDll = Join-Path $editorExtensionDir "imm_godot_extension.dll"

if ($LogDir) {
    $LogDir = (New-Item -ItemType Directory -Force $LogDir).FullName
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
    $missingDlls = @()
    foreach ($dll in $requiredDlls) {
        $candidate = Join-Path $extensionDir $dll
        if (-not (Test-Path $candidate)) {
            $missingDlls += $candidate
        }
    }
    if ($LogDir) {
        $inventory = @("Expected staged DLLs:")
        foreach ($dll in $requiredDlls) {
            $candidate = Join-Path $extensionDir $dll
            if (Test-Path $candidate) {
                $item = Get-Item $candidate
                $inventory += ("FOUND`t{0}`t{1}`t{2:o}" -f $item.Name, $item.Length, $item.LastWriteTimeUtc)
            }
            else {
                $inventory += ("MISSING`t{0}" -f $dll)
            }
        }
        $inventory | Out-File -FilePath (Join-Path $LogDir "godot-extension-dlls.txt") -Encoding utf8
    }
    if ($missingDlls.Count -gt 0) {
        throw "Godot GDExtension runtime DLLs are missing:`n  $($missingDlls -join "`n  ")"
    }

    if ($editorExtensionDir -ne $extensionDir) {
        New-Item -ItemType Directory -Force $editorExtensionDir | Out-Null
        foreach ($dll in $requiredDlls) {
            Copy-Item -Force (Join-Path $extensionDir $dll) (Join-Path $editorExtensionDir $dll)
        }
        Write-Host "Mirrored $Configuration GDExtension DLLs for Godot editor feature lookup: $editorExtensionDir"
    }
}

if (-not $SmokeScene) {
    $SmokeScene = if ($RequireExtension) { "res://scenes/NativeSmokeScene.tscn" } else { "res://scenes/SampleScene.tscn" }
}

$godot = Resolve-GodotExe $GodotExe
Write-Host "Using Godot: $godot"
Write-Host "Running smoke script: $smokeScript"
Write-Host "Smoke scene: $SmokeScene"
Write-Host "GDExtension configuration: $Configuration"
Write-Host "Load/unload cycles: $LoadUnloadCycles"
Write-Host "Renderer API: $RendererApi"
if ($RequireExtension) {
    Write-Host "GDExtension directory: $extensionDir"
    Write-Host "Godot editor extension directory: $editorExtensionDir"
}
if ($LogDir) {
    Write-Host "Smoke log directory: $LogDir"
}

if ($PreflightOnly) {
    Write-Host "Godot smoke preflight passed."
    return
}

$env:IMM_GODOT_SMOKE_SCENE = $SmokeScene
$env:IMM_GODOT_EXPECT_NATIVE = if ($RequireExtension) { "1" } else { "0" }
$env:IMM_GODOT_LOAD_UNLOAD_CYCLES = "$LoadUnloadCycles"
$env:IMM_GODOT_RENDERER_API = "$RendererApi"
$output = & $godot --headless --path $sampleProject --script $smokeScript 2>&1
$exitCode = $LASTEXITCODE
$output | ForEach-Object { Write-Host $_ }
$successMarker = "IMM Godot smoke test passed"
$hasSuccessMarker = ($output -join "`n").Contains($successMarker)
if ($LogDir) {
    $output | Out-File -FilePath (Join-Path $LogDir "godot-smoke-output.log") -Encoding utf8
    $summary = @(
        "Godot=$godot",
        "SampleProject=$sampleProject",
        "SmokeScript=$smokeScript",
        "SmokeScene=$SmokeScene",
        "Configuration=$Configuration",
        "RequireExtension=$($RequireExtension.IsPresent)",
        "LoadUnloadCycles=$LoadUnloadCycles",
        "RendererApi=$RendererApi",
        "ExtensionDir=$extensionDir",
        "ExtensionDll=$extensionDll",
        "EditorExtensionDir=$editorExtensionDir",
        "EditorExtensionDll=$editorExtensionDll",
        "SuccessMarker=$successMarker",
        "HasSuccessMarker=$hasSuccessMarker",
        "ExitCode=$exitCode"
    )
    $summary | Out-File -FilePath (Join-Path $LogDir "godot-smoke-summary.txt") -Encoding utf8
}
if ($exitCode -ne 0) {
    throw "Godot smoke test failed with exit code $exitCode"
}
if (-not $hasSuccessMarker) {
    throw "Godot smoke test exited successfully but did not print success marker: $successMarker"
}
