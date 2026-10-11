#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#endif

/*
 * Pinout Definitions for Seeed Studio reTerminal E1003
 * MCU: ESP32-S3R8 (ESP32-S3-WROOM-1)
 * Controller: IT8951 E-Paper Timing Controller
 * Display: 10.3" Grayscale E-Paper (1872x1404)
 */

// ─── IT8951 SPI & Control ───────────────────────────────────────────────────
#define PIN_EPD_SCK      7
#define PIN_EPD_MISO     8
#define PIN_EPD_MOSI     9
#define PIN_EPD_CS       10
#define PIN_EPD_RST      12
#define PIN_EPD_BUSY     13
#define PIN_EPD_TFT_EN   11
#define PIN_EPD_ITE_EN   21

// ─── Peripherals & Power ─────────────────────────────────────────────────────
#define PIN_LED          16
#define PIN_BATT_EN      40

// ─── User Buttons ────────────────────────────────────────────────────────────
#define PIN_BTN_OK       4
#define PIN_BTN_UP       5
#define PIN_BTN_DOWN     6

// ─── I2C Sensors ─────────────────────────────────────────────────────────────
#define PIN_I2C_SDA      19
#define PIN_I2C_SCL      20

// ─── Capacitive Touch (Goodix GT911) ─────────────────────────────────────────
#define PIN_TOUCH_SDA    19
#define PIN_TOUCH_SCL    20
#define PIN_TOUCH_INT    1
#define PIN_TOUCH_RST    2
