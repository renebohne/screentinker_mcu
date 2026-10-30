# Architecture & Design Proposal: LED Matrix Animations, Video Playback & `32×8` Rolling Tickers

**Status:** Draft / Evaluation Proposal  
**Date:** 2026-10-30  
**Scope:** `screentinker` (Server Embedded Renderer & Widgets) + `screentinker_mcu` (ESP32-C6 / ESP32-S3 / ESP32-C3 Firmware & SDL2 Desktop Simulator)

---

## 1. Executive Summary & Goals

Currently, both the E-Paper and Smart RGB LED (`seeed-xiao-led`) embedded pipelines render **single static frames**:
- Static images (`PNG`, `JPEG`, `WebP`, `BMP`) are decoded via `Jimp` into a single `width × height × 3` raw RGB888 buffer.
- Animated `GIF` files only have their first frame (`frame 0`) decoded by `Jimp`.
- Uploaded videos (`MP4`, `WebM`) fall back to their static upload thumbnail (`content.thumbnail_path`).
- HTML Widgets / Slides are captured via a single headless Chromium screenshot (`page.screenshot()`).

While E-Paper displays are physically constrained by 180 ms – 22,000 ms refresh times, **WS2812B / SK6812 RGB888 LED matrices can refresh in < 2 ms per frame (30–60+ FPS)**.

Our primary goals are:
1. **Rich Pixel-Art Animations & Video Loops on `64×64` Matrices:** Play animated GIFs, animated WebPs, and short MP4/WebM loops (e.g., arcade sprites, synthwave loops, weather animations) smoothly at 12–25 FPS.
2. **Rolling Text with Crisp Icons on `32×8` Matrices:** Deliver a LaMetric Time / Tidbyt / AWTRIX-style smart ticker where an `8×8` (or animated) icon sits on the left while crisp pixel-font text scrolls smoothly across the matrix at 25–60 FPS.

---

## 2. Hardware & Bandwidth Constraints

### 2.1 Payload Math (`24bit-rgb888`, 3 bytes per pixel)

| Matrix Size | Pixels | 1 Frame Size | 15 FPS Bandwidth | 30 FPS Bandwidth | 5-Sec Clip (15 FPS, 75 frames) | 10-Sec Clip (15 FPS, 150 frames) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`5×1` Strip** | 5 | **15 B** | 225 B/s | 450 B/s | **1.1 KB** | **2.2 KB** |
| **`16×16` Matrix** | 256 | **768 B** | 11.2 KB/s | 22.5 KB/s | **56 KB** | **112 KB** |
| **`32×8` Banner** | 256 | **768 B** | 11.2 KB/s | 22.5 KB/s | **56 KB** | **112 KB** |
| **`64×32` Matrix** | 2,048 | **6,144 B (6 KB)** | 90 KB/s | 180 KB/s | **450 KB** | **900 KB** |
| **`64×64` Matrix** | 4,096 | **12,288 B (12 KB)** | 180 KB/s | 360 KB/s | **900 KB** | **1.8 MB** |
| **`128×64` Matrix** | 8,192 | **24,576 B (24 KB)** | 360 KB/s | 720 KB/s | **1.8 MB** | **3.6 MB** |

### 2.2 MCU Memory Profiles (`screentinker_mcu`)

| Target Board | MCU | Internal SRAM | External PSRAM | Free Heap (Wi-Fi Active) | Flash (LittleFS Available) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`seeed_xiao_esp32s3_led`** | ESP32-S3 (Dual Xtensa) | 320 KB | **8 MB OPI PSRAM** | ~200 KB SRAM + **~7.5 MB PSRAM** | 8 MB |
| **`seeed_xiao_esp32c6_led`** | ESP32-C6 (RISC-V Wi-Fi 6) | 512 KB | None | **~180–220 KB SRAM** | 4 MB |
| **`seeed_xiao_esp32c3_led`** | ESP32-C3 (RISC-V) | 400 KB | None | **~150–180 KB SRAM** | 4 MB |

**Key Takeaway:**
- On **all three boards**, a `32×8` animation (56–112 KB) or a short `64×64` pixel-art GIF (8–10 frames = 96–120 KB) can fit entirely in RAM/PSRAM or Flash!
- For longer `64×64` video clips (e.g., 150 frames = 1.8 MB), **XIAO ESP32-S3** can hold the entire clip in PSRAM, whereas **XIAO ESP32-C6 / C3** must either **stream frames directly from the HTTP response** or buffer them via LittleFS flash.

---

## 3. Architectural Options for `64×64` Animations & Videos

### Option A: Packed Multi-Frame RGB888 Stream (`X-ST-Frame-Count` + `X-ST-Frame-Rate`)

#### How It Works
1. **Server (`screentinker`)**:
   - ScreenTinker already includes `ffmpeg` (`server/lib/media-tools.js`, `server/lib/a11y/media-worker.js`).
   - In `server/lib/embedded-render.js`, when rendering for a `24bit-rgb888` profile (`seeed-xiao-led`) and the content is an **animated GIF** (`image/gif`), **animated WebP** (`image/webp`), or **video** (`video/mp4`, `video/webm`, `video/quicktime`):
   - Instead of extracting a single still image via `Jimp` or `content.thumbnail_path`, spawn `ffmpeg` to decode and scale all frames up to a bounded duration (`maxDurationSec = Math.min(item.duration || 10, 15)`):
     ```bash
     ffmpeg -v error -nostdin -i <input_file> -an -sn -t <max_sec> \
       -vf "fps=15,scale=64:64:flags=neighbor:force_original_aspect_ratio=decrease,pad=64:64:(ow-iw)/2:(oh-ih)/2:color=#000000,format=rgb24" \
       -f rawvideo pipe:1
     ```
     - **Crisp Pixel Art (`flags=neighbor`):** Using nearest-neighbor scaling (`flags=neighbor`) ensures 8×8, 16×16, or 32×32 pixel-art GIFs scale up to `64×64` with razor-sharp pixel edges instead of bilinear blur. (Can fall back to `flags=lanczos` or `area` when downscaling high-res 1080p MP4 videos).
   - The output is simply `N` concatenated raw RGB888 frames: `[Frame 0 (W*H*3 bytes)][Frame 1 (W*H*3 bytes)]...[Frame N-1]`.
   - **Caching (`server/lib/embedded-cache.js`):** The multi-frame buffer + metadata (`{ frameCount, fps, frameBytes }`) is cached under the content ETag key so `ffmpeg` only runs **once** on the first poll.
   - **HTTP Response Headers (`GET /api/embedded/render`):**
     ```http
     HTTP/1.1 200 OK
     Content-Type: application/octet-stream
     Content-Length: 294912
     ETag: "a1b2c3..."
     X-ST-Color-Mode: 24bit-rgb888
     X-ST-Screen-Width: 64
     X-ST-Screen-Height: 64
     X-ST-Frame-Count: 24
     X-ST-Frame-Rate: 15
     X-ST-Frame-Bytes: 12288
     X-ST-Expires-In: 30
     ```
   - **Dashboard Preview (`?format=jpeg` or `?preview=1`):**
     - When the dashboard requests `?format=jpeg`, the server returns the first frame as `image/jpeg` (or `image/gif` if we add `?format=gif`).

2. **Firmware & Simulator (`screentinker_mcu`)**:
   - Inspect `X-ST-Frame-Count` (default `1`) and `X-ST-Frame-Rate` (default `15`).
   - If `X-ST-Frame-Count == 1`: Existing single-frame path.
   - If `X-ST-Frame-Count > 1`:
     - **Path 1 — Local Loop Cache (when `totalBytes <= maxAnimCacheBytes`):**
       - On **ESP32-S3 (PSRAM)**: `maxAnimCacheBytes = 4 * 1024 * 1024` (4 MB — holds ~340 frames of `64×64`).
       - On **ESP32-C6 / C3 (Internal RAM)**: `maxAnimCacheBytes = 80 * 1024` (80 KB — holds 100+ frames of `32×8` or 6 frames of `64×64`), plus optional **LittleFS file `/anim.bin`** up to `512 KB` (~42 frames of `64×64`).
       - The MCU downloads the entire payload once, stores `lastEtag`, and plays the frames in a non-blocking `millis()` state machine inside `loop()` at `1000 / fps` ms per frame until `X-ST-Expires-In` elapses.
       - If the next poll returns **`304 Not Modified`**, the MCU keeps looping the cached animation with **zero network traffic**!
     - **Path 2 — Live HTTP Stream (when `totalBytes > maxAnimCacheBytes`):**
       - The MCU keeps the `WiFiClient` stream open and reads `frameBytes` (`12,288` bytes) at a time directly into the existing single-frame `frameBuffer`, calling `led_driver_update(frameBuffer, frameBytes)` and pacing each frame by `1000 / fps` ms.
       - Requires **0 bytes of additional RAM** on ESP32-C6 / C3.

---

### Option B: Custom RLE / Delta-Compressed Animation Format (`x-led-anim-rle`)

#### How It Works
Pixel-art animations and scrolling text have massive frame-to-frame and run-length redundancy (large solid black backgrounds, only a few sprites moving):
- Encode frames using a lightweight header + **Run-Length Encoding (RLE)** or **XOR/Delta frames**:
  - Header (16 bytes): `Magic ("STLA") | Version (1) | Width (u16) | Height (u16) | FrameCount (u16) | FPS (u8) | Flags (u8)`
  - Each pixel run: `[repeat_count (1 byte: 1..255)][R][G][B]` (4 bytes per run).
- A typical `64×64` pixel-art or scrolling text frame compresses from **12,288 bytes down to 800 – 2,500 bytes (5× to 15× smaller)**.
- **Pros:** Even a 100-frame `64×64` pixel-art animation shrinks from `1.2 MB` to `~150 KB`, fitting in RAM/LittleFS on ESP32-C6 and ESP32-C3 without PSRAM.
- **Cons:** Full-screen photographic video with noise/gradients does not compress well with RLE (can expand by 1.33× unless fallback raw frames are supported per-frame); requires a small ~40-line RLE decoder in `screentinker_mcu` and encoder in `embedded-postprocess.js`.

> **Tip:** Option A and Option B can be combined! Each frame can have a 1-byte prefix (`0x00 = raw RGB888`, `0x01 = RLE`), so pixel art compresses 10× while photographic video stays raw.

---

### Option C: Continuous UDP / WebSocket Realtime Push (WLED / Art-Net / DDP / TPM2.net Style)

#### How It Works
- Instead of the MCU polling `GET /api/embedded/render`, the server (or an external controller) pushes raw RGB888 frames via UDP (`DDP` — Distributed Display Protocol, port `4048`, or `Art-Net`) at 30–60 FPS.
- **Pros:** Zero HTTP overhead; ultra-low latency for live audio visualizers or screen mirroring.
- **Cons:**
  - Breaks ScreenTinker's pull-based architecture (firewalls/NAT block inbound UDP from cloud servers to LAN devices).
  - High continuous Wi-Fi & server CPU usage (server must actively pace a 30 FPS timer per device 24/7).
  - Does not work with ETag caching or autonomous offline playback.

*(Verdict: UDP/DDP could be a fun optional local feature later, but HTTP pull with Option A/B fits ScreenTinker's architecture much better).*

---

## 4. Architectural Options for `32×8` Rolling Text + Icons

A `32×8` matrix (like the Ulanzi TC001, LaMetric Time, or BTF-Lighting 32×8 WS2812B panel) is only **8 pixels tall**. That creates two specific requirements:
1. **Standard anti-aliased TrueType fonts look blurry at 8px height.** Text must use a pixel-aligned bitmap font (e.g., `5×7` or `4×6` pixel font rendered without subpixel anti-aliasing).
2. **Only 5 characters fit on `32×8` at once.** Any meaningful message (`"23.5°C Sunny"`, `"New PR #758 merged"`, `"14:32 • Meeting in 5m"`) must scroll horizontally, ideally while keeping an `8×8` icon pinned on the left (`x = 0..7`) so the viewer always sees the category icon while reading the scrolling text (`x = 8..31`).

Here are the three ways we can support this:

---

### Option 1: Wide "Panoramic Strip" Render + MCU Hardware Scroller (`X-ST-LED-Scroll`)

```
Server renders one wide 8px-tall image (e.g. 168 × 8 pixels = 4,032 bytes):
┌────────┬──────────────────────────────────────────────────────────────────────┐
│  8×8   │  160×8 Wide Text Strip ("Hello ScreenTinker! 23.5°C")                │
│  Icon  │  ◄─── MCU slides a 24×8 window across this strip at 30-60 FPS        │
└────────┴──────────────────────────────────────────────────────────────────────┘
  Fixed                  Displayed on cols 8..31 of the 32×8 Matrix
cols 0..7
```

#### How It Works
1. **Server Side (`screentinker`)**:
   - The server renders a **single wide image** of height `8` (or `H`) and width `W_total` (up to `512` pixels wide, e.g., `200×8`).
   - The response includes a scroll instruction header:
     ```http
     X-ST-Screen-Width: 200
     X-ST-Screen-Height: 8
     X-ST-LED-Scroll: marquee;pin-left=8;fps=30;pause-ms=1000
     ```
     - `pin-left=8`: Columns `0..7` (the `8×8` icon) stay pinned on the left side of the physical `32×8` matrix.
     - `fps=30`: Scroll speed in pixels per second.
     - `pause-ms=1000`: Optional pause at the start of the text before scrolling begins.
2. **MCU Side (`screentinker_mcu`)**:
   - Even a **`512×8`** wide strip is only **`512 × 8 × 3 = 12,288 bytes` (12 KB)**—which fits inside the existing `16,384`-byte `frameBuffer` on **all** XIAO chips (ESP32-C3, C6, S3)!
   - In `loop()`, every `1000 / fps` ms (e.g., 33 ms for 30 FPS):
     - Copy the pinned left `8×8` region (`x = 0..7`) + the current `24×8` sliding window (`x = 8 + scrollOffset .. 31 + scrollOffset`) into the `32×8` display buffer and call `led_driver_update()`.
     - Advance `scrollOffset` by 1 pixel (wrapping around with a blank gap at the end).
3. **Why This Is Awesome for `32×8`:**
   - **Tiny payload:** 2–12 KB total download!
   - **60 FPS buttery-smooth scrolling** driven by the MCU hardware timer with zero network jitter.
   - **Works with static PNG uploads too:** Anyone can upload a wide `200×8` PNG banner into ScreenTinker, set `fit_mode` to `scroll` (or auto-detect when `image.width > matrix.width * 2` on an 8px-tall matrix), and it automatically scrolls!

---

### Option 2: Dedicated "LED Pixel Ticker / Marquee" Widget in ScreenTinker

To make creating `32×8` (and `64×64`) tickers effortless in the ScreenTinker Dashboard without external image editors:

#### How It Works
- Add a built-in **`led_ticker` (Pixel Matrix Ticker)** widget type in `server/routes/widgets.js`:
  - **Widget Settings in Dashboard:**
    - **Icon Picker:** Built-in library of crisp `8×8` and `16×16` pixel-art icons (Sun, Cloud, Rain, Fire, Heart, Check, Warning, Mail, GitHub, Clock, Arrow Up/Down, Smiley, etc.) — static or 2-frame animated!
    - **Text / Data Binding:** Static message or dynamic template variables (e.g., Time, Weather, RSS headline, Custom Text).
    - **Text Color & Background Color:** e.g., `#00FFCC` on `#000000` (black = LEDs off, saving power and maximizing contrast).
    - **Scroll Mode:** `auto` (scrolls if text exceeds width), `always`, or `static`.
- **Server Rendering (Zero-Browser Native Jimp / Bitmap Font):**
  - Instead of spinning up headless Chromium (which anti-aliases fonts), the server renders the `8×8` pixel icon + bitmap font (`5×7` / `6×8` pixel glyph table) **directly into a Jimp buffer in < 2 milliseconds**!
  - It can either output:
    - A **wide strip** (`Option 1`, `X-ST-LED-Scroll: marquee;pin-left=8`), OR
    - A **multi-frame RGB888 animation** (`Option A`, `X-ST-Frame-Count: N`) so even firmware without `X-ST-LED-Scroll` plays it as a standard animation!

---

### Option 3: Animated Icon + Scrolling Text via Multi-Frame Stream

What if the `8×8` icon on the left is **also animated** (e.g., a beating heart, flickering flame, or spinning sun) while the text scrolls on the right?
- Using **Option A (Multi-Frame RGB888 Stream)** at `32×8`:
  - Each `32×8` frame is only **768 bytes**.
  - A 120-frame loop (6 seconds at 20 FPS, enough to scroll a full sentence while animating the `8×8` icon) is **`120 × 768 = 92,160 bytes` (~90 KB)**.
  - That 90 KB payload works using the **exact same multi-frame player** as `64×64` videos/GIFs, with zero special-case scrolling logic needed on the MCU!

---

## 5. Side-by-Side Comparison

| Feature / Criterion | Option A: Multi-Frame RGB888 Stream (`ffmpeg`) | Option 1: Wide Strip + MCU Scroller (`X-ST-LED-Scroll`) | Option 2: Native Pixel Ticker Widget |
| :--- | :--- | :--- | :--- |
| **Best For** | `64×64` & `32×8` GIFs, MP4s, WebPs | Ultra-smooth 30–60 FPS `32×8` text banners | Turnkey `32×8` icon + text creation in UI |
| **Supports Uploaded GIFs / MP4s** | ✅ Yes (automatic via `ffmpeg`) | ❌ Static/wide images only | ❌ Widget only (unless paired with Option A) |
| **Supports Animated `8×8` Icons** | ✅ Yes | ⚠️ Static icon (unless icon has 2–4 frames) | ✅ Yes (when emitting multi-frame) |
| **Payload Size (`32×8` 10s)** | ~112 KB (at 15 FPS) | **~4 KB** (single wide strip) | ~4 KB (strip) or ~90 KB (multi-frame) |
| **Payload Size (`64×64` 5s)** | ~900 KB raw (~120 KB with RLE) | N/A | N/A |
| **Works on XIAO C6 / C3 (No PSRAM)** | ✅ Yes (RAM for `32×8`, HTTP stream/RLE for `64×64`) | ✅ Yes (fits in existing 16 KB buffer) | ✅ Yes |
| **Works on XIAO S3 (8 MB PSRAM)** | ✅ Yes (caches up to 4 MB in PSRAM) | ✅ Yes | ✅ Yes |
| **Requires Headless Chromium?** | ❌ No (uses `ffmpeg`) | ❌ No (uses `Jimp`) | ❌ No (uses native bitmap font + `Jimp`) |

---

## 6. Recommended Phased Implementation Roadmap

### Phase 1: Multi-Frame Animation Engine (`64×64` & `32×8` GIFs + MP4s)
1. **Server (`screentinker`)**:
   - In `server/lib/embedded-render.js`: Detect animated `GIF`, `WebP`, and `MP4`/`WebM` when rendering for `24bit-rgb888` (`seeed-xiao-led`).
   - Decode frames via `ffmpeg` (`-vf fps=15,scale=W:H:flags=neighbor...,format=rgb24 -f rawvideo`), cache in `embedded-cache.js`, and emit `X-ST-Frame-Count`, `X-ST-Frame-Rate`, and `X-ST-Frame-Bytes` headers.
2. **Firmware & Simulator (`screentinker_mcu`)**:
   - Update `fetchAndDisplay()` and `loop()` in `src/main.cpp` (and `simulator/main_sim.cpp`):
     - If `X-ST-Frame-Count > 1` and payload fits in PSRAM (ESP32-S3) or RAM/LittleFS (ESP32-C6/C3), buffer the animation and loop it non-blockingly in `loop()` at `X-ST-Frame-Rate` FPS across `304 Not Modified` intervals.
     - If payload exceeds local cache size, stream frames directly from the HTTP response at `X-ST-Frame-Rate` FPS.

### Phase 2: `32×8` Panoramic Marquee Scroller & Pixel Ticker
1. **MCU & Simulator (`screentinker_mcu`)**:
   - Support `X-ST-LED-Scroll: marquee;pin-left=<px>;fps=<fps>` when `X-ST-Screen-Width > matrix.width`: hold the wide strip in `frameBuffer` and pan the viewport across columns `pin_left .. matrix.width - 1` at 30–60 FPS in `loop()`.
2. **Server (`screentinker`)**:
   - Add a **Pixel Matrix Ticker** widget (or native `5×7` bitmap font + `8×8` pixel icon renderer) that outputs either a wide strip (`X-ST-LED-Scroll`) or a multi-frame animation (`X-ST-Frame-Count`).
