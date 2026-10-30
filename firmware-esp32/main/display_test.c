#include "display_test.h"

#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "st7789.h"

static const char *TAG = "display_test";

/* st7789_driver configures the SPI bus for 4096-byte transfers. Keep the
 * generated pixel payload at or below that limit if the panel width changes. */
#define QW_DISPLAY_MAX_TRANSFER_BYTES 4096U
#define QW_DISPLAY_BAR_COUNT 8U
#define QW_DISPLAY_PROFILE_HOLD_MS 2500U

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb888_t;

typedef struct {
    const char *name;
    uint16_t x_offset;
    uint8_t madctl;
} display_profile_t;

static const rgb888_t s_bars[QW_DISPLAY_BAR_COUNT] = {
    {255, 255, 255}, /* white */
    {255, 255, 0},   /* yellow */
    {0, 255, 255},   /* cyan */
    {0, 255, 0},     /* green */
    {255, 0, 255},   /* magenta */
    {255, 0, 0},     /* red */
    {0, 0, 255},     /* blue */
    {0, 0, 0},       /* black */
};

static const display_profile_t s_profiles[] = {
    {"configured", CONFIG_NIULAI_DISPLAY_X_OFFSET,
     CONFIG_NIULAI_DISPLAY_MADCTL},
    {"madctl-60-x35", 35, 0x60},
    {"madctl-60-x0", 0, 0x60},
    {"madctl-68-x35", 35, 0x68},
    {"madctl-c0-x35", 35, 0xc0},
};

static uint16_t rgb888_to_rgb565(rgb888_t color)
{
    return (uint16_t)(((uint16_t)(color.red & 0xf8U) << 8) |
                      ((uint16_t)(color.green & 0xfcU) << 3) |
                      (color.blue >> 3));
}

static void fill_rgb565_be(uint8_t *buffer, size_t pixel_count,
                           uint16_t color)
{
    for (size_t i = 0; i < pixel_count; i++) {
        buffer[2U * i] = (uint8_t)(color >> 8);
        buffer[2U * i + 1U] = (uint8_t)color;
    }
}

static st7789_config_t display_config(void)
{
    st7789_config_t config = st7789_config_default();
    config.sclk_gpio = CONFIG_NIULAI_DISPLAY_SCLK_GPIO;
    config.mosi_gpio = CONFIG_NIULAI_DISPLAY_MOSI_GPIO;
    config.cs_gpio = CONFIG_NIULAI_DISPLAY_CS_GPIO;
    config.dc_gpio = CONFIG_NIULAI_DISPLAY_DC_GPIO;
    config.reset_gpio = CONFIG_NIULAI_DISPLAY_RESET_GPIO;
    config.backlight_gpio = CONFIG_NIULAI_DISPLAY_BACKLIGHT_GPIO;
    config.width = CONFIG_NIULAI_DISPLAY_WIDTH;
    config.height = CONFIG_NIULAI_DISPLAY_HEIGHT;
    config.x_offset = CONFIG_NIULAI_DISPLAY_X_OFFSET;
    config.y_offset = CONFIG_NIULAI_DISPLAY_Y_OFFSET;
    config.madctl = CONFIG_NIULAI_DISPLAY_MADCTL;
#if defined(CONFIG_NIULAI_DISPLAY_BACKLIGHT_ACTIVE_HIGH)
    config.backlight_active_high = true;
#else
    config.backlight_active_high = false;
#endif
    return config;
}

static esp_err_t draw_bars(st7789_handle_t display,
                           const st7789_config_t *config)
{
    if (config->width == 0 || config->height < QW_DISPLAY_BAR_COUNT) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t bytes_per_row = (size_t)config->width * 2U;
    size_t rows_per_chunk = QW_DISPLAY_MAX_TRANSFER_BYTES / bytes_per_row;
    if (rows_per_chunk == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *buffer = malloc(rows_per_chunk * bytes_per_row);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    for (size_t bar = 0; bar < QW_DISPLAY_BAR_COUNT && err == ESP_OK;
         bar++) {
        uint16_t y0 = (uint16_t)(((uint32_t)config->height * bar) /
                                 QW_DISPLAY_BAR_COUNT);
        uint16_t y1 = (uint16_t)(((uint32_t)config->height * (bar + 1U)) /
                                     QW_DISPLAY_BAR_COUNT -
                                 1U);
        uint16_t color = rgb888_to_rgb565(s_bars[bar]);

        for (uint16_t y = y0; y <= y1 && err == ESP_OK;) {
            uint16_t remaining = (uint16_t)(y1 - y + 1U);
            uint16_t chunk_rows = remaining < rows_per_chunk
                                      ? remaining
                                      : (uint16_t)rows_per_chunk;
            size_t pixel_count = (size_t)config->width * chunk_rows;
            fill_rgb565_be(buffer, pixel_count, color);

            err = st7789_set_window(display, 0, y,
                                    (uint16_t)(config->width - 1U),
                                    (uint16_t)(y + chunk_rows - 1U));
            if (err == ESP_OK) {
                err = st7789_write_pixels(display, buffer,
                                          pixel_count * 2U);
            }
            y = (uint16_t)(y + chunk_rows);
        }
    }

    free(buffer);
    return err;
}

esp_err_t qw_display_color_bar_test(void)
{
    st7789_config_t base_config = display_config();

    ESP_LOGI(TAG,
             "ST7789 color-bar test: %ux%u, SPI=%dHz, pins SCLK=%d MOSI=%d "
             "CS=%d DC=%d RESET=%d BL=%d",
             base_config.width, base_config.height, base_config.spi_clock_hz,
             base_config.sclk_gpio, base_config.mosi_gpio, base_config.cs_gpio,
             base_config.dc_gpio, base_config.reset_gpio,
             base_config.backlight_gpio);

    for (;;) {
        for (size_t i = 0; i < sizeof(s_profiles) / sizeof(s_profiles[0]); i++) {
            st7789_config_t config = base_config;
            st7789_handle_t display = NULL;
            config.x_offset = s_profiles[i].x_offset;
            config.madctl = s_profiles[i].madctl;

            ESP_LOGI(TAG, "trying profile %s: x_offset=%u madctl=0x%02x",
                     s_profiles[i].name, config.x_offset, config.madctl);
            esp_err_t err = st7789_init(&config, &display);
            if (err == ESP_OK) {
                err = st7789_set_backlight(display, true);
            }
            if (err == ESP_OK) {
                err = draw_bars(display, &config);
            }
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "profile %s failed: %s", s_profiles[i].name,
                         esp_err_to_name(err));
                if (display != NULL) {
                    st7789_set_backlight(display, false);
                    st7789_deinit(display);
                }
                continue;
            }

            ESP_LOGI(TAG, "profile %s displayed for %u ms", s_profiles[i].name,
                     QW_DISPLAY_PROFILE_HOLD_MS);
            vTaskDelay(pdMS_TO_TICKS(QW_DISPLAY_PROFILE_HOLD_MS));
            st7789_set_backlight(display, false);
            st7789_deinit(display);
        }
    }
}
