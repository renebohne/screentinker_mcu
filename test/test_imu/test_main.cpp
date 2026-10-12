#include <cassert>
#include <iostream>
#include <vector>
#include "imu_driver.h"
#include "setup_screen.h"

void test_imu_orientation_detection() {
  std::cout << "[Test] Running IMU orientation detection tests...\n";

  imu_init();
  assert(imu_is_available());

  // 1. Landscape 0 (Dominant +X)
  imu_set_mock_accel(16384, 0, 0);
  int orient = imu_detect_orientation(ORIENTATION_LANDSCAPE_0);
  assert(orient == ORIENTATION_LANDSCAPE_0);

  // 2. Landscape 180 (Dominant -X)
  imu_set_mock_accel(-16384, 500, 200);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_LANDSCAPE_180);

  // 3. Portrait 90 (Dominant +Y)
  imu_set_mock_accel(300, 16384, 100);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_90);

  // 4. Portrait 270 (Dominant -Y)
  imu_set_mock_accel(-200, -16384, 100);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_270);

  // 5. Flat desk test: +Z dominant, small X/Y -> should maintain previous orientation (ORIENTATION_PORTRAIT_270)
  imu_set_mock_accel(1500, -1200, 16000);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_270);

  // 6. Hysteresis band test: X and Y close together -> should maintain previous orientation
  imu_set_mock_accel(9000, 8500, 2000);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_270);

  std::cout << "  ✓ All IMU orientation tests passed!\n";
}

void test_system_menu_rendering() {
  std::cout << "[Test] Running System Menu orientation rendering tests...\n";

  gfx_set_screen_dimensions(800, 480);
  std::vector<uint8_t> buffer(gfx_get_buffer_size(), 0xFF);

  // Test menu with IMU device (Sticky) in Auto mode
  renderSystemMenu(buffer.data(), 1, LANG_EN, 0, (uint8_t)ORIENT_MODE_AUTO, true);
  // Verify buffer isn't completely empty
  bool nonZero = false;
  for (uint8_t b : buffer) {
    if (b != 0xFF) { nonZero = true; break; }
  }
  assert(nonZero);

  // Test menu with non-IMU device (E1001) in Landscape mode
  std::fill(buffer.begin(), buffer.end(), 0xFF);
  renderSystemMenu(buffer.data(), 1, LANG_DE, 1, (uint8_t)ORIENT_MODE_LANDSCAPE, false);
  nonZero = false;
  for (uint8_t b : buffer) {
    if (b != 0xFF) { nonZero = true; break; }
  }
  assert(nonZero);

  // Test menu with non-IMU device in Portrait mode
  std::fill(buffer.begin(), buffer.end(), 0xFF);
  renderSystemMenu(buffer.data(), 1, LANG_EN, 2, (uint8_t)ORIENT_MODE_PORTRAIT, false);
  nonZero = false;
  for (uint8_t b : buffer) {
    if (b != 0xFF) { nonZero = true; break; }
  }
  assert(nonZero);

  std::cout << "  ✓ All System Menu rendering tests passed!\n";
}

#include <cmath>
#include "sht40_driver.h"

void test_sht40_driver() {
  std::cout << "[Test] Running SHT40 driver tests...\n";

  // 1. Sensirion CRC-8 test vector (0xBEEF -> 0x92)
  uint8_t sample[2] = { 0xBE, 0xEF };
  uint8_t crc = sht40_crc8(sample, 2);
  assert(crc == 0x92);

  // 2. Temperature conversion tests
  // Min raw (0) -> -45.0 C
  float t_min = sht40_raw_to_temp_c(0);
  assert(std::fabs(t_min - (-45.0f)) < 0.01f);

  // Max raw (65535) -> 130.0 C
  float t_max = sht40_raw_to_temp_c(65535);
  assert(std::fabs(t_max - 130.0f) < 0.01f);

  // 3. Humidity conversion and clamping tests
  // Min raw (0) -> clamped to 0.0%
  float rh_min = sht40_raw_to_humidity_pct(0);
  assert(rh_min == 0.0f);

  // Max raw (65535) -> clamped to 100.0%
  float rh_max = sht40_raw_to_humidity_pct(65535);
  assert(rh_max == 100.0f);

  // 4. Mock read and availability
  sht40_init();
  assert(sht40_is_available());

  sht40_set_mock_values(22.3f, 48.5f);
  float read_t = 0.0f;
  float read_rh = 0.0f;
  assert(sht40_read(&read_t, &read_rh));
  assert(std::fabs(read_t - 22.3f) < 0.01f);
  assert(std::fabs(read_rh - 48.5f) < 0.01f);

  // Test unavailable mock
  sht40_set_mock_available(false);
  assert(!sht40_read(&read_t, &read_rh));

  // Restore availability
  sht40_set_mock_available(true);
  assert(sht40_read(&read_t, &read_rh));

  std::cout << "  ✓ All SHT40 driver tests passed!\n";
}

void test_status_screen_telemetry() {
  std::cout << "[Test] Running Status Screen with SHT40 telemetry rendering tests...\n";

  gfx_set_screen_dimensions(800, 480);
  std::vector<uint8_t> buffer(gfx_get_buffer_size(), 0xFF);

  // Test rendering status info screen with valid temp & humidity
  renderStatusInfoScreen(
    buffer.data(),
    "192.168.1.50",
    "ScreenTinker-WiFi",
    -65,
    "http://192.168.1.10:3001",
    "st-dev-test01",
    "1.3.0",
    LANG_EN,
    21.4f,
    45.0f
  );

  bool nonZero = false;
  for (uint8_t b : buffer) {
    if (b != 0xFF) { nonZero = true; break; }
  }
  assert(nonZero);

  // Test rendering without sensor (-999.0f, -1.0f)
  std::fill(buffer.begin(), buffer.end(), 0xFF);
  renderStatusInfoScreen(
    buffer.data(),
    "192.168.1.50",
    "ScreenTinker-WiFi",
    -65,
    "http://192.168.1.10:3001",
    "st-dev-test01",
    "1.3.0",
    LANG_DE,
    -999.0f,
    -1.0f
  );
  nonZero = false;
  for (uint8_t b : buffer) {
    if (b != 0xFF) { nonZero = true; break; }
  }
  assert(nonZero);

  std::cout << "  ✓ All Status Screen telemetry tests passed!\n";
}

#include "diag_logger.h"

void test_diag_logger() {
  std::cout << "[Test] Running Diagnostic Logger ring buffer tests...\n";

  diag_log_init();
  assert(diag_log_count() == 0);

  // 1. Basic write and read
  diag_log_write(DIAG_LOG_INFO, "WiFi", "Connected to AP");
  assert(diag_log_count() == 1);

  DiagLogEntry entry;
  assert(diag_log_get(0, &entry));
  assert(entry.level == DIAG_LOG_INFO);
  assert(std::string(entry.tag) == "WiFi");
  assert(std::string(entry.msg) == "Connected to AP");

  // Out of bounds get
  assert(!diag_log_get(1, &entry));

  // 2. Clear buffer
  diag_log_clear();
  assert(diag_log_count() == 0);

  // 3. Ring buffer overflow / overwrite test (150 entries into buffer size 100)
  for (int i = 0; i < 150; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Log event %d", i);
    diag_log_write(DIAG_LOG_DEBUG, "SYS", buf);
  }

  assert(diag_log_count() == 100);

  // Oldest should now be index 50
  assert(diag_log_get(0, &entry));
  assert(std::string(entry.msg) == "Log event 50");

  // Newest should be index 149
  assert(diag_log_get(99, &entry));
  assert(std::string(entry.msg) == "Log event 149");

  // 4. JSON serialization test
  diag_log_clear();
  diag_log_write(DIAG_LOG_INFO, "Sync", "200 OK");
  diag_log_write(DIAG_LOG_WARN, "Battery", "Low voltage: < 3.3V");

  std::string json = diag_log_to_json_string("dev-test-123", "1.3.0", 85, -65);
  assert(json.find("\"device_id\":\"dev-test-123\"") != std::string::npos);
  assert(json.find("\"version\":\"1.3.0\"") != std::string::npos);
  assert(json.find("\"battery\":85") != std::string::npos);
  assert(json.find("\"rssi\":-65") != std::string::npos);
  assert(json.find("\"log_count\":2") != std::string::npos);
  assert(json.find("\"tag\":\"Sync\"") != std::string::npos);
  assert(json.find("\"msg\":\"200 OK\"") != std::string::npos);
  assert(json.find("\"level\":\"WARN\"") != std::string::npos);

  std::cout << "  ✓ All Diagnostic Logger tests passed!\n";
}

int main() {
  std::cout << "==========================================\n";
  std::cout << " ScreenTinker Native Unit Tests (#9, #21, #22)\n";
  std::cout << "==========================================\n";

  test_imu_orientation_detection();
  test_system_menu_rendering();
  test_sht40_driver();
  test_status_screen_telemetry();
  test_diag_logger();

  std::cout << "\n✅ ALL UNIT TESTS PASSED SUCCESSFULLY!\n";
  return 0;
}
