#ifndef QW_VOICE_KEYWORD_H
#define QW_VOICE_KEYWORD_H

/* Host-testable, dependency-free front-end for the fixed "牛来" keyword.
 *
 * This is deliberately not a speech-to-text engine.  It extracts a small
 * spectral feature vector from 16 kHz mono PCM, segments voiced utterances,
 * and compares an enrolled template with bounded dynamic time warping.  The
 * ESP32 service is responsible for I2S/NVS; a future ESP-SR model can replace
 * this component without changing the service's event boundary.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QW_VOICE_SAMPLE_RATE_HZ 16000U
#define QW_VOICE_FRAME_SAMPLES 480U       /* 30 ms */
#define QW_VOICE_FRAME_HOP_SAMPLES 160U  /* 10 ms */
#define QW_VOICE_FEATURE_DIM 9U
#define QW_VOICE_MIN_ZERO_CROSSING_RATE 2U
#define QW_VOICE_MIN_UTTERANCE_FRAMES 4U
#define QW_VOICE_MAX_TEMPLATE_FRAMES 128U
#define QW_VOICE_MAX_UTTERANCE_FRAMES 128U
#define QW_VOICE_END_SILENCE_FRAMES 8U   /* 80 ms */
#define QW_VOICE_DEFAULT_DISTANCE_THRESHOLD 40U
#define QW_VOICE_DEFAULT_COOLDOWN_FRAMES 50U /* 500 ms */
#define QW_VOICE_ENROLL_SAMPLE_COUNT 5U
#define QW_VOICE_ENROLL_INTER_SAMPLE_DELAY_FRAMES 500U /* 5 seconds */
#define QW_VOICE_MAX_SPEAKERS 2U
#define QW_VOICE_TEMPLATE_VERSION 2U
#define QW_VOICE_LEGACY_TEMPLATE_VERSION 1U

typedef uint8_t qw_voice_feature_t[QW_VOICE_FEATURE_DIM];

typedef struct {
    uint8_t frame_count;
    qw_voice_feature_t frames[QW_VOICE_MAX_TEMPLATE_FRAMES];
} qw_voice_template_t;

/* Accumulates several utterances into one time-normalized template.  The
 * first accepted sample chooses the target frame count; subsequent samples
 * are resampled by nearest frame before their feature bytes are averaged. */
typedef struct {
    uint8_t sample_count;
    uint8_t target_frame_count;
    uint32_t sums[QW_VOICE_MAX_TEMPLATE_FRAMES][QW_VOICE_FEATURE_DIM];
} qw_voice_template_accumulator_t;

typedef struct {
    bool in_utterance;
    bool overflow;
    uint8_t frame_count;
    uint8_t silence_frames;
    qw_voice_feature_t frames[QW_VOICE_MAX_UTTERANCE_FRAMES];
} qw_voice_segmenter_t;

typedef struct {
    bool complete;
    bool overflow;
    uint8_t frame_count;
} qw_voice_segment_event_t;

typedef struct {
    bool utterance_complete;
    bool template_available;
    bool matched;
    uint16_t distance;
    uint8_t frame_count;
} qw_voice_keyword_event_t;

typedef struct {
    qw_voice_template_t template_data;
    bool has_template;
    uint16_t distance_threshold;
    uint16_t cooldown_frames;
    uint16_t cooldown_remaining;
    qw_voice_segmenter_t segmenter;
} qw_voice_keyword_state_t;

/* Extracts 8 coarse spectral-ratio features plus zero-crossing rate.  The
 * returned RMS is in the input PCM's units and is used by the VAD. */
bool qw_voice_extract_feature(const int16_t *samples, size_t sample_count,
                              qw_voice_feature_t out, uint32_t *rms_out);

bool qw_voice_frame_is_voiced(uint32_t rms, uint32_t rms_threshold);

/* A high-RMS, one-sided I2S burst is not reliable speech.  The normalized
 * zero-crossing feature provides a cheap fail-closed guard for the known
 * MS4030 wiring/slot failure mode without coupling the pure matcher to I2S. */
bool qw_voice_frame_is_usable(const qw_voice_feature_t feature,
                              uint32_t rms, uint32_t rms_threshold);

void qw_voice_segmenter_init(qw_voice_segmenter_t *segmenter);
void qw_voice_segmenter_reset(qw_voice_segmenter_t *segmenter);

/* Pushes one feature frame.  At a completed utterance the segmenter's frame
 * storage remains readable until the next voiced frame; event->frame_count
 * identifies the valid prefix. */
bool qw_voice_segmenter_push(qw_voice_segmenter_t *segmenter,
                             const qw_voice_feature_t feature, bool voiced,
                             qw_voice_segment_event_t *event);

bool qw_voice_template_set(qw_voice_template_t *template_data,
                           const qw_voice_feature_t *frames,
                           size_t frame_count);
bool qw_voice_template_is_valid(const qw_voice_template_t *template_data);

void qw_voice_template_accumulator_init(
    qw_voice_template_accumulator_t *accumulator);
bool qw_voice_template_accumulator_add(
    qw_voice_template_accumulator_t *accumulator,
    const qw_voice_feature_t *frames, size_t frame_count);
bool qw_voice_template_accumulator_finalize(
    const qw_voice_template_accumulator_t *accumulator,
    qw_voice_template_t *template_data);

bool qw_voice_keyword_init(qw_voice_keyword_state_t *state,
                           const qw_voice_template_t *template_data,
                           uint16_t distance_threshold,
                           uint16_t cooldown_frames);
void qw_voice_keyword_clear_template(qw_voice_keyword_state_t *state);

/* Compares one segmented utterance to a template. */
bool qw_voice_keyword_distance(const qw_voice_template_t *template_data,
                               const qw_voice_feature_t *frames,
                               size_t frame_count, uint16_t *distance_out);

/* Pushes one feature/VAD decision and emits at most one event when the
 * trailing silence closes an utterance. */
bool qw_voice_keyword_push(qw_voice_keyword_state_t *state,
                           const qw_voice_feature_t feature, bool voiced,
                           qw_voice_keyword_event_t *event);

/* The only side effect currently authorized for a matched keyword is the
 * embedded mama audio playback request. */
bool qw_voice_keyword_event_requests_mama_audio(
    const qw_voice_keyword_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* QW_VOICE_KEYWORD_H */
