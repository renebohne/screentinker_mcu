#pragma once
#include <string>
#include <cstdint>
#include "config.h"
#include "led_matrix_map.h"

enum SimColorMode {
  SIM_COLOR_1BPP_MONO = 0,
  SIM_COLOR_GRAY4 = 1,
  SIM_COLOR_GRAY16 = 2,
  SIM_COLOR_SPECTRA6 = 3,
  SIM_COLOR_RGB888 = 4,
  SIM_COLOR_RGB565 = 5
};

struct SimConfig {
  std::string serverUrl = "http://192.168.100.222:3001";
  std::string deviceId = "06813352-374a-476b-bca9-986ebdb1ff2c";
  std::string deviceToken = "";
#ifdef FIRMWARE_VERSION
  std::string firmwareVersion = FIRMWARE_VERSION;
#else
  std::string firmwareVersion = "1.4.2";
#endif
  int batteryPercent = 100;
  std::string powerSource = "battery"; // "battery" or "usb"
  bool isCharging = false;
  int wifiRssi = -68;
  int timeoutMs = 10000;
  int syncIntervalSec = 30;
  bool headless = false;
  bool oneShot = false;
  int maxCycles = 0;
  bool fastForwardSleep = true;
  int scale = 1;
  bool pairMode = false;
  std::string snapshotPath = "sim_output.bmp";
  std::string configFilePath = "simulator/sim_config.json";

  // Device Model, Resolution, Color & Refresh Capabilities
  std::string model = "sticky";
  std::string screenProfile = "seeed-reterminal-sticky";
  int nativeWidth = 800;
  int nativeHeight = 480;
  int width = 800;
  int height = 480;
  int rotation = 0;              // 0, 90, 180, 270
  std::string orientation = "";  // "landscape", "portrait", "landscape-flipped", "portrait-flipped"
  SimColorMode colorMode = SIM_COLOR_1BPP_MONO;
  bool supportsPartialRefresh = true;
  int fullRefreshTimeMs = 1800;
  int partialRefreshTimeMs = 350;
  bool touchSupported = true;
  bool isLedDevice = false;
  bool customDimensions = false;
  MatrixWiring matrixWiring = MATRIX_WIRING_SERPENTINE;
  MatrixAxis matrixAxis = MATRIX_AXIS_HORIZONTAL;
  MatrixOrigin matrixOrigin = MATRIX_ORIGIN_TOP_LEFT;

  // Sensirion SHT40 Temperature & Humidity Telemetry
  bool hasSht40 = true;
  float mockTemp = 21.5f;
  float mockHumidity = 45.0f;

  // Dedicated Screen Rendering (Bypasses network sync for UI testing)
  std::string targetScreen = "";

  std::string colorModeString() const;
  size_t bufferSize() const;
  bool setModel(const std::string& modelName);
  bool applyOrientation(const std::string& orient);
  bool applyRotation(int rot);
  void cycleRotation();
  static SimConfig load(int argc, char** argv);
  void print() const;
  bool save(const std::string& path = "") const;
};
