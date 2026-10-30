#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the RGB LCD panel and backlight PWM. */
bool lcd_driver_init();

/** Set LCD backlight brightness (0 = off, 255 = maximum brightness). */
void lcd_driver_set_backlight(uint8_t brightness);

/** Get current LCD backlight brightness level (0 - 255). */
uint8_t lcd_driver_get_backlight();

/** Blit a full-frame 16-bit RGB565 buffer (480x272) to the LCD. */
bool lcd_driver_draw_rgb565(const uint8_t* buffer, size_t length);

/** Fill the entire screen with a single RGB565 color. */
void lcd_driver_fill_screen(uint16_t color);

/** Blit a 1-bit monochrome buffer (MSB first, e.g. from setup_screen) onto the LCD in RGB565. */
bool lcd_driver_draw_mono_buffer(const uint8_t* monoBuf, int width, int height, uint16_t fgColor, uint16_t bgColor);

/** Put the display into low-power sleep (turns off backlight and clocks). */
void lcd_driver_sleep();

/** Wake the display from sleep and restore backlight. */
void lcd_driver_wakeup();

#ifdef __cplusplus
}
#endif
