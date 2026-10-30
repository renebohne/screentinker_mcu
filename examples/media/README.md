# ScreenTinker MCU Sample Media & Pixel Art

This directory contains test patterns and pixel art specifically crafted for testing ScreenTinker MCU LED strip and matrix rendering.

---

## 1. 5×1 Smart LED Strip Patterns (`examples/media/5x1/`)

Designed for testing 5-pixel WS2812/SK6812 LED strips and controllers (e.g. Seeed Studio XIAO ESP32-C6 / ESP32-S3).

| File | Resolution | Description | Colors |
| :--- | :--- | :--- | :--- |
| [`led_pattern_1_spectrum.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/5x1/led_pattern_1_spectrum.png) | 5×1 | Full Color Spectrum | Red, Yellow, Green, Cyan, Magenta |
| [`led_pattern_2_neon.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/5x1/led_pattern_2_neon.png) | 5×1 | Cyberpunk Neon Shift | Deep Pink, Hot Magenta, Violet, Cyan, Aqua |
| [`led_pattern_3_sunset.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/5x1/led_pattern_3_sunset.png) | 5×1 | Sunset Warm Gradient | Golden Yellow, Amber, Orange, Coral, Crimson |

---

## 2. 64×64 LED Matrix Pixel Art (`examples/media/64x64/`)

Designed for 64×64 RGB LED matrices (HUB75, WS2812, etc.) using `24bit-rgb888` color depth.

| File | Resolution | Description |
| :--- | :--- | :--- |
| [`matrix_64x64_synthwave.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/64x64/matrix_64x64_synthwave.png) | 64×64 | **Cyberpunk Synthwave Sunset**: Neon sun with scanlines over mountain silhouettes and a cyan perspective wireframe grid. |
| [`matrix_64x64_arcade_alien.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/64x64/matrix_64x64_arcade_alien.png) | 64×64 | **Arcade Space Invader**: Retro 8-bit lime green alien sprite with orange eyes inside a framed arcade display. |
| [`matrix_64x64_heart.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/64x64/matrix_64x64_heart.png) | 64×64 | **8-Bit Health Heart**: Shaded red video-game heart power-up with specular white highlights and golden sparkles. |
| [`matrix_64x64_ghost.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/64x64/matrix_64x64_ghost.png) | 64×64 | **Retro Pac-Ghost & Maze**: Classic red ghost sprite traversing blue neon arcade labyrinth walls with power pellets. |

---

## 3. 32×8 LED Matrix Pixel Art & Status Badges (`examples/media/32x8/`)

Designed for 32×8 LED pixel matrix displays (e.g. WS2812 / SK6812 flexible 32×8 panels or 4× 8×8 chained modules) using `24bit-rgb888` color depth.

| File | Resolution | Description |
| :--- | :--- | :--- |
| [`matrix_32x8_clock.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/32x8/matrix_32x8_clock.png) | 32×8 | **Neon Digital Clock**: Crisp 5×7 cyan digital clock reading `14:35` with amber colons. |
| [`matrix_32x8_weather.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/32x8/matrix_32x8_weather.png) | 32×8 | **Weather & Temperature**: Radiant 8×8 golden sun glyph with orange corona and `+23°C` readout. |
| [`matrix_32x8_on_air.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/32x8/matrix_32x8_on_air.png) | 32×8 | **Studio ON AIR Sign**: High-contrast illuminated studio door sign framed in deep crimson red. |
| [`matrix_32x8_pacman.png`](file:///Users/rene/Documents/github/screentinker_mcu/examples/media/32x8/matrix_32x8_pacman.png) | 32×8 | **Arcade Pac-Chase**: Classic yellow Pac-Man sprite pursuing pellets and a red arcade ghost. |

---

## Testing with the ScreenTinker MCU Simulator

### Running the 5×1 Strip Simulator
```bash
./screentinker_sim --width 5 --height 1
```

### Running the 32×8 Matrix Simulator
```bash
./screentinker_sim --width 32 --height 8 --rgb888
```

### Running the 64×64 Matrix Simulator
```bash
# First time pairing:
./screentinker_sim --config sim_config_64x64.json --width 64 --height 64 --pair

# Normal playback:
./screentinker_sim --config sim_config_64x64.json
```
