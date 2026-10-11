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

int main() {
  std::cout << "==========================================\n";
  std::cout << " ScreenTinker Native Unit Tests (Issue #21)\n";
  std::cout << "==========================================\n";

  test_imu_orientation_detection();
  test_system_menu_rendering();

  std::cout << "\n✅ ALL UNIT TESTS PASSED SUCCESSFULLY!\n";
  return 0;
}
