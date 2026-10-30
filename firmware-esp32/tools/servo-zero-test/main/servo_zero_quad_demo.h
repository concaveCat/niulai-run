#ifndef NIULAI_SERVO_ZERO_QUAD_DEMO_H
#define NIULAI_SERVO_ZERO_QUAD_DEMO_H

#include <stdbool.h>
#include <stdint.h>

#include "servo_zero_leg_math.h"

/*
 * This is an isolated, planar four-leg demo.  It deliberately keeps the
 * logical order used by the servo-zero bench records instead of reusing the
 * FL/FR/RL/RR bit order used by the BLE/contact protocol.
 */
#define SERVO_ZERO_QUAD_DEMO_LEG_COUNT 4u
#define SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT 8u
#define SERVO_ZERO_QUAD_DEMO_UPDATE_MS 20u
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM 30.0f
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_STANCE_STEP_LENGTH_MM 9.0f
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_BACK_MM 6.0f
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MM 40.0f
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_SETTLE_MS 180u
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MS 300u
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_SWING_MS 360u
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_SEARCH_MS 120u
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_BODY_DRIVE_PERCENT 100u
#define SERVO_ZERO_QUAD_DEMO_TOUCHDOWN_SWING_RETENTION_PERCENT 25u
#define SERVO_ZERO_QUAD_DEMO_SUPPORT_RESERVE_MARGIN_PERCENT 85u
#define SERVO_ZERO_QUAD_DEMO_FULL_HIP_SCALE_PERCENT 100u
#define SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT 50u
#define SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM (-50.0f)
#define SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM 50.0f
#define SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM (-105.0f)
#define SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM (-50.0f)
#define SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM 40.0f
#define SERVO_ZERO_QUAD_DEMO_MAX_LIFT_MM 50.0f
#define SERVO_ZERO_QUAD_DEMO_MAX_PULSE_STEP_US 250u
#define SERVO_ZERO_QUAD_DEMO_TRANSPORT_MIN_US 300u
#define SERVO_ZERO_QUAD_DEMO_TRANSPORT_MAX_US 3000u

typedef enum {
    SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP = 0,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_KNEE,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_KNEE,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_HIP,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_KNEE,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_HIP,
    SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_KNEE,
} servo_zero_quad_demo_channel_t;

typedef enum {
    /* Keyboard 1. */
    SERVO_ZERO_QUAD_DEMO_LEG_RF = 0,
    /* Keyboard 2. */
    SERVO_ZERO_QUAD_DEMO_LEG_LF,
    /* Keyboard 3. */
    SERVO_ZERO_QUAD_DEMO_LEG_RR,
    /* Keyboard 4. */
    SERVO_ZERO_QUAD_DEMO_LEG_LR,
} servo_zero_quad_demo_leg_t;

typedef enum {
    SERVO_ZERO_QUAD_DEMO_PHASE_SETTLE = 0,
    SERVO_ZERO_QUAD_DEMO_PHASE_SWING_LIFT,
    SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD,
    SERVO_ZERO_QUAD_DEMO_PHASE_SEARCH,
    SERVO_ZERO_QUAD_DEMO_PHASE_COMPLETE,
} servo_zero_quad_demo_phase_t;

typedef struct {
    uint8_t hip_channel;
    uint8_t knee_channel;
    int8_t knee_branch;
    /* Candidate local affine q->us map.  It is not a mechanical calibration. */
    servo_zero_leg_pulse_map_t pulse_map;
} servo_zero_quad_demo_leg_config_t;

/*
 * Per-channel actuator safety envelope for this demo. These values must be
 * measured/approved mechanical-safe pulse endpoints; the generic
 * 300..3000 us transport range is deliberately not a substitute.
 */
typedef struct {
    bool has_min;
    uint16_t min_us;
    bool has_max;
    uint16_t max_us;
} servo_zero_quad_demo_channel_limit_t;

typedef struct {
    /* Must be true only after the complete table has been reviewed. */
    bool configured;
    servo_zero_quad_demo_channel_limit_t
        channel[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT];
} servo_zero_quad_demo_pulse_limits_t;

typedef struct {
    uint8_t active_leg;
    /* Optional simultaneous-leg mask; zero preserves single-leg behavior. */
    uint8_t active_leg_mask;
    servo_zero_leg_point_t start_foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_quad_demo_leg_config_t leg[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_quad_demo_pulse_limits_t pulse_limits;
    float step_length_mm;
    float stance_step_length_mm;
    float lift_back_mm;
    float lift_mm;
    /* Legacy UART field name. Fixed diagnostics scale the active hip angle;
     * the live keyboard path scales horizontal foot-space displacement. */
    uint8_t active_hip_scale_percent;
    uint32_t settle_ms;
    uint32_t lift_ms;
    uint32_t swing_ms;
    uint32_t search_ms;
} servo_zero_quad_demo_params_t;

typedef struct {
    uint8_t active_leg;
    uint8_t active_leg_mask;
    servo_zero_leg_point_t start_foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_quad_demo_leg_config_t leg[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_quad_demo_pulse_limits_t pulse_limits;
    float step_length_mm;
    /* Each selected leg may stop growing at a different key-up time. */
    float leg_step_length_mm[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    float stance_step_length_mm;
    float lift_back_mm;
    float lift_mm;
    uint8_t active_hip_scale_percent;
    uint32_t settle_ms;
    uint32_t lift_ms;
    uint32_t swing_ms;
    uint32_t search_ms;
    uint32_t total_ms;
    /* Live mode caches the fixed knee-lift endpoint and the first safe
     * horizontal offset.  Runtime ticks then evaluate only the requested
     * point instead of repeating a full boundary search. */
    servo_zero_leg_point_t live_lifted_foot[
        SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_leg_angles_t live_lifted_angles[
        SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    float live_swing_limit_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint8_t live_cache_valid_mask;
    bool valid;
} servo_zero_quad_demo_plan_t;

typedef struct {
    servo_zero_quad_demo_phase_t phase;
    servo_zero_leg_point_t foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_leg_angles_t angles[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint16_t pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT];
    bool pulse_limit_violation;
    uint8_t limit_channel;
    uint16_t limit_pulse_us;
    uint16_t limit_min_us;
    uint16_t limit_max_us;
    bool soft_limit_clamped;
    uint8_t soft_limit_leg_mask;
    /* Reaching the player-selected bounded step is normal completion, not a
     * geometry or pulse safety event. */
    uint8_t swing_target_reached_mask;
    float accepted_swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    /* Largest accepted horizontal foot-space swing among currently held
     * legs, after the configured live speed scale.  This is intentionally
     * separate from body_drive_mm. */
    float effective_swing_drive_mm;
    /* ARD-1 common body-frame request.  Without body/foot feedback this must
     * not be interpreted as measured body translation. */
    float body_drive_mm;
    /* Nominal per-support retreat request before each support leg applies
     * its own workspace/pulse-limit capacity. */
    float support_retreat_mm;
    /* Per-leg command actually applied after independent reserve clipping. */
    float support_applied_mm[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint8_t support_leg_mask;
    bool valid;
    bool complete;
} servo_zero_quad_demo_pose_t;

/* Runtime state for the keyboard-controlled, open-ended hold path.  The
 * plan contains geometry and limits; this structure contains only transient
 * per-key state so a held key cannot be confused with a timed one-shot. */
typedef struct {
    uint8_t moving_leg_mask;
    uint8_t held_leg_mask;
    uint8_t search_leg_mask;
    uint32_t lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    /* Commanded swing time, advanced one control frame at a time after the
     * previous output frame has been accepted.  It must not be derived from
     * wall-clock catch-up after an I2C retry or task delay. */
    uint32_t swing_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint32_t search_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    float swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_leg_point_t support_base_foot[
        SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    /* Body-drive level already represented by support_base_foot.  The
     * reference and bases form one coordinate frame; they are rebased only
     * after a pose has been written. */
    float support_drive_reference_mm;
    /* Holds the last body-drive target while a released leg is in SEARCH.
     * Without this latch, removing a leg from held_leg_mask would make D
     * fall to zero and move every support foot forward on the next frame. */
    float body_drive_floor_mm;
    /* Fixed safe rear endpoint for the current landed support cycle.  Rebase
     * moves the origin but never moves this boundary rearward.  The main
     * runtime carries these fields across separate press/release actions. */
    float support_safe_rear_x_mm[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint8_t support_safe_rear_valid_mask;
    servo_zero_leg_point_t release_foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    servo_zero_leg_angles_t release_angles[
        SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
} servo_zero_quad_demo_live_state_t;

/* Build and preflight the complete four-leg path before outputs are enabled. */
bool servo_zero_quad_demo_make_plan(
    const servo_zero_quad_demo_params_t *params,
    servo_zero_quad_demo_plan_t *plan);

/* Build the shared geometry/limit plan for live press/release mode.  Live
 * mode preflights its current snapshot and knee-lift endpoint at startup;
 * it must not be rejected by the separate fixed-duration diagnostic path. */
bool servo_zero_quad_demo_make_live_plan(
    const servo_zero_quad_demo_params_t *params,
    servo_zero_quad_demo_plan_t *plan);

/* Check a complete output frame against the plan's per-channel safety window. */
bool servo_zero_quad_demo_pulses_within_limits(
    const servo_zero_quad_demo_plan_t *plan,
    const uint16_t pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT],
    uint8_t *bad_channel, uint16_t *bad_pulse_us, uint16_t *bad_min_us,
    uint16_t *bad_max_us);

/* Evaluate one atomic four-leg snapshot at elapsed_ms. */
servo_zero_quad_demo_pose_t servo_zero_quad_demo_pose_at(
    uint32_t elapsed_ms, const servo_zero_quad_demo_plan_t *plan);

/* Evaluate one hold-to-swing snapshot. Held legs stay in LIFT until their
 * individual lift timer completes, then advance toward a bounded
 * step_length_mm target at the plan's nominal rate. Released legs use their
 * latched hip angle while the knee performs the fixed SEARCH drop. The
 * returned pose exposes visible swing, body-drive and common support retreat
 * for that snapshot. */
servo_zero_quad_demo_pose_t servo_zero_quad_demo_live_pose(
    const servo_zero_quad_demo_plan_t *plan,
    const servo_zero_quad_demo_live_state_t *state);

/* Return the largest prefix-safe rearward retreat from one support origin.
 * The live allocator applies the ARD-1 reserve margin independently per leg. */
float servo_zero_quad_demo_support_capacity_mm(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot);

/* Freeze the rear safety boundary for a newly established support cycle. */
float servo_zero_quad_demo_support_safe_rear_x_mm(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot);

/* Atomically rebase runtime support origins from the last successfully
 * written pose. Only newly landed legs receive a fresh safety boundary. */
bool servo_zero_quad_demo_rebase_written_pose(
    const servo_zero_quad_demo_plan_t *plan,
    servo_zero_quad_demo_live_state_t *state,
    const servo_zero_quad_demo_pose_t *written_pose, uint8_t landed_mask);

/* Keep a command-estimated starting foot inside the demo workspace. */
servo_zero_leg_point_t servo_zero_quad_demo_limit_start_foot(
    servo_zero_leg_point_t foot);

#endif /* NIULAI_SERVO_ZERO_QUAD_DEMO_H */
