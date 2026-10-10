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
  bool fastForwardSleep = true;
  std::string snapshotPath = "sim_output.bmp";
  std::string configFilePath = "simulator/sim_config.json";

  static SimConfig load(int argc, char** argv);
  void print() const;
};
