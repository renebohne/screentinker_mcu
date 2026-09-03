#pragma once

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

// ─── Display & Timing Parameters ─────────────────────────────────────────────
#define EPD_WIDTH             800
#define EPD_HEIGHT            480
#define EPD_BUFFER_SIZE       (EPD_WIDTH * EPD_HEIGHT / 8) // 48,000 Bytes

// ─── PSRAM Cache & Power Management ──────────────────────────────────────────
#define MAX_CACHED_ITEMS      32   // Up to 32 frames in 8MB PSRAM (~1.5 MB)
#define DEFAULT_SLEEP_SEC     30   // Default sync interval in seconds
#define HTTP_TIMEOUT_MS       15000
#define ENABLE_WIFI_POWER_SAVE true // Disconnect WiFi when idle to save battery

// Set to true to always download and draw the frame (bypasses 304 on boot)
#define FORCE_REFRESH_ON_BOOT false
