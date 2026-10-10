#include "sim_config.h"
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>

SimConfig SimConfig::load(int argc, char** argv) {
  SimConfig cfg;

  // 1. Look for custom --config path first in argv
  for (int i = 1; i < argc; ++i) {
    if ((strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) && i + 1 < argc) {
      cfg.configFilePath = argv[i + 1];
      break;
    }
  }

  // 2. Load JSON config file if present
  std::ifstream f(cfg.configFilePath);
  if (!f.is_open()) {
    // Try current directory sim_config.json as fallback
    f.open("sim_config.json");
  }

  if (f.is_open()) {
    std::string jsonStr((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (!err) {
      if (doc["server"]["url"].is<const char*>()) cfg.serverUrl = doc["server"]["url"].as<std::string>();
      if (doc["server"]["device_id"].is<const char*>()) cfg.deviceId = doc["server"]["device_id"].as<std::string>();
      if (doc["server"]["device_token"].is<const char*>()) cfg.deviceToken = doc["server"]["device_token"].as<std::string>();
      if (doc["server"]["timeout_ms"].is<int>()) cfg.timeoutMs = doc["server"]["timeout_ms"].as<int>();

      if (doc["hardware"]["battery_percent"].is<int>()) cfg.batteryPercent = doc["hardware"]["battery_percent"].as<int>();
      if (doc["hardware"]["wifi_rssi_dbm"].is<int>()) cfg.wifiRssi = doc["hardware"]["wifi_rssi_dbm"].as<int>();
      if (doc["hardware"]["firmware_version"].is<const char*>()) cfg.firmwareVersion = doc["hardware"]["firmware_version"].as<std::string>();

      if (doc["simulator"]["headless"].is<bool>()) cfg.headless = doc["simulator"]["headless"].as<bool>();
      if (doc["simulator"]["snapshot_path"].is<const char*>()) cfg.snapshotPath = doc["simulator"]["snapshot_path"].as<std::string>();
      if (doc["simulator"]["fast_forward_sleep"].is<bool>()) cfg.fastForwardSleep = doc["simulator"]["fast_forward_sleep"].as<bool>();
      if (doc["simulator"]["sync_interval_sec"].is<int>()) cfg.syncIntervalSec = doc["simulator"]["sync_interval_sec"].as<int>();
    } else {
      std::cerr << "[Config] Warning: Failed to parse " << cfg.configFilePath << " (" << err.c_str() << ")\n";
    }
  }

  // 3. Environment variable overrides
  if (const char* v = std::getenv("ST_SERVER_URL")) cfg.serverUrl = v;
  if (const char* v = std::getenv("ST_DEVICE_ID")) cfg.deviceId = v;
  if (const char* v = std::getenv("ST_DEVICE_TOKEN")) cfg.deviceToken = v;
  if (const char* v = std::getenv("ST_BATTERY")) cfg.batteryPercent = std::atoi(v);
  if (const char* v = std::getenv("ST_RSSI")) cfg.wifiRssi = std::atoi(v);

  // 4. CLI Argument overrides
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--server" || arg == "-s") && i + 1 < argc) {
      cfg.serverUrl = argv[++i];
    } else if ((arg == "--device" || arg == "-d") && i + 1 < argc) {
      cfg.deviceId = argv[++i];
    } else if ((arg == "--token" || arg == "-t") && i + 1 < argc) {
      cfg.deviceToken = argv[++i];
    } else if (arg == "--battery" && i + 1 < argc) {
      cfg.batteryPercent = std::atoi(argv[++i]);
    } else if (arg == "--power" && i + 1 < argc) {
      cfg.powerSource = argv[++i];
      if (cfg.powerSource == "usb") cfg.isCharging = (cfg.batteryPercent < 100);
    } else if (arg == "--usb") {
      cfg.powerSource = "usb";
      cfg.isCharging = (cfg.batteryPercent < 100);
    } else if (arg == "--rssi" && i + 1 < argc) {
      cfg.wifiRssi = std::atoi(argv[++i]);
    } else if (arg == "--snapshot" && i + 1 < argc) {
      cfg.snapshotPath = argv[++i];
    } else if (arg == "--interval" && i + 1 < argc) {
      cfg.syncIntervalSec = std::atoi(argv[++i]);
    } else if (arg == "--headless") {
      cfg.headless = true;
    } else if (arg == "--oneshot") {
      cfg.oneShot = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "ScreenTinker MCU Host Simulator\n"
                << "Usage: ./screentinker_sim [options]\n\n"
                << "Options:\n"
                << "  -s, --server <url>       Server URL (default: " << cfg.serverUrl << ")\n"
                << "  -d, --device <uuid>      Device ID\n"
                << "  -t, --token <token>      Device Token\n"
                << "  --battery <percent>      Simulated battery (0-100)\n"
                << "  --power <usb|battery>    Power source mode (default: battery)\n"
                << "  --usb                    Shortcut for --power usb\n"
                << "  --rssi <dbm>             Simulated Wi-Fi RSSI\n"
                << "  --headless               Run without SDL2 window (console + snapshot only)\n"
                << "  --oneshot                Perform single poll cycle and exit\n"
                << "  --snapshot <path>        Output image path (default: " << cfg.snapshotPath << ")\n"
                << "  -c, --config <path>      Path to JSON configuration\n"
                << "  -h, --help               Show this help message\n";
      std::exit(0);
    }
  }

  return cfg;
}

void SimConfig::print() const {
  std::cout << "───────────────────────────────────────────────────────\n"
            << " ScreenTinker MCU Simulator (macOS Host)\n"
            << "───────────────────────────────────────────────────────\n"
            << " • Server URL:        " << serverUrl << "\n"
            << " • Device ID:         " << deviceId << "\n"
            << " • Firmware Version:  " << firmwareVersion << "\n"
            << " • Power Source:      " << (powerSource == "usb" ? (isCharging ? "USB (Charging)" : "USB (Mains Connected)") : "Battery") << "\n"
            << " • Battery Level:     " << batteryPercent << "%\n"
            << " • Wi-Fi RSSI:        " << wifiRssi << " dBm\n"
            << " • Mode:              " << (headless ? "Headless (Snapshot only)" : "Interactive Window (SDL2)") << "\n"
            << " • Snapshot Output:   " << snapshotPath << "\n"
            << "───────────────────────────────────────────────────────\n";
}
