#include "sim_display.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>

static const uint32_t s_spectra6_palette[6] = {
  0xFF141414, // 0: Black (R:20, G:20, B:20)
  0xFFFBFBFB, // 1: White (R:251, G:251, B:251)
  0xFFF5DE00, // 2: Yellow (R:245, G:222, B:0)
  0xFFD71920, // 3: Red (R:215, G:25, B:32)
  0xFF005BAC, // 4: Blue (R:0, G:91, B:172)
  0xFF00873D  // 5: Green (R:0, G:135, B:61)
};

SimDisplay::SimDisplay(bool headless, int width, int height, int scale, SimColorMode colorMode)
  : m_headless(headless), m_width(width), m_height(height), m_scale(scale > 0 ? scale : 1), m_colorMode(colorMode) {
  m_pixelBuffer = new uint32_t[m_width * m_height];
}

size_t SimDisplay::bufferSize() const {
  switch (m_colorMode) {
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
    std::cout << "[Display] Initialized in Headless mode (" << m_width << "x" << m_height << ").\n";
    return true;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "[Display] SDL_Init Error: " << SDL_GetError() << " -> Falling back to headless mode.\n";
    m_headless = true;
    return true;
  }

  int winWidth = m_width * m_scale;
  int winHeight = m_height * m_scale;
  char winTitle[128];
  snprintf(winTitle, sizeof(winTitle), "ScreenTinker E-Paper Simulator (%dx%d)", m_width, m_height);
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
    SDL_RenderSetLogicalSize(m_renderer, m_width, m_height);
  }

  m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, m_width, m_height);

  std::cout << "[Display] SDL2 Window created: " << winWidth << "x" << winHeight
            << " E-Paper Viewport (logical " << m_width << "x" << m_height << ").\n";
  return true;
}

void SimDisplay::renderBuffer(const uint8_t* buffer) {
  if (!buffer) return;

  if (m_colorMode == SIM_COLOR_SPECTRA6) {
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        size_t byte_idx = ((size_t)y * (m_width / 2)) + (x / 2);
        uint8_t nibble = (x & 1) ? (buffer[byte_idx] & 0x0F) : ((buffer[byte_idx] >> 4) & 0x0F);
        uint32_t color = (nibble < 6) ? s_spectra6_palette[nibble] : 0xFFFBFBFB;
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
    SDL_UpdateTexture(m_texture, nullptr, m_pixelBuffer, m_width * sizeof(uint32_t));
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

  int rowSize = ((m_width * 3 + 3) / 4) * 4; // 24-bit padded row size
  uint32_t imageSize = rowSize * m_height;
  uint32_t fileSize = 54 + imageSize;

  uint8_t header[54] = {
    'B', 'M',
    (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
    0, 0, 0, 0,
    54, 0, 0, 0, // offset
    40, 0, 0, 0, // biSize
    (uint8_t)(m_width), (uint8_t)(m_width >> 8), (uint8_t)(m_width >> 16), (uint8_t)(m_width >> 24),
    (uint8_t)(m_height), (uint8_t)(m_height >> 8), (uint8_t)(m_height >> 16), (uint8_t)(m_height >> 24),
    1, 0, // biPlanes
    24, 0, // biBitCount
    0, 0, 0, 0, // biCompression
    (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  out.write((char*)header, 54);

  std::vector<uint8_t> row(rowSize, 0);
  // BMP writes bottom-to-top
  for (int y = m_height - 1; y >= 0; --y) {
    for (int x = 0; x < m_width; ++x) {
      uint32_t pix = m_pixelBuffer[y * m_width + x];
      row[x * 3 + 0] = (uint8_t)(pix & 0xFF);         // B
      row[x * 3 + 1] = (uint8_t)((pix >> 8) & 0xFF);  // G
      row[x * 3 + 2] = (uint8_t)((pix >> 16) & 0xFF); // R
    }
    out.write((char*)row.data(), rowSize);
  }

  std::cout << "[Display] Saved snapshot to " << path << " (" << m_width << "x" << m_height << " BMP)\n";
  return true;
}

void SimDisplay::updateDimensions(int width, int height) {
  if (width == m_width && height == m_height) return;

  m_width = width;
  m_height = height;

  delete[] m_pixelBuffer;
  m_pixelBuffer = new uint32_t[m_width * m_height];

  if (!m_headless && m_window && m_renderer) {
    if (m_texture) {
      SDL_DestroyTexture(m_texture);
      m_texture = nullptr;
    }
    int winWidth = m_width * m_scale;
    int winHeight = m_height * m_scale;
    SDL_SetWindowSize(m_window, winWidth, winHeight);
    SDL_RenderSetLogicalSize(m_renderer, m_width, m_height);
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, m_width, m_height);
    char winTitle[128];
    snprintf(winTitle, sizeof(winTitle), "ScreenTinker E-Paper Simulator (%dx%d)", m_width, m_height);
    SDL_SetWindowTitle(m_window, winTitle);
    std::cout << "[Display] Viewport resized to " << winWidth << "x" << winHeight
              << " (logical " << m_width << "x" << m_height << ").\n";
  }
}

bool SimDisplay::pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested) {
  if (m_headless) return true;

  wakeRequested = false;
  batteryDelta = 0;
  powerToggled = false;
  rotateRequested = false;

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return false; // Exit simulator
    }
    if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_q:
          return false;
        case SDLK_SPACE:
        case SDLK_RETURN:
          wakeRequested = true;
          std::cout << "[Simulator] Manual wake-up triggered by keypress!\n";
          break;
        case SDLK_r:
          rotateRequested = true;
          std::cout << "[Simulator] Manual rotation toggle triggered by keypress 'R'!\n";
          break;
        case SDLK_UP:
          batteryDelta = +10;
          break;
        case SDLK_DOWN:
          batteryDelta = -10;
          break;
        case SDLK_b:
          // Toggle battery low/full
          batteryDelta = 999; // Special toggle code
          break;
        case SDLK_p:
          // Toggle power source (battery <-> USB mains)
          powerToggled = true;
          break;
      }
    }
  }
  return true;
}
