# DSPi ESP32 Front Panel v1.1.2

## New in this version

- Adds SD-card music playback for WAV, FLAC and MP3 files.
- Adds album and folder browsing, artwork display, pause, seek and track navigation.
- Adds Wi-Fi Music Transfer with browser-based upload, folder creation and deletion.
- Adds live upload progress, speed and estimated completion time.
- Adds safe transfer shutdown before returning to normal media use.
- Improves long-running playback stability by keeping SD reads bounded and cooperative.
- Keeps normal playback on the shared 20 MHz SD path with 10 MHz and 4 MHz fallbacks.
- Retains preset control, source selection, volume limit, display settings, meters and BLE remote support.

## Compatibility

This firmware targets the Waveshare ESP32-S3-LCD-2 front panel and the DSPi UART interface used by the existing project.
