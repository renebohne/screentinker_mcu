#pragma once
/*
 * Remote LED settings from ScreenTinker (X-ST-Led-Config on every render answer) plus the power
 * limiter's brownout learning. Server side: server/lib/embedded-led-config.js in ScreenTinker.
 *
 *   X-ST-Led-Config: v=1;rev=<8 hex>;b=<0-255>;o=<GRB..>;wir=<serpentine|progressive>;
 *                    ax=<horizontal|vertical>;org=<top_left..>;w=<cols>;h=<rows>;
 *                    pl=<mA, 0 = none>;auto=<0|1>;lr=<n>
 *   X-ST-Led-State (sent back): rev=..;b=..;pl=<effective mA>;lpl=<learned mA>;bo=<brownouts>;
 *                    est=<last frame mA>;rr=<last reset reason>
 *
 * Settings are applied only when `rev` changes, and saved to NVS so a reboot keeps them.
 */
#include "device_profile.h"

#if defined(DEVICE_IS_LED_STRIP)
#include <Arduino.h>

struct LedRemoteConfig {
  char rev[9];
  uint8_t brightness;
  char colorOrder[4];
  uint8_t wiring;        // MatrixWiring
  uint8_t axis;          // MatrixAxis
  uint8_t origin;        // MatrixOrigin
  uint16_t width;
  uint16_t height;
  uint32_t powerLimitMa; // 0 = no limit
  bool autoProtect;
  uint16_t learnedReset;
};

// Boot: load the saved settings, and if this boot follows a brownout, lower the learned limit.
void led_config_begin();
// Parse + apply X-ST-Led-Config. Returns true when something changed (and was saved).
bool led_config_apply_header(const String& header);
// The X-ST-Led-State value for the next render request.
String led_config_state_header();
// What led_driver_show_rgb should use for this frame.
const char* led_config_color_order();
uint32_t led_config_effective_limit_ma();
const LedRemoteConfig& led_config_current();
// The limiter's estimate for the last frame (mA), kept across a brownout reset so the next boot
// knows how much the supply could not deliver.
void led_config_note_frame_estimate(uint32_t ma);

#endif
