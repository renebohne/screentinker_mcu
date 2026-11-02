#include "sim_serial.h"
#include "sim_display.h"
#include "diag_logger.h"
#include <ArduinoJson.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

static std::string s_stdinBuffer;
static bool s_stdinEof = false;

void SimSerial::init() {
  s_stdinBuffer.clear();
  s_stdinEof = false;
}

bool SimSerial::isEof() {
  return s_stdinEof;
}

static std::string getArg(const std::string& str, int index) {
  std::istringstream iss(str);
  std::string token;
  int current = 0;
  while (iss >> token) {
    if (current == index) return token;
    current++;
  }
  return "";
}

bool SimSerial::poll(SimConfig& cfg, SimDisplay& display,
                     std::vector<uint8_t>& frameBuffer,
                     std::vector<uint8_t>& monoBuffer,
                     bool& wakeRequested,
                     bool& forceFullRefresh,
                     bool& running) {
  if (s_stdinEof) return false;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  struct timeval tv = {0, 0};

  int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
  if (ret <= 0) return false;

  char buf[512];
  ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
  if (n <= 0) {
    s_stdinEof = true;
    if (!s_stdinBuffer.empty()) {
      std::string line = s_stdinBuffer;
      s_stdinBuffer.clear();
      processLine(line, cfg, display, frameBuffer, monoBuffer, wakeRequested, forceFullRefresh, running);
      return true;
    }
    return false;
  }

  buf[n] = '\0';
  s_stdinBuffer.append(buf, n);

  bool anyProcessed = false;
  size_t nlPos;
  while ((nlPos = s_stdinBuffer.find('\n')) != std::string::npos) {
    std::string line = s_stdinBuffer.substr(0, nlPos);
    s_stdinBuffer.erase(0, nlPos + 1);

    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    size_t start = line.find_first_not_of(" \t");
    size_t end = line.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
      line = line.substr(start, end - start + 1);
    } else {
      line.clear();
    }

    if (!line.empty()) {
      processLine(line, cfg, display, frameBuffer, monoBuffer, wakeRequested, forceFullRefresh, running);
      anyProcessed = true;
    }
  }

  return anyProcessed;
}

void SimSerial::printHelp(const SimConfig& cfg) {
  std::cout << "\n=================================================================\n"
            << "  ScreenTinker Simulator Serial Command Line Interface (CLI)\n"
            << "  Active Hardware: " << cfg.model << " (" << cfg.screenProfile << ", "
            << cfg.width << "x" << cfg.height << ")\n"
            << "=================================================================\n"
            << "Commands:\n"
            << "  help, ?                    Show this command reference\n"
            << "  status                     Show device status summary (JSON telemetry)\n"
            << "  get [key]                  Show config value (use 'get all' for all)\n"
            << "  set <key> <value>          Update configuration key\n"
            << "  export [pretty]            Export entire configuration as JSON (excluding password)\n"
            << "  refresh                    Trigger immediate server poll and render\n"
            << "  clean, full_refresh        Force full clean cycle display refresh\n"
            << "  scan                       Scan and display visible Wi-Fi networks\n"
            << "  pair                       Initiate 6-digit PIN device pairing flow\n"
            << "  reboot, restart            Reboot simulator\n"
            << "  reset                      Reset configuration tokens (factory unpair)\n"
            << "  logs                       Dump simulator diagnostic logs\n"
            << "  poweroff, quit, exit       Terminate simulator\n";
  if (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888) {
    std::cout << "  led_test                   Run RGBW test sequence on simulated LEDs\n"
              << "  led_off                    Turn off all LEDs\n"
              << "  led_red/green/blue/white   Set solid test color\n";
  }
  std::cout << "\nConfiguration Keys for 'get' / 'set':\n"
            << "  ssid <name>                Wi-Fi network SSID\n"
            << "  pass <password>            Wi-Fi network password\n"
            << "  wifi <ssid> <pass>         Set Wi-Fi network and password at once\n"
            << "  server <url>               ScreenTinker server URL\n"
            << "  device_id <uuid>           Assigned device UUID\n"
            << "  token <token>              Device authentication bearer token\n"
            << "  model <sticky|e1001|e1002|e1003|e1004|crowpanel_43|xiao_led>\n"
            << "  orient <landscape|portrait|auto>\n"
            << "  rotate <0|90|180|270>\n"
            << "  battery <0..100>           Simulated battery percent\n"
            << "  power <usb|battery>        Simulated power source\n"
            << "  rssi <dbm>                 Simulated Wi-Fi RSSI\n";
  if (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888) {
    std::cout << "  matrix <w> <h> [wir] [ax] [orig]  Configure LED matrix geometry\n"
              << "  width <px>                 Matrix width\n"
              << "  height <px>                Matrix height\n"
              << "  wiring <serpentine|progressive>\n"
              << "  axis <horizontal|vertical>\n"
              << "  origin <top_left|bottom_left|top_right|bottom_right>\n"
              << "  brightness <0..255>        Overall LED brightness\n";
  }
  std::cout << "=================================================================\n\n";
}

void SimSerial::printStatus(const SimConfig& cfg) {
  JsonDocument doc;
  doc["status"] = "ok";
  doc["v"] = 1;
  doc["version"] = cfg.firmwareVersion;
  doc["ip"] = "127.0.0.1";
  doc["ssid"] = cfg.wifiSsid;
  doc["connected"] = true;
  doc["device_id"] = cfg.deviceId;
  doc["multi_zone"] = cfg.multiZone;
  doc["wifi_always_on"] = cfg.wifiAlwaysOn;
  doc["psram_free"] = 8388608;
  doc["rssi"] = cfg.wifiRssi;
  std::string out;
  serializeJson(doc, out);
  std::cout << out << "\n";
}

void SimSerial::printExport(const SimConfig& cfg, bool pretty) {
  JsonDocument doc;
  doc["version"] = cfg.firmwareVersion;
  doc["profile"] = cfg.screenProfile;
  doc["model"] = cfg.model;
  doc["ssid"] = cfg.wifiSsid;
  // NOTE: Wi-Fi password is intentionally excluded from export
  doc["server"] = cfg.serverUrl;
  doc["device_id"] = cfg.deviceId;
  doc["token"] = cfg.deviceToken;
  doc["wifi_always_on"] = cfg.wifiAlwaysOn;
  doc["tx_power"] = cfg.wifiTxPowerLevel;
  doc["multizone"] = cfg.multiZone;
  doc["lang"] = cfg.language;
  doc["orient"] = cfg.orientation.empty() ? (cfg.width > cfg.height ? "landscape" : "portrait") : cfg.orientation;
  doc["batt_cutoff"] = cfg.criticalBatteryPct;
  doc["battery"] = cfg.batteryPercent;
  doc["power"] = cfg.powerSource;

  if (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888) {
    JsonObject mat = doc["matrix"].to<JsonObject>();
    mat["width"] = cfg.width;
    mat["height"] = cfg.height;
    mat["wiring"] = (cfg.matrixWiring == MATRIX_WIRING_SERPENTINE) ? "serpentine" : "progressive";
    mat["axis"] = (cfg.matrixAxis == MATRIX_AXIS_VERTICAL) ? "vertical" : "horizontal";
    std::string originStr = (cfg.matrixOrigin == MATRIX_ORIGIN_TOP_RIGHT) ? "top_right" :
                            (cfg.matrixOrigin == MATRIX_ORIGIN_BOTTOM_LEFT) ? "bottom_left" :
                            (cfg.matrixOrigin == MATRIX_ORIGIN_BOTTOM_RIGHT) ? "bottom_right" : "top_left";
    mat["origin"] = originStr;
    doc["brightness"] = cfg.brightness;
  }

  std::string out;
  if (pretty) {
    serializeJsonPretty(doc, out);
  } else {
    serializeJson(doc, out);
  }
  std::cout << out << "\n";
}

void SimSerial::handleGet(const std::string& rawKey, const SimConfig& cfg) {
  std::string key = rawKey;
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });

  if (key.empty() || key == "all" || key == "config") {
    std::cout << "\n--- Current Simulator Configuration ---\n"
              << "  model           : " << cfg.model << " (" << cfg.screenProfile << ")\n"
              << "  resolution      : " << cfg.width << "x" << cfg.height << "\n"
              << "  color_mode      : " << cfg.colorModeString() << "\n"
              << "  ssid            : " << (cfg.wifiSsid.empty() ? "(none)" : cfg.wifiSsid) << "\n"
              << "  pass            : " << (cfg.wifiPass.empty() ? "(empty)" : "********") << "\n"
              << "  server          : " << cfg.serverUrl << "\n"
              << "  device_id       : " << (cfg.deviceId.empty() ? "(unpaired)" : cfg.deviceId) << "\n"
              << "  token           : " << (cfg.deviceToken.empty() ? "(none)" : (cfg.deviceToken.substr(0, 10) + "...")) << "\n"
              << "  wifi_always_on  : " << (cfg.wifiAlwaysOn ? "true" : "false") << "\n"
              << "  orient          : " << (cfg.orientation.empty() ? (cfg.width > cfg.height ? "landscape" : "portrait") : cfg.orientation) << "\n"
              << "  rotation        : " << cfg.rotation << "°\n"
              << "  battery         : " << cfg.batteryPercent << "%\n"
              << "  power           : " << cfg.powerSource << (cfg.isCharging ? " (Charging)" : "") << "\n"
              << "  rssi            : " << cfg.wifiRssi << " dBm\n";
    if (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888) {
      std::string originStr = (cfg.matrixOrigin == MATRIX_ORIGIN_TOP_RIGHT) ? "top_right" :
                              (cfg.matrixOrigin == MATRIX_ORIGIN_BOTTOM_LEFT) ? "bottom_left" :
                              (cfg.matrixOrigin == MATRIX_ORIGIN_BOTTOM_RIGHT) ? "bottom_right" : "top_left";
      std::cout << "  matrix          : " << cfg.width << "x" << cfg.height << " (total: " << (cfg.width * cfg.height) << " LEDs)\n"
                << "  wiring          : " << (cfg.matrixWiring == MATRIX_WIRING_SERPENTINE ? "serpentine" : "progressive") << "\n"
                << "  axis            : " << (cfg.matrixAxis == MATRIX_AXIS_VERTICAL ? "vertical" : "horizontal") << "\n"
                << "  origin          : " << originStr << "\n"
                << "  brightness      : " << (int)cfg.brightness << "/255\n";
    }
    std::cout << "---------------------------------------\n\n";
    return;
  }

  if (key == "ssid" || key == "wifi_ssid") {
    std::cout << "ssid = " << cfg.wifiSsid << "\n";
  } else if (key == "pass" || key == "wifi_pass" || key == "password") {
    std::cout << "pass = " << (cfg.wifiPass.empty() ? "(empty)" : "********") << "\n";
  } else if (key == "server" || key == "host") {
    std::cout << "server = " << cfg.serverUrl << "\n";
  } else if (key == "device_id" || key == "id") {
    std::cout << "device_id = " << cfg.deviceId << "\n";
  } else if (key == "token") {
    std::cout << "token = " << cfg.deviceToken << "\n";
  } else if (key == "model") {
    std::cout << "model = " << cfg.model << "\n";
  } else if (key == "wifi_always_on" || key == "always_on") {
    std::cout << "wifi_always_on = " << (cfg.wifiAlwaysOn ? "true" : "false") << "\n";
  } else if (key == "battery" || key == "batt") {
    std::cout << "battery = " << cfg.batteryPercent << "%\n";
  } else if (key == "power") {
    std::cout << "power = " << cfg.powerSource << (cfg.isCharging ? " (charging)" : "") << "\n";
  } else if (key == "rssi") {
    std::cout << "rssi = " << cfg.wifiRssi << " dBm\n";
  } else if (key == "orient" || key == "orientation") {
    std::cout << "orientation = " << (cfg.orientation.empty() ? (cfg.width > cfg.height ? "landscape" : "portrait") : cfg.orientation) << "\n";
  } else if (key == "rotation" || key == "rotate") {
    std::cout << "rotation = " << cfg.rotation << "°\n";
  } else if (key == "matrix") {
    std::string originStr = (cfg.matrixOrigin == MATRIX_ORIGIN_TOP_RIGHT) ? "top_right" :
                            (cfg.matrixOrigin == MATRIX_ORIGIN_BOTTOM_LEFT) ? "bottom_left" :
                            (cfg.matrixOrigin == MATRIX_ORIGIN_BOTTOM_RIGHT) ? "bottom_right" : "top_left";
    std::cout << "matrix = " << cfg.width << "x" << cfg.height
              << " (wiring: " << (cfg.matrixWiring == MATRIX_WIRING_SERPENTINE ? "serpentine" : "progressive")
              << ", axis: " << (cfg.matrixAxis == MATRIX_AXIS_VERTICAL ? "vertical" : "horizontal")
              << ", origin: " << originStr << ")\n";
  } else if (key == "width") {
    std::cout << "width = " << cfg.width << "\n";
  } else if (key == "height") {
    std::cout << "height = " << cfg.height << "\n";
  } else if (key == "brightness") {
    std::cout << "brightness = " << (int)cfg.brightness << "/255\n";
  } else if (key == "temp" || key == "temperature") {
    std::cout << "temperature = " << cfg.mockTemp << " °C\n";
  } else if (key == "humidity") {
    std::cout << "humidity = " << cfg.mockHumidity << " %\n";
  } else {
    std::cout << "[CLI] Unknown key '" << key << "'. Type 'get all' or 'help' to see available keys.\n";
  }
}

void SimSerial::handleSet(const std::string& args, SimConfig& cfg, SimDisplay& display,
                          std::vector<uint8_t>& frameBuffer,
                          std::vector<uint8_t>& monoBuffer,
                          bool& wakeRequested,
                          bool& forceFullRefresh) {
  size_t firstSpace = args.find(' ');
  if (firstSpace == std::string::npos) {
    std::cout << "[CLI] Usage: set <key> <value>\nType 'help' to see configurable keys.\n";
    return;
  }

  std::string key = args.substr(0, firstSpace);
  std::string val = args.substr(firstSpace + 1);
  size_t s1 = key.find_first_not_of(" \t");
  size_t e1 = key.find_last_not_of(" \t");
  if (s1 != std::string::npos) key = key.substr(s1, e1 - s1 + 1);
  size_t s2 = val.find_first_not_of(" \t");
  size_t e2 = val.find_last_not_of(" \t");
  if (s2 != std::string::npos) val = val.substr(s2, e2 - s2 + 1);

  std::string lowerKey = key;
  std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), [](unsigned char c) { return std::tolower(c); });
  std::string lowerVal = val;
  std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), [](unsigned char c) { return std::tolower(c); });

  if (lowerKey == "ssid" || lowerKey == "wifi_ssid") {
    cfg.wifiSsid = val;
    std::cout << "[OK] Wi-Fi SSID set to '" << cfg.wifiSsid << "'\n";
  } else if (lowerKey == "pass" || lowerKey == "wifi_pass" || lowerKey == "password") {
    cfg.wifiPass = val;
    std::cout << "[OK] Wi-Fi password updated.\n";
  } else if (lowerKey == "wifi") {
    std::string s = getArg(val, 0);
    std::string p = getArg(val, 1);
    if (!s.empty()) cfg.wifiSsid = s;
    if (!p.empty()) cfg.wifiPass = p;
    std::cout << "[OK] Wi-Fi configured for SSID '" << cfg.wifiSsid << "'\n";
  } else if (lowerKey == "server" || lowerKey == "host") {
    cfg.serverUrl = val;
    std::cout << "[OK] Server URL set to " << cfg.serverUrl << "\n";
  } else if (lowerKey == "device_id" || lowerKey == "id") {
    cfg.deviceId = val;
    std::cout << "[OK] Device ID set to " << cfg.deviceId << "\n";
  } else if (lowerKey == "token") {
    cfg.deviceToken = val;
    std::cout << "[OK] Device Token updated.\n";
  } else if (lowerKey == "model") {
    if (cfg.setModel(val)) {
      display.updateDimensions(cfg.width, cfg.height);
      display.setColorMode(cfg.colorMode);
      frameBuffer.resize(cfg.bufferSize(), 0xFF);
      std::cout << "[OK] Model switched to " << cfg.model << " (" << cfg.width << "x" << cfg.height << ")\n";
      forceFullRefresh = true;
      wakeRequested = true;
    } else {
      std::cout << "[CLI] Unknown model '" << val << "'\n";
    }
  } else if (lowerKey == "orient" || lowerKey == "orientation") {
    cfg.applyOrientation(val);
    display.updateDimensions(cfg.width, cfg.height);
    frameBuffer.resize(cfg.bufferSize(), 0xFF);
    std::cout << "[OK] Orientation set to " << cfg.orientation << " (" << cfg.width << "x" << cfg.height << ")\n";
    forceFullRefresh = true;
    wakeRequested = true;
  } else if (lowerKey == "rotate" || lowerKey == "rotation") {
    cfg.applyRotation(std::atoi(val.c_str()));
    display.updateDimensions(cfg.width, cfg.height);
    frameBuffer.resize(cfg.bufferSize(), 0xFF);
    std::cout << "[OK] Rotation set to " << cfg.rotation << "° (" << cfg.width << "x" << cfg.height << ")\n";
    forceFullRefresh = true;
    wakeRequested = true;
  } else if (lowerKey == "battery" || lowerKey == "batt") {
    cfg.batteryPercent = std::max(0, std::min(100, std::atoi(val.c_str())));
    std::cout << "[OK] Battery set to " << cfg.batteryPercent << "%\n";
  } else if (lowerKey == "power") {
    cfg.powerSource = (lowerVal == "usb") ? "usb" : "battery";
    cfg.isCharging = (cfg.powerSource == "usb" && cfg.batteryPercent < 100);
    std::cout << "[OK] Power source set to " << cfg.powerSource << "\n";
  } else if (lowerKey == "rssi") {
    cfg.wifiRssi = std::atoi(val.c_str());
    std::cout << "[OK] Wi-Fi RSSI set to " << cfg.wifiRssi << " dBm\n";
  } else if (lowerKey == "brightness") {
    cfg.brightness = (uint8_t)std::max(0, std::min(255, std::atoi(val.c_str())));
    std::cout << "[OK] Brightness set to " << (int)cfg.brightness << "/255\n";
  } else if (lowerKey == "width" || lowerKey == "w") {
    int w = std::atoi(val.c_str());
    if (w > 0) {
      cfg.width = cfg.nativeWidth = w;
      cfg.customDimensions = true;
      display.updateDimensions(cfg.width, cfg.height);
      frameBuffer.resize(cfg.bufferSize(), 0xFF);
      std::cout << "[OK] Width set to " << cfg.width << "\n";
    }
  } else if (lowerKey == "height" || lowerKey == "h") {
    int h = std::atoi(val.c_str());
    if (h > 0) {
      cfg.height = cfg.nativeHeight = h;
      cfg.customDimensions = true;
      display.updateDimensions(cfg.width, cfg.height);
      frameBuffer.resize(cfg.bufferSize(), 0xFF);
      std::cout << "[OK] Height set to " << cfg.height << "\n";
    }
  } else if (lowerKey == "wiring") {
    cfg.matrixWiring = (lowerVal == "serpentine" || lowerVal == "zigzag") ? MATRIX_WIRING_SERPENTINE : MATRIX_WIRING_PROGRESSIVE;
    std::cout << "[OK] Wiring set to " << (cfg.matrixWiring == MATRIX_WIRING_SERPENTINE ? "serpentine" : "progressive") << "\n";
  } else if (lowerKey == "axis") {
    cfg.matrixAxis = (lowerVal == "vertical" || lowerVal == "col") ? MATRIX_AXIS_VERTICAL : MATRIX_AXIS_HORIZONTAL;
    std::cout << "[OK] Scanning axis set to " << (cfg.matrixAxis == MATRIX_AXIS_VERTICAL ? "vertical" : "horizontal") << "\n";
  } else if (lowerKey == "origin") {
    if (lowerVal == "top_right" || lowerVal == "top-right") cfg.matrixOrigin = MATRIX_ORIGIN_TOP_RIGHT;
    else if (lowerVal == "bottom_left" || lowerVal == "bottom-left") cfg.matrixOrigin = MATRIX_ORIGIN_BOTTOM_LEFT;
    else if (lowerVal == "bottom_right" || lowerVal == "bottom-right") cfg.matrixOrigin = MATRIX_ORIGIN_BOTTOM_RIGHT;
    else cfg.matrixOrigin = MATRIX_ORIGIN_TOP_LEFT;
    std::cout << "[OK] DIN origin set to " << lowerVal << "\n";
  } else if (lowerKey == "matrix") {
    int w = std::atoi(getArg(val, 0).c_str());
    int h = std::atoi(getArg(val, 1).c_str());
    std::string wirStr = getArg(val, 2);
    std::string axStr = getArg(val, 3);
    std::string origStr = getArg(val, 4);
    if (w > 0 && h > 0) {
      cfg.width = cfg.nativeWidth = w;
      cfg.height = cfg.nativeHeight = h;
      cfg.customDimensions = true;
      if (!wirStr.empty()) {
        std::transform(wirStr.begin(), wirStr.end(), wirStr.begin(), [](unsigned char c) { return std::tolower(c); });
        cfg.matrixWiring = (wirStr == "progressive" || wirStr == "linear") ? MATRIX_WIRING_PROGRESSIVE : MATRIX_WIRING_SERPENTINE;
      }
      if (!axStr.empty()) {
        std::transform(axStr.begin(), axStr.end(), axStr.begin(), [](unsigned char c) { return std::tolower(c); });
        cfg.matrixAxis = (axStr == "vertical" || axStr == "col") ? MATRIX_AXIS_VERTICAL : MATRIX_AXIS_HORIZONTAL;
      }
      if (!origStr.empty()) {
        std::transform(origStr.begin(), origStr.end(), origStr.begin(), [](unsigned char c) { return std::tolower(c); });
        if (origStr == "top_right" || origStr == "top-right") cfg.matrixOrigin = MATRIX_ORIGIN_TOP_RIGHT;
        else if (origStr == "bottom_left" || origStr == "bottom-left") cfg.matrixOrigin = MATRIX_ORIGIN_BOTTOM_LEFT;
        else if (origStr == "bottom_right" || origStr == "bottom-right") cfg.matrixOrigin = MATRIX_ORIGIN_BOTTOM_RIGHT;
        else cfg.matrixOrigin = MATRIX_ORIGIN_TOP_LEFT;
      }
      display.updateDimensions(cfg.width, cfg.height);
      frameBuffer.resize(cfg.bufferSize(), 0xFF);
      std::cout << "[OK] Matrix geometry set to " << cfg.width << "x" << cfg.height << " ("
                << (cfg.matrixWiring == MATRIX_WIRING_SERPENTINE ? "serpentine" : "progressive") << ", "
                << (cfg.matrixAxis == MATRIX_AXIS_VERTICAL ? "vertical" : "horizontal") << ")\n";
    } else {
      std::cout << "[CLI] Invalid matrix dimensions: set matrix <w> <h> [wiring] [axis] [origin]\n";
    }
  } else if (lowerKey == "save" || lowerKey == "config") {
    cfg.save();
  } else {
    std::cout << "[CLI] Unknown key '" << key << "'. Type 'help' to see configurable keys.\n";
  }
}

void SimSerial::handleJson(const std::string& jsonStr, SimConfig& cfg, SimDisplay& display,
                           std::vector<uint8_t>& frameBuffer,
                           std::vector<uint8_t>& monoBuffer,
                           bool& wakeRequested,
                           bool& forceFullRefresh,
                           bool& running) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    std::cout << "{\"status\":\"error\",\"v\":1,\"message\":\"Invalid JSON\"}\n";
    return;
  }

  std::string cmd = doc["cmd"] | "";
  if (cmd == "ping") {
    std::cout << "{\"status\":\"ok\",\"pong\":true}\n";
  } else if (cmd == "status") {
    printStatus(cfg);
  } else if (cmd == "export" || cmd == "export_config") {
    printExport(cfg, doc["pretty"] | false);
  } else if (cmd == "refresh") {
    wakeRequested = true;
    std::cout << "{\"status\":\"ok\",\"v\":1,\"message\":\"refresh_triggered\"}\n";
  } else if (cmd == "reboot" || cmd == "restart") {
    std::cout << "{\"status\":\"ok\",\"v\":1,\"message\":\"rebooting\"}\n";
  } else if (cmd == "config") {
    if (doc["server"]["url"].is<const char*>()) cfg.serverUrl = doc["server"]["url"].as<std::string>();
    if (doc["server"]["device_id"].is<const char*>()) cfg.deviceId = doc["server"]["device_id"].as<std::string>();
    if (doc["server"]["device_token"].is<const char*>()) cfg.deviceToken = doc["server"]["device_token"].as<std::string>();
    if (doc["screen"]["width"].is<int>()) {
      cfg.width = cfg.nativeWidth = doc["screen"]["width"].as<int>();
      cfg.customDimensions = true;
    }
    if (doc["screen"]["height"].is<int>()) {
      cfg.height = cfg.nativeHeight = doc["screen"]["height"].as<int>();
      cfg.customDimensions = true;
    }
    display.updateDimensions(cfg.width, cfg.height);
    frameBuffer.resize(cfg.bufferSize(), 0xFF);
    std::cout << "{\"status\":\"ok\",\"v\":1}\n";
  } else {
    std::cout << "{\"status\":\"error\",\"v\":1,\"message\":\"Unknown JSON command: " << cmd << "\"}\n";
  }
}

void SimSerial::processLine(const std::string& rawLine,
                            SimConfig& cfg,
                            SimDisplay& display,
                            std::vector<uint8_t>& frameBuffer,
                            std::vector<uint8_t>& monoBuffer,
                            bool& wakeRequested,
                            bool& forceFullRefresh,
                            bool& running) {
  std::string line = rawLine;
  size_t s = line.find_first_not_of(" \t\r\n");
  size_t e = line.find_last_not_of(" \t\r\n");
  if (s == std::string::npos) return;
  line = line.substr(s, e - s + 1);

  if (line[0] == '{') {
    handleJson(line, cfg, display, frameBuffer, monoBuffer, wakeRequested, forceFullRefresh, running);
    return;
  }

  size_t sp = line.find(' ');
  std::string cmd = (sp != std::string::npos) ? line.substr(0, sp) : line;
  std::string args = (sp != std::string::npos) ? line.substr(sp + 1) : "";
  size_t as = args.find_first_not_of(" \t");
  if (as != std::string::npos) args = args.substr(as);
  else args.clear();

  std::string lowerCmd = cmd;
  std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), [](unsigned char c) { return std::tolower(c); });

  if (lowerCmd == "help" || lowerCmd == "?") {
    printHelp(cfg);
    return;
  }
  if (lowerCmd == "status") {
    printStatus(cfg);
    return;
  }
  if (lowerCmd == "get") {
    handleGet(args, cfg);
    return;
  }
  if (lowerCmd == "set") {
    handleSet(args, cfg, display, frameBuffer, monoBuffer, wakeRequested, forceFullRefresh);
    return;
  }
  if (lowerCmd == "export" || lowerCmd == "dump") {
    printExport(cfg, args == "pretty" || args == "-p");
    return;
  }
  if (lowerCmd == "refresh") {
    wakeRequested = true;
    std::cout << "[Simulator] Sync refresh triggered via serial stdio.\n";
    return;
  }
  if (lowerCmd == "clean" || lowerCmd == "full_refresh") {
    forceFullRefresh = true;
    wakeRequested = true;
    std::cout << "[Simulator] Full clean refresh triggered.\n";
    return;
  }
  if (lowerCmd == "reboot" || lowerCmd == "restart") {
    std::cout << "[Simulator] Simulated reboot executed.\n";
    return;
  }
  if (lowerCmd == "reset") {
    cfg.deviceId.clear();
    cfg.deviceToken.clear();
    cfg.pairMode = true;
    std::cout << "[Simulator] Factory reset executed. Device unpaired.\n";
    return;
  }
  if (lowerCmd == "pair") {
    cfg.pairMode = true;
    wakeRequested = true;
    std::cout << "[Simulator] Device pairing flow initiated.\n";
    return;
  }
  if (lowerCmd == "scan") {
    std::cout << "[WiFi] Visible networks: 'smarthome' (-58 dBm, Ch 6), 'Guest-WiFi' (-74 dBm, Ch 11)\n";
    return;
  }
  if (lowerCmd == "logs") {
    diag_log_dump_stdout();
    return;
  }
  if (lowerCmd == "battery" || lowerCmd == "batt") {
    std::cout << "Battery: " << cfg.batteryPercent << "%, Power: " << cfg.powerSource
              << (cfg.isCharging ? " (charging)" : "") << "\n";
    return;
  }
  if (lowerCmd == "led_test" && (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888)) {
    std::cout << "[LED] Running LED test pattern...\n";
    for (size_t i = 0; i < frameBuffer.size(); i += 3) {
      frameBuffer[i] = 255;
      frameBuffer[i + 1] = 128;
      frameBuffer[i + 2] = 0;
    }
    display.renderBuffer(frameBuffer.data());
    display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
    return;
  }
  if (lowerCmd == "led_off" && (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888)) {
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0);
    display.renderBuffer(frameBuffer.data());
    display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
    std::cout << "[LED] LEDs turned off.\n";
    return;
  }
  if (lowerCmd == "led_red" && (cfg.isLedDevice || cfg.colorMode == SIM_COLOR_RGB888)) {
    for (size_t i = 0; i < frameBuffer.size(); i += 3) {
      frameBuffer[i] = 255; frameBuffer[i + 1] = 0; frameBuffer[i + 2] = 0;
    }
    display.renderBuffer(frameBuffer.data());
    display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
    std::cout << "[LED] Solid Red\n";
    return;
  }
  if (lowerCmd == "poweroff" || lowerCmd == "shutdown" || lowerCmd == "off" || lowerCmd == "exit" || lowerCmd == "quit") {
    std::cout << "[Simulator] Exiting on serial command.\n";
    running = false;
    return;
  }

  std::cout << "[CLI] Unknown command: '" << cmd << "'. Type 'help' for command list.\n";
}
