# ScreenTinker Firmware for Seeed Studio reTerminal & reTerminal E-Series

Ultra-low-power, high-performance C++ firmware for the **[Seeed Studio reTerminal Sticky](https://www.seeedstudio.com/reTerminal-Sticky-p-6861.html)** and the entire **[reTerminal E-Series](https://www.seeedstudio.com/)** (E1001, E1002, E1003, E1004) targeting the **ScreenTinker Digital Signage Server**.

---

## Key Features

- **Multi-Device E-Series Platform Support:**
  - **reTerminal Sticky**: 3.97" 800×480 Monochrome E-Paper (SSD1677 driver, 1bpp).
  - **reTerminal E1001**: 7.5" 800×480 Monochrome / 4-Level Grayscale (UC8179 driver).
  - **reTerminal E1002**: 7.3" 800×480 6-Color Spectra 6 (ED2208 driver, 4bpp).
  - **reTerminal E1003**: 10.3" 1872×1404 16-Level Grayscale (IT8951 controller with touch, 4bpp).
  - **reTerminal E1004**: 13.3" 1200×1600 6-Color Spectra 6 (T133A01 dual-controller, 4bpp).
- **Dynamic Display Orientation & IMU Auto-Rotation:**
  - STMicroelectronics LSM6DS3TR-C 6-axis IMU automatically detects gravity orientation and rotates display buffers in real time (Landscape 0°/180°, Portrait 90°/270°) with 350ms hysteresis debouncing.
  - Selectable orientation modes (`Auto (IMU)`, `Landscape`, `Portrait`) configurable via On-Device System Menu, serial CLI, or ScreenTinker server.
  - Dedicated IMU calibration screen mode for physical orientation validation.
- **Sensirion SHT40 Environmental Telemetry:**
  - Reads ambient temperature and relative humidity via I2C.
  - Transmits live environmental metrics on every poll via `X-ST-Device-Temperature` and `X-ST-Device-Humidity` headers.
  - Displays live temperature and humidity directly on the on-device Status Screen.
- **Dual-Partition Over-The-Air (OTA) Updates & Rollback Safety:**
  - A/B partition flashing via HTTP with on-the-fly hardware mbedTLS SHA-256 verification.
  - Automated boot rollback protection: new builds must successfully sync with ScreenTinker before promotion to `VALID`.
  - Triggerable remotely from ScreenTinker web dashboard or via serial CLI.
- **In-Memory Rolling Diagnostic Logs:**
  - Circular 100-entry ring buffer captures boot events and network diagnostics.
  - Uploadable on demand to the server via `X-ST-Command: upload_logs` or serial CLI without requiring USB debug probes.
- **Smart Battery Telemetry & Deep-Discharge Protection:**
  - Reports battery percentage (`X-ST-Device-Battery`), power source (`X-ST-Power-Source: usb|battery`), and charging status (`X-ST-Charging: 1|0`) on every poll.
  - Displays a live battery badge (`[ 85% CHG ]`, `[ 100% USB ]`, `[ 82% ]`) in the Status header and a dedicated **Power** row on the Status & Info screen.
  - **Low-Battery Warning (`< 10%`):** Automatically overlays a compact warning badge on active signage frames when discharging below 10%.
  - **Critical Battery Shutdown (`<= 1%`):** Renders a dedicated shutdown notice and enters indefinite deep sleep until USB power is connected, protecting the Li-Po cell from deep discharge.
- **Seamless 6-Digit PIN Pairing:**
  - Displays a clean 6-digit code (e.g. `545 658`) and QR code on first boot; simply claim the code in the ScreenTinker web UI.
- **8 MB PSRAM In-Memory Frame Cache (Up to 32 Frames):**
  - Instant frame switching (< 2 sec) when pressing side buttons (`UP` / `DOWN`) without waiting for Wi-Fi downloads.
- **Ultra-Low-Power Wi-Fi & Sleep Management:**
  - Wi-Fi radio powers off (`WIFI_OFF`) during sleep intervals.
  - HTTP 304 (ETag) caching skips unnecessary E-Paper refreshes and SPI writes when content is unchanged.
  - Synchronized sleep intervals via `X-ST-Expires-In` and Display Power Schedules (`X-ST-Power-State`).
- **Dual Onboarding / Provisioning:**
  - **Option A (Wi-Fi Captive Portal):** Broadcasts `ScreenTinker-Setup` Wi-Fi hotspot (`192.168.4.1`).
  - **Option B (1-Click Universal Web Installer):** Flash any supported e-Paper or LED device and configure Wi-Fi, server, and matrix settings directly in Chrome/Edge via [Web Installer](https://www.rene-bohne.de/screentinker_mcu/tools/).
- **Multi-Zone & Fullscreen Layout Rendering:**
  - Supports standard fullscreen slides as well as server-composited multi-zone layouts.

---

## Hardware Specification Matrix

| Model | Display Panel / Target | Resolution | Color Depth | Controller IC | Special Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **reTerminal Sticky** | 3.97" E-Paper | 800×480 | 1bpp Mono | SSD1677 | SHT40, LSM6DS3TR-C IMU, BQ27220, 3 Buttons |
| **reTerminal E1001** | 7.5" E-Paper | 800×480 | 1bpp / Gray4 | UC8179 | SHT40, RTC PCF8563, MicroSD, 3 Buttons |
| **reTerminal E1002** | 7.3" E-Paper | 800×480 | 4bpp Spectra 6 | ED2208 | SHT40, RTC PCF8563, MicroSD, 3 Buttons |
| **reTerminal E1003** | 10.3" E-Paper | 1872×1404 | 4bpp Gray16 | IT8951 | SHT40, Touchscreen (GT911), RTC, MicroSD |
| **reTerminal E1004** | 13.3" E-Paper | 1200×1600 | 4bpp Spectra 6 | T133A01 | SHT40, RTC PCF8563, MicroSD |
| **XIAO LED Controller (C6/S3/C3)** | Smart RGB LED Strip / Matrix | Arbitrary (5×1 to 128×64) | 24bpp RGB888 | WS2812B / SK6812 | 5V Level Shifter, Grove I2C, Optional SSD1306 OLED |

> [!TIP]
> For complete details on building and configuring addressable LED strips and matrices, see the dedicated [**LED Mode Master Documentation**](docs/led_mode_guide.md) and [**LED Matrix Physical Wiring & Coordinate Mapping Guide**](docs/led_matrix_wiring_guide.md). Sample test patterns and pixel art are located in [**`examples/media/`**](examples/media/).


---

## Quick Start / Onboarding (For Users)

### Option A: Smartphone Setup (Captive Portal)
1. Power on the reTerminal device. If unconfigured, it broadcasts a Wi-Fi network called **`ScreenTinker-Setup`**.
2. Connect your smartphone/laptop to **`ScreenTinker-Setup`**.
3. A setup page will open automatically (or navigate to `http://192.168.4.1`).
4. Enter your Wi-Fi details, ScreenTinker Server URL, and select your preferred Layout & Orientation Mode, then click **"Save & Connect"**.

### Option B: USB-C Web Installer
1. Connect your reTerminal or XIAO LED Controller to your PC/Mac using a USB-C data cable.
2. Open the **[ScreenTinker Universal Web Installer](https://www.rene-bohne.de/screentinker_mcu/tools/)** (or [`tools/sticky-installer.html`](tools/sticky-installer.html)) in Google Chrome or Microsoft Edge.
3. Select your hardware target and click **"🚀 Firmware direkt im Browser installieren"** to flash the latest v1.4.0 firmware.
4. Click **"🔌 USB-Verbindung herstellen"**, enter your Wi-Fi SSID, Password, Server URL (and LED matrix dimensions if applicable), and click **"💾 Speichern & Koppeln"**.

### Step 2: Claim Display in ScreenTinker
1. The device connects to Wi-Fi and renders a large **6-digit Pairing Code** (e.g. `545 658`) on its E-Paper screen.
2. Open your ScreenTinker web dashboard, click **"Add Display"**, enter the 6 digits, and name your screen.
3. **Done!** The display immediately transitions into active signage playback.

---

## Button Controls & On-Device System Menu

| Button | Setup / Pairing Mode | Signage Playback Mode | System Menu Mode |
| :--- | :--- | :--- | :--- |
| **UP** | Switch Language to **Deutsch** | Navigate to **Previous Item** in cache | Navigate menu UP |
| **DOWN** | Switch Language to **English** | Navigate to **Next Item** in cache | Navigate menu DOWN |
| **OK (Short Press)** | Refresh / Request Pairing Code | Force immediate sync with server | Confirm selection / Toggle option |
| **OK (Hold 1.5s)** | Open **System Menu** | Open **System Menu** | — |

### On-Device System Menu Options
Holding **OK** for 1.5 seconds (or waking the device via button press) opens the on-device System Menu:
1. **Back to Display** — Exit the menu and return to the active slide.
2. **Sync Now (Fetch Latest)** — Immediately poll the ScreenTinker server for new content.
3. **Status & Info Screen** — View IP address, Wi-Fi SSID & RSSI, Server URL, Firmware version + live SHT40 Temperature/Humidity, Power/Battery status, and Device UUID.
4. **Orientation** — Cycle between `Auto (IMU)`, `Landscape`, and `Portrait`.
5. **Language** — Toggle on-device UI between `English` and `Deutsch`.
6. **Wi-Fi Power** — Cycle Wi-Fi TX power between `Max (19.5 dBm)`, `Med (15 dBm)`, and `Low (11 dBm)`.
7. **IMU Calibration** *(devices with IMU)* — Open interactive orientation test screen (`A` / `B` / `C` / `D`).
8. **Reset / Unpair Device** — Clear stored Wi-Fi/pairing credentials in NVS and restart in setup mode.

---

## Development & Building from Source

### Prerequisites
- [PlatformIO Core (CLI)](https://platformio.org/) or PlatformIO IDE extension for VS Code.
- **NixOS / Nix Flakes:** Enter the development shell with `nix-shell` or `nix develop` (note: use `nix-shell` with a hyphen or `nix develop`, not `nix shell`). For non-root USB flashing on NixOS, add `services.udev.packages = with pkgs; [ platformio-core.udev ];` to your NixOS configuration.

### Multi-Target Build Environments
Compile and flash for your specific reTerminal hardware target:

| Target Model | PlatformIO Environment Command |
| :--- | :--- |
| **reTerminal Sticky** (3.97" Mono) | `pio run -e seeed_reterminal_sticky -t upload` |
| **reTerminal E1001** (7.5" Mono/Gray4) | `pio run -e seeed_reterminal_e1001 -t upload` |
| **reTerminal E1002** (7.3" Spectra 6) | `pio run -e seeed_reterminal_e1002 -t upload` |
| **reTerminal E1003** (10.3" Gray16) | `pio run -e seeed_reterminal_e1003 -t upload` |
| **reTerminal E1004** (13.3" Spectra 6) | `pio run -e seeed_reterminal_e1004 -t upload` |
| **Desktop Simulator** (Linux/macOS) | `pio run -e simulator` |

```bash
# Monitor serial output (115200 Baud)
pio device monitor -b 115200
```

---

## Serial CLI & Diagnostics Interface

When connected via USB-C Serial (`115200 Baud`), the firmware accepts plaintext or JSON commands:

| Command | Description |
| :--- | :--- |
| `status` | Output JSON status (version, IP, RSSI, temperature, humidity, battery, orientation). |
| `imu` | Print live LSM6DS3TR-C accelerometer axes and detected orientation. |
| `imu_debug` | Toggle visual IMU calibration card on display (cards A, B, C, D). |
| `set_orientation <auto\|portrait\|landscape>` | Override active orientation mode. |
| `logs` | Dump rolling 100-entry in-memory diagnostic log ring buffer. |
| `update <url> [sha256]` | Initiate Over-The-Air dual-partition firmware update. |
| `rollback` | Trigger immediate partition rollback and reboot. |
| `reboot` | Restart the microcontroller. |
| `factory_reset` | Wipe NVS credentials and reboot into pairing mode. |

---

### Precompiled Binaries
Ready-to-flash binaries for ESP Web Tools / WebSerial are maintained in [tools/](tools/):
- `tools/firmware.bin` (Application offset `0x10000`)
- `tools/bootloader.bin` (Bootloader offset `0x0000`)
- `tools/partitions.bin` (Partition table offset `0x8000`)
- `tools/manifest.json` (ESP Web Tools manifest)

---


## Device Simulator (Linux, NixOS & macOS)

Test displays, 6-digit pairing, power schedules, and telemetry natively on your desktop without physical hardware:

```bash
# Enter Nix development shell (on NixOS)
nix-shell   # or: nix develop

# Build simulator with PlatformIO
pio run -e simulator

# Run interactive pairing test
./screentinker_sim --pair --server http://<your-server>:3001
```

See [simulator/README.md](simulator/README.md) for full documentation, interactive hotkeys, and headless CI modes.

---

## Automated Testing & CI

### 1. Native C++ Unit Tests (Unity)
Run the host-compiled C++ unit tests covering IMU orientation & hysteresis, SHT40 environmental telemetry, the 100-entry diagnostic ring buffer, and OTA verification/rollback state:
```bash
pio test -e test_native
```

### 2. Simulator E2E Integration Suite
An end-to-end integration test suite runs the simulator in headless mode against a lightweight embedded mock ScreenTinker server:

```bash
# Run all tests locally
./tests/run_tests.sh

# Or invoke with Python directly
python3 -m unittest discover -s tests -p "test_*.py" -v

# Run the mock server standalone for manual testing
python3 tests/mock_server.py --port 3001
```

All Pull Requests and commits to `main` are continuously validated via [GitHub Actions](.github/workflows/ci.yml) for:
1. **Embedded Firmware Compilation:** `pio run -e seeed_reterminal_sticky` (and E-Series targets)
2. **Native C++ Unit Tests:** `pio test -e test_native`
3. **Simulator E2E Integration Suite:** 200 frame sync, 304 ETag caching, 404 empty playlist diagnostics, 501 fallback, 6-digit PIN pairing, and hardware telemetry headers.
