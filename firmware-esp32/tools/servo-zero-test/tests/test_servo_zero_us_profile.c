#include <assert.h>
#include <stdint.h>

#include "../main/servo_zero_us_profile.h"

static servo_zero_us_leg_profile_t make_profile(void)
{
    return (servo_zero_us_leg_profile_t){
        .support = {.hip_us = 1500u, .knee_us = 1500u},
        .lift = {.hip_us = 1500u, .knee_us = 1200u},
        .swing = {.hip_us = 1800u, .knee_us = 1200u},
        .hip_min_us = 1000u,
        .hip_max_us = 2200u,
        .knee_min_us = 900u,
        .knee_max_us = 1800u,
        .search_limit_us = 1500u,
        .search_step_us = 10u,
        .max_search_steps = 100u,
        .search_direction = 1,
        .load_offset_us = 25,
    };
}

static void test_us_interpolation_is_continuous(void)
{
    uint16_t previous = 1000u;
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= 1000u;
         elapsed_ms += 20u) {
        const uint16_t value = servo_zero_us_minimum_jerk_lerp(
            1000u, 2000u, elapsed_ms, 1000u);
        assert(value >= previous);
        assert(value <= 2000u);
        assert((uint16_t)(value - previous) <= 40u);
        previous = value;
    }
    assert(previous == 2000u);
    assert(servo_zero_us_minimum_jerk_lerp(1000u, 2000u, 0u, 1000u) ==
           1000u);
    assert(servo_zero_us_minimum_jerk_lerp(1000u, 2000u, 500u, 1000u) ==
           1500u);
    assert(servo_zero_us_minimum_jerk_lerp(1000u, 2000u, 1000u, 1000u) ==
           2000u);
    assert(servo_zero_us_minimum_jerk_lerp(1000u, 2000u, 1u, 0u) ==
           2000u);

    previous = 2000u;
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= 1000u;
         elapsed_ms += 20u) {
        const uint16_t value = servo_zero_us_minimum_jerk_lerp(
            2000u, 1000u, elapsed_ms, 1000u);
        assert(value <= previous);
        assert(value >= 1000u);
        assert((uint16_t)(previous - value) <= 40u);
        previous = value;
    }
    assert(previous == 1000u);

    const servo_zero_us_leg_pose_t pose = servo_zero_us_leg_pose_lerp(
        (servo_zero_us_leg_pose_t){.hip_us = 1000u, .knee_us = 1800u},
        (servo_zero_us_leg_pose_t){.hip_us = 1800u, .knee_us = 1000u}, 500u,
        1000u);
    assert(pose.hip_us == 1400u && pose.knee_us == 1400u);
}

static void test_profile_validation(void)
{
    servo_zero_us_leg_profile_t profile = make_profile();
    assert(servo_zero_us_leg_profile_valid(&profile));

    profile.search_direction = 0;
    assert(!servo_zero_us_leg_profile_valid(&profile));
    profile = make_profile();
    profile.search_limit_us = 800u;
    assert(!servo_zero_us_leg_profile_valid(&profile));
    profile = make_profile();
    profile.swing.knee_us = 1600u;
    assert(!servo_zero_us_leg_profile_valid(&profile));
    profile = make_profile();
    profile.hip_min_us = 1900u;
    assert(!servo_zero_us_leg_profile_valid(&profile));
    profile = make_profile();
    profile.search_step_us = 0u;
    assert(!servo_zero_us_leg_profile_valid(&profile));
}

static void test_contact_is_latched_in_pulse_space(void)
{
    const servo_zero_us_leg_profile_t profile = make_profile();
    servo_zero_us_touchdown_t touchdown;
    servo_zero_us_leg_pose_t target;
    servo_zero_us_leg_pose_t contact_pose;
    servo_zero_us_leg_pose_t load_pose;

    assert(servo_zero_us_touchdown_start(&profile, 1820u, 1200u,
                                         &touchdown));
    for (uint16_t expected_knee_us = 1210u; expected_knee_us <= 1240u;
         expected_knee_us += 10u) {
        assert(servo_zero_us_touchdown_next(&touchdown, &target));
        assert(target.hip_us == 1820u);
        assert(target.knee_us == expected_knee_us);
    }
    assert(touchdown.steps == 4u);
    assert(servo_zero_us_touchdown_accept_contact(&touchdown));
    assert(touchdown.phase == SERVO_ZERO_US_TOUCHDOWN_CONTACT);
    assert(servo_zero_us_touchdown_contact_pose(&touchdown, &contact_pose));
    assert(contact_pose.hip_us == 1820u && contact_pose.knee_us == 1240u);
    assert(servo_zero_us_touchdown_load_pose(&touchdown, &load_pose));
    assert(load_pose.hip_us == 1820u && load_pose.knee_us == 1265u);
    assert(!servo_zero_us_touchdown_next(&touchdown, &target));
    assert(!servo_zero_us_touchdown_accept_contact(&touchdown));
}

static void test_search_never_passes_limit_and_reports_exhaustion(void)
{
    servo_zero_us_leg_profile_t profile = make_profile();
    servo_zero_us_touchdown_t touchdown;
    servo_zero_us_leg_pose_t target;

    assert(servo_zero_us_touchdown_start(&profile, 1800u, 1495u,
                                         &touchdown));
    assert(servo_zero_us_touchdown_next(&touchdown, &target));
    assert(target.hip_us == 1800u && target.knee_us == 1500u);
    assert(!servo_zero_us_touchdown_next(&touchdown, &target));
    assert(touchdown.phase == SERVO_ZERO_US_TOUCHDOWN_EXHAUSTED);

    profile.search_direction = -1;
    profile.search_limit_us = 1000u;
    assert(servo_zero_us_leg_profile_valid(&profile));
    assert(servo_zero_us_touchdown_start(&profile, 1800u, 1200u,
                                         &touchdown));
    assert(servo_zero_us_touchdown_next(&touchdown, &target));
    assert(target.hip_us == 1800u && target.knee_us == 1190u);

    profile = make_profile();
    profile.max_search_steps = 1u;
    assert(servo_zero_us_touchdown_start(&profile, 1800u, 1200u,
                                         &touchdown));
    assert(servo_zero_us_touchdown_next(&touchdown, &target));
    assert(!servo_zero_us_touchdown_next(&touchdown, &target));
    assert(touchdown.phase == SERVO_ZERO_US_TOUCHDOWN_EXHAUSTED);

    assert(!servo_zero_us_touchdown_start(&profile, 1800u, 1510u,
                                          &touchdown));
    assert(touchdown.phase == SERVO_ZERO_US_TOUCHDOWN_INVALID);
}

static void test_load_offset_is_clamped(void)
{
    servo_zero_us_leg_profile_t profile = make_profile();
    profile.knee_max_us = 1500u;
    profile.load_offset_us = 100;
    servo_zero_us_touchdown_t touchdown;
    servo_zero_us_leg_pose_t target;
    servo_zero_us_leg_pose_t load_pose;

    assert(servo_zero_us_leg_profile_valid(&profile));
    assert(servo_zero_us_touchdown_start(&profile, 1800u, 1490u,
                                         &touchdown));
    assert(servo_zero_us_touchdown_next(&touchdown, &target));
    assert(target.knee_us == 1500u);
    assert(servo_zero_us_touchdown_accept_contact(&touchdown));
    assert(servo_zero_us_touchdown_load_pose(&touchdown, &load_pose));
    assert(load_pose.knee_us == 1500u);
}

int main(void)
{
    test_us_interpolation_is_continuous();
    test_profile_validation();
    test_contact_is_latched_in_pulse_space();
    test_search_never_passes_limit_and_reports_exhaustion();
    test_load_offset_is_clamped();
    return 0;
}
