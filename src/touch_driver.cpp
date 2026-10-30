#include "touch_driver.h"
#include "device_profile.h"
#include "menu_layout.h"

// ─── Universal Geometry Hit-Testing ──────────────────────────────────────────

int touch_hit_test_menu(int x, int y, int screen_width, int screen_height) {
  if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) {
    return -1;
  }

  const bool isCompact = (screen_height < 400);
  const int footerH    = isCompact ? MENU_COMPACT_FOOTER_HEIGHT : MENU_FOOTER_HEIGHT;
  const int marginX    = isCompact ? MENU_COMPACT_ITEM_MARGIN_X : MENU_ITEM_MARGIN_X;
  const int startY     = isCompact ? MENU_COMPACT_START_Y : MENU_START_Y;
  const int itemH      = isCompact ? MENU_COMPACT_ITEM_HEIGHT : MENU_ITEM_HEIGHT;
  const int pitch      = isCompact ? MENU_COMPACT_ITEM_PITCH : MENU_ITEM_PITCH;

  // Footer Banner: y >= (screen_height - footerH)
  // Tapping anywhere in the footer banner corresponds to item 8 (BACK / CLOSE)
  if (y >= screen_height - footerH) {
    return 8;
  }

  const int itemW = screen_width - (2 * marginX);

  // Boxes span x in [marginX, marginX + itemW) like gfx_fill_rect
  if (x < marginX || x >= (marginX + itemW)) {
    return -1;
  }
  if (y < startY) {
    return -1;
  }

  int row = (y - startY) / pitch;
  int offset = (y - startY) % pitch;

  // offset <= itemH allows an intentional 1px touch tolerance at row bottom border
  if (row >= 0 && row < MENU_ITEM_COUNT && offset <= itemH) {
    return row;
  }

  return -1;
}

bool touch_hit_test_status(int x, int y, int screen_width, int screen_height) {
  return (x >= 0 && x < screen_width && y >= 0 && y < screen_height);
}

// ─── Hardware Driver (Arduino & ESP32) ───────────────────────────────────────

#if defined(ARDUINO) && (DEVICE_SUPPORTS_TOUCH == 1)

#include <Arduino.h>
#include <Wire.h>

#define TOUCH_CHIP_NONE    0
#define TOUCH_CHIP_GT911   1
#define TOUCH_CHIP_FT6336U 2
#define TOUCH_CHIP_XPT2046 3

#if defined(PIN_TOUCH_CS) && (PIN_TOUCH_CS >= 0) && defined(PIN_TOUCH_SCK) && (PIN_TOUCH_SCK >= 0)
  #define HAS_XPT2046_TOUCH 1
  #include <SPI.h>
  #include <XPT2046_Touchscreen.h>
  static XPT2046_Touchscreen* s_xpt = nullptr;
#endif

#if defined(PIN_TOUCH_SDA) && defined(PIN_I2C_SDA) && (PIN_TOUCH_SDA != PIN_I2C_SDA)
  #define TOUCH_WIRE Wire1
#else
  #define TOUCH_WIRE Wire
#endif

static uint8_t s_touchChip = TOUCH_CHIP_NONE;
static uint8_t s_touchAddr = 0;
static bool s_initialized = false;
static bool s_sleeping = false;

// GT911 Helper: Write single byte register
static bool gt911_write(uint16_t reg, uint8_t val) {
  TOUCH_WIRE.beginTransmission(s_touchAddr);
  TOUCH_WIRE.write((uint8_t)(reg >> 8));
  TOUCH_WIRE.write((uint8_t)(reg & 0xFF));
  TOUCH_WIRE.write(val);
  return (TOUCH_WIRE.endTransmission() == 0);
}

// GT911 Helper: Read multi-byte registers
static bool gt911_read(uint16_t reg, uint8_t* buf, size_t len) {
  TOUCH_WIRE.beginTransmission(s_touchAddr);
  TOUCH_WIRE.write((uint8_t)(reg >> 8));
  TOUCH_WIRE.write((uint8_t)(reg & 0xFF));
  if (TOUCH_WIRE.endTransmission(false) != 0) return false;
  if (TOUCH_WIRE.requestFrom((uint8_t)s_touchAddr, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) {
    buf[i] = TOUCH_WIRE.read();
  }
  return true;
}

// FT6336U Helper: Read multi-byte registers
static bool ft6336_read(uint8_t reg, uint8_t* buf, size_t len) {
  TOUCH_WIRE.beginTransmission(s_touchAddr);
  TOUCH_WIRE.write(reg);
  if (TOUCH_WIRE.endTransmission(false) != 0) return false;
  if (TOUCH_WIRE.requestFrom((uint8_t)s_touchAddr, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) {
    buf[i] = TOUCH_WIRE.read();
  }
  return true;
}

bool touch_init() {
  if (s_initialized) return true;

#if defined(HAS_XPT2046_TOUCH)
  if (PIN_TOUCH_CS >= 0) {
    Serial.printf("[Touch] Initializing XPT2046 SPI Touch (SCK=%d, MISO=%d, MOSI=%d, CS=%d, INT=%d)...\n",
                  PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS, PIN_TOUCH_INT);
    SPI.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    s_xpt = new XPT2046_Touchscreen(PIN_TOUCH_CS, (PIN_TOUCH_INT >= 0) ? PIN_TOUCH_INT : 255);
    if (s_xpt && s_xpt->begin()) {
      s_xpt->setRotation(0);
      s_touchChip = TOUCH_CHIP_XPT2046;
      s_initialized = true;
      Serial.println("[Touch] XPT2046 Resistive Touch Controller initialized successfully.");
      return true;
    } else {
      Serial.println("[Touch] XPT2046 initialization failed.");
      return false;
    }
  }
#endif

#ifdef PIN_TOUCH_EN
  gpio_hold_dis((gpio_num_t)PIN_TOUCH_EN);
  pinMode(PIN_TOUCH_EN, OUTPUT);
  digitalWrite(PIN_TOUCH_EN, HIGH);
  delay(10);
#endif

#ifdef PIN_TOUCH_RST
  gpio_hold_dis((gpio_num_t)PIN_TOUCH_RST);
  pinMode(PIN_TOUCH_RST, OUTPUT);
#endif

#ifdef PIN_TOUCH_INT
  gpio_hold_dis((gpio_num_t)PIN_TOUCH_INT);
  pinMode(PIN_TOUCH_INT, OUTPUT);
  digitalWrite(PIN_TOUCH_INT, LOW);
#endif

  // GT911 reset timing for 0x5D slave address selection:
  // Hold INT LOW while RST transitions from LOW to HIGH, then hold INT LOW 50ms to synchronize scanning
#ifdef PIN_TOUCH_RST
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(20);
  digitalWrite(PIN_TOUCH_INT, LOW);
  delay(1);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(10);
  digitalWrite(PIN_TOUCH_INT, LOW);
  delay(50);
#endif

#ifdef PIN_TOUCH_INT
  pinMode(PIN_TOUCH_INT, INPUT);
  delay(10);
#endif

  // Initialize I2C interface at 100 kHz standard mode with pullups
#if defined(PIN_TOUCH_SDA) && defined(PIN_TOUCH_SCL)
  pinMode(PIN_TOUCH_SDA, INPUT_PULLUP);
  pinMode(PIN_TOUCH_SCL, INPUT_PULLUP);
  TOUCH_WIRE.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 100000);
#elif defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL)
  TOUCH_WIRE.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
#else
  return false;
#endif

  // Probe GT911 primary address 0x5D
  TOUCH_WIRE.beginTransmission(0x5D);
  if (TOUCH_WIRE.endTransmission() == 0) {
    s_touchChip = TOUCH_CHIP_GT911;
    s_touchAddr = 0x5D;
  } else {
    // Probe GT911 alternate address 0x14
    TOUCH_WIRE.beginTransmission(0x14);
    if (TOUCH_WIRE.endTransmission() == 0) {
      s_touchChip = TOUCH_CHIP_GT911;
      s_touchAddr = 0x14;
    } else {
      // Probe FT6336U address 0x38
      TOUCH_WIRE.beginTransmission(0x38);
      if (TOUCH_WIRE.endTransmission() == 0) {
        s_touchChip = TOUCH_CHIP_FT6336U;
        s_touchAddr = 0x38;
      }
    }
  }

  if (s_touchChip == TOUCH_CHIP_NONE) {
    Serial.println("[Touch] No capacitive touch digitizer detected on I2C bus.");
    return false;
  }

  if (s_touchChip == TOUCH_CHIP_GT911) {
    uint8_t prodId[5] = {0};
    gt911_read(0x8140, prodId, 4);
    Serial.printf("[Touch] Goodix GT911 detected at 0x%02X (Product ID: %s)\n", s_touchAddr, prodId);
    // Clear initial buffer status
    gt911_write(0x814E, 0);
  } else {
    Serial.printf("[Touch] FocalTech FT6336U detected at 0x%02X\n", s_touchAddr);
  }

  s_initialized = true;
  s_sleeping = false;
  return true;
}

bool touch_read(touch_point_t* out_point) {
  if (!out_point) return false;
  out_point->x = 0;
  out_point->y = 0;
  out_point->pressed = false;

  if (!s_initialized || s_sleeping) return false;

#if defined(HAS_XPT2046_TOUCH)
  if (s_touchChip == TOUCH_CHIP_XPT2046 && s_xpt) {
    if (s_xpt->touched()) {
      TS_Point p = s_xpt->getPoint();
      // Calibrated coordinates for CrowPanel 4.3 (480x272):
      // Factory map: X 4000 (left) -> 200 (right), Y 200 (top) -> 4000 (bottom)
      int x = map(p.x, 4000, 200, 0, DEVICE_DISPLAY_WIDTH - 1);
      int y = map(p.y, 200, 4000, 0, DEVICE_DISPLAY_HEIGHT - 1);

      out_point->pressed = true;
      out_point->x = constrain(x, 0, DEVICE_DISPLAY_WIDTH - 1);
      out_point->y = constrain(y, 0, DEVICE_DISPLAY_HEIGHT - 1);
      return true;
    }
    return true;
  }
#endif

  if (s_touchChip == TOUCH_CHIP_GT911) {
    uint8_t status = 0;
    if (!gt911_read(0x814E, &status, 1)) return false;

    // Bit 7 indicates buffer status (1 = new touch coordinates available)
    if ((status & 0x80) == 0) {
      return true; // No touch active, not an error
    }

    uint8_t pointCount = status & 0x0F;
    if (pointCount == 0) {
      // Finger lifted / no points
      gt911_write(0x814E, 0);
      return true;
    }

    // Read point 0 coordinates (0x8150 - 0x8153)
    uint8_t buf[4] = {0};
    if (gt911_read(0x8150, buf, 4)) {
      uint16_t rawX = buf[0] | ((uint16_t)buf[1] << 8);
      uint16_t rawY = buf[2] | ((uint16_t)buf[3] << 8);

      out_point->pressed = true;

#if defined(DEVICE_RETERMINAL_STICKY)
      // reTerminal Sticky digitizer reports in native 480x800 portrait coordinates.
      // Map to 800x480 landscape display layout:
      // Logical X: rawY (0..800)
      // Logical Y: 479 - rawX (0..480)
      if (rawX <= 480 && rawY <= 800) {
        out_point->x = (int)rawY;
        out_point->y = 479 - (int)rawX;
      } else {
        out_point->x = (int)rawX;
        out_point->y = (int)rawY;
      }
#else
      out_point->x = (int)rawX;
      out_point->y = (int)rawY;
#endif
      // Clamp within display bounds
      if (out_point->x < 0) out_point->x = 0;
      if (out_point->x >= DEVICE_DISPLAY_WIDTH) out_point->x = DEVICE_DISPLAY_WIDTH - 1;
      if (out_point->y < 0) out_point->y = 0;
      if (out_point->y >= DEVICE_DISPLAY_HEIGHT) out_point->y = DEVICE_DISPLAY_HEIGHT - 1;
    }

    // Critical for Goodix: Clear buffer ready flag
    gt911_write(0x814E, 0);
    return true;

  } else if (s_touchChip == TOUCH_CHIP_FT6336U) {
    uint8_t buf[5] = {0};
    if (!ft6336_read(0x02, buf, 5)) return false;

    uint8_t points = buf[0] & 0x0F;
    if (points > 0) {
      uint16_t rawX = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
      uint16_t rawY = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];

      out_point->pressed = true;
      out_point->x = (int)rawX;
      out_point->y = (int)rawY;

      if (out_point->x < 0) out_point->x = 0;
      if (out_point->x >= DEVICE_DISPLAY_WIDTH) out_point->x = DEVICE_DISPLAY_WIDTH - 1;
      if (out_point->y < 0) out_point->y = 0;
      if (out_point->y >= DEVICE_DISPLAY_HEIGHT) out_point->y = DEVICE_DISPLAY_HEIGHT - 1;
    }
    return true;
  }

  return false;
}

void touch_sleep() {
  if (!s_initialized || s_sleeping) return;

  if (s_touchChip == TOUCH_CHIP_GT911) {
    // Write 0x05 to GT911 command register 0x8040 (Enter Sleep)
    gt911_write(0x8040, 0x05);
  } else if (s_touchChip == TOUCH_CHIP_FT6336U) {
    // FT6336 PMODE 0xA5 = 0x03 (Deep Sleep)
    TOUCH_WIRE.beginTransmission(s_touchAddr);
    TOUCH_WIRE.write(0xA5);
    TOUCH_WIRE.write(0x03);
    TOUCH_WIRE.endTransmission();
  } else if (s_touchChip == TOUCH_CHIP_XPT2046) {
    // Passive resistive touch digitizer
  }

#ifdef PIN_TOUCH_EN
  digitalWrite(PIN_TOUCH_EN, LOW);
#endif

  s_sleeping = true;
  Serial.println("[Touch] Digitizer entered low-power sleep mode.");
}

void touch_wake() {
  if (!s_initialized || !s_sleeping) return;

  if (s_touchChip == TOUCH_CHIP_XPT2046) {
    s_sleeping = false;
    Serial.println("[Touch] Digitizer awakened from sleep.");
    return;
  }

#ifdef PIN_TOUCH_EN
  digitalWrite(PIN_TOUCH_EN, HIGH);
  delay(10);
#endif

  // Enforce GT911 address selection on wake: INT held LOW while RST rises
#ifdef PIN_TOUCH_INT
  pinMode(PIN_TOUCH_INT, OUTPUT);
  digitalWrite(PIN_TOUCH_INT, LOW);
#endif

#ifdef PIN_TOUCH_RST
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(5);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(20);
#endif

#ifdef PIN_TOUCH_INT
  pinMode(PIN_TOUCH_INT, INPUT);
#endif
  delay(50);

  s_sleeping = false;
  Serial.println("[Touch] Digitizer awakened from sleep.");
}

#else

// ─── Stubs for Non-Touch Hardware Targets & Native Unit Tests ────────────────

bool touch_init() {
  return false;
}

bool touch_read(touch_point_t* out_point) {
  if (out_point) {
    out_point->x = 0;
    out_point->y = 0;
    out_point->pressed = false;
  }
  return false;
}

void touch_sleep() {}

void touch_wake() {}

#endif
