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

#include "device_profile.h"
#include "config.h"
#include "setup_screen.h"
#include "imu_driver.h"
#include "sht40_driver.h"
#include "diag_logger.h"
#include "ota_updater.h"

#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>

// ─── Hardware I2C & Fuel Gauge (TI BQ27220) ──────────────────────────────────
static bool s_wireInitialized = false;
static void ensureI2C() {
  if (!s_wireInitialized) {
#if defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
#else
    Wire.begin();
#endif
    s_wireInitialized = true;
  }
}

static int readBatteryPercent() {
  ensureI2C();

  // BQ27220 standard command StateOfCharge() is at 0x2C (16-bit little-endian)
  Wire.beginTransmission(0x55);
  Wire.write(0x2C);
  if (Wire.endTransmission(false) != 0) {
    return -1;
  }

  if (Wire.requestFrom((uint8_t)0x55, (uint8_t)2) == 2) {
    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    int soc = (msb << 8) | lsb;
    if (soc >= 0 && soc <= 100) {
      return soc;
    }
  }
  return -1;
}

static bool isUsbPowerConnected() {
#ifdef PIN_EXT_POWER
  pinMode(PIN_EXT_POWER, INPUT);
  return digitalRead(PIN_EXT_POWER) == HIGH;
#else
  return true;
#endif
}

static bool isBatteryCharging() {
#ifdef PIN_CHG_STAT
  pinMode(PIN_CHG_STAT, INPUT);
  return isUsbPowerConnected() && (digitalRead(PIN_CHG_STAT) == LOW);
#else
  return false;
#endif
}

// ─── Persistent Storage (NVS) ────────────────────────────────────────────────
static Preferences s_prefs;

static String g_wifiSsid = "";
static String g_wifiPass = "";
static String g_serverUrl = "";
static String g_deviceId = "";
static String g_deviceToken = "";
static bool g_multiZone = DEFAULT_MULTI_ZONE;

// ─── Captive Portal State ────────────────────────────────────────────────────
static DNSServer s_dnsServer;
static WebServer* s_webServer = nullptr;
static bool s_apModeActive = false;

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

#ifdef PIN_PWR_HOLD
  gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
#endif
#ifdef PIN_PWR_LOCK
  gpio_hold_dis((gpio_num_t)PIN_PWR_LOCK);
  pinMode(PIN_PWR_LOCK, OUTPUT);
  digitalWrite(PIN_PWR_LOCK, HIGH);
#endif

#ifdef PIN_SD_CS
  gpio_hold_dis((gpio_num_t)PIN_SD_CS);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
#endif
#ifdef PIN_SD_EN
  gpio_hold_dis((gpio_num_t)PIN_SD_EN);
  pinMode(PIN_SD_EN, OUTPUT);
  digitalWrite(PIN_SD_EN, HIGH);
#endif
#ifdef PIN_SD_DETECT
  gpio_hold_dis((gpio_num_t)PIN_SD_DETECT);
#endif

#ifdef PIN_EPD_PWR_EN
  gpio_hold_dis((gpio_num_t)PIN_EPD_PWR_EN);
  pinMode(PIN_EPD_PWR_EN, OUTPUT);
  digitalWrite(PIN_EPD_PWR_EN, HIGH);
#endif
#ifdef PIN_EPD_TFT_EN
  gpio_hold_dis((gpio_num_t)PIN_EPD_TFT_EN);
  pinMode(PIN_EPD_TFT_EN, OUTPUT);
  digitalWrite(PIN_EPD_TFT_EN, HIGH);
#endif
#ifdef PIN_EPD_ITE_EN
  gpio_hold_dis((gpio_num_t)PIN_EPD_ITE_EN);
  pinMode(PIN_EPD_ITE_EN, OUTPUT);
  digitalWrite(PIN_EPD_ITE_EN, HIGH);
#endif
#ifdef PIN_EPD_ENABLE
  gpio_hold_dis((gpio_num_t)PIN_EPD_ENABLE);
  pinMode(PIN_EPD_ENABLE, OUTPUT);
  digitalWrite(PIN_EPD_ENABLE, HIGH);
#endif
#ifdef PIN_BATT_EN
  gpio_hold_dis((gpio_num_t)PIN_BATT_EN);
  pinMode(PIN_BATT_EN, OUTPUT);
  digitalWrite(PIN_BATT_EN, HIGH);
#endif

#ifdef PIN_TOUCH_EN
  gpio_hold_dis((gpio_num_t)PIN_TOUCH_EN);
#endif
#ifdef PIN_TOUCH_RST
  gpio_hold_dis((gpio_num_t)PIN_TOUCH_RST);
#endif
#ifdef PIN_BUZZER
  gpio_hold_dis((gpio_num_t)PIN_BUZZER);
#endif

  // Buttons (Active LOW with internal pullup)
#ifdef PIN_BTN_OK
  gpio_hold_dis((gpio_num_t)PIN_BTN_OK);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
#endif
#ifdef PIN_BTN_UP
  gpio_hold_dis((gpio_num_t)PIN_BTN_UP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
#endif
#ifdef PIN_BTN_DOWN
  gpio_hold_dis((gpio_num_t)PIN_BTN_DOWN);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
#endif

  delay(50);
}

// ─── Display Driver HAL (Multi-Panel Unified Abstraction) ───────────────────
static bool initDisplayPanel() {
  if (s_epdInitialized && s_panel != NULL) return true;

  spi_bus_config_t bus_config = {};
  bus_config.mosi_io_num = PIN_EPD_MOSI;
#ifdef PIN_EPD_MISO
  bus_config.miso_io_num = PIN_EPD_MISO;
#else
  bus_config.miso_io_num = -1;
#endif
  bus_config.sclk_io_num = PIN_EPD_SCK;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.data4_io_num = -1;
  bus_config.data5_io_num = -1;
  bus_config.data6_io_num = -1;
  bus_config.data7_io_num = -1;
  bus_config.max_transfer_sz = EPD_BUFFER_SIZE;

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
#ifdef PIN_EPD_DC
  panel_config.pin_dc = (gpio_num_t)PIN_EPD_DC;
#else
  panel_config.pin_dc = GPIO_NUM_NC;
#endif
  panel_config.pin_rst = (gpio_num_t)PIN_EPD_RST;
  panel_config.pin_busy = (gpio_num_t)PIN_EPD_BUSY;
#ifdef PIN_EPD_ENABLE
  panel_config.pin_enable = (gpio_num_t)PIN_EPD_ENABLE;
#else
  panel_config.pin_enable = GPIO_NUM_NC;
#endif
#ifdef PIN_EPD_CS1
  panel_config.pin_cs1 = (gpio_num_t)PIN_EPD_CS1;
#else
  panel_config.pin_cs1 = GPIO_NUM_NC;
#endif
  panel_config.busy_timeout_ms = DEVICE_FULL_REFRESH_TIME_MS + 2000;
  panel_config.reset_low_ms = 10;
  panel_config.reset_high_ms = 10;
  panel_config.busy_level = 1;
  panel_config.enable_level = 1;
  panel_config.mirror_x = true;

  err = seeed_epaper_new_panel((seeed_epaper_panel_model_t)DEVICE_PANEL_MODEL, &panel_config, &s_panel);
  if (err != ESP_OK) {
    Serial.printf("[EPD] seeed_epaper_new_panel error: %s\n", esp_err_to_name(err));
    return false;
  }

  s_epdInitialized = true;
  Serial.printf("[EPD] Display panel initialized (%s, model %d).\n", DEVICE_PROFILE_NAME, (int)DEVICE_PANEL_MODEL);
  return true;
}

static bool renderRotatedBuffer(const uint8_t* rotatedBuf, bool fastPartial = false) {
  if (!initDisplayPanel()) return false;

  const seeed_epaper_area_t full_screen = {0, 0, (uint16_t)EPD_WIDTH, (uint16_t)EPD_HEIGHT};
  uint32_t refStart = millis();

  seeed_epaper_refresh_mode_t mode = SEEED_EPAPER_REFRESH_FULL;
  if (fastPartial && DEVICE_SUPPORTS_PARTIAL_REFRESH) {
    mode = SEEED_EPAPER_REFRESH_PARTIAL;
  }

  seeed_epaper_pixel_format_t pixel_fmt = SEEED_EPAPER_PIXEL_FORMAT_MONO1_MSB;
  size_t stride = EPD_WIDTH / 8;
#if (DEVICE_COLOR_MODE == COLOR_MODE_SPECTRA6)
  pixel_fmt = SEEED_EPAPER_PIXEL_FORMAT_SPECTRA6;
  stride = (EPD_WIDTH + 1) / 2;
#elif (DEVICE_COLOR_MODE == COLOR_MODE_GRAY16)
  pixel_fmt = SEEED_EPAPER_PIXEL_FORMAT_GRAY16;
  stride = (EPD_WIDTH + 1) / 2;
#elif (DEVICE_COLOR_MODE == COLOR_MODE_GRAY4)
  pixel_fmt = SEEED_EPAPER_PIXEL_FORMAT_GRAY4;
  stride = (EPD_WIDTH + 3) / 4;
#endif

  esp_err_t refErr = seeed_epaper_panel_refresh_area(
    s_panel,
    &full_screen,
    rotatedBuf,
    stride,
    pixel_fmt,
    mode
  );

  uint32_t refElapsed = millis() - refStart;
  Serial.printf("[EPD] Refresh (%s) completed in %u ms (result: %s)\n",
                (mode == SEEED_EPAPER_REFRESH_PARTIAL) ? "PARTIAL" : "FULL", refElapsed, esp_err_to_name(refErr));
  return refErr == ESP_OK;
}

static Language s_currentLang = LANG_EN;
static uint8_t s_wifiTxPowerLevel = 0; // 0 = MAX (19.5 dBm), 1 = MED (15 dBm), 2 = LOW (11 dBm)

#if DEVICE_SUPPORTS_IMU
static bool s_hasImu = true;
#else
static bool s_hasImu = false;
#endif

#if !DEVICE_SUPPORTS_IMU && (EPD_HEIGHT > EPD_WIDTH)
static OrientationMode s_orientationMode = ORIENT_MODE_PORTRAIT;
static int s_activeOrientation = ORIENTATION_PORTRAIT_90;
#else
static OrientationMode s_orientationMode = ORIENT_MODE_LANDSCAPE;
static int s_activeOrientation = ORIENTATION_LANDSCAPE_0;
#endif
static int s_pendingOrientation = -1;
static uint32_t s_pendingOrientationStart = 0;
static uint32_t s_lastImuCheckMillis = 0;
static bool s_imuDebugScreen = false;

// ─── OTA Rollback & Verification Failure Tracking ────────────────────────────
static int s_otaVerifyFailures = 0;
static uint32_t s_bootMillis = 0;

static void checkOtaVerificationFailure(const char* reason) {
  OtaState st;
  ota_get_state(&st);
  if (!st.is_pending_verification) {
    return;
  }
  s_otaVerifyFailures++;
  uint32_t elapsed = (s_bootMillis > 0) ? (millis() - s_bootMillis) : 0;
  Serial.printf("[OTA] Verification failure #%d: %s (elapsed %u ms)\n", s_otaVerifyFailures, reason, elapsed);
  if (s_otaVerifyFailures >= 3 || elapsed > 90000) {
    Serial.println("[OTA] Critical: Pending verification failed (>=3 consecutive failures or >90s elapsed). Rolling back...");
    ota_rollback_and_reboot();
  }
}

void showImuDebugLetterScreen(int orient) {
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  int16_t ax = 0, ay = 0, az = 0;
  imu_read_accel(&ax, &ay, &az);
  char letter = 'A' + (orient >= 0 && orient <= 3 ? orient : 0);

  Serial.println("\n************************************************************");
  Serial.printf(">>> [IMU DEBUG] DISPLAYING DEBUG SCREEN LETTER '%c'\n", letter);
  Serial.printf(">>> [IMU DEBUG] ID: %d, ax=%d, ay=%d, az=%d\n", orient, (int)ax, (int)ay, (int)az);
  Serial.println("************************************************************\n");

  // Sticky PCB axes / orientation mapping:
  // - USB port to the left:  ORIENTATION_LANDSCAPE_0 (0°) -> Letter A (upright)
  // - USB port pointing down: ORIENTATION_PORTRAIT_90 (90°) -> Letter B (upright)
  // - USB port to the right: ORIENTATION_LANDSCAPE_180 (180°) -> Letter C (upright)
  // - USB port pointing up:   ORIENTATION_PORTRAIT_270 (270°) -> Letter D (upright)
  if (orient == ORIENTATION_PORTRAIT_90 || orient == ORIENTATION_PORTRAIT_270) {
    gfx_set_screen_dimensions(EPD_HEIGHT, EPD_WIDTH); // 480 x 800
    renderImuDebugScreen(s_rawBuffer, orient, ax, ay, az);
    gfx_set_screen_dimensions(EPD_WIDTH, EPD_HEIGHT); // Restore 800 x 480
    bool flip = (orient == ORIENTATION_PORTRAIT_270);
    rotate_mono_portrait(s_rawBuffer, s_rotatedBuffer, EPD_HEIGHT, EPD_WIDTH, flip);
  } else {
    // Landscape 0 and Landscape 180 both use panel scan-direction rotation
    renderImuDebugScreen(s_rawBuffer, orient, ax, ay, az);
    rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  }

  renderRotatedBuffer(s_rotatedBuffer, false);
}

void showSplashScreen(const char* version, const char* statusMsg) {
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.printf("[Display] Rendering Splash Screen (v%s, %s)...\n", version ? version : FIRMWARE_VERSION, statusMsg ? statusMsg : "");
  renderSplashScreen(s_rawBuffer, version, statusMsg, s_currentLang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

void showOnboardingScreen(Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.println("[Display] Rendering Onboarding Screen on E-Paper...");
  renderOnboardingScreen(s_rawBuffer, lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

void showNoWifiScreen(Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.println("[Display] Rendering No Wi-Fi Screen on E-Paper...");
  renderNoWifiScreen(s_rawBuffer, g_wifiSsid.c_str(), lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

void showEmptyPlaylistScreen(const char* deviceName = "", Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.printf("[Display] Rendering Empty Playlist Screen for '%s' on E-Paper...\n", deviceName ? deviceName : "");
  renderEmptyPlaylistScreen(s_rawBuffer, deviceName, g_serverUrl.c_str(), lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

void showOtaUpdateScreen(const char* statusMsg = nullptr, Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.println("[Display] Rendering OTA Update Screen on E-Paper...");
  renderOtaUpdateScreen(s_rawBuffer, statusMsg, lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

static String s_activePairingCode = "";
static String s_activePairingDeviceId = "";
static String s_activeClaimSecret = "";

void showPairingCodeScreen(const char* code, Language lang = LANG_EN) {
  s_currentLang = lang;
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.printf("[Display] Rendering Pairing Code Screen (Code: %s)...\n", code ? code : "");
  renderPairingCodeScreen(s_rawBuffer, code, g_serverUrl.c_str(), lang);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

// ─── Refresh Mode Policy ──────────────────────────────────────────────────────
#define REFRESH_MODE_SMART_PARTIAL 0
#define REFRESH_MODE_ALWAYS_FULL   1

static uint8_t s_refreshMode = REFRESH_MODE_SMART_PARTIAL;
static uint8_t s_partialRefreshLimit = 6;
RTC_DATA_ATTR int rtc_consecutivePartialCount = 0;

static bool s_inSystemMenu = false;
static bool s_inStatusScreen = false;
static int s_menuSelection = 0;
static uint32_t s_menuOpenMillis = 0;

void showSystemMenu(int selectedIndex, bool fastPartial = false) {
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  int battPct = readBatteryPercent();
  bool charging = isBatteryCharging();
  bool usb = isUsbPowerConnected();
  Serial.printf("[Display] Rendering System Menu (Item: %d, Fast: %d, Lang: %s, TxPwr: %d, OrientMode: %d, RefreshMode: %d, Batt: %d%%)...\n",
                selectedIndex, fastPartial ? 1 : 0, (s_currentLang == LANG_DE) ? "DE" : "EN", s_wifiTxPowerLevel, (int)s_orientationMode, s_refreshMode, battPct);
  renderSystemMenu(s_rawBuffer, selectedIndex, s_currentLang, s_wifiTxPowerLevel, (uint8_t)s_orientationMode, s_hasImu, battPct, charging, usb, s_refreshMode, DEVICE_SUPPORTS_PARTIAL_REFRESH);
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, fastPartial);
}

static String s_lastIp = "";
static int s_lastRssi = 0;

void showStatusInfoScreen() {
  if (!s_rawBuffer || !s_rotatedBuffer) return;
  Serial.println("[Display] Rendering Status & Info Screen...");
  String currentIp = s_lastIp;
  int currentRssi = s_lastRssi;
  if (WiFi.status() == WL_CONNECTED) {
    currentIp = WiFi.localIP().toString();
    currentRssi = WiFi.RSSI();
  }
  float curT = -999.0f;
  float curH = -1.0f;
  sht40_read(&curT, &curH);
  int battPct = readBatteryPercent();
  bool charging = isBatteryCharging();
  bool usb = isUsbPowerConnected();

  renderStatusInfoScreen(
    s_rawBuffer,
    currentIp.c_str(),
    g_wifiSsid.c_str(),
    currentRssi,
    g_serverUrl.c_str(),
    g_deviceId.c_str(),
    FIRMWARE_VERSION,
    s_currentLang,
    curT,
    curH,
    battPct,
    charging,
    usb
  );
  rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
  renderRotatedBuffer(s_rotatedBuffer, false);
}

// ─── Adaptive Deep Sleep & RTC State ──────────────────────────────────────────
RTC_DATA_ATTR int      rtc_unchangedCycles   = 0;
RTC_DATA_ATTR int      rtc_currentItemIndex  = 0;
RTC_DATA_ATTR uint32_t rtc_lastSyncIntervalS = 30;
RTC_DATA_ATTR char     rtc_lastEtag[64]      = {0};

static bool     s_inInteractiveMode      = false;
static uint32_t s_interactiveUntilMillis = 0;

void enterDeepSleepWithWakeup(uint32_t sleepSeconds) {
  if (sleepSeconds < 5) sleepSeconds = 5;

  Serial.printf("\n[Power] Entering Deep Sleep for %u seconds (Adaptive 304 Count: %d)...\n",
                sleepSeconds, rtc_unchangedCycles);
  Serial.println("[Power] Wakeup: RTC Timer OR Buttons (OK / UP / DOWN). Standby ~15 µA.");

  // Controller Deep Sleep
  if (s_panel) {
    seeed_epaper_panel_sleep(s_panel);
  }

  // Release display power boost circuit
#ifdef PIN_EPD_PWR_EN
  digitalWrite(PIN_EPD_PWR_EN, LOW);
#endif
#ifdef PIN_EPD_TFT_EN
  digitalWrite(PIN_EPD_TFT_EN, LOW);
#endif
#ifdef PIN_EPD_ITE_EN
  digitalWrite(PIN_EPD_ITE_EN, LOW);
#endif
#ifdef PIN_EPD_ENABLE
  digitalWrite(PIN_EPD_ENABLE, LOW);
#endif
  delay(10);

  // Maintain board power latches across deep sleep
#ifdef PIN_PWR_HOLD
  gpio_hold_en((gpio_num_t)PIN_PWR_HOLD);
#endif
#ifdef PIN_PWR_LOCK
  gpio_hold_en((gpio_num_t)PIN_PWR_LOCK);
#endif
  gpio_deep_sleep_hold_en();

  // 1. Timer Wakeup
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

  // 2. Button Wakeup (OK, UP, DOWN on GPIO 4, 5, 6 - active LOW)
  uint64_t btn_mask = (1ULL << PIN_BTN_OK) | (1ULL << PIN_BTN_UP) | (1ULL << PIN_BTN_DOWN);
  esp_sleep_enable_ext1_wakeup(btn_mask, ESP_EXT1_WAKEUP_ANY_LOW);

  Serial.flush();
  esp_deep_sleep_start();
}

static uint32_t s_nextSyncMillis = 0;

void scheduleNextSync(uint32_t sleepSeconds) {
  if (isUsbPowerConnected()) {
    Serial.printf("\n[Power] USB Power Supply Detected (Mains Mode). Staying awake with active Wi-Fi. Next sync in %u s.\n", sleepSeconds);
    s_nextSyncMillis = millis() + (sleepSeconds * 1000UL);
    return;
  }
  enterDeepSleepWithWakeup(sleepSeconds);
}

void powerOffDevice() {
  Serial.println("\n=============================================");
  Serial.println("  [Power] RENDERING SHUTDOWN SCREEN & POWERING OFF");
  Serial.println("=============================================");
  
  // Render dedicated Power Off screen on E-Paper so user knows the device is off
  if (s_rawBuffer && s_rotatedBuffer) {
    renderPowerOffScreen(s_rawBuffer, s_currentLang);
    rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
    renderRotatedBuffer(s_rotatedBuffer);
  }

  delay(200);

  // Controller Deep Sleep
  if (s_panel) {
    seeed_epaper_panel_sleep(s_panel);
  }

  // Release display power boost circuit
#ifdef PIN_EPD_PWR_EN
  digitalWrite(PIN_EPD_PWR_EN, LOW);
#endif
#ifdef PIN_EPD_TFT_EN
  digitalWrite(PIN_EPD_TFT_EN, LOW);
#endif
#ifdef PIN_EPD_ITE_EN
  digitalWrite(PIN_EPD_ITE_EN, LOW);
#endif
#ifdef PIN_EPD_ENABLE
  digitalWrite(PIN_EPD_ENABLE, LOW);
#endif
#ifdef PIN_BATT_EN
  digitalWrite(PIN_BATT_EN, LOW);
#endif
  delay(50);
  
  // Release hardware power latches (cuts battery power on battery mode)
#ifdef PIN_PWR_HOLD
  digitalWrite(PIN_PWR_HOLD, LOW);
#endif
#ifdef PIN_PWR_LOCK
  digitalWrite(PIN_PWR_LOCK, LOW);
#endif
  
  // Set wakeup on OK button (GPIO 4 is active LOW)
#ifdef PIN_BTN_OK
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_OK, 0);
#endif
  
  Serial.println("[Power] Power latch released. Entering Deep Sleep (Press OK to wake)...");
  delay(100);
  esp_deep_sleep_start();
}

bool registerAndStartPairing();
bool checkPairingStatus();
void fetchAndRender(bool forceFreshFetch = false, int requestedItemIndex = -1);

void redrawCurrentState() {
  if (s_imuDebugScreen) {
    showImuDebugLetterScreen(s_activeOrientation);
    return;
  }
  bool isConfigured = (g_wifiSsid.length() > 0 && g_wifiSsid != "Your-WiFi-SSID");
  bool isPaired = (isConfigured && g_deviceId.length() > 0 && g_deviceToken.length() > 0 && g_deviceId != "your-device-uuid");
  if (!isConfigured) {
    showOnboardingScreen(s_currentLang);
  } else if (!isPaired) {
    showPairingCodeScreen(s_activePairingCode.c_str(), s_currentLang);
  } else {
    fetchAndRender(false, s_currentItemIndex);
  }
}

// ─── NVS Configuration Management ─────────────────────────────────────────────
void loadConfigFromNVS() {
  s_prefs.begin("screentinker", false);
  g_wifiSsid    = s_prefs.getString("wifi_ssid", WIFI_SSID);
  g_wifiPass    = s_prefs.getString("wifi_pass", WIFI_PASSWORD);
  g_serverUrl   = s_prefs.getString("server_url", SCREENTINKER_HOST);
  g_deviceId    = s_prefs.getString("device_id", DEVICE_ID);
  g_deviceToken = s_prefs.getString("device_token", DEVICE_TOKEN);
  g_multiZone   = s_prefs.getBool("multi_zone", DEFAULT_MULTI_ZONE);
  uint8_t langCode = s_prefs.getUChar("lang", (uint8_t)LANG_EN);
  s_currentLang = (langCode == 1) ? LANG_DE : LANG_EN;
  s_wifiTxPowerLevel = s_prefs.getUChar("wifi_tx_pwr", 0);
  if (s_wifiTxPowerLevel > 2) s_wifiTxPowerLevel = 0;
#if !DEVICE_SUPPORTS_IMU && (EPD_HEIGHT > EPD_WIDTH)
  uint8_t defOrient = (uint8_t)ORIENT_MODE_PORTRAIT;
#else
  uint8_t defOrient = s_hasImu ? (uint8_t)ORIENT_MODE_AUTO : (uint8_t)ORIENT_MODE_LANDSCAPE;
#endif
  uint8_t savedOrient = s_prefs.getUChar("orient_mode", defOrient);
  if (!s_hasImu && savedOrient == ORIENT_MODE_AUTO) {
#if (EPD_HEIGHT > EPD_WIDTH)
    savedOrient = ORIENT_MODE_PORTRAIT;
#else
    savedOrient = ORIENT_MODE_LANDSCAPE;
#endif
  }
  if (savedOrient > 2) savedOrient = defOrient;
  s_orientationMode = (OrientationMode)savedOrient;
  if (s_orientationMode == ORIENT_MODE_LANDSCAPE) {
    s_activeOrientation = ORIENTATION_LANDSCAPE_0;
  } else if (s_orientationMode == ORIENT_MODE_PORTRAIT) {
    s_activeOrientation = ORIENTATION_PORTRAIT_90;
  }
  s_refreshMode = s_prefs.getUChar("refresh_mode", REFRESH_MODE_SMART_PARTIAL);
  if (s_refreshMode > 1) s_refreshMode = REFRESH_MODE_SMART_PARTIAL;
  s_partialRefreshLimit = s_prefs.getUChar("part_limit", 6);
  if (s_partialRefreshLimit == 0) s_partialRefreshLimit = 6;
  s_prefs.end();

  Serial.println("[Config] Loaded configuration from NVS/Defaults:");
  Serial.printf("  SSID:        %s\n", g_wifiSsid.c_str());
  Serial.printf("  Server:      %s\n", g_serverUrl.c_str());
  Serial.printf("  Device ID:   %s\n", g_deviceId.c_str());
  Serial.printf("  Token set:   %s\n", g_deviceToken.length() > 0 ? "YES" : "NO");
  Serial.printf("  Multi-Zone:  %s\n", g_multiZone ? "ENABLED" : "DISABLED");
  Serial.printf("  Language:    %s\n", (s_currentLang == LANG_DE) ? "Deutsch (DE)" : "English (EN)");
  Serial.printf("  Wi-Fi TX:    %s\n", (s_wifiTxPowerLevel == 1) ? "15.0 dBm" : (s_wifiTxPowerLevel == 2) ? "11.0 dBm" : "19.5 dBm (MAX)");
  Serial.printf("  Refresh:     %s (limit: %d)\n", (s_refreshMode == REFRESH_MODE_SMART_PARTIAL) ? "Smart Partial [6:1]" : "Always Full", s_partialRefreshLimit);
  Serial.printf("  Orientation: %s\n", (s_orientationMode == ORIENT_MODE_AUTO) ? "Auto (IMU)" : (s_orientationMode == ORIENT_MODE_PORTRAIT) ? "Portrait" : "Landscape");
}

void stopCaptivePortal();

void saveConfigToNVS(const String& ssid, const String& pass, const String& server, const String& devId, const String& token, bool multiZone = false) {
  stopCaptivePortal();
  s_prefs.begin("screentinker", false);
  s_prefs.putString("wifi_ssid", ssid);
  s_prefs.putString("wifi_pass", pass);
  s_prefs.putString("server_url", server);
  s_prefs.putString("device_id", devId);
  s_prefs.putString("device_token", token);
  s_prefs.putBool("multi_zone", multiZone);
  s_prefs.end();

  g_wifiSsid = ssid;
  g_wifiPass = pass;
  g_serverUrl = server;
  g_deviceId = devId;
  g_deviceToken = token;
  g_multiZone = multiZone;
  cacheClear();
  Serial.printf("[Config] Configuration saved: SSID='%s', Server='%s', DeviceID='%s', MultiZone=%d\n",
                g_wifiSsid.c_str(), g_serverUrl.c_str(), g_deviceId.c_str(), g_multiZone ? 1 : 0);
}

void factoryResetNVS() {
  stopCaptivePortal();
  s_prefs.begin("screentinker", false);
  s_prefs.clear();
  s_prefs.end();
  cacheClear();
  Serial.println("[Config] NVS wiped! Resetting device in 1 second...");
  delay(1000);
  ESP.restart();
}

// ─── Captive Portal Implementation ───────────────────────────────────────────
static const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ScreenTinker Setup</title>
  <style>
    * { box-sizing: border-box; }
    body { font-family: system-ui, -apple-system, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; justify-content: center; }
    .card { background: #1e293b; border-radius: 12px; padding: 24px; max-width: 440px; width: 100%; border: 1px solid #334155; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
    h1 { margin: 0 0 4px; font-size: 22px; color: #38bdf8; display: flex; align-items: center; gap: 8px; }
    p.sub { margin: 0 0 20px; color: #94a3b8; font-size: 14px; }
    label { display: block; font-weight: 600; font-size: 13px; margin: 14px 0 4px; color: #cbd5e1; }
    input, select { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid #475569; background: #0f172a; color: #fff; font-size: 15px; }
    input:focus, select:focus { outline: none; border-color: #38bdf8; }
    .btn { margin-top: 24px; width: 100%; padding: 14px; border: none; border-radius: 8px; background: #0284c7; color: #fff; font-size: 16px; font-weight: 600; cursor: pointer; }
    .btn:hover { background: #0369a1; }
    .footer { margin-top: 16px; font-size: 12px; color: #64748b; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h1>🖥️ ScreenTinker Setup</h1>
    <p class="sub">Seeed Studio reTerminal Sticky Configuration</p>
    <form action="/save" method="POST">
      <label>Wi-Fi Network (SSID)</label>
      <input type="text" name="ssid" placeholder="MyHomeNetwork" required>
      <label>Wi-Fi Password</label>
      <input type="password" name="pass" placeholder="••••••••">
      <label>ScreenTinker Server URL</label>
      <input type="text" name="server" value="http://192.168.1.100:3001" placeholder="http://192.168.1.100:3001" required>
      <label>Layout / Rendering Mode</label>
      <select name="multizone">
        <option value="0">Standard (Fullscreen Slides & Widgets)</option>
        <option value="1">Multi-Zone Layouts</option>
      </select>
      <label>Orientation / Ausrichtung</label>
      <select name="orientation">
        <option value="1">Landscape / Querformat</option>
        <option value="2">Portrait / Hochformat</option>
        <option value="0">Auto (IMU Sensor)</option>
      </select>
      <button class="btn" type="submit">💾 Save & Connect / Speichern</button>
    </form>
    <div class="footer">reTerminal Series • E-Paper Display</div>
  </div>
</body>
</html>
)rawliteral";

void stopCaptivePortal();

static void handleCaptivePortalRoot() {
  if (!s_webServer) return;
  s_webServer->send(200, "text/html", CAPTIVE_PORTAL_HTML);
}

static bool s_pendingConfigSave = false;
static uint32_t s_pendingSaveMillis = 0;
static String s_pendingSsid = "";
static String s_pendingPass = "";
static String s_pendingServer = "";
static bool s_pendingMultiZone = false;
static uint8_t s_pendingOrient = 1;

static void handleCaptivePortalSave() {
  if (!s_webServer) return;
  String ssid = s_webServer->arg("ssid");
  String pass = s_webServer->arg("pass");
  String server = s_webServer->arg("server");
  String mzStr = s_webServer->arg("multizone");
  String orientArg = s_webServer->arg("orientation");

  ssid.trim();
  pass.trim();
  server.trim();

  if (ssid.length() == 0 || server.length() == 0) {
    s_webServer->send(400, "text/plain", "SSID and Server URL are required.");
    return;
  }

  s_pendingSsid = ssid;
  s_pendingPass = pass;
  s_pendingServer = server;
  s_pendingMultiZone = (mzStr == "1" || mzStr == "true");
  if (orientArg.length() > 0) {
    s_pendingOrient = orientArg.toInt();
    if (!s_hasImu && s_pendingOrient == 0) s_pendingOrient = 1;
    if (s_pendingOrient > 2) s_pendingOrient = 1;
  }
  s_pendingConfigSave = true;
  s_pendingSaveMillis = millis();

  String successHtml = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Saving...</title><style>body{font-family:system-ui;background:#0f172a;color:#fff;padding:40px;text-align:center;}.card{background:#1e293b;padding:30px;border-radius:12px;max-width:400px;margin:auto;border:1px solid #334155;}</style></head><body><div class='card'><h2>✅ Settings Saved!</h2><p>Connecting to <b>" + ssid + "</b> and pairing with ScreenTinker...</p><p>Please check your E-Paper display for the 6-digit code!</p></div></body></html>";
  s_webServer->send(200, "text/html", successHtml);
  Serial.printf("[CaptivePortal] Credentials received for SSID '%s', scheduling save & pairing in loop...\n", ssid.c_str());
}

void stopCaptivePortal() {
  if (!s_apModeActive) return;
  Serial.println("[CaptivePortal] Stopping SoftAP and WebServer...");
  if (s_webServer) {
    s_webServer->stop();
    delete s_webServer;
    s_webServer = nullptr;
  }
  s_dnsServer.stop();
  WiFi.softAPdisconnect(true);
  s_apModeActive = false;
  Serial.println("[CaptivePortal] SoftAP stopped.");
}

void startCaptivePortal() {
  if (s_apModeActive) return;

  Serial.println("\n[CaptivePortal] Initializing SoftAP 'ScreenTinker-Setup'...");
  WiFi.persistent(false);
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  bool apSuccess = WiFi.softAP("ScreenTinker-Setup", "", 1, 0, 4);
  Serial.printf("[CaptivePortal] SoftAP started: %s (IP: %s)\n",
                apSuccess ? "SUCCESS" : "FAILED", WiFi.softAPIP().toString().c_str());

  s_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  s_dnsServer.start(53, "*", apIP);

  if (s_webServer) delete s_webServer;
  s_webServer = new WebServer(80);

  s_webServer->on("/", HTTP_GET, handleCaptivePortalRoot);
  s_webServer->on("/save", HTTP_POST, handleCaptivePortalSave);
  s_webServer->on("/generate_204", HTTP_GET, handleCaptivePortalRoot);
  s_webServer->on("/hotspot-detect.html", HTTP_GET, handleCaptivePortalRoot);
  s_webServer->on("/ncsi.txt", HTTP_GET, handleCaptivePortalRoot);
  s_webServer->on("/connecttest.txt", HTTP_GET, handleCaptivePortalRoot);
  s_webServer->onNotFound(handleCaptivePortalRoot);

  s_webServer->begin();
  s_apModeActive = true;
  Serial.println("[CaptivePortal] SoftAP & WebServer active at http://192.168.4.1 (SSID: ScreenTinker-Setup)");
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
  wifi_power_t pwrEnum = (s_wifiTxPowerLevel == 1) ? WIFI_POWER_15dBm :
                         (s_wifiTxPowerLevel == 2) ? WIFI_POWER_11dBm : WIFI_POWER_19_5dBm;
  WiFi.setTxPower(pwrEnum);
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
  s_lastIp = WiFi.localIP().toString();
  s_lastRssi = WiFi.RSSI();
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
  doc["screen_profile"] = DEVICE_SCREEN_PROFILE;
  doc["screen_width"] = EPD_WIDTH;
  doc["screen_height"] = EPD_HEIGHT;

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

// ─── Diagnostic Log Remote Upload ─────────────────────────────────────────────
bool uploadLogsToServer() {
  if (g_serverUrl.length() == 0 || g_deviceId.length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED) {
    if (!connectWiFi()) return false;
  }

  diag_log_printf(DIAG_LOG_INFO, "Diag", "Uploading %zu diagnostic log entries to server...", diag_log_count());

  HTTPClient http;
  String url = g_serverUrl + "/api/embedded/logs?device_id=" + g_deviceId;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  if (g_deviceToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + g_deviceToken);
  }
  http.addHeader("X-ST-Device-Id", g_deviceId);

  int bat = readBatteryPercent();
  int rssi = WiFi.RSSI();
  std::string payload = diag_log_to_json_string(g_deviceId.c_str(), FIRMWARE_VERSION, bat, rssi);

  int httpCode = http.POST((uint8_t*)payload.c_str(), payload.length());
  bool success = (httpCode >= 200 && httpCode < 300);
  if (success) {
    Serial.printf("[Diag] Logs successfully uploaded (HTTP %d). Clearing buffer.\n", httpCode);
    diag_log_clear();
  } else {
    Serial.printf("[Diag] Log upload failed with HTTP code %d\n", httpCode);
    diag_log_printf(DIAG_LOG_WARN, "Diag", "Log upload failed: HTTP %d", httpCode);
  }
  http.end();
  return success;
}

// ─── Fetch & Render (with PSRAM Caching) ──────────────────────────────────────
void fetchAndRender(bool forceRefresh, int requestedItemIndex) {
  int targetIndex = (requestedItemIndex >= 0) ? requestedItemIndex : s_currentItemIndex;

  // 1. Fetch or re-validate from server over Wi-Fi
  if (!connectWiFi()) {
    checkOtaVerificationFailure("WiFi connection failed");
    int validCount = cacheValidCount();
    if (validCount == 0) {
      Serial.println("[Sync] Wi-Fi offline and cache is empty -> Showing No Wi-Fi screen.");
      showNoWifiScreen(s_currentLang);
    } else {
      int offlineTarget = (requestedItemIndex >= 0) ? requestedItemIndex : getNextCachedItemIndex(s_currentItemIndex);
      if (cacheGet(offlineTarget, s_rotatedBuffer)) {
        s_currentItemIndex = offlineTarget;
        Serial.printf("[Sync] Offline mode: Displaying cached item %d with Offline Badge...\n", offlineTarget);
        rotate_buffer_180(s_rotatedBuffer, s_rawBuffer, EPD_WIDTH, EPD_HEIGHT);
        gfx_draw_offline_badge(s_rawBuffer);
        int battPct = readBatteryPercent();
        bool charging = isBatteryCharging();
        bool usb = isUsbPowerConnected();
        if (battPct >= 0 && battPct < 10 && !charging && !usb) {
          gfx_draw_low_battery_icon(s_rawBuffer, EPD_WIDTH - 210, 14, battPct);
        }
        rotate_buffer_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
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

  const char* reqOrientStr = "landscape";
  int rotDeg = 0;
  if (s_orientationMode == ORIENT_MODE_LANDSCAPE) {
    reqOrientStr = "landscape";
    rotDeg = 0;
  } else if (s_orientationMode == ORIENT_MODE_PORTRAIT) {
    reqOrientStr = "portrait";
    rotDeg = 90;
  } else if (s_orientationMode == ORIENT_MODE_AUTO) {
    if (s_activeOrientation == ORIENTATION_PORTRAIT_90) {
      reqOrientStr = "portrait";
      rotDeg = 90;
    } else if (s_activeOrientation == ORIENTATION_LANDSCAPE_180) {
      reqOrientStr = "landscape-flipped";
      rotDeg = 180;
    } else if (s_activeOrientation == ORIENTATION_PORTRAIT_270) {
      reqOrientStr = "portrait-flipped";
      rotDeg = 270;
    } else {
      reqOrientStr = "landscape";
      rotDeg = 0;
    }
  }

  String url = g_serverUrl + "/api/embedded/render?device_id=" + g_deviceId;
  url += "&orientation=" + String(reqOrientStr);
  url += "&rotation=" + String(rotDeg);
  if (requestedItemIndex >= 0) {
    url += "&item=" + String(requestedItemIndex);
  }
  Serial.printf("[Sync] Fetching %s (orient=%s, rot=%d)\n", url.c_str(), reqOrientStr, rotDeg);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Authorization", String("Bearer ") + g_deviceToken);
  http.addHeader("User-Agent", "ScreenTinker-Sticky/1.2 (ESP32-S3-PSRAM)");
  http.addHeader("X-ST-Firmware-Version", FIRMWARE_VERSION);
  http.addHeader("X-ST-Device-Orientation", reqOrientStr);
  http.addHeader("X-ST-Device-Rotation", String(rotDeg));

  int curRssi = WiFi.RSSI();
  if (curRssi != 0) {
    http.addHeader("X-ST-Device-RSSI", String(curRssi));
  }

  int batt = readBatteryPercent();
  if (batt >= 0) {
    http.addHeader("X-ST-Device-Battery", String(batt));
    Serial.printf("[Sync] Battery: %d%%\n", batt);
  }

  bool usbPower = isUsbPowerConnected();
  bool charging = isBatteryCharging();
  http.addHeader("X-ST-Power-Source", usbPower ? "usb" : "battery");
  http.addHeader("X-ST-Charging", charging ? "1" : "0");
  Serial.printf("[Sync] Power Source: %s (charging=%s)\n", usbPower ? "USB/Mains" : "Battery", charging ? "yes" : "no");

  float tempC = -999.0f;
  float humidityPct = -1.0f;
  if (sht40_read(&tempC, &humidityPct)) {
    http.addHeader("X-ST-Device-Temperature", String(tempC, 1));
    http.addHeader("X-ST-Device-Humidity", String(humidityPct, 1));
    Serial.printf("[Sync] Ambient SHT40: %.1f C, %.1f%% RH\n", tempC, humidityPct);
  }

  String cachedEtag = cacheGetEtag(targetIndex);
  if (cachedEtag.length() == 0 && strlen(rtc_lastEtag) > 0) {
    cachedEtag = String(rtc_lastEtag);
  }
  if (!forceRefresh && !FORCE_REFRESH_ON_BOOT && cachedEtag.length() > 0) {
    http.addHeader("If-None-Match", cachedEtag);
    Serial.printf("[Sync] Sent conditional ETag: %s\n", cachedEtag.c_str());
  }

  const char* headerKeys[] = {
    "ETag", "X-ST-Expires-In", "X-ST-Item-Index", "X-ST-Total-Items",
    "X-ST-Total-Zones", "X-ST-Layout-Fallback", "X-ST-Device-Name",
    "X-ST-Command", "X-ST-Power-State", "X-ST-Screen-Width",
    "X-ST-Screen-Height", "X-ST-Rotation", "X-ST-Orientation",
    "X-ST-Refresh-Mode", "X-ST-Color-Mode"
  };
  http.collectHeaders(headerKeys, 15);

  int httpCode = http.GET();
  Serial.printf("[Sync] HTTP Response: %d\n", httpCode);

  int serverW = 0, serverH = 0, serverRot = -1;
  String orientStr = "";
  if (http.hasHeader("X-ST-Screen-Width")) {
    serverW = http.header("X-ST-Screen-Width").toInt();
  }
  if (http.hasHeader("X-ST-Screen-Height")) {
    serverH = http.header("X-ST-Screen-Height").toInt();
  }
  if (http.hasHeader("X-ST-Rotation")) {
    serverRot = http.header("X-ST-Rotation").toInt();
  }
  if (http.hasHeader("X-ST-Orientation")) {
    orientStr = http.header("X-ST-Orientation");
  }

  bool isPortraitFrame = false;
  if (orientStr.length() > 0) {
    isPortraitFrame = orientStr.startsWith("portrait");
    Serial.printf("[Display] Server orientation: %s, rotation: %d deg (%dx%d)\n",
                  orientStr.c_str(),
                  serverRot,
                  serverW,
                  serverH);
  } else if (s_activeOrientation == ORIENTATION_PORTRAIT_90 || s_activeOrientation == ORIENTATION_PORTRAIT_270) {
    isPortraitFrame = true;
  }

  if (http.hasHeader("X-ST-Command")) {
    String cmd = http.header("X-ST-Command");
    cmd.trim();
    if (cmd.equalsIgnoreCase("reboot")) {
      diag_log_printf(DIAG_LOG_WARN, "Sync", "Remote reboot command received from server! Restarting ESP32...");
      Serial.println("[Sync] Remote reboot command received from server! Restarting ESP32...");
      http.end();
      delay(500);
      ESP.restart();
    } else if (cmd.equalsIgnoreCase("upload_logs") || cmd.equalsIgnoreCase("logs")) {
      diag_log_printf(DIAG_LOG_INFO, "Sync", "Remote upload_logs command received from server!");
      Serial.println("[Sync] Remote upload_logs command received from server!");
      uploadLogsToServer();
    } else if (cmd.startsWith("update") || cmd.startsWith("ota")) {
      int firstSpace = cmd.indexOf(' ');
      if (firstSpace > 0) {
        String rest = cmd.substring(firstSpace + 1);
        rest.trim();
        String url = rest;
        String sha = "";
        int secondSpace = rest.indexOf(' ');
        if (secondSpace > 0) {
          url = rest.substring(0, secondSpace);
          sha = rest.substring(secondSpace + 1);
          sha.trim();
        }
        if (url.startsWith("/")) {
          url = g_serverUrl + url;
        }
        diag_log_printf(DIAG_LOG_WARN, "Sync", "Remote update command received: url=%s", url.c_str());
        Serial.printf("[Sync] Remote update command received: url=%s\n", url.c_str());
        http.end();
        showOtaUpdateScreen("Updating Firmware...", s_currentLang);
        rtc_lastEtag[0] = '\0';
        ota_perform_update(url.c_str(), sha.length() > 0 ? sha.c_str() : nullptr);
        return;
      }
    }
  }

  if (http.hasHeader("X-ST-Power-State")) {
    String pState = http.header("X-ST-Power-State");
    if (pState.length() > 0) {
      Serial.printf("[Power] Server power state: %s\n", pState.c_str());
    }
  }

  if (http.hasHeader("X-ST-Expires-In")) {
    int exp = http.header("X-ST-Expires-In").toInt();
    // Enforce minimum 10s sync interval to protect E-Paper display
    if (exp > 0) s_syncIntervalMs = max((uint32_t)10000, (uint32_t)(exp * 1000));
  }
  int resolvedIndex = s_currentItemIndex;
  if (http.hasHeader("X-ST-Item-Index")) {
    resolvedIndex = http.header("X-ST-Item-Index").toInt();
  }
  if (http.hasHeader("X-ST-Total-Items")) {
    s_totalItems = http.header("X-ST-Total-Items").toInt();
  }
  if (http.hasHeader("X-ST-Power-State")) {
    String pState = http.header("X-ST-Power-State");
    if (pState.length() > 0) {
      Serial.printf("[Power] Server power state: %s\n", pState.c_str());
    }
  }

  String serverRefreshMode = "";
  if (http.hasHeader("X-ST-Refresh-Mode")) {
    serverRefreshMode = http.header("X-ST-Refresh-Mode");
    serverRefreshMode.toLowerCase();
    serverRefreshMode.trim();
  }

  bool useFastPartial = false;
  if (!DEVICE_SUPPORTS_PARTIAL_REFRESH) {
    useFastPartial = false;
    rtc_consecutivePartialCount = 0;
  } else if (forceRefresh || FORCE_REFRESH_ON_BOOT) {
    useFastPartial = false;
    rtc_consecutivePartialCount = 0;
    Serial.println("[EPD] Force refresh active -> performing FULL screen refresh.");
  } else if (serverRefreshMode == "full") {
    useFastPartial = false;
    rtc_consecutivePartialCount = 0;
    Serial.println("[EPD] Server header override (X-ST-Refresh-Mode: full) -> performing FULL screen refresh.");
  } else if (s_refreshMode == REFRESH_MODE_ALWAYS_FULL) {
    useFastPartial = false;
    rtc_consecutivePartialCount = 0;
  } else {
    // Smart Partial Refresh policy
    if (rtc_consecutivePartialCount < s_partialRefreshLimit) {
      useFastPartial = true;
      rtc_consecutivePartialCount++;
      Serial.printf("[EPD] Refresh policy: PARTIAL (%d/%d consecutive)\n", rtc_consecutivePartialCount, s_partialRefreshLimit);
    } else {
      useFastPartial = false;
      rtc_consecutivePartialCount = 0;
      Serial.printf("[EPD] Refresh policy: FULL (Clean cycle after %d partial refreshes)\n", s_partialRefreshLimit);
    }
  }

  if (httpCode == HTTP_CODE_NOT_MODIFIED) {
    rtc_unchangedCycles++;
    Serial.printf("[Sync] 304 Not Modified — Content for item %d unchanged (0 bytes download, consecutive 304s: %d).\n",
                  resolvedIndex, rtc_unchangedCycles);
    http.end();
    disconnectWiFiIfPowerSave();
    ota_confirm_running_partition();
    // If playlist advanced to another item that is in cache, display it without re-downloading
    if (resolvedIndex != s_currentItemIndex && cacheGet(resolvedIndex, s_rotatedBuffer)) {
      s_currentItemIndex = resolvedIndex;
      rtc_currentItemIndex = resolvedIndex;
      Serial.printf("[Sync] Switched display to cached item %d.\n", resolvedIndex);
      int battPct = readBatteryPercent();
      bool charging = isBatteryCharging();
      bool usb = isUsbPowerConnected();
      if (battPct >= 0 && battPct < 10 && !charging && !usb) {
        rotate_buffer_180(s_rotatedBuffer, s_rawBuffer, EPD_WIDTH, EPD_HEIGHT);
        gfx_draw_low_battery_icon(s_rawBuffer, -1, -1, battPct);
        rotate_buffer_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
      }
      renderRotatedBuffer(s_rotatedBuffer, useFastPartial);
    }
    return;
  }

  s_currentItemIndex = resolvedIndex;
  rtc_currentItemIndex = resolvedIndex;

  if (httpCode == HTTP_CODE_OK) {
    rtc_unchangedCycles = 0; // Reset adaptive backoff counter on content update
    String newEtag = http.header("ETag");
    if (newEtag.length() > 0) {
      strncpy(rtc_lastEtag, newEtag.c_str(), sizeof(rtc_lastEtag) - 1);
    }

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
      bool isSwapped = (serverW > 0 && serverH > 0 && (serverW != EPD_WIDTH || serverH != EPD_HEIGHT))
                       || (serverW == 0 && isPortraitFrame && EPD_WIDTH != EPD_HEIGHT);
      uint16_t src_w = (serverW > 0) ? (uint16_t)serverW : (uint16_t)(isPortraitFrame ? EPD_HEIGHT : EPD_WIDTH);
      uint16_t src_h = (serverH > 0) ? (uint16_t)serverH : (uint16_t)(isPortraitFrame ? EPD_WIDTH : EPD_HEIGHT);

      bool flip = false; // Fixed transpose; server handles 180 deg inversion for 270 deg

      int battPct = readBatteryPercent();
      bool charging = isBatteryCharging();
      bool usb = isUsbPowerConnected();
      if (battPct >= 0 && battPct < 10 && !charging && !usb) {
        Serial.printf("[Power] Low battery warning (%d%%)! Drawing low battery icon.\n", battPct);
        gfx_draw_low_battery_icon_native(s_rawBuffer, src_w, src_h, DEVICE_COLOR_MODE, -1, -1, battPct);
      }

      if (isSwapped) {
#if (DEVICE_COLOR_MODE == COLOR_MODE_1BPP_MONO)
        rotate_mono_portrait(s_rawBuffer, s_rotatedBuffer, src_w, src_h, flip);
#elif (DEVICE_COLOR_MODE == COLOR_MODE_SPECTRA6) || (DEVICE_COLOR_MODE == COLOR_MODE_GRAY16)
        rotate_nibble_portrait(s_rawBuffer, s_rotatedBuffer, src_w, src_h, flip);
#else
        rotate_mono_portrait(s_rawBuffer, s_rotatedBuffer, src_w, src_h, flip);
#endif
      } else {
        rotate_buffer_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
      }
      cachePut(s_currentItemIndex, newEtag, s_rotatedBuffer);
      if (renderRotatedBuffer(s_rotatedBuffer, useFastPartial)) {
        ota_confirm_running_partition();
      } else {
        checkOtaVerificationFailure("Display panel refresh failed");
      }
    } else {
      Serial.println("[Sync] Incomplete frame received! Refresh aborted.");
      checkOtaVerificationFailure("Incomplete frame received");
    }

    disconnectWiFiIfPowerSave();
    return;
  }

  if (httpCode == 401) {
    checkOtaVerificationFailure("HTTP 401 Unauthorized");
    String errBody = http.getString();
    Serial.printf("[Sync] HTTP 401 Unauthorized (%s). Device unlinked from server.\n", errBody.c_str());
    Serial.println("[Sync] Resetting device pairing credentials and switching to Pairing Mode...");
    http.end();
    saveConfigToNVS(g_wifiSsid, g_wifiPass, g_serverUrl, "", "", g_multiZone);
    registerAndStartPairing();
    return;
  }

  if (httpCode == 404) {
    String devName = "";
    if (http.hasHeader("X-ST-Device-Name")) {
      devName = http.header("X-ST-Device-Name");
    }
    String errBody = http.getString();
    if (devName.length() == 0 && errBody.length() > 0) {
      JsonDocument errDoc;
      if (!deserializeJson(errDoc, errBody) && errDoc["device_name"].is<String>()) {
        devName = errDoc["device_name"].as<String>();
      }
    }

    Serial.printf("[Sync] HTTP 404 (%s). No playlist assigned to '%s'.\n", errBody.c_str(), devName.c_str());
    http.end();
    disconnectWiFiIfPowerSave();

    // If cache is empty, display clear diagnostic message so screen is not stuck on pairing QR
    if (cacheValidCount() == 0) {
      showEmptyPlaylistScreen(devName.c_str(), s_currentLang);
    }
    ota_confirm_running_partition();
    return;
  }

  if (httpCode == 501) {
    String errBody = http.getString();
    String reason = "Widget rendering requires a browser (set CHROME_PATH).";
    if (errBody.length() > 0) {
      JsonDocument doc;
      if (!deserializeJson(doc, errBody)) {
        if (doc["detail"].is<String>()) reason = doc["detail"].as<String>();
        else if (doc["error"].is<String>()) reason = doc["error"].as<String>();
      }
    }
    Serial.printf("[Sync] HTTP 501 Unsupported Content: %s\n", reason.c_str());
    http.end();
    disconnectWiFiIfPowerSave();

    if (s_rawBuffer && s_rotatedBuffer) {
      renderUnsupportedContentScreen(s_rawBuffer, reason.c_str(), s_currentLang);
      rotate_mono_180(s_rawBuffer, s_rotatedBuffer, EPD_WIDTH, EPD_HEIGHT);
      renderRotatedBuffer(s_rotatedBuffer);
    }
    ota_confirm_running_partition();
    return;
  }

  String errBody = http.getString();
  Serial.printf("[Sync] HTTP Error %d: %s\n", httpCode, errBody.c_str());
  checkOtaVerificationFailure(httpCode <= 0 ? "HTTP connection failed" : "HTTP error response");
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
      Serial.printf("{\"status\":\"ok\",\"v\":1,\"version\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"connected\":%s,\"device_id\":\"%s\",\"multi_zone\":%s,\"psram_free\":%u,\"rssi\":%d}\n",
                    FIRMWARE_VERSION,
                    WiFi.localIP().toString().c_str(), g_wifiSsid.c_str(),
                    WiFi.status() == WL_CONNECTED ? "true" : "false",
                    g_deviceId.c_str(),
                    g_multiZone ? "true" : "false",
                    psramFound() ? ESP.getFreePsram() : 0,
                    WiFi.RSSI());
    } else if (line == "reset") {
      Serial.println("{\"status\":\"ok\",\"v\":1,\"message\":\"NVS wiped. Restarting device.\"}");
      delay(500);
      factoryResetNVS();
    } else if (line == "multizone" || line == "layout") {
      g_multiZone = !g_multiZone;
      s_prefs.begin("screentinker", false);
      s_prefs.putBool("multi_zone", g_multiZone);
      s_prefs.end();
      cacheClear();
      Serial.printf("{\"status\":\"ok\",\"v\":1,\"multi_zone\":%s}\n", g_multiZone ? "true" : "false");
      fetchAndRender(true);
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
    } else if (line == "debug" || line == "imu_debug") {
      s_imuDebugScreen = !s_imuDebugScreen;
      Serial.printf("{\"status\":\"ok\",\"v\":1,\"imu_debug_screen\":%s,\"active_letter\":\"%c\",\"orient\":%d}\n",
                    s_imuDebugScreen ? "true" : "false", 'A' + (s_activeOrientation >= 0 && s_activeOrientation <= 3 ? s_activeOrientation : 0), s_activeOrientation);
      if (s_imuDebugScreen) {
        showImuDebugLetterScreen(s_activeOrientation);
      } else {
        fetchAndRender(true);
      }
    } else if (line == "imu" || line == "accel") {
      int16_t ax = 0, ay = 0, az = 0;
      bool ok = imu_read_accel(&ax, &ay, &az);
      int detected = imu_detect_orientation(s_activeOrientation);
      char letter = 'A' + (s_activeOrientation >= 0 && s_activeOrientation <= 3 ? s_activeOrientation : 0);
      char detLetter = 'A' + (detected >= 0 && detected <= 3 ? detected : 0);
      Serial.printf("{\"status\":\"ok\",\"v\":1,\"sensor\":\"LSM6DS3\",\"ok\":%s,\"ax\":%d,\"ay\":%d,\"az\":%d,\"detected\":%d,\"detected_letter\":\"%c\",\"active\":%d,\"active_letter\":\"%c\",\"mode\":%d,\"debug_screen\":%s}\n",
                    ok ? "true" : "false", ax, ay, az, detected, detLetter, s_activeOrientation, letter, (int)s_orientationMode, s_imuDebugScreen ? "true" : "false");
    } else if (line == "poweroff" || line == "shutdown" || line == "off") {
      powerOffDevice();
    } else if (line == "menu") {
      s_inSystemMenu = true;
      s_inStatusScreen = false;
      s_menuSelection = 0;
      s_menuOpenMillis = millis();
      showSystemMenu(0, false);
    } else if (line == "ota_status" || line == "ota") {
      OtaState st;
      ota_get_state(&st);
      Serial.printf("{\"status\":\"ok\",\"v\":1,\"running\":\"%s\",\"next\":\"%s\",\"rollback_possible\":%s,\"pending_verify\":%s,\"ota_status\":%d}\n",
                    st.running_partition ? st.running_partition : "unknown",
                    st.next_partition ? st.next_partition : "unknown",
                    st.is_rollback_possible ? "true" : "false",
                    st.is_pending_verification ? "true" : "false",
                    (int)st.status);
    } else if (line == "rollback") {
      ota_rollback_and_reboot();
    } else if (line.startsWith("update ") || line.startsWith("ota ")) {
      int firstSpace = line.indexOf(' ');
      String rest = line.substring(firstSpace + 1);
      rest.trim();
      String url = rest;
      String sha = "";
      int secondSpace = rest.indexOf(' ');
      if (secondSpace > 0) {
        url = rest.substring(0, secondSpace);
        sha = rest.substring(secondSpace + 1);
        sha.trim();
      }
      if (url.startsWith("/")) {
        url = g_serverUrl + url;
      }
      showOtaUpdateScreen("Updating Firmware...", s_currentLang);
      ota_perform_update(url.c_str(), sha.length() > 0 ? sha.c_str() : nullptr);
    }
    return;
  }

  int protoVersion = doc["v"] | 0;
  if (protoVersion == 0 || protoVersion > 1) {
    Serial.printf("{\"status\":\"error\",\"v\":1,\"message\":\"Unsupported protocol version: %d\"}\n", protoVersion);
    return;
  }

  const char* cmd = doc["cmd"];
  if (!cmd) return;

  if (strcmp(cmd, "config") == 0 || strcmp(cmd, "setup") == 0) {
    if (!doc["ssid"].is<String>() || doc["ssid"].as<String>().length() == 0) {
      Serial.println("{\"status\":\"error\",\"v\":1,\"message\":\"Missing required field: ssid\"}");
      return;
    }

    String ssid    = doc["ssid"] | "";
    String pass    = doc["pass"] | "";
    String server  = doc["server"] | g_serverUrl;
    String devId   = doc["device_id"] | "";
    String token   = doc["device_token"] | "";
    bool multiZone = !doc["multi_zone"].isNull() ? doc["multi_zone"].as<bool>() : (!doc["multizone"].isNull() ? doc["multizone"].as<bool>() : g_multiZone);

    saveConfigToNVS(ssid, pass, server, devId, token, multiZone);
    Serial.println("{\"status\":\"ok\",\"v\":1,\"message\":\"Configuration saved.\"}");

    if (devId.length() > 0 && token.length() > 0) {
      fetchAndRender(true);
    } else {
      registerAndStartPairing();
    }
  } else if (strcmp(cmd, "status") == 0) {
    float curT = -999.0f, curH = -1.0f;
    bool hasEnv = sht40_read(&curT, &curH);
    Serial.printf("{\"status\":\"ok\",\"v\":1,\"version\":\"%s\",\"ip\":\"%s\",\"connected\":%s,\"device_id\":\"%s\",\"multi_zone\":%s,\"orient_mode\":%d,\"active_orient\":%d,\"temp_c\":%s,\"humidity\":%s,\"psram_free\":%u,\"rssi\":%d}\n",
                  FIRMWARE_VERSION,
                  WiFi.localIP().toString().c_str(),
                  WiFi.status() == WL_CONNECTED ? "true" : "false",
                  g_deviceId.c_str(),
                  g_multiZone ? "true" : "false",
                  (int)s_orientationMode,
                  s_activeOrientation,
                  hasEnv ? String(curT, 1).c_str() : "null",
                  hasEnv ? String(curH, 1).c_str() : "null",
                  psramFound() ? ESP.getFreePsram() : 0,
                  WiFi.RSSI());
  } else if (strcmp(cmd, "sht40") == 0 || strcmp(cmd, "sensors") == 0) {
    float t = 0.0f, h = 0.0f;
    if (sht40_read(&t, &h)) {
      Serial.printf("{\"status\":\"ok\",\"v\":1,\"sensor\":\"SHT40\",\"temp_c\":%.2f,\"humidity\":%.2f}\n", t, h);
    } else {
      Serial.println("{\"status\":\"error\",\"v\":1,\"message\":\"SHT40 read failed or sensor not available\"}");
    }
  } else if (strcmp(cmd, "imu") == 0 || strcmp(cmd, "accel") == 0) {
    int16_t ax = 0, ay = 0, az = 0;
    bool ok = imu_read_accel(&ax, &ay, &az);
    int detected = imu_detect_orientation(s_activeOrientation);
    char letter = 'A' + (s_activeOrientation >= 0 && s_activeOrientation <= 3 ? s_activeOrientation : 0);
    char detLetter = 'A' + (detected >= 0 && detected <= 3 ? detected : 0);
    Serial.printf("{\"status\":\"ok\",\"v\":1,\"sensor\":\"LSM6DS3\",\"ok\":%s,\"ax\":%d,\"ay\":%d,\"az\":%d,\"detected\":%d,\"detected_letter\":\"%c\",\"active\":%d,\"active_letter\":\"%c\",\"mode\":%d,\"debug_screen\":%s}\n",
                  ok ? "true" : "false", ax, ay, az, detected, detLetter, s_activeOrientation, letter, (int)s_orientationMode, s_imuDebugScreen ? "true" : "false");
  } else if (strcmp(cmd, "imu_debug") == 0 || strcmp(cmd, "debug") == 0) {
    if (doc.containsKey("screen")) {
      s_imuDebugScreen = doc["screen"].as<bool>();
    } else {
      s_imuDebugScreen = !s_imuDebugScreen;
    }
    Serial.printf("{\"status\":\"ok\",\"v\":1,\"imu_debug_screen\":%s,\"active_letter\":\"%c\",\"orient\":%d}\n",
                  s_imuDebugScreen ? "true" : "false", 'A' + (s_activeOrientation >= 0 && s_activeOrientation <= 3 ? s_activeOrientation : 0), s_activeOrientation);
    if (s_imuDebugScreen) {
      showImuDebugLetterScreen(s_activeOrientation);
    } else {
      fetchAndRender(true);
    }
  } else if (strcmp(cmd, "logs") == 0 || strcmp(cmd, "dump_logs") == 0) {
    diag_log_dump_stdout();
  } else if (strcmp(cmd, "clear_logs") == 0) {
    diag_log_clear();
    Serial.println("{\"status\":\"ok\",\"v\":1,\"message\":\"Diagnostic logs cleared\"}");
  } else if (strcmp(cmd, "upload_logs") == 0) {
    bool ok = uploadLogsToServer();
    Serial.printf("{\"status\":\"%s\",\"v\":1}\n", ok ? "ok" : "error");
  } else if (strcmp(cmd, "ota_status") == 0) {
    OtaState st;
    ota_get_state(&st);
    Serial.printf("{\"status\":\"ok\",\"v\":1,\"running\":\"%s\",\"next\":\"%s\",\"rollback_possible\":%s,\"pending_verify\":%s,\"ota_status\":%d}\n",
                  st.running_partition ? st.running_partition : "unknown",
                  st.next_partition ? st.next_partition : "unknown",
                  st.is_rollback_possible ? "true" : "false",
                  st.is_pending_verification ? "true" : "false",
                  (int)st.status);
  } else if (strcmp(cmd, "rollback") == 0) {
    ota_rollback_and_reboot();
  } else if (strcmp(cmd, "update") == 0 || strcmp(cmd, "ota") == 0) {
    String url = doc["url"] | "";
    String sha = doc["sha256"] | "";
    if (url.length() > 0) {
      if (url.startsWith("/")) {
        url = g_serverUrl + url;
      }
      showOtaUpdateScreen("Updating Firmware...", s_currentLang);
      bool ok = ota_perform_update(url.c_str(), sha.length() > 0 ? sha.c_str() : nullptr);
      Serial.printf("{\"status\":\"%s\",\"v\":1}\n", ok ? "ok" : "error");
    } else {
      Serial.println("{\"status\":\"error\",\"v\":1,\"message\":\"Missing required field: url\"}");
    }
  } else if (strcmp(cmd, "set_orientation") == 0 || strcmp(cmd, "orientation") == 0) {
    if (!doc["mode"].isNull()) {
      const char* m = doc["mode"].as<const char*>();
      if (strcasecmp(m, "auto") == 0 && s_hasImu) s_orientationMode = ORIENT_MODE_AUTO;
      else if (strcasecmp(m, "portrait") == 0) s_orientationMode = ORIENT_MODE_PORTRAIT;
      else if (strcasecmp(m, "landscape") == 0) s_orientationMode = ORIENT_MODE_LANDSCAPE;
    } else {
      if (s_hasImu) {
        s_orientationMode = (OrientationMode)(((int)s_orientationMode + 1) % 3);
      } else {
        s_orientationMode = (s_orientationMode == ORIENT_MODE_LANDSCAPE) ? ORIENT_MODE_PORTRAIT : ORIENT_MODE_LANDSCAPE;
      }
    }
    s_prefs.begin("screentinker", false);
    s_prefs.putUChar("orient_mode", (uint8_t)s_orientationMode);
    s_prefs.end();
    if (s_orientationMode == ORIENT_MODE_LANDSCAPE) s_activeOrientation = ORIENTATION_LANDSCAPE_0;
    else if (s_orientationMode == ORIENT_MODE_PORTRAIT) s_activeOrientation = ORIENTATION_PORTRAIT_90;
    else if (s_orientationMode == ORIENT_MODE_AUTO && s_hasImu) {
      int detected = imu_detect_orientation(s_activeOrientation);
      if (detected >= 0) s_activeOrientation = detected;
    }
    Serial.printf("{\"status\":\"ok\",\"v\":1,\"orientation_mode\":%d,\"active_orient\":%d}\n", (int)s_orientationMode, s_activeOrientation);
    fetchAndRender(true);
  } else if (strcmp(cmd, "set_multizone") == 0 || strcmp(cmd, "multizone") == 0) {
    if (!doc["enabled"].isNull()) {
      g_multiZone = doc["enabled"].as<bool>();
    } else if (!doc["multi_zone"].isNull()) {
      g_multiZone = doc["multi_zone"].as<bool>();
    } else {
      g_multiZone = !g_multiZone;
    }
    s_prefs.begin("screentinker", false);
    s_prefs.putBool("multi_zone", g_multiZone);
    s_prefs.end();
    cacheClear();
    Serial.printf("{\"status\":\"ok\",\"v\":1,\"multi_zone\":%s}\n", g_multiZone ? "true" : "false");
    fetchAndRender(true);
  } else if (strcmp(cmd, "reset") == 0) {
    Serial.println("{\"status\":\"ok\",\"v\":1,\"message\":\"NVS wiped. Restarting device.\"}");
    delay(500);
    factoryResetNVS();
  } else if (strcmp(cmd, "refresh") == 0) {
    fetchAndRender(true, s_currentItemIndex);
  } else if (strcmp(cmd, "instructions") == 0) {
    showOnboardingScreen(s_currentLang);
  } else if (strcmp(cmd, "nowifi") == 0) {
    showNoWifiScreen(s_currentLang);
  } else if (strcmp(cmd, "pair") == 0) {
    registerAndStartPairing();
  } else if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) {
    powerOffDevice();
  } else if (strcmp(cmd, "menu") == 0) {
    s_inSystemMenu = true;
    s_inStatusScreen = false;
    s_menuSelection = 0;
    s_menuOpenMillis = millis();
    showSystemMenu(0, false);
  } else {
    Serial.printf("{\"status\":\"error\",\"v\":1,\"message\":\"Unknown command: %s\"}\n", cmd);
  }
}

// ─── Arduino Setup & Main Loop ────────────────────────────────────────────────
void setup() {
  s_bootMillis = millis();
  initBoardPowerAndPins();
  Serial.begin(115200);
  delay(100);

  diag_log_init();
  diag_log_printf(DIAG_LOG_INFO, "Boot", "ScreenTinker MCU v%s (%s) booting...", FIRMWARE_VERSION, DEVICE_PROFILE_NAME);
  ota_init();

  OtaState otaState;
  ota_get_state(&otaState);
  if (otaState.is_pending_verification) {
    rtc_lastEtag[0] = '\0';
    Serial.println("[OTA] Firmware pending verification: cleared rtc_lastEtag to enforce full frame HTTP 200 refresh.");
  }

  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
  if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    rtc_consecutivePartialCount = 0;
  }

  initPsramBuffers();

  ensureI2C();
  if (sht40_init()) {
    Serial.println("[SHT40] Sensirion SHT40 temperature & humidity sensor initialized.");
  } else {
    Serial.println("[SHT40] Sensor not detected on I2C bus.");
  }

#if DEVICE_SUPPORTS_IMU
  if (imu_init()) {
    s_hasImu = true;
    Serial.println("[IMU] LSM6DS3TR-C 6-axis IMU detected & initialized.");
  } else {
    s_hasImu = false;
    Serial.println("[IMU] LSM6DS3TR-C not detected on I2C bus.");
  }
#else
  s_hasImu = false;
#endif

  loadConfigFromNVS();

  if (s_hasImu && (s_orientationMode == ORIENT_MODE_AUTO || s_imuDebugScreen)) {
    int detected = imu_detect_orientation(s_activeOrientation);
    if (detected >= 0) {
      s_activeOrientation = detected;
      Serial.printf("[Orientation] Initial IMU orientation detected: %d ('%c')\n",
                    s_activeOrientation, 'A' + (s_activeOrientation >= 0 && s_activeOrientation <= 3 ? s_activeOrientation : 0));
    }
  }

  if (s_imuDebugScreen) {
    s_orientationMode = ORIENT_MODE_AUTO;
    Serial.println("\n[IMU DEBUG] Debug Screen Mode ACTIVE — displaying Letter Screen (A, B, C, D)...");
    showImuDebugLetterScreen(s_activeOrientation);
    return;
  }

  bool isConfigured = (g_wifiSsid.length() > 0 && g_wifiSsid != "Your-WiFi-SSID");
  bool isPaired = (isConfigured && g_deviceId.length() > 0 && g_deviceToken.length() > 0 && g_deviceId != "your-device-uuid");

  if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1 || wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
    // ─── Button Wakeup: Instant Responsiveness & Open System Menu ───
    Serial.println("\n>>> [Wakeup] Woken by Button Press! Opening System Menu in Interactive Mode...");
    rtc_unchangedCycles = 0; // Reset adaptive backoff on user action
    s_inInteractiveMode = true;
    s_interactiveUntilMillis = millis() + 30000;
    s_inSystemMenu = true;
    s_inStatusScreen = false;
    s_menuSelection = 0;
    s_menuOpenMillis = millis();
    showSystemMenu(0, false);
    return;
  }

  if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
    // ─── Scheduled Background Timer Wakeup ───
    Serial.printf("\n>>> [Wakeup] Woken by RTC Timer (Adaptive 304 Count: %d). Running background sync...\n", rtc_unchangedCycles);
    s_inInteractiveMode = false;
    if (isConfigured && isPaired) {
      s_currentItemIndex = rtc_currentItemIndex;
      fetchAndRender(false);

      // Adaptive Sleep Calculation
      uint32_t baseSec = max((uint32_t)10, (uint32_t)(s_syncIntervalMs / 1000));
      uint32_t sleepSec = baseSec;

      // Only apply the 5m-30m adaptive multiplier when the server's base interval is short (< 30 min).
      // When the server explicitly instructed a long sleep (e.g. overnight power schedule or long dwell),
      // obey the server's instructed sleep interval directly instead of clamping it down.
      if (baseSec < 1800) {
        if (rtc_unchangedCycles >= 10) {
          sleepSec = min((uint32_t)1800, baseSec * 10); // up to 30 min
        } else if (rtc_unchangedCycles >= 6) {
          sleepSec = min((uint32_t)600, baseSec * 4);  // up to 10 min
        } else if (rtc_unchangedCycles >= 3) {
          sleepSec = min((uint32_t)300, baseSec * 2);  // up to 5 min
        }
      }
      rtc_lastSyncIntervalS = sleepSec;
      scheduleNextSync(sleepSec);
      if (!isUsbPowerConnected()) return;
    }
  }

  // ─── Cold Boot / Reset ───
  Serial.println("\n=============================================");
  Serial.printf("  ScreenTinker v%s — Seeed Studio reTerminal Sticky\n", FIRMWARE_VERSION);
  Serial.println("  Adaptive Deep Sleep & Ultra-Low-Power Engine");
  Serial.println("=============================================");

  rtc_unchangedCycles = 0;
  rtc_consecutivePartialCount = 0;
  s_inInteractiveMode = false;

  if (isConfigured) {
    showSplashScreen(FIRMWARE_VERSION, (s_currentLang == LANG_DE) ? "Verbinde mit WLAN & Server..." : "Connecting to network...");
    if (isPaired) {
      Serial.println("[Setup] Device configured and paired. Syncing content...");
      fetchAndRender(true);

      // Enter initial deep sleep for slide interval
      uint32_t baseSec = max((uint32_t)10, (uint32_t)(s_syncIntervalMs / 1000));
      rtc_lastSyncIntervalS = baseSec;
      scheduleNextSync(baseSec);
      if (!isUsbPowerConnected()) return;
    } else {
      Serial.println("\n[Setup] Wi-Fi configured, but device not paired. Starting 6-digit Pairing flow...");
      registerAndStartPairing();
    }
  } else {
    Serial.println("\n[Setup] No Wi-Fi credentials configured.");
    Serial.println("[Setup] Displaying Onboarding instructions on E-Paper...");
    showOnboardingScreen(s_currentLang);
    startCaptivePortal();
    Serial.println("[Setup] Options: Connect to Wi-Fi 'ScreenTinker-Setup' OR send JSON via Web-Flasher.");
  }

  s_lastSyncMillis = millis();
}

void loop() {
  if (!s_apModeActive) {
    OtaState otaCheck;
    ota_get_state(&otaCheck);
    if (otaCheck.is_pending_verification && (s_otaVerifyFailures >= 3 || ((millis() - s_bootMillis > 90000) && s_otaVerifyFailures > 0))) {
      Serial.println("[OTA] Critical: Pending verification failed (>=3 failures or >90s elapsed on USB). Rolling back...");
      ota_rollback_and_reboot();
    }
  }

  // Handle Captive Portal DNS & Web requests in AP mode
  if (s_apModeActive) {
    s_dnsServer.processNextRequest();
    if (s_webServer) s_webServer->handleClient();
  }

  // Handle pending config save from Captive Portal
  if (s_pendingConfigSave && (millis() - s_pendingSaveMillis > 800)) {
    s_pendingConfigSave = false;
    Serial.println("[CaptivePortal] Applying pending configuration...");
    s_orientationMode = (OrientationMode)s_pendingOrient;
    if (s_orientationMode == ORIENT_MODE_LANDSCAPE) {
      s_activeOrientation = ORIENTATION_LANDSCAPE_0;
    } else if (s_orientationMode == ORIENT_MODE_PORTRAIT) {
      s_activeOrientation = ORIENTATION_PORTRAIT_90;
    } else if (s_orientationMode == ORIENT_MODE_AUTO && s_hasImu) {
      int detected = imu_detect_orientation(s_activeOrientation);
      if (detected >= 0) s_activeOrientation = detected;
    }
    s_prefs.begin("screentinker", false);
    s_prefs.putUChar("orient_mode", (uint8_t)s_orientationMode);
    s_prefs.end();
    saveConfigToNVS(s_pendingSsid, s_pendingPass, s_pendingServer, "", "", s_pendingMultiZone);
    registerAndStartPairing();
  }

  // Check for incoming serial configuration commands
  if (Serial.available() > 0) {
    s_inInteractiveMode = true;
    s_interactiveUntilMillis = millis() + 30000;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      processSerialLine(line);
    }
  }

  bool isConfigured = (g_wifiSsid.length() > 0 && g_wifiSsid != "Your-WiFi-SSID");
  bool isPaired = (isConfigured && g_deviceId.length() > 0 && g_deviceToken.length() > 0 && g_deviceId != "your-device-uuid");

  // If Wi-Fi is configured but device is not paired yet, poll pairing status periodically (NOT in menu)
  static uint32_t lastPairCheckMillis = 0;
  if (!s_inSystemMenu && !s_inStatusScreen && isConfigured && !isPaired) {
    if (s_activePairingDeviceId.length() == 0 && (millis() - lastPairCheckMillis > 6000)) {
      lastPairCheckMillis = millis();
      registerAndStartPairing();
    } else if (s_activePairingDeviceId.length() > 0 && (millis() - lastPairCheckMillis > 3500)) {
      lastPairCheckMillis = millis();
      if (checkPairingStatus()) {
        fetchAndRender(true);
        if (isConfigured && isPaired) {
          uint32_t baseSec = max((uint32_t)10, (uint32_t)(s_syncIntervalMs / 1000));
          scheduleNextSync(baseSec);
        }
      }
    }
  }

  // ─── USB Active Mains Mode: Background sync without deep sleep ───────────────
  if (!s_imuDebugScreen && isUsbPowerConnected() && isConfigured && isPaired && !s_inSystemMenu && !s_inStatusScreen && !s_inInteractiveMode) {
    if (s_nextSyncMillis > 0 && millis() >= s_nextSyncMillis) {
      Serial.println("\n>>> [Power] USB Connected: Triggering active periodic frame sync...");
      s_nextSyncMillis = 0;
      fetchAndRender(false);
      uint32_t baseSec = max((uint32_t)10, (uint32_t)(s_syncIntervalMs / 1000));
      scheduleNextSync(baseSec);
    }
  }

  // ─── IMU Orientation Debounced Auto-Detection ───────────────────────────────
  if (s_hasImu && (s_orientationMode == ORIENT_MODE_AUTO || s_imuDebugScreen) && !s_inSystemMenu && !s_inStatusScreen) {
    if (millis() - s_lastImuCheckMillis >= 250) {
      s_lastImuCheckMillis = millis();
      int detected = imu_detect_orientation(s_activeOrientation);
      if (detected >= 0 && detected != s_activeOrientation) {
        if (s_pendingOrientation != detected) {
          s_pendingOrientation = detected;
          s_pendingOrientationStart = millis();
        } else if (millis() - s_pendingOrientationStart >= 350) {
          s_activeOrientation = s_pendingOrientation;
          s_pendingOrientation = -1;

          char letter = 'A' + (s_activeOrientation >= 0 && s_activeOrientation <= 3 ? s_activeOrientation : 0);
          int16_t dax = 0, day = 0, daz = 0;
          imu_read_accel(&dax, &day, &daz);
          Serial.println("\n************************************************************");
          Serial.printf(">>> [IMU DEBUG] ACTIVE ORIENTATION CHANGED -> LETTER '%c'\n", letter);
          Serial.printf(">>> [IMU DEBUG] ID: %d, ax=%d, ay=%d, az=%d\n", s_activeOrientation, (int)dax, (int)day, (int)daz);
          Serial.println("************************************************************\n");

          if (s_imuDebugScreen) {
            showImuDebugLetterScreen(s_activeOrientation);
          } else if (isConfigured && isPaired) {
            fetchAndRender(true);
          }
        }
      } else {
        s_pendingOrientation = -1;
      }
    }
  }

  // ─── Button Handling (Instant Responsiveness) ─────────────────────────────────
  static uint32_t lastBtnUp = 0;
  static uint32_t lastBtnDown = 0;
  static uint32_t okPressStart = 0;

  // Track OK Button state
  if (digitalRead(PIN_BTN_OK) == LOW) {
    if (okPressStart == 0) okPressStart = millis();

    // If held for 1.5 seconds while outside menu -> Open System Menu immediately!
    if (!s_inSystemMenu && !s_inStatusScreen && okPressStart > 0 && (millis() - okPressStart >= 1500)) {
      Serial.println("\n>>> [Button] Long press (1.5s) on OK detected -> Opening System Menu!");
      okPressStart = 0; // Reset so it doesn't re-trigger while held
      s_inInteractiveMode = true;
      s_interactiveUntilMillis = millis() + 30000;
      s_inSystemMenu = true;
      s_inStatusScreen = false;
      s_menuSelection = 0; // Default: 1. Status & Info
      s_menuOpenMillis = millis();
      showSystemMenu(s_menuSelection, false);
    }
  } else {
    // Button released
    if (okPressStart > 0) {
      uint32_t duration = millis() - okPressStart;
      okPressStart = 0;

      // Valid Short Press (between 40ms and 1400ms)
      if (duration > 40 && duration < 1400) {
        s_inInteractiveMode = true;
        s_interactiveUntilMillis = millis() + 30000;

        if (s_inStatusScreen) {
          // Inside Status Screen: Return to Menu
          Serial.println("\n>>> [StatusScreen] Return to System Menu");
          s_inStatusScreen = false;
          s_inSystemMenu = true;
          s_menuOpenMillis = millis();
          showSystemMenu(s_menuSelection, false);
        } else if (s_inSystemMenu) {
          // Inside System Menu: Execute selection!
          Serial.printf("\n>>> [SystemMenu] Selection %d confirmed!\n", s_menuSelection);
          s_menuOpenMillis = millis();
          if (s_menuSelection == 0) {
            // 0: Status & Info
            s_inSystemMenu = false;
            s_inStatusScreen = true;
            showStatusInfoScreen();
          } else if (s_menuSelection == 1) {
            // 1: Orientation Mode Cycle
            if (s_hasImu) {
              s_orientationMode = (OrientationMode)(((int)s_orientationMode + 1) % 3);
            } else {
              s_orientationMode = (s_orientationMode == ORIENT_MODE_LANDSCAPE) ? ORIENT_MODE_PORTRAIT : ORIENT_MODE_LANDSCAPE;
            }
            s_prefs.begin("screentinker", false);
            s_prefs.putUChar("orient_mode", (uint8_t)s_orientationMode);
            s_prefs.end();
            rtc_consecutivePartialCount = 0;

            if (s_orientationMode == ORIENT_MODE_LANDSCAPE) {
              s_activeOrientation = ORIENTATION_LANDSCAPE_0;
            } else if (s_orientationMode == ORIENT_MODE_PORTRAIT) {
              s_activeOrientation = ORIENTATION_PORTRAIT_90;
            } else if (s_orientationMode == ORIENT_MODE_AUTO && s_hasImu) {
              int detected = imu_detect_orientation(s_activeOrientation);
              if (detected >= 0) s_activeOrientation = detected;
            }

            Serial.printf("\n>>> [SystemMenu] Orientation mode changed to: %d (%s)!\n",
                          (int)s_orientationMode,
                          (s_orientationMode == ORIENT_MODE_AUTO) ? "Auto (IMU)" : (s_orientationMode == ORIENT_MODE_PORTRAIT) ? "Portrait" : "Landscape");
            showSystemMenu(s_menuSelection, true); // Fast Partial Refresh!
          } else if (s_menuSelection == 2) {
            // 2: Wi-Fi TX Power Toggle (MAX -> MED -> LOW -> MAX)
            s_wifiTxPowerLevel = (s_wifiTxPowerLevel + 1) % 3;
            s_prefs.begin("screentinker", false);
            s_prefs.putUChar("wifi_tx_pwr", s_wifiTxPowerLevel);
            s_prefs.end();
            wifi_power_t pwrEnum = (s_wifiTxPowerLevel == 1) ? WIFI_POWER_15dBm :
                                   (s_wifiTxPowerLevel == 2) ? WIFI_POWER_11dBm : WIFI_POWER_19_5dBm;
            WiFi.setTxPower(pwrEnum);
            Serial.printf("\n>>> [SystemMenu] Wi-Fi TX Power switched to level %d (%s)!\n",
                          s_wifiTxPowerLevel, (s_wifiTxPowerLevel == 1) ? "15 dBm" : (s_wifiTxPowerLevel == 2) ? "11 dBm" : "19.5 dBm");
            showSystemMenu(s_menuSelection, true); // Fast Partial Refresh!
          } else if (s_menuSelection == 3) {
            // 3: Refresh Mode Toggle (Smart Partial [6:1] <-> Always Full)
            if (DEVICE_SUPPORTS_PARTIAL_REFRESH) {
              s_refreshMode = (s_refreshMode == REFRESH_MODE_SMART_PARTIAL) ? REFRESH_MODE_ALWAYS_FULL : REFRESH_MODE_SMART_PARTIAL;
              s_prefs.begin("screentinker", false);
              s_prefs.putUChar("refresh_mode", s_refreshMode);
              s_prefs.end();
              rtc_consecutivePartialCount = 0;
              Serial.printf("\n>>> [SystemMenu] Refresh Mode changed to: %s\n",
                            (s_refreshMode == REFRESH_MODE_SMART_PARTIAL) ? "Smart Partial [6:1]" : "Always Full");
            } else {
              Serial.println("\n>>> [SystemMenu] This device target only supports Full Refresh (Spectra 6).");
            }
            showSystemMenu(s_menuSelection, true); // Fast Partial Refresh!
          } else if (s_menuSelection == 4) {
            // 4: Language Toggle (EN <-> DE)
            s_currentLang = (s_currentLang == LANG_EN) ? LANG_DE : LANG_EN;
            s_prefs.begin("screentinker", false);
            s_prefs.putUChar("lang", (uint8_t)s_currentLang);
            s_prefs.end();
            Serial.printf("\n>>> [SystemMenu] Language changed to: %s\n", (s_currentLang == LANG_DE) ? "DE" : "EN");
            showSystemMenu(s_menuSelection, true); // Fast Partial Refresh!
          } else if (s_menuSelection == 5) {
            // 5: Gerät entkoppeln (Unpair)
            s_inSystemMenu = false;
            Serial.println("[SystemMenu] Unpairing device and generating new pairing code...");
            saveConfigToNVS(g_wifiSsid, g_wifiPass, g_serverUrl, "", "");
            registerAndStartPairing();
          } else if (s_menuSelection == 6) {
            // 6: Ausschalten / Standby
            powerOffDevice();
          } else if (s_menuSelection == 7) {
            // 7: Factory Reset
            s_inSystemMenu = false;
            factoryResetNVS();
          } else if (s_menuSelection == 8) {
            // 8: Zurück / Schließen -> Deep Sleep
            s_inSystemMenu = false;
            s_inInteractiveMode = false;
            redrawCurrentState();
            if (isConfigured && isPaired) {
              uint32_t baseSec = max((uint32_t)10, (uint32_t)(s_syncIntervalMs / 1000));
              scheduleNextSync(baseSec);
            }
          }
        } else {
          // Outside Menu: Normal Short Press -> Open menu or refresh
          if (!isConfigured) {
            showOnboardingScreen(s_currentLang);
          } else if (!isPaired) {
            Serial.println("\n>>> [Button] OK pressed -> Refreshing pairing code...");
            registerAndStartPairing();
          } else {
            Serial.println("\n>>> [Button] OK pressed -> Opening System Menu!");
            s_inSystemMenu = true;
            s_inStatusScreen = false;
            s_menuSelection = 0;
            s_menuOpenMillis = millis();
            showSystemMenu(s_menuSelection, false);
          }
        }
      }
    }
  }

  // Interactive / Menu Inactivity Timeout: auto-exit to Deep Sleep after 30 seconds
  if (s_inInteractiveMode && (millis() > s_interactiveUntilMillis || ((s_inSystemMenu || s_inStatusScreen) && (millis() - s_menuOpenMillis > 30000)))) {
    Serial.println("\n>>> [Interactive] Inactivity timeout (30s) -> Exiting interactive mode to Deep Sleep.");
    s_inInteractiveMode = false;
    s_inSystemMenu = false;
    s_inStatusScreen = false;
    redrawCurrentState();
    if (isConfigured && isPaired) {
      uint32_t baseSec = max((uint32_t)10, (uint32_t)(s_syncIntervalMs / 1000));
      scheduleNextSync(baseSec);
    }
  }

  // UP Button (GPIO 5)
  if (digitalRead(PIN_BTN_UP) == LOW && millis() - lastBtnUp > 200) {
    lastBtnUp = millis();
    s_inInteractiveMode = true;
    s_interactiveUntilMillis = millis() + 30000;

    if (s_inStatusScreen) {
      s_inStatusScreen = false;
      s_inSystemMenu = true;
      s_menuOpenMillis = millis();
      showSystemMenu(s_menuSelection, false);
    } else if (s_inSystemMenu) {
      s_menuSelection = (s_menuSelection - 1 + 9) % 9;
      s_menuOpenMillis = millis();
      Serial.printf("\n>>> [SystemMenu] UP pressed -> Selection: %d\n", s_menuSelection);
      showSystemMenu(s_menuSelection, true); // Fast Partial Refresh!
    } else if (isPaired) {
      int prev = (s_totalItems > 0) ? ((s_currentItemIndex - 1 + s_totalItems) % s_totalItems) : max(0, s_currentItemIndex - 1);
      Serial.printf("\n>>> [Button] UP pressed -> Switching to previous slide %d...\n", prev);
      fetchAndRender(true, prev);
    }
  }

  // DOWN Button (GPIO 6)
  if (digitalRead(PIN_BTN_DOWN) == LOW && millis() - lastBtnDown > 200) {
    lastBtnDown = millis();
    s_inInteractiveMode = true;
    s_interactiveUntilMillis = millis() + 30000;

    if (s_inStatusScreen) {
      s_inStatusScreen = false;
      s_inSystemMenu = true;
      s_menuOpenMillis = millis();
      showSystemMenu(s_menuSelection, false);
    } else if (s_inSystemMenu) {
      s_menuSelection = (s_menuSelection + 1) % 9;
      s_menuOpenMillis = millis();
      Serial.printf("\n>>> [SystemMenu] DOWN pressed -> Selection: %d\n", s_menuSelection);
      showSystemMenu(s_menuSelection, true); // Fast Partial Refresh!
    } else if (isPaired) {
      int next = (s_totalItems > 0) ? ((s_currentItemIndex + 1) % s_totalItems) : (s_currentItemIndex + 1);
      Serial.printf("\n>>> [Button] DOWN pressed -> Switching to next slide %d...\n", next);
      fetchAndRender(true, next);
    }
  }

  // Periodic Auto-Sync (Fallback if active in loop)
  if (!s_imuDebugScreen && !s_inSystemMenu && !s_inStatusScreen && isPaired && (millis() - s_lastSyncMillis > s_syncIntervalMs)) {
    s_lastSyncMillis = millis();
    Serial.println("\n[AutoSync] Periodic sync triggered...");
    fetchAndRender(false);
  }

  delay(20);
}
