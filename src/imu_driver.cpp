#include "imu_driver.h"
#include <stdlib.h>

#ifdef ARDUINO
static uint8_t s_imuAddr = LSM6DS3_DEFAULT_ADDR;
static bool s_imuInitialized = false;

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

static bool readReg(uint8_t addr, uint8_t reg, uint8_t* val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)addr, (uint8_t)1) != 1) return false;
  *val = Wire.read();
  return true;
}

static bool readBytes(uint8_t addr, uint8_t reg, uint8_t* buffer, size_t length) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)addr, (uint8_t)length) != length) return false;
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool imu_init() {
#if !defined(PIN_I2C_SDA) || !defined(PIN_I2C_SCL)
  s_imuInitialized = false;
  return false;
#else
  uint8_t who = 0;
  s_imuAddr = LSM6DS3_DEFAULT_ADDR;
  if (!readReg(s_imuAddr, LSM6DS3_REG_WHO_AM_I, &who)) {
    s_imuAddr = LSM6DS3_ALT_ADDR;
    if (!readReg(s_imuAddr, LSM6DS3_REG_WHO_AM_I, &who)) {
      s_imuInitialized = false;
      return false;
    }
  }

  // WHO_AM_I for LSM6DS3 family is typically 0x69, 0x6A, or 0x6C
  if (who != 0x6A && who != 0x69 && who != 0x6C) {
    s_imuInitialized = false;
    return false;
  }

  // Set ODR_XL = 26 Hz, FS_XL = +/-2g (0x20)
  writeReg(s_imuAddr, LSM6DS3_REG_CTRL1_XL, 0x20);
  // Enable X, Y, Z axes (0x38)
  writeReg(s_imuAddr, LSM6DS3_REG_CTRL9_XL, 0x38);

  s_imuInitialized = true;
  return true;
#endif
}

bool imu_is_available() {
  return s_imuInitialized;
}

bool imu_read_accel(int16_t* ax, int16_t* ay, int16_t* az) {
  if (!s_imuInitialized) return false;
  uint8_t raw[6];
  if (!readBytes(s_imuAddr, LSM6DS3_REG_OUTX_L_XL, raw, 6)) {
    return false;
  }
  if (ax) *ax = (int16_t)((raw[1] << 8) | raw[0]);
  if (ay) *ay = (int16_t)((raw[3] << 8) | raw[2]);
  if (az) *az = (int16_t)((raw[5] << 8) | raw[4]);
  return true;
}

#else

// ─── Non-Arduino (Native Unit Test & Simulator Mock) ─────────────────────────
static bool s_mockAvailable = false;
static int16_t s_mockX = 0;
static int16_t s_mockY = 0;
static int16_t s_mockZ = 16384; // Default: flat

bool imu_init() {
  s_mockAvailable = true;
  return true;
}

bool imu_is_available() {
  return s_mockAvailable;
}

bool imu_read_accel(int16_t* ax, int16_t* ay, int16_t* az) {
  if (!s_mockAvailable) return false;
  if (ax) *ax = s_mockX;
  if (ay) *ay = s_mockY;
  if (az) *az = s_mockZ;
  return true;
}

void imu_set_mock_accel(int16_t ax, int16_t ay, int16_t az) {
  s_mockX = ax;
  s_mockY = ay;
  s_mockZ = az;
}

void imu_set_mock_available(bool available) {
  s_mockAvailable = available;
}

#endif

int imu_detect_orientation(int currentOrientation) {
  int16_t ax = 0, ay = 0, az = 0;
  if (!imu_read_accel(&ax, &ay, &az)) {
    return currentOrientation;
  }

  int32_t absX = abs(ax);
  int32_t absY = abs(ay);
  int32_t absZ = abs(az);

  // Flat on desk check: Z dominates, X and Y small
  if (absZ > 12000 && absX < 6000 && absY < 6000) {
    return currentOrientation; // Maintain previous orientation
  }

  // Hysteresis threshold
  const int32_t HYSTERESIS = 2500;

  // Determine dominant axis
  // On Seeed reTerminal Sticky:
  // - Dominant X axis corresponds to Landscape (800x480)
  // - Dominant Y axis corresponds to Portrait (480x800)
  // Each orientation is rotated 90 degrees to the left relative to sensor PCB
  if (absX > absY + HYSTERESIS) {
    return (ax >= 0) ? ORIENTATION_LANDSCAPE_180 : ORIENTATION_LANDSCAPE_0;
  } else if (absY > absX + HYSTERESIS) {
    return (ay >= 0) ? ORIENTATION_PORTRAIT_270 : ORIENTATION_PORTRAIT_90;
  }

  // Within hysteresis band: keep current orientation
  return currentOrientation;
}
