#pragma once

// ─── ScreenTinker System Menu Layout Geometry ─────────────────────────────────
// Shared between on-screen rendering (setup_screen.cpp) and touch hit-testing (touch_driver.cpp).

#define MENU_ITEM_COUNT       9

#define MENU_HEADER_HEIGHT    54
#define MENU_START_Y          58
#define MENU_ITEM_HEIGHT      35
#define MENU_ITEM_GAP         5
#define MENU_ITEM_PITCH       (MENU_ITEM_HEIGHT + MENU_ITEM_GAP) // 40 px

#define MENU_ITEM_MARGIN_X    30
#define MENU_FOOTER_HEIGHT    56
