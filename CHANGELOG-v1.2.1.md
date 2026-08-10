# DSPi ESP32 Front Panel v1.2.1

## Playback-stability maintenance release

Version 1.2.1 republishes the current tested `main` firmware state as a clean, traceable release so the Git tag, source archive and generated binaries all correspond to the same code.

### Music-player reliability

- Include the post-v1.2.0 SD/FLAC playback stabilisation from commit `fa5ac846818a0387da7b86320035135c5716f6da`.
- Increase decoded PCM reserve capacity with preferred, fallback and emergency PSRAM ring sizes.
- Keep I2S output isolated on CPU1 while allowing decoder/SD work to run on either core.
- Use 4 KiB decoder SD read slices for more efficient contiguous reads.
- Raise low-ring protection thresholds and add detailed SD/decoder/ring telemetry to serial command `s`.

### Other fixes carried from current main

- Document the tested ESP32-to-DSPi I2S wiring.
- Fix occupied preset overwrite acknowledgement timing on compatible DSPi firmware.

## Build environment

- ESP32 Arduino core 3.3.11
- GFX Library for Arduino 1.6.5
- NimBLE-Arduino 2.5.0
- SdFat 2.3.0
- Waveshare ESP32-S3-LCD-2 / ESP32S3 Dev Module
- QIO, 16 MB flash, 3 MB application / 9.9 MB FATFS, OPI PSRAM

## Upgrade

Use the application-only image at offset `0x10000` to preserve BLE pairing, learned mappings and panel settings. Use the full merged image at offset `0x0` for a clean installation or recovery.

The release binaries should be generated with `Build-DSPi-Front-Panel-v1.2.1.ps1`; use the resulting `SHA256SUMS-v1.2.1.txt` values when publishing the GitHub release.
