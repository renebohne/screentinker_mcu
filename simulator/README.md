# ScreenTinker MCU Host Simulator (macOS)

A native macOS simulator for the Seeed Studio reTerminal Sticky e-paper firmware.
It compiles the firmware's graphics rendering engine (`setup_screen.cpp`) and HTTP client logic natively with Clang, allowing you to test screens, layouts, power schedules, and telemetry without flashing physical hardware.

---

## Features

- **800×480 E-Paper Viewport:** Displays the exact monochrome frame in an interactive SDL2 desktop window.
- **Snapshot Export:** Automatically dumps rendered frames to 800×480 BMP (`sim_output.bmp`) on refresh.
- **Real Server Polling:** Connects to any ScreenTinker server (local or remote) via `libcurl`, sending telemetry headers (`X-ST-Firmware-Version`, `X-ST-Device-Battery`, `X-ST-Device-RSSI`, `If-None-Match`).
- **Telemetry & State Simulation:** Test battery levels (e.g. low battery 15% vs 100%), Wi-Fi RSSI, empty playlists (404), cache hits (304), and sleep durations.
- **Interactive Hotkeys:** Trigger immediate wake-ups, toggle battery levels, and save snapshots in real time.
- **Headless Mode:** Run in CI or shell scripts with `--headless --oneshot` for automated visual regression tests.

---

## Building

Prerequisites on macOS:
```bash
brew install sdl2
```

Build the simulator:
```bash
make -C simulator
```
The binary `screentinker_sim` will be created in the project root.

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

### 1. Interactive Desktop Mode (default)
```bash
./screentinker_sim --token <your-token>
```
Opens an 800×480 window on your desktop displaying the live e-paper output.

**Hotkeys in SDL2 Window:**
- **`Space` / `Enter`**: Trigger immediate wake-up and poll server.
- **`Up` / `Down`**: Increase / decrease battery percentage (±10%).
- **`B`**: Toggle between Low Battery (12%) and Full Battery (100%).
- **`Q` / `Esc`**: Quit.

### 2. Low Battery Simulation
```bash
./screentinker_sim --token <token> --battery 12 --rssi -85
```

### 3. Headless One-Shot Mode (CI / Scripting)
```bash
./screentinker_sim --token <token> --headless --oneshot --snapshot frame.bmp
```

### 4. Custom Server / Device
```bash
./screentinker_sim --server http://192.168.100.222:3001 --device <device-uuid> --token <token>
```
