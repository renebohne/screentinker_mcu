# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.2] - 2026-10-30

### Added
- **Elecrow CrowPanel 4.3" HMI Support (`elecrow_crowpanel_43`)**:
  - 480×272 RGB LCD support with NV3047 controller over 16-bit parallel RGB interface.
  - ESP-IDF RGB bounce-buffer driver in internal SRAM (19.2 KB bounce buffer @ 6 MHz PCLK) to prevent PSRAM bandwidth starvation and visual tearing during Wi-Fi and TLS transfers.
  - On-the-fly promotion of 1bpp local system UI (setup screen, System Menu, Status Screen) to RGB565.
  - Physical BOOT button single-button navigation (Short Press: cycle menu, Long Press: select, Very Long Press: factory reset).
  - Compact adaptive UI layout for displays with height < 350px.
  - Web Flasher manifest and precompiled binaries for 1-click browser flashing (`tools/manifest_crowpanel_43.json`).
- **Web Flasher Target Expansion (`tools/index.html`, `tools/sticky-installer.html`)**:
  - Added RGB LCD Display target group featuring the Elecrow CrowPanel 4.3".

### Fixed
- **Multi-bit Offline Fallback Badge Safety (`src/main.cpp`)**:
  - Gated 1-bit monochrome offline badge drawing and rotation strictly to `COLOR_MODE_1BPP_MONO`, preventing visual buffer scrambling when multi-bit displays (RGB565, Spectra 6, Gray16) are offline.

## [1.4.1] - 2026-10-30

### Added
- **Bilingual Web Flasher UI (`tools/index.html`, `tools/sticky-installer.html`)**:
  - English as default UI language with an instant `[ 🇬🇧 English | 🇩🇪 Deutsch ]` language toggle persisted in `localStorage`.

### Fixed
- **Dynamic Matrix Pairing Dimensions (`src/main.cpp`)**:
  - `registerAndStartPairing()` now transmits the runtime-configured LED matrix `width` and `height` (`led_driver_get_matrix_config()`) instead of the compile-time `5×1` fallback during `POST /api/embedded/pair/register`.
- **Web Flasher Provisioning Order (`tools/index.html`, `tools/sticky-installer.html`)**:
  - WebSerial provisioning now sends `matrix` and `led` configuration commands *before* `config`, ensuring NVS matrix dimensions are updated before the device connects to Wi-Fi and registers for pairing.
- **Linux GCC CI Build Compatibility (`src/led_driver.cpp`)**:
  - Added `<cmath>` include for `sinf` on Linux host/CI builds.

## [1.4.0] - 2026-10-30

### Added
- **Smart LED Strip & Matrix Support (WS2812 / SK6812 RGB888)**:
  - Universal physical LED matrix coordinate mapping engine (`include/led_matrix_map.h`) supporting serpentine (zig-zag) and progressive (linear) wiring, row-major and column-major axes, and arbitrary starting origins (`TOP_LEFT`, `TOP_RIGHT`, `BOTTOM_LEFT`, `BOTTOM_RIGHT`).
  - Runtime matrix topology configuration via USB/WebSerial JSON command `matrix`, ESP32 NVS persistence across reboots, and dynamic server pairing profiles.
  - Comprehensive documentation guides in `docs/led_mode_guide.md` and `docs/led_matrix_wiring_guide.md` covering physical wiring, power injection, commercial matrix standards, and content fit modes (`cover`, `contain`, `fill`).
  - Arbitrary matrix and strip dimension support via `--width <px>` and `--height <px>` (e.g. 5×1 strips, 32×8 banners, 64×64 matrices).
  - Authentic SMD LED package renderer in SDL2 simulator: rounded-square diode beads, dark bezels, unlit emitter chips, and glowing lens center highlights.
  - Parametric pairing registration: transmits custom matrix dimensions within the `screen_profile` object (`{"preset":"seeed-xiao-led","width":W,"height":H,...}`) so the server persists arbitrary resolutions without needing dedicated preset entries.
  - Sample LED test patterns and pixel art in `examples/media/5x1/` and `examples/pixel_art_64x64/`.

### Fixed
- **Simulator Dimension Persistence**:
  - `SimConfig::load` and `SimConfig::save` now parse and persist custom `width` and `height` in JSON configs, preventing accidental reversion to 5×1 on restarts.
- **Physical ESP32-C6 LED Strip**:
  - Restored pin configurations (`PIN_NEOPIXEL = 20`, `NUM_LEDS = 5`) and initialized WS2812 RMT driver for real Seeed Studio XIAO hardware.
- **Unit Test Modularization**:
  - Separated `test_matrix` into its own isolated test environment under `test/test_matrix/`, resolving duplicate symbol link collisions with `test_imu`.

## [1.3.1] - 2026-10-29

### Added
- **Capacitive Touchscreen Navigation for System Menu & Status**:
  - GT911 hardware initialization sequence with 50 ms `INT` LOW synchronization hold and 100 kHz I2C mode for reliable capacitive scanning engine startup.
  - Direct touch hit-testing for System Menu items 0–8 and tap-to-return from Status screen.

### Changed
- **Touch Interaction Scope**:
  - Restricted touchscreen processing exclusively to the on-screen System Menu and Status Screen; touching the active image / slide no longer opens settings.
  - System Menu can only be opened via the physical hardware OK button (`PIN_BTN_OK`, GPIO 4) or serial command.
  - Placed touch controller in low-power sleep mode during slide display to conserve battery and eliminate unnecessary I2C polling.
- Bumped firmware version from `1.3.0` to `1.3.1`.

### Fixed
- **Battery Charging**: Enabled TI BQ25616 charger IC via `PIN_CHG_EN` (GPIO 7) and held low across deep sleep so connected batteries charge while running or in deep sleep.

## [1.3.0] - 2026-10-13

### Added
- **Seeed Studio reTerminal E-Series Multi-Device Platform**:
  - Modular hardware abstraction and pinout definitions for reTerminal Sticky (3.97" 800×480 mono SSD1677), E1001 (7.5" 800×480 1bpp mono UC8179), E1002 (7.3" 800×480 6-color Spectra 6 ED2208), E1003 (10.3" 1872×1404 16-level grayscale IT8951), and E1004 (13.3" 1200×1600 6-color Spectra 6 T133A01) ([#18](https://github.com/renebohne/screentinker_mcu/pull/18), [#20](https://github.com/renebohne/screentinker_mcu/pull/20)).
  - PlatformIO multi-target build environments in `platformio.ini` (`seeed_reterminal_sticky`, `seeed_reterminal_e1001`, `seeed_reterminal_e1002`, `seeed_reterminal_e1003`, `seeed_reterminal_e1004`) ([#19](https://github.com/renebohne/screentinker_mcu/pull/19)).
  - Responsive setup, splash, pairing, and status screens adapting to all resolutions and color formats ([#17](https://github.com/renebohne/screentinker_mcu/pull/17)).
  - Desktop simulator multi-device support with dynamic SDL2 window sizing and `--device <profile>` flag ([#16](https://github.com/renebohne/screentinker_mcu/pull/16)).
- **Display Orientation Control & IMU Auto-Rotation**:
  - STMicroelectronics LSM6DS3TR-C 6-axis IMU driver over I2C with hardware axis detection and 350ms hysteresis debouncing ([#21](https://github.com/renebohne/screentinker_mcu/commit/19a93bc)).
  - On-device System Menu setting for Orientation mode: `Auto (IMU)`, `Landscape`, and `Portrait`.
  - Serial commands and JSON API for orientation control (`set_orientation`, `orientation`, `imu`, `imu_debug`).
  - Dedicated IMU calibration test screen mode with large orientation cards (A, B, C, D) and raw accelerometer readouts.
- **Environmental Sensor Telemetry**:
  - Sensirion SHT40 high-accuracy relative humidity and temperature sensor I2C driver ([#22](https://github.com/renebohne/screentinker_mcu/commit/2c7abdb)).
  - Telemetry headers `X-ST-Device-Temperature` and `X-ST-Device-Humidity` reported on every server sync.
  - SHT40 temperature and humidity metrics displayed on the on-device E-Paper Status Screen and in serial JSON status output.
- **Dual-Partition Over-The-Air (OTA) Updates**:
  - Robust A/B dual-partition HTTP firmware update engine (`ota_updater.h`, `ota_updater.cpp`) with SHA-256 chunked stream validation ([#8](https://github.com/renebohne/screentinker_mcu/commit/7af2c7f)).
  - Automated boot rollback protection: new firmware is verified upon successful server connection and frame sync; failed boots or network drops trigger automatic rollback to the previous partition.
  - Remote OTA update triggering via `X-ST-Command: update <url> [sha256]` or serial commands (`update`, `rollback`).
  - On-screen bilingual "Updating Firmware..." E-Paper splash screen during transfer.
- **Rolling Diagnostic Log Buffer**:
  - In-memory 100-entry circular diagnostic log ring buffer with level filtering (`diag_logger.h`, `diag_logger.cpp`) ([#9](https://github.com/renebohne/screentinker_mcu/commit/4ab18bd)).
  - Remote log buffer upload via `X-ST-Command: upload_logs` header dispatched to `POST /api/embedded/logs`.
  - Serial CLI command `logs` and JSON export for troubleshooting without USB debugging.

### Changed
- Bumped firmware version from `1.2.0` to `1.3.0` across configuration headers, onboarding screens, and telemetry headers.
- Fixed portrait display buffer stride and rotation mapping to align perfectly with server-rendered frames without double-inversion.

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
