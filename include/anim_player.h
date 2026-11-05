#pragma once
/*
 * Plays STA1 animations from ScreenTinker (server/lib/embedded-animation.js) on LED builds.
 *
 * The firmware asks for them with X-ST-Animation: 1 (+ -Budget bytes, -Max-Fps) and gets
 * Content-Type application/x-st-anim. Layout (little-endian):
 *   header 16 B: "STA1", u8 version=1, u8 format (1 = RGB888), u16 w, u16 h, u16 frames, u16 loops (0 = forever), u16 0
 *   per frame 16 B + data: u16 delay ms, u16 x, u16 y, u16 w, u16 h, u16 0, u32 len, then w*h*3 bytes
 * Frame 0 is the whole screen; later frames are only the rectangle that changed.
 */
#include "device_profile.h"

#if defined(DEVICE_IS_LED_STRIP)
#include <Arduino.h>

// The most bytes this device can take for an animation right now (sent as X-ST-Animation-Budget).
size_t anim_budget();
// Takes ownership of `buf` (malloc'd). Validates it, shows frame 0 and starts playing.
// Returns false (and frees buf) when it is not a valid STA1 animation for this device.
bool anim_start(uint8_t* buf, size_t len, const char* colorOrder);
// Call every loop(): shows the next frame when it is due.
void anim_tick();
// Stop and free (a still frame or another item replaced it).
void anim_stop();
bool anim_active();
// Same colour order the still frames use; re-read when the server changes it.
void anim_set_color_order(const char* order);

#endif
