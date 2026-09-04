# RFC & Technical Proposal: Standardized WebSerial Post-Flash Configuration Protocol for Seeed Studio Playground

**Author:** René Bohne  
**Date:** September 4, 2026  
**Status:** Proposed Standard  
**Target Repositories:** 
- `Seeed-Projects/reterminal-sticky-playground-registry`
- `seeedstudio.com/sticky/playground`

---

## 1. Executive Summary & Problem Statement

The **Seeed Studio reTerminal Sticky Playground** (`seeedstudio.com/sticky/playground`) currently provides a smooth 1-click web flashing experience using WebSerial / ESP Web Tools in Chrome and Edge.

However, the user onboarding journey abruptly stops immediately after flashing:
1. **The Post-Flash Gap:** Once the firmware is flashed (100%), the device boots unconfigured.
2. **Current Workarounds & Their Flaws:**
   - **Wi-Fi SoftAP / Captive Portal:** Users have to disconnect from their home Wi-Fi on their phone/laptop, find a temporary hotspot, navigate captive portal popups (which frequently fail on modern OSes), and type credentials on a tiny mobile keyboard.
   - **Hardcoded Credentials:** Requires users to install PlatformIO/ESP-IDF locally to rebuild binaries.
   - **External Terminal Tools:** Requires serial monitor tools like PuTTY, screen, or command-line scripts.

### The Proposed Solution
Extend the Seeed Playground Web Flasher to include a **native, standardized Post-Flash Configuration step in the same browser tab**:
1. **Flash (Step 1):** User selects firmware and clicks **"Flash Firmware"** (existing functionality).
2. **Configure (Step 2):** As soon as flashing finishes, the Playground dynamically renders form fields (Wi-Fi SSID, Password, Server URL, API Keys) based on a declarative `config` schema defined in `firmware.json`.
3. **Save & Connect (Step 3):** User clicks **"Save & Connect"** ➔ The browser sends a clean JSON payload over the already-open WebSerial connection ➔ The device saves parameters to NVS, connects to Wi-Fi, and reports a live `{"status":"ok", "ip":"..."}` confirmation back to the Playground.

---

## 2. Architectural Overview

```text
┌───────────────────────────────────────────────────────────────────────────────┐
│                        SEEED STUDIO PLAYGROUND WEB UI                         │
│                                                                               │
│  1. Read firmware.json Schema ──▶ 2. Flash Binary (ESP Web Tools)             │
│                                           │                                   │
│  3. Render Dynamic Config Form ◀──────────┘ (Flash 100% Complete)             │
│     [ Wi-Fi SSID : ____________ ]                                             │
│     [ Password   : •••••••••••• ]                                             │
│     [ Server URL : ____________ ]                                             │
│     [ 💾 Save & Connect ]                                                     │
│            │                                                                  │
│            ▼ JSON Line via WebSerial (115200 Baud)                            │
│  {"cmd":"config","ssid":"HomeWiFi","pass":"secret","server":"https://..."}\n │
└──────────────────────────────────────┬────────────────────────────────────────┘
                                       │ WebSerial (USB CDC / UART)
┌──────────────────────────────────────▼────────────────────────────────────────┐
│               reTerminal Sticky (ESP32-S3 Firmware Client)                    │
│                                                                               │
│  1. Parse JSON line in loop()                                                 │
│  2. Save key-values to NVS (Flash Preferences)                                │
│  3. Connect to Wi-Fi & Initialize Application                                 │
│  4. Send Response: {"status":"ok","ip":"192.168.1.85","connected":true}\n    │
└───────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Specification Part 1: Schema in `firmware.json`

To make this generic across all current and future community/partner firmwares (e.g. ScreenTinker, Weather Station, ChatGPT Companion, TRMNL, Crypto Ticker), each firmware declares its required parameters in `firmware.json`.

### Schema Extension for `firmware.json`

Add an optional `config` object to the root of `firmware.json`:

```json
{
  "schemaVersion": 1,
  "id": "screentinker",
  "name": "ScreenTinker",
  "group": "community",
  "catalogSection": "community",
  "category": "productivity",
  "mode": "flash",
  "status": "stable",
  
  "config": {
    "supported": true,
    "protocol": "json-line",
    "baudRate": 115200,
    "fields": [
      {
        "key": "ssid",
        "label": "Wi-Fi Network (SSID)",
        "type": "text",
        "required": true,
        "placeholder": "MyHomeNetwork",
        "help": "Your 2.4 GHz Wi-Fi network name"
      },
      {
        "key": "pass",
        "label": "Wi-Fi Password",
        "type": "password",
        "required": false,
        "placeholder": "••••••••",
        "help": "Leave blank if your network is open"
      },
      {
        "key": "server",
        "label": "ScreenTinker Server URL",
        "type": "url",
        "required": true,
        "default": "http://192.168.1.100:3001",
        "placeholder": "http://192.168.1.100:3001",
        "help": "IP or Domain of your ScreenTinker instance"
      }
    ]
  }
}
```

### Supported Field Types (`type`)

| Type | Rendered HTML Control | Description | Example |
| :--- | :--- | :--- | :--- |
| `text` | `<input type="text">` | Single-line text input | Wi-Fi SSID, Device Name, Location |
| `password` | `<input type="password">` | Obscured text input | Wi-Fi Password, OpenAI Key, API Tokens |
| `url` | `<input type="url">` | Validated HTTP/HTTPS URL | Server Endpoint, Home Assistant URL |
| `number` | `<input type="number">` | Numeric input with `min`/`max` | Refresh Interval (seconds), Timezone Offset |
| `boolean` | `<input type="checkbox">` | Toggle switch / checkbox | Enable Sleep Mode, Invert Colors |
| `select` | `<select>` | Dropdown selection | Language (`["en", "de"]`), Unit (`["C", "F"]`) |

---

## 4. Specification Part 2: WebSerial Communication Protocol

### Transport Layer
* **Baud Rate:** `115200` (8 data bits, no parity, 1 stop bit, no hardware flow control).
* **Framing:** Plain ASCII/UTF-8 JSON string terminated by a newline (`\n` or `\r\n`).
* **Direction:** Bi-directional asynchronous messaging.

---

### Command 1: Save & Apply Configuration (`cmd: "config"`)

Sent by the Playground browser interface to provision the device.

**Request (Browser ➔ ESP32):**
```json
{
  "cmd": "config",
  "ssid": "MyHomeWiFi",
  "pass": "SuperSecretPass123",
  "server": "https://signage.example.com",
  "api_key": "sk-1234567890"
}
```

**Success Response (ESP32 ➔ Browser):**
```json
{
  "status": "ok",
  "message": "Configuration saved to NVS and applied.",
  "ip": "192.168.1.85",
  "connected": true
}
```

**Error Response (ESP32 ➔ Browser):**
```json
{
  "status": "error",
  "message": "Wi-Fi connection failed: Invalid password or SSID not in range."
}
```

---

### Command 2: Query Device Status & Telemetry (`cmd: "status"`)

Can be sent by the Playground to verify device health, Wi-Fi status, or firmware version.

**Request (Browser ➔ ESP32):**
```json
{"cmd": "status"}
```

**Response (ESP32 ➔ Browser):**
```json
{
  "status": "ok",
  "version": "1.1.0",
  "device_id": "sticky-epaper-01",
  "connected": true,
  "ip": "192.168.1.85",
  "rssi": -54,
  "psram_free": 8228592,
  "battery_mv": 4120
}
```

---

### Command 3: Factory Reset / Wipe NVS (`cmd: "reset"`)

Sent to clear stored configuration and revert to clean onboarding.

**Request (Browser ➔ ESP32):**
```json
{"cmd": "reset"}
```

**Response (ESP32 ➔ Browser):**
```json
{
  "status": "ok",
  "message": "NVS configuration wiped. Restarting device."
}
```

---

## 5. Playground Web Frontend Implementation Guide (For Seeed Developers)

Here is a concrete, drop-in implementation for the Seeed Studio Playground frontend in TypeScript/JavaScript:

### 1. Dynamic Form Generator Component

```typescript
interface ConfigField {
  key: string;
  label: string;
  type: 'text' | 'password' | 'url' | 'number' | 'boolean' | 'select';
  required?: boolean;
  default?: any;
  placeholder?: string;
  help?: string;
  options?: Array<{ label: string; value: string }>;
}

interface FirmwareConfigSchema {
  supported: boolean;
  baudRate?: number;
  fields: ConfigField[];
}

export function renderConfigForm(schema: FirmwareConfigSchema): string {
  const fieldsHtml = schema.fields.map(field => {
    let inputHtml = '';
    
    if (field.type === 'select') {
      const optionsHtml = (field.options || [])
        .map(opt => `<option value="${opt.value}" ${opt.value === field.default ? 'selected' : ''}>${opt.label}</option>`)
        .join('');
      inputHtml = `<select name="${field.key}" class="seeed-input">${optionsHtml}</select>`;
    } else if (field.type === 'boolean') {
      inputHtml = `<input type="checkbox" name="${field.key}" ${field.default ? 'checked' : ''} class="seeed-checkbox" />`;
    } else {
      inputHtml = `
        <input 
          type="${field.type === 'password' ? 'password' : (field.type === 'number' ? 'number' : 'text')}" 
          name="${field.key}" 
          value="${field.default || ''}" 
          placeholder="${field.placeholder || ''}" 
          ${field.required ? 'required' : ''} 
          class="seeed-input"
        />`;
    }

    return `
      <div class="seeed-form-group" style="margin-bottom: 14px;">
        <label style="display:block; font-weight:600; font-size:13px; margin-bottom:4px;">
          ${field.label} ${field.required ? '<span style="color:#ef4444">*</span>' : ''}
        </label>
        ${inputHtml}
        ${field.help ? `<div style="color:#64748b; font-size:11px; margin-top:3px;">${field.help}</div>` : ''}
      </div>
    `;
  }).join('');

  return `
    <div class="seeed-config-card" style="padding:20px; background:#1e293b; border-radius:10px; color:#fff;">
      <h3 style="margin-top:0; font-size:18px;">⚙️ Device Configuration</h3>
      <p style="color:#94a3b8; font-size:13px; margin-bottom:16px;">
        Flashing completed! Enter your credentials below to configure the device immediately over USB.
      </p>
      <form id="seeedSerialConfigForm">
        ${fieldsHtml}
        <button type="submit" class="seeed-btn-primary" style="margin-top:10px; width:100%; padding:12px; background:#0284c7; color:#fff; border:none; border-radius:6px; font-weight:600; cursor:pointer;">
          💾 Save Configuration & Connect
        </button>
      </form>
      <div id="seeedConfigStatus" style="display:none; margin-top:14px; padding:10px; border-radius:6px; font-size:13px;"></div>
    </div>
  `;
}
```

---

### 2. WebSerial Send & Handshake Logic

```typescript
export async function sendConfigOverSerial(
  port: SerialPort, 
  configValues: Record<string, any>, 
  baudRate: number = 115200,
  timeoutMs: number = 15000
): Promise<{ success: boolean; data?: any; error?: string }> {
  
  // Ensure port is open at target baud rate
  if (!port.readable || !port.writable) {
    await port.open({ baudRate });
  }

  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  // 1. Send JSON command line
  const payload = JSON.stringify({ cmd: 'config', ...configValues }) + '\n';
  const writer = port.writable.getWriter();
  await writer.write(encoder.encode(payload));
  writer.releaseLock();

  // 2. Read response line with timeout
  const reader = port.readable.getReader();
  let buffer = '';
  const startTime = Date.now();

  try {
    while (Date.now() - startTime < timeoutMs) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value) {
        buffer += decoder.decode(value, { stream: true });
        if (buffer.includes('\n')) {
          const lines = buffer.split('\n');
          for (const line of lines) {
            const trimmed = line.trim();
            if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
              try {
                const response = JSON.parse(trimmed);
                if (response.status === 'ok') {
                  return { success: true, data: response };
                } else if (response.status === 'error') {
                  return { success: false, error: response.message || 'Device returned error' };
                }
              } catch (_) {
                // Ignore non-JSON log lines
              }
            }
          }
        }
      }
    }
    return { success: false, error: 'Configuration timeout: No response received from device.' };
  } finally {
    reader.releaseLock();
  }
}
```

---

## 6. Firmware Reference Implementation (ESP32 Arduino / ESP-IDF)

Here is the lightweight C++ reference handler for firmware developers:

```cpp
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

static Preferences s_prefs;

void processSerialLine(const String& line) {
  if (!line.startsWith("{")) return; // Ignore non-JSON debug logs

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) return;

  const char* cmd = doc["cmd"];
  if (!cmd) return;

  if (strcmp(cmd, "config") == 0) {
    s_prefs.begin("app_config", false);
    
    // Save any string keys passed in the payload
    for (JsonPair kv : doc.as<JsonObject>()) {
      if (strcmp(kv.key().c_str(), "cmd") != 0) {
        s_prefs.putString(kv.key().c_str(), kv.value().as<String>());
      }
    }
    s_prefs.end();

    // Trigger immediate Wi-Fi connection
    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";
    WiFi.begin(ssid.c_str(), pass.c_str());

    // Send success confirmation
    Serial.println("{\"status\":\"ok\",\"message\":\"Config saved. Connecting...\"}");
  } 
  else if (strcmp(cmd, "status") == 0) {
    Serial.printf("{\"status\":\"ok\",\"connected\":%s,\"ip\":\"%s\"}\n",
                  WiFi.status() == WL_CONNECTED ? "true" : "false",
                  WiFi.localIP().toString().c_str());
  }
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

## 7. Key Benefits for Seeed Studio & The Community

1. **Flawless End-to-End User Experience:** The user never leaves `seeedstudio.com/sticky/playground`. They select a firmware, click flash, fill out Wi-Fi/API keys, and their device is running live in under 45 seconds.
2. **Eliminates Captive Portal & Wi-Fi Switching Pain:** Mobile OS captive portal detection is notoriously flaky (SSL warnings, automatic dropouts). WebSerial is 100% reliable hardware-level communication.
3. **Zero Breaking Changes:** Fully backwards-compatible. If a firmware does not declare `config` in `firmware.json`, the Playground flasher behaves exactly as it does today.
4. **First-Mover Advantage:** Seeed Studio Playground would become the **first web flasher platform in the world** with declarative post-flash configuration.

---

## 8. Reference Working Firmware

This protocol is actively implemented, tested, and validated on physical hardware in the ScreenTinker reTerminal Sticky client:
* **Firmware Repository:** [https://github.com/renebohne/screentinker_mcu](https://github.com/renebohne/screentinker_mcu)
* **Standalone Web Flasher & Configurator Demo:** [https://github.com/renebohne/screentinker_mcu/blob/main/tools/sticky-installer.html](https://github.com/renebohne/screentinker_mcu/blob/main/tools/sticky-installer.html)
