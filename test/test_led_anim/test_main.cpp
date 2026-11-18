#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "../include/sta1_format.h"
#include "../include/led_power_limit.h"

static void wr16(std::vector<uint8_t>& v, uint16_t val) {
  v.push_back((uint8_t)(val & 0xFF));
  v.push_back((uint8_t)((val >> 8) & 0xFF));
}

static void wr32(std::vector<uint8_t>& v, uint32_t val) {
  v.push_back((uint8_t)(val & 0xFF));
  v.push_back((uint8_t)((val >> 8) & 0xFF));
  v.push_back((uint8_t)((val >> 16) & 0xFF));
  v.push_back((uint8_t)((val >> 24) & 0xFF));
}

static std::vector<uint8_t> make_sta1_2frame(uint16_t w, uint16_t h, uint16_t f1_x, uint16_t f1_y, uint16_t f1_w, uint16_t f1_h) {
  std::vector<uint8_t> b;
  b.insert(b.end(), {'S', 'T', 'A', '1', 1, 1});
  wr16(b, w);
  wr16(b, h);
  wr16(b, 2); // frames
  wr16(b, 0); // loops
  wr16(b, 0); // reserved

  // Frame 0: full screen
  wr16(b, 100); wr16(b, 0); wr16(b, 0); wr16(b, w); wr16(b, h); wr16(b, 0);
  uint32_t dl0 = (uint32_t)w * h * 3;
  wr32(b, dl0);
  b.resize(b.size() + dl0, 0x11);

  // Frame 1: dirty rect
  wr16(b, 50); wr16(b, f1_x); wr16(b, f1_y); wr16(b, f1_w); wr16(b, f1_h); wr16(b, 0);
  uint32_t dl1 = (uint32_t)f1_w * f1_h * 3;
  wr32(b, dl1);
  b.resize(b.size() + dl1, 0x22);
  return b;
}

static void test_sta1_validation() {
  std::cout << "[Test] Verifying STA1 animation validation...\n";

  // Valid 4x4, 2-frame animation with 2x2 dirty rect
  auto valid = make_sta1_2frame(4, 4, 1, 1, 2, 2);
  assert(sta1_validate(valid.data(), valid.size()));

  // Null / short buffer
  assert(!sta1_validate(nullptr, 0));
  assert(!sta1_validate(valid.data(), 15));
  assert(!sta1_validate(valid.data(), valid.size() - 1));

  // Trailing garbage
  auto trailing = valid;
  trailing.push_back(0);
  assert(!sta1_validate(trailing.data(), trailing.size()));

  // Bad magic / version / format
  auto bad = valid;
  bad[0] = 'X';
  assert(!sta1_validate(bad.data(), bad.size()));
  bad = valid; bad[4] = 2;
  assert(!sta1_validate(bad.data(), bad.size()));
  bad = valid; bad[5] = 2;
  assert(!sta1_validate(bad.data(), bad.size()));

  // Frame 0 must be full screen
  bad = valid;
  bad[18] = 1; // f0.x = 1
  assert(!sta1_validate(bad.data(), bad.size()));

  // Frame 1 out of bounds (x + rw > w)
  auto oob = make_sta1_2frame(4, 4, 3, 0, 2, 2);
  assert(!sta1_validate(oob.data(), oob.size()));

  // Dimension limit (> 16384 pixels)
  std::vector<uint8_t> tooBig = {'S', 'T', 'A', '1', 1, 1};
  wr16(tooBig, 200); wr16(tooBig, 100); wr16(tooBig, 2); wr16(tooBig, 0); wr16(tooBig, 0);
  assert(!sta1_validate(tooBig.data(), tooBig.size()));

  std::cout << "  ✓ STA1 validation tests passed!\n";
}

static void test_power_limiter() {
  std::cout << "[Test] Verifying LED power limiter math...\n";

  uint32_t est = 0;

  // 1. No limit (0): keeps full configured brightness
  // 100 LEDs, full white (255*3 per pixel): idle=100 mA, drive at 255 = 6000 mA -> 6100 mA
  uint64_t fullWhite100 = 100ULL * 255ULL * 3ULL;
  uint8_t b = led_power_limit_brightness(fullWhite100, 100, 255, 0, &est);
  assert(b == 255);
  assert(est == 6100);

  // 2. Dark frame within 3000 mA limit: keeps full configured brightness (180)
  b = led_power_limit_brightness(100ULL * 10ULL * 3ULL, 100, 180, 3000, &est);
  assert(b == 180);
  assert(est <= 3000);

  // 3. Full white 100 LEDs at brightness 255 with 3000 mA limit: throttled to <= 3000 mA
  b = led_power_limit_brightness(fullWhite100, 100, 255, 3000, &est);
  assert(b < 255 && b >= 1);
  assert(est <= 3000);

  // 4. Tight limit below idle current: clamps to minimum brightness 1 without underflow
  b = led_power_limit_brightness(fullWhite100, 100, 200, 50, &est);
  assert(b == 1);

  // 5. Configured brightness 0: stays 0, estimate = idle
  b = led_power_limit_brightness(fullWhite100, 100, 0, 3000, &est);
  assert(b == 0);
  assert(est == 100);

  std::cout << "  ✓ Power limiter tests passed!\n";
}

int main() {
  test_sta1_validation();
  test_power_limiter();
  return 0;
}
