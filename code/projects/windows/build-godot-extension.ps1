param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [string]$GodotCppPath = $env:GODOT_CPP_PATH,

    [string]$GodotCppLib = $env:GODOT_CPP_LIB,

    [string]$GodotCppRef = $env:GODOT_CPP_REF,

    [switch]$BootstrapGodotCpp,

    [switch]$BuildGodotCpp,

    [switch]$VerifyOnly,

    [switch]$PreflightOnly,

    [switch]$RunSmoke,

    [switch]$HeadedSmoke,

    [ValidateRange(0, 5)]
    [int]$SmokeRendererApi = 0,

    [string]$GodotExe = $env:GODOT_EXE
)

$ErrorActionPreference = "Stop"

if (-not $GodotCppRef) {
    $GodotCppRef = "godot-4.5-stable"
}

function Resolve-MsBuildPath {
    function Test-CppToolchainForMsBuild([string]$msbuildPath) {
        if (-not $msbuildPath -or -not (Test-Path $msbuildPath)) {
            return $false
        }

        $msbuildDir = Split-Path -Parent $msbuildPath
        $msbuildRoot = Resolve-Path (Join-Path $msbuildDir "..\..")
        $cppProps = Join-Path $msbuildRoot "Microsoft\VC\v170\Microsoft.Cpp.Default.props"
        return Test-Path $cppProps
    }

    $preferredPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($preferred in $preferredPaths) {
        if (Test-CppToolchainForMsBuild $preferred) {
            return $preferred
        }
    }

    if ($env:MSBUILD_EXE_PATH -and (Test-Path $env:MSBUILD_EXE_PATH)) {
        if (Test-CppToolchainForMsBuild $env:MSBUILD_EXE_PATH) {
            return $env:MSBUILD_EXE_PATH
        }

        throw "MSBUILD_EXE_PATH is set but missing C++ build props: $env:MSBUILD_EXE_PATH"
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $candidates = & $vswhere -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"
        foreach ($candidate in $candidates) {
            if (Test-CppToolchainForMsBuild $candidate) {
                return $candidate
            }
        }
    }

    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd -and (Test-CppToolchainForMsBuild $cmd.Source)) {
        return $cmd.Source
    }

    throw "MSBuild.exe not found. Install Visual Studio Build Tools (Desktop development with C++) or set MSBUILD_EXE_PATH."
}

function Resolve-Git {
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        return $git.Source
    }

    throw "git not found. Install Git or pass an existing -GodotCppPath."
}

function Resolve-GodotCppPath([string]$requested, [string]$repoRoot, [bool]$bootstrap, [string]$ref, [bool]$preflight) {
    if ($requested -and (Test-Path $requested)) {
        return (Resolve-Path $requested).Path
    }

    $defaultPath = Join-Path $repoRoot "thirdparty\godot-cpp"
    if (Test-Path $defaultPath) {
        return (Resolve-Path $defaultPath).Path
    }

    if ($bootstrap -and $preflight) {
        Write-Warning "godot-cpp checkout not found at $defaultPath. A full run with -BootstrapGodotCpp will clone ref $ref."
        return $defaultPath
    }

    if (-not $bootstrap) {
        throw "godot-cpp checkout not found. Pass -GodotCppPath, set GODOT_CPP_PATH, place it at thirdparty\godot-cpp, or pass -BootstrapGodotCpp."
    }

    $targetPath = if ($requested) { $requested } else { $defaultPath }
    $targetParent = Split-Path -Parent $targetPath
    if ($targetParent -and -not (Test-Path $targetParent)) {
        New-Item -ItemType Directory -Force $targetParent | Out-Null
    }

    $git = Resolve-Git
    Write-Host "Cloning godot-cpp $ref to $targetPath"
    & $git clone --depth 1 --branch $ref https://github.com/godotengine/godot-cpp.git $targetPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to clone godot-cpp ref $ref"
    }

    return (Resolve-Path $targetPath).Path
}

function Resolve-SCons {
    $cmd = Get-Command scons -ErrorAction SilentlyContinue
    if ($cmd) {
        return @($cmd.Source)
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return @($python.Source, "-m", "SCons")
    }

    $python3 = Get-Command python3 -ErrorAction SilentlyContinue
    if ($python3) {
        return @($python3.Source, "-m", "SCons")
    }

    throw "SCons not found. Install SCons or make it available through python -m SCons."
}

function Resolve-Python {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return $python.Source
    }

    $python3 = Get-Command python3 -ErrorAction SilentlyContinue
    if ($python3) {
        return $python3.Source
    }

    throw "Python not found. Install Python or make python/python3 available on PATH."
}

function Invoke-Tool([string[]]$command, [string[]]$arguments, [string]$workingDirectory) {
    Push-Location $workingDirectory
    try {
        if (-not $env:PROCESSOR_ARCHITECTURE) {
            $env:PROCESSOR_ARCHITECTURE = "AMD64"
        }
        $toolArgs = @()
        if ($command.Length -gt 1) {
            $toolArgs += $command[1..($command.Length - 1)]
        }
        $toolArgs += $arguments
        & $command[0] @toolArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code ${LASTEXITCODE}: $($command -join ' ') $($arguments -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$solution = Join-Path $scriptDir "imm.sln"
$extensionDir = Join-Path $repoRoot "code\appImmGodotGDExtension"
$smokeHelper = Join-Path $scriptDir "run-godot-smoke.ps1"

$python = Resolve-Python
& $python (Join-Path $extensionDir "verify_local.py")
if ($LASTEXITCODE -ne 0) {
    throw "Godot local verification failed with exit code $LASTEXITCODE"
}

if ($VerifyOnly) {
    Write-Host "Godot local verification passed."
    return
}

$godotCpp = Resolve-GodotCppPath $GodotCppPath $repoRoot $BootstrapGodotCpp.IsPresent $GodotCppRef $PreflightOnly.IsPresent
$scons = Resolve-SCons

$target = if ($Configuration -eq "Debug") { "template_debug" } else { "template_release" }

$msbuild = Resolve-MsBuildPath
Write-Host "Using MSBuild: $msbuild"
Write-Host "Using godot-cpp: $godotCpp"
Write-Host "Using godot-cpp ref: $GodotCppRef"
Write-Host "Using SCons: $($scons -join ' ')"

$foundLib = $null
if ($GodotCppLib) {
    if (-not (Test-Path $GodotCppLib)) {
        throw "GodotCppLib was provided but does not exist: $GodotCppLib"
    }
    Write-Host "Using explicit godot-cpp library: $GodotCppLib"
}
else {
    $shortTarget = if ($target.EndsWith("debug")) { "debug" } else { "release" }
    $candidateLibs = @(
        (Join-Path $godotCpp "bin\libgodot-cpp.windows.$target.x86_64.lib"),
        (Join-Path $godotCpp "bin\libgodot-cpp.windows.$target.dev.x86_64.lib"),
        (Join-Path $godotCpp "bin\libgodot-cpp.windows.$shortTarget.x86_64.lib")
    )
    $foundLib = $candidateLibs | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($foundLib) {
        Write-Host "Found godot-cpp library: $foundLib"
    }
    elseif (-not $BuildGodotCpp) {
        Write-Warning "godot-cpp library was not found yet. Pass -BuildGodotCpp or provide -GodotCppLib."
    }
}

$godotCppGeneratedHeader = Join-Path $godotCpp "gen\include\godot_cpp\classes\camera3d.hpp"
$godotCppHasGeneratedBindings = Test-Path $godotCppGeneratedHeader
if (-not $godotCppHasGeneratedBindings) {
    Write-Warning "godot-cpp generated bindings were not found yet: $godotCppGeneratedHeader"
}

if ($PreflightOnly) {
    Write-Host "Godot extension preflight passed."
    return
}

& $msbuild $solution "/t:appImmGodot" "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PostBuildEventUseInBuild=false" "/p:TrackFileAccess=false" "/p:CL_MPCount=1" "-nr:false"
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

if ($BuildGodotCpp) {
    if ($foundLib -and $godotCppHasGeneratedBindings -and -not $GodotCppLib) {
        Write-Host "Skipping godot-cpp build; cached library and generated bindings are already present."
    }
    else {
        Invoke-Tool $scons @("platform=windows", "target=$target", "arch=x86_64", "generate_bindings=yes") $godotCpp
    }
}

$previousGodotCppPath = $env:GODOT_CPP_PATH
$env:GODOT_CPP_PATH = $godotCpp
try {
    & $python (Join-Path $extensionDir "verify_local.py")
    if ($LASTEXITCODE -ne 0) {
        throw "Godot local verification with godot-cpp failed with exit code $LASTEXITCODE"
    }
}
finally {
    $env:GODOT_CPP_PATH = $previousGodotCppPath
}

$extensionArgs = @(
    "platform=windows",
    "target=$target",
    "arch=x86_64",
    "imm_config=$Configuration",
    "godot_cpp=$godotCpp"
)

if ($GodotCppLib) {
    $extensionArgs += "godot_cpp_lib=$GodotCppLib"
}

Invoke-Tool $scons $extensionArgs $extensionDir

$variant = if ($Configuration -eq "Debug") { "debug" } else { "release" }
$outputDir = Join-Path $repoRoot "code\ImmGodotSampleProject\addons\imm_viewer\bin\windows\$variant"
$xrOutputDir = Join-Path $repoRoot "code\ImmGodotXRSampleProject\addons\imm_viewer\bin\windows\$variant"
$requiredOutputDlls = @(
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
New-Item -ItemType Directory -Force $xrOutputDir | Out-Null
foreach ($dll in $requiredOutputDlls) {
    Copy-Item -LiteralPath (Join-Path $outputDir $dll) -Destination (Join-Path $xrOutputDir $dll) -Force
}

$outputDirs = @($outputDir, $xrOutputDir)
foreach ($stagedOutputDir in $outputDirs) {
    $missingOutputDlls = @()
    foreach ($dll in $requiredOutputDlls) {
        $candidate = Join-Path $stagedOutputDir $dll
        if (-not (Test-Path $candidate)) {
            $missingOutputDlls += $candidate
        }
    }
    if ($missingOutputDlls.Count -gt 0) {
        throw "Godot staged output DLLs are missing:`n  $($missingOutputDlls -join "`n  ")"
    }

    $relativeOutputDir = [System.IO.Path]::GetRelativePath($repoRoot, $stagedOutputDir).Replace("\", "/")
    $manifest = @(
        "Configuration=$Configuration",
        "OutputDir=$relativeOutputDir",
        "ExpectedDllCount=$($requiredOutputDlls.Count)",
        "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString("o"))",
        "DLLs:"
    )
    foreach ($dll in $requiredOutputDlls) {
        $candidate = Join-Path $stagedOutputDir $dll
        $item = Get-Item $candidate
        $manifest += ("FOUND`t{0}`t{1}`t{2:o}" -f $item.Name, $item.Length, $item.LastWriteTimeUtc)
    }
    $manifestPath = Join-Path $stagedOutputDir "godot-extension-dlls.txt"
    $manifestText = ($manifest -join "`n") + "`n"
    [System.IO.File]::WriteAllText($manifestPath, $manifestText, [System.Text.UTF8Encoding]::new($false))
}

Write-Host "Updated Godot sample binaries:"
foreach ($stagedOutputDir in $outputDirs) {
    Write-Host "  $stagedOutputDir"
}
Write-Host "Verified staged Godot DLL sets: $($requiredOutputDlls.Count) files each"

if ($RunSmoke) {
    $smokeArgs = @("-Configuration", $Configuration, "-RequireExtension", "-RendererApi", $SmokeRendererApi)
    if ($HeadedSmoke) {
        $smokeArgs += "-Headed"
    }
    if ($GodotExe) {
        $smokeArgs += @("-GodotExe", $GodotExe)
    }
    & $smokeHelper @smokeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Godot smoke test failed with exit code $LASTEXITCODE"
    }
}
