[CmdletBinding()]
param(
    [string]$Port = "COM10",
    [int]$Baud = 921600,
    [switch]$Erase,
    [switch]$PreserveSettings,
    [switch]$BuildOnly,
    [switch]$FlashOnly,
    [switch]$SkipLibraryInstall
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$EncodedPatch = Join-Path $Root "firmware\DSPi-ESP32-Front-Panel-v1.1.2.patch.gz.b64"
$CompressedPatch = Join-Path $Root "firmware\DSPi-ESP32-Front-Panel-v1.1.2.patch.gz"
$PatchFile = Join-Path $Root "firmware\DSPi-ESP32-Front-Panel-v1.1.2.patch"
$Engine = Join-Path $Root "Build-and-Flash-DSPi-Front-Panel-v1.1.2.ps1"

if (-not (Test-Path -LiteralPath $EncodedPatch -PathType Leaf)) {
    throw "Firmware source update not found: $EncodedPatch"
}
if (-not (Test-Path -LiteralPath $Engine -PathType Leaf)) {
    throw "Build and flash engine not found: $Engine"
}

# Recreate the patch on every run. This avoids stale or partially extracted
# files left by an interrupted earlier build.
$base64 = (Get-Content -LiteralPath $EncodedPatch -Raw) -replace '\s', ''
$compressedBytes = [Convert]::FromBase64String($base64)
[System.IO.File]::WriteAllBytes($CompressedPatch, $compressedBytes)

$input = [System.IO.File]::OpenRead($CompressedPatch)
try {
    $gzip = New-Object System.IO.Compression.GZipStream(
        $input,
        [System.IO.Compression.CompressionMode]::Decompress
    )
    try {
        $output = [System.IO.File]::Create($PatchFile)
        try { $gzip.CopyTo($output) }
        finally { $output.Dispose() }
    }
    finally { $gzip.Dispose() }
}
finally { $input.Dispose() }

$arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $Engine,
    "-Port", $Port,
    "-Baud", $Baud
)
if ($Erase) { $arguments += "-Erase" }
if ($PreserveSettings) { $arguments += "-PreserveSettings" }
if ($BuildOnly) { $arguments += "-BuildOnly" }
if ($FlashOnly) { $arguments += "-FlashOnly" }
if ($SkipLibraryInstall) { $arguments += "-SkipLibraryInstall" }

& powershell.exe @arguments
if ($LASTEXITCODE -ne 0) {
    throw "DSPi ESP32 Front Panel v1.1.2 failed with exit code $LASTEXITCODE."
}
