#include "sim_config.h"
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifndef PLATFORM_OS
#if defined(__APPLE__)
#define PLATFORM_OS "macOS"
#elif defined(__linux__)
#define PLATFORM_OS "Linux"
#else
#define PLATFORM_OS "Generic"
#endif
#endif

bool SimConfig::setModel(const std::string& modelName) {
  std::string m = modelName;
  std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return std::tolower(c); });

  if (m == "sticky" || m == "seeed-reterminal-sticky" || m == "reterminal-sticky") {
    model = "sticky";
    screenProfile = "seeed-reterminal-sticky";
    nativeWidth = 800;
    nativeHeight = 480;
    width = 800;
    height = 480;
    colorMode = SIM_COLOR_1BPP_MONO;
    supportsPartialRefresh = true;
    fullRefreshTimeMs = 1800;
    partialRefreshTimeMs = 350;
    if (!orientation.empty()) applyOrientation(orientation);
    else if (rotation != 0) applyRotation(rotation);
    return true;
  } else if (m == "e1001" || m == "seeed-reterminal-e1001" || m == "reterminal-e1001") {
    model = "e1001";
    screenProfile = "seeed-reterminal-e1001";
    nativeWidth = 800;
    nativeHeight = 480;
    width = 800;
    height = 480;
    colorMode = SIM_COLOR_1BPP_MONO;
    supportsPartialRefresh = true;
    fullRefreshTimeMs = 3000;
    partialRefreshTimeMs = 450;
    if (!orientation.empty()) applyOrientation(orientation);
    else if (rotation != 0) applyRotation(rotation);
    return true;
  } else if (m == "e1002" || m == "seeed-reterminal-e1002" || m == "reterminal-e1002") {
    model = "e1002";
    screenProfile = "seeed-reterminal-e1002";
    nativeWidth = 800;
    nativeHeight = 480;
    width = 800;
    height = 480;
    colorMode = SIM_COLOR_SPECTRA6;
    supportsPartialRefresh = false;
    fullRefreshTimeMs = 14000;
    partialRefreshTimeMs = 0;
    if (!orientation.empty()) applyOrientation(orientation);
    else if (rotation != 0) applyRotation(rotation);
    return true;
  } else if (m == "e1003" || m == "seeed-reterminal-e1003" || m == "reterminal-e1003") {
    model = "e1003";
    screenProfile = "seeed-reterminal-e1003";
    nativeWidth = 1872;
    nativeHeight = 1404;
    width = 1872;
    height = 1404;
    colorMode = SIM_COLOR_GRAY16;
    supportsPartialRefresh = true;
    fullRefreshTimeMs = 1200;
    partialRefreshTimeMs = 180;
    if (!orientation.empty()) applyOrientation(orientation);
    else if (rotation != 0) applyRotation(rotation);
    return true;
  } else if (m == "e1004" || m == "seeed-reterminal-e1004" || m == "reterminal-e1004") {
    model = "e1004";
    screenProfile = "seeed-reterminal-e1004";
    nativeWidth = 1200;
    nativeHeight = 1600;
    width = 1200;
    height = 1600;
    colorMode = SIM_COLOR_SPECTRA6;
    supportsPartialRefresh = false;
    fullRefreshTimeMs = 22000;
    partialRefreshTimeMs = 0;
    if (!orientation.empty()) applyOrientation(orientation);
    else if (rotation != 0) applyRotation(rotation);
    return true;
  }

  std::cerr << "[Config] Warning: Unknown hardware model '" << modelName << "'. Defaulting to sticky (800x480).\n";
  return false;
}

bool SimConfig::applyOrientation(const std::string& orient) {
  std::string o = orient;
  std::transform(o.begin(), o.end(), o.begin(), [](unsigned char c) { return std::tolower(c); });

  if (o == "portrait" || o == "portrait-flipped") {
    orientation = o;
    if (nativeWidth > nativeHeight) {
      width = nativeHeight;
      height = nativeWidth;
    } else {
      width = nativeWidth;
      height = nativeHeight;
    }
    if (o == "portrait-flipped") {
      rotation = 180;
    } else if (rotation == 180) {
      rotation = 0;
    }
    return true;
  } else if (o == "landscape" || o == "landscape-flipped") {
    orientation = o;
    if (nativeWidth < nativeHeight) {
      width = nativeHeight;
      height = nativeWidth;
    } else {
      width = nativeWidth;
      height = nativeHeight;
    }
    if (o == "landscape-flipped") {
      rotation = 180;
    } else if (rotation == 180) {
      rotation = 0;
    }
    return true;
  }
  return false;
}

bool SimConfig::applyRotation(int rot) {
  rotation = ((rot % 360) + 360) % 360;
  bool isRotated90or270 = (rotation == 90 || rotation == 270);
  bool isLandscapeNative = (nativeWidth >= nativeHeight);

  if (isRotated90or270) {
    width = std::min(nativeWidth, nativeHeight);
    height = std::max(nativeWidth, nativeHeight);
    if (!isLandscapeNative) {
      std::swap(width, height);
      orientation = (rotation == 90) ? "landscape" : "landscape-flipped";
    } else {
      orientation = (rotation == 90) ? "portrait" : "portrait-flipped";
    }
  } else {
    width = nativeWidth;
    height = nativeHeight;
    if (isLandscapeNative) {
      orientation = (rotation == 0) ? "landscape" : "landscape-flipped";
    } else {
      orientation = (rotation == 0) ? "portrait" : "portrait-flipped";
    }
  }
  return true;
}

void SimConfig::cycleRotation() {
  int nextRot = (rotation + 90) % 360;
  applyRotation(nextRot);
}

std::string SimConfig::colorModeString() const {
  switch (colorMode) {
    case SIM_COLOR_GRAY4: return "gray4";
    case SIM_COLOR_GRAY16: return "gray16";
    case SIM_COLOR_SPECTRA6: return "spectra6";
    case SIM_COLOR_1BPP_MONO:
    default: return "mono";
  }
}

size_t SimConfig::bufferSize() const {
  switch (colorMode) {
    case SIM_COLOR_GRAY4:
      return ((size_t)width * height) / 4;
    case SIM_COLOR_GRAY16:
    case SIM_COLOR_SPECTRA6:
      return ((size_t)width * height) / 2;
    case SIM_COLOR_1BPP_MONO:
    default:
      return ((size_t)width * height) / 8;
  }
}

static bool isKnownModelName(const std::string& name) {
  std::string m = name;
  std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return std::tolower(c); });
  return (m == "sticky" || m == "e1001" || m == "e1002" || m == "e1003" || m == "e1004" ||
          m == "seeed-reterminal-sticky" || m == "seeed-reterminal-e1001" ||
          m == "seeed-reterminal-e1002" || m == "seeed-reterminal-e1003" ||
          m == "seeed-reterminal-e1004" || m == "reterminal-sticky" ||
          m == "reterminal-e1001" || m == "reterminal-e1002" ||
          m == "reterminal-e1003" || m == "reterminal-e1004");
}

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

      if (doc["device_model"].is<const char*>()) cfg.setModel(doc["device_model"].as<std::string>());
      if (doc["model"].is<const char*>()) cfg.setModel(doc["model"].as<std::string>());
      if (doc["hardware"]["device_model"].is<const char*>()) cfg.setModel(doc["hardware"]["device_model"].as<std::string>());
      if (doc["hardware"]["model"].is<const char*>()) cfg.setModel(doc["hardware"]["model"].as<std::string>());
      if (doc["device"]["model"].is<const char*>()) cfg.setModel(doc["device"]["model"].as<std::string>());
      if (doc["simulator"]["device_model"].is<const char*>()) cfg.setModel(doc["simulator"]["device_model"].as<std::string>());
      if (doc["simulator"]["model"].is<const char*>()) cfg.setModel(doc["simulator"]["model"].as<std::string>());

      if (doc["simulator"]["headless"].is<bool>()) cfg.headless = doc["simulator"]["headless"].as<bool>();
      if (doc["simulator"]["snapshot_path"].is<const char*>()) cfg.snapshotPath = doc["simulator"]["snapshot_path"].as<std::string>();
      if (doc["simulator"]["fast_forward_sleep"].is<bool>()) cfg.fastForwardSleep = doc["simulator"]["fast_forward_sleep"].as<bool>();
      if (doc["simulator"]["sync_interval_sec"].is<int>()) cfg.syncIntervalSec = doc["simulator"]["sync_interval_sec"].as<int>();
      if (doc["simulator"]["scale"].is<int>()) cfg.scale = doc["simulator"]["scale"].as<int>();

      if (doc["orientation"].is<const char*>()) cfg.applyOrientation(doc["orientation"].as<std::string>());
      if (doc["hardware"]["orientation"].is<const char*>()) cfg.applyOrientation(doc["hardware"]["orientation"].as<std::string>());
      if (doc["simulator"]["orientation"].is<const char*>()) cfg.applyOrientation(doc["simulator"]["orientation"].as<std::string>());
      if (doc["rotation"].is<int>()) cfg.applyRotation(doc["rotation"].as<int>());
      if (doc["hardware"]["rotation"].is<int>()) cfg.applyRotation(doc["hardware"]["rotation"].as<int>());
      if (doc["simulator"]["rotation"].is<int>()) cfg.applyRotation(doc["simulator"]["rotation"].as<int>());
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
  if (const char* v = std::getenv("ST_MODEL")) cfg.setModel(v);
  if (const char* v = std::getenv("ST_DEVICE_MODEL")) cfg.setModel(v);
  if (const char* v = std::getenv("ST_ORIENTATION")) cfg.applyOrientation(v);
  if (const char* v = std::getenv("ST_ROTATION")) cfg.applyRotation(std::atoi(v));

  // 4. CLI Argument overrides
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--server" || arg == "-s") && i + 1 < argc) {
      cfg.serverUrl = argv[++i];
    } else if ((arg == "--model" || arg == "--profile" || arg == "-m") && i + 1 < argc) {
      cfg.setModel(argv[++i]);
    } else if (arg == "--device-id" && i + 1 < argc) {
      cfg.deviceId = argv[++i];
    } else if ((arg == "--device" || arg == "-d") && i + 1 < argc) {
      std::string val = argv[++i];
      if (isKnownModelName(val)) {
        cfg.setModel(val);
      } else {
        cfg.deviceId = val;
      }
    } else if ((arg == "--token" || arg == "-t") && i + 1 < argc) {
      cfg.deviceToken = argv[++i];
    } else if ((arg == "--orientation" || arg == "-o") && i + 1 < argc) {
      cfg.applyOrientation(argv[++i]);
    } else if ((arg == "--rotate" || arg == "--rotation" || arg == "-r") && i + 1 < argc) {
      cfg.applyRotation(std::atoi(argv[++i]));
    } else if (arg == "--portrait") {
      cfg.applyOrientation("portrait");
    } else if (arg == "--landscape") {
      cfg.applyOrientation("landscape");
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
    } else if (arg == "--scale" && i + 1 < argc) {
      cfg.scale = std::max(1, std::min(4, std::atoi(argv[++i])));
    } else if (arg == "--pair") {
      cfg.pairMode = true;
    } else if (arg == "--snapshot" && i + 1 < argc) {
      cfg.snapshotPath = argv[++i];
    } else if (arg == "--interval" && i + 1 < argc) {
      cfg.syncIntervalSec = std::atoi(argv[++i]);
    } else if (arg == "--headless") {
      cfg.headless = true;
    } else if (arg == "--oneshot") {
      cfg.oneShot = true;
      cfg.maxCycles = 1;
    } else if (arg == "--cycles" && i + 1 < argc) {
      cfg.maxCycles = std::atoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "ScreenTinker MCU Host Simulator (" << PLATFORM_OS << ")\n"
                << "Usage: ./screentinker_sim [options]\n\n"
                << "Options:\n"
                << "  -s, --server <url>       Server URL (default: " << cfg.serverUrl << ")\n"
                << "  -m, --model <name>       Hardware model: sticky, e1001, e1002, e1003, e1004 (default: " << cfg.model << ")\n"
                << "  -d, --device <uuid|name> Device ID or hardware model name\n"
                << "      --device-id <uuid>   Explicit Device ID\n"
                << "  -t, --token <token>      Device Token\n"
                << "  -o, --orientation <name> Orientation: landscape, portrait, landscape-flipped, portrait-flipped\n"
                << "  -r, --rotate <deg>       Rotation: 0, 90, 180, 270 degrees\n"
                << "      --portrait           Shortcut for --orientation portrait\n"
                << "      --landscape          Shortcut for --orientation landscape\n"
                << "  --pair                   Start in pairing mode (request 6-digit PIN from server)\n"
                << "  --battery <percent>      Simulated battery (0-100)\n"
                << "  --power <usb|battery>    Power source mode (default: battery)\n"
                << "  --usb                    Shortcut for --power usb\n"
                << "  --rssi <dbm>             Simulated Wi-Fi RSSI\n"
                << "  --scale <1-4>            Window scaling factor (default: 1)\n"
                << "  --headless               Run without SDL2 window (console + snapshot only)\n"
                << "  --oneshot                Perform single poll cycle and exit\n"
                << "  --cycles <count>         Execute N poll cycles and exit\n"
                << "  --snapshot <path>        Output image path (default: " << cfg.snapshotPath << ")\n"
                << "  -c, --config <path>      Path to JSON configuration\n"
                << "  -h, --help               Show this help message\n";
      std::exit(0);
    }
  }

  if (cfg.powerSource == "usb") {
    cfg.isCharging = (cfg.batteryPercent < 100);
  }

  return cfg;
}

void SimConfig::print() const {
  std::cout << "───────────────────────────────────────────────────────\n"
            << " ScreenTinker MCU Simulator (" << PLATFORM_OS << " Host)\n"
            << "───────────────────────────────────────────────────────\n"
            << " • Target Hardware:   " << model << " (" << width << "x" << height << ", " << screenProfile << ")\n"
            << " • Orientation:       " << (orientation.empty() ? (width > height ? "landscape" : "portrait") : orientation)
            << " (" << rotation << "°)\n"
            << " • Color Mode:        " << colorModeString() << " ("
            << (colorMode == SIM_COLOR_SPECTRA6 ? "4bpp, 6-Color Spectra 6" :
                (colorMode == SIM_COLOR_GRAY16 ? "4bpp, 16-Level Grayscale" :
                 (colorMode == SIM_COLOR_GRAY4 ? "2bpp, 4-Level Grayscale" : "1bpp Monochrome"))) << ")\n"
            << " • Refresh Support:   " << (supportsPartialRefresh ? "Full & Fast Partial (" + std::to_string(partialRefreshTimeMs) + "ms)" : "Full Refresh Only (" + std::to_string(fullRefreshTimeMs / 1000) + "s)") << "\n"
            << " • Server URL:        " << serverUrl << "\n"
            << " • Device ID:         " << deviceId << "\n"
            << " • Firmware Version:  " << firmwareVersion << "\n"
            << " • Power Source:      " << (powerSource == "usb" ? (isCharging ? "USB (Charging)" : "USB (Mains Connected)") : "Battery") << "\n"
            << " • Battery Level:     " << batteryPercent << "%\n"
            << " • Wi-Fi RSSI:        " << wifiRssi << " dBm\n"
            << " • Mode:              " << (headless ? "Headless (Snapshot only)" : ("Interactive Window (SDL2, " + std::to_string(scale) + "x)")) << "\n"
            << " • Snapshot Output:   " << snapshotPath << "\n"
            << " • Pairing Mode:      " << (pairMode ? "Enabled (--pair)" : (deviceToken.empty() ? "Auto (No Token)" : "Disabled (Token Configured)")) << "\n"
            << "───────────────────────────────────────────────────────\n";
}

bool SimConfig::save(const std::string& path) const {
  std::string target = path.empty() ? configFilePath : path;
  JsonDocument doc;
  doc["server"]["url"] = serverUrl;
  doc["server"]["device_id"] = deviceId;
  doc["server"]["device_token"] = deviceToken;
  doc["server"]["timeout_ms"] = timeoutMs;

  doc["hardware"]["device_model"] = model;
  doc["hardware"]["battery_percent"] = batteryPercent;
  doc["hardware"]["wifi_rssi_dbm"] = wifiRssi;
  doc["hardware"]["firmware_version"] = firmwareVersion;
  doc["hardware"]["orientation"] = orientation;
  doc["hardware"]["rotation"] = rotation;

  doc["device_model"] = model;
  doc["simulator"]["headless"] = headless;
  doc["simulator"]["snapshot_path"] = snapshotPath;
  doc["simulator"]["fast_forward_sleep"] = fastForwardSleep;
  doc["simulator"]["sync_interval_sec"] = syncIntervalSec;
  doc["simulator"]["scale"] = scale;
  doc["simulator"]["orientation"] = orientation;
  doc["simulator"]["rotation"] = rotation;

  std::ofstream f(target);
  if (!f.is_open()) return false;
  serializeJsonPretty(doc, f);
  f << "\n";
  std::cout << "[Config] Saved active configuration to " << target << "\n";
  return true;
}
