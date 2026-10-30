#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Starting corner where the physical LED chain (index 0) enters the panel.
 */
typedef enum {
  MATRIX_ORIGIN_TOP_LEFT = 0,
  MATRIX_ORIGIN_TOP_RIGHT = 1,
  MATRIX_ORIGIN_BOTTOM_LEFT = 2,
  MATRIX_ORIGIN_BOTTOM_RIGHT = 3
} MatrixOrigin;

/**
 * Major scanning axis of the physical wiring traces.
 */
typedef enum {
  MATRIX_AXIS_HORIZONTAL = 0, // Row-major (horizontal runs first)
  MATRIX_AXIS_VERTICAL = 1    // Column-major (vertical runs first)
} MatrixAxis;

/**
 * Wiring direction traversal across subsequent rows/columns.
 */
typedef enum {
  MATRIX_WIRING_PROGRESSIVE = 0, // All rows/columns run in identical direction
  MATRIX_WIRING_SERPENTINE = 1   // Direction alternates every row/column (zig-zag / snake)
} MatrixWiring;

/**
 * LED Matrix Geometry & Physical Wiring Topology Configuration
 */
typedef struct {
  uint16_t width;
  uint16_t height;
  MatrixOrigin origin;
  MatrixAxis axis;
  MatrixWiring wiring;
} LedMatrixConfig;

/**
 * Map 2D Cartesian framebuffer coordinates (x, y) [0..width-1, 0..height-1]
 * to the 1D sequential physical LED index [0..(width*height)-1].
 */
static inline uint16_t led_matrix_xy_to_index(uint16_t x, uint16_t y, const LedMatrixConfig* cfg) {
  if (!cfg || cfg->width == 0 || cfg->height == 0) return 0;
  if (x >= cfg->width) x = cfg->width - 1;
  if (y >= cfg->height) y = cfg->height - 1;

  // 1. Account for Starting Origin
  uint16_t px = x;
  uint16_t py = y;

  if (cfg->origin == MATRIX_ORIGIN_TOP_RIGHT || cfg->origin == MATRIX_ORIGIN_BOTTOM_RIGHT) {
    px = (cfg->width - 1) - px;
  }
  if (cfg->origin == MATRIX_ORIGIN_BOTTOM_LEFT || cfg->origin == MATRIX_ORIGIN_BOTTOM_RIGHT) {
    py = (cfg->height - 1) - py;
  }

  // 2. Map coordinates according to Axis and Serpentine / Progressive wiring
  if (cfg->axis == MATRIX_AXIS_HORIZONTAL) {
    // Row-major
    if (cfg->wiring == MATRIX_WIRING_SERPENTINE && (py % 2 == 1)) {
      return (uint16_t)(py * cfg->width + (cfg->width - 1 - px));
    } else {
      return (uint16_t)(py * cfg->width + px);
    }
  } else {
    // Column-major
    if (cfg->wiring == MATRIX_WIRING_SERPENTINE && (px % 2 == 1)) {
      return (uint16_t)(px * cfg->height + (cfg->height - 1 - py));
    } else {
      return (uint16_t)(px * cfg->height + py);
    }
  }
}

/**
 * Inverse mapping: Given a 1D sequential LED index, calculate the 2D Cartesian (x, y) coordinates.
 */
static inline void led_matrix_index_to_xy(uint16_t index, const LedMatrixConfig* cfg, uint16_t* outX, uint16_t* outY) {
  if (!cfg || cfg->width == 0 || cfg->height == 0) {
    if (outX) *outX = 0;
    if (outY) *outY = 0;
    return;
  }
  uint32_t totalLeds = (uint32_t)cfg->width * cfg->height;
  if (index >= totalLeds) index = (uint16_t)(totalLeds - 1);

  uint16_t px = 0;
  uint16_t py = 0;

  if (cfg->axis == MATRIX_AXIS_HORIZONTAL) {
    py = index / cfg->width;
    uint16_t rem = index % cfg->width;
    if (cfg->wiring == MATRIX_WIRING_SERPENTINE && (py % 2 == 1)) {
      px = (cfg->width - 1) - rem;
    } else {
      px = rem;
    }
  } else {
    px = index / cfg->height;
    uint16_t rem = index % cfg->height;
    if (cfg->wiring == MATRIX_WIRING_SERPENTINE && (px % 2 == 1)) {
      py = (cfg->height - 1) - rem;
    } else {
      py = rem;
    }
  }

  // Reverse Origin normalization
  if (cfg->origin == MATRIX_ORIGIN_TOP_RIGHT || cfg->origin == MATRIX_ORIGIN_BOTTOM_RIGHT) {
    px = (cfg->width - 1) - px;
  }
  if (cfg->origin == MATRIX_ORIGIN_BOTTOM_LEFT || cfg->origin == MATRIX_ORIGIN_BOTTOM_RIGHT) {
    py = (cfg->height - 1) - py;
  }

  if (outX) *outX = px;
  if (outY) *outY = py;
}

#ifdef __cplusplus
}
#endif
