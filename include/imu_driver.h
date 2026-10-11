#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#endif

// ─── IMU Registers & Constants (ST LSM6DS3TR-C) ──────────────────────────────
#define LSM6DS3_DEFAULT_ADDR    0x6A
#define LSM6DS3_ALT_ADDR        0x6B

#define LSM6DS3_REG_WHO_AM_I    0x0F
#define LSM6DS3_REG_CTRL1_XL    0x10
#define LSM6DS3_REG_CTRL9_XL    0x18
#define LSM6DS3_REG_OUTX_L_XL   0x28

// Cardinal Screen Orientations
#define ORIENTATION_LANDSCAPE_0     0
#define ORIENTATION_PORTRAIT_90     1
#define ORIENTATION_LANDSCAPE_180   2
#define ORIENTATION_PORTRAIT_270    3

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize LSM6DS3TR-C IMU on the current Wire bus.
 * Probes primary address 0x6A then fallback 0x6B.
 * Returns true if sensor is detected and configured.
 */
bool imu_init();

/**
 * Check if the IMU was successfully initialized.
 */
bool imu_is_available();

/**
 * Read raw acceleration on X, Y, Z axes (in LSB, +/-2g scale).
 */
bool imu_read_accel(int16_t* ax, int16_t* ay, int16_t* az);

/**
 * Resolve screen orientation from current acceleration vector.
 * Returns:
 *   ORIENTATION_LANDSCAPE_0 (0),
 *   ORIENTATION_PORTRAIT_90 (1),
 *   ORIENTATION_LANDSCAPE_180 (2),
 *   ORIENTATION_PORTRAIT_270 (3),
 *   or currentOrientation if flat or within hysteresis band.
 */
int imu_detect_orientation(int currentOrientation);

#ifndef ARDUINO
/**
 * Test mock helper for native tests and simulator.
 */
void imu_set_mock_accel(int16_t ax, int16_t ay, int16_t az);
void imu_set_mock_available(bool available);
#endif

#ifdef __cplusplus
}
#endif
