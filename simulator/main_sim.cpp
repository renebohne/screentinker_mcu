#include "sim_config.h"
#include "sim_display.h"
#include "sim_http.h"
#include "setup_screen.h"
#include <ArduinoJson.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

int main(int argc, char** argv) {
  SimConfig cfg = SimConfig::load(argc, argv);
  cfg.print();

  SimDisplay display(cfg.headless, 800, 480);
  if (!display.init()) {
    std::cerr << "[Simulator] Failed to initialize display.\n";
    return 1;
  }

  SimHttp http(cfg.timeoutMs);
  uint8_t monoBuffer[EPD_BUFFER_SIZE];

  // 1. Show Splash Screen at boot
  std::cout << "[Simulator] Booting: Displaying Firmware v" << cfg.firmwareVersion << " Splash Screen...\n";
  renderSplashScreen(monoBuffer, cfg.firmwareVersion.c_str(), "Connecting to ScreenTinker...", LANG_EN);
  display.renderBuffer(monoBuffer);
  display.saveBmp(monoBuffer, cfg.snapshotPath);

  // Small delay so user can see splash screen in window
  if (!cfg.headless && !cfg.oneShot) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  }

  std::string lastEtag = "";
  int sleepDurationSec = cfg.syncIntervalSec;
  bool running = true;

  while (running) {
    std::string url = cfg.serverUrl + "/api/embedded/render?device_id=" + cfg.deviceId;
    std::cout << "\n[Sync] Polling server: " << url << "\n";
    std::cout << "[Sync] Telemetry: Battery=" << cfg.batteryPercent << "%, RSSI=" << cfg.wifiRssi << " dBm, ETag=" << (lastEtag.empty() ? "(none)" : lastEtag) << "\n";

    SimHttpResponse res = http.get(
      url,
      cfg.deviceToken,
      cfg.firmwareVersion,
      cfg.batteryPercent,
      cfg.wifiRssi,
      lastEtag
    );

    std::cout << "[Sync] Server response status: " << res.statusCode << "\n";

    if (res.hasHeader("x-st-expires-in")) {
      int exp = std::atoi(res.getHeader("x-st-expires-in").c_str());
      if (exp > 0) sleepDurationSec = exp;
    }
    if (res.hasHeader("x-st-power-state")) {
      std::cout << "[Sync] Power State: " << res.getHeader("x-st-power-state") << "\n";
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
    } else {
      std::cerr << "[Sync] Connection failed (code " << res.statusCode << "). Rendering No Wi-Fi / Offline warning...\n";
      renderNoWifiScreen(monoBuffer, "ScreenTinker Server", LANG_EN);
      display.renderBuffer(monoBuffer);
      display.saveBmp(monoBuffer, cfg.snapshotPath);
    }

    if (cfg.oneShot) {
      std::cout << "[Simulator] One-shot run complete. Exiting.\n";
      break;
    }

    std::cout << "[Sleep] Next poll scheduled in " << sleepDurationSec << "s (Press [Space] to wake immediately, [B] to toggle battery, [Q] to quit)\n";

    // Simulate sleep interval while polling interactive SDL2 events
    auto sleepStart = std::chrono::steady_clock::now();
    bool wakeRequested = false;

    while (running && !wakeRequested) {
      int batteryDelta = 0;
      if (!display.pollEvents(wakeRequested, batteryDelta)) {
        running = false;
        break;
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
