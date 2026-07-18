# DSPi ESP32 Front Panel

ESP32-S3 front panel for WeebLabs DSPi with rotary encoder control, BLE remote learning, source and DSP feature menus, stereo bar meters, and a combined analogue VU meter.

## Hardware

- Waveshare ESP32-S3-LCD-2, 320 x 240
- Raspberry Pi Pico or Pico 2 running DSPi firmware v1.1.5-beta5
- Mechanical rotary encoder with push switch
- Optional BLE HID remote; tested with an Amazon Fire TV remote

## Wiring

### ESP32 to DSPi UART

| ESP32-S3-LCD-2 | DSPi Pico/Pico 2 | Function |
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

## Flash the prebuilt release on Windows

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
firmware\DSPi_ESP32_Front_Panel_v1_0_0\DSPi_ESP32_Front_Panel_v1_0_0.ino
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
