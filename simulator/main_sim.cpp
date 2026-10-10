#include "sim_config.h"
#include "sim_display.h"
#include "sim_http.h"
#include "setup_screen.h"
#include <ArduinoJson.h>
#include <qrcode.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

static bool runPairingFlow(SimConfig& cfg, SimHttp& http, SimDisplay& display, uint8_t* monoBuffer) {
  std::cout << "\n=======================================================\n"
            << "  STARTING AUTOMATIC 6-DIGIT DEVICE PAIRING FLOW\n"
            << "=======================================================\n"
            << " • Server:         " << cfg.serverUrl << "\n"
            << " • Screen Profile: 800x480 (Seeed reTerminal Sticky)\n";

  std::string registerUrl = cfg.serverUrl + "/api/embedded/pair/register";
  std::string regBody = "{\"screen_profile\":\"seeed-reterminal-sticky\",\"screen_width\":800,\"screen_height\":480}";

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
  renderPairingCodeScreen(monoBuffer, pairingCode.c_str(), cfg.serverUrl.c_str(), LANG_EN);
  display.renderBuffer(monoBuffer);
  display.saveBmp(monoBuffer, cfg.snapshotPath);

  // Poll pairing status every 2.5 seconds until claimed
  std::string statusUrl = cfg.serverUrl + "/api/embedded/pair/status?device_id=" + deviceId;
  bool paired = false;

  while (!paired) {
    bool wakeRequested = false;
    int batteryDelta = 0;
    bool powerToggled = false;
    if (!display.pollEvents(wakeRequested, batteryDelta, powerToggled)) {
      std::cout << "[Pairing] Aborted by user.\n";
      return false;
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
  SimConfig cfg = SimConfig::load(argc, argv);
  cfg.print();

  SimDisplay display(cfg.headless, 800, 480, cfg.scale);
  if (!display.init()) {
    std::cerr << "[Simulator] Failed to initialize display.\n";
    return 1;
  }

  SimHttp http(cfg.timeoutMs);
  uint8_t monoBuffer[EPD_BUFFER_SIZE];

  // Check if Pairing flow is required
  if (cfg.pairMode || cfg.deviceToken.empty()) {
    if (!runPairingFlow(cfg, http, display, monoBuffer)) {
      std::cerr << "[Simulator] Pairing failed or was aborted.\n";
      return 1;
    }
    // Small delay after pairing success before syncing first frame
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  } else {
    // 1. Show Splash Screen at boot
    std::cout << "[Simulator] Booting: Displaying Firmware v" << cfg.firmwareVersion << " Splash Screen...\n";
    renderSplashScreen(monoBuffer, cfg.firmwareVersion.c_str(), "Connecting to ScreenTinker...", LANG_EN);
    display.renderBuffer(monoBuffer);
    display.saveBmp(monoBuffer, cfg.snapshotPath);

    // Small delay so user can see splash screen in window
    if (!cfg.headless && !cfg.oneShot) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
  }

  std::string lastEtag = "";
  int sleepDurationSec = cfg.syncIntervalSec;
  bool running = true;
  int cycleCount = 0;

  while (running) {
    std::string url = cfg.serverUrl + "/api/embedded/render?device_id=" + cfg.deviceId;
    std::cout << "\n[Sync] Polling server: " << url << "\n";
    std::cout << "[Sync] Telemetry: Power=" << cfg.powerSource << (cfg.isCharging ? " (Charging)" : "")
              << ", Battery=" << cfg.batteryPercent << "%, RSSI=" << cfg.wifiRssi
              << " dBm, ETag=" << (lastEtag.empty() ? "(none)" : lastEtag) << "\n";

    SimHttpResponse res = http.get(
      url,
      cfg.deviceToken,
      cfg.firmwareVersion,
      cfg.batteryPercent,
      cfg.wifiRssi,
      lastEtag,
      cfg.powerSource,
      cfg.isCharging
    );

    std::cout << "[Sync] Server response status: " << res.statusCode << "\n";

    if (res.hasHeader("x-st-expires-in")) {
      int exp = std::atoi(res.getHeader("x-st-expires-in").c_str());
      if (exp > 0) sleepDurationSec = exp;
    }
    if (res.hasHeader("x-st-power-state")) {
      std::cout << "[Sync] Power State: " << res.getHeader("x-st-power-state") << "\n";
    }

    std::string cmd = res.getHeader("x-st-command");
    if (!cmd.empty()) {
      std::cout << "[Sync] Remote command received from server: " << cmd << "\n";
      if (cmd == "reboot") {
        std::cout << "[Simulator] Remote reboot executed! Resetting simulated device state...\n";
        lastEtag = "";
      }
    }

    if (res.statusCode == 200) {
      std::string etag = res.getHeader("etag");
      if (!etag.empty()) lastEtag = etag;

      if (res.body.size() == EPD_BUFFER_SIZE) {
        std::memcpy(monoBuffer, res.body.data(), EPD_BUFFER_SIZE);
        std::cout << "[Sync] 200 OK — Downloaded new frame (" << EPD_BUFFER_SIZE << " bytes). Updating display.\n";
        display.renderBuffer(monoBuffer);
        display.saveBmp(monoBuffer, cfg.snapshotPath);
      } else {
        std::cerr << "[Sync] Warning: Received 200 OK but payload size is " << res.body.size()
                  << " bytes (expected " << EPD_BUFFER_SIZE << ")\n";
      }
    } else if (res.statusCode == 304) {
      std::cout << "[Sync] 304 Not Modified — Cache valid (0 bytes downloaded). E-Paper image preserved.\n";
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

      renderEmptyPlaylistScreen(monoBuffer, deviceName.c_str(), cfg.serverUrl.c_str(), LANG_EN);
      display.renderBuffer(monoBuffer);
      display.saveBmp(monoBuffer, cfg.snapshotPath);
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
      renderUnsupportedContentScreen(monoBuffer, detail.c_str(), LANG_EN);
      display.renderBuffer(monoBuffer);
      display.saveBmp(monoBuffer, cfg.snapshotPath);
      lastEtag = "";
    } else {
      std::cerr << "[Sync] Connection failed (code " << res.statusCode << "). Rendering No Wi-Fi / Offline warning...\n";
      renderNoWifiScreen(monoBuffer, "ScreenTinker Server", LANG_EN);
      display.renderBuffer(monoBuffer);
      display.saveBmp(monoBuffer, cfg.snapshotPath);
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

    std::cout << "[Sleep] Next poll scheduled in " << sleepDurationSec << "s (Press [Space] to wake, [P] to toggle USB/Battery, [B] to toggle battery level, [Q] to quit)\n";

    // Simulate sleep interval while polling interactive SDL2 events
    auto sleepStart = std::chrono::steady_clock::now();
    bool wakeRequested = false;

    while (running && !wakeRequested) {
      int batteryDelta = 0;
      bool powerToggled = false;
      if (!display.pollEvents(wakeRequested, batteryDelta, powerToggled)) {
        running = false;
        break;
      }

      if (powerToggled) {
        cfg.powerSource = (cfg.powerSource == "usb") ? "battery" : "usb";
        cfg.isCharging = (cfg.powerSource == "usb" && cfg.batteryPercent < 100);
        std::cout << "[Simulator] Power source toggled to: " << cfg.powerSource
                  << (cfg.isCharging ? " (Charging)" : "") << "!\n";
        wakeRequested = true;
      }

      if (batteryDelta == 999) {
        cfg.batteryPercent = (cfg.batteryPercent > 20) ? 12 : 100;
        std::cout << "[Simulator] Battery level toggled to " << cfg.batteryPercent << "%\n";
      } else if (batteryDelta != 0) {
        cfg.batteryPercent = std::max(0, std::min(100, cfg.batteryPercent + batteryDelta));
        std::cout << "[Simulator] Battery adjusted to " << cfg.batteryPercent << "%\n";
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
