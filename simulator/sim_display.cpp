#include "sim_display.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>

SimDisplay::SimDisplay(bool headless, int width, int height, int scale)
  : m_headless(headless), m_width(width), m_height(height), m_scale(scale > 0 ? scale : 1) {
  m_pixelBuffer = new uint32_t[m_width * m_height];
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
    std::cout << "[Display] Initialized in Headless mode (800x480 mono).\n";
    return true;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "[Display] SDL_Init Error: " << SDL_GetError() << " -> Falling back to headless mode.\n";
    m_headless = true;
    return true;
  }

  int winWidth = m_width * m_scale;
  int winHeight = m_height * m_scale;
  m_window = SDL_CreateWindow("ScreenTinker E-Paper Simulator (800x480)",
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

void SimDisplay::renderBuffer(const uint8_t* monoBuffer) {
  if (!monoBuffer) return;

  // Convert 1-bit monochrome (1 = White, 0 = Black) to 32-bit ARGB
  // E-Paper paper background: 0xFFF7F7F7, ink: 0xFF181818 for realistic e-paper look
  const uint32_t c_white = 0xFFFBFBFB;
  const uint32_t c_black = 0xFF141414;

  for (int y = 0; y < m_height; ++y) {
    for (int x = 0; x < m_width; ++x) {
      size_t byte_idx = (y * (m_width / 8)) + (x / 8);
      uint8_t bit = (monoBuffer[byte_idx] >> (7 - (x & 7))) & 1;
      m_pixelBuffer[y * m_width + x] = bit ? c_white : c_black;
    }
  }

  if (!m_headless && m_renderer && m_texture) {
    SDL_UpdateTexture(m_texture, nullptr, m_pixelBuffer, m_width * sizeof(uint32_t));
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
  }
}

bool SimDisplay::saveBmp(const uint8_t* monoBuffer, const std::string& path) {
  if (!monoBuffer || path.empty()) return false;

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
      size_t byte_idx = (y * (m_width / 8)) + (x / 8);
      uint8_t bit = (monoBuffer[byte_idx] >> (7 - (x & 7))) & 1;
      uint8_t val = bit ? 255 : 0;
      row[x * 3 + 0] = val; // B
      row[x * 3 + 1] = val; // G
      row[x * 3 + 2] = val; // R
    }
    out.write((char*)row.data(), rowSize);
  }

  std::cout << "[Display] Saved snapshot to " << path << " (" << m_width << "x" << m_height << " BMP)\n";
  return true;
}

bool SimDisplay::pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled) {
  if (m_headless) return true;

  wakeRequested = false;
  batteryDelta = 0;
  powerToggled = false;

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
