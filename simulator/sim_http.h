#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct SimHttpResponse {
  int statusCode = 0;
  std::map<std::string, std::string> headers;
  std::vector<uint8_t> body;

  std::string getHeader(const std::string& name) const;
  bool hasHeader(const std::string& name) const;
  std::string bodyAsString() const;
};

class SimHttp {
public:
  SimHttp(int timeoutMs = 10000);
  ~SimHttp();

  SimHttpResponse get(
    const std::string& url,
    const std::string& token,
    const std::string& firmwareVersion,
    int batteryPercent,
    int wifiRssi,
    const std::string& ifNoneMatchEtag = "",
    const std::string& powerSource = "battery",
    bool isCharging = false
  );

private:
  int m_timeoutMs;
};
