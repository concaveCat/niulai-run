#include "servo_zero_us_profile.h"

#include <limits.h>
#include <stddef.h>

#define SERVO_ZERO_US_Q16 (65536ULL)

static bool pulse_in_range(uint16_t pulse_us, uint16_t min_us,
                           uint16_t max_us)
{
    return pulse_us >= min_us && pulse_us <= max_us;
}

static uint32_t minimum_jerk_q16(uint32_t elapsed_ms, uint32_t duration_ms)
{
    if (duration_ms == 0u || elapsed_ms >= duration_ms) {
        return (uint32_t)SERVO_ZERO_US_Q16;
    }

    /* Q16 is sufficient for a 16-bit pulse and keeps every intermediate
     * multiplication inside uint64_t on the ESP32 and on the host. */
    const uint64_t t = ((uint64_t)elapsed_ms * SERVO_ZERO_US_Q16) /
                       (uint64_t)duration_ms;
    const uint64_t t2 = (t * t) / SERVO_ZERO_US_Q16;
    const uint64_t t3 = (t2 * t) / SERVO_ZERO_US_Q16;
    const int64_t term = (int64_t)(10ULL * SERVO_ZERO_US_Q16) -
                         (int64_t)(15ULL * t) + (int64_t)(6ULL * t2);
    if (term <= 0) {
        return 0u;
    }
    return (uint32_t)((t3 * (uint64_t)term + SERVO_ZERO_US_Q16 / 2ULL) /
                      SERVO_ZERO_US_Q16);
}

static int64_t round_q16(int64_t numerator)
{
    if (numerator >= 0) {
        return (numerator + (int64_t)(SERVO_ZERO_US_Q16 / 2ULL)) /
               (int64_t)SERVO_ZERO_US_Q16;
    }
    return -((-numerator + (int64_t)(SERVO_ZERO_US_Q16 / 2ULL)) /
             (int64_t)SERVO_ZERO_US_Q16);
}

static uint16_t add_clamped(uint16_t pulse_us, int16_t offset_us,
                            uint16_t min_us, uint16_t max_us)
{
    int32_t value = (int32_t)pulse_us + (int32_t)offset_us;
    if (value < (int32_t)min_us) {
        value = (int32_t)min_us;
    } else if (value > (int32_t)max_us) {
        value = (int32_t)max_us;
    }
    return (uint16_t)value;
}

bool servo_zero_us_leg_profile_valid(
    const servo_zero_us_leg_profile_t *profile)
{
    if (profile == NULL || profile->hip_min_us > profile->hip_max_us ||
        profile->knee_min_us > profile->knee_max_us ||
        profile->search_step_us == 0u || profile->max_search_steps == 0u ||
        (profile->search_direction != 1 &&
         profile->search_direction != -1)) {
        return false;
    }

    if (!pulse_in_range(profile->support.hip_us, profile->hip_min_us,
                        profile->hip_max_us) ||
        !pulse_in_range(profile->lift.hip_us, profile->hip_min_us,
                        profile->hip_max_us) ||
        !pulse_in_range(profile->swing.hip_us, profile->hip_min_us,
                        profile->hip_max_us) ||
        !pulse_in_range(profile->support.knee_us, profile->knee_min_us,
                        profile->knee_max_us) ||
        !pulse_in_range(profile->lift.knee_us, profile->knee_min_us,
                        profile->knee_max_us) ||
        !pulse_in_range(profile->swing.knee_us, profile->knee_min_us,
                        profile->knee_max_us) ||
        !pulse_in_range(profile->search_limit_us, profile->knee_min_us,
                        profile->knee_max_us)) {
        return false;
    }

    if (profile->search_direction > 0) {
        return profile->swing.knee_us <= profile->search_limit_us;
    }
    return profile->swing.knee_us >= profile->search_limit_us;
}

uint16_t servo_zero_us_minimum_jerk_lerp(
    uint16_t start_us, uint16_t end_us, uint32_t elapsed_ms,
    uint32_t duration_ms)
{
    if (duration_ms == 0u || elapsed_ms >= duration_ms) {
        return end_us;
    }

    const int64_t delta = (int64_t)end_us - (int64_t)start_us;
    const int64_t offset = round_q16(
        delta * (int64_t)minimum_jerk_q16(elapsed_ms, duration_ms));
    int64_t value = (int64_t)start_us + offset;
    if (value < 0) {
        value = 0;
    } else if (value > (int64_t)UINT16_MAX) {
        value = (int64_t)UINT16_MAX;
    }
    return (uint16_t)value;
}

servo_zero_us_leg_pose_t servo_zero_us_leg_pose_lerp(
    servo_zero_us_leg_pose_t start, servo_zero_us_leg_pose_t end,
    uint32_t elapsed_ms, uint32_t duration_ms)
{
    return (servo_zero_us_leg_pose_t){
        .hip_us = servo_zero_us_minimum_jerk_lerp(
            start.hip_us, end.hip_us, elapsed_ms, duration_ms),
        .knee_us = servo_zero_us_minimum_jerk_lerp(
            start.knee_us, end.knee_us, elapsed_ms, duration_ms),
    };
}

bool servo_zero_us_touchdown_start(
    const servo_zero_us_leg_profile_t *profile, uint16_t hip_us,
    uint16_t knee_us, servo_zero_us_touchdown_t *touchdown)
{
    if (touchdown == NULL) {
        return false;
    }
    *touchdown = (servo_zero_us_touchdown_t){
        .phase = SERVO_ZERO_US_TOUCHDOWN_INVALID,
    };
    if (!servo_zero_us_leg_profile_valid(profile) ||
        !pulse_in_range(hip_us, profile->hip_min_us, profile->hip_max_us) ||
        !pulse_in_range(knee_us, profile->knee_min_us,
                        profile->knee_max_us) ||
        (profile->search_direction > 0 &&
         knee_us > profile->search_limit_us) ||
        (profile->search_direction < 0 &&
         knee_us < profile->search_limit_us)) {
        return false;
    }

    touchdown->phase = SERVO_ZERO_US_TOUCHDOWN_SEARCHING;
    touchdown->hip_us = hip_us;
    touchdown->current_knee_us = knee_us;
    touchdown->knee_min_us = profile->knee_min_us;
    touchdown->knee_max_us = profile->knee_max_us;
    touchdown->search_limit_us = profile->search_limit_us;
    touchdown->search_step_us = profile->search_step_us;
    touchdown->max_search_steps = profile->max_search_steps;
    touchdown->search_direction = profile->search_direction;
    touchdown->load_offset_us = profile->load_offset_us;
    return true;
}

bool servo_zero_us_touchdown_next(
    servo_zero_us_touchdown_t *touchdown,
    servo_zero_us_leg_pose_t *target)
{
    if (touchdown == NULL || target == NULL ||
        touchdown->phase != SERVO_ZERO_US_TOUCHDOWN_SEARCHING) {
        return false;
    }
    if (touchdown->steps >= touchdown->max_search_steps) {
        touchdown->phase = SERVO_ZERO_US_TOUCHDOWN_EXHAUSTED;
        return false;
    }

    int32_t candidate = (int32_t)touchdown->current_knee_us +
                        (int32_t)touchdown->search_direction *
                            (int32_t)touchdown->search_step_us;
    if (touchdown->search_direction > 0 &&
        candidate > (int32_t)touchdown->search_limit_us) {
        candidate = (int32_t)touchdown->search_limit_us;
    } else if (touchdown->search_direction < 0 &&
               candidate < (int32_t)touchdown->search_limit_us) {
        candidate = (int32_t)touchdown->search_limit_us;
    }
    if (candidate < (int32_t)touchdown->knee_min_us ||
        candidate > (int32_t)touchdown->knee_max_us ||
        candidate == (int32_t)touchdown->current_knee_us) {
        touchdown->phase = SERVO_ZERO_US_TOUCHDOWN_EXHAUSTED;
        return false;
    }

    touchdown->current_knee_us = (uint16_t)candidate;
    touchdown->steps++;
    *target = (servo_zero_us_leg_pose_t){
        .hip_us = touchdown->hip_us,
        .knee_us = touchdown->current_knee_us,
    };
    return true;
}

bool servo_zero_us_touchdown_accept_contact(
    servo_zero_us_touchdown_t *touchdown)
{
    if (touchdown == NULL ||
        touchdown->phase != SERVO_ZERO_US_TOUCHDOWN_SEARCHING) {
        return false;
    }
    touchdown->contact_knee_us = touchdown->current_knee_us;
    touchdown->load_knee_us = add_clamped(
        touchdown->contact_knee_us, touchdown->load_offset_us,
        touchdown->knee_min_us, touchdown->knee_max_us);
    touchdown->phase = SERVO_ZERO_US_TOUCHDOWN_CONTACT;
    return true;
}

bool servo_zero_us_touchdown_contact_pose(
    const servo_zero_us_touchdown_t *touchdown,
    servo_zero_us_leg_pose_t *pose)
{
    if (touchdown == NULL || pose == NULL ||
        touchdown->phase != SERVO_ZERO_US_TOUCHDOWN_CONTACT) {
        return false;
    }
    *pose = (servo_zero_us_leg_pose_t){
        .hip_us = touchdown->hip_us,
        .knee_us = touchdown->contact_knee_us,
    };
    return true;
}

bool servo_zero_us_touchdown_load_pose(
    const servo_zero_us_touchdown_t *touchdown,
    servo_zero_us_leg_pose_t *pose)
{
    if (touchdown == NULL || pose == NULL ||
        touchdown->phase != SERVO_ZERO_US_TOUCHDOWN_CONTACT) {
        return false;
    }
    *pose = (servo_zero_us_leg_pose_t){
        .hip_us = touchdown->hip_us,
        .knee_us = touchdown->load_knee_us,
    };
    return true;
}
