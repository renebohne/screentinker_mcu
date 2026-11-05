#include "anim_player.h"

#if defined(DEVICE_IS_LED_STRIP)
#include "led_driver.h"
#include "led_config.h"

static uint8_t* s_buf = nullptr;       // the whole STA1 animation as received
static size_t s_len = 0;
static uint8_t* s_fb = nullptr;        // the composed frame, w*h*3
static uint16_t s_w = 0, s_h = 0, s_frames = 0, s_loops = 0;
static uint16_t s_index = 0, s_loopDone = 0;
static size_t s_offset = 0;            // byte offset of frame s_index in s_buf
static uint32_t s_nextAt = 0;
static uint16_t s_delay = 0;
static bool s_holding = false;         // finished its loop count: keep the last frame
static char s_order[4] = "GRB";

static inline uint16_t rd16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static inline uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

size_t anim_budget() {
  // Half the largest free block, at most 128 KB. The animation playing now is freed before the next
  // one is allocated (main.cpp), so its bytes count as available. The C3 has no PSRAM; if the
  // allocation still fails, the current picture is kept rather than anything breaking.
  size_t largest = ESP.getMaxAllocHeap();
#if defined(BOARD_HAS_PSRAM)
  if (psramFound()) largest = ESP.getMaxAllocPsram();
#endif
  size_t b = (largest + s_len) / 2;
  if (b > 128 * 1024) b = 128 * 1024;
  return b;
}

void anim_stop() {
  if (s_buf) free(s_buf);
  if (s_fb) free(s_fb);
  s_buf = nullptr; s_fb = nullptr; s_len = 0; s_frames = 0;
}

bool anim_active() { return s_buf != nullptr && s_fb != nullptr; }

void anim_set_color_order(const char* order) {
  if (order && strlen(order) == 3) { strncpy(s_order, order, 3); s_order[3] = 0; }
}

/* Check every frame header fits before playing anything, so a short or corrupt body never reads past the end. */
static bool validate(const uint8_t* buf, size_t len) {
  if (len < 16 || memcmp(buf, "STA1", 4) != 0 || buf[4] != 1 || buf[5] != 1) return false;
  uint16_t w = rd16(buf + 6), h = rd16(buf + 8), n = rd16(buf + 10);
  if (w == 0 || h == 0 || n < 2 || (uint32_t)w * h > 16384) return false;
  size_t off = 16;
  for (uint16_t i = 0; i < n; i++) {
    if (off + 16 > len) return false;
    uint16_t x = rd16(buf + off + 2), y = rd16(buf + off + 4), rw = rd16(buf + off + 6), rh = rd16(buf + off + 8);
    uint32_t dl = rd32(buf + off + 12);
    if ((uint32_t)x + rw > w || (uint32_t)y + rh > h) return false;
    if (dl != (uint32_t)rw * rh * 3) return false;
    if (i == 0 && (x != 0 || y != 0 || rw != w || rh != h)) return false;
    off += 16 + dl;
    if (off > len) return false;
  }
  return off == len;
}

/* Copy frame s_index's rectangle into the framebuffer, show it, and schedule the next. */
static void showCurrent() {
  const uint8_t* f = s_buf + s_offset;
  s_delay = rd16(f);
  uint16_t x = rd16(f + 2), y = rd16(f + 4), rw = rd16(f + 6), rh = rd16(f + 8);
  const uint8_t* data = f + 16;
  for (uint16_t row = 0; row < rh; row++) {
    memcpy(s_fb + (((size_t)(y + row) * s_w + x) * 3), data + (size_t)row * rw * 3, (size_t)rw * 3);
  }
  led_driver_show_rgb(s_fb, (size_t)s_w * s_h * 3, s_order);
  led_config_note_frame_estimate(led_driver_last_estimate_ma());
  s_nextAt = millis() + (s_delay ? s_delay : 100);
}

static void rewind() {
  s_index = 0;
  s_offset = 16;
}

bool anim_start(uint8_t* buf, size_t len, const char* colorOrder) {
  anim_stop();
  if (!buf || !validate(buf, len)) {
    Serial.printf("[Anim] Rejected animation (%u bytes): not a valid STA1 for this device.\n", (unsigned)len);
    if (buf) free(buf);
    return false;
  }
  s_w = rd16(buf + 6); s_h = rd16(buf + 8); s_frames = rd16(buf + 10); s_loops = rd16(buf + 12);
  s_fb = (uint8_t*)malloc((size_t)s_w * s_h * 3);
  if (!s_fb) {
    Serial.println("[Anim] Out of memory for the framebuffer.");
    free(buf);
    return false;
  }
  s_buf = buf; s_len = len;
  s_loopDone = 0; s_holding = false;
  anim_set_color_order(colorOrder);
  rewind();
  showCurrent();
  Serial.printf("[Anim] Playing %ux%u, %u frames, %s (%u bytes). Heap free %u, largest %u, min ever %u.\n", s_w, s_h, s_frames,
                s_loops == 0 ? "looping" : "finite", (unsigned)len,
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (unsigned)ESP.getMinFreeHeap());
  return true;
}

void anim_tick() {
  if (!anim_active() || s_holding) return;
  if ((int32_t)(millis() - s_nextAt) < 0) return;
  // Advance.
  s_offset += 16 + rd32(s_buf + s_offset + 12);
  s_index++;
  if (s_index >= s_frames) {
    s_loopDone++;
    if (s_loops != 0 && s_loopDone >= s_loops) { s_holding = true; return; }
    // Frame 0 is the whole screen again, so restarting needs no clear.
    rewind();
  }
  showCurrent();
}

#endif
