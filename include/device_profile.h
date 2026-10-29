#pragma once

/*
 * ScreenTinker Unified Device Profile Engine
 * Maps compile-time hardware target flags to pinouts, panel geometries,
 * color modes, refresh characteristics, and touch digitizer architectures.
 */

// ─── Color Modes ─────────────────────────────────────────────────────────────
#define COLOR_MODE_1BPP_MONO   0
#define COLOR_MODE_GRAY4       1
#define COLOR_MODE_GRAY16      2
#define COLOR_MODE_SPECTRA6    3

// Note: touch-capable targets (DEVICE_SUPPORTS_TOUCH=1) auto-probe GT911 (0x5D/0x14)
// and FT6336U (0x38) on the I2C bus at runtime; see src/touch_driver.cpp.

#if defined(DEVICE_RETERMINAL_E1001)
  #include "pins_e1001.h"
  #define DEVICE_PROFILE_NAME              "e1001"
  #define DEVICE_MODEL_NAME                "Seeed Studio reTerminal E1001"
  #define DEVICE_SCREEN_PROFILE            "seeed-reterminal-e1001"
  #define DEVICE_DISPLAY_WIDTH             800
  #define DEVICE_DISPLAY_HEIGHT            480
  #define DEVICE_COLOR_MODE                COLOR_MODE_1BPP_MONO
  #define DEVICE_PANEL_MODEL               SEEED_EPAPER_PANEL_UC8179
  #define DEVICE_BUFFER_SIZE               96000
  #define DEVICE_SUPPORTS_PARTIAL_REFRESH  1
  #define DEVICE_SUPPORTS_GRAY4            1
  #define DEVICE_FULL_REFRESH_TIME_MS      3000
  #define DEVICE_PARTIAL_REFRESH_TIME_MS   350
  #define DEVICE_SUPPORTS_TOUCH            0
  #define DEVICE_SUPPORTS_IMU              0

#elif defined(DEVICE_RETERMINAL_E1002)
  #include "pins_e1002.h"
  #define DEVICE_PROFILE_NAME              "e1002"
  #define DEVICE_MODEL_NAME                "Seeed Studio reTerminal E1002"
  #define DEVICE_SCREEN_PROFILE            "seeed-reterminal-e1002"
  #define DEVICE_DISPLAY_WIDTH             800
  #define DEVICE_DISPLAY_HEIGHT            480
  #define DEVICE_COLOR_MODE                COLOR_MODE_SPECTRA6
  #define DEVICE_PANEL_MODEL               SEEED_EPAPER_PANEL_ED2208
  #define DEVICE_BUFFER_SIZE               192000
  #define DEVICE_SUPPORTS_PARTIAL_REFRESH  0
  #define DEVICE_SUPPORTS_GRAY4            0
  #define DEVICE_FULL_REFRESH_TIME_MS      14000
  #define DEVICE_PARTIAL_REFRESH_TIME_MS   0
  #define DEVICE_SUPPORTS_TOUCH            0
  #define DEVICE_SUPPORTS_IMU              0

#elif defined(DEVICE_RETERMINAL_E1003)
  #include "pins_e1003.h"
  #define DEVICE_PROFILE_NAME              "e1003"
  #define DEVICE_MODEL_NAME                "Seeed Studio reTerminal E1003"
  #define DEVICE_SCREEN_PROFILE            "seeed-reterminal-e1003"
  #define DEVICE_DISPLAY_WIDTH             1872
  #define DEVICE_DISPLAY_HEIGHT            1404
  #define DEVICE_COLOR_MODE                COLOR_MODE_GRAY16
  #define DEVICE_PANEL_MODEL               SEEED_EPAPER_PANEL_IT8951
  #define DEVICE_BUFFER_SIZE               1314144
  #define DEVICE_SUPPORTS_PARTIAL_REFRESH  1
  #define DEVICE_SUPPORTS_GRAY4            0
  #define DEVICE_FULL_REFRESH_TIME_MS      1200
  #define DEVICE_PARTIAL_REFRESH_TIME_MS   180
  #define DEVICE_SUPPORTS_TOUCH            1
  #define DEVICE_SUPPORTS_IMU              0

#elif defined(DEVICE_RETERMINAL_E1004)
  #include "pins_e1004.h"
  #define DEVICE_PROFILE_NAME              "e1004"
  #define DEVICE_MODEL_NAME                "Seeed Studio reTerminal E1004"
  #define DEVICE_SCREEN_PROFILE            "seeed-reterminal-e1004"
  #define DEVICE_DISPLAY_WIDTH             1200
  #define DEVICE_DISPLAY_HEIGHT            1600
  #define DEVICE_COLOR_MODE                COLOR_MODE_SPECTRA6
  #define DEVICE_PANEL_MODEL               SEEED_EPAPER_PANEL_T133A01
  #define DEVICE_BUFFER_SIZE               960000
  #define DEVICE_SUPPORTS_PARTIAL_REFRESH  0
  #define DEVICE_SUPPORTS_GRAY4            0
  #define DEVICE_FULL_REFRESH_TIME_MS      22000
  #define DEVICE_PARTIAL_REFRESH_TIME_MS   0
  #define DEVICE_SUPPORTS_TOUCH            0
  #define DEVICE_SUPPORTS_IMU              0

#else
  // Default Target: Seeed Studio reTerminal Sticky
  #ifndef DEVICE_RETERMINAL_STICKY
    #define DEVICE_RETERMINAL_STICKY
  #endif
  #include "pins_sticky.h"
  #define DEVICE_PROFILE_NAME              "sticky"
  #define DEVICE_MODEL_NAME                "Seeed Studio reTerminal Sticky"
  #define DEVICE_SCREEN_PROFILE            "seeed-reterminal-sticky"
  #define DEVICE_DISPLAY_WIDTH             800
  #define DEVICE_DISPLAY_HEIGHT            480
  #define DEVICE_COLOR_MODE                COLOR_MODE_1BPP_MONO
  #define DEVICE_PANEL_MODEL               SEEED_EPAPER_PANEL_SSD1677
  #define DEVICE_BUFFER_SIZE               96000
  #define DEVICE_SUPPORTS_PARTIAL_REFRESH  1
  #define DEVICE_SUPPORTS_GRAY4            1
  #define DEVICE_FULL_REFRESH_TIME_MS      1800
  #define DEVICE_PARTIAL_REFRESH_TIME_MS   350
  #define DEVICE_SUPPORTS_TOUCH            1
  #define DEVICE_SUPPORTS_IMU              1
#endif

// ─── Default Fallbacks for Optional Hardware Pins ─────────────────────────────
#ifndef PIN_EPD_MISO
#define PIN_EPD_MISO     -1
#endif

#ifndef PIN_EPD_DC
#define PIN_EPD_DC       -1
#endif
