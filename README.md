# ScreenTinker Firmware for Seeed Studio reTerminal Sticky

Ultra-low-power, high-performance C++ firmware for the **Seeed Studio reTerminal Sticky** (ESP32-S3R8 with 8MB Flash, 8MB PSRAM, 3.97" 800×480 B/W E-Paper & SSD1677 driver) targeting the **ScreenTinker Digital Signage Server**.

---

## Key Features

- **Seamless 6-Digit PIN Pairing:**
  - On first boot or after factory reset, the device generates a 6-digit pairing code (e.g. `545 658`) and displays it on the E-Paper screen.
  - Simply enter this 6-digit code in the ScreenTinker web dashboard under **"Add Display"** — the device claims itself, receives its permanent token, and starts playback automatically.
- **8 MB PSRAM In-Memory Frame Cache (Up to 32 Frames):**
  - Instant frame switching (< 2 sec) when pressing side buttons (`UP` / `DOWN`) without waiting for Wi-Fi downloads.
  - No internal SRAM heap fragmentation; dedicated static buffers in Octal-SPI PSRAM.
- **Ultra-Low-Power Wi-Fi Management:**
  - Wi-Fi radio powers off automatically (`WIFI_OFF`) between sync intervals.
  - HTTP 304 (ETag) support skips unnecessary E-Paper refreshes and SPI writes when content is unchanged.
  - Server-coordinated sleep intervals using `X-ST-Expires-In`.
- **Bilingual Onboarding & Setup Engine (English / German):**
  - Standalone, lightweight 1-bit monochrome graphics engine with embedded ASCII font (no bulky external GFX dependencies).
  - Dynamically switch between English (default) and German using the hardware buttons (`UP` = Deutsch, `DOWN` = English).
- **Offline Reliability & Visual Indicators:**
  - When Wi-Fi is unavailable but frames are cached, images rotate offline seamlessly with a discreet top-right `[! NO WI-FI]` badge.
  - If Wi-Fi fails on an empty cache, an informative full-screen troubleshooting diagnostic is rendered.
- **1-Click WebSerial Browser Installer:**
  - Setup and Wi-Fi provisioning via [tools/sticky-installer.html](tools/sticky-installer.html) directly in Google Chrome / Microsoft Edge (no command-line or drivers needed).

---

## Hardware Specifications & Pinout

| Component / Function | Specification | Pin / GPIO |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32-S3R8 (Dual-core 240MHz, 8MB Octal Flash, 8MB Octal PSRAM) | — |
| **E-Paper Panel** | 3.97" 800×480 Monochrome E-Paper (SSD1677 Controller) | SPI (SCK: 13, MOSI: 14, MISO: 12) |
| **Display Control** | EPD CS: 11, EPD DC: 10, EPD RST: 9, EPD BUSY: 8 | GPIO 11, 10, 9, 8 |
| **Power Latches** | Power Hold (`PIN_PWR_HOLD`), Power Lock (`PIN_PWR_LOCK`) | GPIO 45, GPIO 46 |
| **Navigation Buttons**| OK (GPIO 4), UP (GPIO 5), DOWN (GPIO 6) | Active LOW |

---

## Quick Start / Onboarding (For Users)

1. **Connect via USB-C:**
   Connect the reTerminal Sticky to your PC/Mac using a USB-C data cable.
2. **Open Web Installer:**
   Open `tools/sticky-installer.html` in Chrome or Edge.
3. **Configure Wi-Fi:**
   Click **"Connect Device"**, enter your Wi-Fi SSID and Password, and click **"Save & Connect"**.
4. **Claim in ScreenTinker:**
   The Sticky connects to Wi-Fi and renders a large **6-digit Pairing Code** (e.g. `545 658`) on its E-Paper screen.
   Open your ScreenTinker web dashboard, click **"Add Display"**, enter the 6 digits, and name your screen.
5. **Done!**
   The display immediately transitions into active signage playback.

---

## Button Controls

| Button | Setup / Pairing Mode | Signage Playback Mode |
| :--- | :--- | :--- |
| **UP (Volume Up / GPIO 5)** | Switch Language to **Deutsch** | Navigate to **Previous Item** in PSRAM cache |
| **DOWN (Volume Down / GPIO 6)** | Switch Language to **English** | Navigate to **Next Item** in PSRAM cache |
| **OK (Short Press / GPIO 4)** | Request / Refresh Pairing Code | Force immediate server synchronization |
| **OK (Hold 5 Seconds / GPIO 4)**| **Factory Reset:** Wipe NVS configuration and restart | **Factory Reset:** Wipe NVS configuration and restart |

---

## Development & Building from Source

### Prerequisites
- [PlatformIO Core (CLI)](https://platformio.org/) or PlatformIO IDE extension for VS Code.

### Build & Flash Firmware
```bash
# Build firmware
pio run

# Flash to device via USB
pio run --target upload

# Monitor serial output (115200 Baud)
pio device monitor -b 115200
```

### Precompiled Binaries
Ready-to-flash binaries for ESP Web Tools / WebSerial are maintained in [tools/](tools/):
- `tools/firmware.bin` (Application offset `0x10000`)
- `tools/bootloader.bin` (Bootloader offset `0x0000`)
- `tools/partitions.bin` (Partition table offset `0x8000`)
- `tools/manifest.json` (ESP Web Tools manifest)
