#pragma once
#include <string>
#include <cstdint>

struct SimConfig {
  std::string serverUrl = "http://192.168.100.222:3001";
  std::string deviceId = "06813352-374a-476b-bca9-986ebdb1ff2c";
  std::string deviceToken = "";
  std::string firmwareVersion = "1.2.0";
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

  // Device Model & Profile
  std::string model = "sticky";
  std::string screenProfile = "seeed-reterminal-sticky";
  int width = 800;
  int height = 480;
  size_t bufferSize() const { return ((size_t)width * height) / 8; }

  bool setModel(const std::string& modelName);
  static SimConfig load(int argc, char** argv);
  void print() const;
  bool save(const std::string& path = "") const;
};
