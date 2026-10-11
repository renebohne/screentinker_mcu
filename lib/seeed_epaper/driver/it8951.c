#include "epaper_panel_priv.h"

#include <stddef.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_debug_log.h"

// IT8951 command sequencing for 10.3" Grayscale panel (1872x1404).
static const char *TAG = "it8951";

// IT8951 SPI Preambles
#define IT8951_PREAMBLE_CMD    0x6000
#define IT8951_PREAMBLE_DATA   0x0000

// IT8951 Commands
#define IT8951_CMD_SYS_RUN     0x0001
#define IT8951_CMD_STANDBY     0x0002
#define IT8951_CMD_SLEEP       0x0003
#define IT8951_CMD_LOAD_IMG    0x0021
#define IT8951_CMD_DPY_AREA    0x0034

static uint64_t driver_log_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static esp_err_t it8951_write_cmd(seeed_epaper_panel_t *panel, uint16_t cmd)
{
    uint8_t packet[4] = {
        (uint8_t)(IT8951_PREAMBLE_CMD >> 8),
        (uint8_t)(IT8951_PREAMBLE_CMD & 0xFF),
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF),
    };
    return seeed_epaper_panel_write_data(panel, packet, sizeof(packet));
}

static esp_err_t it8951_write_data16(seeed_epaper_panel_t *panel, uint16_t data)
{
    uint8_t packet[4] = {
        (uint8_t)(IT8951_PREAMBLE_DATA >> 8),
        (uint8_t)(IT8951_PREAMBLE_DATA & 0xFF),
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF),
    };
    return seeed_epaper_panel_write_data(panel, packet, sizeof(packet));
}

static esp_err_t it8951_init_mode(seeed_epaper_panel_t *panel, seeed_epaper_refresh_mode_t mode)
{
    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "init_mode start: size=%ux%u",
             (unsigned)panel->info.width,
             (unsigned)panel->info.height);

    // Hardware reset
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_reset(panel), TAG, "reset failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_wait_ready(panel), TAG, "wait ready failed");

    // System Run
    ESP_RETURN_ON_ERROR(it8951_write_cmd(panel, IT8951_CMD_SYS_RUN), TAG, "sys run failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_wait_ready(panel), TAG, "wait ready after sys run failed");

    APP_EPAPER_DEBUG_LOGI(TAG, "init_mode done: elapsed=%llums",
             (unsigned long long)(driver_log_ms() - start_ms));
    return ESP_OK;
}

static esp_err_t it8951_set_window(seeed_epaper_panel_t *panel, const seeed_epaper_area_t *area)
{
    return ESP_OK;
}

static esp_err_t it8951_write_image(seeed_epaper_panel_t *panel, const uint8_t *buffer, size_t len, bool previous_plane)
{
    if (previous_plane) {
        return ESP_OK;
    }

    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "write_image start: len=%u", (unsigned)len);

    // Load Image Area command
    ESP_RETURN_ON_ERROR(it8951_write_cmd(panel, IT8951_CMD_LOAD_IMG), TAG, "load img cmd failed");
    // Memory target args: x=0, y=0, w=width, h=height
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, 0), TAG, "arg x failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, 0), TAG, "arg y failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, panel->info.width), TAG, "arg w failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, panel->info.height), TAG, "arg h failed");

    // Send preamble for data burst
    uint8_t preamble[2] = {
        (uint8_t)(IT8951_PREAMBLE_DATA >> 8),
        (uint8_t)(IT8951_PREAMBLE_DATA & 0xFF),
    };
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, preamble, sizeof(preamble)), TAG, "data preamble failed");

    // Burst image payload
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, buffer, len), TAG, "burst payload failed");

    APP_EPAPER_DEBUG_LOGI(TAG, "write_image done: elapsed=%llums",
             (unsigned long long)(driver_log_ms() - start_ms));
    return ESP_OK;
}

static esp_err_t it8951_refresh(seeed_epaper_panel_t *panel, seeed_epaper_refresh_mode_t mode)
{
    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "refresh start: mode=%d", (int)mode);

    // Display Area command
    ESP_RETURN_ON_ERROR(it8951_write_cmd(panel, IT8951_CMD_DPY_AREA), TAG, "dpy area failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, 0), TAG, "x failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, 0), TAG, "y failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, panel->info.width), TAG, "w failed");
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, panel->info.height), TAG, "h failed");

    // Mode 2 = GC16 (16 gray levels, ~1.2s), Mode 1 = A2 (fast partial mono, ~180ms)
    uint16_t dpy_mode = (mode == SEEED_EPAPER_REFRESH_PARTIAL) ? 1 : 2;
    ESP_RETURN_ON_ERROR(it8951_write_data16(panel, dpy_mode), TAG, "mode failed");

    esp_err_t err = seeed_epaper_panel_wait_ready(panel);

    APP_EPAPER_DEBUG_LOGI(TAG, "refresh done: elapsed=%llums err=%s",
             (unsigned long long)(driver_log_ms() - start_ms),
             esp_err_to_name(err));
    return err;
}

static esp_err_t it8951_sleep(seeed_epaper_panel_t *panel)
{
    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "sleep start");

    ESP_RETURN_ON_ERROR(it8951_write_cmd(panel, IT8951_CMD_SLEEP), TAG, "sleep cmd failed");

    APP_EPAPER_DEBUG_LOGI(TAG, "sleep done: elapsed=%llums",
             (unsigned long long)(driver_log_ms() - start_ms));
    return ESP_OK;
}

const seeed_epaper_driver_t *seeed_epaper_it8951_driver(void)
{
    static const seeed_epaper_driver_t driver = {
        .name = "IT8951",
        .info = {
            .width = 1872,
            .height = 1404,
            .native_bpp = 4,
            .supports_partial = true,
            .supports_gray4 = true,
            .supports_color = false,
        },
        .init_mode = it8951_init_mode,
        .set_window = it8951_set_window,
        .write_image = it8951_write_image,
        .refresh = it8951_refresh,
        .sleep = it8951_sleep,
    };
    return &driver;
}
