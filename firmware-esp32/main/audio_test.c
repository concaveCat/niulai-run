#include "audio_test.h"

#include <math.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_audio.h"

static const char *TAG = "audio_test";

#define QW_AUDIO_SAMPLE_RATE_HZ 48000U
#define QW_AUDIO_BUFFER_FRAMES 256U
#define QW_AUDIO_SILENCE_SECONDS 2U
#define QW_AUDIO_NOTE_AMPLITUDE 2048.0f
#define QW_AUDIO_PI 3.14159265358979323846f

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
} qw_audio_note_t;

/* Public-domain Ode to Joy motif, transposed to a safe test-register range. */
static const qw_audio_note_t melody[] = {
    {330, 300}, {330, 300}, {349, 300}, {392, 300},
    {392, 300}, {349, 300}, {330, 300}, {294, 300},
    {262, 300}, {262, 300}, {294, 300}, {330, 300},
    {330, 450}, {294, 150}, {294, 600},
};

static int32_t audio_buffer[QW_AUDIO_BUFFER_FRAMES * 2];

static void fill_note(int32_t *buffer, uint16_t frequency_hz, float *phase)
{
    const float step = 2.0f * QW_AUDIO_PI * (float)frequency_hz /
                       (float)QW_AUDIO_SAMPLE_RATE_HZ;
    for (size_t frame = 0; frame < QW_AUDIO_BUFFER_FRAMES; ++frame) {
        int32_t sample =
            (int32_t)(sinf(*phase) * QW_AUDIO_NOTE_AMPLITUDE) << 12;
        buffer[frame * 2] = sample;
        buffer[frame * 2 + 1] = sample;
        *phase += step;
        if (*phase >= 2.0f * QW_AUDIO_PI) {
            *phase -= 2.0f * QW_AUDIO_PI;
        }
    }
}

static void fill_silence(int32_t *buffer)
{
    for (size_t i = 0; i < QW_AUDIO_BUFFER_FRAMES * 2; ++i) {
        buffer[i] = 0;
    }
}

static esp_err_t write_seconds(qw_i2s_audio_t *audio,
                               const int32_t *buffer, uint32_t seconds)
{
    const uint32_t writes = (QW_AUDIO_SAMPLE_RATE_HZ * seconds) /
                            QW_AUDIO_BUFFER_FRAMES;
    for (uint32_t i = 0; i < writes; ++i) {
        size_t written = 0;
        esp_err_t err = qw_i2s_audio_write(
            audio, buffer, QW_AUDIO_BUFFER_FRAMES * 2 * sizeof(int32_t),
            &written, 1000);
        if (err != ESP_OK) return err;
        if (written != QW_AUDIO_BUFFER_FRAMES * 2 * sizeof(int32_t)) {
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}

static esp_err_t write_note(qw_i2s_audio_t *audio, const qw_audio_note_t *note,
                            float *phase)
{
    const uint32_t frames = (QW_AUDIO_SAMPLE_RATE_HZ * note->duration_ms) /
                            1000U;
    const uint32_t writes = (frames + QW_AUDIO_BUFFER_FRAMES - 1U) /
                            QW_AUDIO_BUFFER_FRAMES;
    for (uint32_t i = 0; i < writes; ++i) {
        fill_note(audio_buffer, note->frequency_hz, phase);
        size_t written = 0;
        esp_err_t err = qw_i2s_audio_write(
            audio, audio_buffer, sizeof(audio_buffer), &written, 1000);
        if (err != ESP_OK) return err;
        if (written != sizeof(audio_buffer)) return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t qw_audio_tone_test(void)
{
    qw_i2s_audio_config_t config = qw_i2s_audio_config_default();
    config.sample_rate_hz = QW_AUDIO_SAMPLE_RATE_HZ;
    config.enable_tx = true;
    config.enable_rx = false;

    qw_i2s_audio_t audio;
    esp_err_t err = qw_i2s_audio_init(&config, &audio);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "MAX98357A melody test: public-domain Ode to Joy motif, "
             "48 kHz, 32-bit stereo, DIN=%d BCLK=%d WS=%d",
             config.tx_data_gpio, config.bclk_gpio, config.ws_gpio);

    for (;;) {
        float phase = 0.0f;
        ESP_LOGI(TAG, "melody on: Ode to Joy motif");
        for (size_t i = 0; i < sizeof(melody) / sizeof(melody[0]); ++i) {
            err = write_note(&audio, &melody[i], &phase);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "melody write failed at note %u: %s",
                         (unsigned)i, esp_err_to_name(err));
                break;
            }
        }
        if (err != ESP_OK) break;

        fill_silence(audio_buffer);
        ESP_LOGI(TAG, "silence for %u seconds", QW_AUDIO_SILENCE_SECONDS);
        err = write_seconds(&audio, audio_buffer, QW_AUDIO_SILENCE_SECONDS);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "silence write failed: %s", esp_err_to_name(err));
            break;
        }
    }

    qw_i2s_audio_deinit(&audio);
    return err;
}
