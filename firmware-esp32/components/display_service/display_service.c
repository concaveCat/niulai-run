#include "display_service.h"

#include <stdlib.h>
#include <string.h>

#define QW_DISPLAY_MAX_TRANSFER_BYTES 4096U

static bool dimensions_match(const st7789_config_t *config,
                             qw_display_orientation_t orientation)
{
    if (config == NULL) {
        return false;
    }
    if (orientation == QW_DISPLAY_PORTRAIT) {
        return config->width == QW_ST7789_DEFAULT_WIDTH &&
               config->height == QW_ST7789_DEFAULT_HEIGHT &&
               (config->madctl & 0x20U) == 0U;
    }
    return orientation == QW_DISPLAY_LANDSCAPE &&
           config->width == QW_ST7789_DEFAULT_HEIGHT &&
           config->height == QW_ST7789_DEFAULT_WIDTH &&
           (config->madctl & 0x20U) != 0U;
}

esp_err_t qw_display_service_init(const st7789_config_t *config,
                                  qw_display_orientation_t orientation,
                                  qw_display_service_t *out)
{
    if (config == NULL || out == NULL || !dimensions_match(config, orientation)) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->config = *config;
    out->orientation = orientation;
    esp_err_t err = st7789_init(config, &out->panel);
    if (err != ESP_OK) {
        return err;
    }
    out->transfer_bytes = QW_DISPLAY_MAX_TRANSFER_BYTES;
    out->transfer = malloc(out->transfer_bytes);
    if (out->transfer == NULL) {
        if (st7789_deinit(out->panel)) {
            memset(out, 0, sizeof(*out));
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t qw_display_service_set_backlight(qw_display_service_t *service,
                                            bool on)
{
    return service == NULL || service->panel == NULL
               ? ESP_ERR_INVALID_ARG
               : st7789_set_backlight(service->panel, on);
}

uint16_t qw_display_service_width(const qw_display_service_t *service)
{
    return service == NULL ? 0U : service->config.width;
}

uint16_t qw_display_service_height(const qw_display_service_t *service)
{
    return service == NULL ? 0U : service->config.height;
}

esp_err_t qw_display_service_present(qw_display_service_t *service,
                                      const uint16_t *framebuffer,
                                      size_t pixel_count)
{
    if (service == NULL || service->panel == NULL || framebuffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t required = (size_t)service->config.width *
                            service->config.height;
    if (pixel_count < required) {
        return ESP_ERR_INVALID_SIZE;
    }
    const size_t bytes_per_row = (size_t)service->config.width * 2U;
    const size_t rows_per_chunk = QW_DISPLAY_MAX_TRANSFER_BYTES / bytes_per_row;
    if (rows_per_chunk == 0U) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (service->transfer == NULL ||
        service->transfer_bytes < QW_DISPLAY_MAX_TRANSFER_BYTES) {
        return ESP_ERR_NO_MEM;
    }
    for (uint16_t y = 0; y < service->config.height; y += (uint16_t)rows_per_chunk) {
        const size_t remaining_rows = (size_t)service->config.height - y;
        const uint16_t rows = remaining_rows < rows_per_chunk
                                  ? (uint16_t)remaining_rows
                                  : (uint16_t)rows_per_chunk;
        const size_t chunk_pixels = (size_t)rows * service->config.width;
        for (size_t i = 0; i < chunk_pixels; i++) {
            const uint16_t rgb565 = framebuffer[(size_t)y * service->config.width + i];
            service->transfer[2U * i] = (uint8_t)(rgb565 >> 8);
            service->transfer[2U * i + 1U] = (uint8_t)rgb565;
        }
        esp_err_t err = st7789_set_window(
            service->panel, 0, y, (uint16_t)(service->config.width - 1U),
            (uint16_t)(y + rows - 1U));
        if (err == ESP_OK) {
            err = st7789_write_pixels(service->panel, service->transfer,
                                      chunk_pixels * 2U);
        }
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

void qw_display_service_deinit(qw_display_service_t *service)
{
    if (service == NULL) {
        return;
    }
    if (service->panel != NULL) {
        if (!st7789_deinit(service->panel)) {
            /* Keep the panel and transfer ownership so the caller can retry
             * after an in-flight SPI transaction or bus teardown completes. */
            return;
        }
    }
    free(service->transfer);
    memset(service, 0, sizeof(*service));
}
