# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-10-10

### Added
- **Cross-Platform Host Simulator & Emulator**:
  - Native host execution with SDL2 display rendering and libcurl network transport for macOS, Linux, and NixOS ([#7](https://github.com/renebohne/screentinker_mcu/pull/7)).
  - Interactive 6-digit PIN device pairing flow (`--pair`) with local or remote ScreenTinker servers.
  - Interactive runtime controls: space/enter manual trigger, battery level adjustment, low-battery toggle, and headless mode.
  - Unified PlatformIO native build environment (`pio run -e simulator`).
- **Device Telemetry & Remote Control**:
  - Real-time Wi-Fi signal strength reporting via `X-ST-Device-RSSI` header ([#4](https://github.com/renebohne/screentinker_mcu/pull/4), [#5](https://github.com/renebohne/screentinker_mcu/pull/5)).
  - Firmware version reporting via `X-ST-Firmware-Version` header ([#4](https://github.com/renebohne/screentinker_mcu/pull/4)).
  - Texas Instruments BQ27220 fuel gauge I2C driver reporting battery State-of-Charge via `X-ST-Device-Battery` ([#5](https://github.com/renebohne/screentinker_mcu/pull/5)).
  - Hardware USB power presence (`X-ST-Device-USB-Power`) and charging state (`X-ST-Device-Charging`) headers ([#7](https://github.com/renebohne/screentinker_mcu/pull/7)).
  - Remote device reboot command support via `X-ST-Command: reboot` header ([#7](https://github.com/renebohne/screentinker_mcu/pull/7)).
- **Diagnostics & Error Handling**:
  - Informative Empty Playlist diagnostic screen on HTTP 404 (`NO PLAYLIST ASSIGNED` / `KEINE PLAYLIST ZUGEWIESEN`) with paired display name, QR code, and bilingual layout ([#5](https://github.com/renebohne/screentinker_mcu/pull/5)).
  - HTTP 501 Unsupported Content fallback diagnostic screen ([#7](https://github.com/renebohne/screentinker_mcu/pull/7)).
  - Offline screen rendering harness (`tools/screen_renderer/`) for generating PNG snapshots of all system screens ([#5](https://github.com/renebohne/screentinker_mcu/pull/5)).
- **Power Management & Server Coordination**:
  - Support for server-coordinated power schedules and deep sleep via `X-ST-Power-State` and `X-ST-Sleep-Seconds` headers ([#2](https://github.com/renebohne/screentinker_mcu/pull/2)).
  - Serial logging for server power states (`on` vs. `scheduled_off`) ([#2](https://github.com/renebohne/screentinker_mcu/pull/2)).
- **Build & Platform Support**:
  - Custom board definition `boards/esp32-s3-devkitc1-n8r8.json` for PlatformIO ESP32-S3-WROOM-1-N8R8 build targets ([#7](https://github.com/renebohne/screentinker_mcu/pull/7)).
  - WebSerial Flasher manifest updated for version 1.2.0 ([#6](https://github.com/renebohne/screentinker_mcu/pull/6)).
  - 1-Click Browser Web Flasher hosted live at [https://www.rene-bohne.de/screentinker_mcu/tools/sticky-installer.html](https://www.rene-bohne.de/screentinker_mcu/tools/sticky-installer.html) (mirror: [GitHub Pages](https://renebohne.github.io/screentinker_mcu/tools/sticky-installer.html)).

### Changed
- **Guarded Adaptive Sleep Backoff**: When the server requests long sleep intervals ($\ge 1800\,\text{s}$, such as night-time or inactive power schedules), consecutive HTTP 304 responses will no longer clamp the sleep down to 300 s (5 min) or 1800 s (30 min) ([#2](https://github.com/renebohne/screentinker_mcu/pull/2)).
- Bumped firmware version string from `1.1.0` to `1.2.0` across configuration headers, onboarding screens, WebSerial manifest, and telemetry headers ([#6](https://github.com/renebohne/screentinker_mcu/pull/6)).

## [1.1.0] - 2024-09-04

### Added
- Seamless 6-digit PIN pairing mechanism with ScreenTinker server.
- Multi-zone and fullscreen layout rendering options (`/api/embedded/render-layout`).
- On-device system menu (hold OK for 1.5 s).
- Bilingual onboarding support (English and German).
- WebSerial installer and captive portal provisioning.
- 8 MB PSRAM in-memory frame cache supporting up to 32 frames.
