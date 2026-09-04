# WSCP — WebSerial Config Protocol — Specification v1.0

**Version:** 1.0  
**Date:** September 4, 2026  
**Status:** Proposed Standard

---

## 1. Overview

### 1.1 Problem

Web flashers (ESP Web Tools, WebSerial-based) provide a smooth firmware installation experience, but the user journey stops at 100% flash. The device boots unconfigured, forcing users into awkward workarounds: captive portals, external serial terminals, or rebuilding firmware with hardcoded credentials.

### 1.2 Solution

A standardized protocol that allows a web-based flasher to send configuration parameters to a device over the **same WebSerial connection used for flashing**, immediately after the flash completes. The protocol uses:

- A **declarative schema** in the firmware manifest (`firmware.json`) so the web interface knows what fields to render.
- A **JSON-over-serial** command/response protocol for saving configuration, querying device state, and performing factory resets.

### 1.3 Scope

This specification covers:
- The serial transport layer
- Three commands: `config`, `status`, `reset`
- The `firmware.json` schema extension for declaring configuration fields

This specification does **not** cover:
- Application-level commands (navigation, display control, power management)
- Authentication or encryption (physical USB access is assumed)
- Device provisioning or cloud registration flows

---

## 2. Transport Layer

| Parameter | Value |
|-----------|-------|
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Encoding | UTF-8 |

### 2.1 Framing

Every message is a single line of UTF-8 text terminated by a newline character (`\n` or `\r\n`).

- **Browser to device:** JSON command objects.
- **Device to browser:** JSON response objects.

Lines that do not contain valid JSON (e.g., debug log output) must be ignored by the browser. The device must not send non-JSON data on the same line as a JSON response.

---

## 3. Protocol Versioning

Every JSON command and response includes a `"v"` field indicating the protocol version.

```json
{"v": 1, "cmd": "status"}
```

```json
{"v": 1, "status": "ok", "connected": true}
```

- The current protocol version is **1**.
- A device must reject commands with an unsupported major version:
  ```json
  {"status": "error", "v": 1, "message": "Unsupported protocol version: 2"}
  ```
- A browser must not send commands with a version higher than the device reports in its `status` response.

---

## 4. Commands

### 4.1 `config` — Save Configuration

Saves configuration parameters to non-volatile storage (NVS). This command confirms that the data was written to storage, **not** that the device has connected to a network or applied the configuration. The browser must poll `status` to determine connection state.

#### Request

```json
{
  "v": 1,
  "cmd": "config",
  "<key>": "<value>",
  ...
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `v` | number | Yes | Protocol version (must be `1`) |
| `cmd` | string | Yes | Must be `"config"` |
| `<key>` | string | Varies | Configuration keys as defined by the firmware's `firmware.json` schema. Values are strings. |

The device must only accept keys that it recognizes. Unknown keys must be ignored (not saved).

#### Success Response

```json
{
  "status": "ok",
  "v": 1,
  "message": "Configuration saved."
}
```

The `status: "ok"` response confirms the configuration was written to NVS. It does **not** confirm network connectivity or successful application of the configuration.

#### Error Response

```json
{
  "status": "error",
  "v": 1,
  "message": "Missing required field: ssid"
}
```

#### Browser Workflow

After receiving `status: "ok"`, the browser should:
1. Wait briefly (500–1000 ms) for the device to begin connecting.
2. Poll `status` to check connection state.
3. Display connection result to the user.

---

### 4.2 `status` — Query Device State

Returns the current state of the device including network connectivity, firmware version, and resource information.

#### Request

```json
{"v": 1, "cmd": "status"}
```

#### Response

```json
{
  "status": "ok",
  "v": 1,
  "version": "1.0.0",
  "device_id": "device-abc-123",
  "connected": true,
  "ip": "192.168.1.42",
  "rssi": -54,
  "psram_free": 8228592
}
```

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Always `"ok"` for this command |
| `v` | number | Protocol version |
| `version` | string | Firmware version string |
| `device_id` | string | Device identifier (empty string if unprovisioned) |
| `connected` | boolean | `true` if the device is connected to a network |
| `ip` | string | IP address (empty string or `"0.0.0.0"` if not connected) |
| `rssi` | number | Wi-Fi signal strength in dBm (omitted if not connected) |
| `psram_free` | number | Free PSRAM in bytes (0 if no PSRAM) |

All fields are optional except `status`, `v`, `connected`, and `ip`. A device may include additional fields for diagnostics.

---

### 4.3 `reset` — Factory Reset

Wipes all stored configuration from NVS and restarts the device.

#### Request

```json
{"v": 1, "cmd": "reset"}
```

#### Success Response

```json
{
  "status": "ok",
  "v": 1,
  "message": "NVS wiped. Restarting device."
}
```

The device must send this response **before** restarting, so the browser can confirm the reset was initiated.

---

### 4.4 Error Responses

All commands follow the same error response format:

```json
{
  "status": "error",
  "v": 1,
  "message": "<human-readable error description>"
}
```

The device must respond with an error in the following cases:

| Condition | Example message |
|-----------|----------------|
| Unknown command | `"Unknown command: foo"` |
| Missing required field | `"Missing required field: ssid"` |
| Invalid field value | `"Invalid URL format for field: server"` |
| Unsupported protocol version | `"Unsupported protocol version: 2"` |

If the device cannot parse the incoming line as valid JSON, it must silently ignore the line (no response). This allows the serial line to carry non-protocol traffic (debug logs, boot messages) without confusing the browser.

---

## 5. Configuration Schema (`firmware.json` Extension)

Firmware manifests declare their required configuration parameters using an optional `config` object in `firmware.json`.

### 5.1 Schema Definition

```json
{
  "config": {
    "fields": [
      {
        "key": "string",
        "label": "string",
        "type": "string",
        "required": "boolean",
        "default": "any",
        "placeholder": "string",
        "help": "string",
        "minLength": "number",
        "maxLength": "number",
        "min": "number",
        "max": "number",
        "pattern": "string",
        "options": "array"
      }
    ]
  }
}
```

### 5.2 Field Properties

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `key` | string | Yes | The key name sent in the `config` command JSON payload |
| `label` | string | Yes | Human-readable label displayed next to the form field |
| `type` | string | Yes | One of the supported field types (see 5.3) |
| `required` | boolean | No | If `true`, the browser must not submit the form without a value. Default: `false` |
| `default` | any | No | Pre-filled value. Type must match the field type |
| `placeholder` | string | No | Placeholder text shown when the field is empty |
| `help` | string | No | Help text displayed below the field |
| `minLength` | number | No | Minimum string length (for `text`, `password`, `url`) |
| `maxLength` | number | No | Maximum string length (for `text`, `password`, `url`) |
| `min` | number | No | Minimum value (for `number`) |
| `max` | number | No | Maximum value (for `number`) |
| `pattern` | string | No | Regular expression pattern for validation (for `text`, `password`, `url`) |
| `options` | array | No | Array of `{label, value}` objects (required for `select` type) |

### 5.3 Supported Field Types

| Type | HTML Control | Value Type | Description |
|------|-------------|------------|-------------|
| `text` | `<input type="text">` | string | Single-line text input |
| `password` | `<input type="password">` | string | Obscured text input |
| `url` | `<input type="url">` | string | Validated URL input |
| `number` | `<input type="number">` | number | Numeric input |
| `boolean` | `<input type="checkbox">` | boolean | Toggle / checkbox |
| `select` | `<select>` | string | Dropdown selection |

### 5.4 Select Options Format

For `type: "select"`, the `options` property must be an array of objects:

```json
{
  "key": "unit",
  "label": "Temperature Unit",
  "type": "select",
  "default": "C",
  "options": [
    {"label": "Celsius", "value": "C"},
    {"label": "Fahrenheit", "value": "F"}
  ]
}
```

### 5.5 Example: Weather Station Firmware

```json
{
  "schemaVersion": 1,
  "id": "weather-station",
  "name": "Weather Station",
  "group": "community",
  "category": "iot",
  "mode": "flash",
  "status": "stable",

  "config": {
    "fields": [
      {
        "key": "ssid",
        "label": "Wi-Fi Network (SSID)",
        "type": "text",
        "required": true,
        "minLength": 1,
        "maxLength": 32,
        "placeholder": "MyHomeNetwork",
        "help": "Your 2.4 GHz Wi-Fi network name"
      },
      {
        "key": "pass",
        "label": "Wi-Fi Password",
        "type": "password",
        "required": false,
        "minLength": 8,
        "maxLength": 63,
        "placeholder": "••••••••",
        "help": "Leave blank for open networks"
      },
      {
        "key": "server",
        "label": "Weather API Server",
        "type": "url",
        "required": true,
        "default": "https://api.openweathermap.org/data/2.5",
        "placeholder": "https://api.example.com/weather",
        "help": "Base URL of your weather data server"
      },
      {
        "key": "interval",
        "label": "Refresh Interval (seconds)",
        "type": "number",
        "required": false,
        "default": 300,
        "min": 60,
        "max": 3600,
        "help": "How often to fetch new data (60–3600)"
      },
      {
        "key": "unit",
        "label": "Temperature Unit",
        "type": "select",
        "required": false,
        "default": "C",
        "options": [
          {"label": "Celsius", "value": "C"},
          {"label": "Fahrenheit", "value": "F"}
        ]
      },
      {
        "key": "invert",
        "label": "Invert Display Colors",
        "type": "boolean",
        "required": false,
        "default": false,
        "help": "Use white text on black background"
      }
    ]
  }
}
```

### 5.6 Absent Schema

If `firmware.json` does not include a `config` object, the platform must not render a configuration UI after flashing. The flash-only workflow remains unchanged.

---

## 6. Security Considerations

- **Physical access required:** WebSerial requires a user gesture (button click) and physical USB connection. Remote exploitation is not possible.
- **Credential transmission:** Configuration values (Wi-Fi passwords, API keys) are transmitted as plaintext JSON over USB. This is acceptable for the USB-connected flash-and-configure use case.
- **Device-side logging:** Firmware must not log sensitive configuration values (passwords, API keys, tokens) to the serial port.
- **No replay protection:** The protocol does not include authentication or nonce-based replay protection. This is acceptable because the threat model assumes a trusted physical host.

---

## 7. Reference Implementation: Firmware (Arduino / ESP-IDF)

Minimal firmware handler implementing the three standard commands:

```cpp
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

static Preferences s_prefs;

// ── Configuration keys recognized by this firmware ──────────────────────────
// Extend this list for your firmware's specific needs.
static const char* KNOWN_KEYS[] = {"ssid", "pass", "server"};
static const int KNOWN_KEY_COUNT = 3;

void processSerialLine(const String& line) {
  if (line.length() == 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) return;  // Ignore non-JSON lines (debug logs, boot messages)

  int protoVersion = doc["v"] | 0;
  if (protoVersion == 0 || protoVersion > 1) {
    Serial.printf("{\"status\":\"error\",\"v\":1,\"message\":\"Unsupported protocol version: %d\"}\n", protoVersion);
    return;
  }

  const char* cmd = doc["cmd"];
  if (!cmd) return;

  // ── config ─────────────────────────────────────────────────────────────
  if (strcmp(cmd, "config") == 0) {
    // Validate required fields
    if (!doc.containsKey("ssid") || doc["ssid"].as<String>().length() == 0) {
      Serial.println("{\"status\":\"error\",\"v\":1,\"message\":\"Missing required field: ssid\"}");
      return;
    }

    // Save only known keys to NVS
    s_prefs.begin("app_config", false);
    for (int i = 0; i < KNOWN_KEY_COUNT; i++) {
      const char* key = KNOWN_KEYS[i];
      if (doc.containsKey(key)) {
        s_prefs.putString(key, doc[key].as<String>());
      }
    }
    s_prefs.end();

    Serial.println("{\"status\":\"ok\",\"v\":1,\"message\":\"Configuration saved.\"}");

    // Begin async connection — do NOT block on Wi-Fi here.
    // The browser should poll 'status' to check connection state.
    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";
    if (ssid.length() > 0) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    return;
  }

  // ── status ─────────────────────────────────────────────────────────────
  if (strcmp(cmd, "status") == 0) {
    Serial.printf(
      "{\"status\":\"ok\",\"v\":1,\"version\":\"%s\",\"device_id\":\"%s\","
      "\"connected\":%s,\"ip\":\"%s\",\"rssi\":%d,\"psram_free\":%u}\n",
      FIRMWARE_VERSION,
      "",  // device_id — populate from your provisioning logic
      WiFi.status() == WL_CONNECTED ? "true" : "false",
      WiFi.localIP().toString().c_str(),
      WiFi.RSSI(),
      psramFound() ? ESP.getFreePsram() : 0
    );
    return;
  }

  // ── reset ──────────────────────────────────────────────────────────────
  if (strcmp(cmd, "reset") == 0) {
    s_prefs.begin("app_config", false);
    s_prefs.clear();
    s_prefs.end();

    Serial.println("{\"status\":\"ok\",\"v\":1,\"message\":\"NVS wiped. Restarting device.\"}");
    delay(500);
    ESP.restart();
    return;
  }

  // ── Unknown command ────────────────────────────────────────────────────
  Serial.printf("{\"status\":\"error\",\"v\":1,\"message\":\"Unknown command: %s\"}\n", cmd);
}

void loop() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      processSerialLine(line);
    }
  }
}
```

---

## 8. Reference Implementation: Frontend (TypeScript / WebSerial)

### 8.1 Configuration Schema Types

```typescript
interface ConfigFieldOption {
  label: string;
  value: string;
}

interface ConfigField {
  key: string;
  label: string;
  type: 'text' | 'password' | 'url' | 'number' | 'boolean' | 'select';
  required?: boolean;
  default?: string | number | boolean;
  placeholder?: string;
  help?: string;
  minLength?: number;
  maxLength?: number;
  min?: number;
  max?: number;
  pattern?: string;
  options?: ConfigFieldOption[];
}

interface FirmwareConfigSchema {
  fields: ConfigField[];
}
```

### 8.2 Dynamic Form Renderer

```typescript
export function renderConfigForm(schema: FirmwareConfigSchema): string {
  const fieldsHtml = schema.fields.map(field => {
    let inputHtml = '';

    if (field.type === 'select') {
      const optionsHtml = (field.options || [])
        .map(opt => `<option value="${opt.value}" ${opt.value === field.default ? 'selected' : ''}>${opt.label}</option>`)
        .join('');
      inputHtml = `<select name="${field.key}">${optionsHtml}</select>`;
    } else if (field.type === 'boolean') {
      inputHtml = `<input type="checkbox" name="${field.key}" ${field.default ? 'checked' : ''} />`;
    } else {
      const inputType = field.type === 'password' ? 'password'
        : field.type === 'number' ? 'number'
        : field.type === 'url' ? 'url'
        : 'text';
      const attrs = [
        `type="${inputType}"`,
        `name="${field.key}"`,
        `value="${field.default ?? ''}"`,
        `placeholder="${field.placeholder ?? ''}"`,
        field.required ? 'required' : '',
        field.minLength != null ? `minlength="${field.minLength}"` : '',
        field.maxLength != null ? `maxlength="${field.maxLength}"` : '',
        field.min != null ? `min="${field.min}"` : '',
        field.max != null ? `max="${field.max}"` : '',
        field.pattern ? `pattern="${field.pattern}"` : '',
      ].filter(Boolean).join(' ');
      inputHtml = `<input ${attrs} />`;
    }

    return `
      <div class="config-field">
        <label>${field.label} ${field.required ? '<span class="required">*</span>' : ''}</label>
        ${inputHtml}
        ${field.help ? `<div class="help-text">${field.help}</div>` : ''}
      </div>
    `;
  }).join('');

  return `
    <form id="configForm">
      ${fieldsHtml}
      <button type="submit">Save Configuration</button>
    </form>
    <div id="configStatus"></div>
  `;
}
```

### 8.3 WebSerial Send & Receive

```typescript
export async function sendConfigOverSerial(
  port: SerialPort,
  configValues: Record<string, string | number | boolean>,
  timeoutMs: number = 10000
): Promise<{ success: boolean; data?: Record<string, any>; error?: string }> {

  if (!port.readable || !port.writable) {
    await port.open({ baudRate: 115200 });
  }

  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  // Send config command
  const payload = JSON.stringify({ v: 1, cmd: 'config', ...configValues }) + '\n';
  const writer = port.writable.getWriter();
  await writer.write(encoder.encode(payload));
  writer.releaseLock();

  // Read response with timeout
  const reader = port.readable.getReader();
  let buffer = '';
  const startTime = Date.now();

  try {
    while (Date.now() - startTime < timeoutMs) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value) {
        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop() ?? '';

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed.startsWith('{') || !trimmed.endsWith('}')) continue;
          try {
            const response = JSON.parse(trimmed);
            if (response.status === 'ok') {
              return { success: true, data: response };
            } else if (response.status === 'error') {
              return { success: false, error: response.message || 'Device returned error' };
            }
          } catch (_) {
            // Not JSON — skip
          }
        }
      }
    }
    return { success: false, error: 'Timeout: no response from device.' };
  } finally {
    reader.releaseLock();
  }
}
```

### 8.4 Status Polling

```typescript
export async function pollDeviceStatus(
  port: SerialPort,
  maxAttempts: number = 10,
  intervalMs: number = 1000
): Promise<{ connected: boolean; ip?: string }> {

  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    await new Promise(r => setTimeout(r, intervalMs));

    try {
      const writer = port.writable.getWriter();
      await writer.write(encoder.encode(JSON.stringify({ v: 1, cmd: 'status' }) + '\n'));
      writer.releaseLock();

      const reader = port.readable.getReader();
      const startTime = Date.now();
      let buffer = '';

      while (Date.now() - startTime < 3000) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          buffer += decoder.decode(value, { stream: true });
          const lines = buffer.split('\n');
          buffer = lines.pop() ?? '';

          for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed.startsWith('{')) continue;
            try {
              const data = JSON.parse(trimmed);
              if (data.status === 'ok' && 'connected' in data) {
                reader.releaseLock();
                return { connected: data.connected, ip: data.ip };
              }
            } catch (_) {}
          }
        }
      }
      reader.releaseLock();
    } catch (_) {
      // Port may have closed — retry
    }
  }

  return { connected: false };
}
```

---

## 9. Changelog

### v1.0 (2026-09-04)

- Initial specification
- Commands: `config`, `status`, `reset`
- Field types: `text`, `password`, `url`, `number`, `boolean`, `select`
- Protocol versioning with `"v"` field
