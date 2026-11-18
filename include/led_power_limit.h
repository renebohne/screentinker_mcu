#pragma once
/*
 * Pure LED power-limiter math.
 * Shared by src/led_driver.cpp and native unit tests.
 */
#include <stddef.h>
#include <stdint.h>

static const uint32_t LED_MA_PER_CHANNEL = 20;
static const uint32_t LED_MA_IDLE = 1;

/* Compute the brightness to show this frame at, given the limit, and the current it should then draw. */
static inline uint8_t led_power_limit_brightness(uint64_t channelSum, size_t pixelCount,
                                                 uint8_t configuredBrightness, uint32_t powerLimitMa,
                                                 uint32_t* estimateOut) {
  const uint32_t idle = (uint32_t)pixelCount * LED_MA_IDLE;
  // mA at the configured brightness: idle + sum/255 * 20 mA, scaled by brightness/255.
  const uint64_t drive = (channelSum * LED_MA_PER_CHANNEL * configuredBrightness) / (255ULL * 255ULL);
  uint8_t b = configuredBrightness;
  uint64_t est = idle + drive;
  if (powerLimitMa > 0 && est > powerLimitMa && drive > 0) {
    const uint64_t room = powerLimitMa > idle ? powerLimitMa - idle : 0;
    uint64_t scaled = (uint64_t)configuredBrightness * room / drive;
    b = (uint8_t)(scaled < 1 ? 1 : (scaled > configuredBrightness ? configuredBrightness : scaled));
    est = idle + (drive * b) / (configuredBrightness ? configuredBrightness : 1);
  }
  if (estimateOut) *estimateOut = (uint32_t)est;
  return b;
}
