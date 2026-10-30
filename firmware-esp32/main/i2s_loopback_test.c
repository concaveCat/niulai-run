#include "i2s_loopback_test.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_audio.h"
#include "sdkconfig.h"

#define QW_I2S_LOOPBACK_SAMPLE_RATE_HZ 16000U
#define QW_I2S_LOOPBACK_TONE_HZ 1000U
#define QW_I2S_LOOPBACK_AMPLITUDE_PERCENT 25U
#define QW_I2S_LOOPBACK_BUFFER_FRAMES 256U
#define QW_I2S_LOOPBACK_TEST_BLOCKS 64U
#define QW_I2S_LOOPBACK_PRINT_FRAMES 32U
#define QW_I2S_LOOPBACK_SLOT_COUNT 2U
#define QW_I2S_LOOPBACK_SLOT_BITS 32U
#define QW_I2S_LOOPBACK_BCLK_HZ \
    (QW_I2S_LOOPBACK_SAMPLE_RATE_HZ * QW_I2S_LOOPBACK_SLOT_COUNT * \
     QW_I2S_LOOPBACK_SLOT_BITS)
#define QW_I2S_LOOPBACK_PI 3.14159265358979323846

static const char *TAG = "i2s_loopback";
static int32_t s_tx_buffer[QW_I2S_LOOPBACK_BUFFER_FRAMES *
                           QW_I2S_LOOPBACK_SLOT_COUNT];
static int32_t s_rx_buffer[QW_I2S_LOOPBACK_BUFFER_FRAMES *
                           QW_I2S_LOOPBACK_SLOT_COUNT];

static void fill_sine(double *phase)
{
    const double step = 2.0 * QW_I2S_LOOPBACK_PI *
                        (double)QW_I2S_LOOPBACK_TONE_HZ /
                        (double)QW_I2S_LOOPBACK_SAMPLE_RATE_HZ;
    const double amplitude = (double)INT32_MAX *
                             (double)QW_I2S_LOOPBACK_AMPLITUDE_PERCENT /
                             100.0;
    for (size_t frame = 0U; frame < QW_I2S_LOOPBACK_BUFFER_FRAMES; ++frame) {
        double value = sin(*phase) * amplitude;
        if (value > (double)INT32_MAX) {
            value = (double)INT32_MAX;
        } else if (value < (double)INT32_MIN) {
            value = (double)INT32_MIN;
        }
        const int32_t sample = (int32_t)value;
        s_tx_buffer[frame * QW_I2S_LOOPBACK_SLOT_COUNT] = sample;
        s_tx_buffer[frame * QW_I2S_LOOPBACK_SLOT_COUNT + 1U] = sample;
        *phase += step;
        if (*phase >= 2.0 * QW_I2S_LOOPBACK_PI) {
            *phase -= 2.0 * QW_I2S_LOOPBACK_PI;
        }
    }
}

static void print_raw_frames(const int32_t *samples, size_t frame_count)
{
    const size_t count = frame_count < QW_I2S_LOOPBACK_PRINT_FRAMES
                             ? frame_count
                             : QW_I2S_LOOPBACK_PRINT_FRAMES;
    for (size_t frame = 0U; frame < count; ++frame) {
        printf("I2S_LOOPBACK_RAW frame=%u left=0x%08lx right=0x%08lx\n",
               (unsigned)frame,
               (unsigned long)(uint32_t)
                   samples[frame * QW_I2S_LOOPBACK_SLOT_COUNT],
               (unsigned long)(uint32_t)
                   samples[frame * QW_I2S_LOOPBACK_SLOT_COUNT + 1U]);
    }
    fflush(stdout);
}

esp_err_t qw_i2s_loopback_test(void)
{
    qw_i2s_audio_config_t config = qw_i2s_audio_config_default();
    config.sample_rate_hz = QW_I2S_LOOPBACK_SAMPLE_RATE_HZ;
    config.slot_width = I2S_DATA_BIT_WIDTH_32BIT;
    config.enable_tx = true;
    config.enable_rx = true;

    qw_i2s_audio_t audio;
    esp_err_t err = qw_i2s_audio_init(&config, &audio);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S full-duplex init failed: %s", esp_err_to_name(err));
        return err;
    }

    printf("I2S_LOOPBACK_CONFIG sample_rate_hz=%u bclk_hz=%u format=philips "
           "data_bits=32 slot_bits=32 slot_mode=stereo slot_mask=both "
           "tx_gpio=%d rx_gpio=%d bclk_gpio=%d ws_gpio=%d tone_hz=%u "
           "amplitude_percent=%u\n",
           (unsigned)QW_I2S_LOOPBACK_SAMPLE_RATE_HZ,
           (unsigned)QW_I2S_LOOPBACK_BCLK_HZ, config.tx_data_gpio,
           config.rx_data_gpio, config.bclk_gpio, config.ws_gpio,
           (unsigned)QW_I2S_LOOPBACK_TONE_HZ,
           (unsigned)QW_I2S_LOOPBACK_AMPLITUDE_PERCENT);
    fflush(stdout);

    int32_t rx_min = INT32_MAX;
    int32_t rx_max = INT32_MIN;
    uint64_t rx_nonzero = 0U;
    uint64_t rx_negative = 0U;
    uint64_t rx_positive = 0U;
    uint64_t rx_words = 0U;
    bool printed_raw = false;
    double phase = 0.0;

    for (size_t block = 0U; block < QW_I2S_LOOPBACK_TEST_BLOCKS; ++block) {
        fill_sine(&phase);

        size_t written = 0U;
        err = qw_i2s_audio_write(&audio, s_tx_buffer, sizeof(s_tx_buffer),
                                 &written, 1000U);
        if (err != ESP_OK || written != sizeof(s_tx_buffer)) {
            ESP_LOGE(TAG, "I2S TX write failed: %s written=%u/%u",
                     esp_err_to_name(err), (unsigned)written,
                     (unsigned)sizeof(s_tx_buffer));
            break;
        }

        size_t bytes_read = 0U;
        err = qw_i2s_audio_read(&audio, s_rx_buffer, sizeof(s_rx_buffer),
                                &bytes_read, 1000U);
        if (err != ESP_OK || bytes_read == 0U ||
            bytes_read % (QW_I2S_LOOPBACK_SLOT_COUNT * sizeof(int32_t)) != 0U) {
            ESP_LOGE(TAG, "I2S RX read failed: %s bytes=%u",
                     esp_err_to_name(err), (unsigned)bytes_read);
            break;
        }

        const size_t frame_count =
            bytes_read / (QW_I2S_LOOPBACK_SLOT_COUNT * sizeof(int32_t));
        if (!printed_raw) {
            printf("I2S_LOOPBACK_RAW_BEGIN frames=%u\n",
                   (unsigned)frame_count);
            print_raw_frames(s_rx_buffer, frame_count);
            printf("I2S_LOOPBACK_RAW_END frames=%u\n",
                   (unsigned)(frame_count < QW_I2S_LOOPBACK_PRINT_FRAMES
                                  ? frame_count
                                  : QW_I2S_LOOPBACK_PRINT_FRAMES));
            fflush(stdout);
            printed_raw = true;
        }

        for (size_t word = 0U; word < frame_count * QW_I2S_LOOPBACK_SLOT_COUNT;
             ++word) {
            const int32_t sample = s_rx_buffer[word];
            if (sample < rx_min) {
                rx_min = sample;
            }
            if (sample > rx_max) {
                rx_max = sample;
            }
            if (sample == 0) {
                continue;
            }
            ++rx_nonzero;
            if (sample < 0) {
                ++rx_negative;
            } else {
                ++rx_positive;
            }
        }
        rx_words += frame_count * QW_I2S_LOOPBACK_SLOT_COUNT;
    }

    printf("I2S_LOOPBACK_RESULT tx_gpio=%d rx_gpio=%d blocks=%u rx_words=%u "
           "nonzero=%u negative=%u positive=%u min=%ld max=%ld\n",
           config.tx_data_gpio, config.rx_data_gpio,
           (unsigned)QW_I2S_LOOPBACK_TEST_BLOCKS, (unsigned)rx_words,
           (unsigned)rx_nonzero, (unsigned)rx_negative, (unsigned)rx_positive,
           (long)rx_min, (long)rx_max);
    printf("I2S_LOOPBACK_DONE\n");
    fflush(stdout);
    qw_i2s_audio_deinit(&audio);
    return err;
}
