#include "device_profile.h"

#if defined(DEVICE_IS_RGB_LCD)
#include "lcd_driver.h"
#include "pins_crowpanel_43.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_arduino_version.h>

static Arduino_ESP32RGBPanel* s_bus = nullptr;
static Arduino_RGB_Display* s_lcd = nullptr;
static uint8_t s_backlight = 255;
static bool s_initialized = false;

bool lcd_driver_init() {
  if (s_initialized) return true;

  Serial.println("[LCD] Initializing CrowPanel 4.3\" RGB Parallel Panel (480x272)...");

  // Initialize Backlight PWM
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_LCD_BL, 5000, 8);
  ledcWrite(PIN_LCD_BL, s_backlight);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_LCD_BL, 0);
  ledcWrite(0, s_backlight);
#endif

  s_bus = new Arduino_ESP32RGBPanel(
    PIN_LCD_DE, PIN_LCD_VSYNC, PIN_LCD_HSYNC, PIN_LCD_PCLK,
    PIN_LCD_R0, PIN_LCD_R1, PIN_LCD_R2, PIN_LCD_R3, PIN_LCD_R4,
    PIN_LCD_G0, PIN_LCD_G1, PIN_LCD_G2, PIN_LCD_G3, PIN_LCD_G4, PIN_LCD_G5,
    PIN_LCD_B0, PIN_LCD_B1, PIN_LCD_B2, PIN_LCD_B3, PIN_LCD_B4,
    LCD_HSYNC_POLARITY, LCD_HSYNC_FRONT_PORCH, LCD_HSYNC_PULSE_WIDTH, LCD_HSYNC_BACK_PORCH,
    LCD_VSYNC_POLARITY, LCD_VSYNC_FRONT_PORCH, LCD_VSYNC_PULSE_WIDTH, LCD_VSYNC_BACK_PORCH,
    LCD_PCLK_ACTIVE_NEG, LCD_PIXEL_CLOCK_HZ, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 3840 /* bounce_buffer_size_px (480 * 8 lines, Elecrow official) */
  );

  s_lcd = new Arduino_RGB_Display(480, 272, s_bus, 0 /* rotation */, false /* auto_flush */);

  if (!s_lcd || !s_lcd->begin(LCD_PIXEL_CLOCK_HZ)) {
    Serial.println("[LCD] ERROR: Failed to initialize RGB LCD panel!");
    return false;
  }

  s_lcd->fillScreen(0x0000); // Black
  s_lcd->flush(true);
  s_initialized = true;
  Serial.println("[LCD] Panel initialized successfully.");
  return true;
}

void lcd_driver_set_backlight(uint8_t brightness) {
  s_backlight = brightness;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_LCD_BL, brightness);
#else
  ledcWrite(0, brightness);
#endif
}

uint8_t lcd_driver_get_backlight() {
  return s_backlight;
}

bool lcd_driver_draw_rgb565(const uint8_t* buffer, size_t length) {
  if (!s_initialized && !lcd_driver_init()) return false;
  if (!buffer) return false;

  size_t expected = 480 * 272 * 2;
  if (length < expected) {
    Serial.printf("[LCD] Buffer size mismatch: received %u, expected %u bytes\n", (unsigned)length, (unsigned)expected);
    return false;
  }

  uint16_t* fb = s_lcd->getFramebuffer();
  if (fb) {
    memcpy(fb, buffer, expected);
    s_lcd->flush(true);
    return true;
  }
  return false;
}

void lcd_driver_fill_screen(uint16_t color) {
  if (!s_initialized && !lcd_driver_init()) return;
  s_lcd->fillScreen(color);
  s_lcd->flush(true);
}

bool lcd_driver_draw_mono_buffer(const uint8_t* monoBuf, int width, int height, uint16_t fgColor, uint16_t bgColor) {
  if (!s_initialized && !lcd_driver_init()) return false;
  if (!monoBuf) return false;

  uint16_t* fb = s_lcd->getFramebuffer();
  if (!fb) return false;

  int drawW = (width < 480) ? width : 480;
  int drawH = (height < 272) ? height : 272;
  int stride = (width + 7) / 8;

  for (int y = 0; y < drawH; y++) {
    const uint8_t* srcRow = monoBuf + y * stride;
    uint16_t* dstRow = fb + y * 480;
    for (int x = 0; x < drawW; x++) {
      uint8_t byteVal = srcRow[x / 8];
      bool bit = (byteVal & (0x80 >> (x % 8))) != 0;
      // In 1bpp mono: bit 1 is white (background), bit 0 is black (foreground)
      dstRow[x] = bit ? bgColor : fgColor;
    }
    for (int x = drawW; x < 480; x++) {
      dstRow[x] = bgColor;
    }
  }

  // Clear any remaining rows if height < 272
  for (int y = drawH; y < 272; y++) {
    uint16_t* dstRow = fb + y * 480;
    for (int x = 0; x < 480; x++) {
      dstRow[x] = bgColor;
    }
  }

  s_lcd->flush(true);
  return true;
}

void lcd_driver_sleep() {
  lcd_driver_set_backlight(0);
}

void lcd_driver_wakeup() {
  lcd_driver_set_backlight(s_backlight > 0 ? s_backlight : 255);
}

#endif // DEVICE_IS_RGB_LCD
