param(
    [Parameter(Mandatory = $true)]
    [string]$ReferencePath,
    [Parameter(Mandatory = $true)]
    [string]$CandidatePath,
    [double]$MaxMeanAbsoluteError = 35.0,
    [double]$MaxRootMeanSquareError = 70.0,
    [double]$MinVisibleOverlap = 0.35
)

$ErrorActionPreference = "Stop"

function Read-Ppm {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "PPM file was not found: $Path"
    }

    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path))
    function Read-Token {
        while ($script:idx -lt $bytes.Length -and [char]$bytes[$script:idx] -match "\s") { $script:idx++ }
        $start = $script:idx
        while ($script:idx -lt $bytes.Length -and -not ([char]$bytes[$script:idx] -match "\s")) { $script:idx++ }
        [System.Text.Encoding]::ASCII.GetString($bytes, $start, $script:idx - $start)
    }

    $script:idx = 0
    $magic = Read-Token
    if ($magic -ne "P6") {
        throw "Unsupported PPM magic in ${Path}: $magic"
    }
    $width = [int](Read-Token)
    $height = [int](Read-Token)
    $maxValue = [int](Read-Token)
    if ($script:idx -lt $bytes.Length -and [char]$bytes[$script:idx] -match "\s") { $script:idx++ }

    $expectedPayload = $width * $height * 3
    $payload = $bytes.Length - $script:idx
    if ($maxValue -ne 255 -or $payload -ne $expectedPayload) {
        throw "Invalid PPM payload in ${Path}: max=$maxValue payload=$payload expected=$expectedPayload"
    }

    [pscustomobject]@{
        Path = $Path
        Width = $width
        Height = $height
        Bytes = $bytes
        Offset = $script:idx
    }
}

$reference = Read-Ppm $ReferencePath
$candidate = Read-Ppm $CandidatePath

if ($reference.Width -ne $candidate.Width -or $reference.Height -ne $candidate.Height) {
    throw "PPM dimensions differ: reference=$($reference.Width)x$($reference.Height) candidate=$($candidate.Width)x$($candidate.Height)"
}

$pixels = $reference.Width * $reference.Height
$channels = $pixels * 3
$sumAbs = 0.0
$sumSq = 0.0
$referenceVisible = 0
$candidateVisible = 0
$visibleOverlap = 0
$candidateOnlyVisible = 0
$referenceOnlyVisible = 0
$maxAbs = 0

for ($pixel = 0; $pixel -lt $pixels; $pixel++) {
    $base = $pixel * 3
    $rr = $reference.Bytes[$reference.Offset + $base]
    $rg = $reference.Bytes[$reference.Offset + $base + 1]
    $rb = $reference.Bytes[$reference.Offset + $base + 2]
    $cr = $candidate.Bytes[$candidate.Offset + $base]
    $cg = $candidate.Bytes[$candidate.Offset + $base + 1]
    $cb = $candidate.Bytes[$candidate.Offset + $base + 2]

    $refIsVisible = ($rr -gt 32) -or ($rg -gt 32) -or ($rb -gt 32)
    $candidateIsVisible = ($cr -gt 32) -or ($cg -gt 32) -or ($cb -gt 32)
    if ($refIsVisible) { $referenceVisible++ }
    if ($candidateIsVisible) { $candidateVisible++ }
    if ($refIsVisible -and $candidateIsVisible) {
        $visibleOverlap++
    } elseif ($candidateIsVisible) {
        $candidateOnlyVisible++
    } elseif ($refIsVisible) {
        $referenceOnlyVisible++
    }

    $dr = [Math]::Abs($rr - $cr)
    $dg = [Math]::Abs($rg - $cg)
    $db = [Math]::Abs($rb - $cb)
    $sumAbs += $dr + $dg + $db
    $sumSq += ($dr * $dr) + ($dg * $dg) + ($db * $db)
    if ($dr -gt $maxAbs) { $maxAbs = $dr }
    if ($dg -gt $maxAbs) { $maxAbs = $dg }
    if ($db -gt $maxAbs) { $maxAbs = $db }
}

$mae = $sumAbs / $channels
$rmse = [Math]::Sqrt($sumSq / $channels)
$overlapRatio = if ($referenceVisible -gt 0) { $visibleOverlap / $referenceVisible } else { 0.0 }

Write-Host "PPM comparison"
Write-Host "Reference: $ReferencePath"
Write-Host "Candidate: $CandidatePath"
Write-Host "Pixels: $($reference.Width)x$($reference.Height)"
Write-Host ("Error: mae={0:N3} rmse={1:N3} maxAbs={2}" -f $mae, $rmse, $maxAbs)
Write-Host ("Visible: reference={0} candidate={1} overlap={2} overlapRatio={3:N3} referenceOnly={4} candidateOnly={5}" -f $referenceVisible, $candidateVisible, $visibleOverlap, $overlapRatio, $referenceOnlyVisible, $candidateOnlyVisible)

if ($mae -gt $MaxMeanAbsoluteError) {
    throw "Mean absolute error $mae exceeds threshold $MaxMeanAbsoluteError"
}
if ($rmse -gt $MaxRootMeanSquareError) {
    throw "Root mean square error $rmse exceeds threshold $MaxRootMeanSquareError"
}
if ($overlapRatio -lt $MinVisibleOverlap) {
    throw "Visible overlap ratio $overlapRatio is below threshold $MinVisibleOverlap"
}
