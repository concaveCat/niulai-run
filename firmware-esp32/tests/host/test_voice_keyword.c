#include "qw_check.h"
#include "voice_keyword.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static void make_feature(qw_voice_feature_t feature, uint8_t seed)
{
    for (size_t i = 0; i < QW_VOICE_FEATURE_DIM; ++i) {
        feature[i] = (uint8_t)(seed + (uint8_t)(i * 3U));
    }
}

static void push_silence(qw_voice_keyword_state_t *state, unsigned count,
                         qw_voice_keyword_event_t *event)
{
    qw_voice_feature_t feature = {0};
    for (unsigned i = 0; i < count; ++i) {
        CHECK(qw_voice_keyword_push(state, feature, false, event));
    }
}

static void push_phrase(qw_voice_keyword_state_t *state, uint8_t seed,
                        unsigned count, qw_voice_keyword_event_t *event)
{
    qw_voice_feature_t feature;
    for (unsigned i = 0; i < count; ++i) {
        make_feature(feature, (uint8_t)(seed + i));
        CHECK(qw_voice_keyword_push(state, feature, true, event));
    }
}

static void make_template(qw_voice_template_t *template_data, uint8_t seed,
                          unsigned count)
{
    qw_voice_feature_t frames[QW_VOICE_MAX_TEMPLATE_FRAMES];
    for (unsigned i = 0; i < count; ++i) {
        make_feature(frames[i], (uint8_t)(seed + i));
    }
    CHECK(qw_voice_template_set(template_data, frames, count));
}

static void test_extracts_features_and_rms(void)
{
    int16_t samples[QW_VOICE_FRAME_SAMPLES] = {0};
    qw_voice_feature_t feature;
    uint32_t rms = 123U;
    CHECK(qw_voice_extract_feature(samples, QW_VOICE_FRAME_SAMPLES,
                                   feature, &rms));
    CHECK_INT_EQ(rms, 0);
    CHECK(!qw_voice_frame_is_voiced(rms, 1));

    for (size_t i = 0; i < QW_VOICE_FRAME_SAMPLES; ++i) {
        samples[i] = (int16_t)((i % 2U) == 0U ? 8000 : -8000);
    }
    CHECK(qw_voice_extract_feature(samples, QW_VOICE_FRAME_SAMPLES,
                                   feature, &rms));
    CHECK(rms > 7000U && rms < 9000U);
    CHECK(qw_voice_frame_is_voiced(rms, 500U));
    CHECK(feature[8] > 200U); /* alternating signal has many crossings */
    CHECK(!qw_voice_extract_feature(samples, QW_VOICE_FRAME_SAMPLES - 1U,
                                    feature, &rms));
}

static void test_usable_frame_rejects_one_sided_burst(void)
{
    qw_voice_feature_t feature = {0};
    feature[8] = (uint8_t)(QW_VOICE_MIN_ZERO_CROSSING_RATE - 1U);
    CHECK(!qw_voice_frame_is_usable(feature, 2000U, 500U));
    feature[8] = QW_VOICE_MIN_ZERO_CROSSING_RATE;
    CHECK(qw_voice_frame_is_usable(feature, 2000U, 500U));
    CHECK(!qw_voice_frame_is_usable(feature, 499U, 500U));
    CHECK(!qw_voice_frame_is_usable(NULL, 2000U, 500U));
}

static void test_segmenter_trims_silence_and_reports_overflow(void)
{
    qw_voice_segmenter_t segmenter;
    qw_voice_segmenter_init(&segmenter);
    qw_voice_feature_t feature;
    qw_voice_segment_event_t event;
    make_feature(feature, 20U);

    for (unsigned i = 0; i < 3U; ++i) {
        CHECK(qw_voice_segmenter_push(&segmenter, feature, false, &event));
        CHECK(!event.complete);
    }
    for (unsigned i = 0; i < 5U; ++i) {
        feature[0] = (uint8_t)(20U + i);
        CHECK(qw_voice_segmenter_push(&segmenter, feature, true, &event));
        CHECK(!event.complete);
    }
    for (unsigned i = 0; i < QW_VOICE_END_SILENCE_FRAMES - 1U; ++i) {
        CHECK(qw_voice_segmenter_push(&segmenter, feature, false, &event));
        CHECK(!event.complete);
    }
    CHECK(qw_voice_segmenter_push(&segmenter, feature, false, &event));
    CHECK(event.complete);
    CHECK(!event.overflow);
    CHECK_INT_EQ(event.frame_count, 5);

    qw_voice_segmenter_reset(&segmenter);
    for (unsigned i = 0; i < QW_VOICE_MAX_UTTERANCE_FRAMES + 1U; ++i) {
        CHECK(qw_voice_segmenter_push(&segmenter, feature, true, &event));
    }
    for (unsigned i = 0; i < QW_VOICE_END_SILENCE_FRAMES; ++i) {
        CHECK(qw_voice_segmenter_push(&segmenter, feature, false, &event));
    }
    CHECK(event.complete);
    CHECK(event.overflow);
}

static void test_matching_template_and_dtw_distance(void)
{
    qw_voice_template_t template_data;
    make_template(&template_data, 30U, 6U);
    qw_voice_keyword_state_t state;
    CHECK(qw_voice_keyword_init(&state, &template_data, 5U, 0U));

    qw_voice_keyword_event_t event;
    push_phrase(&state, 30U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.utterance_complete);
    CHECK(event.template_available);
    CHECK(event.matched);
    CHECK(qw_voice_keyword_event_requests_mama_audio(&event));
    CHECK(event.distance <= 5U);

    qw_voice_feature_t different[6];
    for (size_t i = 0; i < 6U; ++i) {
        make_feature(different[i], 220U);
    }
    uint16_t distance = 0U;
    CHECK(qw_voice_keyword_distance(&template_data, different, 6U,
                                   &distance));
    CHECK(distance > 5U);
}

static void test_nonmatch_no_template_and_cooldown(void)
{
    qw_voice_template_t template_data;
    make_template(&template_data, 10U, 6U);
    qw_voice_keyword_state_t state;
    CHECK(qw_voice_keyword_init(&state, &template_data, 5U, 20U));

    qw_voice_keyword_event_t event;
    push_phrase(&state, 200U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.utterance_complete);
    CHECK(!event.matched);
    CHECK(!qw_voice_keyword_event_requests_mama_audio(&event));

    push_phrase(&state, 10U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.matched);

    push_phrase(&state, 10U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.utterance_complete);
    CHECK(!event.matched); /* cooldown suppresses a repeated phrase */

    CHECK(qw_voice_keyword_init(&state, NULL, 5U, 0U));
    push_phrase(&state, 10U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.utterance_complete);
    CHECK(!event.template_available);
    CHECK(!event.matched);
    CHECK(!qw_voice_keyword_event_requests_mama_audio(&event));
    CHECK(!qw_voice_keyword_event_requests_mama_audio(NULL));
}

static void test_conservative_runtime_threshold_rejects_distance_50(void)
{
    qw_voice_template_t template_data;
    make_template(&template_data, 30U, 6U);
    qw_voice_keyword_state_t state;
    CHECK(qw_voice_keyword_init(&state, &template_data, 40U, 0U));

    qw_voice_keyword_event_t event;
    push_phrase(&state, 80U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.utterance_complete);
    CHECK_INT_EQ(event.distance, 50);
    CHECK(!event.matched);

    push_phrase(&state, 30U, 6U, &event);
    push_silence(&state, QW_VOICE_END_SILENCE_FRAMES, &event);
    CHECK(event.matched);
}

static void test_invalid_templates_are_rejected(void)
{
    qw_voice_template_t template_data;
    qw_voice_feature_t frames[QW_VOICE_MAX_TEMPLATE_FRAMES];
    CHECK(!qw_voice_template_set(&template_data, frames,
                                 QW_VOICE_MIN_UTTERANCE_FRAMES - 1U));
    CHECK(!qw_voice_template_set(&template_data, frames,
                                 QW_VOICE_MAX_TEMPLATE_FRAMES + 1U));
    CHECK(!qw_voice_template_is_valid(NULL));
    CHECK(!qw_voice_keyword_init(NULL, NULL, 1U, 1U));
}

static void test_five_sample_template_aggregation(void)
{
    qw_voice_template_accumulator_t accumulator;
    qw_voice_template_accumulator_init(&accumulator);
    qw_voice_template_t template_data;
    CHECK(!qw_voice_template_accumulator_finalize(&accumulator,
                                                  &template_data));

    uint32_t expected[QW_VOICE_MIN_UTTERANCE_FRAMES] = {0};
    for (unsigned sample = 0U; sample < QW_VOICE_ENROLL_SAMPLE_COUNT;
         ++sample) {
        const size_t source_count = QW_VOICE_MIN_UTTERANCE_FRAMES + sample;
        qw_voice_feature_t frames[QW_VOICE_MAX_UTTERANCE_FRAMES];
        for (size_t source = 0U; source < source_count; ++source) {
            const uint8_t value = (uint8_t)(10U + sample * 10U + source);
            for (size_t dimension = 0U; dimension < QW_VOICE_FEATURE_DIM;
                 ++dimension) {
                frames[source][dimension] = value;
            }
        }
        for (size_t target = 0U; target < QW_VOICE_MIN_UTTERANCE_FRAMES;
             ++target) {
            const size_t source =
                (target * (source_count - 1U) +
                 (QW_VOICE_MIN_UTTERANCE_FRAMES - 1U) / 2U) /
                (QW_VOICE_MIN_UTTERANCE_FRAMES - 1U);
            expected[target] += frames[source][0];
        }
        CHECK(qw_voice_template_accumulator_add(&accumulator, frames,
                                                source_count));
    }

    qw_voice_feature_t extra_frames[QW_VOICE_MIN_UTTERANCE_FRAMES] = {{0}};
    CHECK(!qw_voice_template_accumulator_add(
        &accumulator, extra_frames, QW_VOICE_MIN_UTTERANCE_FRAMES));
    CHECK(qw_voice_template_accumulator_finalize(&accumulator, &template_data));
    CHECK_INT_EQ(template_data.frame_count, QW_VOICE_MIN_UTTERANCE_FRAMES);
    for (size_t frame = 0U; frame < QW_VOICE_MIN_UTTERANCE_FRAMES; ++frame) {
        const uint8_t expected_value = (uint8_t)(
            (expected[frame] + QW_VOICE_ENROLL_SAMPLE_COUNT / 2U) /
            QW_VOICE_ENROLL_SAMPLE_COUNT);
        for (size_t dimension = 0U; dimension < QW_VOICE_FEATURE_DIM;
             ++dimension) {
            CHECK_INT_EQ(template_data.frames[frame][dimension],
                         expected_value);
        }
    }
}

static void test_two_speaker_matchers_are_independent(void)
{
    qw_voice_template_t templates[QW_VOICE_MAX_SPEAKERS];
    make_template(&templates[0], 10U, 6U);
    make_template(&templates[1], 100U, 6U);

    qw_voice_keyword_state_t states[QW_VOICE_MAX_SPEAKERS];
    for (size_t speaker = 0U; speaker < QW_VOICE_MAX_SPEAKERS; ++speaker) {
        CHECK(qw_voice_keyword_init(&states[speaker], &templates[speaker],
                                    5U, 0U));
    }

    qw_voice_keyword_event_t events[QW_VOICE_MAX_SPEAKERS];
    push_phrase(&states[0], 10U, 6U, &events[0]);
    push_phrase(&states[1], 10U, 6U, &events[1]);
    push_silence(&states[0], QW_VOICE_END_SILENCE_FRAMES, &events[0]);
    push_silence(&states[1], QW_VOICE_END_SILENCE_FRAMES, &events[1]);
    CHECK(events[0].matched);
    CHECK(!events[1].matched);

    push_phrase(&states[0], 100U, 6U, &events[0]);
    push_phrase(&states[1], 100U, 6U, &events[1]);
    push_silence(&states[0], QW_VOICE_END_SILENCE_FRAMES, &events[0]);
    push_silence(&states[1], QW_VOICE_END_SILENCE_FRAMES, &events[1]);
    CHECK(!events[0].matched);
    CHECK(events[1].matched);
}

int main(void)
{
    RUN_TEST(test_extracts_features_and_rms);
    RUN_TEST(test_usable_frame_rejects_one_sided_burst);
    RUN_TEST(test_segmenter_trims_silence_and_reports_overflow);
    RUN_TEST(test_matching_template_and_dtw_distance);
    RUN_TEST(test_nonmatch_no_template_and_cooldown);
    RUN_TEST(test_conservative_runtime_threshold_rejects_distance_50);
    RUN_TEST(test_invalid_templates_are_rejected);
    RUN_TEST(test_five_sample_template_aggregation);
    RUN_TEST(test_two_speaker_matchers_are_independent);
    printf("test_voice_keyword: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
