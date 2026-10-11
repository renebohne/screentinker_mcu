#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#endif

/*
 * Pinout Definitions for Seeed Studio reTerminal E1002
 * MCU: ESP32-S3R8 (ESP32-S3-WROOM-1)
 * Display: 3.97" Spectra 6 Full-Color E-Paper (800x480)
 */

// ─── E-Paper SPI & Control ───────────────────────────────────────────────────
#define PIN_EPD_SCK      7
#define PIN_EPD_MOSI     9
#define PIN_EPD_CS       10
#define PIN_EPD_DC       11
#define PIN_EPD_RST      12
#define PIN_EPD_BUSY     13

// ─── Peripherals & Power ─────────────────────────────────────────────────────
#define PIN_LED          6
#define PIN_BUZZER       45
#define PIN_BATT_EN      21

// ─── User Buttons ────────────────────────────────────────────────────────────
#define PIN_BTN_OK       4
#define PIN_BTN_UP       5
#define PIN_BTN_DOWN     6

// ─── I2C Sensors ─────────────────────────────────────────────────────────────
#define PIN_I2C_SDA      19
#define PIN_I2C_SCL      20
