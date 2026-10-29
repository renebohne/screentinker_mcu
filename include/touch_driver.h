#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Represents a 2D touch point in display coordinates (pixels).
typedef struct {
  int x;
  int y;
  bool pressed;
} touch_point_t;

// Initialize touch digitizer (GT911 / FT6336U) on target devices supporting touch.
// Returns true if a supported touch digitizer responded on the I2C bus.
bool touch_init();

// Poll current touch state. Returns true if polling succeeded.
// Populates out_point with coordinates and pressed status.
bool touch_read(touch_point_t* out_point);

// Transition touch digitizer to deep sleep / low power standby mode.
void touch_sleep();

// Wake touch digitizer from sleep mode.
void touch_wake();

// Hit-test calculation for the 9-item System Menu.
// Returns menu index 0..8 if hit, or -1 if no item tapped.
int touch_hit_test_menu(int x, int y, int screen_width, int screen_height);

// Hit-test calculation for Status & Info screen.
// Returns true if tap is anywhere inside the screen bounds.
bool touch_hit_test_status(int x, int y, int screen_width, int screen_height);

#ifdef __cplusplus
}
#endif
