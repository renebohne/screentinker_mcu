#pragma once
#include <cstdint>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class SimDisplay {
public:
  SimDisplay(bool headless = false, int width = 800, int height = 480);
  ~SimDisplay();

  bool init();
  void renderBuffer(const uint8_t* monoBuffer);
  bool saveBmp(const uint8_t* monoBuffer, const std::string& path);
  bool pollEvents(bool& wakeRequested, int& batteryDelta);

  int width() const { return m_width; }
  int height() const { return m_height; }
  size_t bufferSize() const { return (m_width * m_height) / 8; }

private:
  bool m_headless;
  int m_width;
  int m_height;
  SDL_Window* m_window = nullptr;
  SDL_Renderer* m_renderer = nullptr;
  SDL_Texture* m_texture = nullptr;
  uint32_t* m_pixelBuffer = nullptr;
};
