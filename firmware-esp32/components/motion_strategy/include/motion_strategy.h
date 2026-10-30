#ifndef QW_MOTION_STRATEGY_H
#define QW_MOTION_STRATEGY_H

/* Hardware-independent motion boundary.  The application sees command
 * frames and diagnostic snapshots only; a strategy's private state and
 * strategy-specific configuration remain behind this interface. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "leg_fsm.h"

#define QW_MOTION_MAX_LEGS 4U
#define QW_MOTION_TRANSITION_RING_SIZE 64U

/* Public semantic leg order. Device-specific channel order stays inside the
 * actuator adapter. */
typedef struct {
    float hip_rad;
    float knee_rad;
    bool valid;
} qw_motion_joint_target_t;

typedef struct {
    uint8_t command_mask;
    leg_command_t legs[QW_MOTION_MAX_LEGS];
    /* A non-zero mask makes the corresponding joint targets authoritative.
     * Zero preserves the legacy leg-command adapter seam. */
    uint8_t joint_target_mask;
    qw_motion_joint_target_t joints[QW_MOTION_MAX_LEGS];
} qw_motion_frame_t;

typedef bool (*qw_motion_read_fn)(void *ctx, uint8_t leg_index);
typedef void (*qw_motion_emit_fn)(const qw_motion_frame_t *frame, void *ctx);

typedef struct {
    const void *strategy_config;
    uint8_t leg_mask;
    qw_motion_read_fn read_pressed;
    qw_motion_read_fn read_contact;
    qw_motion_emit_fn emit;
    void *capability_ctx;
} qw_motion_init_args_t;

typedef enum {
    QW_MOTION_STATE_UNKNOWN = 0,
    QW_MOTION_STATE_SUPPORT,
    QW_MOTION_STATE_LIFT,
    QW_MOTION_STATE_SWING,
    QW_MOTION_STATE_SEARCH,
    QW_MOTION_STATE_FAULT,
} qw_motion_state_code_t;

typedef struct {
    const char *from_state;
    const char *to_state;
    const char *reason;
    float elapsed_s;
} qw_motion_transition_t;

typedef struct {
    const char *strategy_name;
    const char *state_name;
    qw_motion_state_code_t state;
    bool fault;
    /* Public semantic leg mask (FL/FR/RL/RR) currently executing a step.
     * This is a read-only display/diagnostic snapshot; it does not authorize
     * motion and is not part of the BLE control protocol. */
    uint8_t stepping_leg_mask;
    uint64_t transition_total;
    uint32_t transition_count;
    qw_motion_transition_t transitions[QW_MOTION_TRANSITION_RING_SIZE];
} qw_motion_snapshot_t;

typedef struct {
    void *ctx;
    bool (*init)(void *ctx, const qw_motion_init_args_t *args);
    void (*tick)(void *ctx, float dt_s);
    /* Called only after the safety gate and actuator accept the frame. */
    void (*commit)(void *ctx, bool applied);
    bool (*snapshot)(void *ctx, qw_motion_snapshot_t *out);
    void (*reset)(void *ctx);
} qw_motion_strategy_t;

bool qw_motion_strategy_valid(const qw_motion_strategy_t *strategy);
bool qw_motion_strategy_init(qw_motion_strategy_t *strategy,
                             const qw_motion_init_args_t *args);
void qw_motion_strategy_tick(qw_motion_strategy_t *strategy, float dt_s);
void qw_motion_strategy_commit(qw_motion_strategy_t *strategy, bool applied);
bool qw_motion_strategy_snapshot(const qw_motion_strategy_t *strategy,
                                 qw_motion_snapshot_t *out);
void qw_motion_strategy_reset(qw_motion_strategy_t *strategy);

/* Legacy compatibility strategy: one statically configured leg,
 * Hold-to-Swing FSM. Its runner storage is private to the adapter and is not
 * part of the application contract. */
bool qw_motion_leg_fsm_create(qw_motion_strategy_t *strategy);

/* Formal software-only ARD-1 PastureStep-Reserve strategy. */
typedef struct {
    uint32_t lift_duration_ms;
    uint32_t swing_duration_ms;
    uint32_t search_duration_ms;
    float step_length_mm;
    float lift_z_mm;
    float touchdown_z_mm;
} qw_ard1_config_t;

bool qw_motion_ard1_create(qw_motion_strategy_t *strategy);

#endif /* QW_MOTION_STRATEGY_H */
