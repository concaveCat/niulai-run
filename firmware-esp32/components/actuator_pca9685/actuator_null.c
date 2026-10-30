/* Null actuator: takes no physical action. Motion intents are logged by the
 * control task through uart_log.h (FSM_CMD, the single owner of log-line
 * formats), so this adapter deliberately stays silent. It is the default
 * adapter in every gating stage until the runtime gate opens. */
#include "leg_actuator.h"

static bool null_init(void)
{
    return true;
}

static bool null_command_frame(const qw_motion_frame_t *frame)
{
    (void)frame;
    return true;
}

static bool null_force_safe_stop(void)
{
    return true;
}

static bool null_set_oe(bool level)
{
    (void)level;
    return true;
}

const leg_actuator_t qw_actuator_null = {
    null_init,
    null_command_frame,
    null_force_safe_stop,
    null_set_oe,
};
