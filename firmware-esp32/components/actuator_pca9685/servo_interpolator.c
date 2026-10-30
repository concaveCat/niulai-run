#include "servo_interpolator.h"

#include <math.h>
#include <stddef.h>

static float clampf(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

void qw_servo_interpolator_reset(qw_servo_interpolator_t *state,
                                 uint16_t position_us)
{
    if (state == NULL) {
        return;
    }
    state->position_us = (float)position_us;
    state->velocity_us_s = 0.0f;
    state->initialized = true;
}

uint16_t qw_servo_interpolator_step(qw_servo_interpolator_t *state,
                                    uint16_t target_us, float dt_s,
                                    const qw_servo_limits_t *limits)
{
    if (state == NULL || limits == NULL || !isfinite(dt_s) || dt_s <= 0.0f ||
        !isfinite(limits->max_velocity_us_s) ||
        !isfinite(limits->max_acceleration_us_s2) ||
        limits->max_velocity_us_s <= 0.0f ||
        limits->max_acceleration_us_s2 <= 0.0f) {
        return target_us;
    }
    if (!state->initialized) {
        qw_servo_interpolator_reset(state, target_us);
        return target_us;
    }

    const float error = (float)target_us - state->position_us;
    if (fabsf(error) < 0.5f && fabsf(state->velocity_us_s) <
                                  limits->max_acceleration_us_s2 * dt_s) {
        qw_servo_interpolator_reset(state, target_us);
        return target_us;
    }

    /* Velocity needed to stop at the target under the acceleration limit.
     * This creates an online trapezoidal/S-curve-like profile even when the
     * upper layer replaces the target on every control tick. */
    const float braking_velocity =
        sqrtf(2.0f * limits->max_acceleration_us_s2 * fabsf(error));
    const float desired_velocity = copysignf(
        fminf(limits->max_velocity_us_s, braking_velocity), error);
    const float velocity_delta = limits->max_acceleration_us_s2 * dt_s;
    state->velocity_us_s += clampf(desired_velocity - state->velocity_us_s,
                                   -velocity_delta, velocity_delta);
    const float next = state->position_us + state->velocity_us_s * dt_s;
    if ((error > 0.0f && next >= (float)target_us) ||
        (error < 0.0f && next <= (float)target_us)) {
        qw_servo_interpolator_reset(state, target_us);
    } else {
        state->position_us = next;
    }
    return (uint16_t)lroundf(state->position_us);
}
