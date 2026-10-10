# Screen Mockup Renderer

This tool renders all ScreenTinker MCU e-paper display screens (800x480 1-bit monochrome) natively using mock hardware drivers, outputting PNG images to `docs/screens/`.

## Prerequisites

- macOS / Linux with `clang++` or `g++` (C++17)
- Python 3 with `Pillow` (`pip install Pillow`)

## Usage

Run the build & render script from the repository root:

```bash
# Compile and render all screens into docs/screens/
clang++ -std=c++17 -I./include -I./tools/screen_renderer \
  -I.pio/libdeps/seeed_reterminal_sticky/QRCode/src \
  tools/screen_renderer/main.cpp \
  src/setup_screen.cpp \
  .pio/libdeps/seeed_reterminal_sticky/QRCode/src/qrcode.c \
  -o tools/screen_renderer/render_screens

./tools/screen_renderer/render_screens docs/screens

python3 -c "
import glob, os
from PIL import Image
for bmp in glob.glob('docs/screens/*.bmp'):
    png = bmp[:-4] + '.png'
    Image.open(bmp).save(png)
    os.remove(bmp)
"
```
