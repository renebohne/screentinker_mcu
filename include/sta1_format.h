#pragma once
/*
 * Pure STA1 binary animation format helpers (little-endian).
 * Shared by src/anim_player.cpp and native unit tests.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline uint16_t sta1_rd16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t sta1_rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Check every frame header fits before playing anything, so a short or corrupt body never reads past the end. */
static inline bool sta1_validate(const uint8_t* buf, size_t len) {
  if (!buf || len < 16 || memcmp(buf, "STA1", 4) != 0 || buf[4] != 1 || buf[5] != 1) return false;
  uint16_t w = sta1_rd16(buf + 6), h = sta1_rd16(buf + 8), n = sta1_rd16(buf + 10);
  if (w == 0 || h == 0 || n < 2 || (uint32_t)w * h > 16384) return false;
  size_t off = 16;
  for (uint16_t i = 0; i < n; i++) {
    if (off + 16 > len) return false;
    uint16_t x = sta1_rd16(buf + off + 2), y = sta1_rd16(buf + off + 4);
    uint16_t rw = sta1_rd16(buf + off + 6), rh = sta1_rd16(buf + off + 8);
    uint32_t dl = sta1_rd32(buf + off + 12);
    if ((uint32_t)x + rw > w || (uint32_t)y + rh > h) return false;
    if (dl != (uint32_t)rw * rh * 3) return false;
    if (i == 0 && (x != 0 || y != 0 || rw != w || rh != h)) return false;
    off += 16 + dl;
    if (off > len) return false;
  }
  return off == len;
}
