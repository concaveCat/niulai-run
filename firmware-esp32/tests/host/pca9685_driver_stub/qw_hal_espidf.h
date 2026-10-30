#ifndef QW_PCA9685_TEST_HAL_ESP_IDF_H
#define QW_PCA9685_TEST_HAL_ESP_IDF_H

#include "driver/i2c_master.h"
#include "qw_hal.h"

static inline qw_hal_i2c_device_t
qw_hal_espidf_i2c_device(i2c_master_dev_handle_t device)
{
    (void)device;
    return (qw_hal_i2c_device_t){0};
}

static inline qw_hal_gpio_t qw_hal_espidf_gpio(void)
{
    return (qw_hal_gpio_t){0};
}

static inline qw_hal_clock_t qw_hal_espidf_clock(void)
{
    return (qw_hal_clock_t){0};
}

#endif
