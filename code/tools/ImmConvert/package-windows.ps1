[CmdletBinding()]
param(
    [string]$OutputDirectory = 'artifacts/imm-convert-portable',
    [string]$CrtDirectory
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory, $repositoryRoot)
if (Test-Path -LiteralPath $outputRoot) { throw "Choose a new output directory; already exists: $outputRoot" }
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$reportPath = Join-Path $outputRoot 'package-report.json'
@{ passed = $false } | ConvertTo-Json | Set-Content -LiteralPath $reportPath -Encoding utf8

if (-not $CrtDirectory) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Specify -CrtDirectory for the x64 Visual Studio redistributable CRT directory.' }
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Cannot locate Visual Studio C++ redistributable files.' }
    $redistRoot = Join-Path $installation 'VC/Redist/MSVC'
    $versions = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object Name -Match '^\d+\.\d+' | Sort-Object { [version]$_.Name } -Descending
    foreach ($version in $versions) {
        $candidate = Get-ChildItem -LiteralPath (Join-Path $version.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' |
            Select-Object -First 1
        if ($candidate) { $CrtDirectory = $candidate.FullName; break }
    }
}
if (-not $CrtDirectory -or -not (Test-Path -LiteralPath (Join-Path $CrtDirectory 'vcruntime140.dll'))) {
    throw 'The x64 redistributable CRT directory must contain vcruntime140.dll.'
}

$packageDirectory = Join-Path $outputRoot 'imm-convert-win-x64'
$project = Join-Path $PSScriptRoot 'ImmConvert.csproj'
& dotnet publish $project --configuration Release --runtime win-x64 --self-contained true `
    -p:PublishSingleFile=false -p:PublishTrimmed=false --output $packageDirectory *> (Join-Path $outputRoot 'publish.log')
if ($LASTEXITCODE -ne 0) { throw "Self-contained publish failed; see $outputRoot/publish.log" }
$nativeFiles = @()
foreach ($package in @('com.immersive-foundation.imm-unity', 'com.immersive-foundation.imm-stroke-reader')) {
    $source = Join-Path $repositoryRoot "code/ImmUnitySampleProject/Packages/$package/Plugins/x86_64"
    foreach ($file in Get-ChildItem -LiteralPath $source -File -Filter '*.dll') {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $packageDirectory $file.Name)
        $nativeFiles += $file.Name
    }
}
foreach ($file in Get-ChildItem -LiteralPath $CrtDirectory -File -Filter '*.dll') {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $packageDirectory $file.Name)
    $nativeFiles += $file.Name
}

$licenses = Join-Path $packageDirectory 'licenses'
New-Item -ItemType Directory -Path $licenses | Out-Null
$noticeFiles = @{
    'LICENSE' = 'IMM-Apache-2.0.txt'
    'code/libImmCore/LICENSE.TXT' = 'libImmCore-MIT.txt'
    'code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ThirdParty/SharpQuill/LICENSE.md' = 'SharpQuill-Apache-2.0.txt'
    'thirdparty/audio360-sdk/ThirdPartyNotices.txt' = 'Audio360-ThirdPartyNotices.txt'
    'thirdparty/ovr-sdk/LICENSE.txt' = 'Oculus-SDK-License.txt'
    'thirdparty/ovr-sdk/THIRD_PARTY_NOTICES.txt' = 'Oculus-SDK-ThirdPartyNotices.txt'
    'thirdparty/ovr-platform-sdk/LICENSE.txt' = 'Oculus-Platform-License.txt'
    'thirdparty/ovr-platform-sdk/THIRD_PARTY_NOTICES.txt' = 'Oculus-Platform-ThirdPartyNotices.txt'
    'thirdparty/libjpeg-turbo/libjpeg-turbo/LICENSE.md' = 'libjpeg-turbo-LICENSE.md'
    'thirdparty/libjpeg-turbo/libjpeg-turbo/README.ijg' = 'libjpeg-turbo-README.ijg'
    'thirdparty/opus-src/COPYING' = 'opus-COPYING.txt'
    'thirdparty/libopusenc-src/COPYING' = 'libopusenc-COPYING.txt'
    'thirdparty/ogg/libvorbis-1.3.7/COPYING' = 'libvorbis-COPYING.txt'
}
foreach ($entry in $noticeFiles.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $repositoryRoot $entry.Key) -Destination (Join-Path $licenses $entry.Value)
}
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'licenses') -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $licenses $_.Name)
}
$packageRoot = (& dotnet msbuild $project -getProperty:NuGetPackageRoot).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine NuGet package root for license preservation.' }
$runtimeConfig = Get-Content -LiteralPath (Join-Path $packageDirectory 'imm-convert.runtimeconfig.json') -Raw | ConvertFrom-Json
$runtimeVersion = ($runtimeConfig.runtimeOptions.includedFrameworks | Where-Object name -eq 'Microsoft.NETCore.App').version
if (-not $runtimeVersion -or $runtimeConfig.runtimeOptions.framework) { throw 'Publish must include its own .NET runtime.' }
$runtimePackage = Join-Path $packageRoot "microsoft.netcore.app.runtime.win-x64/$runtimeVersion"
Copy-Item -LiteralPath (Join-Path $runtimePackage 'LICENSE.TXT') -Destination (Join-Path $licenses 'dotnet-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $runtimePackage 'THIRD-PARTY-NOTICES.TXT') -Destination (Join-Path $licenses 'dotnet-THIRD-PARTY-NOTICES.txt')
$deps = Get-Content -LiteralPath (Join-Path $packageDirectory 'imm-convert.deps.json') -Raw | ConvertFrom-Json
$jsonLibrary = $deps.libraries.PSObject.Properties.Name | Where-Object { $_ -like 'Newtonsoft.Json/*' }
if (@($jsonLibrary).Count -ne 1) { throw 'Cannot identify Newtonsoft.Json package license.' }
$jsonVersion = $jsonLibrary.Split('/')[1]
Copy-Item -LiteralPath (Join-Path $packageRoot "newtonsoft.json/$jsonVersion/LICENSE.md") -Destination (Join-Path $licenses 'Newtonsoft.Json-LICENSE.md')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'PORTABLE_README.txt') -Destination (Join-Path $packageDirectory 'START_HERE.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD_PARTY.md') -Destination (Join-Path $packageDirectory 'THIRD_PARTY.md')

$sourceRevision = (& git -C $repositoryRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot record source revision.' }
$dirty = @(& git -C $repositoryRoot status --porcelain=v1).Count -ne 0
if ($LASTEXITCODE -ne 0) { throw 'Cannot record working-tree state.' }
$files = @(Get-ChildItem -LiteralPath $packageDirectory -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{ path = [IO.Path]::GetRelativePath($packageDirectory, $_.FullName).Replace('\', '/'); bytes = $_.Length;
       sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$manifest = @{ schema = 'imm-convert-portable-package-v1'; sourceRevision = $sourceRevision;
    workingTreeDirty = $dirty; runtime = "Microsoft.NETCore.App/$runtimeVersion"; rid = 'win-x64';
    publicReleaseLicensingReviewComplete = $false; nativeFiles = $nativeFiles; files = $files }
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $packageDirectory 'portable-manifest.json') -Encoding utf8
$archive = Join-Path $outputRoot 'imm-convert-win-x64.zip'
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archive -CompressionLevel Optimal
@{ passed = $true; sourceRevision = $sourceRevision; workingTreeDirty = $dirty;
   archive = 'imm-convert-win-x64.zip'; bytes = (Get-Item -LiteralPath $archive).Length;
   sha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant();
   scope = 'self-contained Windows CLI bundle; extracted-runtime tests and public-release acceptance are separate' } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Output "[IMM_CONVERT_PACKAGE] $archive"
