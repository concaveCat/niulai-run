#ifndef QW_HAL_FAKE_H
#define QW_HAL_FAKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "qw_hal.h"

#define QW_HAL_FAKE_MAX_BYTES 64U
#define QW_HAL_FAKE_MAX_WRITES 16U

typedef struct {
    size_t write_count;
    size_t write_lengths[QW_HAL_FAKE_MAX_WRITES];
    uint8_t writes[QW_HAL_FAKE_MAX_WRITES][QW_HAL_FAKE_MAX_BYTES];
    uint8_t read_bytes[QW_HAL_FAKE_MAX_BYTES];
    size_t read_length;
    qw_hal_status_t result;
} qw_hal_fake_i2c_t;

typedef struct {
    bool configured_input[49];
    bool configured_output[49];
    bool levels[49];
    size_t configure_count;
    size_t write_count;
} qw_hal_fake_gpio_t;

typedef struct {
    int64_t now;
    uint32_t slept_ms;
} qw_hal_fake_clock_t;

qw_hal_i2c_device_t qw_hal_fake_i2c(qw_hal_fake_i2c_t *fake);
qw_hal_gpio_t qw_hal_fake_gpio(qw_hal_fake_gpio_t *fake);
qw_hal_clock_t qw_hal_fake_clock(qw_hal_fake_clock_t *fake);

#endif /* QW_HAL_FAKE_H */
