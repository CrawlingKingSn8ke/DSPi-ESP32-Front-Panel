# DSPi ESP32 Front Panel v1.2.0

## VU IMP interface and reliability release

Version 1.2.0 brings the complete VU IMP visual system, clearer preset and settings navigation, DSPi Console change notifications, and hardened long-running 48 kHz SD playback to the public front-panel firmware.

### Theme and display

- Add independent Main Text, Accent, Volume Meters and Analog VU colour settings under `System Setup > Screen Settings > Theme`.
- Add a thirty-colour palette with three shades per colour family for text, accents and digital/Home meters.
- Keep warning, fault, clip, success and black-background safety colours fixed.
- Preserve the original white/cyan appearance as the migration-safe default.
- Add Cyan, Green, honey-gold Amber, Magenta, Warm White and Red analogue VU faces.
- Preserve the approved analogue artwork, scale, shading, black curve, text, needle geometry, calibration and meter ballistics.
- Cache custom analogue faces once in PSRAM so every colour animates through the same lightweight frame path as original Cyan.
- Add configurable Dim, Screen Off, Digital VU and Analog VU idle actions with safe wake/return behaviour.
- Keep the Now Playing album/folder line at full brightness while retaining the original static two-line song-title layout.

### Menus and presets

- Redesign System Setup, Screen Settings, Idle Screen and Theme as consistent list views.
- Present all ten preset slots in a compact music-browser-sized list, including empty slots.
- Keep short Select as an in-list preset load without forcing a Home overlay.
- Use a five-second Select hold to stop music safely and save the highlighted preset after explicit confirmation.
- Save DSPi feature parameters, User Volume and the associated panel screen/theme settings with the preset.
- Use 1 dB User Volume steps throughout the panel controls.

### DSPi runtime integration

- Confirm Console-originated User Volume, preset, source, Loudness, Crossfeed, Leveller and Psy Bass changes through exact UART readbacks.
- Use the same Home full-screen notification path for confirmed Console changes and local encoder/D-pad/BLE actions.
- Prevent boot synchronisation and unchanged polling from creating false notifications.
- Keep music on the normal DSPi I2S Home source rather than a separate Home layer.
- Stop local music when another input takes ownership; starting local music selects the compatible I2S route.
- Keep valid PCM sample-rate metadata in the selected Accent colour on every input.

### Music-player reliability

- Keep 44.1 and 48 kHz WAV, FLAC and MP3 playback, seeking, folder navigation, artwork and automatic track advance.
- Coalesce held browser navigation and defer expensive scans/redraws when the PCM reserve is low.
- Isolate I2S output and decoder scheduling from long automatic Bluetooth reconnect scans.
- Defer background BLE reconnect scans during timing-critical playback while retaining connected remote reports.
- Prevent hidden album-art work from continuing after leaving Now Playing.
- Add silent underrun-event, longest-underrun, last-event and PCM low-water telemetry to serial command `s`.

## DSPi compatibility

- DSPi firmware v1.1.5/V28 and compatible v1.1.6 beta protocol builds.
- Four-input S/PDIF inventory including S/PDIF 4 when reported and enabled.
- Older compatible three-input DSPi configurations remain supported.

## Build environment

- ESP32 Arduino core 3.3.11
- GFX Library for Arduino 1.6.5
- NimBLE-Arduino 2.5.0
- SdFat 2.3.0
- Waveshare ESP32-S3-LCD-2 / ESP32S3 Dev Module
- QIO, 16 MB flash, 3 MB application / 9.9 MB FATFS, OPI PSRAM

## Verified release images

- Application image, 1,894,336 bytes:
  `CEF24D1A44462CC346BF78C755D02EDC37EF4AD61C6F4CEB3D98F83261702788`
- Full merged image, 16,777,216 bytes:
  `9EE74D3E646D23A6DA0B2A2F2A075BA05B2EF5F658731439FAFCAECB77C20749`

## Upgrade

Use the application-only image at offset `0x10000` to preserve BLE pairing, learned mappings and panel settings. Use the full image at offset `0x0` only for a clean installation or recovery.
