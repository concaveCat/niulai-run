/* Pure control-side admission boundary between the safety gate and ARD-1.
 * Input seen before actuator authorization is only link-health evidence; it
 * must never become a delayed motion edge after the gate opens. */
#ifndef CONTROL_ADMISSION_H
#define CONTROL_ADMISSION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool was_armed;
    bool neutral_seen;
} qw_control_admission_t;

typedef struct {
    bool reset_fsm;
    bool allow_tick;
} qw_control_admission_result_t;

static inline qw_control_admission_result_t qw_control_admission_step(
    qw_control_admission_t *state, bool armed, uint8_t leg_mask)
{
    qw_control_admission_result_t result = {0};
    if (state == NULL) {
        return result;
    }

    if (!armed) {
        result.reset_fsm = state->was_armed;
        state->was_armed = false;
        state->neutral_seen = false;
        return result;
    }

    if (!state->was_armed) {
        state->was_armed = true;
        state->neutral_seen = false;
        result.reset_fsm = true;
    }

    if (!state->neutral_seen) {
        if (leg_mask == 0U) {
            state->neutral_seen = true;
        }
        return result;
    }

    result.allow_tick = true;
    return result;
}

#endif /* CONTROL_ADMISSION_H */
