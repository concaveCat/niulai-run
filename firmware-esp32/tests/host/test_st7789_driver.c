#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "qw_hal_espidf.h"
#include "st7789.h"

typedef struct {
    bool transport_allocation_failure;
    bool pending_deinit;
    qw_hal_status_t write_status;
    int bus_initialize_calls;
    int bus_add_calls;
    int bus_remove_calls;
    int bus_free_calls;
} fake_state_t;

static fake_state_t s_state;
static int s_spi_token;

static qw_hal_status_t fake_gpio_configure_output(void *ctx, int pin,
                                                   bool initial_high)
{
    (void)ctx;
    (void)pin;
    (void)initial_high;
    return QW_HAL_OK;
}

static qw_hal_status_t fake_gpio_write(void *ctx, int pin, bool high)
{
    (void)ctx;
    (void)pin;
    (void)high;
    return QW_HAL_OK;
}

static void fake_sleep_ms(void *ctx, uint32_t milliseconds)
{
    (void)ctx;
    (void)milliseconds;
}

static qw_hal_status_t fake_spi_write(void *ctx, const uint8_t *data,
                                      size_t length, int timeout_ms)
{
    (void)ctx;
    (void)data;
    (void)length;
    (void)timeout_ms;
    return s_state.write_status;
}

qw_hal_gpio_t qw_hal_espidf_gpio(void)
{
    return (qw_hal_gpio_t){
        .configure_output = fake_gpio_configure_output,
        .write = fake_gpio_write,
    };
}

qw_hal_clock_t qw_hal_espidf_clock(void)
{
    return (qw_hal_clock_t){.sleep_ms = fake_sleep_ms};
}

qw_hal_spi_device_t qw_hal_espidf_spi_device(spi_device_handle_t device)
{
    (void)device;
    if (s_state.transport_allocation_failure) {
        return (qw_hal_spi_device_t){0};
    }
    return (qw_hal_spi_device_t){
        .ctx = &s_state,
        .write = fake_spi_write,
    };
}

bool qw_hal_espidf_spi_device_deinit(qw_hal_spi_device_t *device)
{
    if (device == NULL || device->ctx == NULL) {
        return true;
    }
    if (s_state.pending_deinit) {
        return false;
    }
    *device = (qw_hal_spi_device_t){0};
    return true;
}

esp_err_t spi_bus_initialize(spi_host_device_t host,
                              const spi_bus_config_t *config, int dma_chan)
{
    (void)host;
    (void)config;
    (void)dma_chan;
    s_state.bus_initialize_calls++;
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t host,
                              const spi_device_interface_config_t *config,
                              spi_device_handle_t *device)
{
    (void)host;
    (void)config;
    s_state.bus_add_calls++;
    *device = &s_spi_token;
    return ESP_OK;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t device)
{
    assert(device == &s_spi_token);
    s_state.bus_remove_calls++;
    return ESP_OK;
}

esp_err_t spi_bus_free(spi_host_device_t host)
{
    (void)host;
    s_state.bus_free_calls++;
    return ESP_OK;
}

static void reset_state(void)
{
    s_state = (fake_state_t){.write_status = QW_HAL_OK};
}

static st7789_config_t config(void)
{
    st7789_config_t value = st7789_config_default();
    value.x_offset = 0U;
    return value;
}

static void test_transport_allocation_failure_is_reported(void)
{
    reset_state();
    s_state.transport_allocation_failure = true;
    st7789_handle_t panel = NULL;
    const st7789_config_t panel_config = config();

    assert(st7789_init(&panel_config, &panel) == ESP_ERR_NO_MEM);
    assert(panel == NULL);
    assert(s_state.bus_remove_calls == 1);
    assert(s_state.bus_free_calls == 1);
}

static void test_pending_initialization_returns_cleanup_handle(void)
{
    reset_state();
    s_state.write_status = QW_HAL_TIMEOUT;
    s_state.pending_deinit = true;
    st7789_handle_t panel = NULL;
    const st7789_config_t panel_config = config();

    assert(st7789_init(&panel_config, &panel) == ESP_FAIL);
    assert(panel != NULL);
    assert(s_state.bus_remove_calls == 0);
    assert(s_state.bus_free_calls == 0);

    s_state.pending_deinit = false;
    assert(st7789_deinit(panel));
    assert(s_state.bus_remove_calls == 1);
    assert(s_state.bus_free_calls == 1);
}

static void test_pending_deinit_preserves_handle_for_retry(void)
{
    reset_state();
    st7789_handle_t panel = NULL;
    const st7789_config_t panel_config = config();
    assert(st7789_init(&panel_config, &panel) == ESP_OK);

    s_state.pending_deinit = true;
    assert(!st7789_deinit(panel));
    assert(s_state.bus_remove_calls == 0);
    assert(s_state.bus_free_calls == 0);

    s_state.pending_deinit = false;
    assert(st7789_deinit(panel));
    assert(s_state.bus_remove_calls == 1);
    assert(s_state.bus_free_calls == 1);
}

int main(void)
{
    test_transport_allocation_failure_is_reported();
    test_pending_initialization_returns_cleanup_handle();
    test_pending_deinit_preserves_handle_for_retry();
    return 0;
}
