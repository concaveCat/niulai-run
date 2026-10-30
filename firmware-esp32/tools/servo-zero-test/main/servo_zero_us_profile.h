#ifndef NIULAI_SERVO_ZERO_US_PROFILE_H
#define NIULAI_SERVO_ZERO_US_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

/* A leg pose is deliberately expressed only in the two calibrated pulse
 * values.  The names describe the experiment phase, not a physical angle;
 * support is the gameplay rest/support pose, not the servo neutral pose. */
typedef struct {
    uint16_t hip_us;
    uint16_t knee_us;
} servo_zero_us_leg_pose_t;

/*
 * Per-leg pulse ruler.
 *
 * search_limit_us is a hard software boundary, not the desired touchdown
 * pose.  The actual touchdown value is learned from a debounced contact
 * event, so the foot does not have to be vertical at contact.
 */
typedef struct {
    servo_zero_us_leg_pose_t support;
    servo_zero_us_leg_pose_t lift;
    servo_zero_us_leg_pose_t swing;
    uint16_t hip_min_us;
    uint16_t hip_max_us;
    uint16_t knee_min_us;
    uint16_t knee_max_us;
    uint16_t search_limit_us;
    uint16_t search_step_us;
    uint16_t max_search_steps;
    int8_t search_direction;
    int16_t load_offset_us;
} servo_zero_us_leg_profile_t;

typedef enum {
    SERVO_ZERO_US_TOUCHDOWN_INVALID = 0,
    SERVO_ZERO_US_TOUCHDOWN_SEARCHING,
    SERVO_ZERO_US_TOUCHDOWN_CONTACT,
    SERVO_ZERO_US_TOUCHDOWN_EXHAUSTED,
} servo_zero_us_touchdown_phase_t;

/* Runtime state for a monotonic knee-pulse search.  The hip pulse is copied
 * at start and remains frozen throughout the search. */
typedef struct {
    servo_zero_us_touchdown_phase_t phase;
    uint16_t hip_us;
    uint16_t current_knee_us;
    uint16_t contact_knee_us;
    uint16_t load_knee_us;
    uint16_t steps;
    uint16_t knee_min_us;
    uint16_t knee_max_us;
    uint16_t search_limit_us;
    uint16_t search_step_us;
    uint16_t max_search_steps;
    int8_t search_direction;
    int16_t load_offset_us;
} servo_zero_us_touchdown_t;

/* Validate a complete pulse-space table before it is used by a controller. */
bool servo_zero_us_leg_profile_valid(
    const servo_zero_us_leg_profile_t *profile);

/* Integer minimum-jerk interpolation.  It has no angle or floating point
 * dependency and returns an endpoint for duration==0 or elapsed>=duration. */
uint16_t servo_zero_us_minimum_jerk_lerp(
    uint16_t start_us, uint16_t end_us, uint32_t elapsed_ms,
    uint32_t duration_ms);
servo_zero_us_leg_pose_t servo_zero_us_leg_pose_lerp(
    servo_zero_us_leg_pose_t start, servo_zero_us_leg_pose_t end,
    uint32_t elapsed_ms, uint32_t duration_ms);

/* Start searching from the currently commanded pulse after hip swing. */
bool servo_zero_us_touchdown_start(
    const servo_zero_us_leg_profile_t *profile, uint16_t hip_us,
    uint16_t knee_us, servo_zero_us_touchdown_t *touchdown);

/* Produce the next knee target.  The returned target is always inside the
 * configured range and never passes search_limit_us.  False means the
 * search is no longer active (contact must be checked before calling this). */
bool servo_zero_us_touchdown_next(
    servo_zero_us_touchdown_t *touchdown,
    servo_zero_us_leg_pose_t *target);

/* Accept one already-debounced contact event and latch the exact pulse. */
bool servo_zero_us_touchdown_accept_contact(
    servo_zero_us_touchdown_t *touchdown);
bool servo_zero_us_touchdown_contact_pose(
    const servo_zero_us_touchdown_t *touchdown,
    servo_zero_us_leg_pose_t *pose);
bool servo_zero_us_touchdown_load_pose(
    const servo_zero_us_touchdown_t *touchdown,
    servo_zero_us_leg_pose_t *pose);

#endif
