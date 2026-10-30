#ifndef QW_DISPLAY_SERVICE_H
#define QW_DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "st7789.h"

typedef enum {
    QW_DISPLAY_PORTRAIT = 0,
    QW_DISPLAY_LANDSCAPE,
} qw_display_orientation_t;

typedef struct {
    st7789_handle_t panel;
    st7789_config_t config;
    qw_display_orientation_t orientation;
    uint8_t *transfer;
    size_t transfer_bytes;
} qw_display_service_t;

/* The service is the only production path that owns panel framing and
 * RGB565 wire-order conversion.  The ST7789 driver remains a device driver;
 * HUD layout/rendering remains a separate application-facing renderer. */
esp_err_t qw_display_service_init(const st7789_config_t *config,
                                  qw_display_orientation_t orientation,
                                  qw_display_service_t *out);
esp_err_t qw_display_service_set_backlight(qw_display_service_t *service,
                                            bool on);
esp_err_t qw_display_service_present(qw_display_service_t *service,
                                      const uint16_t *framebuffer,
                                      size_t pixel_count);
void qw_display_service_deinit(qw_display_service_t *service);

uint16_t qw_display_service_width(const qw_display_service_t *service);
uint16_t qw_display_service_height(const qw_display_service_t *service);

#endif /* QW_DISPLAY_SERVICE_H */
