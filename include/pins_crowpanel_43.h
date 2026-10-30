#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#endif

/*
 * Pinout Definitions for Elecrow CrowPanel 4.3" HMI ESP32 Display (480x272)
 * MCU: ESP32-S3 (ESP32-S3-WROOM-1-N4R2: 4MB Flash, 2MB PSRAM)
 * Display Controller: NV3047 / ILI6485 16-Bit Parallel RGB TFT
 * Touch Controller: XPT2046 Resistive Touch via SPI
 */

// ─── Display Backlight ────────────────────────────────────────────────────────
#define PIN_LCD_BL        2   // Backlight control (PWM capable)

// ─── 16-Bit Parallel RGB Interface Pins ───────────────────────────────────────
// Sync and Clock Control
#define PIN_LCD_DE        40  // Data Enable (DE)
#define PIN_LCD_VSYNC     41  // Vertical Sync (VSYNC)
#define PIN_LCD_HSYNC     39  // Horizontal Sync (HSYNC)
#define PIN_LCD_PCLK      42  // Pixel Clock (DCLK / PCLK)

// Red Bus (5 bits: R0 - R4)
#define PIN_LCD_R0        45
#define PIN_LCD_R1        48
#define PIN_LCD_R2        47
#define PIN_LCD_R3        21
#define PIN_LCD_R4        14

// Green Bus (6 bits: G0 - G5)
#define PIN_LCD_G0        5
#define PIN_LCD_G1        6
#define PIN_LCD_G2        7
#define PIN_LCD_G3        15
#define PIN_LCD_G4        16
#define PIN_LCD_G5        4

// Blue Bus (5 bits: B0 - B4)
#define PIN_LCD_B0        8
#define PIN_LCD_B1        3
#define PIN_LCD_B2        46
#define PIN_LCD_B3        9
#define PIN_LCD_B4        1

// ─── RGB LCD Timing Parameters (NV3047 / ILI6485 480x272) ────────────────────
#define LCD_PIXEL_CLOCK_HZ    (6000000) // 6 MHz (Elecrow official specification for Quad PSRAM)
#define LCD_HSYNC_POLARITY    0
#define LCD_HSYNC_FRONT_PORCH 8
#define LCD_HSYNC_PULSE_WIDTH 4
#define LCD_HSYNC_BACK_PORCH  43
#define LCD_VSYNC_POLARITY    0
#define LCD_VSYNC_FRONT_PORCH 8
#define LCD_VSYNC_PULSE_WIDTH 4
#define LCD_VSYNC_BACK_PORCH  12
#define LCD_PCLK_ACTIVE_NEG   1

// ─── Resistive Touch Digitizer (XPT2046 SPI) ──────────────────────────────────
// NOTE: On CrowPanel 4.3" PCB, the XPT2046 CS pin is physically tied to GPIO 0,
// which is shared with the physical BOOT button. To keep the BOOT button dedicated
// for rock-solid menu navigation and avoid bus contention or electrical shorts,
// PIN_TOUCH_CS is currently set to -1 (Touch disabled). If touch is re-enabled in
// the future, GPIO 0 must be handled via Open-Drain or pulsed only on PENIRQ (GPIO 36).
#define PIN_TOUCH_CS      -1
#define PIN_TOUCH_SCK     12
#define PIN_TOUCH_MOSI    11
#define PIN_TOUCH_MISO    13
#define PIN_TOUCH_INT     36  // Interrupt / PENIRQ (Active Low)

// ─── MicroSD Card (Shares SPI lines with Touch) ────────────────────────────────
#define PIN_SD_CS         10
#define PIN_SD_SCK        12
#define PIN_SD_MOSI       11
#define PIN_SD_MISO       13

// ─── Physical Buttons & Audio ─────────────────────────────────────────────────
#define PIN_BTN_BOOT      0   // Physical BOOT Button on GPIO 0 (Active LOW)
#define PIN_BTN_OK        PIN_BTN_BOOT
#define PIN_BTN_USER      38  // Secondary User / Grove expansion pin
#define PIN_BTN_UP        -1
#define PIN_BTN_DOWN      -1
#define PIN_I2S_DOUT      20
#define PIN_I2S_BCLK      35
#define PIN_I2S_LRC       19
