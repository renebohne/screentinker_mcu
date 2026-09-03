#pragma once
#include <Arduino.h>

/*
 * Pinout Definitions for Seeed Studio reTerminal Sticky
 * MCU: ESP32-S3R8 (ESP32-S3-WROOM-1)
 */

// ─── Power & System Latches ───────────────────────────────────────────────────
#define PIN_PWR_HOLD     45  // Must drive HIGH immediately in setup() to latch main power
#define PIN_PWR_LOCK     46  // Must drive HIGH to maintain system power

// ─── 3.97" B/W E-Paper (800x480, SSD1677) ────────────────────────────────────
#define PIN_EPD_SCK      13  // SPI Clock (Shared with SD Card)
#define PIN_EPD_MOSI     14  // SPI MOSI  (Shared with SD Card)
#define PIN_EPD_MISO     12  // SPI MISO  (Shared with SD Card)
#define PIN_EPD_CS       15  // EPD Chip Select (Active Low)
#define PIN_EPD_DC       16  // EPD Data / Command (Low = Command, High = Data)
#define PIN_EPD_RST      17  // EPD Reset (Active Low)
#define PIN_EPD_BUSY     18  // EPD Busy Status (High = Busy)
#define PIN_EPD_PWR_EN   47  // EPD Power Enable (High = Power Boost Circuit On)

// ─── User Buttons ─────────────────────────────────────────────────────────────
#define PIN_BTN_OK       4   // OK / Power / Wakeup button
#define PIN_BTN_UP       5   // UP button
#define PIN_BTN_DOWN     6   // DOWN button

// ─── Peripherals ──────────────────────────────────────────────────────────────
#define PIN_BUZZER       48  // PWM Buzzer
#define PIN_I2C_SDA      1   // I2C Sensors (SHT40, PCF8563, BQ27220, BQ25616)
#define PIN_SD_CS        8   // MicroSD Card Chip Select
#define PIN_SD_EN        10  // MicroSD Power Enable
#define PIN_SD_DETECT    11  // MicroSD Card Detect

#define PIN_TOUCH_SCL    2
#define PIN_TOUCH_SDA    3
#define PIN_TOUCH_EN     42
#define PIN_TOUCH_INT    21
#define PIN_TOUCH_RST    41
