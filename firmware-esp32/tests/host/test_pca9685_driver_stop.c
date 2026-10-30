#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "actuator_pca9685.h"
#include "qw_hal.h"

typedef struct {
    bool oe_result;
    bool rest_result;
    bool sleep_result;
    unsigned oe_calls;
    unsigned rest_calls;
    unsigned sleep_calls;
} fake_stop_t;

static qw_hal_status_t fake_gpio_write(void *ctx, int pin, bool high)
{
    fake_stop_t *fake = ctx;
    (void)pin;
    (void)high;
    fake->oe_calls++;
    return fake->oe_result ? QW_HAL_OK : QW_HAL_IO_ERROR;
}

static qw_hal_status_t fake_i2c_write(void *ctx, const uint8_t *data,
                                      size_t length, int timeout_ms)
{
    fake_stop_t *fake = ctx;
    (void)timeout_ms;
    assert(data != NULL);
    if (length == 33U && data[0] == 0x06U) {
        fake->rest_calls++;
        return fake->rest_result ? QW_HAL_OK : QW_HAL_IO_ERROR;
    }
    assert(length == 2U && data[0] == 0x00U);
    fake->sleep_calls++;
    return fake->sleep_result ? QW_HAL_OK : QW_HAL_IO_ERROR;
}

static void assert_initialized_case(bool oe, bool rest, bool sleep)
{
    fake_stop_t fake = {
        .oe_result = oe,
        .rest_result = rest,
        .sleep_result = sleep,
    };
    const qw_hal_gpio_t gpio = {
        .ctx = &fake,
        .write = fake_gpio_write,
    };
    const qw_hal_i2c_device_t i2c = {
        .ctx = &fake,
        .write = fake_i2c_write,
    };
    assert(qw_actuator_pca9685_test_force_safe_stop(true, &i2c, &gpio) ==
           (oe && rest && sleep));
    assert(fake.oe_calls == 1U);
    assert(fake.rest_calls == 1U);
    assert(fake.sleep_calls == 1U);
}

int main(void)
{
    for (unsigned mask = 0U; mask < 8U; mask++) {
        assert_initialized_case((mask & 1U) != 0U, (mask & 2U) != 0U,
                                (mask & 4U) != 0U);
    }

    fake_stop_t uninitialized = {.oe_result = true};
    const qw_hal_gpio_t gpio = {
        .ctx = &uninitialized,
        .write = fake_gpio_write,
    };
    const qw_hal_i2c_device_t i2c = {
        .ctx = &uninitialized,
        .write = fake_i2c_write,
    };
    assert(!qw_actuator_pca9685_test_force_safe_stop(false, &i2c, &gpio));
    assert(uninitialized.oe_calls == 1U);
    assert(uninitialized.rest_calls == 0U);
    assert(uninitialized.sleep_calls == 0U);
    return 0;
}
