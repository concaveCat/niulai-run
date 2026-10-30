#include "fall_detector.h"
#include "qw_check.h"

static void test_requires_three_over_limit_samples(void)
{
    qw_fall_detector_t detector;
    qw_fall_detector_init(&detector);
    CHECK(!qw_fall_detector_update(&detector, 60.1f, 0.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 60.1f, 0.0f, true));
    CHECK(qw_fall_detector_update(&detector, 60.1f, 0.0f, true));
    CHECK(qw_fall_detector_update(&detector, 0.0f, 0.0f, true));
}

static void test_boundary_and_pitch(void)
{
    qw_fall_detector_t detector;
    qw_fall_detector_init(&detector);
    CHECK(!qw_fall_detector_update(&detector, 60.0f, 0.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 0.0f, -60.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 0.0f, 60.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 59.9f, 0.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 0.0f, -60.1f, true));
    CHECK(!qw_fall_detector_update(&detector, 0.0f, -60.1f, true));
    CHECK(qw_fall_detector_update(&detector, 0.0f, -60.1f, true));
}

static void test_noise_and_hysteresis(void)
{
    qw_fall_detector_t detector;
    qw_fall_detector_init(&detector);
    CHECK(!qw_fall_detector_update(&detector, 61.0f, 0.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 10.0f, 0.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 61.0f, 0.0f, true));
    CHECK(!qw_fall_detector_update(&detector, 10.0f, 0.0f, true));
    for (unsigned i = 0; i < 2U; i++) {
        CHECK(!qw_fall_detector_update(&detector, 0.0f, 61.0f, true));
    }
    CHECK(qw_fall_detector_update(&detector, 0.0f, 61.0f, true));
    CHECK(qw_fall_detector_update(&detector, 49.9f, 0.0f, true));
    for (unsigned i = 0; i < 23U; i++) {
        CHECK(qw_fall_detector_update(&detector, 49.9f, 0.0f, true));
    }
    CHECK(!qw_fall_detector_update(&detector, 49.9f, 0.0f, true));
}

static void test_invalid_samples_do_not_clear_latched_state(void)
{
    qw_fall_detector_t detector;
    qw_fall_detector_init(&detector);
    for (unsigned i = 0; i < 3U; i++) {
        CHECK(qw_fall_detector_update(&detector, 0.0f, 61.0f, true) ||
              i < 2U);
    }
    CHECK(qw_fall_detector_update(&detector, 0.0f, 0.0f, false));
}

int main(void)
{
    RUN_TEST(test_requires_three_over_limit_samples);
    RUN_TEST(test_boundary_and_pitch);
    RUN_TEST(test_noise_and_hysteresis);
    RUN_TEST(test_invalid_samples_do_not_clear_latched_state);
    printf("test_fall_detector: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
