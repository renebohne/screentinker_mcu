# Code Review — EPD Refresh & Touch Stack (2026-10-27)

**Scope:** Review of the three most recent commits on the `feat/touch-settings-navigation` branch stack.

| Commit | Branch | Subject |
|---|---|---|
| `ef8fc24` | `feat/smart-partial-refresh` | feat(epd): implement smart partial refresh with 6:1 ghosting mitigation policy |
| `d39ec67` | `feat/gray4-support` | feat(epd): implement 4-level grayscale (Gray4) mode for Sticky and E1001 |
| `571daa7` | `feat/touch-settings-navigation` | feat(touch): implement capacitive touch navigation for settings & status screens (Ticket 2) |

**Verdict:** Architecture and test coverage are good overall, but there is **one critical bug** (touch driver is never compiled on hardware) plus several issues that should be fixed before merging to `main`.

---

## 🔴 CRITICAL-1: Touch driver is never compiled on hardware

**File:** `src/touch_driver.cpp:46-50`
**Severity:** Critical — the entire touch feature is a silent no-op on real devices.

### Problem

```cpp
// src/touch_driver.cpp
#include "touch_driver.h"           // does NOT include device_profile.h
...
#if defined(ARDUINO) && DEVICE_SUPPORTS_TOUCH   // line 46: DEVICE_SUPPORTS_TOUCH is UNDEFINED here

#include <Arduino.h>
#include <Wire.h>
#include "device_profile.h"          // line 50: macro is only defined HERE, inside the #if
```

`DEVICE_SUPPORTS_TOUCH` is defined in `include/device_profile.h`, but that header is only included **inside** the `#if` block that tests the macro. `platformio.ini` only passes `-DDEVICE_RETERMINAL_*` via `build_flags` — never `-DDEVICE_SUPPORTS_TOUCH`. An undefined identifier in a preprocessor `#if` evaluates to `0`, so **the stub implementation (lines 308–329) is compiled for every target**, including Sticky and E1003.

On real hardware, `touch_init()` always returns `false`, the boot log shows `[Touch] Capacitive touch digitizer not detected.`, and no touch input is ever processed.

**Why tests didn't catch it:**
- Native unit tests compile without `ARDUINO` defined → stubs are expected there.
- `tests/test_display_drivers.py::test_09` only greps header text.
- Firmware CI compilation passes either way (stubs compile fine).
- The simulator uses SDL mouse events directly, bypassing the hardware driver path.

### Fix instructions

1. Move the include of `device_profile.h` to the top of `src/touch_driver.cpp`, **before** the `#if`:

```cpp
#include "touch_driver.h"
#include "device_profile.h"   // MUST precede the #if below

// ... hit-test functions ...

#if defined(ARDUINO) && DEVICE_SUPPORTS_TOUCH
#include <Arduino.h>
#include <Wire.h>
// (remove the duplicate device_profile.h include here)
```

2. Harden the macro definitions in `include/device_profile.h` — use `1`/`0` instead of `true`/`false` so `#if` works in any translation unit (C and C++ alike):

```cpp
#define DEVICE_SUPPORTS_TOUCH            1   // was: true
#define DEVICE_SUPPORTS_TOUCH            0   // was: false
```

3. Add a compile-time smoke check to CI so this class of bug is caught: build the `seeed_reterminal_sticky` env and grep the firmware symbols, or add a `build_flags` post-assertion. Minimal option — add to `tests/test_display_drivers.py`:

```python
def test_touch_driver_include_order(self):
    """device_profile.h must be included before any #if DEVICE_SUPPORTS_TOUCH."""
    src = (SRC_DIR / "touch_driver.cpp").read_text()
    include_pos = src.index('#include "device_profile.h"')
    guard_pos = src.index("#if defined(ARDUINO) && DEVICE_SUPPORTS_TOUCH")
    self.assertLess(include_pos, guard_pos,
        "device_profile.h must be included before the DEVICE_SUPPORTS_TOUCH guard")
```

4. **Verify on real hardware** (Sticky + E1003) after fixing: boot log must show `[Touch] Goodix GT911 detected at 0x5D ...`.

---

## HIGH-1: GT911 may wake at the wrong I2C address

**File:** `src/touch_driver.cpp:289-306` (`touch_wake()`)
**Severity:** High — needs hardware validation; touch may be dead after menu-open wake.

### Problem

The GT911 samples its I2C address (0x5D vs 0x14) from the **INT pin level at the rising edge of RST**. `touch_init()` performs the correct dance (INT driven LOW during reset release, then switched to INPUT). `touch_wake()` only toggles RST while INT remains a floating INPUT — the address after wake is **indeterminate**, while `s_touchAddr` is locked to the address found at init. Subsequent reads may target an address the chip no longer answers.

Deep-sleep recovery is safe (power is cut via `PIN_TOUCH_EN` and full `touch_init()` re-runs on next boot), but the menu-open path (`showSystemMenu()` → `touch_wake()`) is affected.

### Fix instructions

Replicate the init-time address-selection sequence in `touch_wake()`:

```cpp
void touch_wake() {
  if (!s_initialized || !s_sleeping) return;

#ifdef PIN_TOUCH_EN
  digitalWrite(PIN_TOUCH_EN, HIGH);
  delay(10);
#endif

  // Repeat GT911 address selection: INT LOW while RST rises
#ifdef PIN_TOUCH_INT
  pinMode(PIN_TOUCH_INT, OUTPUT);
  digitalWrite(PIN_TOUCH_INT, LOW);
#endif
#ifdef PIN_TOUCH_RST
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(5);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(20);
#endif
#ifdef PIN_TOUCH_INT
  pinMode(PIN_TOUCH_INT, INPUT);
#endif
  delay(50);

  s_sleeping = false;
}
```

**Validate on hardware:** open the system menu (button), close it (touch sleep), reopen it (touch wake), then confirm taps register.

---

## HIGH-2: Partial-refresh counter counts syncs, not renders

**File:** `src/main.cpp:1426-1455` (policy block), before the `HTTP_CODE_NOT_MODIFIED` branch
**Severity:** High — the 6:1 ghosting mitigation fires at the wrong moments.

### Problem

The refresh-policy block (which increments `rtc_consecutivePartialCount`) executes **before** the 304 handling. On a `304 Not Modified` where nothing is rendered, the counter still increments. Consequences:

- The "clean cycle" full refresh often fires on a no-op cycle (nothing drawn), wasting the mitigation.
- When genuinely **new** content (HTTP 200) arrives — the highest ghosting-risk render — the counter sits at an arbitrary position, so a brand-new image can get a *partial* refresh.

### Fix instructions

Evaluate the policy **only at points where a render actually happens**:

1. Compute a `bool wantRender` first (new content, cached-item switch, forced refresh).
2. Move the counter increment/decision into a helper invoked immediately before `renderRotatedBuffer(...)`:

```cpp
static bool decideRefreshMode(bool forceRefresh, const String& serverRefreshMode) {
  if (!DEVICE_SUPPORTS_PARTIAL_REFRESH)      { rtc_consecutivePartialCount = 0; return false; }
  if (forceRefresh || FORCE_REFRESH_ON_BOOT) { rtc_consecutivePartialCount = 0; return false; }
  if (serverRefreshMode == "full")           { rtc_consecutivePartialCount = 0; return false; }
  if (s_refreshMode == REFRESH_MODE_ALWAYS_FULL) { rtc_consecutivePartialCount = 0; return false; }
  if (rtc_consecutivePartialCount < s_partialRefreshLimit) {
    rtc_consecutivePartialCount++;
    return true;
  }
  rtc_consecutivePartialCount = 0;
  return false;  // clean cycle
}
```

3. In the 304 no-change path (no render), do **not** touch the counter.
4. Consider forcing a **full** refresh whenever the ETag changed (new image), independent of the cadence — partial-updating into a completely new image is the primary ghosting scenario.
5. Extend the native tests: assert that N consecutive 304 no-ops leave the counter unchanged.

---

## HIGH-3: Gray4 frames bypass the partial-refresh policy

**File:** `src/main.cpp:411-461` (`renderRotatedBuffer`)
**Severity:** High — undefined interaction between the two new features.

### Problem

When `colorMode == COLOR_MODE_GRAY4`, the `mode` parameter (partial vs full, decided by the 6:1 policy) is ignored; `SEEED_EPAPER_REFRESH_GRAY4` is always committed:

```cpp
if (colorMode == COLOR_MODE_GRAY4) {
  refErr = seeed_epaper_panel_write_bitmap_gray4(...);
  if (refErr == ESP_OK) {
    refErr = seeed_epaper_panel_commit(s_panel, &full_screen, SEEED_EPAPER_REFRESH_GRAY4);  // policy ignored
  }
}
```

### Fix instructions

1. Check the `seeed_epaper` library: does `SEEED_EPAPER_REFRESH_GRAY4` flash like a full refresh? Document the answer in `docs/`.
2. Make the interaction explicit — either:
   - pass the policy decision through (if the library supports a partial gray4 waveform), or
   - document + log that gray4 content always uses the gray4 waveform and is exempt from the 6:1 cadence (and still count it in `rtc_consecutivePartialCount` so the cadence for mono content isn't skewed).
3. Add a test asserting the chosen refresh mode for a gray4 frame under both policy states.

---

## MEDIUM-1: Timer wake + USB plugged in → double sync & counter clobber

**File:** `src/main.cpp:2011-2061`
**Severity:** Medium — wasteful edge case.

### Problem

On `ESP_SLEEP_WAKEUP_TIMER`, `fetchAndRender(false)` runs; then if USB was connected in the meantime, execution falls through the "Cold Boot / Reset" section (`main.cpp:2041-2061`), resets `rtc_consecutivePartialCount` (line 2048) and runs `fetchAndRender(true)` — **two HTTP syncs in one wake**. On the pure battery path this is unreachable (device deep-sleeps inside `scheduleNextSync`), and cold boot is already handled at line 1944, so line 2048's unconditional reset only adds risk.

### Fix instructions

- After the timer-wake block, `return` unconditionally (or guard the cold-boot section with `if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED)`):

```cpp
      rtc_lastSyncIntervalS = sleepSec;
      scheduleNextSync(sleepSec);
      return;   // was: if (!isUsbPowerConnected()) return;
```

Note: on USB, `scheduleNextSync()` only arms `s_nextSyncMillis` and returns — so an explicit `return` keeps USB behavior identical while eliminating the double sync.

---

## MEDIUM-2: Gray4 auto-detection fallback can render garbage

**File:** `src/main.cpp:1485-1509`
**Severity:** Medium — server-dependent.

### Problem

Color mode is detected via `X-ST-Color-Mode` header **or** `http.getSize() == 96000`. If the server sends a gray4 payload **without** the header and with a chunked response (`Content-Length: -1`), detection falls back to mono: 48,000 bytes are read, the size check passes, and the first half of the 2bpp frame is rendered as 1bpp → garbage on screen.

### Fix instructions

1. Require the header for gray4; treat length alone as insufficient unless it *exactly* matches a known format:

```cpp
if (colorModeHdr == "gray4" || colorModeHdr == "2bpp") {
  activeColorMode = COLOR_MODE_GRAY4; expectedSize = 96000;
} else if (colorModeHdr == "mono" || colorModeHdr == "1bpp") {
  activeColorMode = COLOR_MODE_1BPP_MONO; expectedSize = 48000;
} else if (httpContentLen == 96000) {          // header absent: trust exact length
  activeColorMode = COLOR_MODE_GRAY4; expectedSize = 96000;
} else if (httpContentLen == 48000) {
  activeColorMode = COLOR_MODE_1BPP_MONO; expectedSize = 48000;
} else {
  Serial.printf("[Sync] Unknown frame format (len=%d, hdr='%s') — aborting render.\n",
                httpContentLen, colorModeHdr.c_str());
  http.end();
  return;   // do NOT guess
}
```

2. Derive `96000`/`48000` from `EPD_WIDTH`/`EPD_HEIGHT` (`(W*H)/4` and `(W*H)/8`) instead of hardcoding.

---

## LOW issues & maintainability

| # | File:Line | Issue | Recommended fix |
|---|---|---|---|
| L1 | `src/touch_driver.cpp` + `src/setup_screen.cpp:575-579` | Menu geometry constants (`58/35/5/30/w-60`, footer 56) duplicated in two files — will drift | Extract into a shared `menu_layout.h` included by both |
| L2 | `src/touch_driver.cpp:33` | `offset <= itemH` makes y=93 hit row 0 though the box ends at 92 (tests encode this tolerance) | Either accept + comment as intentional touch tolerance, or change to `< itemH` and update tests |
| L3 | `src/touch_driver.cpp:23` | `x > (itemX + itemW)` vs `>=` inconsistency at the exact right edge (x=770) | Use `>=` for consistency with exclusive bounds elsewhere |
| L4 | `include/device_profile.h:72,112` / `src/touch_driver.cpp` | `DEVICE_TOUCH_CONTROLLER` (and new `TOUCH_AUTO`) are never read — driver auto-probes all addresses | Remove the dead defines, or honor the configured controller and skip probing others |
| L5 | `src/main.cpp:~680,~715` | `touch_sleep()` called twice in `enterDeepSleepWithWakeup()` | Remove one call (guard makes it harmless, but it's noise) |
| L6 | `src/main.cpp:805` | NVS key `part_limit` is read but never written → always 6 | Either add a menu/NVS write path or remove the read |
| L7 | `src/main.cpp:411-461` | Redundant compile-time (`#if DEVICE_COLOR_MODE`) **and** runtime (`if (colorMode == ...)`) format selection; runtime GRAY16/SPECTRA6 branches unreachable on mono devices | Simplify to runtime dispatch only |
| L8 | `src/main.cpp:107` | Stale comment: `s_rotatedBuffer // 48,000 bytes` (now 96 KB for Sticky/E1001) | Update comment |
| L9 | `simulator/main_sim.cpp` | `redrawSimMenu` always logs "fast partial refresh" even when `fastPartial=false` | Use the parameter in the log line |
| L10 | `simulator/sim_display.cpp` ('M' hotkey) | Fake tap at (50,20) only opens the menu; is a no-op when the menu is already open (header zone → hit -1) | Document or pick a coordinate that toggles |
| L11 | `src/setup_screen.cpp` (`rotate_gray4_portrait`) | Per-pixel RMW loop (~384k iterations) — measure on-device latency | If slow, optimize with word-level ops or LUT |
| L12 | `ef8fc24` commit hygiene | Buried unrelated bugfix: `initDisplayPanel()` missing `return true;` (was UB) | Keep, but call it out in the merge/PR description |

---

## What's good (keep it)

- ✅ Clean `executeMenuAction()` extraction — removed ~120 lines of duplicated if/else from the OK-button handler.
- ✅ Runtime rotation dispatchers (`rotate_buffer_180_mode` / `rotate_buffer_portrait_mode`) are the right architecture; `rotate_gray4_180` bitwise 2bpp reversal is correct for 800 px rows and symmetric (verified by tests).
- ✅ Cache correctly tracks per-slide `colorMode`/`frameSize` across 304 transitions.
- ✅ Menu grew 8→9 items consistently (renderer, navigation modulo, hit-test rows, tests).
- ✅ Edge-triggered touch debounce (250 ms) and proper touch sleep/wake hooks into deep sleep, power-off, unpair, factory reset, and menu timeout.
- ✅ Simulator mirrors the full touch menu workflow via SDL mouse (`[Click]`/`[M]`), including per-model `touchSupported` gating.
- ✅ Both native C++ and Python tests pin the menu hit-test geometry.

---

## Recommended merge checklist

- [x] **CRITICAL-1** fixed (include order) — verified: GT911 strings present in Sticky firmware ELF ✅ (`942f18c`)
- [ ] **HIGH-1** validated on hardware (menu open/close/reopen via touch) — code fixed in `942f18c`, on-device validation pending
- [x] **HIGH-2** policy evaluated only at render points; counter tests updated ✅ (`942f18c`)
- [x] **HIGH-3** gray4 × partial-policy interaction documented/decided ✅ (`942f18c`)
- [x] **MEDIUM-1** timer-wake double-sync eliminated ✅ (`942f18c`)
- [x] **MEDIUM-2** gray4 detection hardened (no-guess fallback) ✅ (`942f18c`)
- [x] **L1–L6, L10** fixed (uncommitted follow-up, verified: all tests pass, all 5 firmware envs + simulator build)
- [ ] **L11** gray4 portrait rotation perf — requires on-device measurement
- [x] Native unit tests pass: `pio test -e test_native`
- [x] Python suite passes: `cd tests && python -m unittest test_display_drivers`
- [x] Firmware builds for all five envs (sticky, e1001–e1004) + simulator

### Follow-up fixes (after `942f18c`)

| Item | Fix |
|---|---|
| L3 | Hit-test right edge now `>=` (exclusive), consistent with `gfx_fill_rect` [x, x+w); boundary locked by new assertions (769 hit / 770 miss) in native + Python tests |
| L4 | Dead `TOUCH_*` constants and per-device `DEVICE_TOUCH_CONTROLLER` defines removed from `device_profile.h`; auto-probing documented in a comment instead |
| L6 | Never-written `part_limit` NVS read removed; `s_partialRefreshLimit` is now a `static const` single source of truth |
| L1 (rest) | `renderSystemMenu` footer now uses `MENU_FOOTER_HEIGHT` (text positioned relative to `footerY`) |
| L10 | Sim 'M' hotkey now taps the footer zone (`m_height - 10`): opens the menu when closed, triggers BACK/CLOSE when open — a true toggle |
