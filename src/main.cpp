#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

extern "C" {
#include "epaper_panel.h"
}

#include "pins_sticky.h"
#include "config.h"
#include "setup_screen.h"

// ─── Persistent Storage (NVS) ────────────────────────────────────────────────
static Preferences s_prefs;

static String g_wifiSsid = "";
static String g_wifiPass = "";
static String g_serverUrl = "";
static String g_deviceId = "";
static String g_deviceToken = "";

// ─── Hardware Handles & State ────────────────────────────────────────────────
static seeed_epaper_panel_handle_t s_panel = NULL;
static spi_device_handle_t s_spi_device = NULL;
static bool s_epdInitialized = false;

static int s_currentItemIndex = 0;
static int s_totalItems = 0;
static uint32_t s_lastSyncMillis = 0;
static uint32_t s_syncIntervalMs = DEFAULT_SLEEP_SEC * 1000;

// ─── PSRAM Frame Buffers & Cache ─────────────────────────────────────────────
static uint8_t* s_rawBuffer = nullptr;       // 48,000 bytes (Download buffer)
static uint8_t* s_rotatedBuffer = nullptr;   // 48,000 bytes (Rotated E-Paper ready buffer)

struct CacheItem {
  int itemIndex;
  String etag;
  uint8_t* buffer;  // 48,000 bytes in PSRAM
  bool valid;
};

static CacheItem s_cache[MAX_CACHED_ITEMS];

static void initPsramBuffers() {
  if (psramFound()) {
    Serial.printf("[PSRAM] PSRAM found! Total: %u KB, Free: %u KB\n",
                  ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);

    s_rawBuffer = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_rotatedBuffer = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  } else {
    Serial.println("[PSRAM] WARNING: PSRAM not found, falling back to internal heap!");
    s_rawBuffer = (uint8_t*)malloc(EPD_BUFFER_SIZE);
    s_rotatedBuffer = (uint8_t*)malloc(EPD_BUFFER_SIZE);
  }

  if (!s_rawBuffer || !s_rotatedBuffer) {
    Serial.println("[PSRAM] ERROR: Failed to allocate working frame buffers!");
  } else {
    Serial.printf("[PSRAM] Working buffers allocated (2x %u bytes).\n", EPD_BUFFER_SIZE);
  }

  // Initialize cache entry pointers
  for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
    s_cache[i].itemIndex = -1;
    s_cache[i].etag = "";
    s_cache[i].buffer = nullptr;
    s_cache[i].valid = false;
  }
}

static uint8_t* getOrAllocCacheBuffer(int slot) {
  if (slot < 0 || slot >= MAX_CACHED_ITEMS) return nullptr;
  if (s_cache[slot].buffer == nullptr) {
    if (psramFound()) {
      s_cache[slot].buffer = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
      s_cache[slot].buffer = (uint8_t*)malloc(EPD_BUFFER_SIZE);
    }
  }
  return s_cache[slot].buffer;
}

static int findCacheSlot(int itemIndex) {
  for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
    if (s_cache[i].valid && s_cache[i].itemIndex == itemIndex) {
      return i;
    }
  }
  return -1;
}

static int findFreeOrLruSlot() {
  for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
    if (!s_cache[i].valid) return i;
  }
  return 0; // Overwrite first slot if full
}

static bool cacheGet(int itemIndex, uint8_t* outRotatedBuffer) {
  int slot = findCacheSlot(itemIndex);
  if (slot >= 0 && s_cache[slot].valid && s_cache[slot].buffer != nullptr) {
    memcpy(outRotatedBuffer, s_cache[slot].buffer, EPD_BUFFER_SIZE);
    return true;
  }
  return false;
}

static String cacheGetEtag(int itemIndex) {
  int slot = findCacheSlot(itemIndex);
  if (slot >= 0 && s_cache[slot].valid) {
    return s_cache[slot].etag;
  }
  return "";
}

static void cachePut(int itemIndex, const String& etag, const uint8_t* rotatedFrame) {
  int slot = findCacheSlot(itemIndex);
  if (slot < 0) {
    slot = findFreeOrLruSlot();
  }

  uint8_t* targetBuf = getOrAllocCacheBuffer(slot);
  if (targetBuf) {
    memcpy(targetBuf, rotatedFrame, EPD_BUFFER_SIZE);
    s_cache[slot].itemIndex = itemIndex;
    s_cache[slot].etag = etag;
    s_cache[slot].valid = true;
    Serial.printf("[Cache] Stored item %d (ETag: %s) in slot %d\n", itemIndex, etag.c_str(), slot);
  } else {
    Serial.printf("[Cache] Failed to allocate memory for cache slot %d!\n", slot);
  }
}

static void cacheClear() {
  for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
    s_cache[i].valid = false;
    s_cache[i].itemIndex = -1;
    s_cache[i].etag = "";
  }
  Serial.println("[Cache] Playlist cache cleared.");
}

static int cacheValidCount() {
  int count = 0;
  for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
    if (s_cache[i].valid) count++;
  }
  return count;
}

static int getNextCachedItemIndex(int currentIndex) {
  int lowest = -1;
  int nextHighest = -1;
  for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
    if (s_cache[i].valid) {
      int idx = s_cache[i].itemIndex;
      if (lowest == -1 || idx < lowest) lowest = idx;
      if (idx > currentIndex) {
        if (nextHighest == -1 || idx < nextHighest) nextHighest = idx;
      }
    }
  }
  if (nextHighest != -1) return nextHighest;
  return (lowest != -1) ? lowest : currentIndex;
}

// ─── Board Power & GPIO Unhold ────────────────────────────────────────────────
void initBoardPowerAndPins() {
  gpio_deep_sleep_hold_dis();

  constexpr int kHeldPins[] = {
    PIN_PWR_HOLD,
    PIN_PWR_LOCK,
    PIN_BTN_OK,
    PIN_BTN_UP,
    PIN_BTN_DOWN,
    PIN_EPD_PWR_EN,
    PIN_SD_CS,
    PIN_SD_EN,
    PIN_SD_DETECT,
    PIN_TOUCH_EN,
    PIN_TOUCH_RST,
    PIN_BUZZER,
  };
  for (int pin : kHeldPins) {
    gpio_hold_dis((gpio_num_t)pin);
  }

  // Latch board power
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  pinMode(PIN_PWR_LOCK, OUTPUT);
  digitalWrite(PIN_PWR_LOCK, HIGH);

  // Release shared SPI bus by idling SD card pins (without altering SD content)
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_SD_EN, OUTPUT);
  digitalWrite(PIN_SD_EN, HIGH);

  // Power EPD display boost converter
  pinMode(PIN_EPD_PWR_EN, OUTPUT);
  digitalWrite(PIN_EPD_PWR_EN, HIGH);

  // Buttons (Active LOW with internal pullup)
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);

  delay(50);
}

// ─── Display Driver (SSD1677) ────────────────────────────────────────────────
static bool initStickyEpaper() {
  if (s_epdInitialized && s_panel != NULL) return true;

  spi_bus_config_t bus_config = {};
  bus_config.mosi_io_num = PIN_EPD_MOSI;
  bus_config.miso_io_num = PIN_EPD_MISO;
  bus_config.sclk_io_num = PIN_EPD_SCK;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.data4_io_num = -1;
  bus_config.data5_io_num = -1;
  bus_config.data6_io_num = -1;
  bus_config.data7_io_num = -1;
  bus_config.max_transfer_sz = 800 * 480 / 8;

  esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[EPD] spi_bus_initialize error: %s\n", esp_err_to_name(err));
    return false;
  }

  spi_device_interface_config_t device_config = {};
  device_config.clock_speed_hz = 10 * 1000 * 1000;
  device_config.mode = 0;
  device_config.spics_io_num = PIN_EPD_CS;
  device_config.queue_size = 1;

  err = spi_bus_add_device(SPI2_HOST, &device_config, &s_spi_device);
  if (err != ESP_OK) {
    Serial.printf("[EPD] spi_bus_add_device error: %s\n", esp_err_to_name(err));
    return false;
  }

  gpio_install_isr_service(0);

  seeed_epaper_panel_config_t panel_config = {};
  panel_config.spi_handle = s_spi_device;
  panel_config.pin_dc = (gpio_num_t)PIN_EPD_DC;
  panel_config.pin_rst = (gpio_num_t)PIN_EPD_RST;
  panel_config.pin_busy = (gpio_num_t)PIN_EPD_BUSY;
  panel_config.pin_enable = GPIO_NUM_NC;
  panel_config.busy_timeout_ms = 10000;
  panel_config.reset_low_ms = 10;
  panel_config.reset_high_ms = 10;
  panel_config.busy_level = 1;
  panel_config.enable_level = 1;
  panel_config.mirror_x = true;

  err = seeed_epaper_new_panel(SEEED_EPAPER_PANEL_SSD1677, &panel_config, &s_panel);
  if (err != ESP_OK) {
    Serial.printf("[EPD] seeed_epaper_new_panel error: %s\n", esp_err_to_name(err));
    return false;
  }

  s_epdInitialized = true;
  Serial.println("[EPD] SSD1677 display panel initialized.");
  return true;
}

static void rotate_mono_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height) {
  const size_t stride = width / 8U;
  for (uint16_t y = 0; y < height; ++y) {
    const size_t src_row = y * stride;
    const size_t dst_row = (height - 1U - y) * stride;
    for (size_t x = 0; x < stride; ++x) {
      uint8_t b = source[src_row + (stride - 1U - x)];
      b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
      b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
      b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
      destination[dst_row + x] = b;
    }
  }
}

static bool renderRotatedBuffer(const uint8_t* rotatedBuf) {
  if (!initStickyEpaper()) return false;

  const seeed_epaper_area_t full_screen = {0, 0, 800, 480};
  Serial.println("[EPD] Refreshing E-Paper display...");
  uint32_t refStart = millis();

  esp_err_t refErr = seeed_epaper_panel_refresh_area(
    s_panel,
    &full_screen,
    rotatedBuf,
    100,
    SEEED_EPAPER_PIXEL_FORMAT_MONO1_MSB,
    SEEED_EPAPER_REFRESH_FULL
  );

  uint32_t refElapsed = millis() - refStart;
  Serial.printf("[EPD] Refresh completed in %u ms (result: %s)\n", refElapsed, esp_err_to_name(refErr));
  return refErr == ESP_OK;
}

static Language s_currentLang = LANG_EN;

void showOnboardingScreen(Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.printf("[Display] Rendering Onboarding Instructions on E-Paper (%s, 180 deg rotated)...\n",
                (lang == LANG_DE ? "Deutsch" : "English"));
  renderOnboardingScreen(s_rawBuffer, lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, 800, 480);
  renderRotatedBuffer(s_rotatedBuffer);
}

void showNoWifiScreen(Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.printf("[Display] Rendering No Wi-Fi Screen on E-Paper (%s, 180 deg rotated)...\n",
                (lang == LANG_DE ? "Deutsch" : "English"));
  renderNoWifiScreen(s_rawBuffer, g_wifiSsid.c_str(), lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, 800, 480);
  renderRotatedBuffer(s_rotatedBuffer);
}

static String s_activePairingCode = "";
static String s_activePairingDeviceId = "";
static String s_activeClaimSecret = "";

void showPairingCodeScreen(const char* code, Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.printf("[Display] Rendering Pairing Code Screen (%s, Code: %s, 180 deg rotated)...\n",
                (lang == LANG_DE ? "Deutsch" : "English"), code ? code : "");
  renderPairingCodeScreen(s_rawBuffer, code, lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, 800, 480);
  renderRotatedBuffer(s_rotatedBuffer);
}

bool registerAndStartPairing();
bool checkPairingStatus();

// ─── NVS Configuration Management ─────────────────────────────────────────────
void loadConfigFromNVS() {
  s_prefs.begin("screentinker", false);
  g_wifiSsid    = s_prefs.getString("wifi_ssid", WIFI_SSID);
  g_wifiPass    = s_prefs.getString("wifi_pass", WIFI_PASSWORD);
  g_serverUrl   = s_prefs.getString("server_url", SCREENTINKER_HOST);
  g_deviceId    = s_prefs.getString("device_id", DEVICE_ID);
  g_deviceToken = s_prefs.getString("device_token", DEVICE_TOKEN);
  s_prefs.end();

  Serial.println("[Config] Loaded configuration from NVS/Defaults:");
  Serial.printf("  SSID:       %s\n", g_wifiSsid.c_str());
  Serial.printf("  Server:     %s\n", g_serverUrl.c_str());
  Serial.printf("  Device ID:  %s\n", g_deviceId.c_str());
  Serial.printf("  Token set:  %s\n", g_deviceToken.length() > 0 ? "YES" : "NO");
}

void saveConfigToNVS(const String& ssid, const String& pass, const String& server, const String& devId, const String& token) {
  s_prefs.begin("screentinker", false);
  s_prefs.putString("wifi_ssid", ssid);
  s_prefs.putString("wifi_pass", pass);
  s_prefs.putString("server_url", server);
  s_prefs.putString("device_id", devId);
  s_prefs.putString("device_token", token);
  s_prefs.end();

  g_wifiSsid = ssid;
  g_wifiPass = pass;
  g_serverUrl = server;
  g_deviceId = devId;
  g_deviceToken = token;
  cacheClear();
  Serial.printf("[Config] Configuration saved: SSID='%s', Server='%s', DeviceID='%s'\n",
                g_wifiSsid.c_str(), g_serverUrl.c_str(), g_deviceId.c_str());
}

void factoryResetNVS() {
  s_prefs.begin("screentinker", false);
  s_prefs.clear();
  s_prefs.end();
  cacheClear();
  Serial.println("[Config] NVS wiped! Resetting device in 1 second...");
  delay(1000);
  ESP.restart();
}

// ─── Network & Power Management ───────────────────────────────────────────────
bool connectWiFi(uint32_t timeoutMs = 20000) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (g_wifiSsid.length() == 0 || g_wifiSsid == "Your-WiFi-SSID") {
    Serial.println("[WiFi] No valid SSID configured. Use Serial / Web-Flasher to configure.");
    return false;
  }

  Serial.printf("[WiFi] Connecting to '%s'...", g_wifiSsid.c_str());
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect();
  delay(100);

  if (g_wifiPass.length() > 0) {
    WiFi.begin(g_wifiSsid.c_str(), g_wifiPass.c_str());
  } else {
    WiFi.begin(g_wifiSsid.c_str());
  }

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > timeoutMs) {
      Serial.printf("\n[WiFi] Connection timeout (Status: %d)\n", WiFi.status());
      return false;
    }
  }

  Serial.printf("\n[WiFi] Connected! IP: %s (RSSI: %d dBm)\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

void disconnectWiFiIfPowerSave() {
  if (ENABLE_WIFI_POWER_SAVE) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi] Power save: Wi-Fi radio powered off.");
  }
}

// ─── Automatic 6-Digit Pairing Flow ───────────────────────────────────────────
bool registerAndStartPairing() {
  if (!connectWiFi()) return false;

  String url = g_serverUrl + "/api/embedded/pair/register";
  Serial.printf("[Pairing] Requesting pairing code from server at %s...\n", url.c_str());

  HTTPClient http;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["screen_profile"] = "seeed-reterminal-sticky";
  doc["screen_width"] = 800;
  doc["screen_height"] = 480;

  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);
  if (httpCode == 200) {
    String resp = http.getString();
    JsonDocument respDoc;
    deserializeJson(respDoc, resp);
    s_activePairingDeviceId = respDoc["device_id"].as<String>();
    s_activePairingCode     = respDoc["pairing_code"].as<String>();
    s_activeClaimSecret     = respDoc["claim_secret"].as<String>();
    Serial.printf("[Pairing] Successfully registered with server!\n");
    Serial.printf("  Device ID:    %s\n", s_activePairingDeviceId.c_str());
    Serial.printf("  Pairing Code: %s\n", s_activePairingCode.c_str());
    showPairingCodeScreen(s_activePairingCode.c_str(), s_currentLang);
    return true;
  } else {
    Serial.printf("[Pairing] Registration failed with HTTP %d\n", httpCode);
    return false;
  }
}

bool checkPairingStatus() {
  if (s_activePairingDeviceId.length() == 0 || s_activeClaimSecret.length() == 0) return false;
  if (!connectWiFi()) return false;

  String url = g_serverUrl + "/api/embedded/pair/status?device_id=" + s_activePairingDeviceId;
  HTTPClient http;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Authorization", "Bearer " + s_activeClaimSecret);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String resp = http.getString();
    JsonDocument doc;
    deserializeJson(doc, resp);
    if (doc["paired"].as<bool>() == true) {
      String newDevId = doc["device_id"].as<String>();
      String newToken = doc["device_token"].as<String>();
      Serial.printf("\n🎉 [Pairing] SUCCESS! Device claimed by user in ScreenTinker UI!\n");
      Serial.printf("  Device ID:    %s\n", newDevId.c_str());
      Serial.printf("  Device Token: %s\n", newToken.c_str());

      saveConfigToNVS(g_wifiSsid, g_wifiPass, g_serverUrl, newDevId, newToken);
      s_activePairingCode = "";
      s_activePairingDeviceId = "";
      s_activeClaimSecret = "";
      return true;
    }
  }
  return false;
}

// ─── Fetch & Render (with PSRAM Caching) ──────────────────────────────────────
void fetchAndRender(bool forceRefresh = false, int requestedItemIndex = -1) {
  int targetIndex = (requestedItemIndex >= 0) ? requestedItemIndex : s_currentItemIndex;

  // 1. Check if frame is available in PSRAM cache for instant display
  if (!forceRefresh && cacheGet(targetIndex, s_rotatedBuffer)) {
    Serial.printf("[Sync] Item %d found in PSRAM cache -> Instant render!\n", targetIndex);
    s_currentItemIndex = targetIndex;
    renderRotatedBuffer(s_rotatedBuffer);
  }

  // 2. Fetch or re-validate from server over Wi-Fi
  if (!connectWiFi()) {
    int validCount = cacheValidCount();
    if (validCount == 0) {
      Serial.println("[Sync] Wi-Fi offline and cache is empty -> Showing No Wi-Fi screen.");
      showNoWifiScreen(s_currentLang);
    } else {
      int offlineTarget = (requestedItemIndex >= 0) ? requestedItemIndex : getNextCachedItemIndex(s_currentItemIndex);
      if (cacheGet(offlineTarget, s_rotatedBuffer)) {
        s_currentItemIndex = offlineTarget;
        Serial.printf("[Sync] Offline mode: Displaying cached item %d with Offline Badge...\n", offlineTarget);
        rotate_mono_180(s_rotatedBuffer, s_rawBuffer, 800, 480);
        gfx_draw_offline_badge(s_rawBuffer);
        rotate_mono_180(s_rawBuffer, s_rotatedBuffer, 800, 480);
        renderRotatedBuffer(s_rotatedBuffer);
      }
    }
    disconnectWiFiIfPowerSave();
    return;
  }
  if (g_deviceId.length() == 0 || g_deviceToken.length() == 0 || g_deviceId == "your-device-uuid") {
    Serial.println("[Sync] Missing device_id or device_token. Please pair the device.");
    disconnectWiFiIfPowerSave();
    return;
  }

  String url = g_serverUrl + "/api/embedded/render?device_id=" + g_deviceId;
  if (requestedItemIndex >= 0) {
    url += "&item=" + String(requestedItemIndex);
  }
  Serial.printf("[Sync] Fetching %s\n", url.c_str());

  HTTPClient http;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Authorization", String("Bearer ") + g_deviceToken);
  http.addHeader("User-Agent", "ScreenTinker-Sticky/1.1 (ESP32-S3-PSRAM)");

  String cachedEtag = cacheGetEtag(targetIndex);
  if (!forceRefresh && !FORCE_REFRESH_ON_BOOT && cachedEtag.length() > 0) {
    http.addHeader("If-None-Match", cachedEtag);
    Serial.printf("[Sync] Sent conditional ETag: %s\n", cachedEtag.c_str());
  }

  const char* headerKeys[] = {"ETag", "X-ST-Expires-In", "X-ST-Item-Index", "X-ST-Total-Items"};
  http.collectHeaders(headerKeys, 4);

  int httpCode = http.GET();
  Serial.printf("[Sync] HTTP Response: %d\n", httpCode);

  if (http.hasHeader("X-ST-Expires-In")) {
    int exp = http.header("X-ST-Expires-In").toInt();
    if (exp > 0) s_syncIntervalMs = exp * 1000;
  }
  if (http.hasHeader("X-ST-Item-Index")) {
    s_currentItemIndex = http.header("X-ST-Item-Index").toInt();
  }
  if (http.hasHeader("X-ST-Total-Items")) {
    s_totalItems = http.header("X-ST-Total-Items").toInt();
  }

  if (httpCode == HTTP_CODE_NOT_MODIFIED) {
    Serial.println("[Sync] 304 Not Modified — Content unchanged.");
    http.end();
    disconnectWiFiIfPowerSave();
    return;
  }

  if (httpCode == HTTP_CODE_OK) {
    String newEtag = http.header("ETag");

    if (!s_rawBuffer || !s_rotatedBuffer) {
      Serial.println("[Sync] Buffers not initialized!");
      http.end();
      disconnectWiFiIfPowerSave();
      return;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t totalReceived = 0;
    uint32_t streamStart = millis();

    while (http.connected() && totalReceived < EPD_BUFFER_SIZE) {
      size_t available = stream->available();
      if (available > 0) {
        size_t toRead = min(available, (size_t)(EPD_BUFFER_SIZE - totalReceived));
        int readBytes = stream->readBytes((char*)(s_rawBuffer + totalReceived), toRead);
        if (readBytes > 0) totalReceived += readBytes;
      } else {
        delay(5);
        if (millis() - streamStart > 10000) break;
      }
    }

    http.end();
    Serial.printf("[Sync] Received %u / %u bytes.\n", totalReceived, EPD_BUFFER_SIZE);

    if (totalReceived == EPD_BUFFER_SIZE) {
      rotate_mono_180(s_rawBuffer, s_rotatedBuffer, 800, 480);
      cachePut(s_currentItemIndex, newEtag, s_rotatedBuffer);
      renderRotatedBuffer(s_rotatedBuffer);
    } else {
      Serial.println("[Sync] Incomplete frame received! Refresh aborted.");
    }

    disconnectWiFiIfPowerSave();
    return;
  }

  String errBody = http.getString();
  Serial.printf("[Sync] HTTP Error %d: %s\n", httpCode, errBody.c_str());
  http.end();
  disconnectWiFiIfPowerSave();
}

// ─── Serial Command Protocol (WebSerial / Web-Flasher) ────────────────────────
void processSerialLine(const String& line) {
  if (line.length() == 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    if (line == "status") {
      Serial.printf("{\"status\":\"ok\",\"ip\":\"%s\",\"ssid\":\"%s\",\"device_id\":\"%s\",\"psram_free\":%u}\n",
                    WiFi.localIP().toString().c_str(), g_wifiSsid.c_str(), g_deviceId.c_str(),
                    psramFound() ? ESP.getFreePsram() : 0);
    } else if (line == "reset") {
      factoryResetNVS();
    } else if (line == "refresh") {
      fetchAndRender(true, s_currentItemIndex);
    } else if (line == "next") {
      int next = (s_totalItems > 0) ? ((s_currentItemIndex + 1) % s_totalItems) : (s_currentItemIndex + 1);
      fetchAndRender(true, next);
    } else if (line == "prev") {
      int prev = (s_totalItems > 0) ? ((s_currentItemIndex - 1 + s_totalItems) % s_totalItems) : max(0, s_currentItemIndex - 1);
      fetchAndRender(true, prev);
    } else if (line == "cache") {
      Serial.printf("--- PSRAM Cache Status ---\n");
      Serial.printf("PSRAM Free: %u KB / %u KB\n", ESP.getFreePsram() / 1024, ESP.getPsramSize() / 1024);
      for (int i = 0; i < MAX_CACHED_ITEMS; i++) {
        if (s_cache[i].valid) {
          Serial.printf("  Slot %d -> Item %d | ETag: %s\n", i, s_cache[i].itemIndex, s_cache[i].etag.c_str());
        }
      }
    } else if (line == "help" || line == "setup" || line == "instructions" || line == "en") {
      showOnboardingScreen(LANG_EN);
    } else if (line == "de") {
      showOnboardingScreen(LANG_DE);
    } else if (line == "nowifi") {
      showNoWifiScreen(s_currentLang);
    } else if (line == "pair") {
      registerAndStartPairing();
    }
    return;
  }

  const char* cmd = doc["cmd"];
  if (!cmd) return;

  if (strcmp(cmd, "config") == 0 || strcmp(cmd, "setup") == 0) {
    String ssid    = doc["ssid"] | g_wifiSsid;
    String pass    = doc["pass"] | g_wifiPass;
    String server  = doc["server"] | g_serverUrl;
    String devId   = doc["device_id"] | "";
    String token   = doc["device_token"] | "";

    saveConfigToNVS(ssid, pass, server, devId, token);
    Serial.println("{\"status\":\"ok\",\"message\":\"Configuration applied.\"}");

    if (devId.length() > 0 && token.length() > 0) {
      fetchAndRender(true);
    } else {
      registerAndStartPairing();
    }
  } else if (strcmp(cmd, "status") == 0) {
    Serial.printf("{\"status\":\"ok\",\"ip\":\"%s\",\"connected\":%s,\"device_id\":\"%s\",\"psram_free\":%u,\"rssi\":%d}\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.status() == WL_CONNECTED ? "true" : "false",
                  g_deviceId.c_str(),
                  psramFound() ? ESP.getFreePsram() : 0,
                  WiFi.RSSI());
  } else if (strcmp(cmd, "reset") == 0) {
    factoryResetNVS();
  } else if (strcmp(cmd, "refresh") == 0) {
    fetchAndRender(true, s_currentItemIndex);
  } else if (strcmp(cmd, "instructions") == 0) {
    showOnboardingScreen(s_currentLang);
  } else if (strcmp(cmd, "nowifi") == 0) {
    showNoWifiScreen(s_currentLang);
  } else if (strcmp(cmd, "pair") == 0) {
    registerAndStartPairing();
  }
}

// ─── Arduino Setup & Main Loop ────────────────────────────────────────────────
void setup() {
  initBoardPowerAndPins();
  Serial.begin(115200);
  delay(300);

  Serial.println("\n=============================================");
  Serial.println("  ScreenTinker — Seeed Studio reTerminal Sticky");
  Serial.println("  PSRAM Cache Engine & Ultra-Low-Power Sync");
  Serial.println("=============================================");

  initPsramBuffers();
  loadConfigFromNVS();

  if (g_wifiSsid.length() > 0 && g_wifiSsid != "Your-WiFi-SSID") {
    if (g_deviceId.length() > 0 && g_deviceToken.length() > 0 && g_deviceId != "your-device-uuid") {
      Serial.println("[Setup] Device configured and paired. Syncing content...");
      fetchAndRender(FORCE_REFRESH_ON_BOOT);
    } else {
      Serial.println("\n[Setup] Wi-Fi configured, but device not paired. Starting 6-digit Pairing flow...");
      registerAndStartPairing();
    }
  } else {
    Serial.println("\n[Setup] No Wi-Fi credentials configured.");
    Serial.println("[Setup] Displaying Onboarding / Web-Flasher instructions on E-Paper...");
    showOnboardingScreen(LANG_EN);
    Serial.println("[Setup] Send JSON config over Serial via Web-Flasher or Serial monitor.");
  }

  s_lastSyncMillis = millis();
}

void loop() {
  // Check for incoming serial configuration commands
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      processSerialLine(line);
    }
  }

  bool isConfigured = (g_wifiSsid.length() > 0 && g_wifiSsid != "Your-WiFi-SSID");
  bool isPaired = (isConfigured && g_deviceId.length() > 0 && g_deviceToken.length() > 0 && g_deviceId != "your-device-uuid");
  int cachedCount = cacheValidCount();

  // If Wi-Fi is configured but device is not paired yet, poll pairing status periodically
  static uint32_t lastPairCheckMillis = 0;
  if (isConfigured && !isPaired) {
    if (s_activePairingDeviceId.length() == 0 && (millis() - lastPairCheckMillis > 6000)) {
      lastPairCheckMillis = millis();
      registerAndStartPairing();
    } else if (s_activePairingDeviceId.length() > 0 && (millis() - lastPairCheckMillis > 3500)) {
      lastPairCheckMillis = millis();
      if (checkPairingStatus()) {
        fetchAndRender(true);
      }
    }
  }

  // Button handling
  static uint32_t lastBtnOk = 0;
  static uint32_t lastBtnUp = 0;
  static uint32_t lastBtnDown = 0;
  static uint32_t okPressStart = 0;

  // OK Button (GPIO 4): Short press -> Refresh; Long press (5s) -> Factory reset
  if (digitalRead(PIN_BTN_OK) == LOW) {
    if (okPressStart == 0) okPressStart = millis();
    if (millis() - okPressStart > 5000) {
      Serial.println("\n>>> [Button] Long press (5s) detected -> Performing Factory Reset!");
      factoryResetNVS();
    }
  } else {
    if (okPressStart > 0) {
      uint32_t duration = millis() - okPressStart;
      okPressStart = 0;
      if (duration > 50 && duration < 3000 && millis() - lastBtnOk > 800) {
        lastBtnOk = millis();
        if (!isConfigured) {
          Serial.println("\n>>> [Button] OK pressed -> Redrawing setup instructions...");
          showOnboardingScreen(s_currentLang);
        } else if (!isPaired) {
          Serial.println("\n>>> [Button] OK pressed -> Requesting new pairing code...");
          registerAndStartPairing();
        } else {
          Serial.println("\n>>> [Button] OK pressed -> Forcing refresh...");
          fetchAndRender(true, s_currentItemIndex);
          s_lastSyncMillis = millis();
        }
      }
    }
  }

  // UP Button (GPIO 5):
  if (digitalRead(PIN_BTN_UP) == LOW && millis() - lastBtnUp > 600) {
    lastBtnUp = millis();
    if (!isConfigured) {
      Serial.println("\n>>> [Button] UP pressed -> Language: DE (Deutsch)");
      showOnboardingScreen(LANG_DE);
    } else if (!isPaired) {
      Serial.println("\n>>> [Button] UP pressed -> Language: DE (Pairing Screen)");
      showPairingCodeScreen(s_activePairingCode.c_str(), LANG_DE);
    } else if (cachedCount == 0 && WiFi.status() != WL_CONNECTED) {
      Serial.println("\n>>> [Button] UP pressed -> Language: DE (No-WiFi Screen)");
      showNoWifiScreen(LANG_DE);
    } else {
      int prev = (s_totalItems > 0) ? ((s_currentItemIndex - 1 + s_totalItems) % s_totalItems) : max(0, s_currentItemIndex - 1);
      Serial.printf("\n>>> [Button] UP pressed -> Navigating to item %d...\n", prev);
      fetchAndRender(false, prev);
      s_lastSyncMillis = millis();
    }
  }

  // DOWN Button (GPIO 6):
  if (digitalRead(PIN_BTN_DOWN) == LOW && millis() - lastBtnDown > 600) {
    lastBtnDown = millis();
    if (!isConfigured) {
      Serial.println("\n>>> [Button] DOWN pressed -> Language: EN (English)");
      showOnboardingScreen(LANG_EN);
    } else if (!isPaired) {
      Serial.println("\n>>> [Button] DOWN pressed -> Language: EN (Pairing Screen)");
      showPairingCodeScreen(s_activePairingCode.c_str(), LANG_EN);
    } else if (cachedCount == 0 && WiFi.status() != WL_CONNECTED) {
      Serial.println("\n>>> [Button] DOWN pressed -> Language: EN (No-WiFi Screen)");
      showNoWifiScreen(LANG_EN);
    } else {
      int next = (s_totalItems > 0) ? ((s_currentItemIndex + 1) % s_totalItems) : (s_currentItemIndex + 1);
      Serial.printf("\n>>> [Button] DOWN pressed -> Navigating to item %d...\n", next);
      fetchAndRender(false, next);
      s_lastSyncMillis = millis();
    }
  }

  // Periodic Auto-Sync (Only when device is fully configured & paired)
  if (isPaired && (millis() - s_lastSyncMillis > s_syncIntervalMs)) {
    s_lastSyncMillis = millis();
    Serial.println("\n[AutoSync] Periodic sync triggered...");
    fetchAndRender(false);
  }

  delay(20);
}
