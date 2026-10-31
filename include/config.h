#pragma once

#define FIRMWARE_VERSION      "1.4.2"

// ─── Network & Wi-Fi Configuration ───────────────────────────────────────────
#define WIFI_SSID             "Your-WiFi-SSID"
#define WIFI_PASSWORD         "Your-WiFi-Password"

// ─── ScreenTinker Server Configuration ────────────────────────────────────────
// Replace with the local IP of your server (e.g. http://192.168.1.100:3001)
#define SCREENTINKER_HOST     "http://192.168.1.100:3001"

// ─── Device Credentials ───────────────────────────────────────────────────────
// Copy these from your ScreenTinker Web UI (Displays -> your display)
#define DEVICE_ID             "your-device-uuid"
#define DEVICE_TOKEN          "your-device-token"

#include "device_profile.h"

// ─── Display & Timing Parameters ─────────────────────────────────────────────
#ifndef EPD_WIDTH
#define EPD_WIDTH             DEVICE_DISPLAY_WIDTH
#endif
#ifndef EPD_HEIGHT
#define EPD_HEIGHT            DEVICE_DISPLAY_HEIGHT
#endif
#ifndef EPD_BUFFER_SIZE
#define EPD_BUFFER_SIZE       DEVICE_BUFFER_SIZE
#endif

// ─── PSRAM Cache & Power Management ──────────────────────────────────────────
#ifndef MAX_CACHED_ITEMS
#if defined(DEVICE_IS_LED_STRIP)
#define MAX_CACHED_ITEMS      4
#elif defined(BOARD_HAS_PSRAM)
// Up to 4MB total cache in 8MB PSRAM (32 frames for 48KB, 20 for 192KB, 3 for 1.31MB, 4 for 960KB)
#define MAX_CACHED_ITEMS      ((4000000 / EPD_BUFFER_SIZE) > 32 ? 32 : ((4000000 / EPD_BUFFER_SIZE) < 2 ? 2 : (4000000 / EPD_BUFFER_SIZE)))
#else
#define MAX_CACHED_ITEMS      2
#endif
#endif
#define DEFAULT_SLEEP_SEC     30   // Default sync interval in seconds
#define HTTP_TIMEOUT_MS       15000
#define ENABLE_WIFI_POWER_SAVE true // Disconnect WiFi when idle to save battery

// ─── Power & Battery Thresholds ──────────────────────────────────────────────
#ifndef CRITICAL_BATTERY_SHUTDOWN_PCT
#define CRITICAL_BATTERY_SHUTDOWN_PCT 5   // Trigger graceful shutdown and render battery-depleted screen at <= 5%
#endif

// Set to true to always download and draw the frame (bypasses 304 on boot)
#define FORCE_REFRESH_ON_BOOT false

// Set to true to render multi-zone layouts by default
#define DEFAULT_MULTI_ZONE    false

