#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "sim_config.h"

class SimDisplay;

class SimSerial {
public:
  static void init();

  // Polls standard input non-blockingly for incoming serial commands.
  // Returns true if at least one command was read and executed.
  static bool poll(SimConfig& cfg, SimDisplay& display,
                   std::vector<uint8_t>& frameBuffer,
                   std::vector<uint8_t>& monoBuffer,
                   bool& wakeRequested,
                   bool& forceFullRefresh,
                   bool& running);

  // Directly processes a line (either JSON or CLI string)
  static void processLine(const std::string& rawLine,
                          SimConfig& cfg,
                          SimDisplay& display,
                          std::vector<uint8_t>& frameBuffer,
                          std::vector<uint8_t>& monoBuffer,
                          bool& wakeRequested,
                          bool& forceFullRefresh,
                          bool& running);

  static bool isEof();

private:
  static void printHelp(const SimConfig& cfg);
  static void printStatus(const SimConfig& cfg);
  static void printExport(const SimConfig& cfg, bool pretty = false);
  static void handleGet(const std::string& key, const SimConfig& cfg);
  static void handleSet(const std::string& args, SimConfig& cfg, SimDisplay& display,
                        std::vector<uint8_t>& frameBuffer,
                        std::vector<uint8_t>& monoBuffer,
                        bool& wakeRequested,
                        bool& forceFullRefresh);
  static void handleJson(const std::string& jsonStr, SimConfig& cfg, SimDisplay& display,
                         std::vector<uint8_t>& frameBuffer,
                         std::vector<uint8_t>& monoBuffer,
                         bool& wakeRequested,
                         bool& forceFullRefresh,
                         bool& running);
};
