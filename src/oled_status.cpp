#include "device_profile.h"

#if defined(DEVICE_IS_LED_STRIP)
#include "oled_status.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

static Adafruit_SSD1306* s_display = nullptr;
static bool s_available = false;
static uint8_t s_i2c_addr = 0x3C;

bool oled_status_init() {
#if defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL) && (PIN_I2C_SDA >= 0) && (PIN_I2C_SCL >= 0)
  Wire.setTimeOut(50);

  // Probe address 0x3C (primary 7-bit address corresponding to PCB marking 0x78)
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    s_i2c_addr = 0x3C;
    s_available = true;
  } else {
    // Try alternate address 0x3D
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
      s_i2c_addr = 0x3D;
      s_available = true;
    }
  }

  if (!s_available) {
    Serial.println("[OLED] No SSD1306 OLED detected on Grove I2C port (0x3C/0x3D). Running headless.");
    return false;
  }

  if (!s_display) {
    s_display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
  }

  if (!s_display->begin(SSD1306_SWITCHCAPVCC, s_i2c_addr)) {
    Serial.printf("[OLED] Failed to initialize SSD1306 at 0x%02X!\n", s_i2c_addr);
    delete s_display;
    s_display = nullptr;
    s_available = false;
    return false;
  }

  s_display->clearDisplay();
  s_display->setTextColor(SSD1306_WHITE);
  s_display->setTextSize(1);
  s_display->setCursor(18, 10);
  s_display->println("ScreenTinker");
  s_display->setCursor(18, 26);
  s_display->println("LED Controller");
  s_display->setCursor(28, 46);
  s_display->println("Starting...");
  s_display->display();

  Serial.printf("[OLED] SSD1306 OLED (128x64) detected and initialized at 0x%02X on Grove port!\n", s_i2c_addr);
  return true;
#else
  Serial.println("[OLED] Grove I2C pins not defined for this target.");
  return false;
#endif
}

bool oled_status_is_available() {
  return s_available && s_display != nullptr;
}

void oled_show_pairing(const char* pairingCode, const char* serverUrl, const char* ipStr) {
  if (!oled_status_is_available()) return;

  s_display->clearDisplay();
  s_display->setTextColor(SSD1306_WHITE);

  // Header banner
  s_display->setTextSize(1);
  s_display->setCursor(20, 2);
  s_display->println("SCREENTINKER");
  s_display->drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);

  // Pairing code label
  s_display->setCursor(26, 16);
  s_display->println("PAIRING PIN:");

  // Large 6-digit code
  s_display->setTextSize(2);
  if (pairingCode && strlen(pairingCode) == 6) {
    // Format as "123 456" centered
    char buf[12];
    snprintf(buf, sizeof(buf), "%.3s %.3s", pairingCode, pairingCode + 3);
    s_display->setCursor(16, 28);
    s_display->println(buf);
  } else if (pairingCode && strlen(pairingCode) > 0) {
    s_display->setCursor(22, 28);
    s_display->println(pairingCode);
  } else {
    s_display->setCursor(16, 28);
    s_display->println("WAITING...");
  }

  // Footer separator & IP / server
  s_display->drawFastHLine(0, 48, SCREEN_WIDTH, SSD1306_WHITE);
  s_display->setTextSize(1);
  s_display->setCursor(2, 52);
  if (ipStr && strlen(ipStr) > 0) {
    s_display->printf("IP: %s", ipStr);
  } else {
    s_display->println("Open ScreenTinker UI");
  }

  s_display->display();
}

void oled_show_wifi(const char* ssid, const char* statusMsg) {
  if (!oled_status_is_available()) return;

  s_display->clearDisplay();
  s_display->setTextColor(SSD1306_WHITE);

  s_display->setTextSize(1);
  s_display->setCursor(20, 4);
  s_display->println("ScreenTinker");
  s_display->drawFastHLine(0, 16, SCREEN_WIDTH, SSD1306_WHITE);

  s_display->setCursor(4, 24);
  s_display->println("Wi-Fi Connection:");

  s_display->setCursor(4, 36);
  if (ssid && strlen(ssid) > 0) {
    s_display->printf("%.18s", ssid);
  } else {
    s_display->println("No SSID");
  }

  s_display->setCursor(4, 50);
  s_display->printf("%s", statusMsg ? statusMsg : "Connecting...");

  s_display->display();
}

void oled_show_status(const char* deviceName, int itemIdx, int totalItems, int rssi, const char* statusMsg) {
  if (!oled_status_is_available()) return;

  s_display->clearDisplay();
  s_display->setTextColor(SSD1306_WHITE);

  // Top bar: Device Name & RSSI
  s_display->setTextSize(1);
  s_display->setCursor(2, 2);
  if (deviceName && strlen(deviceName) > 0) {
    s_display->printf("%.14s", deviceName);
  } else {
    s_display->print("LED Controller");
  }
  s_display->setCursor(95, 2);
  s_display->printf("%ddBm", rssi);

  s_display->drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);

  // Item / Slide info
  s_display->setCursor(4, 18);
  if (totalItems > 0) {
    s_display->printf("Slide %d of %d", itemIdx + 1, totalItems);
  } else {
    s_display->println("Streaming Active");
  }

  // Status message
  s_display->setCursor(4, 34);
  if (statusMsg && strlen(statusMsg) > 0) {
    s_display->println(statusMsg);
  } else {
    s_display->println("Syncing OK");
  }

  // Footer: IP / LED info
  s_display->drawFastHLine(0, 48, SCREEN_WIDTH, SSD1306_WHITE);
  s_display->setCursor(2, 53);
  s_display->println("ScreenTinker Online");

  s_display->display();
}

void oled_clear() {
  if (!oled_status_is_available()) return;
  s_display->clearDisplay();
  s_display->display();
}

#endif // DEVICE_IS_LED_STRIP
