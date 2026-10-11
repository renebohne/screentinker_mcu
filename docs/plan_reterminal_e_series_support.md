# Architecture & Roadmap: Supporting Seeed Studio reTerminal E-Series (E1001, E1002, E1003, E1004)

## 1. Executive Summary & Goals

This project expands **ScreenTinker MCU** from a single-device target ([reTerminal Sticky](file:///home/rene/github/screentinker_mcu/include/pins_sticky.h)) into a modular, multi-target platform supporting Seeed Studio's entire **reTerminal E Series**:
* **reTerminal Sticky**: 3.97" B/W (800×480, SSD1677)
* **reTerminal E1001**: 7.5" Monochrome / 4-Level Grayscale (800×480, UC8179)
* **reTerminal E1002**: 7.3" 6-Color Spectra 6 (800×480, ED2208 / GDEP073E01)
* **reTerminal E1003**: 10.3" Monochrome / 16-Level Grayscale + Capacitive Touch (1872×1404, IT8951)
* **reTerminal E1004**: 13.3" 6-Color Spectra 6 (1200×1600, T133A01 Dual-Chip)

Both the **embedded ESP32-S3 firmware** and the **cross-platform host simulator (`screentinker_sim`)** will support all 5 models.

---

## 2. Hardware Specification Matrix

```mermaid
classDiagram
    class BaseDevice {
        +ESP32-S3R8 (8MB PSRAM + 32MB Flash)
        +Wi-Fi 802.11 b/g/n + BLE 5.0
        +USB-C 5V/1A
        +I2C Sensors (SHT4x, PCF8563 RTC)
        +MicroSD Card Slot
    }
    class Sticky {
        +3.97" E-Paper (800x480)
        +SSD1677 Controller
        +1-bit Mono (48 KB frame)
    }
    class E1001 {
        +7.5" E-Paper (800x480)
        +UC8179 Controller
        +1-bit Mono / Gray4 (48-96 KB frame)
    }
    class E1002 {
        +7.3" E-Paper (800x480)
        +ED2208 Controller (Spectra 6)
        +6-Color (192 KB / raw frame)
    }
    class E1003 {
        +10.3" E-Paper (1872x1404)
        +IT8951 Controller + Touch
        +1-bit Mono / Gray16 (328-937 KB frame)
    }
    class E1004 {
        +13.3" E-Paper (1200x1600)
        +T133A01 Dual-Chip Controller (Spectra 6)
        +6-Color (960 KB / raw frame)
    }

    BaseDevice <|-- Sticky
    BaseDevice <|-- E1001
    BaseDevice <|-- E1002
    BaseDevice <|-- E1003
    BaseDevice <|-- E1004
```

### Detailed Specs & Capabilities Matrix

| Model | Screen Size | Resolution | Color Depth | E-Paper Controller | Partial Refresh | Refresh Time | Touch Interface | Frame Size | Onboard Peripherals |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **Sticky** | 3.97" | 800 × 480 | 1bpp Mono | SSD1677 | **Yes** (~350ms) | ~1.8 s | **Capacitive Touch** (I2C) | 48,000 B | BQ27220 fuel gauge, BQ25616 charger, 3 buttons, buzzer, SD slot |
| **E1001** | 7.5" | 800 × 480 | 1bpp / Gray4 | UC8179 | **Yes** (~450ms) | ~3.0 s | *None* (Buttons) | 48,000 B (Mono)<br>96,000 B (Gray4) | ADC battery divider, 3 buttons, buzzer, LED, SHT4x, PCF8563, SD slot |
| **E1002** | 7.3" | 800 × 480 | 6-Color Spectra 6 | ED2208 | **No** (Full AC only) | ~14.0 s | *None* (Buttons) | 192,000 B | ADC battery divider, 3 buttons, buzzer, LED, SHT4x, PCF8563, SD slot |
| **E1003** | 10.3" | 1872 × 1404 | 1bpp / Gray16 | IT8951 | **Yes** (~180ms A2) | ~1.2 s | **Capacitive Touch** (Multi-touch) | 328,536 B (Mono)<br>1,314,144 B (Gray16) | Touch digitizer, ADC battery divider, 3 buttons, buzzer, LED, PCF8563, SD slot |
| **E1004** | 13.3" | 1200 × 1600 | 6-Color Spectra 6 | T133A01 (dual CS) | **No** (Full AC only) | ~22.0 s | *None* (Buttons) | 960,000 B | Front/rear buttons, buzzer, LED, ADC battery divider, PCF8563, SD slot |

### 2.1 Touch Screen Architecture & Future Roadmap

Both **reTerminal Sticky** and **reTerminal E1003** are equipped with hardware capacitive touch panels:
* **reTerminal Sticky Touch Hardware**:
  - Touch Bus: Dedicated I2C bus on `PIN_TOUCH_SDA` (GPIO 3), `PIN_TOUCH_SCL` (GPIO 2).
  - Control Pins: Interrupt `PIN_TOUCH_INT` (GPIO 21), Reset `PIN_TOUCH_RST` (GPIO 41), Power Rail Latch `PIN_TOUCH_EN` (GPIO 42).
  - Low Power: Can enter deep sleep touch-listen mode for wake-on-tap.
* **reTerminal E1003 Touch Hardware**:
  - 10.3" large-format capacitive digitizer layer over I2C, paired with IT8951's ultra-low latency A2 waveform mode (~150-200ms).
* **Future Firmware Feature Potential**:
  - *Slide Carousel Navigation*: Tap left/right screen edges to advance or rewind slides without opening the mobile app.
  - *Interactive Dashboard Widgets*: Tapping checklist items, toggling room occupancy, or confirming meeting check-ins directly on the e-paper display.
  - *On-Screen Settings UI*: Direct touchscreen interaction for Wi-Fi selection and settings menus.
* **Simulator Integration**:
  - SDL2 mouse clicks and touch events can map directly to simulated touch interrupts, enabling offline development of touch features before physical deployment.

### 2.2 Refresh Timing & Waveform Strategies

* **Monochrome Panels (Sticky, E1001)**:
  - Full refresh (~1.8s - 3.0s) inverts pixels to clear ghosting.
  - Fast partial refresh (~350ms - 450ms) writes changed regions only, ideal for clocks and metric updates.
* **Spectra 6 Color Panels (E1002, E1004)**:
  - Do **not** support partial refresh. Moving 6 colored electrophoretic particles (Black, White, Red, Yellow, Blue, Green) requires complex multi-phase AC agitation waveforms.
  - Minimum slide intervals should be ≥ 30s–60s on the ScreenTinker server to protect panel longevity.
* **Grayscale Fast-Response (E1003 IT8951)**:
  - Hardware accelerated waveform controller supports GC16 (16-level grayscale, 1.2s) and ultra-fast A2/DU (black/white 180ms) modes.

### 2.3 Onboard Sensors & Orientation Subsystem

* **Sensirion SHT40 (Temperature & Humidity)**:
  - Present on **all 5 models** (Sticky, E1001, E1002, E1003, E1004) over I2C at address `0x44`.
  - Sticky I2C: SDA = GPIO 1, SCL = GPIO 0.
  - E1001–E1004 I2C: SDA = GPIO 19, SCL = GPIO 20 (shared with PCF8563 RTC).
  - Used for ambient environmental telemetry in sync heartbeats (`X-ST-Device-Temperature`, `X-ST-Device-Humidity`) and display waveform temperature compensation.
* **STMicroelectronics LSM6DS3TR-C IMU (reTerminal Sticky)**:
  - 6-axis IMU (3-axis accelerometer + 3-axis gyroscope) over I2C at address `0x6A` (SDA = GPIO 1, SCL = GPIO 0, INT1 = GPIO 7).
  - Present **only on reTerminal Sticky**; E-Series devices (E1001–E1004) do not have an onboard IMU.
  - Used for optional auto-orientation detection (Auto mode in settings).
* **Firmware Orientation Modes**:
  - **IMU devices (Sticky)**: `AUTO [IMU]` | `LANDSCAPE` | `PORTRAIT`.
  - **Non-IMU devices (E1001–E1004)**: `LANDSCAPE` | `PORTRAIT`.
  - Configurable via on-screen system settings menu (`setup_screen.cpp`) and SoftAP captive portal.
  - Persisted in NVS across reboots.

---

## 3. Firmware Architecture (`platformio.ini` & Pin Headers)

### 3.1 PlatformIO Environment Separation
`default_envs = seeed_reterminal_sticky` is preserved to avoid any regressions. New environments are declared modularly:

```ini
[platformio]
default_envs = seeed_reterminal_sticky

[env:seeed_reterminal_sticky]
platform = espressif32
board = esp32-s3-devkitc1-n8r8
framework = arduino
build_flags =
    -DDEVICE_RETERMINAL_STICKY
    -DBOARD_HAS_PSRAM
...

[env:seeed_reterminal_e1001]
extends = env:seeed_reterminal_sticky
build_flags =
    -DDEVICE_RETERMINAL_E1001
    -DBOARD_HAS_PSRAM

[env:seeed_reterminal_e1002]
extends = env:seeed_reterminal_sticky
build_flags =
    -DDEVICE_RETERMINAL_E1002
    -DBOARD_HAS_PSRAM

[env:seeed_reterminal_e1003]
extends = env:seeed_reterminal_sticky
build_flags =
    -DDEVICE_RETERMINAL_E1003
    -DBOARD_HAS_PSRAM

[env:seeed_reterminal_e1004]
extends = env:seeed_reterminal_sticky
build_flags =
    -DDEVICE_RETERMINAL_E1004
    -DBOARD_HAS_PSRAM
```

### 3.2 Unified Device Profile Abstraction (`device_profile.h`)
Instead of hardcoding SSD1677 pins and dimensions in `src/main.cpp`, a central header `include/device_profile.h` will include the appropriate pin mapping:
* `pins_sticky.h`
* `pins_e1001.h` (SCK=7, MOSI=9, CS=10, DC=11, RST=12, BUSY=13, LED=6, BUZZER=45, BATT_EN=21)
* `pins_e1002.h` (SCK=7, MOSI=9, CS=10, DC=11, RST=12, BUSY=13, LED=6, BUZZER=45, BATT_EN=21)
* `pins_e1003.h` (SCK=7, MISO=8, MOSI=9, CS=10, RST=12, BUSY=13, TFT_EN=11, ITE_EN=21, LED=16, BATT_EN=40)
* `pins_e1004.h` (SCK=7, MISO=8, MOSI=9, CS=10, DC=11, CS1=2, RST=38, BUSY=13, ENABLE=12, LED=48, BATT_EN=21)

---

## 4. Host Simulator Architecture (`screentinker_sim`)

The simulator provides an instant testbench without needing the physical hardware:

1. **CLI & JSON Configuration**:
   * Flag: `--device <sticky|e1001|e1002|e1003|e1004>`
   * Default fallback to `sim_config.json`: `"device_model": "e1001"`
2. **Dynamic Viewport**:
   * Initializes SDL2 window to exact resolution (`800x480`, `1872x1404`, `1200x1600`).
   * Supports `--scale <1-4>` so large panels (like 1872×1404) fit comfortably on standard laptop screens (e.g. at scale 0.5 or scale 1).
3. **Dynamic Pairing Registration**:
   * In `--pair` mode, informs the ScreenTinker server of the exact screen profile:
     ```json
     {
       "screen_profile": "seeed-reterminal-e1001",
       "screen_width": 800,
       "screen_height": 480
     }
     ```
4. **Adaptive UI Generator**:
   * `setup_screen.cpp` adapts QR code and pairing code centering dynamically based on `EPD_WIDTH` and `EPD_HEIGHT`.

---

## 5. Phased Implementation Roadmap

* **Phase 1: Simulator Multi-Device Profile Engine** [✓ PR #16]
  * Update `sim_config.h` & `sim_config.cpp` to parse `--device` and profile presets.
  * Update `sim_display.h` & `sim_display.cpp` to dynamically allocate and resize viewport.
  * Update `main_sim.cpp` pairing flow to announce chosen model.
* **Phase 2: Responsive Setup Screen** [✓ PR #17]
  * Refactor `setup_screen.cpp` so splash screens, pairing PINs, and QR codes scale and center properly for 800×480, 1872×1404, and 1200×1600.
* **Phase 3: Firmware Pinout & Profile Headers** [✓ PR #18]
  * Create `include/pins_e1001.h`, `include/pins_e1002.h`, `include/pins_e1003.h`, `include/pins_e1004.h`.
  * Create `include/device_profile.h` and conditionalize `config.h` dimensions.
  * Add corresponding environments in `platformio.ini` [✓ PR #19].
* **Phase 4: Display Driver Integration & Verification** [✓ PR #20]
  * Integrate display controllers for all 5 models (SSD1677, UC8179, ED2208, IT8951, T133A01).
  * Test simulated pairing with ScreenTinker server across all 5 profiles.
  * Validate bitmap downloads and BMP snapshot outputs.
* **Phase 5: Orientation Modes & Sensor Heartbeat Telemetry** (In Progress)
  * Implement firmware settings menu orientation control (`Auto`/`Landscape`/`Portrait` for Sticky, `Landscape`/`Portrait` for E1001–E1004) (#21).
  * Integrate LSM6DS3TR-C accelerometer driver on Sticky for debounced auto-orientation switching (#21).
  * Integrate Sensirion SHT40 driver across all 5 devices and transmit temperature/humidity in sync heartbeat headers (#22).
  * Add simulator CLI & hotkey support (`--orientation`, `O`, `R`, mock sensor telemetry) (#21, #22).
  * Companion server integration for `device_telemetry` ingestion and web UI visualization (renebohne/screentinker#7).
