#pragma once
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the LED strip driver on PIN_LED_DATA
bool led_driver_init(uint16_t numLeds = 5);

// Set overall strip brightness (0-255)
void led_driver_set_brightness(uint8_t brightness);

// Reconfigure the number of LEDs if changed by server profile
void led_driver_set_count(uint16_t numLeds);

// Get current configured LED count
uint16_t led_driver_get_count();

// Set all LEDs to a single color
void led_driver_show_solid(uint8_t r, uint8_t g, uint8_t b);

// Clear all LEDs (turn off)
void led_driver_clear();

// Render raw RGB888 byte stream (length = numPixels * 3)
// Supported color orders: "GRB", "RGB", "BRG", "RBG", "BGR", "GBR"
bool led_driver_show_rgb(const uint8_t* rgbData, size_t dataLen, const char* colorOrder = "GRB");

// Animated breathing / status effect during pairing/onboarding
// phase: 0.0 to 1.0 (repeating cycle)
void led_driver_pulse_status(float phase, uint8_t baseR = 255, uint8_t baseG = 140, uint8_t baseB = 0);

#ifdef __cplusplus
}
#endif
