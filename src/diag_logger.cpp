#include "diag_logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
static auto s_simStartTime = std::chrono::steady_clock::now();
static uint32_t host_millis() {
  auto now = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - s_simStartTime).count();
}
#endif

static DiagLogEntry s_entries[DIAG_LOG_MAX_ENTRIES];
static size_t s_head = 0;
static size_t s_count = 0;

static const char* levelToString(DiagLogLevel level) {
  switch (level) {
    case DIAG_LOG_DEBUG: return "DEBUG";
    case DIAG_LOG_INFO:  return "INFO";
    case DIAG_LOG_WARN:  return "WARN";
    case DIAG_LOG_ERROR: return "ERROR";
    default:             return "UNKNOWN";
  }
}

void diag_log_init(void) {
  s_head = 0;
  s_count = 0;
#ifndef ARDUINO
  s_simStartTime = std::chrono::steady_clock::now();
#endif
}

size_t diag_log_count(void) {
  return s_count;
}

void diag_log_clear(void) {
  s_head = 0;
  s_count = 0;
}

bool diag_log_get(size_t index, DiagLogEntry* out) {
  if (index >= s_count || !out) return false;
  size_t slot = (s_head + index) % DIAG_LOG_MAX_ENTRIES;
  *out = s_entries[slot];
  return true;
}

void diag_log_write(DiagLogLevel level, const char* tag, const char* msg) {
  if (!msg) return;

  uint32_t nowMs = 0;
#ifdef ARDUINO
  nowMs = millis();
#else
  nowMs = host_millis();
#endif

  size_t slot = 0;
  if (s_count < DIAG_LOG_MAX_ENTRIES) {
    slot = (s_head + s_count) % DIAG_LOG_MAX_ENTRIES;
    s_count++;
  } else {
    slot = s_head;
    s_head = (s_head + 1) % DIAG_LOG_MAX_ENTRIES;
  }

  s_entries[slot].uptime_ms = nowMs;
  s_entries[slot].level = (uint8_t)level;

  if (tag) {
    strncpy(s_entries[slot].tag, tag, DIAG_LOG_MAX_TAG_LEN - 1);
    s_entries[slot].tag[DIAG_LOG_MAX_TAG_LEN - 1] = '\0';
  } else {
    s_entries[slot].tag[0] = '\0';
  }

  strncpy(s_entries[slot].msg, msg, DIAG_LOG_MAX_MSG_LEN - 1);
  s_entries[slot].msg[DIAG_LOG_MAX_MSG_LEN - 1] = '\0';
}

void diag_log_printf(DiagLogLevel level, const char* tag, const char* fmt, ...) {
  if (!fmt) return;
  char buf[DIAG_LOG_MAX_MSG_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  diag_log_write(level, tag, buf);

#ifdef ARDUINO
  Serial.printf("[%s] [%s] %s\n", levelToString(level), tag ? tag : "SYS", buf);
#endif
}

void diag_log_dump_stdout(void) {
  printf("=== DIAGNOSTIC LOG DUMP (%zu entries) ===\n", s_count);
  for (size_t i = 0; i < s_count; i++) {
    DiagLogEntry e;
    if (diag_log_get(i, &e)) {
      printf("[+%6.2fs] [%s] [%s] %s\n",
             (double)e.uptime_ms / 1000.0,
             levelToString((DiagLogLevel)e.level),
             e.tag[0] ? e.tag : "SYS",
             e.msg);
    }
  }
  printf("=== END OF LOG DUMP ===\n");
}

static void escapeJsonString(const char* src, std::string& out) {
  if (!src) return;
  for (const char* p = src; *p; ++p) {
    switch (*p) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((unsigned char)*p < 0x20) {
          char hex[8];
          snprintf(hex, sizeof(hex), "\\u%04x", (unsigned char)*p);
          out += hex;
        } else {
          out += *p;
        }
        break;
    }
  }
}

std::string diag_log_to_json_string(const char* device_id, const char* version, int battery_pct, int rssi_dbm) {
  std::string json;
  json.reserve(256 + s_count * 128);

  json += "{\"device_id\":\"";
  if (device_id) escapeJsonString(device_id, json);
  json += "\",\"version\":\"";
  if (version) escapeJsonString(version, json);
  json += "\",\"battery\":";
  json += std::to_string(battery_pct);
  json += ",\"rssi\":";
  json += std::to_string(rssi_dbm);
  json += ",\"log_count\":";
  json += std::to_string(s_count);
  json += ",\"logs\":[";

  for (size_t i = 0; i < s_count; i++) {
    DiagLogEntry e;
    if (diag_log_get(i, &e)) {
      if (i > 0) json += ",";
      json += "{\"uptime_ms\":";
      json += std::to_string(e.uptime_ms);
      json += ",\"level\":\"";
      json += levelToString((DiagLogLevel)e.level);
      json += "\",\"tag\":\"";
      escapeJsonString(e.tag, json);
      json += "\",\"msg\":\"";
      escapeJsonString(e.msg, json);
      json += "\"}";
    }
  }

  json += "]}";
  return json;
}

size_t diag_log_to_json(char* dst, size_t max_len) {
  if (!dst || max_len == 0) return 0;
  std::string s = diag_log_to_json_string(nullptr, nullptr, -1, 0);
  if (s.length() >= max_len) return 0;
  strncpy(dst, s.c_str(), max_len - 1);
  dst[max_len - 1] = '\0';
  return s.length();
}
