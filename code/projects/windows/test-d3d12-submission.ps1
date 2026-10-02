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
$evidenceFiles = @(
    'd3d12-submission-result.json',
    'd3d12-submission.ppm',
    'd3d12-player-result.json',
    'd3d12-model-linear.ppm',
    'd3d12-model-gamma.ppm',
    'd3d12-layered-1x.ppm',
    'd3d12-layered-2x.ppm',
    'd3d12-layered-4x.ppm',
    'd3d12-layered-8x.ppm',
    'd3d12-orthographic-technique0-color0-depth0.ppm',
    'd3d12-orthographic-technique0-color0-depth1.ppm',
    'd3d12-orthographic-technique0-color0-depth2.ppm',
    'd3d12-orthographic-technique0-color1-depth0.ppm',
    'd3d12-orthographic-technique0-color1-depth1.ppm',
    'd3d12-orthographic-technique0-color1-depth2.ppm',
    'd3d12-orthographic-technique1-color0-depth0.ppm',
    'd3d12-orthographic-technique1-color0-depth1.ppm',
    'd3d12-orthographic-technique1-color0-depth2.ppm',
    'd3d12-orthographic-technique1-color1-depth0.ppm',
    'd3d12-orthographic-technique1-color1-depth1.ppm',
    'd3d12-orthographic-technique1-color1-depth2.ppm',
    'd3d12-model-half-opacity-1x.ppm',
    'd3d12-model-half-opacity-2x.ppm',
    'd3d12-model-half-opacity-4x.ppm',
    'd3d12-model-half-opacity-8x.ppm',
    'd3d12-picture-format1-half-opacity-1x.ppm',
    'd3d12-picture-format1-half-opacity-2x.ppm',
    'd3d12-picture-format1-half-opacity-4x.ppm',
    'd3d12-picture-format1-half-opacity-8x.ppm',
    'd3d12-picture-format2-half-opacity-1x.ppm',
    'd3d12-picture-format2-half-opacity-2x.ppm',
    'd3d12-picture-format2-half-opacity-4x.ppm',
    'd3d12-picture-format2-half-opacity-8x.ppm',
    'd3d12-picture-format3-half-opacity-1x.ppm',
    'd3d12-picture-format3-half-opacity-2x.ppm',
    'd3d12-picture-format3-half-opacity-4x.ppm',
    'd3d12-picture-format3-half-opacity-8x.ppm',
    'd3d12-picture-format4-half-opacity-1x.ppm',
    'd3d12-picture-format4-half-opacity-2x.ppm',
    'd3d12-picture-format4-half-opacity-4x.ppm',
    'd3d12-picture-format4-half-opacity-8x.ppm',
    'd3d12-picture-format5-half-opacity-1x.ppm',
    'd3d12-picture-format5-half-opacity-2x.ppm',
    'd3d12-picture-format5-half-opacity-4x.ppm',
    'd3d12-picture-format5-half-opacity-8x.ppm',
    'd3d12-panorama-linear.ppm',
    'd3d12-panorama-gamma.ppm',
    'd3d12-cubemap-cross-linear-face0.ppm',
    'd3d12-cubemap-cross-linear-face1.ppm',
    'd3d12-cubemap-cross-linear-face2.ppm',
    'd3d12-cubemap-cross-linear-face3.ppm',
    'd3d12-cubemap-cross-linear-face4.ppm',
    'd3d12-cubemap-cross-linear-face5.ppm',
    'd3d12-cubemap-cross-gamma-face0.ppm',
    'd3d12-cubemap-cross-gamma-face1.ppm',
    'd3d12-cubemap-cross-gamma-face2.ppm',
    'd3d12-cubemap-cross-gamma-face3.ppm',
    'd3d12-cubemap-cross-gamma-face4.ppm',
    'd3d12-cubemap-cross-gamma-face5.ppm',
    'd3d12-cubemap-strip-linear-face0.ppm',
    'd3d12-cubemap-strip-linear-face1.ppm',
    'd3d12-cubemap-strip-linear-face2.ppm',
    'd3d12-cubemap-strip-linear-face3.ppm',
    'd3d12-cubemap-strip-linear-face4.ppm',
    'd3d12-cubemap-strip-linear-face5.ppm',
    'd3d12-cubemap-strip-gamma-face0.ppm',
    'd3d12-cubemap-strip-gamma-face1.ppm',
    'd3d12-cubemap-strip-gamma-face2.ppm',
    'd3d12-cubemap-strip-gamma-face3.ppm',
    'd3d12-cubemap-strip-gamma-face4.ppm',
    'd3d12-cubemap-strip-gamma-face5.ppm',
    'd3d12-player.log',
    'd3d12-bridge.log',
    'imm-render-graph.log',
    'd3d12-player-scene-static-linear.ppm',
    'd3d12-player-scene-static-gamma.ppm',
    'd3d12-player-scene-pretessellated-linear.ppm',
    'd3d12-player-scene-pretessellated-gamma.ppm',
    'd3d12-player-depth-static-linear.ppm',
    'd3d12-player-depth-static-gamma.ppm',
    'd3d12-player-depth-pretessellated-linear.ppm',
    'd3d12-player-depth-pretessellated-gamma.ppm',
    'd3d12-player-depth-write-static-linear.ppm',
    'd3d12-player-depth-write-static-gamma.ppm',
    'd3d12-player-depth-write-pretessellated-linear.ppm',
    'd3d12-player-depth-write-pretessellated-gamma.ppm'
)
foreach ($name in $evidenceFiles) {
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
    "-DCMAKE_GENERATOR_INSTANCE=$($visualStudio.installationPath)" -DIMM_D3D12_PLAYER_SMOKE=ON *> $configureLog
if ($LASTEXITCODE -ne 0) { throw "D3D12 smoke configuration failed; see $configureLog" }
& cmake --build $buildDirectory --config $Configuration --parallel 2 *> $buildLog
if ($LASTEXITCODE -ne 0) { throw "D3D12 smoke compilation failed; see $buildLog" }

# CTest bounds GPU/fence failures with a timeout. Results are copied only after
# a successful test so an old capture cannot masquerade as current evidence.
& ctest --test-dir $buildDirectory -C $Configuration --output-on-failure *> $testLog
$testExitCode = $LASTEXITCODE
$playerLog = Join-Path $buildDirectory 'd3d12-player.log'
if (Test-Path -LiteralPath $playerLog) { Copy-Item -LiteralPath $playerLog -Destination $outputDirectory -Force }
if ($testExitCode -ne 0) { throw "D3D12 submission smoke failed; see $testLog" }
foreach ($name in $evidenceFiles) {
    Copy-Item -LiteralPath (Join-Path $buildDirectory $name) -Destination $outputDirectory -Force
}
$playerResult = Get-Content -LiteralPath (Join-Path $outputDirectory 'd3d12-player-result.json') -Raw | ConvertFrom-Json
if ($playerResult.status -ne 'pass' -or $playerResult.configurations_verified -ne 4 -or $playerResult.documents_loaded -ne 4 -or $playerResult.scene_frames_verified -ne 12 -or $playerResult.imm_depth_write_frames_verified -ne 4 -or $playerResult.host_depth_frames_verified -ne 4 -or $playerResult.msaa_samples -ne 8 -or -not $playerResult.unity_queue_event_contract_mocked -or -not $playerResult.unity_target_binding_mocked -or -not $playerResult.borrowed_renderer_lifecycle_verified -or -not $playerResult.render_graph_packet_lifecycle_verified -or -not $playerResult.debug_layer_enabled) {
    throw 'D3D12 player scene-readback evidence is incomplete.'
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
if ($result.production_panorama_pipelines_verified -ne 4) {
    throw "D3D12 production panorama shader pipeline evidence is incomplete."
}
if ($result.renderer_mesh_layout_frames_verified -ne 3) {
    throw "D3D12 mesh layout GPU readback evidence is incomplete."
}
if ($result.renderer_indexed_frames_verified -ne 9) {
    throw 'D3D12 smoke is missing indexed vertex/instance stream evidence.'
}
if ($result.renderer_sampled_mip_frames_verified -ne 9) {
    throw 'D3D12 smoke is missing sampled mip-chain evidence.'
}
if ($result.renderer_state_frames_verified -ne 9) {
    throw 'D3D12 smoke is missing renderer state/unit-quad evidence.'
}
if ($result.renderer_cube_faces_verified -ne 6) {
    throw 'D3D12 smoke is missing cube-face sampling evidence.'
}
$packetAbiLog = Join-Path $outputDirectory 'packet-abi.log'
$nativePlugin = Join-Path $repoRoot 'code/appImmUnity/exe/ImmUnityPlugin.dll'
& python (Join-Path $repoRoot 'tests/tools/verify_render_graph_packet_abi.py') $nativePlugin *> $packetAbiLog
if ($LASTEXITCODE -ne 0) { throw "RenderGraph packet ABI validation failed; see $packetAbiLog" }
$playerResult | Add-Member -NotePropertyName native_packet_abi_verified -NotePropertyValue $true
$playerResult | Add-Member -NotePropertyName native_plugin_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $nativePlugin -Algorithm SHA256).Hash
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
    'code/libImmPlayer/src/layerRenderers/layerRendererPicture/shader_pip360Equirect_vs.hlsl',
    'code/libImmPlayer/src/layerRenderers/layerRendererPicture/shader_pip360Equirect_fs.hlsl',
    'code/libImmCore/tests/d3d12/submission_smoke.cpp',
    'code/libImmCore/tests/d3d12/player_smoke.cpp',
    'code/appImmUnity/src/imm_unity_d3d12_host.h',
    'code/appImmUnity/src/imm_unity_render_graph.h',
    'code/appImmUnity/src/main.cpp',
    'code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmRenderGraphTransport.cs',
    'tests/tools/verify_render_graph_packet_abi.py',
    'code/appImmUnity/src/IUnityGraphicsD3D12.h',
    'code/appImmShared/src/imm_engine_bridge.h',
    'code/appImmShared/src/imm_engine_bridge.cpp',
    'code/libImmCore/tests/d3d12/CMakeLists.txt'
)) {
    $sourceHashes[$relativePath] = (Get-FileHash -LiteralPath (Join-Path $repoRoot $relativePath) -Algorithm SHA256).Hash
}
$playerResult | Add-Member -NotePropertyName source_revision -NotePropertyValue $result.source_revision
$playerExecutable = Join-Path $buildDirectory "$Configuration/imm_d3d12_player_smoke.exe"
$playerResult | Add-Member -NotePropertyName executable_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $playerExecutable -Algorithm SHA256).Hash
$libraryHashes = [ordered]@{}
foreach ($library in @('libImmPlayer', 'libImmImporter', 'libImmExporter', 'libImmCore')) {
    $libraryPath = Join-Path $repoRoot "code/$library/bin/x64/$Configuration/$library.lib"
    $libraryHashes[$library] = (Get-FileHash -LiteralPath $libraryPath -Algorithm SHA256).Hash
}
$playerResult | Add-Member -NotePropertyName library_sha256 -NotePropertyValue $libraryHashes
$playerResult | Add-Member -NotePropertyName fixture_sha256 -NotePropertyValue (Get-FileHash -LiteralPath (Join-Path $repoRoot 'exampleImmFiles/sample1.imm') -Algorithm SHA256).Hash
$playerResult | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputDirectory 'd3d12-player-result.json') -Encoding utf8
$result | Add-Member -NotePropertyName source_sha256 -NotePropertyValue $sourceHashes
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputDirectory 'd3d12-submission-result.json') -Encoding utf8
Write-Host "IMM_DX12_PHASE1 PASS: D3D12/WARP draw and reversed-Z readback; evidence in $outputDirectory"
