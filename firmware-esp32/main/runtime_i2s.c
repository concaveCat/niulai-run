#include "runtime_i2s.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static StaticSemaphore_t s_lock_storage;
static SemaphoreHandle_t s_lock;
static qw_i2s_audio_t *s_owner;

static bool ensure_lock(void)
{
    if (s_lock != NULL) {
        return true;
    }
    /* The I2S owner can hand the shared clock domain to another task: the
     * voice task releases the microphone before playback, while app_main can
     * request direct playback. A FreeRTOS mutex cannot be released by a
     * different task because that invokes priority disinheritance. */
    s_lock = xSemaphoreCreateBinaryStatic(&s_lock_storage);
    return s_lock != NULL && xSemaphoreGive(s_lock) == pdTRUE;
}

esp_err_t qw_runtime_i2s_acquire(const qw_i2s_audio_config_t *config,
                                 qw_i2s_audio_t *out)
{
    if (config == NULL || out == NULL || !ensure_lock() ||
        xSemaphoreTake(s_lock, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    const esp_err_t err = qw_i2s_audio_init(config, out);
    if (err != ESP_OK) {
        xSemaphoreGive(s_lock);
    } else {
        s_owner = out;
    }
    return err;
}

void qw_runtime_i2s_release(qw_i2s_audio_t *audio)
{
    if (audio == NULL || s_lock == NULL || s_owner != audio) {
        return;
    }
    qw_i2s_audio_deinit(audio);
    s_owner = NULL;
    xSemaphoreGive(s_lock);
}
