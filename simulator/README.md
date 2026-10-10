# ScreenTinker MCU Host Simulator (Linux, NixOS & macOS)

A native cross-platform simulator for the Seeed Studio reTerminal Sticky e-paper firmware.
It compiles the firmware's graphics rendering engine (`setup_screen.cpp`) and HTTP client logic natively, allowing you to test screens, layouts, power schedules, and telemetry without flashing physical hardware.

---

## Features

- **800×480 E-Paper Viewport:** Displays the exact monochrome frame in an interactive SDL2 desktop window.
- **Window Scaling (`--scale 2`):** Crisp nearest-neighbor integer scaling (1x–4x) for High-DPI Linux / macOS monitors.
- **Snapshot Export:** Automatically dumps rendered frames to 800×480 BMP (`sim_output.bmp`) on refresh.
- **Real Server Polling:** Connects to any ScreenTinker server (local or remote) via `libcurl`, sending telemetry headers (`X-ST-Firmware-Version`, `X-ST-Device-Battery`, `X-ST-Device-RSSI`, `X-ST-Power-Source`, `X-ST-Charging`, `If-None-Match`).
- **Telemetry & State Simulation:** Test battery levels (e.g. low battery 12% vs 100%), power sources (USB mains vs battery), Wi-Fi RSSI, empty playlists (404), unsupported widgets (501), cache hits (304), and sleep durations.
- **Interactive Hotkeys:** Trigger immediate wake-ups, toggle power source, adjust battery, and save snapshots in real time.
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
2. Environment variables (`ST_SERVER_URL`, `ST_DEVICE_ID`, `ST_DEVICE_TOKEN`, `ST_BATTERY`, `ST_RSSI`)
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
- Opens an 800×480 window displaying the live **6-digit PIN** and **QR code**.
- Polls `/api/embedded/pair/status` every 2.5s.
- Once claimed in your web dashboard, saves the token and seamlessly starts displaying your slides!

### 2. Interactive Desktop Mode (with existing token)
```bash
./screentinker_sim --token <your-token>
```
Opens an 800×480 window on your desktop displaying the live e-paper output.

**Hotkeys in SDL2 Window:**
- **`Space` / `Enter`**: Trigger immediate wake-up and poll server.
- **`P`**: Toggle power source mode (Battery ⟷ USB Mains / Charging).
- **`Up` / `Down`**: Increase / decrease battery percentage (±10%).
- **`B`**: Toggle between Low Battery (12%) and Full Battery (100%).
- **`Q` / `Esc`**: Quit.

### 3. High-DPI Desktop Scaling
```bash
./screentinker_sim --token <token> --scale 2
```
Opens a 1600×960 window while preserving crisp 800×480 e-paper pixel geometry.

### 4. Low Battery Simulation
```bash
./screentinker_sim --token <token> --battery 12 --rssi -85
```

### 5. USB Mains Mode (Continuous Active Polling)
```bash
./screentinker_sim --token <token> --usb
```

### 6. Headless One-Shot Mode (CI / Scripting)
```bash
./screentinker_sim --token <token> --headless --oneshot --snapshot frame.bmp
```

### 7. Custom Server / Device
```bash
./screentinker_sim --server http://192.168.100.222:3001 --device <device-uuid> --token <token>
```
