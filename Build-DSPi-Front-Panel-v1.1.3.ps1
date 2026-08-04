[CmdletBinding()]
param(
    [switch]$SkipLibraryInstall
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$LegacyBuild = Join-Path $Root "Build-DSPi-Front-Panel-v1.1.2.ps1"
$ReleaseDir = Join-Path $Root "release"

if (-not (Test-Path -LiteralPath $LegacyBuild -PathType Leaf)) {
    throw "Proven v1.1.2 build wrapper not found: $LegacyBuild"
}

$arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $LegacyBuild
)
if ($SkipLibraryInstall) { $arguments += "-SkipLibraryInstall" }

& powershell.exe @arguments
if ($LASTEXITCODE -ne 0) {
    throw "DSPi ESP32 Front Panel v1.1.3 build failed with exit code $LASTEXITCODE."
}

$OldFull = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.2-Full.bin"
$OldApp = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.2.bin"
$NewFull = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.3-Full.bin"
$NewApp = Join-Path $ReleaseDir "DSPi-ESP32-Front-Panel-v1.1.3.bin"

foreach ($file in @($OldFull, $OldApp)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Expected build artifact not found: $file"
    }
}

Copy-Item -LiteralPath $OldFull -Destination $NewFull -Force
Copy-Item -LiteralPath $OldApp -Destination $NewApp -Force

$fullHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $NewFull).Hash
$appHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $NewApp).Hash
$rows = @(
    ("{0}  {1}" -f $fullHash, (Split-Path -Leaf $NewFull))
    ("{0}  {1}" -f $appHash, (Split-Path -Leaf $NewApp))
)
$rows | Set-Content -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS-v1.1.3.txt") -Encoding ASCII

Write-Host ""
Write-Host "DSPi ESP32 Front Panel v1.1.3 artifacts ready:" -ForegroundColor Green
Write-Host "  $NewFull"
Write-Host "  SHA256: $fullHash"
Write-Host "  $NewApp"
Write-Host "  SHA256: $appHash"
Write-Host "  $(Join-Path $ReleaseDir 'SHA256SUMS-v1.1.3.txt')"
Write-Host "No device was flashed."
