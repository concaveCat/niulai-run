/* C11 port of firmware/control/leg_fsm.py.
 *
 * The update() body mirrors the Python line by line: sanitize dt first
 * (non-finite or negative -> self-transition "invalid input ignored",
 * dt = 0.0), state_elapsed always accumulates, rising edge, then the branch
 * order that is observable within a single tick:
 *   LIFT:    state timeout (strictly `>`) BEFORE lift complete (`>=`)
 *   SWING:   release BEFORE progress accumulation, then state timeout
 *   SEARCH:  contact BEFORE state timeout
 *   FAULT:   terminal, no branch
 * Command mapping matches Python command(): lift progress is
 * min(1.0, elapsed/lift_duration) or 1.0 when lift_duration == 0; search
 * progress is uncapped; hold_support/safe_stop report progress 0.0.
 *
 * The Python non-bool-input sanitize branch has no C equivalent (bool
 * parameters) - see the file header of leg_fsm.h. The dt branch is kept and
 * mirrors test #5 of firmware/tests/test_leg_fsm.py for dt = -1.0.
 */
#include "leg_fsm.h"

#include <math.h>
#include <string.h>

static bool finite_nonneg(float v)
{
    return isfinite(v) && v >= 0.0f;
}

bool leg_fsm_init(leg_fsm_t *fsm, const leg_fsm_config_t *config)
{
    if (fsm == NULL || config == NULL) {
        return false;
    }
    if (!finite_nonneg(config->lift_duration_s) ||
        !finite_nonneg(config->swing_speed) ||
        !finite_nonneg(config->search_speed) ||
        !finite_nonneg(config->max_swing)) {
        return false;
    }
    /* timeout: disabled (<= 0) or finite strictly positive */
    if (!isfinite(config->state_timeout_s)) {
        return false;
    }

    memset(fsm, 0, sizeof(*fsm));
    fsm->config = *config;
    fsm->state = LEG_STATE_SUPPORT;
    return true;
}

static void transition(leg_fsm_t *fsm, leg_state_t to, const char *reason)
{
    leg_transition_t t = {fsm->state, to, reason, fsm->state_elapsed_s};

    fsm->ring[fsm->transition_total % LEG_FSM_TRANSITION_RING_SIZE] = t;
    fsm->transition_total++;
    fsm->state = to;
    fsm->state_elapsed_s = 0.0f;
    if (to == LEG_STATE_SWING) {
        fsm->swing_progress = 0.0f;
    }
}

static void record_invalid_input(leg_fsm_t *fsm)
{
    transition(fsm, fsm->state, "invalid input ignored");
}

static bool timed_out(const leg_fsm_t *fsm)
{
    return fsm->config.state_timeout_s > 0.0f &&
           fsm->state_elapsed_s > fsm->config.state_timeout_s;
}

static leg_command_t command(const leg_fsm_t *fsm)
{
    leg_command_t cmd = {0, 0.0f};

    switch (fsm->state) {
    case LEG_STATE_SUPPORT:
        cmd.mode = LEG_CMD_HOLD_SUPPORT;
        break;
    case LEG_STATE_LIFT:
        cmd.mode = LEG_CMD_LIFT;
        if (fsm->config.lift_duration_s > 0.0f) {
            cmd.progress =
                fminf(1.0f, fsm->state_elapsed_s / fsm->config.lift_duration_s);
        } else {
            cmd.progress = 1.0f;
        }
        break;
    case LEG_STATE_SWING:
        cmd.mode = LEG_CMD_SWING;
        cmd.progress = fsm->swing_progress;
        break;
    case LEG_STATE_SEARCH:
        cmd.mode = LEG_CMD_SEARCH;
        cmd.progress = fsm->state_elapsed_s * fsm->config.search_speed;
        break;
    default:
        cmd.mode = LEG_CMD_SAFE_STOP;
        break;
    }
    return cmd;
}

leg_command_t leg_fsm_update(leg_fsm_t *fsm, bool pressed, bool contact,
                             float dt_s)
{
    float dt;

    if (!isfinite(dt_s) || dt_s < 0.0f) {
        record_invalid_input(fsm);
        dt = 0.0f;
    } else {
        dt = dt_s;
    }

    fsm->state_elapsed_s += dt;
    bool rising_edge = pressed && !fsm->previous_pressed;
    fsm->previous_pressed = pressed;

    if (fsm->state == LEG_STATE_SUPPORT && rising_edge) {
        transition(fsm, LEG_STATE_LIFT, "button press");
    } else if (fsm->state == LEG_STATE_LIFT) {
        if (timed_out(fsm)) {
            transition(fsm, LEG_STATE_FAULT, "state timeout");
        } else if (fsm->state_elapsed_s >= fsm->config.lift_duration_s) {
            transition(fsm,
                       pressed ? LEG_STATE_SWING : LEG_STATE_SEARCH,
                       "lift complete");
        }
    } else if (fsm->state == LEG_STATE_SWING) {
        if (!pressed) {
            transition(fsm, LEG_STATE_SEARCH, "button release");
        } else {
            fsm->swing_progress = fminf(fsm->config.max_swing,
                                        fsm->swing_progress +
                                            dt * fsm->config.swing_speed);
            if (timed_out(fsm)) {
                transition(fsm, LEG_STATE_FAULT, "state timeout");
            }
        }
    } else if (fsm->state == LEG_STATE_SEARCH) {
        if (contact) {
            transition(fsm, LEG_STATE_SUPPORT, "foot contact");
        } else if (timed_out(fsm)) {
            transition(fsm, LEG_STATE_FAULT, "state timeout");
        }
    }

    return command(fsm);
}

void leg_fsm_reset(leg_fsm_t *fsm)
{
    fsm->state = LEG_STATE_SUPPORT;
    fsm->state_elapsed_s = 0.0f;
    fsm->swing_progress = 0.0f;
    fsm->previous_pressed = false;
    /* ring and transition_total deliberately preserved */
}

uint32_t leg_fsm_transition_count(const leg_fsm_t *fsm)
{
    if (fsm->transition_total < LEG_FSM_TRANSITION_RING_SIZE) {
        return (uint32_t)fsm->transition_total;
    }
    return LEG_FSM_TRANSITION_RING_SIZE;
}

const leg_transition_t *leg_fsm_transition_get(const leg_fsm_t *fsm,
                                               uint32_t index)
{
    uint32_t count = leg_fsm_transition_count(fsm);
    if (index >= count) {
        return NULL;
    }
    uint64_t pos = (fsm->transition_total - count + index) %
                   LEG_FSM_TRANSITION_RING_SIZE;
    return &fsm->ring[pos];
}

const char *leg_state_name(leg_state_t s)
{
    switch (s) {
    case LEG_STATE_SUPPORT:
        return "SUPPORT";
    case LEG_STATE_LIFT:
        return "LIFT";
    case LEG_STATE_SWING:
        return "SWING";
    case LEG_STATE_SEARCH:
        return "SEARCH";
    default:
        return "FAULT";
    }
}

const char *leg_command_mode_name(leg_command_mode_t m)
{
    switch (m) {
    case LEG_CMD_HOLD_SUPPORT:
        return "hold_support";
    case LEG_CMD_LIFT:
        return "lift";
    case LEG_CMD_SWING:
        return "swing";
    case LEG_CMD_SEARCH:
        return "search";
    default:
        return "safe_stop";
    }
}
