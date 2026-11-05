#include "led_config.h"

#if defined(DEVICE_IS_LED_STRIP)
#include <Preferences.h>
#include <esp_system.h>
#include "led_driver.h"

/*
 * See include/led_config.h. NVS namespace "st_led" holds what the server last sent; the matrix
 * layout is also written to the "screentinker" mat_* keys the CLI and web installer already use,
 * so `get matrix` and the next boot agree with what the server set.
 */

static const char* NS = "st_led";
static const uint32_t LEARN_FLOOR_MA = 300;     // never learn below this: the controller needs some
static const uint32_t LEARN_DEFAULT_BASE_MA = 3000;
static const uint8_t LEARN_PERCENT = 85;        // each brownout keeps 85% of the limit that failed

static LedRemoteConfig s_cfg;
static uint32_t s_learnedMa = 0;
static uint32_t s_brownouts = 0;
static const char* s_resetReason = "unknown";

// Kept across a brownout reset (RTC slow memory is not cleared by it), so the boot after a brownout
// knows roughly what the supply could not deliver. The magic tells a real value from power-on noise.
RTC_NOINIT_ATTR static uint32_t rtc_estMagic;
RTC_NOINIT_ATTR static uint32_t rtc_lastEstMa;
static const uint32_t EST_MAGIC = 0x5354454D; // "STEM"

static char s_reasonBuf[16];
static const char* reasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog";
    case ESP_RST_SW: return "software";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_EXT: return "external";
    default:
      // Anything else (USB-JTAG reset, CPU lockup, ...) as its number, so a crash is never just "unknown".
      snprintf(s_reasonBuf, sizeof(s_reasonBuf), "rst%d", (int)r);
      return s_reasonBuf;
  }
}

static uint8_t wiringFrom(const String& v, uint8_t dflt) {
  if (v.equalsIgnoreCase("serpentine")) return MATRIX_WIRING_SERPENTINE;
  if (v.equalsIgnoreCase("progressive")) return MATRIX_WIRING_PROGRESSIVE;
  return dflt;
}
static uint8_t axisFrom(const String& v, uint8_t dflt) {
  if (v.equalsIgnoreCase("horizontal")) return MATRIX_AXIS_HORIZONTAL;
  if (v.equalsIgnoreCase("vertical")) return MATRIX_AXIS_VERTICAL;
  return dflt;
}
static uint8_t originFrom(const String& v, uint8_t dflt) {
  if (v.equalsIgnoreCase("top_left")) return MATRIX_ORIGIN_TOP_LEFT;
  if (v.equalsIgnoreCase("top_right")) return MATRIX_ORIGIN_TOP_RIGHT;
  if (v.equalsIgnoreCase("bottom_left")) return MATRIX_ORIGIN_BOTTOM_LEFT;
  if (v.equalsIgnoreCase("bottom_right")) return MATRIX_ORIGIN_BOTTOM_RIGHT;
  return dflt;
}
static const char* wiringName(uint8_t w) { return w == MATRIX_WIRING_PROGRESSIVE ? "progressive" : "serpentine"; }
static const char* axisName(uint8_t a) { return a == MATRIX_AXIS_VERTICAL ? "vertical" : "horizontal"; }
static const char* originName(uint8_t o) {
  return o == MATRIX_ORIGIN_TOP_RIGHT ? "top_right" : o == MATRIX_ORIGIN_BOTTOM_LEFT ? "bottom_left"
       : o == MATRIX_ORIGIN_BOTTOM_RIGHT ? "bottom_right" : "top_left";
}
static bool validOrder(const String& o) {
  static const char* ok[] = {"GRB", "RGB", "BRG", "RBG", "GBR", "BGR"};
  for (const char* x : ok) if (o.equalsIgnoreCase(x)) return true;
  return false;
}

uint32_t led_config_effective_limit_ma() {
  uint32_t a = s_cfg.powerLimitMa, b = s_learnedMa;
  if (a == 0) return b;
  if (b == 0) return a;
  return a < b ? a : b;
}

static void applyToDriver() {
  led_driver_set_brightness(s_cfg.brightness);
  led_driver_set_power_limit(led_config_effective_limit_ma());
  if (s_cfg.width > 0 && s_cfg.height > 0) {
    led_driver_set_matrix_config(s_cfg.width, s_cfg.height, (MatrixWiring)s_cfg.wiring, (MatrixAxis)s_cfg.axis, (MatrixOrigin)s_cfg.origin);
  }
}

static void save() {
  Preferences p;
  p.begin(NS, false);
  p.putString("rev", s_cfg.rev);
  p.putUChar("b", s_cfg.brightness);
  p.putString("o", s_cfg.colorOrder);
  p.putULong("pl", s_cfg.powerLimitMa);
  p.putBool("auto", s_cfg.autoProtect);
  p.putUShort("lr", s_cfg.learnedReset);
  p.putULong("lpl", s_learnedMa);
  p.putULong("bo", s_brownouts);
  p.end();
  // The layout, where the rest of the firmware already keeps it.
  Preferences m;
  m.begin("screentinker", false);
  m.putUShort("mat_w", s_cfg.width);
  m.putUShort("mat_h", s_cfg.height);
  m.putUChar("mat_wir", s_cfg.wiring);
  m.putUChar("mat_axis", s_cfg.axis);
  m.putUChar("mat_orig", s_cfg.origin);
  m.end();
}

void led_config_begin() {
  // Start from what the driver was set up with (main.cpp loads the mat_* keys before this).
  LedMatrixConfig mc = led_driver_get_matrix_config();
  memset(&s_cfg, 0, sizeof(s_cfg));
  s_cfg.width = mc.width;
  s_cfg.height = mc.height;
  s_cfg.wiring = mc.wiring;
  s_cfg.axis = mc.axis;
  s_cfg.origin = mc.origin;

  Preferences p;
  p.begin(NS, true);
  String rev = p.getString("rev", "");
  strncpy(s_cfg.rev, rev.c_str(), sizeof(s_cfg.rev) - 1);
  s_cfg.brightness = p.getUChar("b", led_driver_get_brightness());
  String o = p.getString("o", "GRB");
  strncpy(s_cfg.colorOrder, validOrder(o) ? o.c_str() : "GRB", sizeof(s_cfg.colorOrder) - 1);
  // No server settings yet: the board's rated 3 A, the same default the server uses.
  s_cfg.powerLimitMa = p.getULong("pl", 3000);
  s_cfg.autoProtect = p.getBool("auto", true);
  s_cfg.learnedReset = p.getUShort("lr", 0);
  s_learnedMa = p.getULong("lpl", 0);
  s_brownouts = p.getULong("bo", 0);
  p.end();

  esp_reset_reason_t rr = esp_reset_reason();
  s_resetReason = reasonName(rr);
  if (rr == ESP_RST_BROWNOUT) {
    s_brownouts++;
    if (s_cfg.autoProtect) {
      // Lower the limit below whatever failed: the effective limit, or the last frame's draw if
      // that was less (the supply could not even deliver that), or the board default.
      uint32_t base = led_config_effective_limit_ma();
      if (rtc_estMagic == EST_MAGIC && rtc_lastEstMa > 0 && (base == 0 || rtc_lastEstMa < base)) base = rtc_lastEstMa;
      if (base == 0) base = LEARN_DEFAULT_BASE_MA;
      uint32_t learned = base * LEARN_PERCENT / 100;
      s_learnedMa = learned < LEARN_FLOOR_MA ? LEARN_FLOOR_MA : learned;
      Serial.printf("[LED] Brownout reset #%u: power limit lowered to %u mA (was %u mA).\n",
                    (unsigned)s_brownouts, (unsigned)s_learnedMa, (unsigned)base);
    } else {
      Serial.printf("[LED] Brownout reset #%u (brownout protection off).\n", (unsigned)s_brownouts);
    }
    save();
  }
  rtc_estMagic = 0;  // consumed; only a fresh estimate from this boot counts next time
  applyToDriver();
  Serial.printf("[LED] Settings: brightness %u, order %s, limit %u mA (learned %u), reset reason %s\n",
                s_cfg.brightness, s_cfg.colorOrder, (unsigned)led_config_effective_limit_ma(),
                (unsigned)s_learnedMa, s_resetReason);
}

bool led_config_apply_header(const String& header) {
  if (header.length() == 0 || header.length() > 400) return false;
  String rev, b, o, wir, ax, org, w, h, pl, autoP, lr;
  bool v1 = false;
  int start = 0;
  while (start < (int)header.length()) {
    int end = header.indexOf(';', start);
    if (end < 0) end = header.length();
    String part = header.substring(start, end);
    int eq = part.indexOf('=');
    if (eq > 0) {
      String k = part.substring(0, eq), v = part.substring(eq + 1);
      k.trim(); v.trim();
      if (k == "v") v1 = (v == "1");
      else if (k == "rev") rev = v;
      else if (k == "b") b = v;
      else if (k == "o") o = v;
      else if (k == "wir") wir = v;
      else if (k == "ax") ax = v;
      else if (k == "org") org = v;
      else if (k == "w") w = v;
      else if (k == "h") h = v;
      else if (k == "pl") pl = v;
      else if (k == "auto") autoP = v;
      else if (k == "lr") lr = v;
    }
    start = end + 1;
  }
  if (!v1 || rev.length() != 8) return false;
  if (rev == String(s_cfg.rev)) return false;  // nothing changed since last time

  if (b.length()) { long x = b.toInt(); if (x >= 0 && x <= 255) s_cfg.brightness = (uint8_t)x; }
  if (o.length() && validOrder(o)) { o.toUpperCase(); strncpy(s_cfg.colorOrder, o.c_str(), sizeof(s_cfg.colorOrder) - 1); }
  // Layout fields are only present once an operator set them on the server (it never sends
  // defaults), so a missing key means "keep the wall's own".
  if (wir.length()) s_cfg.wiring = wiringFrom(wir, s_cfg.wiring);
  if (ax.length()) s_cfg.axis = axisFrom(ax, s_cfg.axis);
  if (org.length()) s_cfg.origin = originFrom(org, s_cfg.origin);
  if (w.length() && h.length()) {
    long ww = w.toInt(), hh = h.toInt();
    if (ww > 0 && hh > 0 && ww * hh <= 16384) { s_cfg.width = (uint16_t)ww; s_cfg.height = (uint16_t)hh; }
  }
  if (pl.length()) { long x = pl.toInt(); if (x >= 0 && x <= 100000) s_cfg.powerLimitMa = (uint32_t)x; }
  if (autoP.length()) s_cfg.autoProtect = (autoP == "1");
  if (lr.length()) {
    long x = lr.toInt();
    if (x >= 0 && x <= 0xffff && (uint16_t)x != s_cfg.learnedReset) {
      s_cfg.learnedReset = (uint16_t)x;
      s_learnedMa = 0;   // the operator says the supply is fixed: forget what was learned
      Serial.println("[LED] Learned power limit cleared from the dashboard.");
    }
  }
  strncpy(s_cfg.rev, rev.c_str(), sizeof(s_cfg.rev) - 1);
  applyToDriver();
  save();
  Serial.printf("[LED] Applied server settings rev %s: brightness %u, order %s, %ux%u %s/%s/%s, limit %u mA%s\n",
                s_cfg.rev, s_cfg.brightness, s_cfg.colorOrder, s_cfg.width, s_cfg.height,
                wiringName(s_cfg.wiring), axisName(s_cfg.axis), originName(s_cfg.origin),
                (unsigned)led_config_effective_limit_ma(), s_cfg.autoProtect ? " (brownout protection on)" : "");
  return true;
}

String led_config_state_header() {
  char buf[220];
  snprintf(buf, sizeof(buf), "rev=%s;b=%u;pl=%u;lpl=%u;bo=%u;est=%u;rr=%s;o=%s;wir=%s;ax=%s;org=%s",
           s_cfg.rev[0] ? s_cfg.rev : "00000000", s_cfg.brightness, (unsigned)led_config_effective_limit_ma(),
           (unsigned)s_learnedMa, (unsigned)s_brownouts, (unsigned)led_driver_last_estimate_ma(), s_resetReason,
           s_cfg.colorOrder, wiringName(s_cfg.wiring), axisName(s_cfg.axis), originName(s_cfg.origin));
  return String(buf);
}

const char* led_config_color_order() { return s_cfg.colorOrder[0] ? s_cfg.colorOrder : "GRB"; }
const LedRemoteConfig& led_config_current() { return s_cfg; }

void led_config_note_frame_estimate(uint32_t ma) {
  rtc_lastEstMa = ma;
  rtc_estMagic = EST_MAGIC;
}

#endif
