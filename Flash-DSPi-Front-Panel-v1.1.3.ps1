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
$LegacyFlash = Join-Path $Root "Flash-DSPi-Front-Panel-v1.1.2.ps1"

if (-not (Test-Path -LiteralPath $LegacyFlash -PathType Leaf)) {
    throw "Proven v1.1.2 flash wrapper not found: $LegacyFlash"
}

$arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $LegacyFlash,
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
    throw "DSPi ESP32 Front Panel v1.1.3 failed with exit code $LASTEXITCODE."
}

# The underlying build engine keeps its historical v1.1.2 names. Copy any
# generated release artifacts to the public v1.1.3 names after a successful run.
$ReleaseDir = Join-Path $Root "release"
$OldFull = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.2-Full.bin"
$OldApp = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.2.bin"
$NewFull = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.3-Full.bin"
$NewApp = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.3.bin"

if (Test-Path -LiteralPath $OldFull -PathType Leaf) {
    Copy-Item -LiteralPath $OldFull -Destination $NewFull -Force
}
if (Test-Path -LiteralPath $OldApp -PathType Leaf) {
    Copy-Item -LiteralPath $OldApp -Destination $NewApp -Force
}
if ((Test-Path -LiteralPath $NewFull -PathType Leaf) -and
    (Test-Path -LiteralPath $NewApp -PathType Leaf)) {
    @(
        "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $NewFull).Hash, (Split-Path -Leaf $NewFull),
        "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $NewApp).Hash, (Split-Path -Leaf $NewApp)
    ) | Set-Content -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS-v1.1.3.txt") -Encoding ASCII
}

Write-Host "DSPi ESP32 Front Panel v1.1.3 wrapper completed successfully." -ForegroundColor Green
