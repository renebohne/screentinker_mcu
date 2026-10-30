#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <algorithm>
using std::max;
using std::min;
#endif
#include "config.h"

enum Language {
  LANG_EN = 0,
  LANG_DE = 1
};

enum OrientationMode {
  ORIENT_MODE_AUTO = 0,
  ORIENT_MODE_LANDSCAPE = 1,
  ORIENT_MODE_PORTRAIT = 2
};

// ─── Simple 8x8 ASCII Font (Characters 32 - 126) ─────────────────────────────
extern const uint8_t font8x8_basic[95][8];

void gfx_set_screen_dimensions(int w, int h);
int gfx_get_width();
int gfx_get_height();
size_t gfx_get_buffer_size();

void gfx_fill(uint8_t* buf, uint8_t color);
void gfx_draw_pixel(uint8_t* buf, int x, int y, uint8_t color);
uint8_t gfx_get_pixel(const uint8_t* buf, int x, int y);
void gfx_draw_hline(uint8_t* buf, int x, int y, int w, uint8_t color);
void gfx_draw_vline(uint8_t* buf, int x, int y, int h, uint8_t color);
void gfx_draw_rect(uint8_t* buf, int x, int y, int w, int h, uint8_t color);
void gfx_fill_rect(uint8_t* buf, int x, int y, int w, int h, uint8_t color);
void gfx_draw_char(uint8_t* buf, int x, int y, char c, uint8_t color, int scale);
void gfx_draw_string(uint8_t* buf, int x, int y, const char* str, uint8_t color, int scale);
void gfx_draw_screentinker_logo(uint8_t* buf, int x, int y, int scale = 1);
void gfx_draw_qr(uint8_t* buf, int x, int y, const char* text, int scale = 4);

void renderSplashScreen(uint8_t* buffer, const char* version, const char* statusMsg, Language lang = LANG_EN);
void renderOnboardingScreen(uint8_t* buffer, Language lang = LANG_EN);
void renderNoWifiScreen(uint8_t* buffer, const char* ssid, Language lang = LANG_EN);
void renderEmptyPlaylistScreen(uint8_t* buffer, const char* deviceName, const char* serverUrl, Language lang = LANG_EN);
void renderPairingCodeScreen(uint8_t* buffer, const char* code, const char* serverUrl, Language lang = LANG_EN);
void renderStatusInfoScreen(uint8_t* buffer, const char* ip, const char* ssid, int rssi, const char* serverUrl, const char* devId, const char* version, Language lang = LANG_EN, float tempC = -999.0f, float humidityPct = -1.0f, int batteryPct = -1, bool isCharging = false, bool isUsbPower = false);
void renderSystemMenu(uint8_t* buffer, int selectedIndex, Language lang = LANG_EN, uint8_t wifiTxLevel = 0, uint8_t orientMode = 1, bool hasImu = false, int batteryPct = -1, bool isCharging = false, bool isUsbPower = false, uint8_t refreshMode = 0, bool supportsPartial = true);
void renderPowerOffScreen(uint8_t* buffer, Language lang = LANG_EN, bool isBatteryDepleted = false);
void renderBatteryDepletedScreen(uint8_t* buffer, Language lang = LANG_EN);
void renderUnsupportedContentScreen(uint8_t* buffer, const char* reason = nullptr, Language lang = LANG_EN);
void renderOtaUpdateScreen(uint8_t* buffer, const char* statusMsg = nullptr, Language lang = LANG_EN);
void renderImuDebugScreen(uint8_t* buffer, int orient, int16_t ax, int16_t ay, int16_t az);
void gfx_draw_offline_badge(uint8_t* buffer);
void gfx_draw_low_battery_icon(uint8_t* buffer, int x = -1, int y = -1, int batteryPct = -1);
void gfx_draw_low_battery_icon_native(uint8_t* buffer, int width, int height, int colorMode, int x = -1, int y = -1, int batteryPct = -1);

// ─── Display Buffer Rotation Algorithms ──────────────────────────────────────
void rotate_mono_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height);
void rotate_gray4_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height);
void rotate_nibble_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height);
void rotate_rgb565_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height);
void rotate_mono_portrait(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip = false);
void rotate_gray4_portrait(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip = false);
void rotate_nibble_portrait(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip = false);
void rotate_rgb565_portrait(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip = false);
void rotate_buffer_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height);
void rotate_buffer_180_mode(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height, uint8_t colorMode);
void rotate_buffer_portrait_mode(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip, uint8_t colorMode);



