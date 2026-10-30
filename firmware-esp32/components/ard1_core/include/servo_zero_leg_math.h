#ifndef NIULAI_SERVO_ZERO_LEG_MATH_H
#define NIULAI_SERVO_ZERO_LEG_MATH_H

#include <stdbool.h>
#include <stdint.h>

/* Isolated planar model only; these are not yet the robot's calibrated angles. */
#define SERVO_ZERO_LEG_THIGH_MM 45.0f
#define SERVO_ZERO_LEG_SHANK_MM 70.0f
#define SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD 1
#define SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD -1

typedef struct {
    float x_mm;
    float z_mm;
} servo_zero_leg_point_t;

/* hip_rad is measured from +x; knee_rad is relative to the thigh. */
typedef struct {
    float hip_rad;
    float knee_rad;
} servo_zero_leg_angles_t;

typedef struct {
    servo_zero_leg_point_t start;
    float lift_mm;
    float forward_mm;
    uint32_t lift_ms;
    uint32_t swing_ms;
    uint32_t lower_ms;
    int8_t knee_branch;
} servo_zero_leg_lift_params_t;

typedef struct {
    servo_zero_leg_point_t foot;
    servo_zero_leg_angles_t angles;
    bool valid;
    bool complete;
} servo_zero_leg_pose_t;

/*
 * Joint-ordered single-leg experiment:
 *   rest -> knee lift (CH1 only) -> hip forward swing (CH0 only)
 *   -> vertical touchdown (CH0 + CH1 through IK).
 *
 * hip_swing_rad is signed in the model frame: positive moves the foot toward
 * +x.  knee_branch also selects the direction in which the relative knee
 * angle is increased/decreased during the CH1-only lift and the IK branch
 * used for touchdown.  Neither field is a servo-pulse direction.
 */
typedef struct {
    servo_zero_leg_angles_t rest_angles;
    float lift_clearance_mm;
    float hip_swing_rad;
    uint32_t rest_ms;
    uint32_t knee_lift_ms;
    uint32_t hip_swing_ms;
    uint32_t touchdown_ms;
    int8_t knee_branch;
} servo_zero_leg_single_params_t;

typedef struct {
    servo_zero_leg_angles_t rest_angles;
    servo_zero_leg_angles_t knee_lift_angles;
    servo_zero_leg_angles_t swing_angles;
    servo_zero_leg_angles_t touchdown_angles;
    servo_zero_leg_point_t rest_foot;
    servo_zero_leg_point_t knee_lift_foot;
    servo_zero_leg_point_t swing_foot;
    servo_zero_leg_point_t touchdown_foot;
    uint32_t rest_ms;
    uint32_t knee_lift_ms;
    uint32_t hip_swing_ms;
    uint32_t touchdown_ms;
    uint32_t total_ms;
    int8_t knee_branch;
    bool valid;
} servo_zero_leg_single_plan_t;

typedef enum {
    SERVO_ZERO_LEG_SINGLE_PHASE_REST = 0,
    SERVO_ZERO_LEG_SINGLE_PHASE_KNEE_LIFT,
    SERVO_ZERO_LEG_SINGLE_PHASE_HIP_SWING,
    SERVO_ZERO_LEG_SINGLE_PHASE_TOUCHDOWN,
    SERVO_ZERO_LEG_SINGLE_PHASE_COMPLETE,
} servo_zero_leg_single_phase_t;

typedef struct {
    servo_zero_leg_point_t foot;
    servo_zero_leg_angles_t angles;
    servo_zero_leg_single_phase_t phase;
    bool valid;
    bool complete;
} servo_zero_leg_single_pose_t;

/* Continuous foot-space step. The cubic Bezier has vertical tangents at
 * liftoff/touchdown and no artificial stop at an internal phase boundary. */
typedef struct {
    servo_zero_leg_angles_t rest_angles;
    float lift_clearance_mm;
    float hip_swing_rad;
    uint32_t rest_ms;
    uint32_t move_ms;
    int8_t knee_branch;
} servo_zero_leg_flow_params_t;

typedef struct {
    servo_zero_leg_angles_t rest_angles;
    servo_zero_leg_angles_t touchdown_angles;
    servo_zero_leg_point_t rest_foot;
    servo_zero_leg_point_t touchdown_foot;
    servo_zero_leg_point_t control_liftoff;
    servo_zero_leg_point_t control_touchdown;
    uint32_t rest_ms;
    uint32_t move_ms;
    uint32_t total_ms;
    int8_t knee_branch;
    bool valid;
} servo_zero_leg_flow_plan_t;

typedef struct {
    servo_zero_leg_point_t foot;
    servo_zero_leg_angles_t angles;
    bool valid;
    bool complete;
} servo_zero_leg_flow_pose_t;

/* One-way hip sweep with a fixed absolute shank direction.  The caller
 * supplies signed model-frame hip endpoints; the servo-pulse direction is a
 * separate adapter concern. */
typedef struct {
    float start_hip_rad;
    float end_hip_rad;
    float shank_rad;
    uint32_t move_ms;
} servo_zero_leg_vertical_sweep_params_t;

typedef struct {
    servo_zero_leg_angles_t start_angles;
    servo_zero_leg_angles_t end_angles;
    servo_zero_leg_point_t start_foot;
    servo_zero_leg_point_t end_foot;
    float shank_rad;
    uint32_t move_ms;
    bool valid;
} servo_zero_leg_vertical_sweep_plan_t;

typedef struct {
    servo_zero_leg_angles_t angles;
    servo_zero_leg_point_t foot;
    float shank_rad;
    bool valid;
    bool complete;
} servo_zero_leg_vertical_sweep_pose_t;

/* Experimental local linear map for the currently observed RF channels.
 * The slopes are supplied by the caller and are not a mechanical calibration. */
typedef struct {
    uint16_t hip_rest_us;
    uint16_t knee_rest_us;
    float hip_us_per_rad;
    float knee_us_per_rad;
    uint16_t min_us;
    uint16_t max_us;
} servo_zero_leg_pulse_map_t;

typedef struct {
    uint16_t hip_us;
    uint16_t knee_us;
    bool valid;
} servo_zero_leg_pulse_pose_t;

/* Experimental press/hold/release motion-leg plan. The hip swing uses a
 * cosine profile and is frozen at the release sample; touchdown returns the
 * knee by the accumulated lift plus hip-swing rotation. */
typedef struct {
    servo_zero_leg_angles_t rest_angles;
    servo_zero_leg_angles_t lifted_angles;
    servo_zero_leg_angles_t swing_angles;
    servo_zero_leg_angles_t touchdown_angles;
    servo_zero_leg_point_t rest_foot;
    servo_zero_leg_point_t lifted_foot;
    servo_zero_leg_point_t swing_foot;
    servo_zero_leg_point_t touchdown_foot;
    float hip_swing_rad;
    int8_t knee_branch;
    bool valid;
} servo_zero_motionleg_plan_t;

typedef enum {
    SERVO_ZERO_MOTIONLEG_LIFT = 0,
    SERVO_ZERO_MOTIONLEG_WAIT,
    SERVO_ZERO_MOTIONLEG_SWING,
    SERVO_ZERO_MOTIONLEG_TOUCHDOWN,
    SERVO_ZERO_MOTIONLEG_COMPLETE,
} servo_zero_motionleg_phase_t;

typedef struct {
    servo_zero_leg_angles_t angles;
    servo_zero_leg_point_t foot;
    servo_zero_motionleg_phase_t phase;
    bool valid;
    bool complete;
} servo_zero_motionleg_pose_t;

bool servo_zero_leg_fk(servo_zero_leg_angles_t angles,
                       servo_zero_leg_point_t *foot);
bool servo_zero_leg_ik(servo_zero_leg_point_t foot, int8_t knee_branch,
                       servo_zero_leg_angles_t *angles);
/* Solve q_k for a fixed q_h and target z, selecting the requested positive
 * (forward) or negative (backward) relative-knee branch. */
bool servo_zero_leg_knee_at_height(
    float hip_rad, float target_z_mm, float reference_knee_rad,
    int8_t knee_branch, float *knee_rad);
/* Solve a knee-only lift from rest, selecting the root that moves in the
 * branch's signed direction instead of the potentially wrong nearest root. */
bool servo_zero_leg_knee_lift_at_height(
    servo_zero_leg_angles_t rest_angles, float target_z_mm,
    int8_t knee_branch, float *knee_rad);
servo_zero_leg_pose_t servo_zero_leg_lift_pose(
    uint32_t elapsed_ms, uint8_t amplitude_percent,
    const servo_zero_leg_lift_params_t *params);
bool servo_zero_leg_make_single_plan(
    const servo_zero_leg_single_params_t *params,
    servo_zero_leg_single_plan_t *plan);
servo_zero_leg_single_pose_t servo_zero_leg_single_pose(
    uint32_t elapsed_ms, const servo_zero_leg_single_plan_t *plan);
bool servo_zero_leg_make_flow_plan(
    const servo_zero_leg_flow_params_t *params,
servo_zero_leg_flow_plan_t *plan);
servo_zero_leg_flow_pose_t servo_zero_leg_flow_pose(
    uint32_t elapsed_ms, const servo_zero_leg_flow_plan_t *plan);
bool servo_zero_leg_make_vertical_sweep_plan(
    const servo_zero_leg_vertical_sweep_params_t *params,
    servo_zero_leg_vertical_sweep_plan_t *plan);
servo_zero_leg_vertical_sweep_pose_t servo_zero_leg_vertical_sweep_pose(
    uint32_t elapsed_ms,
    const servo_zero_leg_vertical_sweep_plan_t *plan);
servo_zero_leg_pulse_pose_t servo_zero_leg_map_angles_to_pulses(
    servo_zero_leg_angles_t rest_angles,
    servo_zero_leg_angles_t angles,
    const servo_zero_leg_pulse_map_t *map);
bool servo_zero_motionleg_make_plan(
    servo_zero_leg_angles_t rest_angles, float knee_lift_rad,
    float hip_swing_rad, int8_t knee_branch,
    servo_zero_motionleg_plan_t *plan);
servo_zero_motionleg_pose_t servo_zero_motionleg_pose(
    uint32_t elapsed_ms, uint32_t release_ms,
    uint32_t lift_ms, uint32_t wait_ms, uint32_t swing_ms,
    uint32_t touchdown_ms,
    const servo_zero_motionleg_plan_t *plan);

#endif
