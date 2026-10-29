#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#endif

/*
 * Pinout Definitions for Seeed Studio LED Driver Board for XIAO
 * Compatible with Seeed Studio XIAO ESP32-C6, ESP32-S3, ESP32-C3
 */

// ─── LED Data Terminal (Level-shifted to 5V) ──────────────────────────────────
#ifndef PIN_LED_DATA
#define PIN_LED_DATA     D0  // Hardware D0 terminal on Seeed LED Driver Board
#endif

// ─── Grove Connector (Standard XIAO I2C Bus) ──────────────────────────────────
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA      D4  // Grove Pin 2 (White)
#endif

#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL      D5  // Grove Pin 1 (Yellow)
#endif

// ─── XIAO ESP32-C6 RF & Antenna Controls ─────────────────────────────────────
#if defined(ARDUINO_XIAO_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(HAS_XIAO_RF_SWITCH)
#ifndef PIN_WIFI_ENABLE
#define PIN_WIFI_ENABLE      3   // Active LOW: powers on RF frontend switch
#endif
#ifndef PIN_WIFI_ANT_CONFIG
#define PIN_WIFI_ANT_CONFIG  14  // LOW = built-in ceramic antenna, HIGH = external u.FL antenna
#endif
#endif

// ─── User Button (XIAO onboard BOOT button) ──────────────────────────────────
// On ESP32-C6, GPIO 9 is the BOOT button (Active LOW)
// On ESP32-S3, GPIO 0 is the BOOT button (Active LOW)
// On ESP32-C3, GPIO 9 is the BOOT button (Active LOW)
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C3)
#define PIN_BTN_OK       9
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define PIN_BTN_OK       0
#else
#define PIN_BTN_OK       -1
#endif

#define PIN_BTN_UP       -1
#define PIN_BTN_DOWN     -1

// ─── Dummy / Unused Peripherals ───────────────────────────────────────────────
#define PIN_PWR_HOLD     -1
#define PIN_PWR_LOCK     -1
#define PIN_EXT_POWER    -1
#define PIN_CHG_STAT     -1
#define PIN_CHG_EN       -1

#define PIN_EPD_SCK      -1
#define PIN_EPD_MOSI     -1
#define PIN_EPD_MISO     -1
#define PIN_EPD_CS       -1
#define PIN_EPD_DC       -1
#define PIN_EPD_RST      -1
#define PIN_EPD_BUSY     -1
#define PIN_EPD_PWR_EN   -1

#define PIN_BUZZER       -1
#define PIN_SD_CS        -1
#define PIN_SD_EN        -1
#define PIN_SD_DETECT    -1

#define PIN_TOUCH_SCL    -1
#define PIN_TOUCH_SDA    -1
#define PIN_TOUCH_EN     -1
#define PIN_TOUCH_INT    -1
#define PIN_TOUCH_RST    -1
#define PIN_IMU_INT      -1
