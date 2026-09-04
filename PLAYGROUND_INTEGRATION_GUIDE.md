# WSCP — Playground Integration Guide

**For:** Seeed Studio reTerminal Sticky Playground  
**Protocol:** WSCP v1.0 (WebSerial Config Protocol)  
**Hardware:** Seeed Studio reTerminal Sticky (ESP32-S3 + SSD1677 E-Paper)  
**Reference Firmware:** [ScreenTinker](https://github.com/renebohne/screentinker_mcu)  
**Reference Demo:** [sticky-installer.html](https://github.com/renebohne/screentinker_mcu/blob/main/tools/sticky-installer.html)

---

## 1. How ScreenTinker Implements the Protocol

### 1.1 ScreenTinker's `firmware.json`

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
    "fields": [
      {
        "key": "ssid",
        "label": "WLAN Name (SSID)",
        "type": "text",
        "required": true,
        "minLength": 1,
        "maxLength": 32,
        "placeholder": "MeinHeimnetz",
        "help": "2.4 GHz Wi-Fi Netzwerkname"
      },
      {
        "key": "pass",
        "label": "WLAN Passwort",
        "type": "password",
        "required": false,
        "minLength": 8,
        "maxLength": 63,
        "placeholder": "••••••••",
        "help": " Leer lassen für offene Netze"
      },
      {
        "key": "server",
        "label": "ScreenTinker Server URL",
        "type": "url",
        "required": true,
        "default": "http://192.168.1.100:3001",
        "placeholder": "http://192.168.1.100:3001",
        "help": "IP-Adresse oder Domain deiner ScreenTinker-Instanz"
      }
    ]
  }
}
```

### 1.2 NVS Key Mapping

The ScreenTinker firmware maps protocol field keys to NVS keys:

| Protocol Key | NVS Key | NVS Namespace |
|-------------|---------|---------------|
| `ssid` | `wifi_ssid` | `screentinker` |
| `pass` | `wifi_pass` | `screentinker` |
| `server` | `server_url` | `screentinker` |
| `device_id` | `device_id` | `screentinker` |
| `device_token` | `device_token` | `screentinker` |

The `device_id` and `device_token` fields are **not** in the config form — they are assigned by the server during the 6-digit pairing flow. The firmware accepts them via the `config` command but they are not user-facing.

### 1.3 The 6-Digit Pairing Flow

After the user saves Wi-Fi configuration, ScreenTinker follows this sequence:

1. **Save config** → NVS write, Wi-Fi connection begins
2. **Register with server** → HTTP POST to `<server>/api/embedded/pair/register`
3. **Server returns** → `device_id`, `pairing_code` (6 digits), `claim_secret`
4. **Display pairing code** → Rendered on E-Paper display
5. **User enters code** → In the ScreenTinker web dashboard, click "Add Screen", enter 6 digits
6. **Server confirms** → HTTP GET to `<server>/api/embedded/pair/status`
7. **Firmware saves** → `device_id` and `device_token` written to NVS
8. **Content sync** → Firmware fetches and renders content from server

This pairing flow is ScreenTinker-specific and is **outside** the scope of the configuration protocol. Other firmware may use different provisioning mechanisms or none at all.

---

## 2. Playground Web Flasher Integration

### 2.1 Flash → Configure Lifecycle

The Playground should implement this lifecycle:

```
┌─────────────────────────────────────────────────────┐
│  1. User selects firmware in Playground UI          │
│  2. User clicks "Flash Firmware"                    │
│  3. ESP Web Tools flashes binary (0% → 100%)       │
│  4. Device reboots (2–3 seconds)                    │
│  5. Playground opens WebSerial connection           │
│  6. Playground renders config form from firmware.json│
│  7. User fills in fields and clicks "Save"          │
│  8. Playground sends {"cmd":"config", ...}          │
│  9. Device responds {"status":"ok", ...}            │
│ 10. Playground polls {"cmd":"status"} until connected│
│ 11. Playground shows "Connected! IP: x.x.x.x"      │
└─────────────────────────────────────────────────────┘
```

### 2.2 WebSerial Connection After Flashing

ESP Web Tools closes the serial port after flashing completes. The Playground must:

1. **Wait** 2–3 seconds after flash 100% for the device to reboot.
2. **Re-open** the serial port using `navigator.serial.requestPort()` or reuse the existing port reference.
3. **Open** at 115200 baud.
4. **Send** a `status` query to verify the device is alive.
5. **Render** the config form.

```typescript
// After flash completes
await new Promise(resolve => setTimeout(resolve, 3000)); // Wait for reboot

// Re-open the port (ESP Web Tools may have closed it)
if (!port.readable || !port.writable) {
  await port.open({ baudRate: 115200 });
}

// Verify device is alive
const status = await sendStatusQuery(port);
if (status?.status === 'ok') {
  // Render config form from firmware.json schema
  renderConfigForm(schema);
}
```

### 2.3 Reading `firmware.json` from the Registry

The Playground should read the `config` schema from the firmware's `firmware.json` in the registry repository:

```
Seeed-Projects/reterminal-sticky-playground-registry/
  └── firmware/
      ├── screentinker/
      │   ├── firmware.json    ← contains "config" schema
      │   └── firmware.bin
      ├── weather-station/
      │   ├── firmware.json
      │   └── firmware.bin
      └── ...
```

Parse the `config` field from `firmware.json` and pass it to the form renderer. If `config` is absent, skip the configuration step entirely.

### 2.4 Dynamic Form Rendering

Use the `renderConfigForm()` function from the WSCP specification (Section 8.2) to generate the configuration UI. Adapt the styling to match the Playground's design system.

For the reTerminal Sticky Playground, the form should:
- Use the dark theme (background `#0f172a`, card `#1e293b`)
- Show field labels in the user's language (German/English based on browser locale)
- Highlight required fields with a red asterisk
- Show help text below each field
- Disable the "Save" button while waiting for device response

### 2.5 WebSerial Send & Poll Pattern

```typescript
async function configureDevice(port: SerialPort, schema: FirmwareConfigSchema) {
  // 1. Collect form values
  const values = collectFormValues(schema);

  // 2. Send config command
  const result = await sendConfigOverSerial(port, values);

  if (!result.success) {
    showConfigError(result.error);
    return;
  }

  showConfigStatus('Configuration saved. Connecting to Wi-Fi...');

  // 3. Poll status until connected (max 20 attempts × 2s = 40s)
  for (let i = 0; i < 20; i++) {
    await new Promise(r => setTimeout(r, 2000));
    const status = await sendStatusQuery(port);

    if (status?.connected && status.ip && status.ip !== '0.0.0.0') {
      showConfigSuccess(`Connected! IP: ${status.ip} (RSSI: ${status.rssi} dBm)`);
      return;
    }
  }

  showConfigWarning('Device could not connect to Wi-Fi. Check credentials and try again.');
}
```

---

## 3. Firmware Developer Guide

### 3.1 Adding Config Support to a New Firmware

To make your firmware compatible with the Playground configuration protocol:

**Step 1: Declare fields in `firmware.json`**

Add a `config` object with your required fields (see Protocol Spec Section 5).

**Step 2: Implement the three standard commands**

Your firmware's serial command handler must process:

| Command | Your action |
|---------|-------------|
| `config` | Parse JSON, validate required fields, save to NVS, respond `{"status":"ok","v":1,...}`, begin async connection |
| `status` | Respond with `connected`, `ip`, `version`, and any diagnostics |
| `reset` | Clear NVS, respond `{"status":"ok","v":1,...}`, restart |

**Step 3: Implement the connection flow**

After receiving `config`:
1. Save parameters to NVS immediately.
2. Respond with `{"status":"ok","v":1,"message":"Configuration saved."}`.
3. Begin Wi-Fi connection asynchronously (do **not** block the serial response).
4. The browser will poll `status` to check if you connected.

### 3.2 NVS Namespace Convention

Use the namespace `"screentinker"` for compatibility with the ScreenTinker ecosystem, or define your own namespace. The Playground does not depend on a specific namespace — it only communicates via the serial protocol.

### 3.3 Testing with the Playground

1. Flash your firmware using the Playground.
2. Open Chrome DevTools → Console.
3. Connect the device via USB.
4. Open the Playground's configuration panel.
5. Fill in test values and click "Save".
6. Verify in the serial monitor that:
   - The device receives the JSON command.
   - The device saves to NVS.
   - The device responds with `{"status":"ok",...}`.
   - The device connects to Wi-Fi.
   - Subsequent `status` queries return `{"connected":true}`.

---

## 4. Known Limitations & Troubleshooting

### 4.1 Wi-Fi Connection Timeout

The firmware's `connectWiFi()` function uses a 20-second timeout. If the device cannot connect within this window:
- The `status` response will return `{"connected":false,"ip":"0.0.0.0"}`.
- The Playground should show an error and allow the user to retry.
- On the reTerminal Sticky, the device will show a "No Wi-Fi" screen on the E-Paper.

### 4.2 Captive Portal Fallback

If the device boots without configured Wi-Fi, ScreenTinker starts a SoftAP captive portal (`ScreenTinker-Setup`). This is a fallback for users who cannot use WebSerial. The Playground's WebSerial approach is preferred because:
- No need to switch Wi-Fi networks on the phone/laptop.
- No captive portal detection issues on modern OSes.
- Credentials are entered on a full keyboard, not a mobile popup.

### 4.3 Common Failure Modes

| Symptom | Cause | Solution |
|---------|-------|----------|
| Playground can't open serial port | Another app has the port open | Close serial monitor / PuTTY / screen |
| Config save succeeds but device doesn't connect | Wrong SSID or password | Verify credentials, retry via Status button |
| Device connects but shows "No Server" | Server URL is wrong or server is offline | Check server URL, ensure server is running |
| Pairing code not displayed | Server returned an error | Check server logs, verify network connectivity |
| Status shows `connected: false` permanently | Wi-Fi timeout (20s) | Check SSID/password, ensure 2.4 GHz network |
| E-Paper shows garbled text after config | SPI bus conflict | Ensure SD card pins are released after boot |

### 4.4 Serial Port Recovery

If the serial connection is lost during configuration:
1. The Playground should detect the port closing (`port.readable === null`).
2. Prompt the user to reconnect the USB cable.
3. Re-open the serial port.
4. Send a `status` query to verify the device is responsive.
5. Re-render the config form if needed.

---

## 5. Glossary

| Term | Definition |
|------|-----------|
| **Playground** | The Seeed Studio web flasher at `seeedstudio.com/sticky/playground` |
| **Sticky** | The Seeed Studio reTerminal Sticky hardware (ESP32-S3 + E-Paper) |
| **ScreenTinker** | The firmware project that implements this protocol for the Sticky |
| **WebSerial** | Browser API for serial port communication (Chrome, Edge, Opera) |
| **ESP Web Tools** | Espressif's library for flashing ESP32 devices via WebSerial |
| **NVS** | Non-Volatile Storage — ESP32's key-value flash storage |
| **Pairing flow** | ScreenTinker-specific process for linking a device to a server account |
