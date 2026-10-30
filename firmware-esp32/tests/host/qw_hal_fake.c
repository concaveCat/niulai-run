#include "qw_hal_fake.h"

#include <string.h>

static qw_hal_status_t fake_i2c_write(void *ctx, const uint8_t *data,
                                      size_t length, int timeout_ms)
{
    (void)timeout_ms;
    qw_hal_fake_i2c_t *fake = (qw_hal_fake_i2c_t *)ctx;
    if (fake == NULL || data == NULL || length == 0U ||
        length > QW_HAL_FAKE_MAX_BYTES ||
        fake->write_count >= QW_HAL_FAKE_MAX_WRITES) {
        return QW_HAL_INVALID_ARG;
    }
    memcpy(fake->writes[fake->write_count], data, length);
    fake->write_lengths[fake->write_count] = length;
    fake->write_count++;
    return fake->result;
}

static qw_hal_status_t fake_i2c_write_read(void *ctx, const uint8_t *write_data,
                                           size_t write_length,
                                           uint8_t *read_data,
                                           size_t read_length, int timeout_ms)
{
    qw_hal_fake_i2c_t *fake = (qw_hal_fake_i2c_t *)ctx;
    qw_hal_status_t result = fake_i2c_write(ctx, write_data, write_length,
                                            timeout_ms);
    if (result != QW_HAL_OK) {
        return result;
    }
    if (fake == NULL || read_data == NULL || read_length == 0U ||
        read_length > fake->read_length) {
        return QW_HAL_INVALID_ARG;
    }
    memcpy(read_data, fake->read_bytes, read_length);
    return fake->result;
}

qw_hal_i2c_device_t qw_hal_fake_i2c(qw_hal_fake_i2c_t *fake)
{
    return (qw_hal_i2c_device_t){
        .ctx = fake,
        .write = fake_i2c_write,
        .write_read = fake_i2c_write_read,
    };
}

static bool valid_pin(int pin)
{
    return pin >= 0 && pin < 49;
}

static qw_hal_status_t fake_configure_input(void *ctx, int pin, bool pull_up)
{
    (void)pull_up;
    qw_hal_fake_gpio_t *fake = (qw_hal_fake_gpio_t *)ctx;
    if (fake == NULL || !valid_pin(pin)) {
        return QW_HAL_INVALID_ARG;
    }
    fake->configured_input[pin] = true;
    fake->configure_count++;
    return QW_HAL_OK;
}

static qw_hal_status_t fake_configure_output(void *ctx, int pin,
                                             bool initial_high)
{
    qw_hal_fake_gpio_t *fake = (qw_hal_fake_gpio_t *)ctx;
    if (fake == NULL || !valid_pin(pin)) {
        return QW_HAL_INVALID_ARG;
    }
    fake->configured_output[pin] = true;
    fake->levels[pin] = initial_high;
    fake->configure_count++;
    return QW_HAL_OK;
}

static qw_hal_status_t fake_write(void *ctx, int pin, bool high)
{
    qw_hal_fake_gpio_t *fake = (qw_hal_fake_gpio_t *)ctx;
    if (fake == NULL || !valid_pin(pin)) {
        return QW_HAL_INVALID_ARG;
    }
    fake->levels[pin] = high;
    fake->write_count++;
    return QW_HAL_OK;
}

static int fake_read(void *ctx, int pin)
{
    qw_hal_fake_gpio_t *fake = (qw_hal_fake_gpio_t *)ctx;
    return fake == NULL || !valid_pin(pin) ? -1 : fake->levels[pin] ? 1 : 0;
}

qw_hal_gpio_t qw_hal_fake_gpio(qw_hal_fake_gpio_t *fake)
{
    return (qw_hal_gpio_t){
        .ctx = fake,
        .configure_input = fake_configure_input,
        .configure_output = fake_configure_output,
        .write = fake_write,
        .read = fake_read,
    };
}

static int64_t fake_now_us(void *ctx)
{
    qw_hal_fake_clock_t *fake = (qw_hal_fake_clock_t *)ctx;
    return fake == NULL ? 0 : fake->now;
}

static void fake_sleep_ms(void *ctx, uint32_t milliseconds)
{
    qw_hal_fake_clock_t *fake = (qw_hal_fake_clock_t *)ctx;
    if (fake != NULL) {
        fake->slept_ms += milliseconds;
        fake->now += (int64_t)milliseconds * 1000;
    }
}

static void fake_sleep_us(void *ctx, uint32_t microseconds)
{
    qw_hal_fake_clock_t *fake = (qw_hal_fake_clock_t *)ctx;
    if (fake != NULL) {
        fake->now += microseconds;
    }
}

qw_hal_clock_t qw_hal_fake_clock(qw_hal_fake_clock_t *fake)
{
    return (qw_hal_clock_t){
        .ctx = fake,
        .now_us = fake_now_us,
        .sleep_us = fake_sleep_us,
        .sleep_ms = fake_sleep_ms,
    };
}
