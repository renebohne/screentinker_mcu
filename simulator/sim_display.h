#pragma once
#include "sim_config.h"
#include <cstdint>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class SimDisplay {
public:
  SimDisplay(bool headless = false, int width = 800, int height = 480, int scale = 1, SimColorMode colorMode = SIM_COLOR_1BPP_MONO, bool isLedDevice = false);
  ~SimDisplay();

  bool init();
  void renderBuffer(const uint8_t* buffer);
  bool saveBmp(const uint8_t* buffer, const std::string& path);
  bool pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested);
  bool pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested, bool& forceFullRequested);
  bool pollEvents(bool& wakeRequested, int& batteryDelta, bool& powerToggled, bool& rotateRequested, bool& forceFullRequested, int& touchX, int& touchY, bool& touchClicked);
  void updateDimensions(int width, int height);
  void setColorMode(SimColorMode mode);
  void setLedMode(bool isLed) { m_isLedDevice = isLed; computeLayout(); }

  int width() const { return m_width; }
  int height() const { return m_height; }
  int renderWidth() const { return m_renderWidth; }
  int renderHeight() const { return m_renderHeight; }
  int scale() const { return m_scale; }
  SimColorMode colorMode() const { return m_colorMode; }
  bool isLedMode() const { return m_isLedDevice || m_colorMode == SIM_COLOR_RGB888; }
  size_t bufferSize() const;

private:
  void computeLayout();

  bool m_headless;
  int m_width;
  int m_height;
  int m_scale;
  SimColorMode m_colorMode;
  bool m_isLedDevice;

  int m_renderWidth = 0;
  int m_renderHeight = 0;
  int m_ledBeadSize = 0;
  int m_ledGap = 0;
  int m_ledPadX = 0;
  int m_ledPadY = 0;

  SDL_Window* m_window = nullptr;
  SDL_Renderer* m_renderer = nullptr;
  SDL_Texture* m_texture = nullptr;
  uint32_t* m_pixelBuffer = nullptr;
};
