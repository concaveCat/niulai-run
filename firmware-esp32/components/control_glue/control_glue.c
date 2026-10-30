#include "control_glue.h"

#include <stddef.h>


bool leg_runner_init(leg_runner_t *runner, const leg_fsm_config_t *config,
                     leg_read_pressed_fn read_pressed,
                     leg_read_contact_fn read_contact,
                     leg_command_leg_fn command_leg, void *ctx)
{
    if (runner == NULL || read_pressed == NULL || read_contact == NULL ||
        command_leg == NULL) {
        return false;
    }
    if (!leg_fsm_init(&runner->fsm, config)) {
        return false;
    }
    runner->read_pressed = read_pressed;
    runner->read_contact = read_contact;
    runner->command_leg = command_leg;
    runner->ctx = ctx;
    return true;
}

void leg_runner_tick(leg_runner_t *runner, float dt_s)
{
    /* Mirrors SingleLegController.step in firmware/control/controller.py:
     * one FSM update fed by the two read callbacks, then hand the command
     * to the actuator callback. */
    leg_command_t cmd =
        leg_fsm_update(&runner->fsm, runner->read_pressed(runner->ctx),
                       runner->read_contact(runner->ctx), dt_s);
    runner->command_leg(&cmd, runner->ctx);
}
