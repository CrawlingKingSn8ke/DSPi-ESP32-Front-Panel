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
$BaseEngine = Join-Path $Root "Build-and-Flash-DSPi-Front-Panel-v1.2.0.ps1"
$GeneratedEngine = Join-Path $Root ".Build-and-Flash-DSPi-Front-Panel-v1.2.1.generated.ps1"

if (-not (Test-Path -LiteralPath $BaseEngine -PathType Leaf)) {
    throw "v1.2.0 build engine not found: $BaseEngine"
}

$source = Get-Content -LiteralPath $BaseEngine -Raw
$needle = '$ReleaseVersion = "1.2.0"'
$replacement = '$ReleaseVersion = "1.2.1"'
if (-not $source.Contains($needle)) {
    throw "Could not locate the v1.2.0 release-version declaration in $BaseEngine"
}
$source = $source.Replace($needle, $replacement)
Set-Content -LiteralPath $GeneratedEngine -Value $source -Encoding UTF8

$arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $GeneratedEngine,
    "-Port", $Port,
    "-Baud", $Baud
)
if ($Erase) { $arguments += "-Erase" }
if ($PreserveSettings) { $arguments += "-PreserveSettings" }
if ($BuildOnly) { $arguments += "-BuildOnly" }
if ($FlashOnly) { $arguments += "-FlashOnly" }
if ($SkipLibraryInstall) { $arguments += "-SkipLibraryInstall" }

try {
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "DSPi ESP32 Front Panel v1.2.1 failed with exit code $LASTEXITCODE."
    }
}
finally {
    Remove-Item -LiteralPath $GeneratedEngine -Force -ErrorAction SilentlyContinue
}
