param(
    [string]$Port,
    [int]$Baud = 921600,
    [switch]$PreserveSettings
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$FullBin = Join-Path $Root "release\DSPi_ESP32_Front_Panel_v1_1_0_usability_beta1_Full.bin"
$AppBin = Join-Path $Root "release\DSPi_ESP32_Front_Panel_v1_1_0_usability_beta1.bin"
$ExpectedFullHash = "607693DC221180EF3298B0C74614AB77A020091CDB908FFB3830385776234E9B"
$ExpectedAppHash = "CE37060A0BDA4B5744D54D584D07B542393334287DDA550A281BF769430AFE91"

Set-Location -LiteralPath $Root

if (-not (Test-Path -LiteralPath $FullBin)) { throw "Missing full firmware image: $FullBin" }
if (-not (Test-Path -LiteralPath $AppBin)) { throw "Missing update firmware image: $AppBin" }

$FullHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $FullBin).Hash
$AppHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $AppBin).Hash
if ($FullHash -ne $ExpectedFullHash) { throw "Full firmware image hash mismatch." }
if ($AppHash -ne $ExpectedAppHash) { throw "Update firmware image hash mismatch." }

$Python = $null
if (Get-Command py -ErrorAction SilentlyContinue) {
    $Python = "py"
}
elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $Python = "python"
}
else {
    throw "Python 3 was not found. Install Python from python.org, then run this script again."
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    $Ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    if ($Ports.Count -eq 1) {
        $Port = $Ports[0]
    }
    else {
        Write-Host "Available serial ports:" -ForegroundColor Cyan
        if ($Ports.Count -eq 0) {
            Write-Host "  No COM ports detected. Connect the ESP32-S3 with USB and retry." -ForegroundColor Yellow
        }
        else {
            $Ports | ForEach-Object { Write-Host "  $_" }
        }
        $Port = Read-Host "Enter the ESP32 COM port, for example COM7"
    }
}

if ([string]::IsNullOrWhiteSpace($Port)) { throw "No COM port was supplied." }

Write-Host "Checking esptool..." -ForegroundColor Cyan
& $Python -m esptool version *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Installing esptool..." -ForegroundColor Cyan
    & $Python -m pip install --user --upgrade esptool
    if ($LASTEXITCODE -ne 0) { throw "esptool installation failed." }
}

Write-Host ""
Write-Host "DSPi ESP32 Front Panel v1.1.0-usability beta 1" -ForegroundColor Cyan
Write-Host "Board: Waveshare ESP32-S3-LCD-2"
Write-Host "Port:  $Port"
Write-Host "Baud:  $Baud"
if ($PreserveSettings) {
    Write-Host "Mode:  Firmware update; BLE pairing and panel settings are preserved." -ForegroundColor Green
}
else {
    Write-Host "Mode:  Clean install; BLE pairing and panel settings are erased." -ForegroundColor Yellow
}
Write-Host ""
Write-Host "Close Arduino Serial Monitor and any program using $Port." -ForegroundColor Yellow
Read-Host "Press ENTER to flash"

if ($PreserveSettings) {
    & $Python -m esptool --chip esp32s3 --port $Port --baud $Baud --before default-reset --after hard-reset write-flash 0x10000 $AppBin
}
else {
    & $Python -m esptool --chip esp32s3 --port $Port --baud $Baud --before default-reset --after no-reset erase-flash
    if ($LASTEXITCODE -ne 0) { throw "Flash erase failed." }
    & $Python -m esptool --chip esp32s3 --port $Port --baud $Baud --before default-reset --after hard-reset write-flash 0x0 $FullBin
}

if ($LASTEXITCODE -ne 0) { throw "Firmware flash failed." }
Write-Host ""
Write-Host "DSPi ESP32 Front Panel v1.1.0-usability beta 1 flashed successfully." -ForegroundColor Green
