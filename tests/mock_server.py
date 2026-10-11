#!/usr/bin/env python3
"""
Lightweight Mock ScreenTinker Server for E2E Simulator & MCU Testing.
Serves mock endpoints matching ScreenTinker API v1.2.0:
  - GET  /api/embedded/render?device_id=...
  - POST /api/embedded/pair/register
  - GET  /api/embedded/pair/status?device_id=...
  - POST /test/scenario
  - GET  /test/telemetry
  - POST /test/reset
"""

import argparse
import json
import sys
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

# Standard 800x480 monochrome 1-bit frame (800 * 480 / 8 = 48,000 bytes)
EPD_FRAME_SIZE = 48000

def generate_mock_frame(pattern: str = "checkerboard", width: int = 800, height: int = 480, color_mode: str = "mono") -> bytes:
    """Generates deterministic test patterns for arbitrary resolutions and color modes."""
    if color_mode == "spectra6":
        # 4 bits per pixel (2 pixels per byte):
        # 0: Black, 1: White, 2: Yellow, 3: Red, 4: Blue, 5: Green
        frame_size = (width * height) // 2
        data = bytearray(frame_size)
        if pattern == "spectra_stripes":
            # 4 horizontal stripes: Yellow, Red, Blue, Green
            stripe_h = height // 4
            for y in range(height):
                if y < stripe_h:
                    color = 2  # Yellow
                elif y < stripe_h * 2:
                    color = 3  # Red
                elif y < stripe_h * 3:
                    color = 4  # Blue
                else:
                    color = 5  # Green
                byte_val = (color << 4) | color
                row_start = y * (width // 2)
                for x in range(width // 2):
                    data[row_start + x] = byte_val
        elif pattern == "solid_red":
            for i in range(frame_size):
                data[i] = 0x33
        else:
            for i in range(frame_size):
                data[i] = ((i % 6) << 4) | ((i + 1) % 6)
        return bytes(data)

    elif color_mode == "gray16":
        # 4 bits per pixel (2 pixels per byte), levels 0x0 (black) to 0xF (white)
        frame_size = (width * height) // 2
        data = bytearray(frame_size)
        if pattern == "gray16_ramp":
            stripe_h = height // 4
            for y in range(height):
                if y < stripe_h:
                    val = 0x0  # Black
                elif y < stripe_h * 2:
                    val = 0x5  # Dark Gray (~85)
                elif y < stripe_h * 3:
                    val = 0xA  # Light Gray (~170)
                else:
                    val = 0xF  # White (255)
                byte_val = (val << 4) | val
                row_start = y * (width // 2)
                for x in range(width // 2):
                    data[row_start + x] = byte_val
        else:
            for i in range(frame_size):
                data[i] = 0x88  # Mid-gray
        return bytes(data)

    elif color_mode == "gray4":
        frame_size = (width * height) // 4
        data = bytearray(frame_size)
        for i in range(frame_size):
            data[i] = 0x55
        return bytes(data)

    else:
        # 1-bit monochrome (default)
        frame_size = (width * height) // 8
        data = bytearray(frame_size)
        if pattern == "checkerboard":
            row_bytes = width // 8
            for y in range(height):
                byte_val = 0xAA if (y % 16 < 8) else 0x55
                offset = y * row_bytes
                for x in range(row_bytes):
                    data[offset + x] = byte_val
        elif pattern == "solid_white":
            for i in range(frame_size):
                data[i] = 0xFF
        elif pattern == "solid_black":
            for i in range(frame_size):
                data[i] = 0x00
        else:
            for i in range(frame_size):
                data[i] = (i & 0xFF)
        return bytes(data)


class MockServerState:
    def __init__(self):
        self.lock = threading.Lock()
        self.reset()

    def reset(self):
        with getattr(self, "lock", threading.Lock()):
            self.scenario = "normal"  # normal, empty_playlist, unsupported, reboot
            self.etag = "mock-etag-v1.2.0"
            self.expires_in = 60
            self.power_state = "on"
            self.device_name = "Mock Sticky 120"
            self.auto_claim = True
            self.claim_poll_count = 0
            self.pairing_code = "412309"
            self.claim_secret = "test-claim-secret-xyz"
            self.device_token = "mock-device-token-123"
            self.device_id = "test-device-uuid-456"
            self.screen_width = 800
            self.screen_height = 480
            self.color_mode = "mono"
            self.frame_pattern = "checkerboard"
            self.refresh_mode_header = None
            self.last_pair_register = {}
            self.requests_log = []
            self.last_telemetry = {}

    def log_request(self, method: str, path: str, headers: dict):
        with self.lock:
            entry = {
                "method": method,
                "path": path,
                "headers": dict(headers),
                "timestamp": time.time()
            }
            self.requests_log.append(entry)
            self.last_telemetry = entry


class MockRequestHandler(BaseHTTPRequestHandler):
    state: MockServerState = None

    def log_message(self, format, *args):
        # Silence default stderr logging during automated test runs
        if getattr(self.server, "verbose", False):
            sys.stderr.write(f"[MockServer] {self.address_string()} - {format % args}\n")

    def _send_json(self, status: int, data: dict, extra_headers: dict = None):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        if extra_headers:
            for k, v in extra_headers.items():
                self.send_header(k, str(v))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urlparse(self.path)
        path = url.path
        query = parse_qs(url.query)
        self.state.log_request("GET", self.path, self.headers)

        # ── Test Diagnostics API ──
        if path == "/test/telemetry":
            with self.state.lock:
                self._send_json(200, {
                    "last_telemetry": self.state.last_telemetry,
                    "requests_count": len(self.state.requests_log)
                })
            return

        # ── Pairing Status Check ──
        if path == "/api/embedded/pair/status":
            with self.state.lock:
                self.state.claim_poll_count += 1
                if self.state.auto_claim or self.state.claim_poll_count >= 1:
                    self._send_json(200, {
                        "paired": True,
                        "device_id": self.state.device_id,
                        "device_token": self.state.device_token
                    })
                else:
                    self._send_json(200, {"paired": False})
            return

        # ── Render Endpoint (/api/embedded/render) ──
        if path == "/api/embedded/render":
            with self.state.lock:
                scenario = self.state.scenario
                current_etag = self.state.etag
                dev_name = self.state.device_name
                exp_sec = self.state.expires_in
                p_state = self.state.power_state

            # Scenario: 404 Empty Playlist
            if scenario == "empty_playlist":
                self._send_json(404, {
                    "device_name": dev_name,
                    "message": "No playlist assigned or no active items for this device"
                }, extra_headers={"X-ST-Device-Name": dev_name})
                return

            # Scenario: 501 Unsupported Content
            if scenario == "unsupported":
                self._send_json(501, {
                    "error": "Not Implemented",
                    "detail": "Widget rendering requires browser (set CHROME_PATH)"
                })
                return

            # Conditional ETag revalidation (304 Not Modified)
            client_etag = self.headers.get("If-None-Match", "").strip('"')
            if client_etag and client_etag == current_etag:
                self.send_response(304)
                self.send_header("ETag", f'"{current_etag}"')
                self.send_header("X-ST-Expires-In", str(exp_sec))
                self.send_header("X-ST-Power-State", p_state)
                self.end_headers()
                return

            # 200 OK Normal Frame Delivery
            frame_bytes = generate_mock_frame(self.state.frame_pattern, self.state.screen_width, self.state.screen_height, self.state.color_mode)
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(frame_bytes)))
            self.send_header("ETag", f'"{current_etag}"')
            self.send_header("X-ST-Expires-In", str(exp_sec))
            self.send_header("X-ST-Power-State", p_state)
            if self.state.refresh_mode_header:
                self.send_header("X-ST-Refresh-Mode", self.state.refresh_mode_header)
            if scenario == "reboot":
                self.send_header("X-ST-Command", "reboot")
            self.end_headers()
            self.wfile.write(frame_bytes)
            return

        # Fallback 404
        self._send_json(404, {"error": "Not Found", "path": path})

    def do_POST(self):
        url = urlparse(self.path)
        path = url.path
        self.state.log_request("POST", self.path, self.headers)

        content_len = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_len) if content_len > 0 else b""

        # ── Test Control API ──
        if path == "/test/scenario":
            try:
                payload = json.loads(body.decode("utf-8"))
            except Exception:
                payload = {}
            with self.state.lock:
                if "scenario" in payload:
                    self.state.scenario = payload["scenario"]
                if "etag" in payload:
                    self.state.etag = payload["etag"]
                if "auto_claim" in payload:
                    self.state.auto_claim = bool(payload["auto_claim"])
                if "device_name" in payload:
                    self.state.device_name = payload["device_name"]
            self._send_json(200, {"status": "ok", "scenario": self.state.scenario})
            return

        if path == "/test/reset":
            self.state.reset()
            self._send_json(200, {"status": "reset"})
            return

        # ── 6-Digit Pairing Registration ──
        if path == "/api/embedded/pair/register":
            try:
                reg_payload = json.loads(body.decode("utf-8"))
            except Exception:
                reg_payload = {}
            with self.state.lock:
                self.state.last_pair_register = reg_payload
                if "screen_width" in reg_payload:
                    self.state.screen_width = int(reg_payload["screen_width"])
                if "screen_height" in reg_payload:
                    self.state.screen_height = int(reg_payload["screen_height"])
                self._send_json(200, {
                    "device_id": self.state.device_id,
                    "pairing_code": self.state.pairing_code,
                    "claim_secret": self.state.claim_secret
                })
            return

        self._send_json(404, {"error": "Endpoint not found"})


class MockServer:
    def __init__(self, host: str = "127.0.0.1", port: int = 0, verbose: bool = False):
        self.host = host
        self.requested_port = port
        self.state = MockServerState()
        self.server = None
        self.thread = None
        self.verbose = verbose

    def start(self):
        class BoundHandler(MockRequestHandler):
            state = self.state

        self.server = HTTPServer((self.host, self.requested_port), BoundHandler)
        self.server.verbose = self.verbose
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        return self.port

    def stop(self):
        if self.server:
            self.server.shutdown()
            self.server.server_close()
            self.thread.join(timeout=2.0)

    @property
    def url(self) -> str:
        return f"http://{self.host}:{self.port}"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ScreenTinker Mock Server")
    parser.add_argument("-p", "--port", type=int, default=3001, help="Port to bind (default: 3001)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    server = MockServer(port=args.port, verbose=args.verbose)
    actual_port = server.start()
    print(f"=======================================================")
    print(f" ScreenTinker Mock Server Running on http://127.0.0.1:{actual_port}")
    print(f" • Endpoints: /api/embedded/render, /api/embedded/pair/register")
    print(f" • Test Scenarios: POST /test/scenario (normal, empty_playlist, unsupported, reboot)")
    print(f"=======================================================")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping Mock Server...")
        server.stop()
