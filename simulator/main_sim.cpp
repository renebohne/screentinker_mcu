#include "sim_config.h"
#include "sim_display.h"
#include "sim_http.h"
#include "diag_logger.h"
#include "setup_screen.h"
#include "ota_updater.h"
#include "touch_driver.h"
#include <ArduinoJson.h>
#include <qrcode.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>

static void monoToNativeBuffer(const uint8_t* monoBuf, uint8_t* nativeBuf, int width, int height, SimColorMode colorMode) {
  if (colorMode == SIM_COLOR_1BPP_MONO) {
    std::memcpy(nativeBuf, monoBuf, ((size_t)width * height) / 8);
    return;
  }
  if (colorMode == SIM_COLOR_SPECTRA6) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        size_t monoIdx = ((size_t)y * (width / 8)) + (x / 8);
        uint8_t bit = (monoBuf[monoIdx] >> (7 - (x & 7))) & 1;
        uint8_t nibble = bit ? 0x01 : 0x00;
        size_t natIdx = ((size_t)y * (width / 2)) + (x / 2);
        if ((x & 1) == 0) {
          nativeBuf[natIdx] = (nibble << 4);
        } else {
          nativeBuf[natIdx] |= (nibble & 0x0F);
        }
      }
    }
  } else if (colorMode == SIM_COLOR_GRAY16) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        size_t monoIdx = ((size_t)y * (width / 8)) + (x / 8);
        uint8_t bit = (monoBuf[monoIdx] >> (7 - (x & 7))) & 1;
        uint8_t nibble = bit ? 0x0F : 0x00;
        size_t natIdx = ((size_t)y * (width / 2)) + (x / 2);
        if ((x & 1) == 0) {
          nativeBuf[natIdx] = (nibble << 4);
        } else {
          nativeBuf[natIdx] |= (nibble & 0x0F);
        }
      }
    }
  } else if (colorMode == SIM_COLOR_GRAY4) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        size_t monoIdx = ((size_t)y * (width / 8)) + (x / 8);
        uint8_t bit = (monoBuf[monoIdx] >> (7 - (x & 7))) & 1;
        uint8_t val = bit ? 0x03 : 0x00;
        size_t natIdx = ((size_t)y * (width / 4)) + (x / 4);
        uint8_t shift = 6 - 2 * (x & 3);
        if ((x & 3) == 0) nativeBuf[natIdx] = 0;
        nativeBuf[natIdx] |= (val << shift);
      }
    }
  }
}

static bool runPairingFlow(SimConfig& cfg, SimHttp& http, SimDisplay& display, std::vector<uint8_t>& frameBuffer) {
  std::cout << "\n=======================================================\n"
            << "  STARTING AUTOMATIC 6-DIGIT DEVICE PAIRING FLOW\n"
            << "=======================================================\n"
            << " • Server:         " << cfg.serverUrl << "\n"
            << " • Screen Profile: " << cfg.width << "x" << cfg.height << " (" << cfg.screenProfile << ")\n"
            << " • Color Mode:     " << cfg.colorModeString() << "\n";

  std::string registerUrl = cfg.serverUrl + "/api/embedded/pair/register";
  std::string regBody = "{\"screen_profile\":\"" + cfg.screenProfile +
                        "\",\"screen_width\":" + std::to_string(cfg.width) +
                        ",\"screen_height\":" + std::to_string(cfg.height) +
                        ",\"color_mode\":\"" + cfg.colorModeString() +
                        "\",\"supports_partial_refresh\":" + (cfg.supportsPartialRefresh ? "true" : "false") +
                        ",\"full_refresh_time_ms\":" + std::to_string(cfg.fullRefreshTimeMs) +
                        ",\"partial_refresh_time_ms\":" + std::to_string(cfg.partialRefreshTimeMs) + "}";

  SimHttpResponse regRes = http.postJson(registerUrl, regBody);
  if (regRes.statusCode != 200) {
    std::cerr << "[Pairing] Error: Server returned status " << regRes.statusCode
              << " during pair/register: " << regRes.bodyAsString() << "\n";
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, regRes.bodyAsString());
  if (err || !doc["device_id"].is<const char*>() || !doc["pairing_code"].is<const char*>() || !doc["claim_secret"].is<const char*>()) {
    std::cerr << "[Pairing] Error: Failed to parse registration JSON: " << regRes.bodyAsString() << "\n";
    return false;
  }

  std::string deviceId = doc["device_id"].as<std::string>();
  std::string pairingCode = doc["pairing_code"].as<std::string>();
  std::string claimSecret = doc["claim_secret"].as<std::string>();

  std::cout << " • Device ID:    " << deviceId << "\n"
            << " • Pairing PIN:  " << pairingCode << "\n"
            << "-------------------------------------------------------\n"
            << " >>> Please enter the 6-digit PIN [" << pairingCode << "] in your ScreenTinker web dashboard <<<\n"
            << "=======================================================\n";

  // Render Pairing PIN + QR code on E-Paper display
  std::vector<uint8_t> monoBuffer(gfx_get_buffer_size(), 0xFF);
  renderPairingCodeScreen(monoBuffer.data(), pairingCode.c_str(), cfg.serverUrl.c_str(), LANG_EN);
  monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
  display.renderBuffer(frameBuffer.data());
  display.saveBmp(frameBuffer.data(), cfg.snapshotPath);

  // Poll pairing status every 2.5 seconds until claimed
  std::string statusUrl = cfg.serverUrl + "/api/embedded/pair/status?device_id=" + deviceId;
  bool paired = false;

  while (!paired) {
    bool wakeRequested = false;
    int batteryDelta = 0;
    bool powerToggled = false;
    bool rotateRequested = false;
    if (!display.pollEvents(wakeRequested, batteryDelta, powerToggled, rotateRequested)) {
      std::cout << "[Pairing] Aborted by user.\n";
      return false;
    }

    if (rotateRequested) {
      cfg.cycleRotation();
      gfx_set_screen_dimensions(cfg.width, cfg.height);
      display.updateDimensions(cfg.width, cfg.height);
      frameBuffer = std::vector<uint8_t>(cfg.bufferSize(), 0xFF);
      monoBuffer.resize(gfx_get_buffer_size(), 0xFF);
      renderPairingCodeScreen(monoBuffer.data(), pairingCode.c_str(), cfg.serverUrl.c_str(), LANG_EN);
      monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
      display.renderBuffer(frameBuffer.data());
      display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    SimHttpResponse statusRes = http.get(statusUrl, claimSecret);
    if (statusRes.statusCode == 200) {
      JsonDocument statusDoc;
      if (!deserializeJson(statusDoc, statusRes.bodyAsString())) {
        if (statusDoc["paired"].as<bool>() == true) {
          std::string token = statusDoc["device_token"].as<std::string>();
          std::string devId = statusDoc["device_id"].is<const char*>() ? statusDoc["device_id"].as<std::string>() : deviceId;

          std::cout << "\n🎉 [Pairing] SUCCESS! Device claimed by user in ScreenTinker UI!\n"
                    << " • Device ID:    " << devId << "\n"
                    << " • Device Token: " << token << "\n";

          cfg.deviceId = devId;
          cfg.deviceToken = token;
          cfg.pairMode = false;
          cfg.save(); // Save to sim_config.json
          paired = true;
          break;
        } else {
          std::cout << "[Pairing] Waiting for PIN confirmation (" << pairingCode << ") on web UI...\n";
        }
      }
    } else {
      std::cerr << "[Pairing] Status check failed (HTTP " << statusRes.statusCode << "). Retrying...\n";
    }
  }

  return true;
}

int main(int argc, char** argv) {
  diag_log_init();
  SimConfig cfg = SimConfig::load(argc, argv);
  diag_log_printf(DIAG_LOG_INFO, "Boot", "Simulator starting up: model %s (%dx%d)", cfg.model.c_str(), cfg.width, cfg.height);
  cfg.print();

  gfx_set_screen_dimensions(cfg.width, cfg.height);

  SimDisplay display(cfg.headless, cfg.width, cfg.height, cfg.scale, cfg.colorMode);
  if (!display.init()) {
    std::cerr << "[Simulator] Failed to initialize display.\n";
    return 1;
  }

  SimHttp http(cfg.timeoutMs);
  std::vector<uint8_t> frameBuffer(cfg.bufferSize(), 0xFF);
  std::vector<uint8_t> monoBuffer(gfx_get_buffer_size(), 0xFF);

  // Check critical battery depletion before any network or flow operations
  if (cfg.powerSource != "usb" && !cfg.isCharging && cfg.batteryPercent >= 0 && cfg.batteryPercent <= 1) {
    std::cout << "\n=============================================\n";
    std::cout << "  [Power] CRITICAL: Battery depleted (" << cfg.batteryPercent << "% <= 1%)!\n";
    std::cout << "  [Power] RENDERING BATTERY DEPLETED SHUTDOWN SCREEN & POWERING OFF\n";
    std::cout << "=============================================\n";
    renderBatteryDepletedScreen(monoBuffer.data(), LANG_EN);
    monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
    display.renderBuffer(frameBuffer.data());
    display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
    std::cout << "[Simulator] Device powered off due to depleted battery. Exiting.\n";
    return 0;
  }

  // Check if Pairing flow is required
  if (cfg.pairMode || cfg.deviceToken.empty()) {
    if (!runPairingFlow(cfg, http, display, frameBuffer)) {
      std::cerr << "[Simulator] Pairing failed or was aborted.\n";
      return 1;
    }
    // Small delay after pairing success before syncing first frame
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  } else {
    // 1. Show Splash Screen at boot
    std::cout << "[Simulator] Booting: Displaying Firmware v" << cfg.firmwareVersion << " Splash Screen...\n";
    renderSplashScreen(monoBuffer.data(), cfg.firmwareVersion.c_str(), "Connecting to ScreenTinker...", LANG_EN);
    monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
    display.renderBuffer(frameBuffer.data());
    display.saveBmp(frameBuffer.data(), cfg.snapshotPath);

    // Small delay so user can see splash screen in window
    if (!cfg.headless && !cfg.oneShot) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
  }

  ota_init();
  std::string lastEtag = "";
  int sleepDurationSec = cfg.syncIntervalSec;
  bool running = true;
  int cycleCount = 0;
  int consecutivePartialCount = 0;
  const int partialRefreshLimit = 6;
  bool forceFullRefresh = true; // Initial boot always triggers full refresh

  bool simInMenu = false;
  bool simInStatus = false;
  int simMenuSelection = 0;
  Language simLang = LANG_EN;
  uint8_t simWifiPowerLevel = 0;
  uint8_t simRefreshMode = 0;

  while (running) {
    if (cfg.powerSource != "usb" && !cfg.isCharging && cfg.batteryPercent >= 0 && cfg.batteryPercent <= 1) {
      std::cout << "\n=============================================\n";
      std::cout << "  [Power] CRITICAL: Battery depleted (" << cfg.batteryPercent << "% <= 1%)!\n";
      std::cout << "  [Power] RENDERING BATTERY DEPLETED SHUTDOWN SCREEN & POWERING OFF\n";
      std::cout << "=============================================\n";
      renderBatteryDepletedScreen(monoBuffer.data(), LANG_EN);
      monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
      display.renderBuffer(frameBuffer.data());
      display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
      std::cout << "[Simulator] Device powered off due to depleted battery. Exiting.\n";
      return 0;
    }

    std::string url = cfg.serverUrl + "/api/embedded/render?device_id=" + cfg.deviceId;
    if (!cfg.orientation.empty()) {
      url += "&orientation=" + cfg.orientation;
    }
    if (cfg.rotation != 0) {
      url += "&rotation=" + std::to_string(cfg.rotation);
    }
    std::cout << "\n[Sync] Polling server: " << url << "\n";
    std::cout << "[Sync] Telemetry: Power=" << cfg.powerSource << (cfg.isCharging ? " (Charging)" : "")
              << ", Battery=" << cfg.batteryPercent << "%, RSSI=" << cfg.wifiRssi
              << " dBm";
    if (cfg.hasSht40) {
      std::cout << ", Ambient SHT40=" << cfg.mockTemp << "°C / " << cfg.mockHumidity << "% RH";
    }
    std::cout << ", ETag=" << (lastEtag.empty() ? "(none)" : lastEtag) << "\n";

    SimHttpResponse res = http.get(
      url,
      cfg.deviceToken,
      cfg.firmwareVersion,
      cfg.batteryPercent,
      cfg.wifiRssi,
      lastEtag,
      cfg.powerSource,
      cfg.isCharging,
      cfg.hasSht40 ? cfg.mockTemp : -999.0f,
      cfg.hasSht40 ? cfg.mockHumidity : -1.0f
    );

    std::cout << "[Sync] Server response status: " << res.statusCode << "\n";

    if (res.hasHeader("x-st-screen-width") && res.hasHeader("x-st-screen-height")) {
      int serverW = std::atoi(res.getHeader("x-st-screen-width").c_str());
      int serverH = std::atoi(res.getHeader("x-st-screen-height").c_str());
      if (serverW > 0 && serverH > 0 && (serverW != cfg.width || serverH != cfg.height)) {
        std::cout << "[Sync] Server updated dimensions to " << serverW << "x" << serverH
                  << " (was " << cfg.width << "x" << cfg.height << ")\n";
        cfg.width = serverW;
        cfg.height = serverH;
        gfx_set_screen_dimensions(cfg.width, cfg.height);
        display.updateDimensions(cfg.width, cfg.height);
        frameBuffer.resize(cfg.bufferSize(), 0xFF);
        monoBuffer.resize(gfx_get_buffer_size(), 0xFF);
      }
    }
    if (res.hasHeader("x-st-rotation")) {
      cfg.rotation = std::atoi(res.getHeader("x-st-rotation").c_str());
    }
    if (res.hasHeader("x-st-orientation")) {
      cfg.orientation = res.getHeader("x-st-orientation");
    }

    if (res.hasHeader("x-st-expires-in")) {
      int exp = std::atoi(res.getHeader("x-st-expires-in").c_str());
      if (exp > 0) sleepDurationSec = exp;
    }
    if (res.hasHeader("x-st-power-state")) {
      std::cout << "[Sync] Power State: " << res.getHeader("x-st-power-state") << "\n";
    }
    if (res.hasHeader("x-st-refresh-mode")) {
      std::string reqRefresh = res.getHeader("x-st-refresh-mode");
      if (reqRefresh == "partial" && !cfg.supportsPartialRefresh) {
        std::cout << "[Display] Notice: Device '" << cfg.model
                  << "' (Spectra 6) does not support partial refresh; promoting to full refresh.\n";
      }
    }

    std::string cmd = res.getHeader("x-st-command");
    if (!cmd.empty()) {
      std::cout << "[Sync] Remote command received from server: " << cmd << "\n";
      diag_log_printf(DIAG_LOG_INFO, "Sync", "Remote command: %s", cmd.c_str());
      if (cmd == "reboot") {
        std::cout << "[Simulator] Remote reboot executed! Resetting simulated device state...\n";
        lastEtag = "";
      } else if (cmd == "upload_logs" || cmd == "logs") {
        std::cout << "[Diag] Uploading diagnostic logs to server...\n";
        std::string logJson = diag_log_to_json_string(cfg.deviceId.c_str(), cfg.firmwareVersion.c_str(), cfg.batteryPercent, cfg.wifiRssi);
        std::string uploadUrl = cfg.serverUrl + "/api/embedded/logs?device_id=" + cfg.deviceId;
        SimHttpResponse logRes = http.postJson(uploadUrl, logJson, cfg.deviceToken);
        if (logRes.statusCode >= 200 && logRes.statusCode < 300) {
          std::cout << "[Diag] Logs successfully uploaded (HTTP " << logRes.statusCode << "). Clearing buffer.\n";
          diag_log_clear();
        } else {
          std::cerr << "[Diag] Log upload failed (HTTP " << logRes.statusCode << ")\n";
        }
      } else if (cmd.rfind("update ", 0) == 0 || cmd.rfind("ota ", 0) == 0) {
        std::string rest = cmd.substr(cmd.find(' ') + 1);
        size_t firstNonSpace = rest.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) rest = rest.substr(firstNonSpace);
        std::string updateUrl = rest;
        std::string sha = "";
        size_t secondSpace = rest.find(' ');
        if (secondSpace != std::string::npos) {
          updateUrl = rest.substr(0, secondSpace);
          sha = rest.substr(secondSpace + 1);
          size_t shaNonSpace = sha.find_first_not_of(" \t");
          if (shaNonSpace != std::string::npos) {
            sha = sha.substr(shaNonSpace);
          } else {
            sha = "";
          }
        }
        if (!updateUrl.empty() && updateUrl[0] == '/') {
          updateUrl = cfg.serverUrl + updateUrl;
        }
        std::cout << "[Simulator] Remote OTA update command received: " << updateUrl
                  << (sha.empty() ? "" : (" (SHA: " + sha + ")")) << "\n";
        renderOtaUpdateScreen(monoBuffer.data(), "Updating Firmware...", LANG_EN);
        monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
        display.renderBuffer(frameBuffer.data());
        display.saveBmp(frameBuffer.data(), cfg.snapshotPath);

        bool otaSuccess = ota_perform_update(updateUrl.c_str(), sha.empty() ? nullptr : sha.c_str());
        if (otaSuccess) {
          std::cout << "[Simulator] Simulated OTA update complete. Resetting state.\n";
          lastEtag = "";
        } else {
          std::cerr << "[Simulator] Simulated OTA update failed!\n";
        }
      }
    }

    if (res.statusCode == 200) {
      std::string etag = res.getHeader("etag");
      if (!etag.empty()) lastEtag = etag;

      std::string cModeHdr = res.getHeader("x-st-color-mode");
      if ((cModeHdr == "gray4" || cModeHdr == "2bpp" || res.body.size() == ((size_t)cfg.width * cfg.height) / 4)
          && (cfg.model == "sticky" || cfg.model == "e1001")) {
        if (cfg.colorMode != SIM_COLOR_GRAY4) {
          std::cout << "[Sync] Detected 2bpp Gray4 payload (" << res.body.size() << " bytes). Updating display mode to Gray4.\n";
          cfg.colorMode = SIM_COLOR_GRAY4;
          display.setColorMode(SIM_COLOR_GRAY4);
          frameBuffer.resize(cfg.bufferSize(), 0xFF);
        }
      } else if ((cModeHdr == "mono" || cModeHdr == "1bpp" || res.body.size() == ((size_t)cfg.width * cfg.height) / 8)
                 && (cfg.model == "sticky" || cfg.model == "e1001")) {
        if (cfg.colorMode != SIM_COLOR_1BPP_MONO) {
          std::cout << "[Sync] Detected 1bpp Mono payload (" << res.body.size() << " bytes). Updating display mode to Mono.\n";
          cfg.colorMode = SIM_COLOR_1BPP_MONO;
          display.setColorMode(SIM_COLOR_1BPP_MONO);
          frameBuffer.resize(cfg.bufferSize(), 0xFF);
        }
      }

      if (res.body.size() == cfg.bufferSize()) {
        std::memcpy(frameBuffer.data(), res.body.data(), cfg.bufferSize());
        if (cfg.batteryPercent >= 0 && cfg.batteryPercent < 10 && !cfg.isCharging && cfg.powerSource != "usb") {
          std::cout << "[Power] Low battery warning (" << cfg.batteryPercent << "%)! Drawing low battery icon.\n";
          gfx_draw_low_battery_icon_native(frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode, -1, -1, cfg.batteryPercent);
        }

        std::string reqRefresh = res.getHeader("x-st-refresh-mode");
        bool useFastPartial = false;

        if (!cfg.supportsPartialRefresh) {
          useFastPartial = false;
          consecutivePartialCount = 0;
          std::cout << "[Display] Refresh: FULL [" << cfg.fullRefreshTimeMs << " ms]\n";
        } else if (forceFullRefresh) {
          useFastPartial = false;
          consecutivePartialCount = 0;
          forceFullRefresh = false;
          std::cout << "[Display] Refresh: FULL (Initial boot or forced clean) [" << cfg.fullRefreshTimeMs << " ms]\n";
        } else if (reqRefresh == "full") {
          useFastPartial = false;
          consecutivePartialCount = 0;
          std::cout << "[Display] Refresh: FULL (Server override: x-st-refresh-mode: full) [" << cfg.fullRefreshTimeMs << " ms]\n";
        } else if (consecutivePartialCount < partialRefreshLimit) {
          useFastPartial = true;
          consecutivePartialCount++;
          std::cout << "[Display] Refresh: PARTIAL (" << consecutivePartialCount << "/" << partialRefreshLimit << ") [" << cfg.partialRefreshTimeMs << " ms]\n";
        } else {
          useFastPartial = false;
          consecutivePartialCount = 0;
          std::cout << "[Display] Refresh: FULL (Clean cycle after " << partialRefreshLimit << " partial refreshes) [" << cfg.fullRefreshTimeMs << " ms]\n";
        }

        std::cout << "[Sync] 200 OK — Downloaded new frame (" << cfg.bufferSize() << " bytes). Updating display.\n";
        display.renderBuffer(frameBuffer.data());
        display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
        ota_confirm_running_partition();

        if (!cfg.headless) {
          std::this_thread::sleep_for(std::chrono::milliseconds(useFastPartial ? cfg.partialRefreshTimeMs : std::min(cfg.fullRefreshTimeMs, 1000)));
        }
      } else {
        std::cerr << "[Sync] Warning: Received 200 OK but payload size is " << res.body.size()
                  << " bytes (expected " << cfg.bufferSize() << ")\n";
      }
    } else if (res.statusCode == 304) {
      std::cout << "[Sync] 304 Not Modified — Cache valid (0 bytes downloaded). E-Paper image preserved.\n";
      ota_confirm_running_partition();
    } else if (res.statusCode == 404) {
      std::string deviceName = res.getHeader("x-st-device-name");
      std::string bodyStr = res.bodyAsString();

      if (!bodyStr.empty()) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, bodyStr);
        if (!err && doc["device_name"].is<const char*>()) {
          deviceName = doc["device_name"].as<std::string>();
        }
      }

      std::cout << "[Sync] 404 No Active Playlist — Rendering Empty Playlist card for '"
                << (deviceName.empty() ? "(unnamed)" : deviceName) << "'...\n";

      renderEmptyPlaylistScreen(monoBuffer.data(), deviceName.c_str(), cfg.serverUrl.c_str(), LANG_EN);
      monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
      if (cfg.batteryPercent >= 0 && cfg.batteryPercent < 10 && !cfg.isCharging && cfg.powerSource != "usb") {
        gfx_draw_low_battery_icon_native(frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode, -1, -1, cfg.batteryPercent);
      }
      display.renderBuffer(frameBuffer.data());
      display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
      ota_confirm_running_partition();
      lastEtag = ""; // Reset ETag on empty playlist
    } else if (res.statusCode == 501) {
      std::string detail = "Widget rendering requires a browser (set CHROME_PATH).";
      std::string bodyStr = res.bodyAsString();
      if (!bodyStr.empty()) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, bodyStr);
        if (!err && doc["detail"].is<const char*>()) {
          detail = doc["detail"].as<std::string>();
        } else if (!err && doc["error"].is<const char*>()) {
          detail = doc["error"].as<std::string>();
        }
      }
      std::cout << "[Sync] 501 Unsupported Content — " << detail << "\n";
      renderUnsupportedContentScreen(monoBuffer.data(), detail.c_str(), LANG_EN);
      monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
      if (cfg.batteryPercent >= 0 && cfg.batteryPercent < 10 && !cfg.isCharging && cfg.powerSource != "usb") {
        gfx_draw_low_battery_icon_native(frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode, -1, -1, cfg.batteryPercent);
      }
      display.renderBuffer(frameBuffer.data());
      display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
      ota_confirm_running_partition();
      lastEtag = "";
    } else {
      std::cerr << "[Sync] Connection failed (code " << res.statusCode << "). Rendering No Wi-Fi / Offline warning...\n";
      renderNoWifiScreen(monoBuffer.data(), "ScreenTinker Server", LANG_EN);
      monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
      if (cfg.batteryPercent >= 0 && cfg.batteryPercent < 10 && !cfg.isCharging && cfg.powerSource != "usb") {
        gfx_draw_low_battery_icon_native(frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode, -1, -1, cfg.batteryPercent);
      }
      display.renderBuffer(frameBuffer.data());
      display.saveBmp(frameBuffer.data(), cfg.snapshotPath);
    }

    if (cfg.oneShot) {
      std::cout << "[Simulator] One-shot run complete. Exiting.\n";
      return (res.statusCode == 0) ? 1 : 0;
    }

    cycleCount++;
    if (cfg.maxCycles > 0 && cycleCount >= cfg.maxCycles) {
      std::cout << "[Simulator] Completed " << cycleCount << " poll cycle(s). Exiting.\n";
      return (res.statusCode == 0) ? 1 : 0;
    }

    std::cout << "[Sleep] Next poll scheduled in " << sleepDurationSec << "s (Press [Space] to wake, [Click]/[M] for Touch/Menu, [F] to force full clean, [O]/[R] to rotate orientation, [P] to toggle USB/Battery, [B] to toggle battery level, [L] to dump logs, [Q] to quit)\n";

    // Simulate sleep interval while polling interactive SDL2 events
    auto sleepStart = std::chrono::steady_clock::now();
    bool wakeRequested = false;

    auto redrawSimMenu = [&](bool fastPartial) {
      if (simInStatus) {
        renderStatusInfoScreen(monoBuffer.data(), "192.168.1.100", "ScreenTinker-WLAN", cfg.wifiRssi, cfg.serverUrl.c_str(), cfg.deviceId.c_str(), cfg.firmwareVersion.c_str(), simLang, cfg.mockTemp, cfg.mockHumidity, cfg.batteryPercent, cfg.isCharging, (cfg.powerSource == "usb"));
      } else {
        renderSystemMenu(monoBuffer.data(), simMenuSelection, simLang, simWifiPowerLevel, 0, false, cfg.batteryPercent, cfg.isCharging, (cfg.powerSource == "usb"), simRefreshMode, cfg.supportsPartialRefresh);
      }
      monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
      display.renderBuffer(frameBuffer.data());
      std::cout << "[Simulator] " << (simInStatus ? "Status Screen" : "System Menu")
                << " rendered via " << (fastPartial ? "fast partial refresh" : "full refresh")
                << " (" << (fastPartial ? cfg.partialRefreshTimeMs : cfg.fullRefreshTimeMs) << "ms)\n";
    };

    while (running && !wakeRequested) {
      int batteryDelta = 0;
      bool powerToggled = false;
      bool rotateRequested = false;
      bool forceFullRequested = false;
      int touchX = 0, touchY = 0;
      bool touchClicked = false;
      if (!display.pollEvents(wakeRequested, batteryDelta, powerToggled, rotateRequested, forceFullRequested, touchX, touchY, touchClicked)) {
        running = false;
        break;
      }

      if (touchClicked) {
        if (!cfg.touchSupported) {
          std::cout << "[Simulator] Touch ignored: target model '" << cfg.model << "' does not support capacitive touch.\n";
          wakeRequested = false;
        } else {
          if (simInStatus) {
            std::cout << "[Simulator] Touch at (" << touchX << ", " << touchY << ") -> Exiting Status Screen back to Menu\n";
            simInStatus = false;
            simInMenu = true;
            redrawSimMenu(true);
            wakeRequested = false;
          } else if (simInMenu) {
            int row = touch_hit_test_menu(touchX, touchY, cfg.width, cfg.height);
            if (row >= 0 && row < 9) {
              simMenuSelection = row;
              std::cout << "[Simulator] Touch tap on menu item " << row << " at (" << touchX << ", " << touchY << ")!\n";
              if (row == 0) {
                simInMenu = false;
                simInStatus = true;
                redrawSimMenu(false);
                wakeRequested = false;
              } else if (row == 1) {
                cfg.cycleRotation();
                std::cout << "[Simulator] Orientation cycled to " << cfg.rotation << "°\n";
                gfx_set_screen_dimensions(cfg.width, cfg.height);
                display.updateDimensions(cfg.width, cfg.height);
                frameBuffer.resize(cfg.bufferSize(), 0xFF);
                monoBuffer.resize(gfx_get_buffer_size(), 0xFF);
                redrawSimMenu(true);
                wakeRequested = false;
              } else if (row == 2) {
                simWifiPowerLevel = (simWifiPowerLevel + 1) % 3;
                std::cout << "[Simulator] Wi-Fi Power Level: " << (int)simWifiPowerLevel << "\n";
                redrawSimMenu(true);
                wakeRequested = false;
              } else if (row == 3) {
                if (cfg.supportsPartialRefresh) {
                  simRefreshMode = (simRefreshMode == 0) ? 1 : 0;
                  std::cout << "[Simulator] Refresh Mode: " << (simRefreshMode == 0 ? "Smart Partial [6:1]" : "Always Full") << "\n";
                }
                redrawSimMenu(true);
                wakeRequested = false;
              } else if (row == 4) {
                simLang = (simLang == LANG_EN) ? LANG_DE : LANG_EN;
                std::cout << "[Simulator] Language: " << (simLang == LANG_DE ? "DE" : "EN") << "\n";
                redrawSimMenu(true);
                wakeRequested = false;
              } else if (row == 5) {
                std::cout << "[Simulator] Unpairing device...\n";
                cfg.deviceToken = "";
                simInMenu = false;
                cfg.pairMode = true;
                wakeRequested = true;
              } else if (row == 6) {
                std::cout << "[Simulator] Powering off device...\n";
                renderPowerOffScreen(monoBuffer.data(), simLang);
                monoToNativeBuffer(monoBuffer.data(), frameBuffer.data(), cfg.width, cfg.height, cfg.colorMode);
                display.renderBuffer(frameBuffer.data());
                running = false;
                break;
              } else if (row == 7) {
                std::cout << "[Simulator] Factory Resetting...\n";
                cfg.deviceToken = "";
                simInMenu = false;
                cfg.pairMode = true;
                wakeRequested = true;
              } else if (row == 8) {
                std::cout << "[Simulator] Closing System Menu, resuming normal slideshow.\n";
                simInMenu = false;
                wakeRequested = true;
              }
            } else {
              wakeRequested = false;
            }
          } else {
            std::cout << "[Simulator] Capacitive touch at (" << touchX << ", " << touchY << ") -> Opening System Menu!\n";
            simInMenu = true;
            simMenuSelection = 0;
            redrawSimMenu(false);
            wakeRequested = false;
          }
        }
      }

      if (forceFullRequested) {
        forceFullRefresh = true;
        wakeRequested = true;
      }

      if (rotateRequested) {
        forceFullRefresh = true;
        consecutivePartialCount = 0;
        cfg.cycleRotation();
        std::cout << "[Simulator] Rotated to " << cfg.rotation << "° ("
                  << (cfg.orientation.empty() ? (cfg.width > cfg.height ? "landscape" : "portrait") : cfg.orientation)
                  << "), new dimensions: " << cfg.width << "x" << cfg.height << "\n";
        gfx_set_screen_dimensions(cfg.width, cfg.height);
        display.updateDimensions(cfg.width, cfg.height);
        frameBuffer.resize(cfg.bufferSize(), 0xFF);
        monoBuffer.resize(gfx_get_buffer_size(), 0xFF);
        lastEtag = ""; // Invalidate ETag to fetch fresh frame for new orientation
        wakeRequested = true;
      }

      if (powerToggled) {
        cfg.powerSource = (cfg.powerSource == "usb") ? "battery" : "usb";
        cfg.isCharging = (cfg.powerSource == "usb" && cfg.batteryPercent < 100);
        std::cout << "[Simulator] Power source toggled to: " << cfg.powerSource
                  << (cfg.isCharging ? " (Charging)" : "") << "!\n";
        wakeRequested = true;
      }

      if (batteryDelta == 999) {
        cfg.batteryPercent = (cfg.batteryPercent > 20) ? 8 : ((cfg.batteryPercent == 8) ? 1 : 100);
        std::cout << "[Simulator] Battery level toggled to " << cfg.batteryPercent << "%\n";
        wakeRequested = true;
      } else if (batteryDelta != 0) {
        cfg.batteryPercent = std::max(0, std::min(100, cfg.batteryPercent + batteryDelta));
        std::cout << "[Simulator] Battery adjusted to " << cfg.batteryPercent << "%\n";
        wakeRequested = true;
      }

      auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - sleepStart).count();
      if (!cfg.headless && elapsedSec >= sleepDurationSec) {
        break; // Sleep elapsed
      }

      if (cfg.headless && cfg.fastForwardSleep) {
        std::cout << "[Simulator] Fast-forwarded sleep (" << sleepDurationSec << "s).\n";
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  std::cout << "[Simulator] Shutdown complete.\n";
  return 0;
}
