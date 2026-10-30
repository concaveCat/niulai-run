/* Thin composition layer: mirrors firmware/control/controller.py
 * SingleLegController (FSM + three replaceable capability callbacks).
 * Pure C11; no ESP-IDF or FreeRTOS dependencies (purity grep enforced). */
#ifndef CONTROL_GLUE_H
#define CONTROL_GLUE_H

#include <stdbool.h>

#include "leg_fsm.h"

typedef bool (*leg_read_pressed_fn)(void *ctx);
typedef bool (*leg_read_contact_fn)(void *ctx);
typedef void (*leg_command_leg_fn)(const leg_command_t *cmd, void *ctx);

typedef struct {
    leg_fsm_t fsm;
    leg_read_pressed_fn read_pressed;
    leg_read_contact_fn read_contact;
    leg_command_leg_fn command_leg;
    void *ctx;
} leg_runner_t;

/* Initializes the embedded FSM from `config` and wires the callbacks.
 * Returns false on invalid config or NULL callbacks. */
bool leg_runner_init(leg_runner_t *runner, const leg_fsm_config_t *config,
                     leg_read_pressed_fn read_pressed,
                     leg_read_contact_fn read_contact,
                     leg_command_leg_fn command_leg, void *ctx);

/* One step: read pressed/contact through the callbacks, advance the FSM one
 * tick, hand the resulting intent to command_leg. */
void leg_runner_tick(leg_runner_t *runner, float dt_s);

#endif /* CONTROL_GLUE_H */
