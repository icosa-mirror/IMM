param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$sourceDirectory = Join-Path $repoRoot 'code/libImmCore/tests/d3d12'
$outputDirectory = Join-Path $repoRoot 'artifacts/d3d12-submission'
$buildDirectory = Join-Path $repoRoot 'build/d3d12-submission'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
# Never leave a previous pass report beside a failed build from this invocation.
foreach ($name in @('d3d12-submission-result.json', 'd3d12-submission.ppm')) {
    $previousResult = Join-Path $outputDirectory $name
    if (Test-Path -LiteralPath $previousResult) { Remove-Item -LiteralPath $previousResult }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$toolchainLog = Join-Path $outputDirectory 'toolchain.log'
$installations = & $vswhere -latest -products '*' -version '[17.0,)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json
$installations | Set-Content -LiteralPath $toolchainLog
$visualStudio = @($installations | ConvertFrom-Json) | Select-Object -First 1
if (-not $visualStudio) { throw 'Visual Studio 2022 or newer C++ tools are required for this smoke test.' }
$majorVersion = ([version]$visualStudio.installationVersion).Major
$buildDirectory = Join-Path $buildDirectory "vs$majorVersion"
$generators = (& cmake -E capabilities | ConvertFrom-Json).generators
$generator = $generators | Where-Object { $_.name -match "^Visual Studio $majorVersion " } | Select-Object -First 1
if (-not $generator) { throw "CMake has no generator for installed Visual Studio $majorVersion; see $toolchainLog" }
$generatorName = $generator.name
"Selected CMake generator: $generatorName" | Add-Content -LiteralPath $toolchainLog

$configureLog = Join-Path $outputDirectory 'configure.log'
$buildLog = Join-Path $outputDirectory 'build.log'
$testLog = Join-Path $outputDirectory 'test.log'
& cmake -S $sourceDirectory -B $buildDirectory -G $generatorName -A x64 `
    "-DCMAKE_GENERATOR_INSTANCE=$($visualStudio.installationPath)" *> $configureLog
if ($LASTEXITCODE -ne 0) { throw "D3D12 smoke configuration failed; see $configureLog" }
& cmake --build $buildDirectory --config $Configuration --parallel 2 *> $buildLog
if ($LASTEXITCODE -ne 0) { throw "D3D12 smoke compilation failed; see $buildLog" }

# CTest bounds GPU/fence failures with a timeout. Results are copied only after
# a successful test so an old capture cannot masquerade as current evidence.
& ctest --test-dir $buildDirectory -C $Configuration --output-on-failure *> $testLog
if ($LASTEXITCODE -ne 0) { throw "D3D12 submission smoke failed; see $testLog" }
foreach ($name in @('d3d12-submission-result.json', 'd3d12-submission.ppm')) {
    Copy-Item -LiteralPath (Join-Path $buildDirectory $name) -Destination $outputDirectory -Force
}
$result = Get-Content -LiteralPath (Join-Path $outputDirectory 'd3d12-submission-result.json') -Raw | ConvertFrom-Json
if ($result.status -ne 'pass' -or $result.api -ne 'D3D12' -or $result.frames_verified -ne 9) {
    throw 'D3D12 smoke did not produce the required readback evidence.'
}
foreach ($bufferType in @('vertex', 'index', 'constant', 'structured')) {
    if ($bufferType -notin $result.buffer_uploads) {
        throw "D3D12 smoke is missing $bufferType buffer upload evidence."
    }
}
if ($result.texture_subresources_verified -ne 8 -or
    'rgba8' -notin $result.texture_uploads -or 'bc1' -notin $result.texture_uploads) {
    throw 'D3D12 smoke is missing texture mip/array readback evidence.'
}
if ($result.production_picture_shader -ne $true) {
    throw 'D3D12 smoke is missing production picture shader evidence.'
}
if ($result.renderer_buffer_versions_verified -ne 18) {
    throw 'D3D12 smoke is missing piRenderer buffer-version evidence.'
}
if ($result.renderer_draw_frames_verified -ne 9) {
    throw 'D3D12 smoke is missing piRenderer draw/depth evidence.'
}
if ($result.renderer_indexed_frames_verified -ne 9) {
    throw 'D3D12 smoke is missing indexed vertex/instance stream evidence.'
}
$result | Add-Member -NotePropertyName source_revision -NotePropertyValue ((& git -C $repoRoot rev-parse HEAD).Trim())
$smokeExecutable = Join-Path $buildDirectory "$Configuration/imm_d3d12_submission_smoke.exe"
$result | Add-Member -NotePropertyName executable_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $smokeExecutable -Algorithm SHA256).Hash
$sourceHashes = [ordered]@{}
foreach ($relativePath in @(
    'code/libImmCore/src/libRender/directx12/piDX12_CommandContext.h',
    'code/libImmCore/src/libRender/directx12/piDX12_CommandContext.cpp',
    'code/libImmCore/src/libRender/directx12/piDX12_ShaderBindings.h',
    'code/libImmCore/src/libRender/directx12/piDX12_ShaderBindings.cpp',
    'code/libImmCore/src/libRender/directx12/piDX12_Renderer.h',
    'code/libImmCore/src/libRender/directx12/piDX12_Renderer.cpp',
    'code/libImmCore/src/libRender/piRenderer.h',
    'code/libImmPlayer/src/layerRenderers/layerRendererPicture/shader_pi2D_vs.hlsl',
    'code/libImmPlayer/src/layerRenderers/layerRendererPicture/shader_pi2D_fs.hlsl',
    'code/libImmCore/tests/d3d12/submission_smoke.cpp',
    'code/libImmCore/tests/d3d12/CMakeLists.txt'
)) {
    $sourceHashes[$relativePath] = (Get-FileHash -LiteralPath (Join-Path $repoRoot $relativePath) -Algorithm SHA256).Hash
}
$result | Add-Member -NotePropertyName source_sha256 -NotePropertyValue $sourceHashes
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputDirectory 'd3d12-submission-result.json') -Encoding utf8
Write-Host "IMM_DX12_PHASE1 PASS: D3D12/WARP draw and reversed-Z readback; evidence in $outputDirectory"
