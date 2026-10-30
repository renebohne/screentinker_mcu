# ScreenTinker MCU — LED Matrix Physical Wiring & Coordinate Mapping Guide

Addressable RGB LEDs (WS2812B, SK6812, etc.) are physically chained in a single continuous 1D data stream (`DIN -> DOUT -> DIN ...`), indexed sequentially from `0` to `(width * height) - 1`.

Because ScreenTinker renders 2D Cartesian image frames in standard row-major raster order ($y=0$ top row left-to-right, $y=1$ second row, etc.), the firmware dynamically translates Cartesian coordinates $(x, y)$ into the physical 1D LED index $i$.

---

## 1. Matrix Degrees of Freedom

Physical LED matrices differ across three hardware parameters:

### A. Wiring Traversal (`MatrixWiring`)
- **`MATRIX_WIRING_SERPENTINE` (Zig-Zag / Snake):** Direction alternates on every subsequent row or column to minimize PCB trace length.
  ```text
  Row 0:  [ 0] -> [ 1] -> [ 2] -> [ 3]
                                    |
  Row 1:  [ 7] <- [ 6] <- [ 5] <- [ 4]
            |
  Row 2:  [ 8] -> [ 9] -> [10] -> [11]
  ```
- **`MATRIX_WIRING_PROGRESSIVE` (Linear / Raster):** Every row or column starts from the same side with return traces.
  ```text
  Row 0:  [ 0] -> [ 1] -> [ 2] -> [ 3]
  Row 1:  [ 4] -> [ 5] -> [ 6] -> [ 7]
  Row 2:  [ 8] -> [ 9] -> [10] -> [11]
  ```

### B. Major Scanning Axis (`MatrixAxis`)
- **`MATRIX_AXIS_HORIZONTAL` (Row-Major):** LEDs run horizontally across rows before advancing vertically. (Most common for standard matrices).
- **`MATRIX_AXIS_VERTICAL` (Column-Major):** LEDs run vertically down columns before advancing horizontally.

### C. Starting Origin (`MatrixOrigin`)
The corner where the data input pad (`DIN` / `LED[0]`) is physically located:
- `MATRIX_ORIGIN_TOP_LEFT` (0) — Standard screen coordinates
- `MATRIX_ORIGIN_TOP_RIGHT` (1)
- `MATRIX_ORIGIN_BOTTOM_LEFT` (2) — Very common on flexible PCB panels (e.g., BTF-Lighting WS2812B 8×8, 8×32, 16×16)
- `MATRIX_ORIGIN_BOTTOM_RIGHT` (3)

<div align="center">
  <img src="images/matrix_serpentine_mapping.svg" alt="LED Matrix Coordinate Mapping: Serpentine vs Progressive" width="100%" style="margin: 16px 0;" />
</div>

---

## 2. Commercial Panel Reference

| Manufacturer / Model | Dimensions | Default Wiring | Primary Axis | Origin |
| :--- | :--- | :--- | :--- | :--- |
| **BTF-Lighting Flexible Matrix** | 8×8, 8×32, 16×16 | `SERPENTINE` | `HORIZONTAL` | `BOTTOM_LEFT` |
| **Adafruit NeoMatrix** (default) | 8×8 | `PROGRESSIVE` | `HORIZONTAL` | `TOP_LEFT` |
| **Waveshare RGB LED Matrix** | 8×8, 16×16 | `SERPENTINE` | `HORIZONTAL` | `TOP_LEFT` |
| **Custom DIY Strip Matrix** | Any | Configurable | Configurable | Configurable |

> [!IMPORTANT]
> **3-Wire Connection for 5V Panels:** The Seeed Studio LED Driver Board (SKU 105100021) has a 4-position screw terminal labeled `[12V] [5V] [A0] [GND]`. When connecting standard 5V addressable LED panels or strips (WS2812B / SK6812), connect **exactly three wires**:
> - **`5V`**: Red wire (5V Logic & Panel Power)
> - **`A0`**: Green wire (Level-shifted Data stream from XIAO `D0`)
> - **`GND`**: Black wire (Common ground)
> 
> The **`12V`** terminal is dedicated solely to 12V LED strips (e.g. WS2811 / WS2815) and must be left **unconnected / empty** for 5V matrices.

<div align="center">
  <img src="images/xiao_led_wiring_diagram.svg" alt="Seeed Studio XIAO LED Controller Wiring Schematic" width="100%" style="margin: 16px 0;" />
</div>

---

## 3. Firmware Configuration

### A. Via USB / WebSerial CLI
You can inspect or reconfigure the topology live over the USB serial terminal without recompiling:

```bash
# Query or configure via JSON:
{"cmd":"matrix","width":64,"height":64,"wiring":"serpentine","origin":"top_left","axis":"horizontal"}

# Or for a BTF-Lighting 16x16 flexible panel starting at bottom-left:
{"cmd":"matrix","width":16,"height":16,"wiring":"serpentine","origin":"bottom_left","axis":"horizontal"}
```

Settings are permanently saved to ESP32 Non-Volatile Storage (NVS) across power cycles and deep sleep.

### B. Via Server Device Profile
During initial pairing or in device configuration, the matrix layout can be passed in the `screen_profile` object:

```json
{
  "preset": "seeed-xiao-led",
  "width": 64,
  "height": 64,
  "colorDepth": "24bit-rgb888",
  "outputFormat": "raw",
  "matrix": {
    "wiring": "serpentine",
    "axis": "horizontal",
    "origin": "top-left"
  }
}
```

---

## 4. Host Simulator Usage

The desktop simulator natively supports all matrix wiring options for visual testing:

```bash
# Serpentine Top-Left (Default for matrices)
./screentinker_sim --width 64 --height 64 --matrix-serpentine --matrix-origin top-left

# Progressive Bottom-Left
./screentinker_sim --width 16 --height 16 --matrix-progressive --matrix-origin bottom-left

# Column-major (vertical) layout
./screentinker_sim --width 32 --height 8 --matrix-axis vertical
```

Settings are stored in the configuration file (`sim_config.json` or `sim_config_64x64.json`):
```json
"matrix": {
  "wiring": "serpentine",
  "axis": "horizontal",
  "origin": "top-left"
}
```

---

## 5. C / C++ API Reference

The universal mapping functions are implemented in [`include/led_matrix_map.h`](file:///Users/rene/Documents/github/screentinker_mcu/include/led_matrix_map.h):

```c
// Convert 2D Cartesian (x, y) to sequential physical LED index
uint16_t led_matrix_xy_to_index(uint16_t x, uint16_t y, const LedMatrixConfig* cfg);

// Inverse: Convert sequential physical LED index to 2D Cartesian (x, y)
void led_matrix_index_to_xy(uint16_t index, const LedMatrixConfig* cfg, uint16_t* outX, uint16_t* outY);
```
