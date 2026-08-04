# DSPi ESP32 Front Panel v1.1.3

## Compatibility update

Version 1.1.3 updates the public ESP32 front-panel firmware for DSPi firmware v1.1.5.

### Changes

- Add S/PDIF 4 as input-source value 6.
- Expand the front-panel S/PDIF inventory from three to four inputs.
- Parse `REQ_GET_SPDIF_INPUT_CONFIG` (`0xEF`) from the device-reported input count instead of requiring an exact five-byte, three-input response.
- Accept both the older three-input response and the current four-input response.
- Keep source selection, source readback, preset restoration and media-route restoration aligned with the expanded source enum.
- Preserve all v1.1.2 music-player, Wi-Fi transfer, BLE remote, display and VU functionality.

## DSPi compatibility

- DSPi firmware v1.1.5: supports all four selectable S/PDIF inputs.
- Earlier compatible DSPi firmware: remains supported; S/PDIF 4 stays hidden when it is not reported or enabled.

## Build environment

- ESP32 Arduino core 3.3.11
- GFX Library for Arduino 1.6.5
- NimBLE-Arduino 2.5.0
- SdFat 2.3.0
- Waveshare ESP32-S3-LCD-2 / ESP32S3 Dev Module
- QIO, 16 MB flash, 3 MB application / 9.9 MB FATFS, OPI PSRAM

## Upgrade

Use the application-only image at offset `0x10000` to preserve BLE pairing and panel settings. Use the full image at offset `0x0` only for a clean installation or recovery.
