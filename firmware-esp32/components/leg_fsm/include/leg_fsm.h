/* Pure, hardware-independent Hold-to-Swing leg state machine.
 *
 * C11 port of firmware/control/leg_fsm.py, tick-for-tick identical in
 * observable behavior (transition ordering within one tick is pinned by the
 * host tests in tests/host/).
 *
 * Deviation note: the Python update() sanitizes non-bool `pressed`/`contact`
 * values ("invalid input ignored" self-transition, contact forced false,
 * pressed held at previous value). C `bool` parameters make that branch
 * unrepresentable; the adapter layer (control_glue callbacks) is responsible
 * for supplying real booleans. The dt_s sanitize branch IS represented here:
 * non-finite or negative dt records the same self-transition and is treated
 * as 0.0.
 *
 * This header and leg_fsm.c are pure C11: no ESP-IDF or FreeRTOS includes,
 * enforced by the component CMakeLists (no REQUIRES) and by a purity grep in
 * tests/host/run_host_tests.sh.
 */
#ifndef LEG_FSM_H
#define LEG_FSM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of transitions retained in the ring (v0 telemetry). */
#define LEG_FSM_TRANSITION_RING_SIZE 64

typedef enum {
    LEG_STATE_SUPPORT = 0,
    LEG_STATE_LIFT,
    LEG_STATE_SWING,
    LEG_STATE_SEARCH,
    LEG_STATE_FAULT, /* terminal: only an explicit reset() leaves it */
} leg_state_t;

typedef enum {
    LEG_CMD_HOLD_SUPPORT = 0,
    LEG_CMD_LIFT,
    LEG_CMD_SWING,
    LEG_CMD_SEARCH,
    LEG_CMD_SAFE_STOP,
} leg_command_mode_t;

/** Tunable experiment parameters, deliberately not final hardware specs. */
typedef struct {
    float lift_duration_s;
    float swing_speed;
    float search_speed;
    float max_swing;
    /** State timeout in seconds; <= 0 disables it (C analogue of Python's
     *  None sentinel). */
    float state_timeout_s;
} leg_fsm_config_t;

/** Hardware-independent motion intent. */
typedef struct {
    leg_command_mode_t mode;
    float progress;
} leg_command_t;

typedef struct {
    leg_state_t from_state;
    leg_state_t to_state;
    const char *reason; /* static string literal, never freed */
    float elapsed_s;
} leg_transition_t;

typedef struct {
    leg_fsm_config_t config;
    leg_state_t state;
    float state_elapsed_s;
    float swing_progress;
    bool previous_pressed;
    leg_transition_t ring[LEG_FSM_TRANSITION_RING_SIZE];
    uint64_t transition_total; /* monotonic; reset() preserves it */
} leg_fsm_t;

/* Validate `config` and reset the FSM to SUPPORT. Returns false (fsm
 * untouched) when the config is invalid: the first four values must be
 * finite and >= 0; state_timeout_s must be finite and either disabled
 * (<= 0) or strictly positive. */
bool leg_fsm_init(leg_fsm_t *fsm, const leg_fsm_config_t *config);

/* Advance one tick and return the motion intent for it. */
leg_command_t leg_fsm_update(leg_fsm_t *fsm, bool pressed, bool contact,
                             float dt_s);

/* Explicitly leave FAULT for a new experiment run. Keeps the transition
 * ring and transition_total (history is telemetry, not experiment state). */
void leg_fsm_reset(leg_fsm_t *fsm);

/* Number of transitions currently stored (0 .. LEG_FSM_TRANSITION_RING_SIZE). */
uint32_t leg_fsm_transition_count(const leg_fsm_t *fsm);

/* Ring accessor; index 0 is the oldest stored transition. Returns NULL when
 * index >= count. */
const leg_transition_t *leg_fsm_transition_get(const leg_fsm_t *fsm,
                                               uint32_t index);

const char *leg_state_name(leg_state_t s);
const char *leg_command_mode_name(leg_command_mode_t m);

#ifdef __cplusplus
}
#endif

#endif /* LEG_FSM_H */
