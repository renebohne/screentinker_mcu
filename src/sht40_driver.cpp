#include "sht40_driver.h"

uint8_t sht40_crc8(const uint8_t* data, uint16_t len) {
  uint8_t crc = 0xFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

float sht40_raw_to_temp_c(uint16_t raw_t) {
  return -45.0f + 175.0f * ((float)raw_t / 65535.0f);
}

float sht40_raw_to_humidity_pct(uint16_t raw_rh) {
  float rh = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);
  if (rh < 0.0f) rh = 0.0f;
  if (rh > 100.0f) rh = 100.0f;
  return rh;
}

#ifdef ARDUINO

static bool s_sht40Available = false;

bool sht40_init() {
#if !defined(PIN_I2C_SDA) || !defined(PIN_I2C_SCL)
  s_sht40Available = false;
  return false;
#else
  Wire.beginTransmission(SHT40_I2C_ADDR);
  Wire.write(SHT40_CMD_SOFT_RESET);
  if (Wire.endTransmission() != 0) {
    s_sht40Available = false;
    return false;
  }
  delay(2);
  s_sht40Available = true;
  return true;
#endif
}

bool sht40_is_available() {
  return s_sht40Available;
}

bool sht40_read(float* temp_c, float* humidity_pct) {
#if !defined(PIN_I2C_SDA) || !defined(PIN_I2C_SCL)
  return false;
#else
  if (!s_sht40Available) {
    // Attempt re-detection once if previously unavailable
    if (!sht40_init()) return false;
  }

  // Trigger high-precision measurement
  Wire.beginTransmission(SHT40_I2C_ADDR);
  Wire.write(SHT40_CMD_MEASURE_HIGH_PREC);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  // High-precision measurement takes max 8.2ms; wait 10ms
  delay(10);

  if (Wire.requestFrom((uint8_t)SHT40_I2C_ADDR, (uint8_t)6) != 6) {
    return false;
  }

  uint8_t rx[6];
  for (int i = 0; i < 6; i++) {
    rx[i] = Wire.read();
  }

  // Verify temperature CRC (bytes 0, 1 -> byte 2)
  if (sht40_crc8(&rx[0], 2) != rx[2]) {
    return false;
  }

  // Verify humidity CRC (bytes 3, 4 -> byte 5)
  if (sht40_crc8(&rx[3], 2) != rx[5]) {
    return false;
  }

  uint16_t raw_t = ((uint16_t)rx[0] << 8) | rx[1];
  uint16_t raw_rh = ((uint16_t)rx[3] << 8) | rx[4];

  if (temp_c) *temp_c = sht40_raw_to_temp_c(raw_t);
  if (humidity_pct) *humidity_pct = sht40_raw_to_humidity_pct(raw_rh);

  return true;
#endif
}

#else

// ─── Non-Arduino (Native Unit Test & Simulator Mock) ─────────────────────────
static bool s_mockAvailable = true;
static float s_mockTemp = 21.5f;
static float s_mockHumidity = 45.0f;

bool sht40_init() {
  s_mockAvailable = true;
  return true;
}

bool sht40_is_available() {
  return s_mockAvailable;
}

bool sht40_read(float* temp_c, float* humidity_pct) {
  if (!s_mockAvailable) return false;
  if (temp_c) *temp_c = s_mockTemp;
  if (humidity_pct) *humidity_pct = s_mockHumidity;
  return true;
}

void sht40_set_mock_values(float temp_c, float humidity_pct) {
  s_mockTemp = temp_c;
  s_mockHumidity = humidity_pct;
}

void sht40_set_mock_available(bool available) {
  s_mockAvailable = available;
}

#endif
