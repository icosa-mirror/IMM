# Copy all required DLLs to a Unity project
# Usage: .\CopyDllsToUnity.ps1 "C:\Path\To\YourUnityProject"

param(
    [Parameter(Mandatory=$true)]
    [string]$UnityProjectPath
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Copying IMM Unity Plugin DLLs" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Resolve paths
$ImmRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$PluginDir = Join-Path $UnityProjectPath "Assets\Plugins\x86_64"

Write-Host "Unity Project: $UnityProjectPath" -ForegroundColor White
Write-Host "Plugin Directory: $PluginDir" -ForegroundColor White
Write-Host "IMM Root: $ImmRoot" -ForegroundColor White
Write-Host ""

# Create plugin directory if it doesn't exist
if (-not (Test-Path $PluginDir)) {
    Write-Host "Creating directory: $PluginDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $PluginDir -Force | Out-Null
}

Write-Host "Copying DLLs..." -ForegroundColor Green
Write-Host ""

# Define DLL mappings (source -> destination name)
$dlls = @(
    @{Source="code\appImmUnity\exe\ImmUnityPlugin.dll"; Name="ImmUnityPlugin.dll"},
    @{Source="thirdparty\audio360-sdk\Audio360\Windows\x64\Audio360.dll"; Name="Audio360.dll"},
    @{Source="thirdparty\libjpeg-turbo\bin\jpeg62.dll"; Name="jpeg62.dll"},
    @{Source="thirdparty\libpng\bin\libpng16.dll"; Name="libpng16.dll"},
    @{Source="thirdparty\libogg\bin\ogg.dll"; Name="ogg.dll"},
    @{Source="thirdparty\opus\bin\opus.dll"; Name="opus.dll"},
    @{Source="thirdparty\libopusenc\bin\opusenc.dll"; Name="opusenc.dll"},
    @{Source="thirdparty\libvorbis\bin\vorbis.dll"; Name="vorbis.dll"},
    @{Source="thirdparty\libvorbis\bin\vorbisenc.dll"; Name="vorbisenc.dll"},
    @{Source="thirdparty\zlib\bin\zlib1.dll"; Name="zlib1.dll"}
)

$index = 1
$total = $dlls.Count
$failed = @()

foreach ($dll in $dlls) {
    $sourcePath = Join-Path $ImmRoot $dll.Source
    $destPath = Join-Path $PluginDir $dll.Name

    Write-Host "[$index/$total] $($dll.Name)" -NoNewline

    if (Test-Path $sourcePath) {
        try {
            Copy-Item -Path $sourcePath -Destination $destPath -Force
            Write-Host " ✓" -ForegroundColor Green
        }
        catch {
            Write-Host " ✗" -ForegroundColor Red
            $failed += @{Name=$dll.Name; Error=$_.Exception.Message}
        }
    }
    else {
        Write-Host " ✗ (Not found)" -ForegroundColor Red
        $failed += @{Name=$dll.Name; Error="Source file not found: $sourcePath"}
    }

    $index++
}

Write-Host ""

if ($failed.Count -eq 0) {
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "SUCCESS! All DLLs copied successfully" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "DLLs are now in: $PluginDir" -ForegroundColor White
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "1. Copy the C# scripts from IMM\code\appImmUnity\Scripts to" -ForegroundColor White
    Write-Host "   $UnityProjectPath\Assets\Plugins\ImmPlayer\" -ForegroundColor White
    Write-Host "2. Open Unity and configure the DLL import settings" -ForegroundColor White
    Write-Host "3. See README.md for detailed setup instructions" -ForegroundColor White
    Write-Host ""
}
else {
    Write-Host "============================================" -ForegroundColor Red
    Write-Host "FAILED! Some DLLs could not be copied" -ForegroundColor Red
    Write-Host "============================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Failed DLLs:" -ForegroundColor Red
    foreach ($fail in $failed) {
        Write-Host "  - $($fail.Name): $($fail.Error)" -ForegroundColor Red
    }
    Write-Host ""
    exit 1
}
