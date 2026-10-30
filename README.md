# ScreenTinker MCU Firmware — Universal Display & Smart LED Controller

Ultra-low-power, high-performance C++ firmware for **E-Paper displays**, **Smart Pixel LED strips/matrices**, and **RGB LCD panels** connected to the **[ScreenTinker Digital Signage Server](https://github.com/rene-bohne/screentinker)**.

ScreenTinker MCU powers a diverse ecosystem of visual hardware — ranging from battery-powered ambient E-Ink signs and vibrant 2D LED pixel art matrices to high-refresh full-color LCD touch panels.

---

## Supported Hardware Platforms

### 1. E-Paper Displays (Ultra-Low-Power Signage)
- **Seeed Studio reTerminal Sticky**: 3.97" 800×480 Monochrome (SSD1677 driver, 1bpp, SHT40, 6-axis IMU, BQ27220 fuel gauge).
- **Seeed Studio reTerminal E1001**: 7.5" 800×480 Monochrome & 4-Level Grayscale (UC8179 driver).
- **Seeed Studio reTerminal E1002**: 7.3" 800×480 6-Color Spectra 6 (ED2208 driver, 4bpp).
- **Seeed Studio reTerminal E1003**: 10.3" 1872×1404 16-Level Grayscale (IT8951 controller, capacitive touch, 4bpp).
- **Seeed Studio reTerminal E1004**: 13.3" 1200×1600 6-Color Spectra 6 (T133A01 dual-controller, 4bpp).

### 2. Smart Pixel LED Displays (Strips & 2D Matrices)
- **Controllers**: Seeed Studio XIAO ESP32-C6, ESP32-S3, or ESP32-C3.
- **LED Types**: WS2812B, SK6812, NeoPixel addressable RGB LEDs (24bpp RGB888).
- **Form Factors**: Single strips (e.g. 5×1 tickers, 30×1 progress bars) up to large 2D matrices (8×8, 8×32, 16×16, 64×32, 128×64).
- **Features**: Serpentine/zigzag wiring coordinate mapping, optional secondary I2C SSD1306 status OLED, and 5V level-shifting support.

### 3. RGB LCD Panels (Full-Color TFT Displays)
- **Elecrow CrowPanel 4.3" HMI Display**: 480×272 16-bit Parallel RGB TFT (NV3047 / ILI6485 driver, 16bpp RGB565).
- **Features**: Real-time rendering via hardware DMA bounce buffers, internal SRAM network decoupling to preserve Quad PSRAM bandwidth, and single-button System Menu navigation.

### 4. Desktop Simulator & Testing
- Native Linux, macOS, and NixOS simulator with interactive hotkeys, mock signage server, and automated headless CI test suites.

---

## Key Architectural Features

- **Universal Multi-Display Engine:**
  Seamless abstraction layer supporting 1-bit monochrome, 4-bit grayscale/Spectra-6, 16-bit RGB565, and 24-bit RGB888 pixel formats.
- **Dynamic Gravity Orientation & IMU Auto-Rotation:**
  On-board 6-axis IMU automatically detects gravity orientation and rotates display buffers in real time (Landscape 0°/180°, Portrait 90°/270°) with 350ms hysteresis debouncing.
- **Sensirion SHT40 Environmental Telemetry:**
  Reads ambient temperature and relative humidity via I2C and transmits live telemetry headers (`X-ST-Device-Temperature`, `X-ST-Device-Humidity`) on every sync cycle.
- **Dual-Partition Over-The-Air (OTA) Updates & Rollback Safety:**
  A/B partition flashing via HTTP with hardware mbedTLS SHA-256 verification and automatic rollback protection.
- **Smart Battery Telemetry & Deep-Discharge Protection:**
  Live battery reporting (`X-ST-Device-Battery`, `X-ST-Power-Source`, `X-ST-Charging`), visual low-battery badge (`< 10%`), and automatic deep-discharge shutdown (`<= 1%`) to safeguard Li-Po cells.
- **In-Memory PSRAM Frame Caching:**
  Multi-frame caching in PSRAM enables instant navigation through playlists without waiting for Wi-Fi downloads.
- **Ultra-Low-Power Wi-Fi & Sleep Management:**
  Powers off Wi-Fi radio during sleep intervals; HTTP 304 (ETag) caching eliminates unnecessary display redraws when content is unchanged.
- **Dual Onboarding Options:**
  - **Captive Portal:** Device broadcasts `ScreenTinker-Setup` Wi-Fi hotspot (`192.168.4.1`) on first boot.
  - **1-Click Web Installer:** Flash and configure Wi-Fi, server URL, and LED matrix geometry directly in Chrome/Edge via [Web Tools](https://www.rene-bohne.de/screentinker_mcu/tools/).
- **Seamless 6-Digit PIN Pairing:**
  Displays a 6-digit claim code (e.g. `545 658`) and QR code on initial boot for instant one-click pairing in the ScreenTinker web dashboard.

---

## Hardware Specification Matrix

| Model / Target | Category | Resolution | Color Depth | Controller IC | Primary Features |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **reTerminal Sticky** | E-Paper | 800×480 | 1bpp Mono | SSD1677 | SHT40, LSM6DS3TR-C IMU, Fuel Gauge, 3 Buttons |
| **reTerminal E1001** | E-Paper | 800×480 | 1bpp / Gray4 | UC8179 | SHT40, RTC PCF8563, MicroSD, 3 Buttons |
| **reTerminal E1002** | E-Paper | 800×480 | 4bpp Spectra 6 | ED2208 | SHT40, RTC PCF8563, MicroSD, 3 Buttons |
| **reTerminal E1003** | E-Paper | 1872×1404 | 4bpp Gray16 | IT8951 | SHT40, Touchscreen (GT911), RTC, MicroSD |
| **reTerminal E1004** | E-Paper | 1200×1600 | 4bpp Spectra 6 | T133A01 | SHT40, RTC PCF8563, MicroSD |
| **XIAO LED (C6 / S3 / C3)** | Smart LED | Arbitrary (5×1 to 128×64) | 24bpp RGB888 | WS2812B / SK6812 | 5V Level Shifter, Grove I2C, Optional SSD1306 OLED |
| **CrowPanel 4.3" HMI** | RGB LCD | 480×272 | 16bpp RGB565 | NV3047 (16-bit Parallel) | ESP32-S3 (2MB Quad PSRAM), MicroSD, BOOT Button |

> [!TIP]
> - For building and configuring addressable LED strips and matrices, see the [**LED Mode Master Documentation**](docs/led_mode_guide.md) and [**LED Matrix Wiring & Mapping Guide**](docs/led_matrix_wiring_guide.md).
> - For hardware architecture and pinout details on the CrowPanel 4.3" RGB LCD, see the [**CrowPanel 4.3" Hardware Notes**](docs/crowpanel_43_hardware_notes.md).

---

## Quick Start / Onboarding

### Option A: Smartphone Setup (Captive Portal)
1. Power on your device. When unconfigured, it broadcasts a Wi-Fi hotspot named **`ScreenTinker-Setup`**.
2. Connect your phone or laptop to **`ScreenTinker-Setup`**.
3. A configuration page opens automatically (or browse to `http://192.168.4.1`).
4. Enter your Wi-Fi credentials and ScreenTinker Server URL, then click **"Save & Connect"**.

### Option B: USB Web Installer (Browser-Based)
1. Connect your MCU (reTerminal, XIAO, or CrowPanel) to your computer using a USB-C cable.
2. Open the **[ScreenTinker Universal Web Installer](https://www.rene-bohne.de/screentinker_mcu/tools/)** in Chrome or Edge.
3. Select your target device and flash the firmware directly in your browser.
4. Click **"Connect via USB"** to configure Wi-Fi, server URL, or LED matrix dimensions via Web Serial.

### Pairing with ScreenTinker
1. Once connected to Wi-Fi, the screen renders a **6-digit Pairing Code** (e.g. `545 658`) and a QR code.
2. In the ScreenTinker web dashboard, navigate to **Displays** $\rightarrow$ **Add Display**, enter the 6-digit code, and assign a name.
3. The display will immediately fetch its assigned playlist and start active playback!

---

## Controls & System Menu

### Multi-Button Devices (reTerminal Sticky & E-Series)
| Button | Normal Mode | System Menu Mode |
| :--- | :--- | :--- |
| **UP** | Previous cached item | Navigate menu up |
| **DOWN** | Next cached item | Navigate menu down |
| **OK (Short Press)** | Force refresh / sync | Confirm selection / Toggle option |
| **OK (Hold 1.5s)** | Open System Menu | — |

### Single-Button Devices (CrowPanel 4.3")
| Action | Normal Mode | System Menu Mode |
| :--- | :--- | :--- |
| **BOOT (Short Press)** | Open System Menu | Cycle to next menu item |
| **BOOT (Long Press > 1.2s)** | — | Confirm / Execute selected item |

### On-Device System Menu Features
- **1. Status & Info**: View IP, Wi-Fi SSID & RSSI, Server URL, Firmware version, live temperature/humidity, power status, and Device UUID.
- **2. Orientation**: Cycle between `Auto (IMU)`, `Landscape`, and `Portrait`.
- **3. Wi-Fi Power**: Toggle transmit power between `Max (19.5 dBm)`, `Med (15 dBm)`, and `Low (11 dBm)`.
- **4. Refresh Mode**: Switch between `Smart Partial (6:1)` and `Always Full`.
- **5. Language**: Toggle on-device interface between `English` and `Deutsch`.
- **6. Unpair Device**: Reset pairing credentials and generate a fresh 6-digit claim PIN.
- **7. Power Off / Standby**: Safely power down or enter deep sleep.
- **8. Factory Reset**: Erase all stored NVS settings.
- **9. Back / Close**: Exit menu and resume playlist playback.

---

## Development & Building from Source

### Prerequisites
- [PlatformIO Core (CLI)](https://platformio.org/) or the PlatformIO IDE extension in VS Code.
- Optional: NixOS / Nix Flakes shell via `nix-shell` or `nix develop`.

### Build & Upload Commands

| Target Environment | Description | PlatformIO Command |
| :--- | :--- | :--- |
| `seeed_reterminal_sticky` | reTerminal Sticky (3.97" Mono E-Paper) | `pio run -e seeed_reterminal_sticky -t upload` |
| `seeed_reterminal_e1001` | reTerminal E1001 (7.5" Mono/Gray4 E-Paper) | `pio run -e seeed_reterminal_e1001 -t upload` |
| `seeed_reterminal_e1002` | reTerminal E1002 (7.3" Spectra 6 E-Paper) | `pio run -e seeed_reterminal_e1002 -t upload` |
| `seeed_reterminal_e1003` | reTerminal E1003 (10.3" Gray16 E-Paper) | `pio run -e seeed_reterminal_e1003 -t upload` |
| `seeed_reterminal_e1004` | reTerminal E1004 (13.3" Spectra 6 E-Paper) | `pio run -e seeed_reterminal_e1004 -t upload` |
| `seeed_xiao_esp32c6_led` | XIAO ESP32-C6 (Smart RGB LED Strip/Matrix) | `pio run -e seeed_xiao_esp32c6_led -t upload` |
| `seeed_xiao_esp32s3_led` | XIAO ESP32-S3 (Smart RGB LED Strip/Matrix) | `pio run -e seeed_xiao_esp32s3_led -t upload` |
| `seeed_xiao_esp32c3_led` | XIAO ESP32-C3 (Smart RGB LED Strip/Matrix) | `pio run -e seeed_xiao_esp32c3_led -t upload` |
| `elecrow_crowpanel_43` | CrowPanel 4.3" (480×272 Parallel RGB LCD) | `pio run -e elecrow_crowpanel_43 -t upload` |
| `simulator` | Desktop Simulator (Linux / macOS) | `pio run -e simulator` |
| `test_native` | Native C++ Unit Tests (Unity) | `pio test -e test_native` |

```bash
# Monitor serial output (115200 Baud)
pio device monitor -b 115200
```

---

## Serial CLI & Diagnostics Interface

When connected over USB Serial (`115200 Baud`), the firmware accepts plaintext and JSON commands:

| Command | Description |
| :--- | :--- |
| `status` | Output JSON status (firmware version, IP, RSSI, temperature, humidity, battery, orientation). |
| `imu` | Print live LSM6DS3TR-C accelerometer axes and detected gravity orientation. |
| `imu_debug` | Toggle visual IMU calibration card on display (cards A, B, C, D). |
| `set_orientation <auto\|portrait\|landscape>` | Override active orientation mode. |
| `logs` | Dump rolling 100-entry in-memory diagnostic log ring buffer. |
| `update <url> [sha256]` | Initiate Over-The-Air dual-partition firmware update. |
| `rollback` | Trigger immediate partition rollback and reboot. |
| `reboot` | Restart the microcontroller. |
| `factory_reset` | Wipe NVS credentials and reboot into pairing mode. |

---

## Testing & Quality Assurance

1. **Native C++ Unit Tests (Unity):**
   ```bash
   pio test -e test_native
   ```
2. **Simulator E2E Integration Suite:**
   ```bash
   ./tests/run_tests.sh
   # Or invoke python directly:
   python3 -m unittest discover -s tests -p "test_*.py" -v
   ```
3. **Continuous Integration:**
   All pull requests and commits are verified via GitHub Actions for multi-target firmware compilation, native unit tests, and headless simulator E2E validation.
