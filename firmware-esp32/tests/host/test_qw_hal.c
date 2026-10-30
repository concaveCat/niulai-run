#include <assert.h>
#include <string.h>

#include "qw_hal_fake.h"

static void test_i2c(void)
{
    qw_hal_fake_i2c_t storage = {.result = QW_HAL_OK};
    storage.read_bytes[0] = 0x42;
    storage.read_length = 1U;
    qw_hal_i2c_device_t device = qw_hal_fake_i2c(&storage);
    const uint8_t register_address = 0x0f;
    uint8_t value = 0;
    assert(device.write_read(device.ctx, &register_address, 1U, &value, 1U,
                             20) == QW_HAL_OK);
    assert(value == 0x42);
    assert(storage.write_count == 1U);
    assert(storage.write_lengths[0] == 1U);
    assert(storage.writes[0][0] == register_address);
}

static void test_gpio(void)
{
    qw_hal_fake_gpio_t storage = {0};
    qw_hal_gpio_t gpio = qw_hal_fake_gpio(&storage);
    assert(gpio.configure_output(gpio.ctx, 3, true) == QW_HAL_OK);
    assert(gpio.read(gpio.ctx, 3) == 1);
    assert(gpio.write(gpio.ctx, 3, false) == QW_HAL_OK);
    assert(gpio.read(gpio.ctx, 3) == 0);
    assert(gpio.configure_input(gpio.ctx, 4, true) == QW_HAL_OK);
    assert(storage.configure_count == 2U);
    assert(storage.write_count == 1U);
}

static void test_clock(void)
{
    qw_hal_fake_clock_t storage = {.now = 1000};
    qw_hal_clock_t clock = qw_hal_fake_clock(&storage);
    assert(clock.now_us(clock.ctx) == 1000);
    clock.sleep_ms(clock.ctx, 7U);
    assert(clock.now_us(clock.ctx) == 8000);
    assert(storage.slept_ms == 7U);
    clock.sleep_us(clock.ctx, 500U);
    assert(clock.now_us(clock.ctx) == 8500);
}

int main(void)
{
    test_i2c();
    test_gpio();
    test_clock();
    assert(strcmp(qw_hal_status_name(QW_HAL_TIMEOUT), "timeout") == 0);
    return 0;
}
