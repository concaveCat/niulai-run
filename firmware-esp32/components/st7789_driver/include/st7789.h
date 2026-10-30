#ifndef QW_ST7789_H
#define QW_ST7789_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "qw_hal.h"

#define QW_ST7789_DEFAULT_WIDTH 170
#define QW_ST7789_DEFAULT_HEIGHT 320
#define QW_ST7789_DEFAULT_SPI_HZ 8000000
#define QW_ST7789_DEFAULT_X_OFFSET 35
#define QW_ST7789_DEFAULT_Y_OFFSET 0
#define QW_ST7789_DEFAULT_PIXEL_FORMAT 0x55 /* RGB565 */
#define QW_ST7789_DEFAULT_MADCTL 0x00

typedef struct {
    spi_host_device_t host;
    gpio_num_t sclk_gpio;
    gpio_num_t mosi_gpio;
    gpio_num_t cs_gpio;
    gpio_num_t dc_gpio;
    gpio_num_t reset_gpio;
    gpio_num_t backlight_gpio;
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

typedef struct {
    qw_hal_gpio_t gpio;
    qw_hal_clock_t clock;
    qw_hal_spi_device_t spi;
} st7789_hal_t;

st7789_config_t st7789_config_default(void);
esp_err_t st7789_init(const st7789_config_t *config, st7789_handle_t *out);
esp_err_t st7789_init_with_hal(const st7789_config_t *config,
                               const st7789_hal_t *hal,
                               st7789_handle_t *out);
esp_err_t st7789_set_backlight(st7789_handle_t dev, bool on);
esp_err_t st7789_set_window(st7789_handle_t dev, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1);
esp_err_t st7789_write_pixels(st7789_handle_t dev, const void *rgb565,
                              size_t length_bytes);
/* Returns true only after all driver-owned resources are reclaimed. If an SPI
 * transfer or bus teardown is still pending, the handle remains valid and the
 * caller must retry later. */
bool st7789_deinit(st7789_handle_t dev);

#endif /* QW_ST7789_H */
