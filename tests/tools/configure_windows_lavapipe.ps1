param(
    [Parameter(Mandatory = $true)]
    [string]$Msys2Location,
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDirectory
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
$setupLog = Join-Path $ArtifactDirectory "vulkan-driver-setup.txt"
$searchRoots = @($Msys2Location, "C:\msys64") |
    Where-Object { $_ -and (Test-Path $_) } |
    Select-Object -Unique
$mingwBin = Join-Path ($searchRoots | Select-Object -First 1) "mingw64\bin"
if (Test-Path $mingwBin) {
    $mingwBin | Out-File -FilePath $env:GITHUB_PATH -Append
}

"MSYS2_LOCATION=$Msys2Location" | Out-File -FilePath $setupLog
"MINGW64_BIN=$mingwBin" | Out-File -FilePath $setupLog -Append
$icds = @($searchRoots | ForEach-Object {
    Get-ChildItem -Path $_ -Recurse -Filter "lvp_icd*.json" -ErrorAction SilentlyContinue
})
$dlls = @($searchRoots | ForEach-Object {
    Get-ChildItem -Path $_ -Recurse -Filter "vulkan_lvp.dll" -ErrorAction SilentlyContinue
})
$sourceIcd = $icds | Where-Object { $_.FullName -like "*\mingw64\*" } | Select-Object -First 1
if (-not $sourceIcd) {
    $sourceIcd = $icds | Select-Object -First 1
}
if (-not $sourceIcd) {
    throw "Mesa lavapipe ICD was not found under: $($searchRoots -join ', ')"
}

$driverRoot = $sourceIcd.FullName -replace "\\share\\vulkan\\icd\.d\\.*$", ""
$sourceDll = Join-Path $driverRoot "bin\vulkan_lvp.dll"
if (-not (Test-Path $sourceDll)) {
    $sourceDll = ($dlls | Where-Object { $_.FullName -like "$driverRoot\*" } | Select-Object -First 1).FullName
}
if (-not $sourceDll -or -not (Test-Path $sourceDll)) {
    throw "Mesa lavapipe DLL was not found for $($sourceIcd.FullName)"
}

$icdDir = Join-Path $env:GITHUB_WORKSPACE "lavapipe-unity-vulkan"
New-Item -ItemType Directory -Force -Path $icdDir | Out-Null
$workspaceIcd = Join-Path $icdDir "lvp_icd.x86_64.json"
$icdJson = Get-Content -Raw $sourceIcd.FullName | ConvertFrom-Json
$icdJson.ICD.library_path = $sourceDll.Replace("\", "/")
$icdJson | ConvertTo-Json -Depth 10 | Out-File -FilePath $workspaceIcd -Encoding utf8

"Selected ICD=$($sourceIcd.FullName)" | Out-File -FilePath $setupLog -Append
"Selected DLL=$sourceDll" | Out-File -FilePath $setupLog -Append
"Workspace ICD=$workspaceIcd" | Out-File -FilePath $setupLog -Append
Get-Content -Raw $workspaceIcd | Out-File -FilePath $setupLog -Append
"VK_ICD_FILENAMES=$workspaceIcd" | Out-File -FilePath $env:GITHUB_ENV -Append
"VK_DRIVER_FILES=$workspaceIcd" | Out-File -FilePath $env:GITHUB_ENV -Append
"VK_ADD_DRIVER_FILES=$workspaceIcd" | Out-File -FilePath $env:GITHUB_ENV -Append
(Split-Path -Parent $sourceDll) | Out-File -FilePath $env:GITHUB_PATH -Append
