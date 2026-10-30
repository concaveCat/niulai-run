/* Three-stage safety gating for actuation (the core safety design).
 *
 * Stages (Kconfig NIULAI_MODE; sdkconfig.defaults always ships BLE_ONLY):
 *   BLE_ONLY            - /OE always high, PCA9685 never initialized, null
 *   BLE_VERIFIED        - PCA9685 init -> safe values -> SLEEP, never armed
 *   ACTUATION_ENABLED   - actuation possible only while the runtime gate is
 *                         open (see gate condition below)
 *
 * Runtime gate opens only when ALL hold:
 *   1. stage == ACTUATION_ENABLED
 *   2. acceptance window (default 1 s) of consecutive watchdog ticks with
 *      fresh BLE packets (inter-arrival within the watchdog timeout)
 *   3. no failsafe active
 *   4. FSM state != FAULT
 *   5. actuator initialization and control task initialization succeeded
 * Gate close (failsafe trip / safe_stop command / FAULT / acceptance break)
 * = force_safe_stop(): /OE high + PCA9685 SLEEP + switch to null adapter.
 * /OE is a motion disable, not a power disconnect - the harness fuse is the
 * hardware precondition for any ACTUATION_ENABLED work. Revocation is
 * serialized with actuator I/O; an in-flight command is rejected after an
 * authorization-epoch change and a failed safe-stop remains pending.
 */
#ifndef SAFETY_GATE_H
#define SAFETY_GATE_H

#include <stdbool.h>

#include "motion_strategy.h"

typedef enum {
    QW_STAGE_BLE_ONLY = 0,
    QW_STAGE_BLE_VERIFIED,
    QW_STAGE_ACTUATION_ENABLED,
} qw_stage_t;

void safety_gate_init(void);
qw_stage_t safety_gate_stage(void);
bool safety_gate_armed(void);

/* Control tick: notify the FSM state (FAULT closes the gate), then dispatch
 * the command to the current actuator (null while the gate is closed). */
void safety_gate_notify_motion_state(qw_motion_state_code_t state);
void safety_gate_notify_control_ready(void);
void safety_gate_notify_contact_ready(bool ready);
/* Returns true only when the currently armed actuator accepted the frame. */
bool safety_gate_apply(const qw_motion_frame_t *frame);

/* Watchdog tick: acceptance feed + failsafe mirror + gate re-evaluation. */
void safety_gate_acceptance_feed(bool fresh_packet, bool failsafe_active);

/* Immediate close on a watchdog failsafe trip (does not wait for a feed). */
void safety_gate_on_failsafe(void);

/* Closes actuation while the MPU6500 reports a fall posture. This is a
 * recoverable safety condition: after the posture clears, a fresh acceptance
 * window is required before /OE can be lowered again. */
void safety_gate_notify_fall(bool detected);

/* Disconnect clears the acceptance window and closes an armed gate. */
void safety_gate_on_input_disconnect(void);

/* Explicit close (reason logged); no-op when already closed. */
void safety_gate_force_safe_stop(const char *reason);

/* Clear an explicit stop latch only after a manual reset, encrypted bonded link,
 * neutral input and a new acceptance window. */
void safety_gate_clear_stop_latch(void);

/* UART status introspection. */
bool safety_gate_acceptance_ok(void);
int safety_gate_acceptance_ticks(void);
bool safety_gate_failsafe(void);

#endif /* SAFETY_GATE_H */
