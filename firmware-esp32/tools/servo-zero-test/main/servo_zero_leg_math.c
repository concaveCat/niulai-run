#include "servo_zero_leg_math.h"

#include <math.h>
#include <stddef.h>

#define SERVO_ZERO_LEG_EPSILON 0.0001f

static float clamp_unit(float value)
{
    return value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
}

static float smoothstep5(float t)
{
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

bool servo_zero_leg_fk(servo_zero_leg_angles_t angles,
                       servo_zero_leg_point_t *foot)
{
    if (foot == NULL || !isfinite(angles.hip_rad) ||
        !isfinite(angles.knee_rad)) {
        return false;
    }
    const float shank_rad = angles.hip_rad + angles.knee_rad;
    foot->x_mm = SERVO_ZERO_LEG_THIGH_MM * cosf(angles.hip_rad) +
                 SERVO_ZERO_LEG_SHANK_MM * cosf(shank_rad);
    foot->z_mm = SERVO_ZERO_LEG_THIGH_MM * sinf(angles.hip_rad) +
                 SERVO_ZERO_LEG_SHANK_MM * sinf(shank_rad);
    return isfinite(foot->x_mm) && isfinite(foot->z_mm);
}

bool servo_zero_leg_ik(servo_zero_leg_point_t foot, int8_t knee_branch,
                       servo_zero_leg_angles_t *angles)
{
    if (angles == NULL || !isfinite(foot.x_mm) || !isfinite(foot.z_mm) ||
        (knee_branch != SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD &&
         knee_branch != SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD)) {
        return false;
    }

    const float thigh_sq = SERVO_ZERO_LEG_THIGH_MM *
                           SERVO_ZERO_LEG_THIGH_MM;
    const float shank_sq = SERVO_ZERO_LEG_SHANK_MM *
                           SERVO_ZERO_LEG_SHANK_MM;
    const float distance_sq = foot.x_mm * foot.x_mm + foot.z_mm * foot.z_mm;
    const float denominator = 2.0f * SERVO_ZERO_LEG_THIGH_MM *
                             SERVO_ZERO_LEG_SHANK_MM;
    float knee_cos = (distance_sq - thigh_sq - shank_sq) / denominator;
    if (!isfinite(knee_cos) || knee_cos < -1.0f - SERVO_ZERO_LEG_EPSILON ||
        knee_cos > 1.0f + SERVO_ZERO_LEG_EPSILON) {
        return false;
    }
    knee_cos = clamp_unit(knee_cos);

    const float knee_magnitude = acosf(knee_cos);
    angles->knee_rad = knee_branch > 0 ? knee_magnitude : -knee_magnitude;
    const float k1 = SERVO_ZERO_LEG_THIGH_MM +
                     SERVO_ZERO_LEG_SHANK_MM * cosf(angles->knee_rad);
    const float k2 = SERVO_ZERO_LEG_SHANK_MM * sinf(angles->knee_rad);
    angles->hip_rad = atan2f(foot.z_mm, foot.x_mm) - atan2f(k2, k1);
    return isfinite(angles->hip_rad) && isfinite(angles->knee_rad);
}

servo_zero_leg_pose_t servo_zero_leg_lift_pose(
    uint32_t elapsed_ms, uint8_t amplitude_percent,
    const servo_zero_leg_lift_params_t *params)
{
    servo_zero_leg_pose_t pose = {
        .foot = {0.0f, 0.0f},
        .angles = {0.0f, 0.0f},
        .valid = false,
        .complete = false,
    };
    if (params == NULL || params->lift_ms == 0u || params->swing_ms == 0u ||
        params->lower_ms == 0u) {
        return pose;
    }

    const uint8_t bounded_amplitude = amplitude_percent > 100u
                                          ? 100u
                                          : amplitude_percent;
    const float scale = (float)bounded_amplitude / 100.0f;
    const float lift_mm = params->lift_mm * scale;
    const float forward_mm = params->forward_mm * scale;
    const uint32_t swing_start_ms = params->lift_ms;
    const uint32_t lower_start_ms = params->lift_ms + params->swing_ms;
    const uint32_t total_ms = lower_start_ms + params->lower_ms;

    pose.foot = params->start;
    if (elapsed_ms < swing_start_ms) {
        const float t = smoothstep5((float)elapsed_ms /
                                    (float)params->lift_ms);
        pose.foot.z_mm = params->start.z_mm + lift_mm * t;
    } else if (elapsed_ms < lower_start_ms) {
        const float t = smoothstep5((float)(elapsed_ms - swing_start_ms) /
                                    (float)params->swing_ms);
        pose.foot.x_mm = params->start.x_mm + forward_mm * t;
        pose.foot.z_mm = params->start.z_mm + lift_mm;
    } else {
        const uint32_t lower_elapsed = elapsed_ms >= total_ms
                                           ? params->lower_ms
                                           : elapsed_ms - lower_start_ms;
        const float t = smoothstep5((float)lower_elapsed /
                                    (float)params->lower_ms);
        pose.foot.x_mm = params->start.x_mm + forward_mm;
        pose.foot.z_mm = params->start.z_mm + lift_mm * (1.0f - t);
    }
    pose.complete = elapsed_ms >= total_ms;
    pose.valid = servo_zero_leg_ik(pose.foot, params->knee_branch,
                                   &pose.angles);
    return pose;
}

#define SERVO_ZERO_LEG_PI 3.14159265358979323846f
#define SERVO_ZERO_LEG_TWO_PI (2.0f * SERVO_ZERO_LEG_PI)

static float cosine_smooth(float t)
{
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return 0.5f - 0.5f * cosf(SERVO_ZERO_LEG_PI * t);
}

static float wrap_angle(float value)
{
    while (value > SERVO_ZERO_LEG_PI) {
        value -= SERVO_ZERO_LEG_TWO_PI;
    }
    while (value < -SERVO_ZERO_LEG_PI) {
        value += SERVO_ZERO_LEG_TWO_PI;
    }
    return value;
}

static float lerp_float(float start, float end, float t)
{
    return start + (end - start) * t;
}

static bool valid_knee_branch(int8_t knee_branch)
{
    return knee_branch == SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD ||
           knee_branch == SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD;
}

bool servo_zero_leg_knee_at_height(
    float hip_rad, float target_z_mm, float reference_knee_rad,
    int8_t knee_branch, float *knee_rad)
{
    if (knee_rad == NULL || !valid_knee_branch(knee_branch) ||
        !isfinite(hip_rad) || !isfinite(target_z_mm) ||
        !isfinite(reference_knee_rad)) {
        return false;
    }
    const float sine =
        (target_z_mm - SERVO_ZERO_LEG_THIGH_MM * sinf(hip_rad)) /
        SERVO_ZERO_LEG_SHANK_MM;
    if (!isfinite(sine) || sine < -1.0f - SERVO_ZERO_LEG_EPSILON ||
        sine > 1.0f + SERVO_ZERO_LEG_EPSILON) {
        return false;
    }
    const float principal = asinf(clamp_unit(sine));
    const float shank_candidates[2] = {
        principal,
        SERVO_ZERO_LEG_PI - principal,
    };
    float best_delta = 0.0f;
    float best_knee = 0.0f;
    bool found = false;
    for (size_t index = 0u; index < 2u; ++index) {
        const float candidate = wrap_angle(shank_candidates[index] - hip_rad);
        if (fabsf(candidate) > SERVO_ZERO_LEG_EPSILON &&
            (candidate > 0.0f ? SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD
                              : SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD) !=
                knee_branch) {
            continue;
        }
        const float delta = wrap_angle(candidate - reference_knee_rad);
        if (!found || fabsf(delta) < fabsf(best_delta)) {
            best_delta = delta;
            best_knee = candidate;
            found = true;
        }
    }
    if (!found) {
        return false;
    }
    *knee_rad = best_knee;
    return true;
}

/* Solve only q_k while q_h remains fixed.  The selected solution must move
 * in the requested branch direction so the first phase is genuinely a CH1
 * lift rather than an accidental branch flip. */
bool servo_zero_leg_knee_lift_at_height(
    servo_zero_leg_angles_t rest_angles, float target_z,
    int8_t knee_branch, float *knee_rad)
{
    if (knee_rad == NULL || !valid_knee_branch(knee_branch) ||
        !isfinite(target_z)) {
        return false;
    }
    const float sine =
        (target_z - SERVO_ZERO_LEG_THIGH_MM * sinf(rest_angles.hip_rad)) /
        SERVO_ZERO_LEG_SHANK_MM;
    if (!isfinite(sine) || sine < -1.0f - SERVO_ZERO_LEG_EPSILON ||
        sine > 1.0f + SERVO_ZERO_LEG_EPSILON) {
        return false;
    }
    if (fabsf(target_z -
              (SERVO_ZERO_LEG_THIGH_MM * sinf(rest_angles.hip_rad) +
               SERVO_ZERO_LEG_SHANK_MM *
                   sinf(rest_angles.hip_rad + rest_angles.knee_rad))) <=
        10.0f * SERVO_ZERO_LEG_EPSILON) {
        /* The first control tick can sample only a few milliseconds into a
         * smoothed lift.  Treat that sub-micron request as rest instead of
         * demanding a strictly signed root that floating-point rounding may
         * place below the direction threshold. */
        *knee_rad = rest_angles.knee_rad;
        return true;
    }

    const float principal = asinf(clamp_unit(sine));
    const float shank_candidates[2] = {
        principal,
        SERVO_ZERO_LEG_PI - principal,
    };
    float best_delta = 0.0f;
    float best_knee = 0.0f;
    bool found = false;
    for (size_t index = 0u; index < 2u; ++index) {
        const float candidate =
            wrap_angle(shank_candidates[index] - rest_angles.hip_rad);
        const float delta = wrap_angle(candidate - rest_angles.knee_rad);
        if ((float)knee_branch * delta > SERVO_ZERO_LEG_EPSILON &&
            (!found || fabsf(delta) < fabsf(best_delta))) {
            best_delta = delta;
            best_knee = candidate;
            found = true;
        }
    }
    if (!found) {
        return false;
    }
    *knee_rad = best_knee;
    return true;
}

static bool validate_swing_clearance(
    const servo_zero_leg_single_plan_t *plan)
{
    for (uint32_t sample = 0u; sample <= 100u; ++sample) {
        const float t = (float)sample / 100.0f;
        servo_zero_leg_angles_t angles = {
            .hip_rad = lerp_float(plan->knee_lift_angles.hip_rad,
                                  plan->swing_angles.hip_rad, t),
            .knee_rad = plan->knee_lift_angles.knee_rad,
        };
        servo_zero_leg_point_t foot;
        if (!servo_zero_leg_fk(angles, &foot) ||
            foot.z_mm < plan->rest_foot.z_mm - SERVO_ZERO_LEG_EPSILON) {
            return false;
        }
    }
    return true;
}

static bool validate_touchdown_path(
    const servo_zero_leg_single_plan_t *plan)
{
    for (uint32_t sample = 0u; sample <= 100u; ++sample) {
        const float t = (float)sample / 100.0f;
        const servo_zero_leg_point_t foot = {
            .x_mm = plan->swing_foot.x_mm,
            .z_mm = lerp_float(plan->swing_foot.z_mm,
                               plan->touchdown_foot.z_mm, t),
        };
        servo_zero_leg_angles_t angles;
        if (!servo_zero_leg_ik(foot, plan->knee_branch, &angles)) {
            return false;
        }
    }
    return true;
}

bool servo_zero_leg_make_single_plan(
    const servo_zero_leg_single_params_t *params,
    servo_zero_leg_single_plan_t *plan)
{
    if (plan == NULL) {
        return false;
    }
    *plan = (servo_zero_leg_single_plan_t){0};
    if (params == NULL || !valid_knee_branch(params->knee_branch) ||
        !isfinite(params->rest_angles.hip_rad) ||
        !isfinite(params->rest_angles.knee_rad) ||
        !isfinite(params->lift_clearance_mm) ||
        !isfinite(params->hip_swing_rad) || params->lift_clearance_mm < 0.0f ||
        params->rest_ms == 0u || params->knee_lift_ms == 0u ||
        params->hip_swing_ms == 0u || params->touchdown_ms == 0u) {
        return false;
    }
    if (params->rest_ms > UINT32_MAX - params->knee_lift_ms ||
        params->rest_ms + params->knee_lift_ms >
            UINT32_MAX - params->hip_swing_ms ||
        params->rest_ms + params->knee_lift_ms + params->hip_swing_ms >
            UINT32_MAX - params->touchdown_ms) {
        return false;
    }

    plan->rest_angles = params->rest_angles;
    plan->rest_ms = params->rest_ms;
    plan->knee_lift_ms = params->knee_lift_ms;
    plan->hip_swing_ms = params->hip_swing_ms;
    plan->touchdown_ms = params->touchdown_ms;
    plan->total_ms = params->rest_ms + params->knee_lift_ms +
                     params->hip_swing_ms + params->touchdown_ms;
    plan->knee_branch = params->knee_branch;
    if (!servo_zero_leg_fk(plan->rest_angles, &plan->rest_foot)) {
        return false;
    }

    plan->knee_lift_angles = plan->rest_angles;
    if (!servo_zero_leg_knee_lift_at_height(
            plan->rest_angles,
            plan->rest_foot.z_mm + params->lift_clearance_mm,
            params->knee_branch, &plan->knee_lift_angles.knee_rad) ||
        !servo_zero_leg_fk(plan->knee_lift_angles, &plan->knee_lift_foot)) {
        return false;
    }
    if (plan->knee_lift_foot.z_mm <
        plan->rest_foot.z_mm + params->lift_clearance_mm -
            10.0f * SERVO_ZERO_LEG_EPSILON) {
        return false;
    }

    plan->swing_angles = plan->knee_lift_angles;
    plan->swing_angles.hip_rad += params->hip_swing_rad;
    if (!servo_zero_leg_fk(plan->swing_angles, &plan->swing_foot) ||
        !validate_swing_clearance(plan)) {
        return false;
    }

    servo_zero_leg_angles_t swing_ik;
    if (!servo_zero_leg_ik(plan->swing_foot, plan->knee_branch, &swing_ik) ||
        fabsf(wrap_angle(swing_ik.hip_rad - plan->swing_angles.hip_rad)) >
            0.01f ||
        fabsf(wrap_angle(swing_ik.knee_rad - plan->swing_angles.knee_rad)) >
            0.01f) {
        return false;
    }

    plan->touchdown_foot = (servo_zero_leg_point_t){
        .x_mm = plan->swing_foot.x_mm,
        .z_mm = plan->rest_foot.z_mm,
    };
    if (!servo_zero_leg_ik(plan->touchdown_foot, plan->knee_branch,
                           &plan->touchdown_angles) ||
        !validate_touchdown_path(plan)) {
        return false;
    }

    plan->valid = true;
    return true;
}

servo_zero_leg_single_pose_t servo_zero_leg_single_pose(
    uint32_t elapsed_ms, const servo_zero_leg_single_plan_t *plan)
{
    servo_zero_leg_single_pose_t pose = {
        .foot = {0.0f, 0.0f},
        .angles = {0.0f, 0.0f},
        .phase = SERVO_ZERO_LEG_SINGLE_PHASE_REST,
        .valid = false,
        .complete = false,
    };
    if (plan == NULL || !plan->valid) {
        return pose;
    }

    const uint32_t knee_start = plan->rest_ms;
    const uint32_t swing_start = knee_start + plan->knee_lift_ms;
    const uint32_t touchdown_start = swing_start + plan->hip_swing_ms;
    pose.valid = true;
    if (elapsed_ms < knee_start) {
        pose.phase = SERVO_ZERO_LEG_SINGLE_PHASE_REST;
        pose.angles = plan->rest_angles;
        pose.foot = plan->rest_foot;
    } else if (elapsed_ms < swing_start) {
        const float t = smoothstep5(
            (float)(elapsed_ms - knee_start) / (float)plan->knee_lift_ms);
        pose.phase = SERVO_ZERO_LEG_SINGLE_PHASE_KNEE_LIFT;
        pose.angles.hip_rad = plan->rest_angles.hip_rad;
        pose.angles.knee_rad = lerp_float(plan->rest_angles.knee_rad,
                                          plan->knee_lift_angles.knee_rad, t);
        pose.valid = servo_zero_leg_fk(pose.angles, &pose.foot);
    } else if (elapsed_ms < touchdown_start) {
        const float t = smoothstep5(
            (float)(elapsed_ms - swing_start) / (float)plan->hip_swing_ms);
        pose.phase = SERVO_ZERO_LEG_SINGLE_PHASE_HIP_SWING;
        pose.angles.hip_rad = lerp_float(plan->knee_lift_angles.hip_rad,
                                         plan->swing_angles.hip_rad, t);
        pose.angles.knee_rad = plan->knee_lift_angles.knee_rad;
        pose.valid = servo_zero_leg_fk(pose.angles, &pose.foot);
    } else if (elapsed_ms < plan->total_ms) {
        const float t = smoothstep5(
            (float)(elapsed_ms - touchdown_start) /
            (float)plan->touchdown_ms);
        pose.phase = SERVO_ZERO_LEG_SINGLE_PHASE_TOUCHDOWN;
        pose.foot.x_mm = plan->swing_foot.x_mm;
        pose.foot.z_mm = lerp_float(plan->swing_foot.z_mm,
                                    plan->touchdown_foot.z_mm, t);
        pose.valid = servo_zero_leg_ik(pose.foot, plan->knee_branch,
                                       &pose.angles);
    } else {
        pose.phase = SERVO_ZERO_LEG_SINGLE_PHASE_COMPLETE;
        pose.angles = plan->touchdown_angles;
        pose.foot = plan->touchdown_foot;
        pose.complete = true;
    }
    return pose;
}

static servo_zero_leg_point_t cubic_bezier_point(
    servo_zero_leg_point_t p0, servo_zero_leg_point_t p1,
    servo_zero_leg_point_t p2, servo_zero_leg_point_t p3, float t)
{
    const float one_minus_t = 1.0f - t;
    const float w0 = one_minus_t * one_minus_t * one_minus_t;
    const float w1 = 3.0f * one_minus_t * one_minus_t * t;
    const float w2 = 3.0f * one_minus_t * t * t;
    const float w3 = t * t * t;
    return (servo_zero_leg_point_t){
        .x_mm = w0 * p0.x_mm + w1 * p1.x_mm +
                w2 * p2.x_mm + w3 * p3.x_mm,
        .z_mm = w0 * p0.z_mm + w1 * p1.z_mm +
                w2 * p2.z_mm + w3 * p3.z_mm,
    };
}

servo_zero_leg_flow_pose_t servo_zero_leg_flow_pose(
    uint32_t elapsed_ms, const servo_zero_leg_flow_plan_t *plan)
{
    servo_zero_leg_flow_pose_t pose = {0};
    if (plan == NULL || !plan->valid) {
        return pose;
    }
    if (elapsed_ms < plan->rest_ms) {
        pose.foot = plan->rest_foot;
        pose.angles = plan->rest_angles;
        pose.valid = true;
        return pose;
    }
    if (elapsed_ms >= plan->total_ms) {
        pose.foot = plan->touchdown_foot;
        pose.angles = plan->touchdown_angles;
        pose.valid = true;
        pose.complete = true;
        return pose;
    }
    const float normalized =
        (float)(elapsed_ms - plan->rest_ms) / (float)plan->move_ms;
    const float progress = smoothstep5(normalized);
    pose.foot = cubic_bezier_point(
        plan->rest_foot, plan->control_liftoff,
        plan->control_touchdown, plan->touchdown_foot, progress);
    pose.valid = servo_zero_leg_ik(pose.foot, plan->knee_branch,
                                   &pose.angles);
    return pose;
}

bool servo_zero_leg_make_flow_plan(
    const servo_zero_leg_flow_params_t *params,
    servo_zero_leg_flow_plan_t *plan)
{
    if (plan == NULL) {
        return false;
    }
    *plan = (servo_zero_leg_flow_plan_t){0};
    if (params == NULL || !valid_knee_branch(params->knee_branch) ||
        !isfinite(params->rest_angles.hip_rad) ||
        !isfinite(params->rest_angles.knee_rad) ||
        !isfinite(params->lift_clearance_mm) ||
        !isfinite(params->hip_swing_rad) ||
        params->lift_clearance_mm <= 0.0f || params->rest_ms == 0u ||
        params->move_ms == 0u || params->rest_ms > UINT32_MAX - params->move_ms) {
        return false;
    }
    plan->rest_angles = params->rest_angles;
    plan->rest_ms = params->rest_ms;
    plan->move_ms = params->move_ms;
    plan->total_ms = params->rest_ms + params->move_ms;
    plan->knee_branch = params->knee_branch;
    if (!servo_zero_leg_fk(plan->rest_angles, &plan->rest_foot)) {
        return false;
    }

    servo_zero_leg_angles_t swing_angles = plan->rest_angles;
    swing_angles.hip_rad += params->hip_swing_rad;
    servo_zero_leg_point_t swing_foot;
    if (!servo_zero_leg_fk(swing_angles, &swing_foot)) {
        return false;
    }
    plan->touchdown_foot = (servo_zero_leg_point_t){
        .x_mm = swing_foot.x_mm,
        .z_mm = plan->rest_foot.z_mm,
    };
    if (!servo_zero_leg_ik(plan->touchdown_foot, params->knee_branch,
                           &plan->touchdown_angles)) {
        return false;
    }

    /* With both controls at z0 + 4h/3, the curve reaches exactly z0 + h
     * at its midpoint. Matching each control x to the adjacent endpoint
     * gives vertical liftoff/touchdown tangents. */
    const float control_z =
        plan->rest_foot.z_mm + 4.0f * params->lift_clearance_mm / 3.0f;
    plan->control_liftoff = (servo_zero_leg_point_t){
        .x_mm = plan->rest_foot.x_mm,
        .z_mm = control_z,
    };
    plan->control_touchdown = (servo_zero_leg_point_t){
        .x_mm = plan->touchdown_foot.x_mm,
        .z_mm = control_z,
    };
    plan->valid = true;
    for (uint32_t sample = 0u; sample <= 100u; ++sample) {
        const uint32_t elapsed = plan->rest_ms +
            (uint32_t)(((uint64_t)plan->move_ms * sample) / 100u);
        const servo_zero_leg_flow_pose_t pose =
            servo_zero_leg_flow_pose(elapsed, plan);
        if (!pose.valid ||
            pose.foot.z_mm < plan->rest_foot.z_mm - SERVO_ZERO_LEG_EPSILON) {
            plan->valid = false;
            return false;
        }
    }
    return true;
}

bool servo_zero_leg_make_vertical_sweep_plan(
    const servo_zero_leg_vertical_sweep_params_t *params,
    servo_zero_leg_vertical_sweep_plan_t *plan)
{
    if (plan == NULL) {
        return false;
    }
    *plan = (servo_zero_leg_vertical_sweep_plan_t){0};
    if (params == NULL || !isfinite(params->start_hip_rad) ||
        !isfinite(params->end_hip_rad) || !isfinite(params->shank_rad) ||
        params->move_ms == 0u ||
        fabsf(params->end_hip_rad - params->start_hip_rad) <=
            SERVO_ZERO_LEG_EPSILON) {
        return false;
    }

    plan->start_angles = (servo_zero_leg_angles_t){
        .hip_rad = params->start_hip_rad,
        .knee_rad = params->shank_rad - params->start_hip_rad,
    };
    plan->end_angles = (servo_zero_leg_angles_t){
        .hip_rad = params->end_hip_rad,
        .knee_rad = params->shank_rad - params->end_hip_rad,
    };
    plan->shank_rad = params->shank_rad;
    plan->move_ms = params->move_ms;
    if (!servo_zero_leg_fk(plan->start_angles, &plan->start_foot) ||
        !servo_zero_leg_fk(plan->end_angles, &plan->end_foot)) {
        return false;
    }

    /* q_h + q_k is held exactly at the requested absolute shank direction;
     * no IK branch selection is needed because the relative knee angle is
     * explicitly continued from the current hip angle. */
    plan->valid = true;
    return true;
}

servo_zero_leg_vertical_sweep_pose_t servo_zero_leg_vertical_sweep_pose(
    uint32_t elapsed_ms,
    const servo_zero_leg_vertical_sweep_plan_t *plan)
{
    servo_zero_leg_vertical_sweep_pose_t pose = {
        .angles = {0.0f, 0.0f},
        .foot = {0.0f, 0.0f},
        .shank_rad = 0.0f,
        .valid = false,
        .complete = false,
    };
    if (plan == NULL || !plan->valid || plan->move_ms == 0u) {
        return pose;
    }

    const float progress = elapsed_ms >= plan->move_ms
                               ? 1.0f
                               : smoothstep5((float)elapsed_ms /
                                             (float)plan->move_ms);
    pose.angles.hip_rad = lerp_float(plan->start_angles.hip_rad,
                                     plan->end_angles.hip_rad, progress);
    pose.angles.knee_rad = plan->shank_rad - pose.angles.hip_rad;
    pose.shank_rad = pose.angles.hip_rad + pose.angles.knee_rad;
    pose.valid = servo_zero_leg_fk(pose.angles, &pose.foot) &&
                 fabsf(pose.shank_rad - plan->shank_rad) <=
                     10.0f * SERVO_ZERO_LEG_EPSILON;
    pose.complete = elapsed_ms >= plan->move_ms;
    return pose;
}

static bool map_pulse(float value, uint16_t min_us, uint16_t max_us,
                      uint16_t *pulse_us)
{
    if (pulse_us == NULL || !isfinite(value) || value < (float)min_us ||
        value > (float)max_us) {
        return false;
    }
    *pulse_us = (uint16_t)lroundf(value);
    return *pulse_us >= min_us && *pulse_us <= max_us;
}

servo_zero_leg_pulse_pose_t servo_zero_leg_map_angles_to_pulses(
    servo_zero_leg_angles_t rest_angles,
    servo_zero_leg_angles_t angles,
    const servo_zero_leg_pulse_map_t *map)
{
    servo_zero_leg_pulse_pose_t pose = {
        .hip_us = 0u,
        .knee_us = 0u,
        .valid = false,
    };
    if (map == NULL || map->min_us > map->max_us ||
        !isfinite(rest_angles.hip_rad) || !isfinite(rest_angles.knee_rad) ||
        !isfinite(angles.hip_rad) || !isfinite(angles.knee_rad) ||
        !isfinite(map->hip_us_per_rad) || !isfinite(map->knee_us_per_rad)) {
        return pose;
    }
    const float hip_delta = angles.hip_rad - rest_angles.hip_rad;
    const float knee_delta = angles.knee_rad - rest_angles.knee_rad;
    const float hip_pulse =
        (float)map->hip_rest_us + map->hip_us_per_rad * hip_delta;
    const float knee_pulse =
        (float)map->knee_rest_us + map->knee_us_per_rad * knee_delta;
    pose.valid = map_pulse(hip_pulse, map->min_us, map->max_us,
                           &pose.hip_us) &&
                 map_pulse(knee_pulse, map->min_us, map->max_us,
                           &pose.knee_us);
    return pose;
}

bool servo_zero_motionleg_make_plan(
    servo_zero_leg_angles_t rest_angles, float knee_lift_rad,
    float hip_swing_rad, int8_t knee_branch,
    servo_zero_motionleg_plan_t *plan)
{
    if (plan == NULL || !valid_knee_branch(knee_branch) ||
        !isfinite(rest_angles.hip_rad) || !isfinite(rest_angles.knee_rad) ||
        !isfinite(knee_lift_rad) || !isfinite(hip_swing_rad) ||
        knee_lift_rad == 0.0f) {
        return false;
    }
    *plan = (servo_zero_motionleg_plan_t){0};
    plan->rest_angles = rest_angles;
    plan->lifted_angles = rest_angles;
    plan->lifted_angles.knee_rad += knee_lift_rad;
    plan->swing_angles = plan->lifted_angles;
    plan->swing_angles.hip_rad += hip_swing_rad;
    /* The second transient swings the hip while holding the lifted knee.
     * The original lift remains available for the later release-to-ground
     * action. */
    plan->hip_swing_rad = hip_swing_rad;
    plan->knee_branch = knee_branch;
    if (!servo_zero_leg_fk(plan->rest_angles, &plan->rest_foot) ||
        !servo_zero_leg_fk(plan->lifted_angles, &plan->lifted_foot) ||
        !servo_zero_leg_fk(plan->swing_angles, &plan->swing_foot) ||
        plan->lifted_foot.z_mm < plan->rest_foot.z_mm) {
        return false;
    }
    plan->touchdown_foot = plan->swing_foot;
    plan->touchdown_foot.z_mm = plan->rest_foot.z_mm;
    plan->touchdown_angles = plan->swing_angles;
    /* Release recovery is deliberately the accumulated rotation: first undo
     * the knee lift, then add the opposite of the frozen hip swing.  Solving
     * only for a fixed-x/fixed-z IK target underestimates this return and was
     * observed as an undersized recovery at 20 degrees. */
    plan->touchdown_angles.knee_rad =
        rest_angles.knee_rad - hip_swing_rad;
    if (!servo_zero_leg_fk(plan->touchdown_angles, &plan->touchdown_foot)) {
        return false;
    }
    plan->valid = true;
    return true;
}

servo_zero_motionleg_pose_t servo_zero_motionleg_pose(
    uint32_t elapsed_ms, uint32_t release_ms,
    uint32_t lift_ms, uint32_t wait_ms, uint32_t swing_ms,
    uint32_t touchdown_ms,
    const servo_zero_motionleg_plan_t *plan)
{
    servo_zero_motionleg_pose_t pose = {0};
    if (plan == NULL || !plan->valid || lift_ms == 0u || swing_ms == 0u ||
        touchdown_ms == 0u || release_ms < lift_ms ||
        wait_ms > UINT32_MAX - lift_ms) {
        return pose;
    }
    pose.valid = true;
    if (elapsed_ms < lift_ms) {
        const float t = smoothstep5((float)elapsed_ms / (float)lift_ms);
        pose.phase = SERVO_ZERO_MOTIONLEG_LIFT;
        pose.angles.hip_rad = plan->rest_angles.hip_rad;
        pose.angles.knee_rad = lerp_float(plan->rest_angles.knee_rad,
                                          plan->lifted_angles.knee_rad, t);
    } else {
        const bool released = release_ms != UINT32_MAX;
        const uint32_t swing_start_ms = lift_ms + wait_ms;
        const uint32_t effective_release_ms =
            released && release_ms < swing_start_ms ? swing_start_ms
                                                     : release_ms;
        if (elapsed_ms < swing_start_ms &&
            (!released || elapsed_ms < effective_release_ms)) {
            pose.phase = SERVO_ZERO_MOTIONLEG_WAIT;
            pose.angles = plan->lifted_angles;
            pose.foot = plan->lifted_foot;
            return pose;
        }
        const uint32_t swing_elapsed = released
                                           ? effective_release_ms - swing_start_ms
                                           : elapsed_ms - swing_start_ms;
        const float swing_t = swing_elapsed >= swing_ms
                                  ? 1.0f
                                  : cosine_smooth((float)swing_elapsed /
                                                  (float)swing_ms);
        const float hip = lerp_float(plan->lifted_angles.hip_rad,
                                     plan->swing_angles.hip_rad, swing_t);
        const float knee = lerp_float(plan->lifted_angles.knee_rad,
                                      plan->swing_angles.knee_rad, swing_t);
        if (!released || elapsed_ms < effective_release_ms) {
            pose.phase = SERVO_ZERO_MOTIONLEG_SWING;
            pose.angles.hip_rad = hip;
            pose.angles.knee_rad = knee;
        } else {
            const uint32_t touchdown_elapsed =
                elapsed_ms - effective_release_ms;
            const float t = touchdown_elapsed >= touchdown_ms
                                ? 1.0f
                                : smoothstep5((float)touchdown_elapsed /
                                              (float)touchdown_ms);
            pose.phase = touchdown_elapsed >= touchdown_ms
                             ? SERVO_ZERO_MOTIONLEG_COMPLETE
                             : SERVO_ZERO_MOTIONLEG_TOUCHDOWN;
            pose.angles.hip_rad = hip;
            const float touchdown_knee_rad =
                plan->rest_angles.knee_rad -
                (hip - plan->rest_angles.hip_rad);
            pose.angles.knee_rad = lerp_float(
                plan->lifted_angles.knee_rad, touchdown_knee_rad, t);
            if (pose.phase == SERVO_ZERO_MOTIONLEG_COMPLETE) {
                pose.complete = true;
            }
        }
    }
    pose.valid = servo_zero_leg_fk(pose.angles, &pose.foot);
    return pose;
}
