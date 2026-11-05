#include "device_profile.h"

#if defined(DEVICE_IS_LED_STRIP)
#include "led_driver.h"
#include <Adafruit_NeoPixel.h>
#include <cmath>

static Adafruit_NeoPixel* s_strip = nullptr;
static uint16_t s_numLeds = 5;
static uint8_t s_brightness = 180; // Default ~70% to avoid excessive power draw
static neoPixelType s_pixelType = NEO_GRB + NEO_KHZ800;

/*
 * Power limiter. WS2812B/SK6812 draw up to ~20 mA per colour channel at full drive (60 mA for full
 * white) plus ~1 mA each idle. Before a frame is shown its current is estimated from the pixel
 * values at the configured brightness; if that is over the limit, THIS frame's brightness is scaled
 * down just enough to fit. Dark frames keep full brightness; a full-white frame is held back.
 * The estimate is deliberately conservative (real LEDs often draw a little less).
 */
static const uint32_t LED_MA_PER_CHANNEL = 20;
static const uint32_t LED_MA_IDLE = 1;
static uint32_t s_powerLimitMa = 0;       // 0 = no limit
static uint32_t s_lastEstimateMa = 0;     // after limiting: what the last frame should draw
static uint8_t s_lastAppliedBrightness = 0;
static void applySolidLimit(uint8_t r, uint8_t g, uint8_t b);

static LedMatrixConfig s_matrixCfg = {
  5, 1,
  MATRIX_ORIGIN_TOP_LEFT,
  MATRIX_AXIS_HORIZONTAL,
  MATRIX_WIRING_PROGRESSIVE
};

static neoPixelType parseColorOrder(const char* order) {
  if (!order) return NEO_GRB + NEO_KHZ800;
  if (strcasecmp(order, "RGB") == 0) return NEO_RGB + NEO_KHZ800;
  if (strcasecmp(order, "BGR") == 0) return NEO_BGR + NEO_KHZ800;
  if (strcasecmp(order, "BRG") == 0) return NEO_BRG + NEO_KHZ800;
  if (strcasecmp(order, "RBG") == 0) return NEO_RBG + NEO_KHZ800;
  if (strcasecmp(order, "GBR") == 0) return NEO_GBR + NEO_KHZ800;
  return NEO_GRB + NEO_KHZ800;
}

bool led_driver_init(uint16_t numLeds) {
#ifdef PIN_LED_DATA
  s_numLeds = (numLeds > 0) ? numLeds : 5;
  if (!s_strip) {
    s_strip = new Adafruit_NeoPixel(s_numLeds, PIN_LED_DATA, s_pixelType);
  } else {
    s_strip->updateLength(s_numLeds);
  }

  s_strip->begin();
  s_strip->setBrightness(s_brightness);
  s_strip->clear();
  s_strip->show();

  Serial.printf("[LED] Driver initialized: %d LEDs on Pin %d (Brightness: %d/255)\n",
                s_numLeds, (int)PIN_LED_DATA, s_brightness);
  return true;
#else
  Serial.println("[LED] Warning: PIN_LED_DATA not defined for this target!");
  return false;
#endif
}

void led_driver_set_brightness(uint8_t brightness) {
  s_brightness = brightness;
  if (s_strip) {
    s_strip->setBrightness(brightness);
    s_strip->show();
  }
}

uint8_t led_driver_get_brightness() {
  return s_brightness;
}

void led_driver_set_count(uint16_t numLeds) {
  if (numLeds == 0 || numLeds == s_numLeds || !s_strip) return;
  s_numLeds = numLeds;
  s_strip->updateLength(s_numLeds);
  s_strip->clear();
  s_strip->show();
  Serial.printf("[LED] Count reconfigured to %d LEDs\n", s_numLeds);
}

uint16_t led_driver_get_count() {
  return s_numLeds;
}

void led_driver_set_matrix_config(uint16_t width, uint16_t height, MatrixWiring wiring, MatrixAxis axis, MatrixOrigin origin) {
  s_matrixCfg.width = (width > 0) ? width : 1;
  s_matrixCfg.height = (height > 0) ? height : 1;
  s_matrixCfg.wiring = wiring;
  s_matrixCfg.axis = axis;
  s_matrixCfg.origin = origin;

  uint16_t total = s_matrixCfg.width * s_matrixCfg.height;
  if (total != s_numLeds && total > 0) {
    led_driver_set_count(total);
  }
  Serial.printf("[LED] Matrix topology configured: %dx%d (%s, %s, origin=%d)\n",
                s_matrixCfg.width, s_matrixCfg.height,
                s_matrixCfg.wiring == MATRIX_WIRING_SERPENTINE ? "Serpentine" : "Progressive",
                s_matrixCfg.axis == MATRIX_AXIS_VERTICAL ? "Vertical" : "Horizontal",
                (int)s_matrixCfg.origin);
}

LedMatrixConfig led_driver_get_matrix_config() {
  return s_matrixCfg;
}

void led_driver_show_solid(uint8_t r, uint8_t g, uint8_t b) {
  if (!s_strip) return;
  applySolidLimit(r, g, b);
  for (uint16_t i = 0; i < s_numLeds; i++) {
    s_strip->setPixelColor(i, r, g, b);
  }
  s_strip->show();
}

void led_driver_clear() {
  if (!s_strip) return;
  s_strip->clear();
  s_strip->show();
}

void led_driver_set_power_limit(uint32_t milliamps) { s_powerLimitMa = milliamps; }
uint32_t led_driver_get_power_limit() { return s_powerLimitMa; }
uint32_t led_driver_last_estimate_ma() { return s_lastEstimateMa; }
uint8_t led_driver_last_applied_brightness() { return s_lastAppliedBrightness; }

/* The brightness to show this frame at, given the limit, and the current it should then draw. */
static uint8_t limitedBrightnessForSum(uint64_t channelSum, size_t pixelCount, uint32_t* estimateOut) {
  const uint32_t idle = (uint32_t)pixelCount * LED_MA_IDLE;
  // mA at the configured brightness: idle + sum/255 * 20 mA, scaled by brightness/255.
  const uint64_t drive = (channelSum * LED_MA_PER_CHANNEL * s_brightness) / (255ULL * 255ULL);
  uint8_t b = s_brightness;
  uint64_t est = idle + drive;
  if (s_powerLimitMa > 0 && est > s_powerLimitMa && drive > 0) {
    const uint64_t room = s_powerLimitMa > idle ? s_powerLimitMa - idle : 0;
    uint64_t scaled = (uint64_t)s_brightness * room / drive;
    b = (uint8_t)(scaled < 1 ? 1 : (scaled > s_brightness ? s_brightness : scaled));
    est = idle + (drive * b) / (s_brightness ? s_brightness : 1);
  }
  *estimateOut = (uint32_t)est;
  return b;
}

static uint8_t limitedBrightness(const uint8_t* rgbData, size_t pixelCount, uint32_t* estimateOut) {
  uint64_t channelSum = 0;
  for (size_t i = 0; i < pixelCount * 3; i++) channelSum += rgbData[i];
  return limitedBrightnessForSum(channelSum, pixelCount, estimateOut);
}

/* Solid colours (LED test, status pulses) go through the limiter too: led_test is full white. */
static void applySolidLimit(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t est = 0;
  uint8_t fb = limitedBrightnessForSum((uint64_t)(r + g + b) * s_numLeds, s_numLeds, &est);
  s_strip->setBrightness(fb);
  s_lastEstimateMa = est;
  s_lastAppliedBrightness = fb;
}

bool led_driver_show_rgb(const uint8_t* rgbData, size_t dataLen, const char* colorOrder) {
  if (!s_strip || !rgbData) return false;

  size_t pixelCount = dataLen / 3;
  if (pixelCount == 0) return false;

  // Dynamically adapt length if server sent more or fewer pixels than currently configured
  if (pixelCount != s_numLeds) {
    Serial.printf("[LED] Dynamic resize from %d to %d LEDs based on payload size (%u bytes)\n",
                  s_numLeds, (int)pixelCount, (unsigned int)dataLen);
    led_driver_set_count(pixelCount);
  }

  // Update color order if needed
  neoPixelType expectedType = parseColorOrder(colorOrder);
  if (expectedType != s_pixelType) {
    s_pixelType = expectedType;
    s_strip->updateType(s_pixelType);
  }

  // Power limit: set this frame's brightness BEFORE the pixels (NeoPixel rescales stored pixels
  // lossily if brightness changes after setPixelColor).
  uint32_t est = 0;
  uint8_t frameBrightness = limitedBrightness(rgbData, pixelCount, &est);
  s_strip->setBrightness(frameBrightness);
  s_lastEstimateMa = est;
  s_lastAppliedBrightness = frameBrightness;

  // Check if active 2D matrix mapping applies
  if (s_matrixCfg.width > 1 && s_matrixCfg.height > 1 &&
      ((size_t)s_matrixCfg.width * s_matrixCfg.height == pixelCount)) {
    // 2D Matrix coordinate mapping: (x, y) Cartesian -> sequential physical LED index
    for (uint16_t y = 0; y < s_matrixCfg.height; y++) {
      for (uint16_t x = 0; x < s_matrixCfg.width; x++) {
        size_t srcOffset = ((size_t)y * s_matrixCfg.width + x) * 3;
        uint16_t physicalIndex = led_matrix_xy_to_index(x, y, &s_matrixCfg);
        if (physicalIndex < s_numLeds) {
          s_strip->setPixelColor(physicalIndex,
                                rgbData[srcOffset + 0],
                                rgbData[srcOffset + 1],
                                rgbData[srcOffset + 2]);
        }
      }
    }
  } else {
    // 1D Linear strip mapping: sequential 1:1
    for (size_t i = 0; i < pixelCount; i++) {
      s_strip->setPixelColor(i,
                            rgbData[i * 3 + 0],
                            rgbData[i * 3 + 1],
                            rgbData[i * 3 + 2]);
    }
  }

  s_strip->show();
  return true;
}

void led_driver_pulse_status(float phase, uint8_t baseR, uint8_t baseG, uint8_t baseB) {
  if (!s_strip) return;

  // Sine ease for smooth breathing: sin(phase * 2 * PI) from -1..1 -> 0..1
  float factor = 0.5f * (1.0f + sinf(phase * 6.2831853f));
  // Scale between 5% and 80% to avoid harsh blinding
  float scale = 0.05f + 0.75f * factor;

  uint8_t r = (uint8_t)(baseR * scale);
  uint8_t g = (uint8_t)(baseG * scale);
  uint8_t b = (uint8_t)(baseB * scale);
  applySolidLimit(r, g, b);

  for (uint16_t i = 0; i < s_numLeds; i++) {
    s_strip->setPixelColor(i, r, g, b);
  }
  s_strip->show();
}

#endif // DEVICE_IS_LED_STRIP
