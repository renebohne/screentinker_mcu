# ScreenTinker MCU Host Simulator (Linux, NixOS & macOS)

A native cross-platform simulator for the Seeed Studio reTerminal Sticky and reTerminal E-Series (E1001, E1002, E1003, E1004) e-paper firmware.
It compiles the firmware's graphics rendering engine (`setup_screen.cpp`), sensor/IMU drivers, diagnostic log ring buffer, OTA state engine, and HTTP client logic natively, allowing you to test screens, layouts, power schedules, and telemetry without flashing physical hardware.

---

## Features

- **reTerminal Series & Multi-Model Viewport:** Simulates the entire Seeed Studio reTerminal family:
  - `sticky`: 800×480 1bpp monochrome e-paper (fast partial refresh)
  - `e1001`: 800×480 1bpp monochrome (fast partial refresh)
  - `e1002`: 800×480 6-color E-Ink Spectra 6
  - `e1003`: 1872×1404 16-level grayscale (touch supported)
  - `e1004`: 1200×1600 6-color E-Ink Spectra 6 (portrait native)
  - `xiao_led`: RGB888 smart addressable LED strips and matrices (WS2812 / SK6812)
- **LED Strip & Matrix Simulation:** Simulate arbitrary LED layouts using `--width <w>` and `--height <h>` (e.g. `--width 5 --height 1` for strips, `--width 32 --height 8` or `--width 64 --height 64` for matrices). Renders realistic rounded-square SMD LED beads with authentic dark PCB bezels, unlit diode chips, and glowing center highlights in SDL2.
- **Orientation & Rotation Support:** Seamlessly test landscape and portrait displays (`--orientation portrait` or `--rotate 90`). Dimensions and coordinate axes automatically transpose.
- **Dynamic Server-Driven Adaptation:** Automatically detects server response headers (`X-ST-Screen-Width`, `X-ST-Screen-Height`, `X-ST-Orientation`, `X-ST-Rotation`) and live-resizes window and framebuffers.
- **Window Scaling (`--scale 2`):** Crisp nearest-neighbor integer scaling (1x–4x) for High-DPI Linux / macOS monitors.
- **Snapshot Export:** Automatically dumps rendered frames to BMP (`sim_output.bmp`) matching active model dimensions.
- **Real Server Polling:** Connects to any ScreenTinker server (local or remote) via `libcurl`, sending full telemetry headers (`X-ST-Firmware-Version`, `X-ST-Device-Battery`, `X-ST-Device-RSSI`, `X-ST-Device-Temperature`, `X-ST-Device-Humidity`, `X-ST-Device-Orientation`, `X-ST-Device-Rotation`, `X-ST-Power-Source`, `X-ST-Charging`, `If-None-Match`).
- **Telemetry, Battery & State Simulation:** Test battery levels (including `< 10%` low-battery warning badge and `<= 1%` critical battery shutdown screen), power sources (USB mains vs battery), ambient SHT40 temperature & humidity, Wi-Fi RSSI, empty playlists (404), unsupported widgets (501), cache hits (304), remote diagnostic log uploads (`upload_logs`), and OTA update commands (`update <url> [sha256]`).
- **Interactive Hotkeys:** Trigger immediate wake-ups, cycle orientation (`R`), toggle power source (`P`), adjust battery (`Up`/`Down`/`B`), and save snapshots in real time.
- **Headless Mode:** Run in CI or shell scripts with `--headless --oneshot` for automated visual regression tests without a display server.

---

## Building

The simulator is built using **PlatformIO** (`platform = native`), just like the physical ESP32 firmware.

### Quick Start (All Platforms)

Build the simulator with PlatformIO:
```bash
pio run -e simulator
```
The binary `screentinker_sim` will be generated in the project root.

---

### Platform Prerequisites

#### NixOS / Nix Flakes (Turnkey)
Enter the provided Nix shell containing PlatformIO, SDL2, curl, and build tools:
```bash
# Using modern Nix flakes
nix develop

# OR using legacy nix-shell
nix-shell

# Build simulator:
pio run -e simulator
```

#### macOS (Apple Silicon & Intel)
```bash
brew install sdl2
pio run -e simulator
```

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev libcurl4-openssl-dev
pio run -e simulator
```

#### Fedora / RHEL
```bash
sudo dnf install gcc-c++ make pkgconf-pkg-config SDL2-devel libcurl-devel
pio run -e simulator
```

#### Arch Linux
```bash
sudo pacman -S base-devel sdl2 curl
pio run -e simulator
```

---

## Configuration

The simulator reads configuration with the following precedence:
1. Command-line flags
2. Environment variables (`ST_SERVER_URL`, `ST_DEVICE_ID`, `ST_DEVICE_TOKEN`, `ST_MODEL`, `ST_ORIENTATION`, `ST_ROTATION`, `ST_BATTERY`, `ST_RSSI`)
3. `simulator/sim_config.json` (or custom path via `--config <path>`)
4. Defaults

Copy the example configuration:
```bash
cp simulator/sim_config.example.json simulator/sim_config.json
```

---

## Usage Examples

### 1. Interactive Device Pairing Flow
Test the entire 6-digit PIN onboarding flow with your ScreenTinker server:
```bash
./screentinker_sim --pair --server http://192.168.100.222:3001
```
- Registers with the server (`POST /api/embedded/pair/register`).
- Opens a window displaying the live **6-digit PIN** and **QR code**.
- Polls `/api/embedded/pair/status` every 2.5s.
- Once claimed in your web dashboard, saves the token and seamlessly starts displaying your slides!

### 2. Interactive Desktop Mode (with existing token)
```bash
./screentinker_sim --token <your-token>
```
Opens a window on your desktop displaying the live e-paper output.

**Hotkeys in SDL2 Window:**
- **`Space` / `Enter`**: Trigger immediate wake-up and poll server.
- **`R`**: Cycle display rotation (0° ⟷ 90° ⟷ 180° ⟷ 270°) and resize viewport dynamically.
- **`P`**: Toggle power source mode (Battery ⟷ USB Mains / Charging).
- **`Up` / `Down`**: Increase / decrease battery percentage (±10%).
- **`B`**: Toggle between Low Battery (12%) and Full Battery (100%).
- **`Q` / `Esc`**: Quit.

### 3. Testing reTerminal E-Series Models
Simulate any device in the family:
```bash
# reTerminal E1001 (Monochrome 800x480)
./screentinker_sim --model e1001 --device-id dev-e1001 --token tok-reterminal-e1001

# reTerminal E1002 (Spectra 6 Color 800x480)
./screentinker_sim --model e1002 --device-id dev-e1002 --token tok-reterminal-e1002

# reTerminal E1003 (Grayscale 1872x1404)
./screentinker_sim --model e1003 --device-id dev-e1003 --token tok-reterminal-e1003

# reTerminal E1004 (Spectra 6 Color 1200x1600 Portrait)
./screentinker_sim --model e1004 --device-id dev-e1004 --token tok-reterminal-e1004
```

### 4. Orientation & Rotation Testing
```bash
# Run E1001 in Portrait orientation (transposes to 480x800)
./screentinker_sim --model e1001 --device-id dev-e1001 --token tok-reterminal-e1001 --orientation portrait

# Run E1004 in Landscape orientation (transposes to 1600x1200)
./screentinker_sim --model e1004 --device-id dev-e1004 --token tok-reterminal-e1004 --orientation landscape

# Explicit 90° or 180° rotation
./screentinker_sim --model e1001 --device-id dev-e1001 --token tok-reterminal-e1001 --rotate 90
```

### 5. High-DPI Desktop Scaling
```bash
./screentinker_sim --token <token> --scale 2
```
Opens a scaled window while preserving crisp pixel geometry.

### 6. Low Battery Simulation
```bash
./screentinker_sim --token <token> --battery 12 --rssi -85
```

### 7. USB Mains Mode (Continuous Active Polling)
```bash
./screentinker_sim --token <token> --usb
```

### 8. Headless One-Shot Mode (CI / Scripting)
```bash
./screentinker_sim --model e1001 --token <token> --headless --oneshot --snapshot frame.bmp
```

### 9. Smart LED Strip & Matrix Simulation (WS2812 / SK6812 RGB888)

Simulate smart LED light strips or matrices with authentic SDL2 physical visualization (rounded-square SMD LED beads, dark bezels, inner dies, and glowing lens cores):

```bash
# 5x1 LED Strip (Desk light / ticker)
./screentinker_sim --width 5 --height 1 --pair

# 64x64 LED Pixel Matrix (e.g. HUB75 / WS2812) with custom config
./screentinker_sim --config sim_config_64x64.json --width 64 --height 64 --pair

# Run active 64x64 matrix simulation using saved configuration
./screentinker_sim --config sim_config_64x64.json
```

**Arbitrary Resolution Support:**
ScreenTinker supports any arbitrary matrix dimension (e.g. 8×8, 16×16, 32×8, 64×64, 128×64) dynamically without needing pre-baked server presets. During pairing, the simulator transmits a dynamic profile containing the exact width and height (`{"preset":"seeed-xiao-led","width":W,"height":H,"colorDepth":"24bit-rgb888","outputFormat":"raw"}`).

Sample media for testing 5×1 strips and 64×64 matrices are available in `examples/media/`.
