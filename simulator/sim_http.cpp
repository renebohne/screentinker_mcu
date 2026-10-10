#include "sim_http.h"
#include <curl/curl.h>
#include <algorithm>
#include <iostream>

static std::string toLower(const std::string& str) {
  std::string s = str;
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
}

std::string SimHttpResponse::getHeader(const std::string& name) const {
  std::string key = toLower(name);
  auto it = headers.find(key);
  if (it != headers.end()) return it->second;
  return "";
}

bool SimHttpResponse::hasHeader(const std::string& name) const {
  return headers.find(toLower(name)) != headers.end();
}

std::string SimHttpResponse::bodyAsString() const {
  if (body.empty()) return "";
  return std::string((const char*)body.data(), body.size());
}

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t total = size * nmemb;
  auto* vec = static_cast<std::vector<uint8_t>*>(userp);
  vec->insert(vec->end(), (uint8_t*)contents, (uint8_t*)contents + total);
  return total;
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
  size_t total = size * nitems;
  auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
  std::string line(buffer, total);

  size_t colon = line.find(':');
  if (colon != std::string::npos) {
    std::string key = line.substr(0, colon);
    std::string val = line.substr(colon + 1);

    // Trim whitespace and \r\n
    while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());
    while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' ')) val.pop_back();

    (*headers)[toLower(key)] = val;
  }
  return total;
}

SimHttp::SimHttp(int timeoutMs) : m_timeoutMs(timeoutMs) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

SimHttp::~SimHttp() {
  curl_global_cleanup();
}

SimHttpResponse SimHttp::get(
  const std::string& url,
  const std::string& token,
  const std::string& firmwareVersion,
  int batteryPercent,
  int wifiRssi,
  const std::string& ifNoneMatchEtag,
  const std::string& powerSource,
  bool isCharging
) {
  SimHttpResponse res;
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::cerr << "[HTTP] Error: Failed to initialize curl\n";
    return res;
  }

  struct curl_slist* headers = nullptr;
  if (!token.empty()) {
    std::string auth = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, auth.c_str());
  }

#ifndef PLATFORM_OS
#if defined(__APPLE__)
#define PLATFORM_OS "macOS"
#elif defined(__linux__)
#define PLATFORM_OS "Linux"
#else
#define PLATFORM_OS "Generic"
#endif
#endif

  std::string ua = "User-Agent: ScreenTinker-Sticky/1.2 (Simulator-" + std::string(PLATFORM_OS) + ")";
  headers = curl_slist_append(headers, ua.c_str());

  if (!firmwareVersion.empty()) {
    std::string fwv = "X-ST-Firmware-Version: " + firmwareVersion;
    headers = curl_slist_append(headers, fwv.c_str());
  }

  if (batteryPercent >= 0) {
    std::string bat = "X-ST-Device-Battery: " + std::to_string(batteryPercent);
    headers = curl_slist_append(headers, bat.c_str());
  }

  if (!powerSource.empty()) {
    std::string pwr = "X-ST-Power-Source: " + powerSource;
    headers = curl_slist_append(headers, pwr.c_str());
  }
  std::string chg = "X-ST-Charging: " + std::string(isCharging ? "1" : "0");
  headers = curl_slist_append(headers, chg.c_str());

  if (wifiRssi != 0) {
    std::string rssi = "X-ST-Device-RSSI: " + std::to_string(wifiRssi);
    headers = curl_slist_append(headers, rssi.c_str());
  }

  if (!ifNoneMatchEtag.empty()) {
    std::string etag = "If-None-Match: " + ifNoneMatchEtag;
    headers = curl_slist_append(headers, etag.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res.headers);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)m_timeoutMs);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode code = curl_easy_perform(curl);
  if (code != CURLE_OK) {
    std::cerr << "[HTTP] Request failed: " << curl_easy_strerror(code) << "\n";
    res.statusCode = 0;
  } else {
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    res.statusCode = (int)status;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return res;
}

SimHttpResponse SimHttp::postJson(
  const std::string& url,
  const std::string& jsonBody,
  const std::string& bearerToken
) {
  SimHttpResponse res;
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::cerr << "[HTTP] Error: Failed to initialize curl\n";
    return res;
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (!bearerToken.empty()) {
    std::string auth = "Authorization: Bearer " + bearerToken;
    headers = curl_slist_append(headers, auth.c_str());
  }

#ifndef PLATFORM_OS
#if defined(__APPLE__)
#define PLATFORM_OS "macOS"
#elif defined(__linux__)
#define PLATFORM_OS "Linux"
#else
#define PLATFORM_OS "Generic"
#endif
#endif

  std::string ua = "User-Agent: ScreenTinker-Sticky/1.2 (Simulator-" + std::string(PLATFORM_OS) + ")";
  headers = curl_slist_append(headers, ua.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res.headers);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)m_timeoutMs);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode code = curl_easy_perform(curl);
  if (code != CURLE_OK) {
    std::cerr << "[HTTP] POST failed: " << curl_easy_strerror(code) << "\n";
    res.statusCode = 0;
  } else {
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    res.statusCode = (int)status;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return res;
}
