#include "voice_keyword_test.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_player.h"
#include "audio_command.h"
#include "i2s_audio.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "runtime_i2s.h"
#include "sdkconfig.h"
#include "voice_keyword.h"

static const char *TAG = "voice_keyword";

#define QW_VOICE_I2S_READ_FRAMES 160U
#define QW_VOICE_TASK_STACK_WORDS 8192U
#define QW_VOICE_TASK_PRIORITY 5U
#define QW_VOICE_NVS_NAMESPACE "voice_kws"
#define QW_VOICE_NVS_KEY "template"
#define QW_VOICE_NVS_MAGIC 0x51574b53U /* QWKS */
#define QW_VOICE_CAPTURE_SECONDS 2U
#define QW_VOICE_CAPTURE_SAMPLE_COUNT \
    (QW_VOICE_SAMPLE_RATE_HZ * QW_VOICE_CAPTURE_SECONDS)
#define QW_VOICE_CAPTURE_HEADER_BYTES 20U
#define QW_VOICE_CAPTURE_CHUNK_SAMPLES 256U
#define QW_VOICE_RAW_FRAME_COUNT 32U
#define QW_VOICE_POST_PLAYBACK_SETTLE_FRAMES 100U /* 1 s at 10 ms/frame */
#define QW_VOICE_I2S_SLOT_BITS 32U
#define QW_VOICE_I2S_SLOT_COUNT 2U
#define QW_VOICE_BCLK_HZ \
    (QW_VOICE_SAMPLE_RATE_HZ * QW_VOICE_I2S_SLOT_BITS * QW_VOICE_I2S_SLOT_COUNT)
#define QW_VOICE_ENROLL_BEEP_SAMPLE_RATE_HZ 16000U
#define QW_VOICE_ENROLL_BEEP_FREQUENCY_HZ 1000U
#define QW_VOICE_ENROLL_BEEP_DURATION_MS 300U
#define QW_VOICE_ENROLL_BEEP_AMPLITUDE_PERCENT 10U
#define QW_VOICE_ENROLL_BEEP_CHUNK_FRAMES 256U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t template_mask;
    qw_voice_template_t templates[QW_VOICE_MAX_SPEAKERS];
} qw_voice_nvs_blob_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    qw_voice_template_t template_data;
} qw_voice_legacy_nvs_blob_t;

static qw_i2s_audio_t s_audio;
static qw_voice_keyword_state_t s_matchers[QW_VOICE_MAX_SPEAKERS];
static qw_voice_segmenter_t s_enrollment_segmenter;
static qw_voice_template_accumulator_t s_enrollment_accumulator;
static qw_voice_template_t s_templates[QW_VOICE_MAX_SPEAKERS];
static TaskHandle_t s_task;
static volatile bool s_template_loaded[QW_VOICE_MAX_SPEAKERS];
static volatile bool s_enrollment_requested;
static volatile uint8_t s_enrollment_speaker;
static volatile bool s_play_requested;
static volatile bool s_capture_requested;
static volatile bool s_raw_requested;
static volatile bool s_clear_requested;
static bool s_enrolling;
static uint16_t s_enrollment_pause_frames;
static bool s_capture_active;
static size_t s_capture_count;
static int16_t s_capture_samples[QW_VOICE_CAPTURE_SAMPLE_COUNT];
static qw_i2s_audio_config_t s_audio_config;
static uint16_t s_post_playback_settle_frames;

static bool init_microphone_i2s(void);
static void play_mama_after_match(void);
static void play_mama_direct_once(void);
static void reset_matcher(void);

static bool play_enrollment_start_beep(void)
{
    static const int16_t sine_table[16] = {
        0, 12539, 23170, 30273, 32767, 30273, 23170, 12539,
        0, -12539, -23170, -30273, -32767, -30273, -23170, -12539,
    };
    const size_t total_frames =
        (QW_VOICE_ENROLL_BEEP_SAMPLE_RATE_HZ *
         QW_VOICE_ENROLL_BEEP_DURATION_MS) /
        1000U;
    int32_t samples[QW_VOICE_ENROLL_BEEP_CHUNK_FRAMES * 2U];
    qw_i2s_audio_t beep_audio;
    qw_i2s_audio_config_t beep_config = qw_i2s_audio_config_default();
    beep_config.sample_rate_hz = QW_VOICE_ENROLL_BEEP_SAMPLE_RATE_HZ;
    beep_config.slot_width = I2S_DATA_BIT_WIDTH_32BIT;
    beep_config.enable_tx = true;
    beep_config.enable_rx = false;

    /* The microphone and speaker share BCLK/WS, so the RX channel must be
     * released before opening the short TX-only cue. */
    qw_runtime_i2s_release(&s_audio);
    esp_err_t err = qw_runtime_i2s_acquire(&beep_config, &beep_audio);
    if (err == ESP_OK) {
        for (size_t offset = 0U; offset < total_frames && err == ESP_OK;
             offset += QW_VOICE_ENROLL_BEEP_CHUNK_FRAMES) {
            size_t chunk = total_frames - offset;
            if (chunk > QW_VOICE_ENROLL_BEEP_CHUNK_FRAMES) {
                chunk = QW_VOICE_ENROLL_BEEP_CHUNK_FRAMES;
            }
            for (size_t frame = 0U; frame < chunk; ++frame) {
                const int64_t scaled =
                    (int64_t)INT32_MAX * sine_table[(offset + frame) % 16U] *
                    QW_VOICE_ENROLL_BEEP_AMPLITUDE_PERCENT;
                const int32_t sample =
                    (int32_t)(scaled / (32767LL * 100LL));
                samples[2U * frame] = sample;
                samples[2U * frame + 1U] = sample;
            }
            size_t written = 0U;
            err = qw_i2s_audio_write(&beep_audio, samples,
                                     chunk * 2U * sizeof(samples[0]),
                                     &written, 1000U);
            if (err != ESP_OK ||
                written != chunk * 2U * sizeof(samples[0])) {
                err = err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
            }
        }
        qw_runtime_i2s_release(&beep_audio);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enrollment start beep failed: %s",
                 esp_err_to_name(err));
    }
    if (!init_microphone_i2s()) {
        ESP_LOGE(TAG, "microphone restore after enrollment beep failed");
        return false;
    }
    return err == ESP_OK;
}

static unsigned enrolled_template_count(void)
{
    unsigned count = 0U;
    for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
        if (s_template_loaded[speaker]) {
            ++count;
        }
    }
    return count;
}

static esp_err_t ensure_nvs_ready(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Do not erase the partition implicitly from an audio test. */
        ESP_LOGE(TAG, "NVS requires maintenance before voice templates can be stored");
    }
    return err;
}

static esp_err_t load_template(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(QW_VOICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t size = 0U;
    err = nvs_get_blob(handle, QW_VOICE_NVS_KEY, NULL, &size);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (size == sizeof(qw_voice_nvs_blob_t)) {
        /* This v2 blob is larger than 2 KiB. Keep it out of the 3584-byte
         * ESP-IDF main-task stack while NVS is reading it. */
        static qw_voice_nvs_blob_t blob;
        err = nvs_open(QW_VOICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            return err;
        }
        size_t read_size = sizeof(blob);
        err = nvs_get_blob(handle, QW_VOICE_NVS_KEY, &blob, &read_size);
        nvs_close(handle);
        if (err != ESP_OK || read_size != sizeof(blob) ||
            blob.magic != QW_VOICE_NVS_MAGIC ||
            blob.version != QW_VOICE_TEMPLATE_VERSION ||
            (blob.template_mask &
             (uint16_t)~((1U << QW_VOICE_MAX_SPEAKERS) - 1U)) != 0U) {
            ESP_LOGW(TAG, "ignoring invalid stored voice template set");
            return ESP_ERR_INVALID_SIZE;
        }
        memset(s_templates, 0, sizeof(s_templates));
        memset((void *)s_template_loaded, 0, sizeof(s_template_loaded));
        for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
            if ((blob.template_mask & (uint16_t)(1U << speaker)) == 0U) {
                continue;
            }
            if (!qw_voice_template_is_valid(&blob.templates[speaker])) {
                ESP_LOGW(TAG, "ignoring invalid stored speaker template=%u",
                         (unsigned)(speaker + 1U));
                return ESP_ERR_INVALID_SIZE;
            }
            s_templates[speaker] = blob.templates[speaker];
            s_template_loaded[speaker] = true;
        }
        return ESP_OK;
    }

    if (size == sizeof(qw_voice_legacy_nvs_blob_t)) {
        qw_voice_legacy_nvs_blob_t blob;
        err = nvs_open(QW_VOICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            return err;
        }
        size_t read_size = sizeof(blob);
        err = nvs_get_blob(handle, QW_VOICE_NVS_KEY, &blob, &read_size);
        nvs_close(handle);
        if (err != ESP_OK || read_size != sizeof(blob) ||
            blob.magic != QW_VOICE_NVS_MAGIC ||
            blob.version != QW_VOICE_LEGACY_TEMPLATE_VERSION ||
            !qw_voice_template_is_valid(&blob.template_data)) {
            ESP_LOGW(TAG, "ignoring invalid legacy voice template");
            return ESP_ERR_INVALID_SIZE;
        }
        memset(s_templates, 0, sizeof(s_templates));
        memset((void *)s_template_loaded, 0, sizeof(s_template_loaded));
        s_templates[0] = blob.template_data;
        s_template_loaded[0] = true;
        ESP_LOGI(TAG, "loaded legacy voice template as speaker=1");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "ignoring invalid stored voice template size=%u",
             (unsigned)size);
    return ESP_ERR_INVALID_SIZE;
}

static esp_err_t save_templates(const qw_voice_template_t *templates,
                                const bool *template_loaded)
{
    if (templates == NULL || template_loaded == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t template_mask = 0U;
    for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
        if (template_loaded[speaker]) {
            if (!qw_voice_template_is_valid(&templates[speaker])) {
                return ESP_ERR_INVALID_ARG;
            }
            template_mask |= (uint16_t)(1U << speaker);
        }
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(QW_VOICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    qw_voice_nvs_blob_t writable_blob = {0};
    writable_blob.magic = QW_VOICE_NVS_MAGIC;
    writable_blob.version = QW_VOICE_TEMPLATE_VERSION;
    writable_blob.template_mask = template_mask;
    for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
        if (template_loaded[speaker]) {
            writable_blob.templates[speaker] = templates[speaker];
        }
    }
    err = nvs_set_blob(handle, QW_VOICE_NVS_KEY, &writable_blob,
                       sizeof(writable_blob));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t erase_template(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(QW_VOICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_key(handle, QW_VOICE_NVS_KEY);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static int16_t pcm_from_i2s(int32_t raw)
{
    int64_t shifted = (int64_t)raw >> CONFIG_NIULAI_VOICE_I2S_SHIFT;
    if (shifted > INT16_MAX) {
        return INT16_MAX;
    }
    if (shifted < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)shifted;
}

static void put_u16_le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)(value >> 8U);
}

static void put_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)(value >> 8U);
    out[2] = (uint8_t)(value >> 16U);
    out[3] = (uint8_t)(value >> 24U);
}

static void emit_capture(void)
{
    uint8_t header[QW_VOICE_CAPTURE_HEADER_BYTES] = {
        'Q', 'W', 'V', 'C', 'A', 'P', '1', '\0',
    };
    put_u32_le(&header[8], QW_VOICE_SAMPLE_RATE_HZ);
    put_u32_le(&header[12], (uint32_t)s_capture_count);
    put_u16_le(&header[16], 16U);
    put_u16_le(&header[18], 1U);

    bool ok = fwrite(header, 1U, sizeof(header), stdout) == sizeof(header);
    for (size_t offset = 0U; ok && offset < s_capture_count;) {
        size_t chunk = s_capture_count - offset;
        if (chunk > QW_VOICE_CAPTURE_CHUNK_SAMPLES) {
            chunk = QW_VOICE_CAPTURE_CHUNK_SAMPLES;
        }
        ok = fwrite(&s_capture_samples[offset], sizeof(s_capture_samples[0]),
                    chunk, stdout) == chunk;
        offset += chunk;
    }
    fflush(stdout);
    if (ok) {
        printf("VOICE_CAPTURE_END samples=%u rate=%u\n",
               (unsigned)s_capture_count, (unsigned)QW_VOICE_SAMPLE_RATE_HZ);
    } else {
        printf("VOICE_CAPTURE_ERROR reason=serial_write\n");
    }
    fflush(stdout);
}

static void emit_raw_frames(const int32_t *raw, size_t frame_count)
{
    printf("VOICE_RAW_BEGIN ws_hz=%u bclk_hz=%u data_bits=%u slot_bits=%u "
           "slot_mode=stereo slot_mask=both frames=%u\n",
           (unsigned)QW_VOICE_SAMPLE_RATE_HZ, (unsigned)QW_VOICE_BCLK_HZ,
           (unsigned)I2S_DATA_BIT_WIDTH_32BIT,
           (unsigned)I2S_SLOT_BIT_WIDTH_32BIT, (unsigned)frame_count);
    for (size_t frame = 0U; frame < frame_count; ++frame) {
        printf("VOICE_RAW frame=%u left=0x%08lx right=0x%08lx\n",
               (unsigned)frame,
               (unsigned long)(uint32_t)raw[frame * QW_VOICE_I2S_SLOT_COUNT],
               (unsigned long)(uint32_t)
                   raw[frame * QW_VOICE_I2S_SLOT_COUNT + 1U]);
    }
    printf("VOICE_RAW_END frames=%u\n", (unsigned)frame_count);
    fflush(stdout);
}

static void start_capture_if_requested(void)
{
    if (!s_capture_requested || s_capture_active) {
        return;
    }
    s_capture_requested = false;
    s_capture_active = true;
    s_capture_count = 0U;
}

static void capture_i2s_frames(const int32_t *raw, size_t frame_count)
{
    for (size_t frame = 0U; frame < frame_count; ++frame) {
        if (s_capture_count >= QW_VOICE_CAPTURE_SAMPLE_COUNT) {
            break;
        }
        const int32_t sample =
            raw[frame * 2U + CONFIG_NIULAI_VOICE_I2S_SLOT];
        s_capture_samples[s_capture_count++] = pcm_from_i2s(sample);
    }
    if (s_capture_count == QW_VOICE_CAPTURE_SAMPLE_COUNT) {
        s_capture_active = false;
        emit_capture();
    }
}

static void reset_matcher(void)
{
    for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
        const qw_voice_template_t *template_data =
            s_template_loaded[speaker] ? &s_templates[speaker] : NULL;
        if (!qw_voice_keyword_init(
                &s_matchers[speaker], template_data,
                CONFIG_NIULAI_VOICE_DISTANCE_THRESHOLD,
                CONFIG_NIULAI_VOICE_COOLDOWN_FRAMES)) {
            ESP_LOGE(TAG, "voice matcher initialization failed speaker=%u",
                     (unsigned)(speaker + 1U));
        }
    }
}

static void service_pending_commands(void)
{
    if (s_clear_requested) {
        s_clear_requested = false;
        esp_err_t err = erase_template();
        if (err == ESP_OK) {
            memset(s_templates, 0, sizeof(s_templates));
            memset((void *)s_template_loaded, 0, sizeof(s_template_loaded));
            s_enrolling = false;
            s_enrollment_pause_frames = 0U;
            qw_voice_segmenter_reset(&s_enrollment_segmenter);
            qw_voice_template_accumulator_init(&s_enrollment_accumulator);
            reset_matcher();
            printf("VOICE_TEMPLATE cleared keyword=牛来\n");
        } else {
            ESP_LOGE(TAG, "voice template clear failed: %s",
                     esp_err_to_name(err));
        }
    }
    if (s_play_requested) {
        s_play_requested = false;
        s_enrolling = false;
        s_enrollment_pause_frames = 0U;
        qw_voice_segmenter_reset(&s_enrollment_segmenter);
        qw_voice_template_accumulator_init(&s_enrollment_accumulator);
        reset_matcher();
        play_mama_direct_once();
    }
    if (s_enrollment_requested) {
        s_enrollment_requested = false;
        s_enrolling = true;
        s_enrollment_pause_frames = 0U;
        qw_voice_segmenter_reset(&s_enrollment_segmenter);
        qw_voice_template_accumulator_init(&s_enrollment_accumulator);
        (void)play_enrollment_start_beep();
        printf("VOICE_ENROLL waiting speaker=%u sample=1/%u keyword=牛来 "
               "say_once=1\n",
               (unsigned)(s_enrollment_speaker + 1U),
               (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
    }
}

static void complete_enrollment_sample(void)
{
    if (s_enrollment_segmenter.overflow ||
        s_enrollment_segmenter.frame_count < QW_VOICE_MIN_UTTERANCE_FRAMES) {
        printf("VOICE_ENROLL rejected speaker=%u sample=%u/%u frames=%u "
               "overflow=%u\n",
               (unsigned)(s_enrollment_speaker + 1U),
               (unsigned)(s_enrollment_accumulator.sample_count + 1U),
               (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT,
               (unsigned)s_enrollment_segmenter.frame_count,
               s_enrollment_segmenter.overflow ? 1U : 0U);
        qw_voice_segmenter_reset(&s_enrollment_segmenter);
        s_enrollment_pause_frames =
            QW_VOICE_ENROLL_INTER_SAMPLE_DELAY_FRAMES;
        printf("VOICE_ENROLL pause_ms=5000 next_speaker=%u next_sample=%u/%u\n",
               (unsigned)(s_enrollment_speaker + 1U),
               (unsigned)(s_enrollment_accumulator.sample_count + 1U),
               (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
        return;
    }

    if (!qw_voice_template_accumulator_add(
            &s_enrollment_accumulator,
            (const qw_voice_feature_t *)s_enrollment_segmenter.frames,
            s_enrollment_segmenter.frame_count)) {
        printf("VOICE_ENROLL rejected speaker=%u sample=%u/%u "
               "reason=aggregation\n",
               (unsigned)(s_enrollment_speaker + 1U),
               (unsigned)(s_enrollment_accumulator.sample_count + 1U),
               (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
        qw_voice_segmenter_reset(&s_enrollment_segmenter);
        s_enrollment_pause_frames =
            QW_VOICE_ENROLL_INTER_SAMPLE_DELAY_FRAMES;
        printf("VOICE_ENROLL pause_ms=5000 next_speaker=%u next_sample=%u/%u\n",
               (unsigned)(s_enrollment_speaker + 1U),
               (unsigned)(s_enrollment_accumulator.sample_count + 1U),
               (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
        return;
    }

    const unsigned sample_count = s_enrollment_accumulator.sample_count;
    if (sample_count < QW_VOICE_ENROLL_SAMPLE_COUNT) {
        printf("VOICE_ENROLL accepted speaker=%u sample=%u/%u frames=%u\n",
               (unsigned)(s_enrollment_speaker + 1U),
               sample_count, (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT,
               (unsigned)s_enrollment_segmenter.frame_count);
        qw_voice_segmenter_reset(&s_enrollment_segmenter);
        s_enrollment_pause_frames =
            QW_VOICE_ENROLL_INTER_SAMPLE_DELAY_FRAMES;
        printf("VOICE_ENROLL pause_ms=5000 next_speaker=%u next_sample=%u/%u\n",
               (unsigned)(s_enrollment_speaker + 1U),
               sample_count + 1U, (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
        return;
    }

    qw_voice_template_t candidate;
    if (!qw_voice_template_accumulator_finalize(&s_enrollment_accumulator,
                                                 &candidate)) {
        printf("VOICE_ENROLL rejected speaker=%u samples=%u "
               "reason=aggregation_finalize\n",
               (unsigned)(s_enrollment_speaker + 1U), sample_count);
        s_enrolling = false;
        return;
    }
    qw_voice_template_t next_templates[QW_VOICE_MAX_SPEAKERS] = {0};
    bool next_template_loaded[QW_VOICE_MAX_SPEAKERS] = {false};
    memcpy(next_templates, s_templates, sizeof(next_templates));
    memcpy(next_template_loaded, (const void *)s_template_loaded,
           sizeof(next_template_loaded));
    next_templates[s_enrollment_speaker] = candidate;
    next_template_loaded[s_enrollment_speaker] = true;
    esp_err_t err = save_templates(next_templates, next_template_loaded);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "voice template save failed: %s", esp_err_to_name(err));
        printf("VOICE_ENROLL rejected speaker=%u samples=%u reason=nvs\n",
               (unsigned)(s_enrollment_speaker + 1U), sample_count);
        s_enrolling = false;
        return;
    }

    memcpy(s_templates, next_templates, sizeof(s_templates));
    memcpy((void *)s_template_loaded, next_template_loaded,
           sizeof(s_template_loaded));
    s_enrolling = false;
    s_enrollment_pause_frames = 0U;
    reset_matcher();
    printf("VOICE_ENROLLED speaker=%u keyword=牛来 samples=%u "
           "template_frames=%u threshold=%u\n",
           (unsigned)(s_enrollment_speaker + 1U),
           sample_count,
           (unsigned)candidate.frame_count,
           (unsigned)CONFIG_NIULAI_VOICE_DISTANCE_THRESHOLD);
}

static bool init_microphone_i2s(void)
{
    esp_err_t err = qw_runtime_i2s_acquire(&s_audio_config, &s_audio);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MS4030 I2S RX init failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void play_mama_after_match(void)
{
    /* The microphone RX clock owns the shared GPIO9/GPIO10 clock lines. Stop
     * it before the blocking 44.1 kHz TX playback, then restore RX. */
    qw_runtime_i2s_release(&s_audio);
    printf("VOICE_ACTION action=play_mama begin=1\n");
    const esp_err_t play_err = qw_audio_player_play_mama_blocking();
    if (play_err == ESP_OK) {
        printf("VOICE_ACTION action=play_mama result=complete\n");
    } else {
        ESP_LOGE(TAG, "voice-triggered playback failed: %s",
                 esp_err_to_name(play_err));
        printf("VOICE_ACTION action=play_mama result=error\n");
    }
    if (!init_microphone_i2s()) {
        printf("VOICE_ACTION action=resume_mic result=error\n");
        return;
    }
    reset_matcher();
    s_post_playback_settle_frames = QW_VOICE_POST_PLAYBACK_SETTLE_FRAMES;
    printf("VOICE_ACTION action=resume_mic result=ready\n");
}

static void play_mama_direct_once(void)
{
    /* A direct playback request is serialized with the microphone and then
     * restores the always-on listener. */
    qw_runtime_i2s_release(&s_audio);
    printf("VOICE_PLAY_REQUESTED asset=牛来叫妈妈.mp3 volume=100%%\n");
    printf("VOICE_ACTION action=play_mama begin=1\n");
    const esp_err_t play_err = qw_audio_player_play_mama_blocking();
    if (play_err == ESP_OK) {
        printf("VOICE_ACTION action=play_mama result=complete\n");
    } else {
        ESP_LOGE(TAG, "direct playback failed: %s", esp_err_to_name(play_err));
        printf("VOICE_ACTION action=play_mama result=error\n");
    }
    if (!init_microphone_i2s()) {
        printf("VOICE_ACTION action=resume_mic result=error\n");
        return;
    }
    reset_matcher();
    s_post_playback_settle_frames = QW_VOICE_POST_PLAYBACK_SETTLE_FRAMES;
    printf("VOICE_ACTION action=resume_mic result=ready\n");
    fflush(stdout);
}

static bool process_feature(const qw_voice_feature_t feature, bool voiced)
{
    service_pending_commands();
    if (s_post_playback_settle_frames > 0U) {
        --s_post_playback_settle_frames;
        return false;
    }
    if (s_enrolling) {
        if (s_enrollment_pause_frames > 0U) {
            --s_enrollment_pause_frames;
            if (s_enrollment_pause_frames == 0U) {
                (void)play_enrollment_start_beep();
                printf("VOICE_ENROLL waiting speaker=%u sample=%u/%u "
                       "keyword=牛来 say_once=1\n",
                       (unsigned)(s_enrollment_speaker + 1U),
                       (unsigned)(s_enrollment_accumulator.sample_count + 1U),
                       (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
                fflush(stdout);
            }
            return false;
        }
        qw_voice_segment_event_t segment_event;
        if (!qw_voice_segmenter_push(&s_enrollment_segmenter, feature, voiced,
                                     &segment_event)) {
            ESP_LOGE(TAG, "voice enrollment segmenter rejected frame");
            return false;
        }
        if (segment_event.complete) {
            complete_enrollment_sample();
        }
        return false;
    }
    if (enrolled_template_count() == 0U) {
        return false;
    }

    bool matched = false;
    unsigned matched_speaker = 0U;
    uint16_t matched_distance = UINT16_MAX;
    uint8_t matched_frame_count = 0U;
    for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
        if (!s_template_loaded[speaker]) {
            continue;
        }
        qw_voice_keyword_event_t event;
        if (!qw_voice_keyword_push(&s_matchers[speaker], feature, voiced,
                                   &event)) {
            ESP_LOGE(TAG, "voice matcher rejected frame speaker=%u",
                     (unsigned)(speaker + 1U));
            continue;
        }
        if (event.utterance_complete && event.template_available &&
            !event.matched &&
            event.distance > CONFIG_NIULAI_VOICE_DISTANCE_THRESHOLD) {
            printf("VOICE_REJECT speaker=%u reason=distance distance=%u "
                   "frames=%u threshold=%u\n",
                   (unsigned)(speaker + 1U), (unsigned)event.distance,
                   (unsigned)event.frame_count,
                   (unsigned)CONFIG_NIULAI_VOICE_DISTANCE_THRESHOLD);
        }
        if (qw_voice_keyword_event_requests_mama_audio(&event) &&
            (!matched || event.distance < matched_distance)) {
            matched = true;
            matched_speaker = (unsigned)(speaker + 1U);
            matched_distance = event.distance;
            matched_frame_count = event.frame_count;
        }
    }
    if (matched) {
        printf("VOICE_COMMAND speaker=%u command=NIULAI text=牛来 "
               "distance=%u frames=%u\n",
               matched_speaker, (unsigned)matched_distance,
               (unsigned)matched_frame_count);
        play_mama_after_match();
        return true;
    }
    return false;
}

static void voice_keyword_task(void *arg)
{
    qw_i2s_audio_t *audio = (qw_i2s_audio_t *)arg;
    int32_t raw[QW_VOICE_I2S_READ_FRAMES * 2U];
    int16_t pcm[QW_VOICE_FRAME_SAMPLES];
    size_t pcm_count = 0U;

    for (;;) {
        start_capture_if_requested();
        size_t bytes_read = 0U;
        esp_err_t err = qw_i2s_audio_read(audio, raw, sizeof(raw),
                                           &bytes_read, 1000U);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S RX read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100U));
            continue;
        }
        const size_t frame_bytes = 2U * sizeof(raw[0]);
        if (bytes_read == 0U || bytes_read % frame_bytes != 0U) {
            ESP_LOGW(TAG, "I2S RX returned partial stereo frame bytes=%u",
                     (unsigned)bytes_read);
            continue;
        }

        const size_t frame_count = bytes_read / frame_bytes;
        if (s_raw_requested) {
            s_raw_requested = false;
            const size_t raw_frame_count =
                frame_count < QW_VOICE_RAW_FRAME_COUNT
                    ? frame_count
                    : QW_VOICE_RAW_FRAME_COUNT;
            emit_raw_frames(raw, raw_frame_count);
        }
        if (s_capture_active) {
            capture_i2s_frames(raw, frame_count);
            continue;
        }
        for (size_t frame = 0U; frame < frame_count; ++frame) {
            const int32_t sample =
                raw[frame * 2U + CONFIG_NIULAI_VOICE_I2S_SLOT];
            pcm[pcm_count++] = pcm_from_i2s(sample);
            if (pcm_count < QW_VOICE_FRAME_SAMPLES) {
                continue;
            }

            qw_voice_feature_t feature;
            uint32_t rms = 0U;
            if (qw_voice_extract_feature(pcm, QW_VOICE_FRAME_SAMPLES, feature,
                                          &rms)) {
                const bool action_triggered = process_feature(
                    feature,
                    qw_voice_frame_is_usable(
                        feature,
                        rms, CONFIG_NIULAI_VOICE_VAD_RMS_THRESHOLD));
                if (!action_triggered) {
                    memmove(pcm, pcm + QW_VOICE_FRAME_HOP_SAMPLES,
                            (QW_VOICE_FRAME_SAMPLES -
                             QW_VOICE_FRAME_HOP_SAMPLES) * sizeof(pcm[0]));
                    pcm_count = QW_VOICE_FRAME_SAMPLES -
                                QW_VOICE_FRAME_HOP_SAMPLES;
                } else {
                    pcm_count = 0U;
                }
            }
        }
    }
}

esp_err_t qw_voice_keyword_test_start(void)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ensure_nvs_ready();
    if (err != ESP_OK) {
        return err;
    }
    err = load_template();
    if (err != ESP_OK && err != ESP_ERR_INVALID_SIZE) {
        ESP_LOGE(TAG, "voice template load failed: %s", esp_err_to_name(err));
        return err;
    }
    qw_voice_segmenter_init(&s_enrollment_segmenter);
    qw_voice_template_accumulator_init(&s_enrollment_accumulator);
    s_enrollment_pause_frames = 0U;
    reset_matcher();

    s_audio_config = qw_i2s_audio_config_default();
    s_audio_config.sample_rate_hz = QW_VOICE_SAMPLE_RATE_HZ;
    s_audio_config.slot_width = I2S_DATA_BIT_WIDTH_32BIT;
    s_audio_config.enable_tx = false;
    s_audio_config.enable_rx = true;
    if (!init_microphone_i2s()) {
        return ESP_FAIL;
    }
    BaseType_t task_result = xTaskCreate(
        voice_keyword_task, "voice_keyword", QW_VOICE_TASK_STACK_WORDS,
        &s_audio, QW_VOICE_TASK_PRIORITY, &s_task);
    if (task_result != pdPASS) {
        qw_runtime_i2s_release(&s_audio);
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    printf("VOICE_READY keyword=牛来 backend=enrolled-template sample_rate=%u "
           "bclk=%d ws=%d din=%d slot=%u shift=%u templates=%u/%u\n",
           (unsigned)s_audio_config.sample_rate_hz, s_audio_config.bclk_gpio,
           s_audio_config.ws_gpio, s_audio_config.rx_data_gpio,
           (unsigned)CONFIG_NIULAI_VOICE_I2S_SLOT,
           (unsigned)CONFIG_NIULAI_VOICE_I2S_SHIFT,
           enrolled_template_count(), (unsigned)QW_VOICE_MAX_SPEAKERS);
    printf("VOICE_I2S_CONFIG ws_hz=%u bclk_hz=%u format=philips "
           "data_bits=%u slot_bits=%u slot_mode=stereo slot_mask=both\n",
           (unsigned)QW_VOICE_SAMPLE_RATE_HZ, (unsigned)QW_VOICE_BCLK_HZ,
           (unsigned)I2S_DATA_BIT_WIDTH_32BIT,
           (unsigned)I2S_SLOT_BIT_WIDTH_32BIT);
    return ESP_OK;
}

esp_err_t qw_voice_keyword_runtime_start(void)
{
    return qw_voice_keyword_test_start();
}

bool qw_voice_keyword_runtime_handle_command(const char *line)
{
    const qw_audio_command_t command = qw_audio_command_parse(line);
    if (command == QW_AUDIO_COMMAND_PLAY_MAMA) {
        if (s_task == NULL) {
            return false;
        }
        s_play_requested = true;
        printf("AUDIO_COMMAND accepted=play\n");
        return true;
    }
    if (command == QW_AUDIO_COMMAND_STOP) {
        if (s_task == NULL) {
            return false;
        }
        s_play_requested = false;
        printf("AUDIO_COMMAND accepted=stop pending_only=1\n");
        return true;
    }
    return false;
}

void qw_voice_keyword_test_request_enroll(uint8_t speaker_index)
{
    if (speaker_index < 1U || speaker_index > QW_VOICE_MAX_SPEAKERS) {
        printf("VOICE_ENROLL_INVALID speaker=%u allowed=1..%u\n",
               (unsigned)speaker_index, (unsigned)QW_VOICE_MAX_SPEAKERS);
        return;
    }
    s_enrollment_speaker = (uint8_t)(speaker_index - 1U);
    s_enrollment_requested = true;
    printf("VOICE_ENROLL_REQUESTED speaker=%u samples=%u\n",
           (unsigned)speaker_index, (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT);
    fflush(stdout);
}

void qw_voice_keyword_test_request_play(void)
{
    s_play_requested = true;
    fflush(stdout);
}

void qw_voice_keyword_test_request_capture(void)
{
    if (s_capture_requested || s_capture_active) {
        printf("VOICE_CAPTURE_BUSY\n");
        return;
    }
    s_capture_requested = true;
    printf("VOICE_CAPTURE_REQUESTED seconds=%u rate=%u slot=%u shift=%u\n",
           (unsigned)QW_VOICE_CAPTURE_SECONDS,
           (unsigned)QW_VOICE_SAMPLE_RATE_HZ,
           (unsigned)CONFIG_NIULAI_VOICE_I2S_SLOT,
           (unsigned)CONFIG_NIULAI_VOICE_I2S_SHIFT);
    fflush(stdout);
}

void qw_voice_keyword_test_request_raw(void)
{
    if (s_raw_requested || s_capture_requested || s_capture_active) {
        printf("VOICE_RAW_BUSY\n");
        return;
    }
    s_raw_requested = true;
    printf("VOICE_RAW_REQUESTED frames=%u\n", (unsigned)QW_VOICE_RAW_FRAME_COUNT);
    fflush(stdout);
}

void qw_voice_keyword_test_clear(void)
{
    s_clear_requested = true;
}

void qw_voice_keyword_test_status(void)
{
    printf("VOICE_STATUS keyword=牛来 backend=enrolled-template enrolled=%u "
           "speakers=%u/%u enrolling=%u enrollment_speaker=%u "
           "enrollment_samples=%u/%u threshold=%u vad_rms=%u "
           "cooldown_frames=%u\n",
           enrolled_template_count(), enrolled_template_count(),
           (unsigned)QW_VOICE_MAX_SPEAKERS,
           s_enrolling ? 1U : 0U,
           s_enrolling ? (unsigned)(s_enrollment_speaker + 1U) : 0U,
           (unsigned)s_enrollment_accumulator.sample_count,
           (unsigned)QW_VOICE_ENROLL_SAMPLE_COUNT,
           (unsigned)CONFIG_NIULAI_VOICE_DISTANCE_THRESHOLD,
           (unsigned)CONFIG_NIULAI_VOICE_VAD_RMS_THRESHOLD,
           (unsigned)CONFIG_NIULAI_VOICE_COOLDOWN_FRAMES);
}
