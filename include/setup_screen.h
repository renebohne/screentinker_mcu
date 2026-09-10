#pragma once
#include <Arduino.h>
#include "config.h"

enum Language {
  LANG_EN = 0,
  LANG_DE = 1
};

// ─── Simple 8x8 ASCII Font (Characters 32 - 126) ─────────────────────────────
extern const uint8_t font8x8_basic[95][8];

void gfx_fill(uint8_t* buf, uint8_t color);
void gfx_draw_pixel(uint8_t* buf, int x, int y, uint8_t color);
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
void renderPairingCodeScreen(uint8_t* buffer, const char* code, const char* serverUrl, Language lang = LANG_EN);
void renderStatusInfoScreen(uint8_t* buffer, const char* ip, const char* ssid, int rssi, const char* serverUrl, const char* devId, const char* version, Language lang = LANG_EN);
void renderSystemMenu(uint8_t* buffer, int selectedIndex, Language lang = LANG_EN, uint8_t wifiTxLevel = 0);
void renderPowerOffScreen(uint8_t* buffer, Language lang = LANG_EN);
void gfx_draw_offline_badge(uint8_t* buffer);


