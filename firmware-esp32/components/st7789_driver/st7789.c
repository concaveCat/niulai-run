#include "st7789.h"
#include "qw_hal_espidf.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "st7789";
#define QW_ST7789_IO_TIMEOUT_MS 100

struct st7789_device {
    st7789_config_t config;
    spi_device_handle_t spi;
    qw_hal_spi_device_t transport;
    qw_hal_gpio_t gpio;
    qw_hal_clock_t clock;
    bool bus_initialized;
};

static esp_err_t transmit(st7789_handle_t dev, bool command,
                          const void *data, size_t length)
{
    if (dev == NULL || dev->gpio.write == NULL || dev->transport.write == NULL ||
        (length != 0 && data == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (dev->gpio.write(dev->gpio.ctx, dev->config.dc_gpio, !command) !=
        QW_HAL_OK) {
        return ESP_FAIL;
    }
    return dev->transport.write(dev->transport.ctx, data, length,
                                QW_ST7789_IO_TIMEOUT_MS) ==
                   QW_HAL_OK
               ? ESP_OK
               : ESP_FAIL;
}

static esp_err_t send_command(st7789_handle_t dev, uint8_t command,
                               const uint8_t *parameters, size_t count)
{
    esp_err_t err = transmit(dev, true, &command, 1);
    if (err != ESP_OK || count == 0) {
        return err;
    }
    return transmit(dev, false, parameters, count);
}

st7789_config_t st7789_config_default(void)
{
    return (st7789_config_t){
        .host = SPI2_HOST,
        .sclk_gpio = GPIO_NUM_8,
        .mosi_gpio = GPIO_NUM_17,
        .cs_gpio = GPIO_NUM_6,
        .dc_gpio = GPIO_NUM_7,
        .reset_gpio = GPIO_NUM_16,
        .backlight_gpio = GPIO_NUM_5,
        .spi_clock_hz = QW_ST7789_DEFAULT_SPI_HZ,
        .width = QW_ST7789_DEFAULT_WIDTH,
        .height = QW_ST7789_DEFAULT_HEIGHT,
        .x_offset = QW_ST7789_DEFAULT_X_OFFSET,
        .y_offset = QW_ST7789_DEFAULT_Y_OFFSET,
        .pixel_format = QW_ST7789_DEFAULT_PIXEL_FORMAT,
        .madctl = QW_ST7789_DEFAULT_MADCTL,
        .backlight_active_high = true,
    };
}

static esp_err_t st7789_init_internal(const st7789_config_t *config,
                                      const st7789_hal_t *hal,
                                      st7789_handle_t *out)
{
    if (out != NULL) {
        *out = NULL;
    }
    if (config == NULL || out == NULL || config->sclk_gpio < 0 ||
        config->mosi_gpio < 0 || config->cs_gpio < 0 || config->dc_gpio < 0 ||
        config->reset_gpio < 0 || config->backlight_gpio < 0 ||
        config->width == 0 || config->height == 0 || config->spi_clock_hz <= 0 ||
        (uint32_t)config->x_offset + config->width > UINT16_MAX + 1U ||
        (uint32_t)config->y_offset + config->height > UINT16_MAX + 1U) {
        return ESP_ERR_INVALID_ARG;
    }

    st7789_handle_t dev = calloc(1, sizeof(*dev));
    if (dev == NULL) {
        return ESP_ERR_NO_MEM;
    }
    dev->config = *config;
    if (hal != NULL) {
        dev->gpio = hal->gpio;
        dev->clock = hal->clock;
        dev->transport = hal->spi;
        if (dev->gpio.configure_output == NULL || dev->gpio.write == NULL ||
            dev->clock.sleep_ms == NULL || dev->transport.write == NULL) {
            free(dev);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        dev->gpio = qw_hal_espidf_gpio();
        dev->clock = qw_hal_espidf_clock();
    }

    if (dev->gpio.configure_output(
            dev->gpio.ctx, config->dc_gpio, false) != QW_HAL_OK ||
        dev->gpio.configure_output(dev->gpio.ctx, config->reset_gpio, true) !=
            QW_HAL_OK ||
        dev->gpio.configure_output(dev->gpio.ctx, config->backlight_gpio,
                                   false) != QW_HAL_OK) {
        free(dev);
        return ESP_FAIL;
    }
    if (dev->gpio.write(dev->gpio.ctx, config->backlight_gpio,
                        config->backlight_active_high ? false : true) !=
            QW_HAL_OK ||
        dev->gpio.write(dev->gpio.ctx, config->reset_gpio, true) !=
            QW_HAL_OK) {
        free(dev);
        return ESP_FAIL;
    }
    esp_err_t err;

    if (hal != NULL) {
        goto panel_reset;
    }

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config->mosi_gpio,
        .miso_io_num = -1,
        .sclk_io_num = config->sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    err = spi_bus_initialize(config->host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        free(dev);
        return err;
    }
    dev->bus_initialized = true;

    spi_device_interface_config_t device_cfg = {
        .clock_speed_hz = config->spi_clock_hz,
        .mode = 0,
        .spics_io_num = config->cs_gpio,
        .queue_size = 1,
    };
    err = spi_bus_add_device(config->host, &device_cfg, &dev->spi);
    if (err != ESP_OK) {
        spi_bus_free(config->host);
        free(dev);
        return err;
    }
    dev->transport = qw_hal_espidf_spi_device(dev->spi);
    if (dev->transport.ctx == NULL || dev->transport.write == NULL) {
        ESP_LOGE(TAG, "SPI transport allocation failed");
        st7789_deinit(dev);
        return ESP_ERR_NO_MEM;
    }

panel_reset:
    if (dev->gpio.write(dev->gpio.ctx, config->reset_gpio, false) !=
        QW_HAL_OK) {
        (void)st7789_deinit(dev);
        return ESP_FAIL;
    }
    dev->clock.sleep_ms(dev->clock.ctx, 10U);
    if (dev->gpio.write(dev->gpio.ctx, config->reset_gpio, true) !=
        QW_HAL_OK) {
        (void)st7789_deinit(dev);
        return ESP_FAIL;
    }
    dev->clock.sleep_ms(dev->clock.ctx, 120U);

    err = send_command(dev, 0x01, NULL, 0); /* SWRESET */
    if (err == ESP_OK) {
        dev->clock.sleep_ms(dev->clock.ctx, 150U);
        err = send_command(dev, 0x11, NULL, 0); /* SLPOUT */
    }
    if (err == ESP_OK) {
        dev->clock.sleep_ms(dev->clock.ctx, 120U);
        uint8_t format = config->pixel_format;
        err = send_command(dev, 0x3A, &format, 1); /* COLMOD */
    }
    if (err == ESP_OK) {
        uint8_t madctl = config->madctl;
        err = send_command(dev, 0x36, &madctl, 1); /* MADCTL */
    }
    if (err == ESP_OK) {
        err = send_command(dev, 0x21, NULL, 0); /* INVON: common panel candidate */
    }
    if (err == ESP_OK) {
        err = send_command(dev, 0x13, NULL, 0); /* NORON */
    }
    if (err == ESP_OK) {
        err = send_command(dev, 0x29, NULL, 0); /* DISPON */
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialization failed: %s", esp_err_to_name(err));
        if (!st7789_deinit(dev)) {
            *out = dev;
        }
        return err;
    }
    *out = dev;
    return ESP_OK;
}

esp_err_t st7789_init(const st7789_config_t *config, st7789_handle_t *out)
{
    return st7789_init_internal(config, NULL, out);
}

esp_err_t st7789_init_with_hal(const st7789_config_t *config,
                               const st7789_hal_t *hal,
                               st7789_handle_t *out)
{
    return st7789_init_internal(config, hal, out);
}

esp_err_t st7789_set_backlight(st7789_handle_t dev, bool on)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bool level = on ? dev->config.backlight_active_high
                    : !dev->config.backlight_active_high;
    return dev->gpio.write(dev->gpio.ctx, dev->config.backlight_gpio, level) ==
                   QW_HAL_OK
               ? ESP_OK
               : ESP_FAIL;
}

esp_err_t st7789_set_window(st7789_handle_t dev, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1)
{
    if (dev == NULL || x0 > x1 || y0 > y1 || x1 >= dev->config.width ||
        y1 >= dev->config.height) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t x_start = (uint32_t)x0 + dev->config.x_offset;
    uint32_t x_end = (uint32_t)x1 + dev->config.x_offset;
    uint32_t y_start = (uint32_t)y0 + dev->config.y_offset;
    uint32_t y_end = (uint32_t)y1 + dev->config.y_offset;
    if (x_end > UINT16_MAX || y_end > UINT16_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t columns[] = {(uint8_t)(x_start >> 8), (uint8_t)x_start,
                         (uint8_t)(x_end >> 8), (uint8_t)x_end};
    uint8_t rows[] = {(uint8_t)(y_start >> 8), (uint8_t)y_start,
                      (uint8_t)(y_end >> 8), (uint8_t)y_end};
    esp_err_t err = send_command(dev, 0x2A, columns, sizeof(columns));
    if (err == ESP_OK) {
        err = send_command(dev, 0x2B, rows, sizeof(rows));
    }
    if (err == ESP_OK) {
        err = send_command(dev, 0x2C, NULL, 0);
    }
    return err;
}

esp_err_t st7789_write_pixels(st7789_handle_t dev, const void *rgb565,
                              size_t length_bytes)
{
    if (dev == NULL || rgb565 == NULL || length_bytes == 0 ||
        (length_bytes & 1U) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return transmit(dev, false, rgb565, length_bytes);
}

bool st7789_deinit(st7789_handle_t dev)
{
    if (dev == NULL) {
        return true;
    }
    if (dev->gpio.write != NULL) {
        (void)dev->gpio.write(dev->gpio.ctx, dev->config.backlight_gpio,
                              dev->config.backlight_active_high ? false : true);
    }
    bool transport_ready = true;
    if (dev->spi != NULL) {
        transport_ready = qw_hal_espidf_spi_device_deinit(&dev->transport);
    }
    if (dev->spi != NULL && transport_ready) {
        if (spi_bus_remove_device(dev->spi) != ESP_OK) {
            return false;
        }
        dev->spi = NULL;
    }
    if (dev->bus_initialized && transport_ready) {
        if (spi_bus_free(dev->config.host) != ESP_OK) {
            return false;
        }
        dev->bus_initialized = false;
    }
    if (!transport_ready) {
        /* The driver handle owns the transport context and its persistent
         * descriptor/DMA buffer. Keep the whole driver object alive until a
         * later deinit call can reclaim the transaction; freeing dev here
         * would leave the SPI ISR with a dangling context. */
        ESP_LOGE(TAG,
                 "SPI transaction still pending; keeping driver and bus allocated");
        return false;
    }
    free(dev);
    return true;
}
