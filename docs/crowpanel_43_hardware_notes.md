# Elecrow CrowPanel 4.3" HMI ESP32 Display (480x272) — Hardware Notes

## Overview
- **MCU**: Espressif ESP32-S3 (ESP32-S3-WROOM-1-N4R2: 4MB Flash Quad, 2MB PSRAM Quad)
- **Display Controller**: NV3047 / ILI6485 16-Bit Parallel RGB TFT (480x272)
- **Touch Controller**: XPT2046 Resistive Touch via SPI
- **Storage**: MicroSD Card Slot via SPI (Shares SPI lines with Touch)

---

## Display Configuration
- **Bus**: 16-bit RGB parallel bus (`Arduino_ESP32RGBPanel` in `GFX Library for Arduino`)
- **Pixel Clock**: 6 MHz (`LCD_PIXEL_CLOCK_HZ = 6000000`, active negative `LCD_PCLK_ACTIVE_NEG = 1`).
- **Timing Porches**:
  - HSYNC: Front porch 8, Pulse width 4, Back porch 43, Polarity 0.
  - VSYNC: Front porch 8, Pulse width 4, Back porch 12, Polarity 0.
- **Bounce Buffer Size**: 3840 pixels (480 x 8 lines) in internal SRAM. Must evenly divide 272 (`(480 * 272) % 3840 == 0`).
- **Network PSRAM Decoupling**: Download chunks are read into internal SRAM buffer `s_netChunk[4096]` before bulk writing into Quad PSRAM to prevent bus saturation during Wi-Fi packet arrivals.

---

## Touch Controller (XPT2046) & Physical BOOT Button Hardware Sharing
On the CrowPanel 4.3" PCB, the 16-bit parallel RGB bus consumes 20 GPIOs. To fit within the ESP32-S3 pin constraints, Elecrow designers shared **GPIO 0** between two functions:
1. **Physical BOOT Button**: Active LOW button to ground with an external pullup resistor.
2. **XPT2046 Touch Chip Select (`TP_CS`)**: Active LOW SPI chip select pin.

### Implications:
- When GPIO 0 is driven as a standard digital `OUTPUT` driven `HIGH`, pressing the physical BOOT button creates an electrical path directly between the output pin and GND.
- When GPIO 0 is configured as `INPUT_PULLUP` to sense the BOOT button, the XPT2046 CS pin floats/is pulled HIGH, remaining deselected and non-responsive over SPI.
- The touch digitizer is a **resistive** pressure panel (pressure/stylus needed), requiring calibration matrix mapping to the 480x272 pixel coordinate space.

### Decision:
- **Current State**: Touch is disabled (`PIN_TOUCH_CS = -1`). The physical BOOT button on GPIO 0 is dedicated to opening the menu (short press), stepping through items (short press), and executing selections (long press > 1.2s).
- **Future Touch Re-activation**: If touch is to be combined with the BOOT button, GPIO 0 must be handled in Open-Drain mode or driven LOW exclusively upon detecting a pen interrupt on `PIN_TOUCH_INT` (GPIO 36, active LOW), then immediately released back to input.
