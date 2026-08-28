param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$ViewerExe = "",
    [string]$SettingsPath = "",
    [string]$SamplePath = "",
    [string]$OutputPath = "",
    [int]$PlayerFrame = 60,
    [int]$MinPictureDrawCalls = 1,
    [int]$MinPicture360DrawCalls = 1,
    [int]$MinPicture360EquirectDrawCalls = 1,
    [int]$TimeoutSeconds = 45,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Resolve-RequiredFile([string]$PathValue, [string]$Description) {
    if (-not $PathValue) {
        throw "$Description path is empty"
    }
    $resolved = Resolve-Path -LiteralPath $PathValue -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "$Description is not a file: $PathValue"
    }
    return $resolved.Path
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRootCandidate = Join-Path $scriptDir "..\..\.."
$repoRoot = ""
if (Test-Path -LiteralPath (Join-Path $repoRootCandidate "code\appImmViewer\exe") -PathType Container) {
    $repoRoot = (Resolve-Path -LiteralPath $repoRootCandidate).Path
}

function Find-MSBuild {
    if ($env:MSBUILD_EXE_PATH -and (Test-Path $env:MSBUILD_EXE_PATH)) {
        return $env:MSBUILD_EXE_PATH
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\Current\Bin\amd64\MSBuild.exe" | Select-Object -First 1
        if ($path -and (Test-Path $path)) {
            return $path
        }
    }

    $knownPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    if (Test-Path $knownPath) {
        return $knownPath
    }

    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "MSBuild.exe was not found. Set MSBUILD_EXE_PATH or install Visual Studio Build Tools."
}

if (-not $SkipBuild) {
    if (-not $repoRoot) {
        throw "Could not resolve repository root for build. Pass -ViewerExe or run from the repository checkout."
    }
    $msbuild = Find-MSBuild
    & $msbuild (Join-Path $repoRoot "code\projects\windows\imm.sln") `
        -t:appImmViewer `
        -p:Configuration=$Configuration `
        -p:Platform=$Platform `
        -p:TrackFileAccess=false `
        -p:CL_MPCount=1 `
        -nr:false `
        -v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
}

if (-not $ViewerExe) {
    $exeName = if ($Configuration -ieq "Debug") { "appImmViewer_Debug.exe" } else { "appImmViewer_Release.exe" }
    if ($repoRoot) {
        $ViewerExe = Join-Path $repoRoot "code\appImmViewer\exe\$exeName"
    }
    if (-not $ViewerExe -or -not (Test-Path -LiteralPath $ViewerExe -PathType Leaf)) {
        $artifactViewer = Join-Path $scriptDir $exeName
        $cwdViewer = Join-Path (Get-Location) $exeName
        if (Test-Path -LiteralPath $artifactViewer -PathType Leaf) {
            $ViewerExe = $artifactViewer
        } elseif (Test-Path -LiteralPath $cwdViewer -PathType Leaf) {
            $ViewerExe = $cwdViewer
        }
    }
}

if (-not $SettingsPath) {
    if ($repoRoot) {
        $SettingsPath = Join-Path $repoRoot "code\appImmViewer\exe\settings-baseline-directx-sample1.json"
    }
    if (-not $SettingsPath -or -not (Test-Path -LiteralPath $SettingsPath -PathType Leaf)) {
        $artifactSettings = Join-Path $scriptDir "settings-baseline-directx-sample1.json"
        $cwdSettings = Join-Path (Get-Location) "settings-baseline-directx-sample1.json"
        if (Test-Path -LiteralPath $artifactSettings -PathType Leaf) {
            $SettingsPath = $artifactSettings
        } elseif (Test-Path -LiteralPath $cwdSettings -PathType Leaf) {
            $SettingsPath = $cwdSettings
        }
    }
}

if (-not $SamplePath) {
    if ($repoRoot) {
        $SamplePath = Join-Path $repoRoot "exampleImmFiles\sample1.imm"
    }
    if (-not $SamplePath -or -not (Test-Path -LiteralPath $SamplePath -PathType Leaf)) {
        $artifactSample = Join-Path $scriptDir "sample1.imm"
        $cwdSample = Join-Path (Get-Location) "sample1.imm"
        if (Test-Path -LiteralPath $artifactSample -PathType Leaf) {
            $SamplePath = $artifactSample
        } elseif (Test-Path -LiteralPath $cwdSample -PathType Leaf) {
            $SamplePath = $cwdSample
        }
    }
}

if (-not $OutputPath) {
    if ($repoRoot) {
        $OutputPath = Join-Path $repoRoot "build\baseline-captures\windows-directx-static.ppm"
    } else {
        $OutputPath = Join-Path $scriptDir "baseline-captures\windows-directx-static.ppm"
    }
}

$ViewerExe = Resolve-RequiredFile $ViewerExe "Viewer executable"
$SettingsPath = Resolve-RequiredFile $SettingsPath "DirectX baseline settings"
$SamplePath = Resolve-RequiredFile $SamplePath "IMM document"

$outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
$outputDir = Split-Path -Parent $outputFullPath
$outputBaseName = [System.IO.Path]::GetFileNameWithoutExtension($outputFullPath)
$viewerDir = Split-Path -Parent $ViewerExe
$viewerDebugLogPath = Join-Path $viewerDir "debug.txt"
$capturedDebugLogPath = Join-Path $outputDir "$outputBaseName.debug.txt"
New-Item -ItemType Directory -Force $outputDir | Out-Null

function Remove-FileIfPossible([string]$PathValue, [switch]$Required) {
    if (-not (Test-Path -LiteralPath $PathValue)) {
        return
    }

    for ($attempt = 1; $attempt -le 5; ++$attempt) {
        try {
            Remove-Item -LiteralPath $PathValue -Force -ErrorAction Stop
            return
        } catch {
            if ($attempt -eq 5) {
                if ($Required) {
                    throw
                }
                Write-Warning "Could not remove stale file '$PathValue': $($_.Exception.Message)"
                return
            }
            Start-Sleep -Milliseconds 250
        }
    }
}

function Copy-ViewerDebugLog {
    if (Test-Path -LiteralPath $viewerDebugLogPath -PathType Leaf) {
        Copy-Item -LiteralPath $viewerDebugLogPath -Destination $capturedDebugLogPath -Force
        Write-Host "Viewer debug log: $capturedDebugLogPath"
        Write-Host "----- viewer debug tail -----"
        Get-Content -LiteralPath $capturedDebugLogPath -Tail 200
        Write-Host "----- end viewer debug tail -----"
    } else {
        Write-Host "Viewer debug log was not written: $viewerDebugLogPath"
    }
}

Remove-FileIfPossible $outputFullPath -Required
Remove-FileIfPossible $viewerDebugLogPath
Remove-FileIfPossible $capturedDebugLogPath -Required

$runtimeSettingsPath = Join-Path $outputDir "settings-baseline-directx-sample1.runtime.json"
$runtimeSettings = Get-Content -LiteralPath $SettingsPath -Raw | ConvertFrom-Json
if (-not $runtimeSettings.PSObject.Properties["File"]) {
    $runtimeSettings | Add-Member -MemberType NoteProperty -Name "File" -Value ([pscustomobject]@{})
}
if ($runtimeSettings.File.PSObject.Properties["Load"]) {
    $runtimeSettings.File.Load = @($SamplePath)
} else {
    $runtimeSettings.File | Add-Member -MemberType NoteProperty -Name "Load" -Value @($SamplePath)
}
$runtimeSettingsJson = $runtimeSettings | ConvertTo-Json -Depth 16
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($runtimeSettingsPath, $runtimeSettingsJson, $utf8NoBom)

$env:IMM_VIEWER_VALIDATE_FRAME = "0"
$env:IMM_VIEWER_VALIDATE_MAX_FRAME = "360"
$env:IMM_VIEWER_VALIDATE_FIXED_DT = "0.0333333333333333"
$env:IMM_VIEWER_VALIDATE_PLAYER_FRAME = "$PlayerFrame"
$env:IMM_VIEWER_VALIDATE_MIN_NONZERO = "16"
$env:IMM_VIEWER_VALIDATE_MIN_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS = "$MinPictureDrawCalls"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS = "$MinPicture360DrawCalls"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS = "$MinPicture360EquirectDrawCalls"
$env:IMM_VIEWER_VALIDATE_MIN_TRIANGLES = "1"
$env:IMM_VIEWER_VALIDATE_CAPTURE_PATH = $outputFullPath
$env:IMM_VIEWER_VALIDATE_DISABLE_AUDIO = "1"

Write-Host "Running Windows DirectX baseline capture..."
Write-Host "Viewer:   $ViewerExe"
Write-Host "Settings: $runtimeSettingsPath"
Write-Host "Sample:   $SamplePath"
Write-Host "Output:   $outputFullPath"

$process = Start-Process -FilePath $ViewerExe -ArgumentList @($runtimeSettingsPath) -WorkingDirectory $viewerDir -WindowStyle Hidden -PassThru
$timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut) {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        Start-Sleep -Seconds 1
    }
    Copy-ViewerDebugLog
    throw "Windows DirectX baseline capture timed out after $TimeoutSeconds seconds"
}
$exitCode = $process.ExitCode
if ($exitCode -ne 0) {
    Copy-ViewerDebugLog
    throw "Windows DirectX baseline capture failed with exit code $exitCode"
}

if (-not (Test-Path -LiteralPath $outputFullPath -PathType Leaf)) {
    Copy-ViewerDebugLog
    throw "Windows DirectX baseline capture did not produce output: $outputFullPath"
}

Copy-ViewerDebugLog
Write-Host "Windows DirectX baseline capture written: $outputFullPath"
