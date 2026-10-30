#include "sim_display.h"
#include "diag_logger.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>

static const uint32_t s_spectra6_palette[7] = {
  0xFF141414, // 0: Black (R:20, G:20, B:20)
  0xFFFBFBFB, // 1: White (R:251, G:251, B:251)
  0xFFF5DE00, // 2: Yellow (R:245, G:222, B:0)
  0xFFD71920, // 3: Red (R:215, G:25, B:32)
  0xFFFFFFFF, // 4: Unused slot
  0xFF005BAC, // 5: Blue (R:0, G:91, B:172)
  0xFF00873D  // 6: Green (R:0, G:135, B:61)
};

void SimDisplay::computeLayout() {
  if (isLedMode()) {
    int maxDim = std::max(m_width, m_height);
    if (maxDim <= 8) {
      m_ledBeadSize = 56;
      m_ledGap = 14;
      m_ledPadX = 20;
      m_ledPadY = 20;
    } else if (maxDim <= 16) {
      m_ledBeadSize = 36;
      m_ledGap = 10;
      m_ledPadX = 18;
      m_ledPadY = 18;
    } else if (maxDim <= 32) {
      m_ledBeadSize = 22;
      m_ledGap = 6;
      m_ledPadX = 16;
      m_ledPadY = 16;
    } else if (maxDim <= 64) {
      m_ledBeadSize = 11;
      m_ledGap = 3;
      m_ledPadX = 14;
      m_ledPadY = 14;
    } else {
      m_ledBeadSize = std::max(4, 768 / maxDim);
      m_ledGap = std::max(1, m_ledBeadSize / 4);
      m_ledPadX = 10;
      m_ledPadY = 10;
    }
    m_renderWidth = m_ledPadX * 2 + m_width * (m_ledBeadSize + m_ledGap) - m_ledGap;
    m_renderHeight = m_ledPadY * 2 + m_height * (m_ledBeadSize + m_ledGap) - m_ledGap;
  } else {
    m_renderWidth = m_width;
    m_renderHeight = m_height;
    m_ledBeadSize = 0;
    m_ledGap = 0;
    m_ledPadX = 0;
    m_ledPadY = 0;
  }
}

SimDisplay::SimDisplay(bool headless, int width, int height, int scale, SimColorMode colorMode, bool isLedDevice)
  : m_headless(headless), m_width(width), m_height(height), m_scale(scale > 0 ? scale : 1),
    m_colorMode(colorMode), m_isLedDevice(isLedDevice) {
  computeLayout();
  m_pixelBuffer = new uint32_t[m_renderWidth * m_renderHeight];
}

size_t SimDisplay::bufferSize() const {
  switch (m_colorMode) {
    case SIM_COLOR_RGB565:
      return (size_t)m_width * m_height * 2;
    case SIM_COLOR_RGB888:
      return (size_t)m_width * m_height * 3;
    case SIM_COLOR_SPECTRA6:
    case SIM_COLOR_GRAY16:
      return ((size_t)m_width * m_height) / 2;
    case SIM_COLOR_GRAY4:
      return ((size_t)m_width * m_height) / 4;
    case SIM_COLOR_1BPP_MONO:
    default:
      return ((size_t)m_width * m_height) / 8;
  }
}

SimDisplay::~SimDisplay() {
  delete[] m_pixelBuffer;
  if (m_texture) SDL_DestroyTexture(m_texture);
  if (m_renderer) SDL_DestroyRenderer(m_renderer);
  if (m_window) SDL_DestroyWindow(m_window);
  if (!m_headless) SDL_Quit();
}

bool SimDisplay::init() {
  if (m_headless) {
    std::cout << "[Display] Initialized in Headless mode (" << m_width << "x" << m_height
              << (isLedMode() ? " LED Viewport" : " E-Paper Viewport") << ").\n";
    return true;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "[Display] SDL_Init Error: " << SDL_GetError() << " -> Falling back to headless mode.\n";
    m_headless = true;
    return true;
  }

  int winWidth = m_renderWidth * m_scale;
  int winHeight = m_renderHeight * m_scale;
  char winTitle[128];
  if (isLedMode()) {
    snprintf(winTitle, sizeof(winTitle), "ScreenTinker LED Matrix Simulator (%dx%d)", m_width, m_height);
  } else {
    snprintf(winTitle, sizeof(winTitle), "ScreenTinker E-Paper Simulator (%dx%d)", m_width, m_height);
  }

  m_window = SDL_CreateWindow(winTitle,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              winWidth, winHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!m_window) {
    std::cerr << "[Display] SDL_CreateWindow Error: " << SDL_GetError() << "\n";
    m_headless = true;
    return true;
  }

  m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!m_renderer) {
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
  }

  if (m_renderer) {
    SDL_RenderSetLogicalSize(m_renderer, m_renderWidth, m_renderHeight);
  }

  m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, m_renderWidth, m_renderHeight);

  std::cout << "[Display] SDL2 Window created: " << winWidth << "x" << winHeight
            << (isLedMode() ? " (LED Matrix Viewport: " : " (E-Paper Viewport: ")
            << m_width << "x" << m_height << ", logical " << m_renderWidth << "x" << m_renderHeight << ").\n";
  return true;
}

void SimDisplay::setColorMode(SimColorMode mode) {
  if (m_colorMode == mode) return;
  m_colorMode = mode;
  computeLayout();

  delete[] m_pixelBuffer;
  m_pixelBuffer = new uint32_t[m_renderWidth * m_renderHeight];

  if (!m_headless && m_window && m_renderer) {
    if (m_texture) {
      SDL_DestroyTexture(m_texture);
      m_texture = nullptr;
    }
    int winWidth = m_renderWidth * m_scale;
    int winHeight = m_renderHeight * m_scale;
    SDL_SetWindowSize(m_window, winWidth, winHeight);
    SDL_RenderSetLogicalSize(m_renderer, m_renderWidth, m_renderHeight);
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, m_renderWidth, m_renderHeight);
  }
}

void SimDisplay::renderBuffer(const uint8_t* buffer) {
  if (!buffer) return;

  if (isLedMode()) {
    // Fill background with dark matte PCB
    const uint32_t c_pcb = 0xFF121215;
    std::fill_n(m_pixelBuffer, (size_t)m_renderWidth * m_renderHeight, c_pcb);

    int radius = std::max(2, m_ledBeadSize / 4);
    int r2 = radius * radius;
    float maxDist = m_ledBeadSize * 0.55f;

    for (int ly = 0; ly < m_height; ++ly) {
      for (int lx = 0; lx < m_width; ++lx) {
        size_t idx = ((size_t)ly * m_width + lx) * 3;
        uint8_t r = buffer[idx + 0];
        uint8_t g = buffer[idx + 1];
        uint8_t b = buffer[idx + 2];

        int bx0 = m_ledPadX + lx * (m_ledBeadSize + m_ledGap);
        int by0 = m_ledPadY + ly * (m_ledBeadSize + m_ledGap);
        float cx = bx0 + m_ledBeadSize * 0.5f;
        float cy = by0 + m_ledBeadSize * 0.5f;

        for (int dy = 0; dy < m_ledBeadSize; ++dy) {
          for (int dx = 0; dx < m_ledBeadSize; ++dx) {
            int px = bx0 + dx;
            int py = by0 + dy;
            if (px < 0 || px >= m_renderWidth || py < 0 || py >= m_renderHeight) continue;

            bool isCorner = false;
            int cdx = 0, cdy = 0;
            if (dx < radius && dy < radius) {
              cdx = radius - 1 - dx;
              cdy = radius - 1 - dy;
              isCorner = true;
            } else if (dx >= m_ledBeadSize - radius && dy < radius) {
              cdx = dx - (m_ledBeadSize - radius);
              cdy = radius - 1 - dy;
              isCorner = true;
            } else if (dx < radius && dy >= m_ledBeadSize - radius) {
              cdx = radius - 1 - dx;
              cdy = dy - (m_ledBeadSize - radius);
              isCorner = true;
            } else if (dx >= m_ledBeadSize - radius && dy >= m_ledBeadSize - radius) {
              cdx = dx - (m_ledBeadSize - radius);
              cdy = dy - (m_ledBeadSize - radius);
              isCorner = true;
            }

            if (isCorner && (cdx * cdx + cdy * cdy > r2)) {
              continue; // Outside rounded square corner
            }

            if (r == 0 && g == 0 && b == 0) {
              // Unlit LED appearance (dark resin & subtle bezel)
              bool isBezel = (dx == 0 || dx == m_ledBeadSize - 1 || dy == 0 || dy == m_ledBeadSize - 1 ||
                             (isCorner && cdx * cdx + cdy * cdy >= (radius - 1) * (radius - 1)));
              if (isBezel) {
                m_pixelBuffer[py * m_renderWidth + px] = 0xFF28282E;
              } else {
                bool isDie = (m_ledBeadSize >= 16 &&
                              dx >= m_ledBeadSize / 3 && dx < m_ledBeadSize * 2 / 3 &&
                              dy >= m_ledBeadSize / 3 && dy < m_ledBeadSize * 2 / 3);
                m_pixelBuffer[py * m_renderWidth + px] = isDie ? 0xFF141416 : 0xFF1C1C20;
              }
            } else {
              // Lit LED appearance: rounded square with realistic bulb highlight
              float dist = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
              float normDist = std::min(1.0f, dist / maxDist);
              float highlight = 1.0f + 0.32f * (1.0f - normDist);

              bool isEdge = (isCorner && cdx * cdx + cdy * cdy >= (radius - 1) * (radius - 1)) ||
                            (dx == 0 || dx == m_ledBeadSize - 1 || dy == 0 || dy == m_ledBeadSize - 1);
              if (isEdge) highlight *= 0.88f;

              uint8_t rOut = (uint8_t)std::min(255, (int)(r * highlight));
              uint8_t gOut = (uint8_t)std::min(255, (int)(g * highlight));
              uint8_t bOut = (uint8_t)std::min(255, (int)(b * highlight));

              m_pixelBuffer[py * m_renderWidth + px] = 0xFF000000 | (rOut << 16) | (gOut << 8) | bOut;
            }
          }
        }
      }
    }
  } else if (m_colorMode == SIM_COLOR_RGB565) {
    const uint16_t* src16 = reinterpret_cast<const uint16_t*>(buffer);
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        uint16_t p = src16[y * m_width + x];
        uint8_t r = ((p >> 11) & 0x1F) * 255 / 31;
        uint8_t g = ((p >> 5) & 0x3F) * 255 / 63;
        uint8_t b = (p & 0x1F) * 255 / 31;
        m_pixelBuffer[y * m_width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
      }
    }
  } else if (m_colorMode == SIM_COLOR_SPECTRA6) {
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        size_t byte_idx = ((size_t)y * (m_width / 2)) + (x / 2);
        uint8_t nibble = (x & 1) ? (buffer[byte_idx] & 0x0F) : ((buffer[byte_idx] >> 4) & 0x0F);
        uint32_t color = (nibble < 7) ? s_spectra6_palette[nibble] : 0xFFFBFBFB;
        m_pixelBuffer[y * m_width + x] = color;
      }
    }
  } else if (m_colorMode == SIM_COLOR_GRAY16) {
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        size_t byte_idx = ((size_t)y * (m_width / 2)) + (x / 2);
        uint8_t nibble = (x & 1) ? (buffer[byte_idx] & 0x0F) : ((buffer[byte_idx] >> 4) & 0x0F);
        uint8_t gray = nibble * 17;
        m_pixelBuffer[y * m_width + x] = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
      }
    }
  } else if (m_colorMode == SIM_COLOR_GRAY4) {
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        size_t byte_idx = ((size_t)y * (m_width / 4)) + (x / 4);
        uint8_t shift = 6 - 2 * (x & 3);
        uint8_t val = (buffer[byte_idx] >> shift) & 0x03;
        uint8_t gray = val * 85;
        m_pixelBuffer[y * m_width + x] = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
      }
    }
  } else {
    // 1-bit monochrome (1 = White, 0 = Black)
    const uint32_t c_white = 0xFFFBFBFB;
    const uint32_t c_black = 0xFF141414;
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        size_t byte_idx = ((size_t)y * (m_width / 8)) + (x / 8);
        uint8_t bit = (buffer[byte_idx] >> (7 - (x & 7))) & 1;
        m_pixelBuffer[y * m_width + x] = bit ? c_white : c_black;
      }
    }
  }

  if (!m_headless && m_renderer && m_texture) {
    SDL_UpdateTexture(m_texture, nullptr, m_pixelBuffer, m_renderWidth * sizeof(uint32_t));
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
  }
}

bool SimDisplay::saveBmp(const uint8_t* buffer, const std::string& path) {
  if (!buffer || path.empty()) return false;

  renderBuffer(buffer);

  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "[Display] Failed to open " << path << " for writing.\n";
    return false;
  }

  int rowSize = ((m_renderWidth * 3 + 3) / 4) * 4; // 24-bit padded row size
  uint32_t imageSize = rowSize * m_renderHeight;
  uint32_t fileSize = 54 + imageSize;

  uint8_t header[54] = {
    'B', 'M',
    (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
    0, 0, 0, 0,
    54, 0, 0, 0, // offset
    40, 0, 0, 0, // biSize
    (uint8_t)(m_renderWidth), (uint8_t)(m_renderWidth >> 8), (uint8_t)(m_renderWidth >> 16), (uint8_t)(m_renderWidth >> 24),
    (uint8_t)(m_renderHeight), (uint8_t)(m_renderHeight >> 8), (uint8_t)(m_renderHeight >> 16), (uint8_t)(m_renderHeight >> 24),
    1, 0, // biPlanes
    24, 0, // biBitCount
    0, 0, 0, 0, // biCompression
    (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  out.write((char*)header, 54);

  std::vector<uint8_t> row(rowSize, 0);
  // BMP writes bottom-to-top
  for (int y = m_renderHeight - 1; y >= 0; --y) {
    for (int x = 0; x < m_renderWidth; ++x) {
      uint32_t pix = m_pixelBuffer[y * m_renderWidth + x];
      row[x * 3 + 0] = (uint8_t)(pix & 0xFF);         // B
      row[x * 3 + 1] = (uint8_t)((pix >> 8) & 0xFF);  // G
      row[x * 3 + 2] = (uint8_t)((pix >> 16) & 0xFF); // R
    }
    out.write((char*)row.data(), rowSize);
  }

  std::cout << "[Display] Saved snapshot to " << path << " (" << m_renderWidth << "x" << m_renderHeight << " BMP)\n";
  return true;
}

void SimDisplay::updateDimensions(int width, int height) {
  if (width == m_width && height == m_height) return;

  m_width = width;
  m_height = height;
  computeLayout();

  delete[] m_pixelBuffer;
  m_pixelBuffer = new uint32_t[m_renderWidth * m_renderHeight];

  if (!m_headless && m_window && m_renderer) {
    if (m_texture) {
      SDL_DestroyTexture(m_texture);
      m_texture = nullptr;
    }
    int winWidth = m_renderWidth * m_scale;
    int winHeight = m_renderHeight * m_scale;
    SDL_SetWindowSize(m_window, winWidth, winHeight);
    SDL_RenderSetLogicalSize(m_renderer, m_renderWidth, m_renderHeight);
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, m_renderWidth, m_renderHeight);
    char winTitle[128];
    if (isLedMode()) {
      snprintf(winTitle, sizeof(winTitle), "ScreenTinker LED Matrix Simulator (%dx%d)", m_width, m_height);
    } else {
      snprintf(winTitle, sizeof(winTitle), "ScreenTinker E-Paper Simulator (%dx%d)", m_width, m_height);
    }
    SDL_SetWindowTitle(m_window, winTitle);
    std::cout << "[Display] Viewport resized to " << winWidth << "x" << winHeight
              << (isLedMode() ? " (LED Matrix: " : " (E-Paper: ")
              << m_width << "x" << m_height << ", logical " << m_renderWidth << "x" << m_renderHeight << ").\n";
  }
}

bool SimDisplay::pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested) {
  bool dummyForceFull = false;
  int dummyTx = 0, dummyTy = 0;
  bool dummyClicked = false;
  return pollEvents(wakeRequested, batteryDelta, powerToggled, rotateRequested, dummyForceFull, dummyTx, dummyTy, dummyClicked);
}

bool SimDisplay::pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested, bool& forceFullRequested) {
  int dummyTx = 0, dummyTy = 0;
  bool dummyClicked = false;
  return pollEvents(wakeRequested, batteryDelta, powerToggled, rotateRequested, forceFullRequested, dummyTx, dummyTy, dummyClicked);
}

bool SimDisplay::pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested, bool& forceFullRequested, int& touchX, int& touchY, bool& touchClicked) {
  if (m_headless) return true;

  wakeRequested = false;
  batteryDelta = 0;
  powerToggled = false;
  rotateRequested = false;
  forceFullRequested = false;
  touchX = 0;
  touchY = 0;
  touchClicked = false;

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return false; // Exit simulator
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
      touchX = event.button.x / m_scale;
      touchY = event.button.y / m_scale;
      touchClicked = true;
      wakeRequested = true;
    }
    if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_q:
          return false; // Exit simulator

        case SDLK_SPACE:
          wakeRequested = true;
          std::cout << "[Simulator] Immediate sync triggered via Spacebar.\n";
          break;

        case SDLK_f:
          forceFullRequested = true;
          wakeRequested = true;
          std::cout << "[Simulator] Forced Full Refresh requested via [F].\n";
          break;

        case SDLK_UP:
          batteryDelta = +5;
          wakeRequested = true;
          break;

        case SDLK_DOWN:
          batteryDelta = -5;
          wakeRequested = true;
          break;

        case SDLK_b:
          // Toggle battery to critical test levels
          batteryDelta = -50;
          wakeRequested = true;
          break;

        case SDLK_p:
          powerToggled = true;
          wakeRequested = true;
          break;

        case SDLK_r:
          rotateRequested = true;
          wakeRequested = true;
          break;

        default:
          break;
      }
    }
  }

  return true;
}
