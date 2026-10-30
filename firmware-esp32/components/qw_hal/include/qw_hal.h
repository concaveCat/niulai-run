#ifndef QW_HAL_H
#define QW_HAL_H

/* Small, deterministic hardware boundary used by replaceable device
 * drivers.  It intentionally does not model all ESP-IDF functionality:
 * RTOS scheduling, BLE, logging and application policy stay above this
 * boundary. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    QW_HAL_OK = 0,
    QW_HAL_INVALID_ARG,
    QW_HAL_NOT_READY,
    QW_HAL_IO_ERROR,
    QW_HAL_TIMEOUT,
} qw_hal_status_t;

const char *qw_hal_status_name(qw_hal_status_t status);

typedef struct {
    void *ctx;
    qw_hal_status_t (*write)(void *ctx, const uint8_t *data, size_t length,
                             int timeout_ms);
    qw_hal_status_t (*write_read)(void *ctx, const uint8_t *write_data,
                                  size_t write_length, uint8_t *read_data,
                                  size_t read_length, int timeout_ms);
} qw_hal_i2c_device_t;

typedef struct {
    void *ctx;
    qw_hal_status_t (*configure_input)(void *ctx, int pin, bool pull_up);
    qw_hal_status_t (*configure_output)(void *ctx, int pin, bool initial_high);
    qw_hal_status_t (*write)(void *ctx, int pin, bool high);
    int (*read)(void *ctx, int pin);
} qw_hal_gpio_t;

typedef struct {
    void *ctx;
    int64_t (*now_us)(void *ctx);
    void (*sleep_us)(void *ctx, uint32_t microseconds);
    void (*sleep_ms)(void *ctx, uint32_t milliseconds);
} qw_hal_clock_t;

/* SPI transport is deliberately byte-oriented.  Chip-select ownership and
 * command/data selection remain in the display driver. */
typedef struct {
    void *ctx;
    qw_hal_status_t (*write)(void *ctx, const uint8_t *data, size_t length,
                             int timeout_ms);
} qw_hal_spi_device_t;

#endif /* QW_HAL_H */
