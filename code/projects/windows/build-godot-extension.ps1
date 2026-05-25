param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$GodotCppPath = "",
    [string]$GodotCppRef = "godot-4.2-stable",
    [switch]$BootstrapGodotCpp,
    [switch]$BuildGodotCpp,
    [string]$SConsExe = "scons",
    [string]$PythonExe = "python",
    [string]$MSBuildExe = "msbuild",
    [string]$SummaryDir = ""
)

$ErrorActionPreference = "Stop"

function Write-GodotBuildSummary {
    param(
        [string]$Phase,
        [string]$Status,
        [string]$Message,
        [string]$Configuration,
        [string]$GodotCppPath,
        [string]$OutputDir = "",
        [string]$MSBuildPath = "",
        [string]$SConsPath = "",
        [string]$GitPath = "",
        [string[]]$MissingDlls = @()
    )

    if ([string]::IsNullOrWhiteSpace($SummaryDir)) {
        return
    }

    New-Item -ItemType Directory -Force $SummaryDir | Out-Null
    $generatedUtc = (Get-Date).ToUniversalTime().ToString('o')
    $summaryText = @(
        "Status=$Status",
        "Phase=$Phase",
        "Configuration=$Configuration",
        "GodotCppPath=$GodotCppPath",
        "OutputDir=$OutputDir",
        "MSBuildPath=$MSBuildPath",
        "SConsPath=$SConsPath",
        "GitPath=$GitPath",
        "MissingDlls=$($MissingDlls -join ';')",
        "GeneratedUtc=$generatedUtc",
        "Message=$Message"
    )
    $summaryText | Set-Content -Path (Join-Path $SummaryDir "godot-build-summary.txt") -Encoding UTF8

    $summaryJson = [ordered]@{
        status = $Status
        phase = $Phase
        message = $Message
        configuration = $Configuration
        godot_cpp_path = $GodotCppPath
        output_dir = $OutputDir
        msbuild_path = $MSBuildPath
        scons_path = $SConsPath
        git_path = $GitPath
        missing_dlls = $MissingDlls
        generated_utc = $generatedUtc
    }
    $summaryJson | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $SummaryDir "godot-build-summary.json") -Encoding UTF8
}

function Resolve-ExecutablePath {
    param(
        [string]$Executable,
        [string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Executable)) {
        throw "$Label executable path is empty."
    }

    if (Test-Path $Executable) {
        return (Resolve-Path $Executable).Path
    }

    $command = Get-Command $Executable -ErrorAction SilentlyContinue
    if ($command -and $command.Source) {
        return $command.Source
    }

    throw "$Label executable was not found: $Executable"
}

function Resolve-MsBuildPath {
    param([string]$RequestedMsBuild)

    function Test-CppToolchainForMsBuild([string]$msbuildPath) {
        if (-not $msbuildPath -or -not (Test-Path $msbuildPath)) {
            return $false
        }

        $msbuildDir = Split-Path -Parent $msbuildPath
        $msbuildRoot = Resolve-Path (Join-Path $msbuildDir "..\..")
        $cppProps = Join-Path $msbuildRoot "Microsoft\VC\v170\Microsoft.Cpp.Default.props"
        return Test-Path $cppProps
    }

    if ($RequestedMsBuild -and $RequestedMsBuild -ne "msbuild") {
        if (Test-CppToolchainForMsBuild $RequestedMsBuild) {
            return $RequestedMsBuild
        }
        throw "Requested MSBuild path is missing C++ build props: $RequestedMsBuild"
    }

    if ($env:MSBUILD_EXE_PATH -and (Test-Path $env:MSBUILD_EXE_PATH)) {
        if (Test-CppToolchainForMsBuild $env:MSBUILD_EXE_PATH) {
            return $env:MSBUILD_EXE_PATH
        }
        throw "MSBUILD_EXE_PATH is set but missing C++ build props: $env:MSBUILD_EXE_PATH"
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

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$extensionRoot = Join-Path $repoRoot "code\appImmGodotGDExtension"
$configurationLower = $Configuration.ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($GodotCppPath)) {
    $GodotCppPath = Join-Path $repoRoot "thirdparty\godot-cpp"
}

try {
    $resolvedSCons = Resolve-ExecutablePath -Executable $SConsExe -Label "SCons"
}
catch {
    Write-GodotBuildSummary -Phase "toolchain" -Status "failed" -Message $_.Exception.Message -Configuration $Configuration -GodotCppPath $GodotCppPath
    throw
}
Write-Host "Using SCons: $resolvedSCons"

$resolvedGit = ""
if ($BootstrapGodotCpp -and -not (Test-Path $GodotCppPath)) {
    try {
        $resolvedGit = Resolve-ExecutablePath -Executable "git" -Label "git"
    }
    catch {
        Write-GodotBuildSummary -Phase "toolchain" -Status "failed" -Message $_.Exception.Message -Configuration $Configuration -GodotCppPath $GodotCppPath -SConsPath $resolvedSCons
        throw
    }
    New-Item -ItemType Directory -Force (Split-Path $GodotCppPath -Parent) | Out-Null
    & $resolvedGit clone --depth 1 --branch $GodotCppRef https://github.com/godotengine/godot-cpp.git $GodotCppPath
    if ($LASTEXITCODE -ne 0) {
        Write-GodotBuildSummary -Phase "bootstrap" -Status "failed" -Message "godot-cpp clone failed" -Configuration $Configuration -GodotCppPath $GodotCppPath -SConsPath $resolvedSCons -GitPath $resolvedGit
        throw "godot-cpp clone failed"
    }
}

if (-not (Test-Path $GodotCppPath)) {
    Write-GodotBuildSummary -Phase "preflight" -Status "failed" -Message "godot-cpp path does not exist: $GodotCppPath" -Configuration $Configuration -GodotCppPath $GodotCppPath -SConsPath $resolvedSCons -GitPath $resolvedGit
    throw "godot-cpp path does not exist: $GodotCppPath"
}

$msbuildConfiguration = $Configuration
try {
    $resolvedMsBuild = Resolve-MsBuildPath -RequestedMsBuild $MSBuildExe
}
catch {
    Write-GodotBuildSummary -Phase "toolchain" -Status "failed" -Message $_.Exception.Message -Configuration $Configuration -GodotCppPath $GodotCppPath -SConsPath $resolvedSCons -GitPath $resolvedGit
    throw
}
Write-Host "Using MSBuild: $resolvedMsBuild"
& $resolvedMsBuild (Join-Path $PSScriptRoot "imm.sln") /t:appImmGodot /p:Configuration=$msbuildConfiguration /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) {
    Write-GodotBuildSummary -Phase "msbuild" -Status "failed" -Message "MSBuild failed for appImmGodot" -Configuration $Configuration -GodotCppPath $GodotCppPath -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit
    throw "MSBuild failed for appImmGodot"
}

$target = if ($Configuration -eq "Debug") { "template_debug" } else { "template_release" }
if ($BuildGodotCpp) {
    Push-Location $GodotCppPath
    try {
        & $resolvedSCons platform=windows target=$target arch=x86_64 generate_bindings=yes
        if ($LASTEXITCODE -ne 0) {
            Write-GodotBuildSummary -Phase "godot_cpp" -Status "failed" -Message "godot-cpp SCons build failed" -Configuration $Configuration -GodotCppPath $GodotCppPath -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit
            throw "godot-cpp SCons build failed"
        }
    }
    finally {
        Pop-Location
    }
}

Push-Location $extensionRoot
try {
    & $resolvedSCons platform=windows target=$target arch=x86_64 configuration=$configurationLower godot_cpp_path="$GodotCppPath"
    if ($LASTEXITCODE -ne 0) {
        Write-GodotBuildSummary -Phase "gdextension" -Status "failed" -Message "Imm Godot GDExtension SCons build failed" -Configuration $Configuration -GodotCppPath $GodotCppPath -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit
        throw "Imm Godot GDExtension SCons build failed"
    }
}
finally {
    Pop-Location
}

$outputDir = Join-Path $repoRoot "code\ImmGodotSampleProject\bin\windows\$configurationLower"
$extensionDll = Join-Path $outputDir "imm_godot_extension.dll"
$pluginDll = Join-Path $outputDir "ImmGodotPlugin.dll"
if (-not (Test-Path $extensionDll) -or -not (Test-Path $pluginDll)) {
    Write-GodotBuildSummary -Phase "stage_outputs" -Status "failed" -Message "Expected Godot extension outputs were not produced in $outputDir" -Configuration $Configuration -GodotCppPath $GodotCppPath -OutputDir $outputDir -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit
    throw "Expected Godot extension outputs were not produced in $outputDir"
}

$runtimeDependencies = @{
    "Audio360.dll"  = (Join-Path $repoRoot "thirdparty\audio360-sdk\Audio360\Windows\x64\Audio360.dll")
    "opus.dll"      = (Join-Path $repoRoot "thirdparty\opus\bin\opus.dll")
    "opusenc.dll"   = (Join-Path $repoRoot "thirdparty\libopusenc\bin\opusenc.dll")
    "vorbisenc.dll" = (Join-Path $repoRoot "thirdparty\libvorbis\bin\vorbisenc.dll")
    "zlib1.dll"     = (Join-Path $repoRoot "thirdparty\zlib\bin\zlib1.dll")
    "jpeg62.dll"    = (Join-Path $repoRoot "thirdparty\libjpeg-turbo\bin\jpeg62.dll")
    "libpng16.dll"  = (Join-Path $repoRoot "thirdparty\libpng\bin\libpng16.dll")
    "ogg.dll"       = (Join-Path $repoRoot "thirdparty\libogg\bin\ogg.dll")
    "vorbis.dll"    = (Join-Path $repoRoot "thirdparty\libvorbis\bin\vorbis.dll")
}

foreach ($dependency in $runtimeDependencies.GetEnumerator()) {
    if (-not (Test-Path $dependency.Value)) {
        Write-GodotBuildSummary -Phase "stage_dependencies" -Status "failed" -Message "Required Godot runtime dependency source is missing: $($dependency.Value)" -Configuration $Configuration -GodotCppPath $GodotCppPath -OutputDir $outputDir -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit -MissingDlls @($dependency.Key)
        throw "Required Godot runtime dependency source is missing: $($dependency.Value)"
    }
    Copy-Item $dependency.Value (Join-Path $outputDir $dependency.Key) -Force
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

$missingDlls = @()
foreach ($dll in $requiredDlls) {
    if (-not (Test-Path (Join-Path $outputDir $dll))) {
        $missingDlls += $dll
    }
}
if ($missingDlls.Count -gt 0) {
    Write-GodotBuildSummary -Phase "stage_verify" -Status "failed" -Message "Godot staged output DLLs are missing: $($missingDlls -join ', ')" -Configuration $Configuration -GodotCppPath $GodotCppPath -OutputDir $outputDir -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit -MissingDlls $missingDlls
    throw "Godot staged output DLLs are missing: $($missingDlls -join ', ')"
}

$manifestPath = Join-Path $outputDir "godot-extension-dlls.txt"
$manifest = @(
    "Configuration=$Configuration",
    "OutputDir=$outputDir",
    "ExpectedDllCount=$($requiredDlls.Count)",
    "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('o'))",
    "DLLs:"
)
foreach ($dll in $requiredDlls) {
    $path = Join-Path $outputDir $dll
    $item = Get-Item $path
    $hash = Get-FileHash -Algorithm SHA256 -Path $path
    $manifest += "FOUND`t$dll`t$($item.Length)`t$($item.LastWriteTimeUtc.ToString('o'))`tSHA256=$($hash.Hash)"
}
$manifest | Set-Content -Path $manifestPath -Encoding UTF8

Write-Host "Godot extension output: $extensionDll"
Write-Host "Verified staged Godot DLL set: $($requiredDlls.Count) files"
Write-Host "Godot staged DLL manifest: $manifestPath"
Write-GodotBuildSummary -Phase "complete" -Status "passed" -Message "Godot extension build and staging completed" -Configuration $Configuration -GodotCppPath $GodotCppPath -OutputDir $outputDir -MSBuildPath $resolvedMsBuild -SConsPath $resolvedSCons -GitPath $resolvedGit
