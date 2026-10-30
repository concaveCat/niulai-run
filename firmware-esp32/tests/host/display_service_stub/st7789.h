#ifndef QW_ST7789_STUB_H
#define QW_ST7789_STUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define QW_ST7789_DEFAULT_WIDTH 170
#define QW_ST7789_DEFAULT_HEIGHT 320

typedef struct {
    int host;
    int sclk_gpio;
    int mosi_gpio;
    int cs_gpio;
    int dc_gpio;
    int reset_gpio;
    int backlight_gpio;
    int spi_clock_hz;
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t pixel_format;
    uint8_t madctl;
    bool backlight_active_high;
} st7789_config_t;

typedef struct st7789_device *st7789_handle_t;

esp_err_t st7789_init(const st7789_config_t *config, st7789_handle_t *out);
esp_err_t st7789_set_backlight(st7789_handle_t dev, bool on);
esp_err_t st7789_set_window(st7789_handle_t dev, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1);
esp_err_t st7789_write_pixels(st7789_handle_t dev, const void *rgb565,
                              size_t length_bytes);
bool st7789_deinit(st7789_handle_t dev);

#endif
