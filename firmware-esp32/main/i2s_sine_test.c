#include "i2s_sine_test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_audio.h"
#include "i2s_sine_command.h"
#include "sdkconfig.h"

#define QW_I2S_SINE_SAMPLE_RATE_HZ 48000U
#define QW_I2S_SINE_DEFAULT_FREQUENCY_HZ 1000U
#define QW_I2S_SINE_BUFFER_FRAMES 256U
#define QW_I2S_SINE_PI 3.14159265358979323846
#define QW_I2S_SINE_UART_LINE_MAX 96U
#define QW_I2S_SINE_UART_TASK_STACK_BYTES 3072U
#define QW_I2S_SINE_UART_TASK_PRIORITY 4U

static const char *TAG = "i2s_sine_test";
static int32_t s_i2s_buffer[QW_I2S_SINE_BUFFER_FRAMES * 2U];

typedef struct {
    uint32_t frequency_hz;
    uint32_t amplitude_percent;
    uint32_t generation;
} qw_i2s_sine_runtime_t;

static qw_i2s_sine_runtime_t s_runtime = {
    .frequency_hz = QW_I2S_SINE_DEFAULT_FREQUENCY_HZ,
    .amplitude_percent = CONFIG_NIULAI_I2S_SINE_AMPLITUDE_PERCENT,
    .generation = 0U,
};
static portMUX_TYPE s_runtime_lock = portMUX_INITIALIZER_UNLOCKED;

static void runtime_snapshot(qw_i2s_sine_runtime_t *out)
{
    portENTER_CRITICAL(&s_runtime_lock);
    *out = s_runtime;
    portEXIT_CRITICAL(&s_runtime_lock);
}

static void runtime_set(uint32_t frequency_hz, uint32_t amplitude_percent)
{
    portENTER_CRITICAL(&s_runtime_lock);
    s_runtime.frequency_hz = frequency_hz;
    s_runtime.amplitude_percent = amplitude_percent;
    ++s_runtime.generation;
    portEXIT_CRITICAL(&s_runtime_lock);
}

static void fill_sine(double *phase, const qw_i2s_sine_runtime_t *runtime)
{
    const double step = 2.0 * QW_I2S_SINE_PI * (double)runtime->frequency_hz /
                        (double)QW_I2S_SINE_SAMPLE_RATE_HZ;
    const double amplitude = (double)INT32_MAX *
                             (double)runtime->amplitude_percent / 100.0;
    for (size_t frame = 0U; frame < QW_I2S_SINE_BUFFER_FRAMES; ++frame) {
        double value = sin(*phase) * amplitude;
        if (value > (double)INT32_MAX) {
            value = (double)INT32_MAX;
        } else if (value < (double)INT32_MIN) {
            value = (double)INT32_MIN;
        }
        const int32_t sample = (int32_t)value;
        s_i2s_buffer[2U * frame] = sample;
        s_i2s_buffer[2U * frame + 1U] = sample;
        *phase += step;
        if (*phase >= 2.0 * QW_I2S_SINE_PI) {
            *phase -= 2.0 * QW_I2S_SINE_PI;
        }
    }
}

static void print_help(void)
{
    printf("I2S tone commands: tone <Hz> <amplitude%%> | freq <Hz> | "
           "amp <amplitude%%> | mute | status | help\n");
    printf("Ranges: frequency 1..20000 Hz, amplitude 0..100%%; "
           "default 1000 Hz / %u%%\n",
           (unsigned)CONFIG_NIULAI_I2S_SINE_AMPLITUDE_PERCENT);
}

static void print_status(void)
{
    qw_i2s_sine_runtime_t runtime;
    runtime_snapshot(&runtime);
    printf("I2S_TONE_STATUS frequency=%u Hz amplitude=%u%% "
           "sample_rate=%u Hz slot=32-bit stereo BCLK=%d WS=%d DIN=%d\n",
           (unsigned)runtime.frequency_hz,
           (unsigned)runtime.amplitude_percent,
           QW_I2S_SINE_SAMPLE_RATE_HZ,
           CONFIG_NIULAI_I2S_BCLK_GPIO,
           CONFIG_NIULAI_I2S_WS_GPIO,
           CONFIG_NIULAI_I2S_TX_GPIO);
}

static void apply_command(const qw_i2s_sine_command_t *command)
{
    qw_i2s_sine_runtime_t current;
    runtime_snapshot(&current);

    switch (command->kind) {
    case QW_I2S_SINE_COMMAND_TONE:
        runtime_set(command->frequency_hz, command->amplitude_percent);
        break;
    case QW_I2S_SINE_COMMAND_FREQUENCY:
        runtime_set(command->frequency_hz, current.amplitude_percent);
        break;
    case QW_I2S_SINE_COMMAND_AMPLITUDE:
        runtime_set(current.frequency_hz, command->amplitude_percent);
        break;
    case QW_I2S_SINE_COMMAND_MUTE:
        runtime_set(current.frequency_hz, 0U);
        break;
    case QW_I2S_SINE_COMMAND_STATUS:
        print_status();
        return;
    case QW_I2S_SINE_COMMAND_HELP:
        print_help();
        return;
    case QW_I2S_SINE_COMMAND_NONE:
    default:
        return;
    }

    print_status();
}

static void i2s_sine_uart_task(void *arg)
{
    (void)arg;
    char line[QW_I2S_SINE_UART_LINE_MAX];
    size_t length = 0U;
    bool overflow = false;

    print_help();
    for (;;) {
        int character = fgetc(stdin);
        if (character == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }

        if (character == '\r' || character == '\n') {
            if (overflow) {
                printf("I2S_TONE_ERROR line_too_long\n");
            } else if (length != 0U) {
                line[length] = '\0';
                qw_i2s_sine_command_t command;
                if (!qw_i2s_sine_command_parse(line, &command)) {
                    printf("I2S_TONE_ERROR invalid_command: %s\n", line);
                } else {
                    apply_command(&command);
                }
            }
            length = 0U;
            overflow = false;
            continue;
        }

        if (overflow) {
            continue;
        }
        if (length < sizeof(line) - 1U) {
            line[length++] = (char)character;
        } else {
            overflow = true;
        }
    }
}

esp_err_t qw_i2s_sine_test(void)
{
    qw_i2s_audio_config_t config = qw_i2s_audio_config_default();
    config.sample_rate_hz = QW_I2S_SINE_SAMPLE_RATE_HZ;
    config.slot_width = I2S_DATA_BIT_WIDTH_32BIT;
    config.enable_tx = true;
    config.enable_rx = false;

    qw_i2s_audio_t audio;
    esp_err_t err = qw_i2s_audio_init(&config, &audio);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX init failed: %s", esp_err_to_name(err));
        return err;
    }

    qw_i2s_sine_runtime_t runtime;
    runtime_snapshot(&runtime);
    ESP_LOGI(TAG,
             "I2S sine diagnostic: tone=%u Hz sample_rate=%u Hz "
             "slot=32-bit stereo amplitude=%u%% BCLK=%d WS=%d DIN=%d",
             (unsigned)runtime.frequency_hz, QW_I2S_SINE_SAMPLE_RATE_HZ,
             (unsigned)runtime.amplitude_percent,
             config.bclk_gpio, config.ws_gpio, config.tx_data_gpio);
    ESP_LOGI(TAG, "Expected waveform: WS=%u Hz BCLK=%u Hz; DIN must toggle",
             QW_I2S_SINE_SAMPLE_RATE_HZ,
             QW_I2S_SINE_SAMPLE_RATE_HZ * 32U * 2U);

    if (xTaskCreate(i2s_sine_uart_task, "i2s_sine_uart",
                   QW_I2S_SINE_UART_TASK_STACK_BYTES, NULL,
                   QW_I2S_SINE_UART_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "I2S tone UART task creation failed");
        qw_i2s_audio_deinit(&audio);
        return ESP_ERR_NO_MEM;
    }

    double phase = 0.0;
    uint32_t last_generation = UINT32_MAX;
    for (;;) {
        runtime_snapshot(&runtime);
        if (runtime.generation != last_generation) {
            phase = 0.0;
            last_generation = runtime.generation;
        }
        fill_sine(&phase, &runtime);
        size_t written = 0U;
        err = qw_i2s_audio_write(&audio, s_i2s_buffer, sizeof(s_i2s_buffer),
                                 &written, 1000U);
        if (err != ESP_OK || written != sizeof(s_i2s_buffer)) {
            ESP_LOGE(TAG, "I2S sine write failed: %s written=%u/%u",
                     esp_err_to_name(err), (unsigned)written,
                     (unsigned)sizeof(s_i2s_buffer));
            break;
        }
    }

    qw_i2s_audio_deinit(&audio);
    return err;
}
