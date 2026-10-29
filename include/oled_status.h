#pragma once
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Probes I2C bus at PIN_I2C_SDA and PIN_I2C_SCL for SSD1306 (0x3C or 0x3D)
// Returns true if an OLED display is detected and initialized, false otherwise.
bool oled_status_init();

// Check if OLED was detected and is currently active
bool oled_status_is_available();

// Displays the 6-digit pairing code prominently on the OLED
void oled_show_pairing(const char* pairingCode, const char* serverUrl, const char* ipStr);

// Displays Wi-Fi connection progress
void oled_show_wifi(const char* ssid, const char* statusMsg);

// Displays runtime status when paired and actively displaying content
void oled_show_status(const char* deviceName, int itemIdx, int totalItems, int rssi, const char* statusMsg);

// Clear the OLED display
void oled_clear();

#ifdef __cplusplus
}
#endif
