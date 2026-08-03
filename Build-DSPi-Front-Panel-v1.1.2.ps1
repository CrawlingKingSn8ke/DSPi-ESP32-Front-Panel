[CmdletBinding()]
param(
    [switch]$SkipLibraryInstall
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$FlashScript = Join-Path $Root "Flash-DSPi-Front-Panel-v1.1.2.ps1"

$arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $FlashScript,
    "-BuildOnly"
)
if ($SkipLibraryInstall) { $arguments += "-SkipLibraryInstall" }

& powershell.exe @arguments
if ($LASTEXITCODE -ne 0) {
    throw "DSPi ESP32 Front Panel v1.1.2 build failed with exit code $LASTEXITCODE."
}
