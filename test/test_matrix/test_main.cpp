#include <cassert>
#include <iostream>
#include <vector>
#include "../include/led_matrix_map.h"

int main() {
  std::cout << "[Test] Verifying LED Matrix Mapping...\n";

  MatrixOrigin origins[] = {MATRIX_ORIGIN_TOP_LEFT, MATRIX_ORIGIN_TOP_RIGHT, MATRIX_ORIGIN_BOTTOM_LEFT, MATRIX_ORIGIN_BOTTOM_RIGHT};
  MatrixAxis axes[] = {MATRIX_AXIS_HORIZONTAL, MATRIX_AXIS_VERTICAL};
  MatrixWiring wirings[] = {MATRIX_WIRING_PROGRESSIVE, MATRIX_WIRING_SERPENTINE};

  uint16_t testSizes[][2] = {{5, 1}, {8, 8}, {32, 8}, {64, 64}};

  for (auto& sz : testSizes) {
    uint16_t w = sz[0];
    uint16_t h = sz[1];

    for (auto orig : origins) {
      for (auto axis : axes) {
        for (auto wiring : wirings) {
          LedMatrixConfig cfg = {w, h, orig, axis, wiring};
          std::vector<bool> seen(w * h, false);

          for (uint16_t y = 0; y < h; y++) {
            for (uint16_t x = 0; x < w; x++) {
              uint16_t idx = led_matrix_xy_to_index(x, y, &cfg);
              assert(idx < w * h);
              assert(!seen[idx]); // Must be unique (bijection)
              seen[idx] = true;

              uint16_t rx = 0, ry = 0;
              led_matrix_index_to_xy(idx, &cfg, &rx, &ry);
              assert(rx == x);
              assert(ry == y);
            }
          }
        }
      }
    }
  }

  std::cout << "[Test] All LED Matrix mappings passed successfully (100% bijective bijection check)!\n";
  return 0;
}
