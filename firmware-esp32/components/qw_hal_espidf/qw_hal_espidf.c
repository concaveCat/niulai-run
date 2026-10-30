#include "qw_hal_espidf.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "qw_hal_i2c";
static uint32_t s_i2c_error_count;
static int64_t s_i2c_last_error_log_us;

static void log_i2c_error(const char *operation, size_t length,
                          int timeout_ms, esp_err_t err)
{
    s_i2c_error_count++;
    const int64_t now_us = esp_timer_get_time();
    if (s_i2c_error_count == 1U ||
        now_us - s_i2c_last_error_log_us >= 1000000LL) {
        s_i2c_last_error_log_us = now_us;
        ESP_LOGE(TAG,
                 "I2C error op=%s len=%u timeout_ms=%d err=%s(0x%x) count=%u",
                 operation, (unsigned)length, timeout_ms, esp_err_to_name(err),
                 (unsigned)err, (unsigned)s_i2c_error_count);
    }
}

static qw_hal_status_t status_from_esp(esp_err_t err)
{
    if (err == ESP_OK) {
        return QW_HAL_OK;
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return QW_HAL_INVALID_ARG;
    }
    if (err == ESP_ERR_TIMEOUT) {
        return QW_HAL_TIMEOUT;
    }
    return QW_HAL_IO_ERROR;
}

static qw_hal_status_t i2c_write(void *ctx, const uint8_t *data,
                                 size_t length, int timeout_ms)
{
    if (ctx == NULL || data == NULL || length == 0U) {
        return QW_HAL_INVALID_ARG;
    }
    const esp_err_t err = i2c_master_transmit(
        (i2c_master_dev_handle_t)ctx, data, length, timeout_ms);
    if (err != ESP_OK) {
        log_i2c_error("write", length, timeout_ms, err);
    }
    return status_from_esp(err);
}

static qw_hal_status_t i2c_write_read(void *ctx, const uint8_t *write_data,
                                      size_t write_length, uint8_t *read_data,
                                      size_t read_length, int timeout_ms)
{
    if (ctx == NULL || write_data == NULL || write_length == 0U ||
        read_data == NULL || read_length == 0U) {
        return QW_HAL_INVALID_ARG;
    }
    const esp_err_t err = i2c_master_transmit_receive(
        (i2c_master_dev_handle_t)ctx, write_data, write_length, read_data,
        read_length, timeout_ms);
    if (err != ESP_OK) {
        log_i2c_error("write_read", write_length + read_length, timeout_ms,
                      err);
    }
    return status_from_esp(err);
}

qw_hal_i2c_device_t qw_hal_espidf_i2c_device(i2c_master_dev_handle_t device)
{
    return (qw_hal_i2c_device_t){
        .ctx = device,
        .write = i2c_write,
        .write_read = i2c_write_read,
    };
}

static qw_hal_status_t gpio_configure_input(void *ctx, int pin, bool pull_up)
{
    (void)ctx;
    if (pin < 0 || pin >= 49) {
        return QW_HAL_INVALID_ARG;
    }
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return status_from_esp(gpio_config(&config));
}

static qw_hal_status_t gpio_configure_output(void *ctx, int pin,
                                             bool initial_high)
{
    (void)ctx;
    if (pin < 0 || pin >= 49) {
        return QW_HAL_INVALID_ARG;
    }
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err == ESP_OK) {
        err = gpio_set_level((gpio_num_t)pin, initial_high ? 1 : 0);
    }
    return status_from_esp(err);
}

static qw_hal_status_t gpio_write(void *ctx, int pin, bool high)
{
    (void)ctx;
    return pin < 0 || pin >= 49 ? QW_HAL_INVALID_ARG
                   : status_from_esp(gpio_set_level((gpio_num_t)pin,
                                                    high ? 1 : 0));
}

static int gpio_read(void *ctx, int pin)
{
    (void)ctx;
    return pin < 0 || pin >= 49 ? -1 : gpio_get_level((gpio_num_t)pin);
}

qw_hal_gpio_t qw_hal_espidf_gpio(void)
{
    return (qw_hal_gpio_t){
        .ctx = NULL,
        .configure_input = gpio_configure_input,
        .configure_output = gpio_configure_output,
        .write = gpio_write,
        .read = gpio_read,
    };
}

static int64_t clock_now_us(void *ctx)
{
    (void)ctx;
    return esp_timer_get_time();
}

static void clock_sleep_ms(void *ctx, uint32_t milliseconds)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

static void clock_sleep_us(void *ctx, uint32_t microseconds)
{
    (void)ctx;
    esp_rom_delay_us(microseconds);
}

qw_hal_clock_t qw_hal_espidf_clock(void)
{
    return (qw_hal_clock_t){
        .ctx = NULL,
        .now_us = clock_now_us,
        .sleep_us = clock_sleep_us,
        .sleep_ms = clock_sleep_ms,
    };
}

typedef struct {
    spi_device_handle_t device;
    SemaphoreHandle_t mutex;
    spi_transaction_t transaction;
    uint8_t *tx_buffer;
    bool pending;
} qw_espidf_spi_context_t;

static void spi_release_buffer(qw_espidf_spi_context_t *spi)
{
    free(spi->tx_buffer);
    spi->tx_buffer = NULL;
    spi->pending = false;
    memset(&spi->transaction, 0, sizeof(spi->transaction));
}

static esp_err_t spi_reclaim_pending(qw_espidf_spi_context_t *spi,
                                     TickType_t wait)
{
    if (!spi->pending) {
        return ESP_OK;
    }
    spi_transaction_t *completed = NULL;
    esp_err_t err = spi_device_get_trans_result(spi->device, &completed, wait);
    if (err == ESP_OK) {
        if (completed != &spi->transaction) {
            return ESP_ERR_INVALID_STATE;
        }
        spi_release_buffer(spi);
    }
    return err;
}

static qw_hal_status_t spi_write(void *ctx, const uint8_t *data,
                                 size_t length, int timeout_ms)
{
    if (ctx == NULL || data == NULL || length == 0U) {
        return QW_HAL_INVALID_ARG;
    }
    if (length > SIZE_MAX / 8U) {
        return QW_HAL_INVALID_ARG;
    }
    qw_espidf_spi_context_t *spi = ctx;
    TickType_t wait = timeout_ms < 0 ? portMAX_DELAY :
                                      pdMS_TO_TICKS((uint32_t)timeout_ms);
    if (xSemaphoreTake(spi->mutex, wait) != pdTRUE) {
        return QW_HAL_TIMEOUT;
    }

    /* A previous timeout owns persistent storage until the driver returns it.
     * Do not queue another transaction or free that storage while it is in
     * flight; a permanently stalled host therefore fails closed and returns
     * to the caller instead of blocking forever. */
    esp_err_t err = spi_reclaim_pending(spi, 0);
    if (err == ESP_ERR_TIMEOUT) {
        xSemaphoreGive(spi->mutex);
        return QW_HAL_NOT_READY;
    }
    if (err != ESP_OK) {
        xSemaphoreGive(spi->mutex);
        return status_from_esp(err);
    }

    spi->tx_buffer = heap_caps_malloc(length, MALLOC_CAP_DMA);
    if (spi->tx_buffer == NULL) {
        xSemaphoreGive(spi->mutex);
        return QW_HAL_IO_ERROR;
    }
    memcpy(spi->tx_buffer, data, length);
    spi->transaction.length = length * 8U;
    spi->transaction.tx_buffer = spi->tx_buffer;
    err = spi_device_queue_trans(spi->device, &spi->transaction, wait);
    if (err != ESP_OK) {
        spi_release_buffer(spi);
        xSemaphoreGive(spi->mutex);
        return status_from_esp(err);
    }
    spi->pending = true;
    err = spi_reclaim_pending(spi, wait);
    if (err == ESP_OK) {
        xSemaphoreGive(spi->mutex);
        return QW_HAL_OK;
    }
    /* Keep descriptor and DMA buffer owned by this context. The next call
     * gets a bounded chance to reclaim it; no stack lifetime is exposed to
     * the SPI ISR/DMA engine. */
    xSemaphoreGive(spi->mutex);
    return err == ESP_ERR_TIMEOUT ? QW_HAL_TIMEOUT : status_from_esp(err);
}

qw_hal_spi_device_t qw_hal_espidf_spi_device(spi_device_handle_t device)
{
    qw_espidf_spi_context_t *spi = calloc(1, sizeof(*spi));
    if (spi == NULL) {
        return (qw_hal_spi_device_t){0};
    }
    spi->device = device;
    spi->mutex = xSemaphoreCreateMutex();
    if (spi->mutex == NULL) {
        free(spi);
        return (qw_hal_spi_device_t){0};
    }
    return (qw_hal_spi_device_t){
        .ctx = spi,
        .write = spi_write,
    };
}

bool qw_hal_espidf_spi_device_deinit(qw_hal_spi_device_t *device)
{
    if (device == NULL || device->ctx == NULL) {
        return true;
    }
    qw_espidf_spi_context_t *spi = device->ctx;
    /* Teardown must not wait behind a caller that intentionally selected an
     * unbounded transfer timeout. The owner can retry deinit later. */
    if (xSemaphoreTake(spi->mutex, 0) != pdTRUE) {
        return false;
    }
    const esp_err_t err = spi_reclaim_pending(spi, 0);
    if (err != ESP_OK) {
        xSemaphoreGive(spi->mutex);
        return false;
    }
    xSemaphoreGive(spi->mutex);
    vSemaphoreDelete(spi->mutex);
    free(spi);
    *device = (qw_hal_spi_device_t){0};
    return true;
}
