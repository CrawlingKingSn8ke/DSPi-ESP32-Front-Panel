# DSPi ESP32 Front Panel

ESP32-S3 front panel for WeebLabs DSPi with rotary encoder control, BLE remote learning, source and DSP feature menus, stereo bar meters, and a combined analogue VU meter.

## v1.1.0-usability beta 1

The `v1.1.0-usability` branch is a hardware-test beta built from the protected v1.0.0 front-panel baseline and audited against WeebLabs DSPi `release/v1.1.5`.

- The analogue meter uses display-only calibration so ordinary programme peaks occupy more of the approved VU face. DSPi telemetry and the stereo meters remain unchanged.
- `Preset > Save Current` overwrites the active slot after a No/Yes confirmation, using DSPi's native complete-preset save command. DSPi remains the only owner of audio state and preset data.
- Preset and source shortcuts show clean, large distance-view confirmations.
- Home-screen shortcuts now include Up, Down, Left, Right and Centre, with previous/next preset and input choices. Existing four-direction assignments migrate automatically.
- `System > Screen Settings` contains timeout, dim level and brightness. Remote or encoder input wakes a dimmed or dark backlight with a non-blocking fade.
- Menu movement requires two encoder notches per item; home and VU volume control remains 0.5 dB per notch.
- `System > Volume Limit` is a device-wide safety ceiling. Applying it switches DSPi to Independent/Global master-volume mode, verifies the live value, and persists the value in DSPi.
- The System status screen no longer gives Psy Bass special treatment; Psy Bass remains a normal listening feature.

The beta was compiled for the listed board profile with ESP32 core 3.3.8, GFX Library for Arduino 1.6.5 and NimBLE-Arduino 2.5.0. It uses 1,001,726 bytes (31%) of the app partition and 38,000 bytes (11%) of dynamic memory.

Use [the beta hardware checklist](V1.1.0-USABILITY-BETA1-TEST-CHECKLIST.md) before promoting this branch to a stable release.

## Hardware

- Waveshare ESP32-S3-LCD-2, 320 x 240 https://thepihut.com/products/esp32-s3-development-board-with-2-ips-display-240-x-320
- Raspberry Pi Pico or Pico 2 running DSPi firmware v1.1.5-beta5
- Mechanical rotary encoder with push switch
- Optional BLE HID remote; tested with an Amazon Fire TV remote

## Wiring

### ESP32 to DSPi UART

| ESP32-S3-LCD-2 | DSPi Pico 2 | Function |
|---|---|---|
| GPIO16 | GPIO16 | ESP32 RX from DSPi TX |
| GPIO17 | GPIO17 | ESP32 TX to DSPi RX |
| GND | GND | Common ground |

The UART runs at 115200 baud. Enable the DSPi UART interface with TX GPIO16 and RX GPIO17 before using the panel.

### Rotary encoder

| Encoder | ESP32-S3-LCD-2 |
|---|---|
| CLK / A | GPIO47 |
| DT / B | GPIO48 |
| Push switch | GPIO21 |
| Common / switch return | GND |

For an encoder module, power it from 3.3 V, not 5 V.

### Power

Power the ESP32 through its 5V/VBUS input and share ground with the DSPi. Do not connect the two boards' 3.3 V rails together. Avoid connecting external 5 V and a powered USB cable to the ESP32 at the same time unless the supplies are isolated.

## Flash v1.1.0-usability beta 1 on Windows

1. Download or clone the `v1.1.0-usability` branch.
2. Install Python 3 from python.org if `py --version` does not show a version.
3. Connect the ESP32-S3-LCD-2 by USB and close Arduino Serial Monitor.
4. Open PowerShell in the project folder and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Flash-DSPi-Front-Panel-v1.1.0-usability-beta1.ps1"
```

To update the app while preserving BLE pairing and panel settings:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Flash-DSPi-Front-Panel-v1.1.0-usability-beta1.ps1" -PreserveSettings
```

The script installs or updates Python `pip` and `esptool` when needed, verifies both firmware SHA-256 hashes, and then flashes the panel. A clean install erases BLE pairing, key mappings, brightness, screen-power settings and shortcut assignments.

## Flash the stable v1.0.0 release on Windows

1. Download and extract the release ZIP.
2. Connect the ESP32-S3-LCD-2 by USB.
3. Close Arduino Serial Monitor.
4. Open PowerShell in the extracted folder and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Flash-DSPi-Front-Panel.ps1"
```

The script detects or asks for the COM port, installs `esptool` through Python when needed, verifies the firmware hashes, and performs a clean install.

To update the firmware while preserving BLE pairing and saved panel settings:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Flash-DSPi-Front-Panel.ps1" -PreserveSettings
```

A clean install erases saved BLE pairing, key mappings, brightness, and other panel settings.

## Build from source

Open:

```text
firmware\DSPi_ESP32_Front_Panel_v1_1_0_usability_beta1\DSPi_ESP32_Front_Panel_v1_1_0_usability_beta1.ino
```

Required libraries and board package:

- ESP32 Arduino core 3.3.8
- GFX Library for Arduino 1.6.5
- NimBLE-Arduino 2.5.0

Arduino board settings:

```text
Board: ESP32S3 Dev Module
USB Mode: Hardware CDC and JTAG
USB CDC On Boot: Enabled
CPU Frequency: 240 MHz
Flash Mode: QIO
Flash Size: 16 MB
Partition Scheme: 16M Flash (3MB APP / 9.9MB FATFS)
PSRAM: OPI
```

## BLE remote setup

With no remote saved, open `Remote > Find Remote`, select the device, then use `Remote > Key Map` to learn buttons. A saved remote must be removed before another remote can be searched for.

## Notes

- The panel controls DSPi over UART; it is not in the audio path.
- Input choices and feature availability follow the connected DSPi firmware and configuration.
- The VU meters show actual DSPi output peaks, not the volume-control position.
