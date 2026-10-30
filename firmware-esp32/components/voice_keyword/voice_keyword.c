#include "voice_keyword.h"

#include <math.h>
#include <string.h>

#define QW_VOICE_LOG_FLOOR 1.0e-5f
#define QW_VOICE_SPECTRAL_DB_FLOOR (-40.0f)
#define QW_VOICE_DTW_INF 65535U

/* One Goertzel probe per coarse band.  The target bins are aligned to the
 * 480-sample, 16 kHz frame (bin spacing = 33.333 Hz): approximately 200,
 * 400, 800, 1200, 1700, 2500, 3500 and 4800 Hz.  The features are ratios
 * between probes, so microphone gain does not enter the template. */
static const float GOERTZEL_COEFFICIENTS[8] = {
    1.993834667466256f, 1.9753766811902755f,
    1.902113032590307f, 1.7820130483767358f,
    1.57063386176149f,  1.1111404660392046f,
    0.39018064403225666f, -0.6180339887498947f,
};

static uint8_t quantize_ratio(float ratio)
{
    if (!(ratio > QW_VOICE_LOG_FLOOR)) {
        return 0U;
    }
    float db = 10.0f * log10f(ratio);
    if (db <= QW_VOICE_SPECTRAL_DB_FLOOR) {
        return 0U;
    }
    if (db >= 0.0f) {
        return 255U;
    }
    float normalized =
        (db - QW_VOICE_SPECTRAL_DB_FLOOR) /
        (0.0f - QW_VOICE_SPECTRAL_DB_FLOOR);
    int value = (int)(normalized * 255.0f + 0.5f);
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    return (uint8_t)value;
}

bool qw_voice_extract_feature(const int16_t *samples, size_t sample_count,
                              qw_voice_feature_t out, uint32_t *rms_out)
{
    if (samples == NULL || out == NULL || rms_out == NULL ||
        sample_count != QW_VOICE_FRAME_SAMPLES) {
        return false;
    }

    int64_t sum = 0;
    for (size_t i = 0; i < sample_count; ++i) {
        sum += samples[i];
    }
    const int32_t mean = (int32_t)(sum / (int64_t)sample_count);
    uint64_t squared_sum = 0U;
    for (size_t i = 0; i < sample_count; ++i) {
        const int32_t centered = (int32_t)samples[i] - mean;
        squared_sum += (uint64_t)((int64_t)centered * centered);
    }
    const float rms = sqrtf((float)squared_sum / (float)sample_count);
    *rms_out = rms >= 4294967295.0f ? UINT32_MAX : (uint32_t)rms;

    float band_energy[8] = {0.0f};
    unsigned zero_crossings = 0U;
    bool previous_positive = false;
    bool have_previous = false;
    for (size_t band = 0; band < 8U; ++band) {
        float q1 = 0.0f;
        float q2 = 0.0f;
        const float coefficient = GOERTZEL_COEFFICIENTS[band];
        for (size_t i = 0; i < sample_count; ++i) {
            const float centered =
                ((float)samples[i] - (float)mean) / 32768.0f;
            const float q0 = coefficient * q1 - q2 + centered;
            q2 = q1;
            q1 = q0;
            if (band == 0U) {
                const int32_t centered_integer = (int32_t)samples[i] - mean;
                if (centered_integer > 32 || centered_integer < -32) {
                    const bool positive = centered_integer > 0;
                    if (have_previous && positive != previous_positive) {
                        ++zero_crossings;
                    }
                    previous_positive = positive;
                    have_previous = true;
                }
            }
        }
        float energy = q1 * q1 + q2 * q2 - coefficient * q1 * q2;
        band_energy[band] = energy > 0.0f ? energy : 0.0f;
    }

    float total_band_energy = 0.0f;
    for (size_t band = 0; band < 8U; ++band) {
        total_band_energy += band_energy[band];
    }
    if (!(total_band_energy > QW_VOICE_LOG_FLOOR)) {
        memset(out, 0, sizeof(*out));
    } else {
        for (size_t band = 0; band < 8U; ++band) {
            out[band] = quantize_ratio(band_energy[band] /
                                       total_band_energy);
        }
    }

    const float zero_crossing_rate =
        (float)zero_crossings / (float)(sample_count - 1U);
    int zcr = (int)(zero_crossing_rate * 255.0f + 0.5f);
    if (zcr < 0) zcr = 0;
    if (zcr > 255) zcr = 255;
    out[8] = (uint8_t)zcr;
    return true;
}

bool qw_voice_frame_is_voiced(uint32_t rms, uint32_t rms_threshold)
{
    return rms >= rms_threshold;
}

bool qw_voice_frame_is_usable(const qw_voice_feature_t feature,
                              uint32_t rms, uint32_t rms_threshold)
{
    return feature != NULL && qw_voice_frame_is_voiced(rms, rms_threshold) &&
           feature[8] >= QW_VOICE_MIN_ZERO_CROSSING_RATE;
}

void qw_voice_segmenter_init(qw_voice_segmenter_t *segmenter)
{
    if (segmenter == NULL) {
        return;
    }
    memset(segmenter, 0, sizeof(*segmenter));
}

void qw_voice_segmenter_reset(qw_voice_segmenter_t *segmenter)
{
    if (segmenter == NULL) {
        return;
    }
    segmenter->in_utterance = false;
    segmenter->overflow = false;
    segmenter->frame_count = 0U;
    segmenter->silence_frames = 0U;
}

bool qw_voice_segmenter_push(qw_voice_segmenter_t *segmenter,
                             const qw_voice_feature_t feature, bool voiced,
                             qw_voice_segment_event_t *event)
{
    if (segmenter == NULL || feature == NULL || event == NULL) {
        return false;
    }
    memset(event, 0, sizeof(*event));

    if (voiced) {
        if (!segmenter->in_utterance) {
            segmenter->in_utterance = true;
            segmenter->overflow = false;
            segmenter->frame_count = 0U;
            segmenter->silence_frames = 0U;
        }
        segmenter->silence_frames = 0U;
        if (segmenter->frame_count < QW_VOICE_MAX_UTTERANCE_FRAMES) {
            memcpy(segmenter->frames[segmenter->frame_count], feature,
                   sizeof(segmenter->frames[0]));
            ++segmenter->frame_count;
        } else {
            segmenter->overflow = true;
        }
        return true;
    }

    if (!segmenter->in_utterance) {
        return true;
    }
    if (segmenter->silence_frames < UINT8_MAX) {
        ++segmenter->silence_frames;
    }
    if (segmenter->silence_frames < QW_VOICE_END_SILENCE_FRAMES) {
        return true;
    }

    event->complete = true;
    event->overflow = segmenter->overflow;
    event->frame_count = segmenter->frame_count;
    segmenter->in_utterance = false;
    segmenter->silence_frames = 0U;
    segmenter->overflow = false;
    /* Keep frame_count and frames readable until the next voiced frame. */
    return true;
}

bool qw_voice_template_set(qw_voice_template_t *template_data,
                           const qw_voice_feature_t *frames,
                           size_t frame_count)
{
    if (template_data == NULL || frames == NULL ||
        frame_count < QW_VOICE_MIN_UTTERANCE_FRAMES ||
        frame_count > QW_VOICE_MAX_TEMPLATE_FRAMES) {
        return false;
    }
    memset(template_data, 0, sizeof(*template_data));
    template_data->frame_count = (uint8_t)frame_count;
    memcpy(template_data->frames, frames,
           frame_count * sizeof(template_data->frames[0]));
    return true;
}

bool qw_voice_template_is_valid(const qw_voice_template_t *template_data)
{
    return template_data != NULL &&
           template_data->frame_count >= QW_VOICE_MIN_UTTERANCE_FRAMES &&
           template_data->frame_count <= QW_VOICE_MAX_TEMPLATE_FRAMES;
}

void qw_voice_template_accumulator_init(
    qw_voice_template_accumulator_t *accumulator)
{
    if (accumulator == NULL) {
        return;
    }
    memset(accumulator, 0, sizeof(*accumulator));
}

bool qw_voice_template_accumulator_add(
    qw_voice_template_accumulator_t *accumulator,
    const qw_voice_feature_t *frames, size_t frame_count)
{
    if (accumulator == NULL || frames == NULL ||
        frame_count < QW_VOICE_MIN_UTTERANCE_FRAMES ||
        frame_count > QW_VOICE_MAX_UTTERANCE_FRAMES ||
        accumulator->sample_count >= QW_VOICE_ENROLL_SAMPLE_COUNT) {
        return false;
    }

    if (accumulator->sample_count == 0U) {
        accumulator->target_frame_count = (uint8_t)frame_count;
    }
    const size_t target_count = accumulator->target_frame_count;
    if (target_count < QW_VOICE_MIN_UTTERANCE_FRAMES ||
        target_count > QW_VOICE_MAX_TEMPLATE_FRAMES) {
        return false;
    }

    for (size_t target = 0U; target < target_count; ++target) {
        const size_t source =
            (target * (frame_count - 1U) + (target_count - 1U) / 2U) /
            (target_count - 1U);
        for (size_t dimension = 0U; dimension < QW_VOICE_FEATURE_DIM;
             ++dimension) {
            accumulator->sums[target][dimension] += frames[source][dimension];
        }
    }
    ++accumulator->sample_count;
    return true;
}

bool qw_voice_template_accumulator_finalize(
    const qw_voice_template_accumulator_t *accumulator,
    qw_voice_template_t *template_data)
{
    if (accumulator == NULL || template_data == NULL ||
        accumulator->sample_count != QW_VOICE_ENROLL_SAMPLE_COUNT ||
        accumulator->target_frame_count < QW_VOICE_MIN_UTTERANCE_FRAMES ||
        accumulator->target_frame_count > QW_VOICE_MAX_TEMPLATE_FRAMES) {
        return false;
    }

    memset(template_data, 0, sizeof(*template_data));
    template_data->frame_count = accumulator->target_frame_count;
    for (size_t frame = 0U; frame < template_data->frame_count; ++frame) {
        for (size_t dimension = 0U; dimension < QW_VOICE_FEATURE_DIM;
             ++dimension) {
            const uint32_t sum = accumulator->sums[frame][dimension];
            template_data->frames[frame][dimension] = (uint8_t)(
                (sum + QW_VOICE_ENROLL_SAMPLE_COUNT / 2U) /
                QW_VOICE_ENROLL_SAMPLE_COUNT);
        }
    }
    return true;
}

bool qw_voice_keyword_init(qw_voice_keyword_state_t *state,
                           const qw_voice_template_t *template_data,
                           uint16_t distance_threshold,
                           uint16_t cooldown_frames)
{
    if (state == NULL) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    state->distance_threshold = distance_threshold;
    state->cooldown_frames = cooldown_frames;
    qw_voice_segmenter_init(&state->segmenter);
    if (template_data != NULL) {
        if (!qw_voice_template_is_valid(template_data)) {
            return false;
        }
        state->template_data = *template_data;
        state->has_template = true;
    }
    return true;
}

void qw_voice_keyword_clear_template(qw_voice_keyword_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(&state->template_data, 0, sizeof(state->template_data));
    state->has_template = false;
    state->cooldown_remaining = 0U;
    qw_voice_segmenter_reset(&state->segmenter);
}

static uint16_t feature_distance(const qw_voice_feature_t left,
                                 const qw_voice_feature_t right)
{
    unsigned sum = 0U;
    for (size_t i = 0; i < QW_VOICE_FEATURE_DIM; ++i) {
        unsigned a = left[i];
        unsigned b = right[i];
        sum += a > b ? a - b : b - a;
    }
    return (uint16_t)(sum / QW_VOICE_FEATURE_DIM);
}

bool qw_voice_keyword_distance(const qw_voice_template_t *template_data,
                               const qw_voice_feature_t *frames,
                               size_t frame_count, uint16_t *distance_out)
{
    if (!qw_voice_template_is_valid(template_data) || frames == NULL ||
        frame_count < QW_VOICE_MIN_UTTERANCE_FRAMES ||
        frame_count > QW_VOICE_MAX_UTTERANCE_FRAMES ||
        distance_out == NULL) {
        return false;
    }

    const size_t template_count = template_data->frame_count;
    uint16_t previous[QW_VOICE_MAX_TEMPLATE_FRAMES + 1U];
    uint16_t current[QW_VOICE_MAX_TEMPLATE_FRAMES + 1U];
    previous[0] = 0U;
    for (size_t column = 1U; column <= template_count; ++column) {
        previous[column] = QW_VOICE_DTW_INF;
    }

    for (size_t row = 1U; row <= frame_count; ++row) {
        current[0] = QW_VOICE_DTW_INF;
        for (size_t column = 1U; column <= template_count; ++column) {
            uint16_t best = previous[column] < current[column - 1U]
                                ? previous[column]
                                : current[column - 1U];
            if (previous[column - 1U] < best) {
                best = previous[column - 1U];
            }
            unsigned sum = (unsigned)best +
                           feature_distance(frames[row - 1U],
                                             template_data->frames[column - 1U]);
            current[column] = sum >= QW_VOICE_DTW_INF
                                  ? QW_VOICE_DTW_INF
                                  : (uint16_t)sum;
        }
        memcpy(previous, current, (template_count + 1U) * sizeof(previous[0]));
    }

    if (previous[template_count] == QW_VOICE_DTW_INF) {
        return false;
    }
    unsigned normalized =
        ((unsigned)previous[template_count] + (unsigned)(frame_count / 2U)) /
        (unsigned)frame_count;
    if (normalized > UINT16_MAX) normalized = UINT16_MAX;
    *distance_out = (uint16_t)normalized;
    return true;
}

bool qw_voice_keyword_push(qw_voice_keyword_state_t *state,
                           const qw_voice_feature_t feature, bool voiced,
                           qw_voice_keyword_event_t *event)
{
    if (state == NULL || feature == NULL || event == NULL) {
        return false;
    }
    memset(event, 0, sizeof(*event));
    if (state->cooldown_remaining > 0U) {
        --state->cooldown_remaining;
    }

    qw_voice_segment_event_t segment_event;
    if (!qw_voice_segmenter_push(&state->segmenter, feature, voiced,
                                 &segment_event)) {
        return false;
    }
    if (!segment_event.complete) {
        return true;
    }

    event->utterance_complete = true;
    event->template_available = state->has_template;
    event->frame_count = segment_event.frame_count;
    if (!state->has_template || segment_event.overflow ||
        segment_event.frame_count < QW_VOICE_MIN_UTTERANCE_FRAMES) {
        return true;
    }

    if (!qw_voice_keyword_distance(&state->template_data,
                                   state->segmenter.frames,
                                   segment_event.frame_count,
                                   &event->distance)) {
        return false;
    }
    if (event->distance <= state->distance_threshold &&
        state->cooldown_remaining == 0U) {
        event->matched = true;
        state->cooldown_remaining = state->cooldown_frames;
    }
    return true;
}

bool qw_voice_keyword_event_requests_mama_audio(
    const qw_voice_keyword_event_t *event)
{
    return event != NULL && event->matched;
}
