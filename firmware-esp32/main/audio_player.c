#include "audio_player.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio_command.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "i2s_audio.h"
#include "mp3dec.h"
#include "sdkconfig.h"

#include "esp_log.h"

static const char *TAG = "audio_player";

#define QW_AUDIO_EVENT_PLAY BIT0
#define QW_AUDIO_EVENT_STOP BIT1
#define QW_AUDIO_TASK_STACK_WORDS 8192U
#define QW_AUDIO_TASK_PRIORITY 5U
#define QW_AUDIO_WRITE_TIMEOUT_MS 1000U
#define QW_AUDIO_MAX_PCM_SAMPLES (MAX_NGRAN * MAX_NSAMP * MAX_NCHAN)
#define QW_AUDIO_MP3_ID3_HEADER_BYTES 10U

extern const uint8_t qw_cow_mama_mp3_start[]
    asm("_binary_qw_cow_mama_mp3_start");
extern const uint8_t qw_cow_mama_mp3_end[] asm("_binary_qw_cow_mama_mp3_end");

static EventGroupHandle_t s_events;
static TaskHandle_t s_task;
static int16_t s_pcm[QW_AUDIO_MAX_PCM_SAMPLES];
static int32_t s_i2s_pcm[QW_AUDIO_MAX_PCM_SAMPLES];

typedef enum {
    QW_AUDIO_CONTROL_CONTINUE = 0,
    QW_AUDIO_CONTROL_RESTART,
    QW_AUDIO_CONTROL_STOPPED,
} qw_audio_control_t;

typedef enum {
    QW_AUDIO_RESULT_DONE = 0,
    QW_AUDIO_RESULT_RESTART,
    QW_AUDIO_RESULT_STOPPED,
    QW_AUDIO_RESULT_ERROR,
} qw_audio_result_t;

static size_t skip_id3v2(const uint8_t *data, size_t length)
{
    if (length < QW_AUDIO_MP3_ID3_HEADER_BYTES ||
        memcmp(data, "ID3", 3U) != 0) {
        return 0U;
    }

    /* ID3v2 stores its tag size as four 7-bit bytes. */
    const size_t tag_size = ((size_t)(data[6] & 0x7fU) << 21U) |
                            ((size_t)(data[7] & 0x7fU) << 14U) |
                            ((size_t)(data[8] & 0x7fU) << 7U) |
                            (size_t)(data[9] & 0x7fU);
    size_t total = QW_AUDIO_MP3_ID3_HEADER_BYTES + tag_size;
    if ((data[5] & 0x10U) != 0U) {
        total += QW_AUDIO_MP3_ID3_HEADER_BYTES;
    }
    return total <= length ? total : length;
}

static qw_audio_control_t playback_control(void)
{
    if (s_events == NULL) {
        return QW_AUDIO_CONTROL_CONTINUE;
    }
    const EventBits_t bits = xEventGroupGetBits(s_events);
    if ((bits & QW_AUDIO_EVENT_STOP) != 0U) {
        xEventGroupClearBits(s_events, QW_AUDIO_EVENT_STOP | QW_AUDIO_EVENT_PLAY);
        return QW_AUDIO_CONTROL_STOPPED;
    }
    if ((bits & QW_AUDIO_EVENT_PLAY) != 0U) {
        xEventGroupClearBits(s_events, QW_AUDIO_EVENT_PLAY);
        return QW_AUDIO_CONTROL_RESTART;
    }
    return QW_AUDIO_CONTROL_CONTINUE;
}

static int32_t pcm16_to_i2s(int16_t sample)
{
    int64_t value = (int64_t)sample * 65536LL;
    value = (value * (int64_t)CONFIG_NIULAI_AUDIO_VOLUME_PERCENT) / 100LL;
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

static size_t interleave_stereo(const int16_t *pcm, size_t sample_count,
                                int channels)
{
    if (channels == 2) {
        for (size_t i = 0U; i < sample_count; ++i) {
            s_i2s_pcm[i] = pcm16_to_i2s(pcm[i]);
        }
        return sample_count;
    }

    if (channels == 1 && sample_count <= QW_AUDIO_MAX_PCM_SAMPLES / 2U) {
        for (size_t i = 0U; i < sample_count; ++i) {
            const int32_t sample = pcm16_to_i2s(pcm[i]);
            s_i2s_pcm[2U * i] = sample;
            s_i2s_pcm[2U * i + 1U] = sample;
        }
        return sample_count * 2U;
    }

    return 0U;
}

static qw_audio_result_t play_embedded_once(void)
{
    const uint8_t *const data = qw_cow_mama_mp3_start;
    const size_t data_length =
        (size_t)(qw_cow_mama_mp3_end - qw_cow_mama_mp3_start);
    ESP_LOGI(TAG, "playback start asset=牛来叫妈妈.mp3 bytes=%u",
             (unsigned)data_length);
    size_t offset = skip_id3v2(data, data_length);
    if (offset >= data_length || data_length - offset > (size_t)INT_MAX) {
        ESP_LOGE(TAG, "embedded MP3 has no decodable payload");
        return QW_AUDIO_RESULT_ERROR;
    }

    HMP3Decoder decoder = MP3InitDecoder();
    if (decoder == NULL) {
        ESP_LOGE(TAG, "MP3 decoder allocation failed");
        return QW_AUDIO_RESULT_ERROR;
    }

    const uint8_t *cursor = data + offset;
    int bytes_left = (int)(data_length - offset);
    int sync_offset = MP3FindSyncWord((unsigned char *)cursor, bytes_left);
    if (sync_offset < 0) {
        MP3FreeDecoder(decoder);
        ESP_LOGE(TAG, "embedded MP3 sync word not found");
        return QW_AUDIO_RESULT_ERROR;
    }
    cursor += (size_t)sync_offset;
    bytes_left -= sync_offset;

    MP3FrameInfo first_frame;
    if (MP3GetNextFrameInfo(decoder, &first_frame,
                            (unsigned char *)cursor) != ERR_MP3_NONE ||
        first_frame.layer != 3 || first_frame.samprate <= 0 ||
        (first_frame.nChans != 1 && first_frame.nChans != 2) ||
        first_frame.outputSamps <= 0 ||
        first_frame.outputSamps > QW_AUDIO_MAX_PCM_SAMPLES) {
        MP3FreeDecoder(decoder);
        ESP_LOGE(TAG, "embedded MP3 header is unsupported");
        return QW_AUDIO_RESULT_ERROR;
    }

    qw_i2s_audio_config_t config = qw_i2s_audio_config_default();
    config.sample_rate_hz = (uint32_t)first_frame.samprate;
    config.slot_width = I2S_DATA_BIT_WIDTH_32BIT;
    config.enable_tx = true;
    config.enable_rx = false;

    qw_i2s_audio_t audio;
    esp_err_t err = qw_i2s_audio_init(&config, &audio);
    if (err != ESP_OK) {
        MP3FreeDecoder(decoder);
        ESP_LOGE(TAG, "I2S TX init failed: %s", esp_err_to_name(err));
        return QW_AUDIO_RESULT_ERROR;
    }

    const int sample_rate = first_frame.samprate;
    const int channels = first_frame.nChans;
    unsigned frame_count = 0U;
    bool stopped = false;
    bool failed = false;

    while (bytes_left > 0) {
        const qw_audio_control_t control = playback_control();
        if (control == QW_AUDIO_CONTROL_STOPPED) {
            stopped = true;
            break;
        }
        if (control == QW_AUDIO_CONTROL_RESTART) {
            qw_i2s_audio_deinit(&audio);
            MP3FreeDecoder(decoder);
            return QW_AUDIO_RESULT_RESTART;
        }

        sync_offset = MP3FindSyncWord((unsigned char *)cursor, bytes_left);
        if (sync_offset < 0) {
            break; /* Allow an ID3v1 tag or padding after the final frame. */
        }
        cursor += (size_t)sync_offset;
        bytes_left -= sync_offset;
        if (bytes_left <= 0) {
            break;
        }

        const int bytes_before_decode = bytes_left;
        unsigned char *decode_cursor = (unsigned char *)cursor;
        const int decode_err = MP3Decode(decoder, &decode_cursor, &bytes_left,
                                         s_pcm, 0);
        if (decode_err != ERR_MP3_NONE || bytes_left >= bytes_before_decode) {
            ESP_LOGE(TAG, "MP3 decode failed error=%d frame=%u", decode_err,
                     frame_count);
            failed = true;
            break;
        }
        cursor = decode_cursor;

        MP3FrameInfo frame;
        MP3GetLastFrameInfo(decoder, &frame);
        if (frame.samprate != sample_rate || frame.nChans != channels ||
            frame.outputSamps <= 0 ||
            frame.outputSamps > QW_AUDIO_MAX_PCM_SAMPLES) {
            ESP_LOGE(TAG, "MP3 format changed during playback");
            failed = true;
            break;
        }

        const size_t output_samples =
            interleave_stereo(s_pcm, (size_t)frame.outputSamps, channels);
        if (output_samples == 0U) {
            ESP_LOGE(TAG, "MP3 frame cannot be converted to stereo");
            failed = true;
            break;
        }

        size_t written = 0U;
        err = qw_i2s_audio_write(&audio, s_i2s_pcm,
                                 output_samples * sizeof(s_i2s_pcm[0]),
                                 &written, QW_AUDIO_WRITE_TIMEOUT_MS);
        if (err != ESP_OK || written != output_samples * sizeof(s_i2s_pcm[0])) {
            ESP_LOGE(TAG, "I2S TX write failed: %s written=%u/%u",
                     esp_err_to_name(err), (unsigned)written,
                     (unsigned)(output_samples * sizeof(s_i2s_pcm[0])));
            failed = true;
            break;
        }
        ++frame_count;
    }

    qw_i2s_audio_deinit(&audio);
    MP3FreeDecoder(decoder);

    if (stopped) {
        return QW_AUDIO_RESULT_STOPPED;
    }
    if (failed || frame_count == 0U) {
        return QW_AUDIO_RESULT_ERROR;
    }
    ESP_LOGI(TAG, "playback complete frames=%u rate=%d channels=%d",
             frame_count, sample_rate, channels);
    return QW_AUDIO_RESULT_DONE;
}

static void audio_player_task(void *arg)
{
    (void)arg;
    for (;;) {
        const EventBits_t bits = xEventGroupWaitBits(
            s_events, QW_AUDIO_EVENT_PLAY | QW_AUDIO_EVENT_STOP, pdTRUE,
            pdFALSE, portMAX_DELAY);
        if ((bits & QW_AUDIO_EVENT_STOP) != 0U) {
            continue;
        }
        if ((bits & QW_AUDIO_EVENT_PLAY) == 0U) {
            continue;
        }

        for (;;) {
            const qw_audio_result_t result = play_embedded_once();
            if (result == QW_AUDIO_RESULT_RESTART &&
                (xEventGroupGetBits(s_events) & QW_AUDIO_EVENT_STOP) == 0U) {
                /* A new play request replaces the current stream. */
                continue;
            }
            if (result == QW_AUDIO_RESULT_ERROR) {
                ESP_LOGE(TAG, "playback stopped due to an audio error");
            }
            break;
        }
    }
}

esp_err_t qw_audio_player_init(void)
{
    if (s_task != NULL || s_events != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(audio_player_task, "audio_player",
                    QW_AUDIO_TASK_STACK_WORDS, NULL, QW_AUDIO_TASK_PRIORITY,
                    &s_task) != pdPASS) {
        vEventGroupDelete(s_events);
        s_events = NULL;
        return ESP_ERR_NO_MEM;
    }

    printf("AUDIO_READY command=audio play alias=NIULAI asset=牛来叫妈妈.mp3 "
           "volume_percent=%u\n",
           (unsigned)CONFIG_NIULAI_AUDIO_VOLUME_PERCENT);
    return ESP_OK;
}

esp_err_t qw_audio_player_play_mama(void)
{
    if (s_events == NULL || s_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupClearBits(s_events, QW_AUDIO_EVENT_STOP);
    xEventGroupSetBits(s_events, QW_AUDIO_EVENT_PLAY);
    return ESP_OK;
}

esp_err_t qw_audio_player_play_mama_blocking(void)
{
    if (s_events != NULL || s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return play_embedded_once() == QW_AUDIO_RESULT_DONE ? ESP_OK
                                                         : ESP_FAIL;
}

esp_err_t qw_audio_player_stop(void)
{
    if (s_events == NULL || s_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupClearBits(s_events, QW_AUDIO_EVENT_PLAY);
    xEventGroupSetBits(s_events, QW_AUDIO_EVENT_STOP);
    return ESP_OK;
}

bool qw_audio_player_handle_command(const char *line)
{
    const qw_audio_command_t command = qw_audio_command_parse(line);
    if (command == QW_AUDIO_COMMAND_PLAY_MAMA) {
        const esp_err_t err = qw_audio_player_play_mama();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "play command rejected: %s", esp_err_to_name(err));
        } else {
            printf("AUDIO_COMMAND accepted=play\n");
        }
        return true;
    }
    if (command == QW_AUDIO_COMMAND_STOP) {
        const esp_err_t err = qw_audio_player_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "stop command rejected: %s", esp_err_to_name(err));
        } else {
            printf("AUDIO_COMMAND accepted=stop\n");
        }
        return true;
    }
    return false;
}
