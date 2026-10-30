#ifndef QW_HAL_ESP_IDF_H
#define QW_HAL_ESP_IDF_H

#include "qw_hal.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

/* Adapters are value types.  A driver owns the lifetime of the ESP-IDF
 * handle; the adapter only translates status and calls. */
qw_hal_i2c_device_t qw_hal_espidf_i2c_device(i2c_master_dev_handle_t device);
qw_hal_gpio_t qw_hal_espidf_gpio(void);
qw_hal_clock_t qw_hal_espidf_clock(void);
qw_hal_spi_device_t qw_hal_espidf_spi_device(spi_device_handle_t device);
/* Nonblocking teardown. Returns false when the context is busy or an
 * unfinished transaction still belongs to the SPI driver; in that case the
 * caller must not remove/free the bus and may retry later. */
bool qw_hal_espidf_spi_device_deinit(qw_hal_spi_device_t *device);

#endif /* QW_HAL_ESP_IDF_H */
