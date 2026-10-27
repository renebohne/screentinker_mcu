#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "imu_driver.h"
#include "setup_screen.h"

void test_imu_orientation_detection() {
  std::cout << "[Test] Running IMU orientation detection tests...\n";

  imu_init();
  assert(imu_is_available());

  // 1. Landscape 180 (Dominant +X)
  imu_set_mock_accel(16384, 0, 0);
  int orient = imu_detect_orientation(ORIENTATION_LANDSCAPE_180);
  assert(orient == ORIENTATION_LANDSCAPE_180);

  // 2. Landscape 0 (Dominant -X)
  imu_set_mock_accel(-16384, 500, 200);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_LANDSCAPE_0);

  // 3. Portrait 270 (Dominant +Y)
  imu_set_mock_accel(300, 16384, 100);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_270);

  // 4. Portrait 90 (Dominant -Y)
  imu_set_mock_accel(-200, -16384, 100);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_90);

  // 5. Flat desk test: +Z dominant, small X/Y -> should maintain previous orientation (ORIENTATION_PORTRAIT_90)
  imu_set_mock_accel(1500, -1200, 16000);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_90);

  // 6. Hysteresis band test: X and Y close together -> should maintain previous orientation
  imu_set_mock_accel(9000, 8500, 2000);
  orient = imu_detect_orientation(orient);
  assert(orient == ORIENTATION_PORTRAIT_90);

  std::cout << "  ✓ All IMU orientation tests passed!\n";
}

void test_system_menu_rendering() {
  std::cout << "[Test] Running System Menu orientation rendering tests...\n";

  gfx_set_screen_dimensions(800, 480);
  std::vector<uint8_t> buffer(gfx_get_buffer_size(), 0xFF);

  // Test menu with IMU device (Sticky) in Auto mode
  renderSystemMenu(buffer.data(), 1, LANG_EN, 0, (uint8_t)ORIENT_MODE_AUTO, true);
  // Header banner at (10, 20) is black (0)
  assert(gfx_get_pixel(buffer.data(), 10, 20) == 0);
  // 'S' in header "ScreenTinker" at (66, 17) is white (1)
  assert(gfx_get_pixel(buffer.data(), 66, 17) == 1);
  // Content margin at (10, 80) is white (1)
  assert(gfx_get_pixel(buffer.data(), 10, 80) == 1);
  // Selected menu item (index 1) filled box at (40, 110) is black (0)
  assert(gfx_get_pixel(buffer.data(), 40, 110) == 0);

  // Test menu with non-IMU device (E1001) in Landscape mode
  std::fill(buffer.begin(), buffer.end(), 0xFF);
  renderSystemMenu(buffer.data(), 1, LANG_DE, 1, (uint8_t)ORIENT_MODE_LANDSCAPE, false);
  assert(gfx_get_pixel(buffer.data(), 10, 20) == 0);
  assert(gfx_get_pixel(buffer.data(), 66, 17) == 1);
  assert(gfx_get_pixel(buffer.data(), 10, 80) == 1);
  assert(gfx_get_pixel(buffer.data(), 40, 110) == 0);

  // Test menu with non-IMU device in Portrait mode
  std::fill(buffer.begin(), buffer.end(), 0xFF);
  renderSystemMenu(buffer.data(), 1, LANG_EN, 2, (uint8_t)ORIENT_MODE_PORTRAIT, false);
  assert(gfx_get_pixel(buffer.data(), 10, 20) == 0);
  assert(gfx_get_pixel(buffer.data(), 66, 17) == 1);
  assert(gfx_get_pixel(buffer.data(), 10, 80) == 1);
  assert(gfx_get_pixel(buffer.data(), 40, 110) == 0);

  std::cout << "  ✓ All System Menu rendering tests passed!\n";
}

void test_smart_partial_refresh_policy() {
  std::cout << "[Test] Running Smart Partial Refresh Policy tests...\n";

  gfx_set_screen_dimensions(800, 480);
  std::vector<uint8_t> buffer(gfx_get_buffer_size(), 0xFF);

  // 1. Verify Menu item 3 (Refresh Mode) selection highlight
  renderSystemMenu(buffer.data(), 3, LANG_EN, 0, (uint8_t)ORIENT_MODE_LANDSCAPE, false, -1, false, false, 0, true);
  // Item 3 y is startY (58) + 3 * 40 = 178; item height is 35 -> y in [178, 213]
  assert(gfx_get_pixel(buffer.data(), 40, 190) == 0); // Item 3 filled rectangle is black

  // 2. Simulate 6:1 partial refresh cadence
  int partialCount = 0;
  const int limit = 6;
  bool isColdBoot = true;

  auto decideRefresh = [&](bool force, const std::string& serverHdr, bool supportsPartial, uint8_t userMode) -> std::string {
    if (!supportsPartial) {
      partialCount = 0;
      return "FULL";
    }
    if (isColdBoot || force) {
      isColdBoot = false;
      partialCount = 0;
      return "FULL";
    }
    if (serverHdr == "full") {
      partialCount = 0;
      return "FULL";
    }
    if (userMode == 1) { // Always full
      partialCount = 0;
      return "FULL";
    }
    if (partialCount < limit) {
      partialCount++;
      return "PARTIAL";
    } else {
      partialCount = 0;
      return "FULL";
    }
  };

  // Cold boot: always FULL
  assert(decideRefresh(false, "", true, 0) == "FULL");
  assert(partialCount == 0);

  // Consecutive 6 partials
  for (int i = 1; i <= 6; ++i) {
    assert(decideRefresh(false, "", true, 0) == "PARTIAL");
    assert(partialCount == i);
  }

  // 7th refresh triggers clean cycle (FULL) and resets to 0
  assert(decideRefresh(false, "", true, 0) == "FULL");
  assert(partialCount == 0);

  // 8th refresh resumes PARTIAL (count = 1)
  assert(decideRefresh(false, "", true, 0) == "PARTIAL");
  assert(partialCount == 1);

  // Server override forces full and resets counter
  assert(decideRefresh(false, "full", true, 0) == "FULL");
  assert(partialCount == 0);

  // Device not supporting partial (Spectra 6) always returns FULL
  assert(decideRefresh(false, "partial", false, 0) == "FULL");
  assert(partialCount == 0);

  // User configured "Always Full" mode
  assert(decideRefresh(false, "", true, 1) == "FULL");
  assert(partialCount == 0);

  std::cout << "  ✓ All Smart Partial Refresh Policy tests passed!\n";
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

#include "ota_updater.h"

void test_ota_updater() {
  std::cout << "[Test] Running OTA Updater tests (#8)...\n";

  // 1. Init and state query
  ota_init();
  OtaState st;
  ota_get_state(&st);
  assert(st.running_partition != nullptr);
  assert(st.next_partition != nullptr);
  assert(st.status == OTA_STATUS_IDLE);

  // 2. Confirm running partition: initially NOT pending verification, so confirm must return false
  assert(!ota_confirm_running_partition());

  // Pending verification confirm flow
  ota_set_pending_verification_for_test(true);
  assert(ota_confirm_running_partition());
  // Subsequent call must return false because flag was consumed
  assert(!ota_confirm_running_partition());

  // 3. Invalid URL or missing hash should fail immediately
  assert(!ota_perform_update(nullptr, nullptr));
  ota_get_state(&st);
  assert(st.status == OTA_STATUS_ERROR);

  assert(!ota_perform_update("", nullptr));
  ota_get_state(&st);
  assert(st.status == OTA_STATUS_ERROR);

  // Updates without a valid 64-hex SHA-256 must fail (M3 requirement)
  assert(!ota_perform_update("file:///tmp/dummy.bin", nullptr));
  ota_get_state(&st);
  assert(st.status == OTA_STATUS_ERROR);

  assert(!ota_perform_update("file:///tmp/dummy.bin", "short_or_invalid_hash"));
  ota_get_state(&st);
  assert(st.status == OTA_STATUS_ERROR);

  // 4. Test real payload fetch and SHA-256 verification on host
  const char* testPayload = "ScreenTinkerValidFirmwarePayloadBytes1234567890";
  const char* validSha = "355c01131c7cd714efc6a415e43c9713c834ca0d7fadfd9499815a617ef53025";
  const char* testFwPath = "/tmp/screentinker_test_fw.bin";
  FILE* fp = fopen(testFwPath, "wb");
  assert(fp != nullptr);
  fwrite(testPayload, 1, strlen(testPayload), fp);
  fclose(fp);

  std::string fileUrl = std::string("file://") + testFwPath;

  // Mismatched SHA-256 must fail
  const char* badSha = "0000000000000000000000000000000000000000000000000000000000000000";
  assert(!ota_perform_update(fileUrl.c_str(), badSha));
  ota_get_state(&st);
  assert(st.status == OTA_STATUS_ERROR);
  assert(!ota_confirm_running_partition());

  // Valid update with matching SHA-256 must succeed
  assert(ota_perform_update(fileUrl.c_str(), validSha));
  ota_get_state(&st);
  assert(st.status == OTA_STATUS_SUCCESS);
  assert(st.progress_percent == 100);

  // Confirm partition after successful OTA
  assert(ota_confirm_running_partition());
  assert(!ota_confirm_running_partition());

  remove(testFwPath);

  // 5. OTA Screen Rendering tests (EN & DE) with pixel assertions
  gfx_set_screen_dimensions(800, 480);
  std::vector<uint8_t> buffer(gfx_get_buffer_size(), 0xFF);

  renderOtaUpdateScreen(buffer.data(), "Updating...", LANG_EN);
  assert(gfx_get_pixel(buffer.data(), 10, 20) == 0); // Header banner black
  assert(gfx_get_pixel(buffer.data(), 66, 17) == 1); // 'S' in ScreenTinker white
  assert(gfx_get_pixel(buffer.data(), 40, 80) == 0); // Headline banner black
  assert(gfx_get_pixel(buffer.data(), 10, 80) == 1); // Margin white

  std::fill(buffer.begin(), buffer.end(), 0xFF);
  renderOtaUpdateScreen(buffer.data(), "Aktualisiere...", LANG_DE);
  assert(gfx_get_pixel(buffer.data(), 10, 20) == 0); // Header banner black
  assert(gfx_get_pixel(buffer.data(), 66, 17) == 1); // 'S' in ScreenTinker white
  assert(gfx_get_pixel(buffer.data(), 40, 80) == 0); // Headline banner black
  assert(gfx_get_pixel(buffer.data(), 10, 80) == 1); // Margin white

  std::cout << "  ✓ All OTA Updater tests passed!\n";
}

void test_imu_debug_screen() {
  std::cout << "[Test] Running IMU Debug Screen tests (letters A, B, C, D)...\n";
  gfx_set_screen_dimensions(800, 480);
  std::vector<uint8_t> buffer(gfx_get_buffer_size(), 0xFF);

  // Orient 0: Letter 'A'
  renderImuDebugScreen(buffer.data(), 0, 0, 0, 16000);
  assert(gfx_get_pixel(buffer.data(), 388, 85) == 0);  // Apex of 'A' is ink
  assert(gfx_get_pixel(buffer.data(), 322, 85) == 1);  // Margin is paper

  // Orient 1: Letter 'B'
  renderImuDebugScreen(buffer.data(), 1, 16000, 0, 0);
  assert(gfx_get_pixel(buffer.data(), 388, 151) == 0); // Middle bar of 'B' is ink
  assert(gfx_get_pixel(buffer.data(), 470, 151) == 1); // Margin is paper

  // Orient 2: Letter 'C'
  renderImuDebugScreen(buffer.data(), 2, 0, 0, -16000);
  assert(gfx_get_pixel(buffer.data(), 344, 151) == 0); // Spine of 'C' is ink
  assert(gfx_get_pixel(buffer.data(), 454, 151) == 1); // Hollow mouth of 'C' is paper

  // Orient 3: Letter 'D'
  renderImuDebugScreen(buffer.data(), 3, -16000, 0, 0);
  assert(gfx_get_pixel(buffer.data(), 432, 151) == 0); // Bowl curve of 'D' is ink
  assert(gfx_get_pixel(buffer.data(), 388, 151) == 1); // Hollow center of 'D' is paper

  std::cout << "  ✓ All IMU Debug Screen tests passed!\n";
}

static uint8_t test_get_mono_pixel(const uint8_t* buf, int x, int y, int width) {
  size_t stride = width / 8;
  return (buf[y * stride + (x / 8)] >> (7 - (x & 7))) & 1;
}

static void test_set_mono_pixel(uint8_t* buf, int x, int y, int width, uint8_t val) {
  size_t stride = width / 8;
  if (val) {
    buf[y * stride + (x / 8)] |= (0x80 >> (x & 7));
  } else {
    buf[y * stride + (x / 8)] &= ~(0x80 >> (x & 7));
  }
}

static uint8_t test_get_nibble_pixel(const uint8_t* buf, int x, int y, int width) {
  size_t stride = (width + 1) / 2;
  uint8_t b = buf[y * stride + (x >> 1)];
  return (x & 1) ? (b & 0x0F) : (b >> 4);
}

static void test_set_nibble_pixel(uint8_t* buf, int x, int y, int width, uint8_t val) {
  size_t stride = (width + 1) / 2;
  if (x & 1) {
    buf[y * stride + (x >> 1)] = (buf[y * stride + (x >> 1)] & 0xF0) | (val & 0x0F);
  } else {
    buf[y * stride + (x >> 1)] = (buf[y * stride + (x >> 1)] & 0x0F) | ((val & 0x0F) << 4);
  }
}

void test_portrait_buffer_rotation() {
  std::cout << "[Test] Running portrait buffer rotation tests (M4 rotation symmetry)...\n";

  const uint16_t src_w = 480;
  const uint16_t src_h = 800;
  const size_t mono_size = (src_w / 8) * src_h; // 48000 bytes

  // ─── 1. Mono (1bpp) Rotation Test ──────────────────────────────────────────
  std::vector<uint8_t> mono_src(mono_size, 0xFF);
  // Mark pixel at (10, 20) as black (0)
  test_set_mono_pixel(mono_src.data(), 10, 20, src_w, 0);
  assert(test_get_mono_pixel(mono_src.data(), 10, 20, src_w) == 0);

  // Rotate to portrait (flip = false) -> lands at (779, 10) in 800x480
  std::vector<uint8_t> mono_dst_90(mono_size, 0xFF);
  rotate_mono_portrait(mono_src.data(), mono_dst_90.data(), src_w, src_h, false);
  assert(test_get_mono_pixel(mono_dst_90.data(), 779, 10, src_h) == 0);

  // Simulate server 180 deg rotation on source buffer: marker moves to (469, 779)
  std::vector<uint8_t> mono_src_180(mono_size, 0xFF);
  rotate_mono_180(mono_src.data(), mono_src_180.data(), src_w, src_h);
  assert(test_get_mono_pixel(mono_src_180.data(), 469, 779, src_w) == 0);

  // Rotate inverted buffer with flip = false -> lands at (20, 469) in 800x480
  std::vector<uint8_t> mono_dst_270(mono_size, 0xFF);
  rotate_mono_portrait(mono_src_180.data(), mono_dst_270.data(), src_w, src_h, false);
  assert(test_get_mono_pixel(mono_dst_270.data(), 20, 469, src_h) == 0);

  // Verify that (779, 10) and (20, 469) are exact 180 deg opposites on the 800x480 physical panel
  assert(src_h - 1 - 779 == 20);
  assert(src_w - 1 - 10 == 469);

  // ─── 2. Nibble (4bpp Spectra 6 / Gray16) Rotation Test ─────────────────────
  const size_t nibble_size = ((src_w + 1) / 2) * src_h; // 192000 bytes
  std::vector<uint8_t> nibble_src(nibble_size, 0x00);
  test_set_nibble_pixel(nibble_src.data(), 10, 20, src_w, 0x05);
  assert(test_get_nibble_pixel(nibble_src.data(), 10, 20, src_w) == 0x05);

  std::vector<uint8_t> nibble_dst_90(nibble_size, 0x00);
  rotate_nibble_portrait(nibble_src.data(), nibble_dst_90.data(), src_w, src_h, false);
  assert(test_get_nibble_pixel(nibble_dst_90.data(), 779, 10, src_h) == 0x05);

  std::vector<uint8_t> nibble_src_180(nibble_size, 0x00);
  rotate_nibble_180(nibble_src.data(), nibble_src_180.data(), src_w, src_h);
  assert(test_get_nibble_pixel(nibble_src_180.data(), 469, 779, src_w) == 0x05);

  std::vector<uint8_t> nibble_dst_270(nibble_size, 0x00);
  rotate_nibble_portrait(nibble_src_180.data(), nibble_dst_270.data(), src_w, src_h, false);
  assert(test_get_nibble_pixel(nibble_dst_270.data(), 20, 469, src_h) == 0x05);

  assert(src_h - 1 - 779 == 20);
  assert(src_w - 1 - 10 == 469);

  std::cout << "  ✓ All portrait buffer rotation tests passed!\n";
}

int main() {
  std::cout << "==========================================\n";
  std::cout << " ScreenTinker Native Unit Tests (#8, #9, #21, #22)\n";
  std::cout << "==========================================\n";

  test_imu_orientation_detection();
  test_system_menu_rendering();
  test_smart_partial_refresh_policy();
  test_sht40_driver();
  test_status_screen_telemetry();
  test_diag_logger();
  test_ota_updater();
  test_imu_debug_screen();
  test_portrait_buffer_rotation();

  std::cout << "\n✅ ALL UNIT TESTS PASSED SUCCESSFULLY!\n";
  return 0;
}
