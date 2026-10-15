# ScreenTinker MCU Host Simulator (Linux, NixOS & macOS)

A native cross-platform simulator for the Seeed Studio reTerminal Sticky e-paper firmware.
It compiles the firmware's graphics rendering engine (`setup_screen.cpp`) and HTTP client logic natively, allowing you to test screens, layouts, power schedules, and telemetry without flashing physical hardware.

---

## Features

- **reTerminal Series & Multi-Model Viewport:** Simulates the entire Seeed Studio reTerminal family:
  - `sticky`: 800×480 1bpp monochrome e-paper (fast partial refresh)
  - `e1001`: 800×480 1bpp monochrome (fast partial refresh)
  - `e1002`: 800×480 6-color E-Ink Spectra 6
  - `e1003`: 1872×1404 16-level grayscale (touch supported)
  - `e1004`: 1200×1600 6-color E-Ink Spectra 6 (portrait native)
- **Orientation & Rotation Support:** Seamlessly test landscape and portrait displays (`--orientation portrait` or `--rotate 90`). Dimensions and coordinate axes automatically transpose.
- **Dynamic Server-Driven Adaptation:** Automatically detects server response headers (`X-ST-Screen-Width`, `X-ST-Screen-Height`, `X-ST-Orientation`, `X-ST-Rotation`) and live-resizes window and framebuffers.
- **Window Scaling (`--scale 2`):** Crisp nearest-neighbor integer scaling (1x–4x) for High-DPI Linux / macOS monitors.
- **Snapshot Export:** Automatically dumps rendered frames to BMP (`sim_output.bmp`) matching active model dimensions.
- **Real Server Polling:** Connects to any ScreenTinker server (local or remote) via `libcurl`, sending telemetry headers (`X-ST-Firmware-Version`, `X-ST-Device-Battery`, `X-ST-Device-RSSI`, `X-ST-Power-Source`, `X-ST-Charging`, `If-None-Match`).
- **Telemetry & State Simulation:** Test battery levels (e.g. low battery 12% vs 100%), power sources (USB mains vs battery), Wi-Fi RSSI, empty playlists (404), unsupported widgets (501), cache hits (304), and sleep durations.
- **Interactive Hotkeys:** Trigger immediate wake-ups, cycle orientation (`R`), toggle power source, adjust battery, and save snapshots in real time.
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
