#include "epaper_panel_priv.h"

#include <stddef.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_debug_log.h"

// ED2208 / GDEP073E01 command sequencing for 7.3" Spectra 6 panel (800x480).
static const char *TAG = "ed2208";

static uint64_t driver_log_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static esp_err_t write_reg8(seeed_epaper_panel_t *panel, uint8_t command, uint8_t value)
{
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, command), TAG, "write command failed");
    return seeed_epaper_panel_write_data(panel, &value, 1);
}

static esp_err_t ed2208_init_mode(seeed_epaper_panel_t *panel, seeed_epaper_refresh_mode_t mode)
{
    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "init_mode start: size=%ux%u",
             (unsigned)panel->info.width,
             (unsigned)panel->info.height);

    // Hardware reset
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_reset(panel), TAG, "reset failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_wait_ready(panel), TAG, "wait ready failed");

    // CMD66 (0xAA): analog settings
    const uint8_t cmd66_data[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0xAA), TAG, "cmd66 failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, cmd66_data, sizeof(cmd66_data)), TAG, "cmd66 data failed");

    // Power Setting (0x01)
    const uint8_t pwr_data[] = {0x3F, 0x00, 0x32, 0x2A, 0x0E, 0x2A};
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x01), TAG, "pwr failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, pwr_data, sizeof(pwr_data)), TAG, "pwr data failed");

    // Panel Setting (0x00)
    const uint8_t psr_data[] = {0x5F, 0x69};
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x00), TAG, "psr failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, psr_data, sizeof(psr_data)), TAG, "psr data failed");

    // Power off sequence (0x03)
    const uint8_t pfs_data[] = {0x00, 0x54, 0x00, 0x44};
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x03), TAG, "pfs failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, pfs_data, sizeof(pfs_data)), TAG, "pfs data failed");

    // Booster Soft Start (0x05)
    const uint8_t btst_data[] = {0x40, 0x1F, 0x1F, 0x2C};
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x05), TAG, "btst failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, btst_data, sizeof(btst_data)), TAG, "btst data failed");

    // TCON (0x06)
    const uint8_t tcon_data[] = {0x6F, 0x1F, 0x1F, 0x22};
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x06), TAG, "tcon failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, tcon_data, sizeof(tcon_data)), TAG, "tcon data failed");

    // CDI (0x08)
    ESP_RETURN_ON_ERROR(write_reg8(panel, 0x08, 0x6F), TAG, "cdi failed");

    // Resolution (0x13): 800 x 480
    const uint8_t res_data[] = {
        (uint8_t)(panel->info.width >> 8),
        (uint8_t)(panel->info.width & 0xFF),
        (uint8_t)(panel->info.height >> 8),
        (uint8_t)(panel->info.height & 0xFF),
    };
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x13), TAG, "res failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, res_data, sizeof(res_data)), TAG, "res data failed");

    // Power ON (0x04)
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x04), TAG, "pon failed");
    esp_err_t err = seeed_epaper_panel_wait_ready(panel);

    APP_EPAPER_DEBUG_LOGI(TAG, "init_mode done: elapsed=%llums err=%s",
             (unsigned long long)(driver_log_ms() - start_ms),
             esp_err_to_name(err));
    return err;
}

static esp_err_t ed2208_set_window(seeed_epaper_panel_t *panel, const seeed_epaper_area_t *area)
{
    // ED2208 always writes full frame starting from 0x10
    return ESP_OK;
}

static esp_err_t ed2208_write_image(seeed_epaper_panel_t *panel, const uint8_t *buffer, size_t len, bool previous_plane)
{
    if (previous_plane) {
        // ED2208 Spectra 6 does not use previous plane differential driving
        return ESP_OK;
    }

    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "write_image start: len=%u", (unsigned)len);

    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x10), TAG, "dtm command failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_data(panel, buffer, len), TAG, "dtm data failed");

    APP_EPAPER_DEBUG_LOGI(TAG, "write_image done: elapsed=%llums",
             (unsigned long long)(driver_log_ms() - start_ms));
    return ESP_OK;
}

static esp_err_t ed2208_refresh(seeed_epaper_panel_t *panel, seeed_epaper_refresh_mode_t mode)
{
    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "refresh start (full AC refresh)");

    // DRF (0x12): Display Refresh
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x12), TAG, "drf failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    // Wait busy (Spectra 6 full AC waveform takes ~14 seconds)
    esp_err_t err = seeed_epaper_panel_wait_ready(panel);

    APP_EPAPER_DEBUG_LOGI(TAG, "refresh done: elapsed=%llums err=%s",
             (unsigned long long)(driver_log_ms() - start_ms),
             esp_err_to_name(err));
    return err;
}

static esp_err_t ed2208_sleep(seeed_epaper_panel_t *panel)
{
    const uint64_t start_ms = driver_log_ms();
    APP_EPAPER_DEBUG_LOGI(TAG, "sleep start");

    // Power OFF (0x02)
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_write_command(panel, 0x02), TAG, "pof failed");
    ESP_RETURN_ON_ERROR(seeed_epaper_panel_wait_ready(panel), TAG, "pof wait failed");

    // Deep Sleep (0x07) with check byte 0xA5
    esp_err_t err = write_reg8(panel, 0x07, 0xA5);

    APP_EPAPER_DEBUG_LOGI(TAG, "sleep done: elapsed=%llums err=%s",
             (unsigned long long)(driver_log_ms() - start_ms),
             esp_err_to_name(err));
    return err;
}

const seeed_epaper_driver_t *seeed_epaper_ed2208_driver(void)
{
    static const seeed_epaper_driver_t driver = {
        .name = "ED2208",
        .info = {
            .width = 800,
            .height = 480,
            .native_bpp = 4,
            .supports_partial = false,
            .supports_gray4 = false,
            .supports_color = true,
        },
        .init_mode = ed2208_init_mode,
        .set_window = ed2208_set_window,
        .write_image = ed2208_write_image,
        .refresh = ed2208_refresh,
        .sleep = ed2208_sleep,
    };
    return &driver;
}
