#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#endif

// ─── Sensirion SHT40 Registers & Commands ─────────────────────────────────────
#define SHT40_I2C_ADDR               0x44

#define SHT40_CMD_MEASURE_HIGH_PREC  0xFD
#define SHT40_CMD_MEASURE_MED_PREC   0xF6
#define SHT40_CMD_MEASURE_LOW_PREC   0xE0
#define SHT40_CMD_SOFT_RESET         0x94
#define SHT40_CMD_READ_SERIAL        0x89

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize SHT40 sensor on the active I2C bus.
 * Performs a soft-reset and checks responsiveness.
 * Returns true if the sensor ACKed, false otherwise.
 */
bool sht40_init();

/**
 * Check if the SHT40 sensor was successfully initialized.
 */
bool sht40_is_available();

/**
 * Trigger a measurement and read ambient temperature and relative humidity.
 *
 * @param temp_c Pointer to store temperature in degrees Celsius (e.g. 21.4).
 * @param humidity_pct Pointer to store relative humidity in % (0.0 to 100.0).
 * @return true if measurement succeeded and CRC check passed; false on error.
 */
bool sht40_read(float* temp_c, float* humidity_pct);

/**
 * Sensirion CRC-8 polynomial 0x31 (x^8 + x^5 + x^4 + 1), initial value 0xFF.
 */
uint8_t sht40_crc8(const uint8_t* data, uint16_t len);

/**
 * Calculate Celsius temperature from 16-bit raw sensor word.
 */
float sht40_raw_to_temp_c(uint16_t raw_t);

/**
 * Calculate Relative Humidity (%) from 16-bit raw sensor word (clamped to 0..100).
 */
float sht40_raw_to_humidity_pct(uint16_t raw_rh);

#ifndef ARDUINO
/**
 * Test & Simulator Mock Functions
 */
void sht40_set_mock_values(float temp_c, float humidity_pct);
void sht40_set_mock_available(bool available);
#endif

#ifdef __cplusplus
}
#endif
