#include "servo_zero_quad_demo.h"

#include <math.h>
#include <stddef.h>

#define SERVO_ZERO_QUAD_DEMO_EPSILON 0.0001f
#define SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM 0.5f
#define SERVO_ZERO_QUAD_DEMO_LIVE_OFFSET_BINARY_ITERATIONS 24u
#define SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS 32u
#define SERVO_ZERO_QUAD_DEMO_MAX_KNEE_STEP_RAD 0.7853982f

static bool valid_point(servo_zero_leg_point_t point)
{
    return isfinite(point.x_mm) && isfinite(point.z_mm);
}

static bool valid_duration(uint32_t duration_ms)
{
    return duration_ms > 0u;
}

static bool add_duration(uint32_t left, uint32_t right, uint32_t *sum)
{
    if (sum == NULL || UINT32_MAX - left < right) {
        return false;
    }
    *sum = left + right;
    return true;
}

static bool valid_map(const servo_zero_leg_pulse_map_t *map)
{
    return map != NULL && map->min_us <= map->max_us &&
           map->hip_rest_us >= map->min_us &&
           map->hip_rest_us <= map->max_us &&
           map->knee_rest_us >= map->min_us &&
           map->knee_rest_us <= map->max_us &&
           isfinite(map->hip_us_per_rad) &&
           isfinite(map->knee_us_per_rad) &&
           fabsf(map->hip_us_per_rad) > SERVO_ZERO_QUAD_DEMO_EPSILON &&
           fabsf(map->knee_us_per_rad) > SERVO_ZERO_QUAD_DEMO_EPSILON;
}

static bool valid_pulse_limits(
    const servo_zero_quad_demo_pulse_limits_t *limits)
{
    if (limits == NULL || !limits->configured) {
        return false;
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        const servo_zero_quad_demo_channel_limit_t *limit =
            &limits->channel[channel];
        if ((limit->has_min &&
             (limit->min_us < SERVO_ZERO_QUAD_DEMO_TRANSPORT_MIN_US ||
              limit->min_us > SERVO_ZERO_QUAD_DEMO_TRANSPORT_MAX_US)) ||
            (limit->has_max &&
             (limit->max_us < SERVO_ZERO_QUAD_DEMO_TRANSPORT_MIN_US ||
              limit->max_us > SERVO_ZERO_QUAD_DEMO_TRANSPORT_MAX_US)) ||
            (!limit->has_min && limit->min_us != 0u) ||
            (!limit->has_max && limit->max_us != 0u) ||
            (limit->has_min && limit->has_max &&
             limit->min_us >= limit->max_us)) {
            return false;
        }
    }
    return true;
}

static float clamp01(float value)
{
    return value <= 0.0f ? 0.0f : (value >= 1.0f ? 1.0f : value);
}

static float smoothstep5(float value)
{
    const float t = clamp01(value);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float clamp_float(float value, float minimum, float maximum)
{
    return value <= minimum ? minimum
                            : (value >= maximum ? maximum : value);
}

static bool leg_is_active(const servo_zero_quad_demo_plan_t *plan,
                          uint8_t leg)
{
    const uint8_t mask = plan->active_leg_mask != 0u
                             ? plan->active_leg_mask
                             : (uint8_t)(1u << plan->active_leg);
    return (mask & (uint8_t)(1u << leg)) != 0u;
}

static uint8_t active_leg_count(uint8_t mask)
{
    uint8_t count = 0u;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((mask & (uint8_t)(1u << leg)) != 0u) {
            ++count;
        }
    }
    return count;
}

servo_zero_leg_point_t servo_zero_quad_demo_limit_start_foot(
    servo_zero_leg_point_t foot)
{
    foot.x_mm = clamp_float(foot.x_mm,
                            SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM,
                            SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM);
    foot.z_mm = clamp_float(foot.z_mm,
                            SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                            SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
    return foot;
}

static float phase_progress(uint32_t elapsed_ms, uint32_t start_ms,
                            uint32_t duration_ms)
{
    if (elapsed_ms <= start_ms) {
        return 0.0f;
    }
    return smoothstep5((float)(elapsed_ms - start_ms) /
                       (float)duration_ms);
}

static bool channel_is_unique(const servo_zero_quad_demo_plan_t *plan)
{
    bool used[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT] = {false};
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t channels[2] = {
            plan->leg[leg].hip_channel,
            plan->leg[leg].knee_channel,
        };
        for (uint8_t joint = 0u; joint < 2u; ++joint) {
            if (channels[joint] >= SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT ||
                used[channels[joint]]) {
                return false;
            }
            used[channels[joint]] = true;
        }
    }
    return true;
}

bool servo_zero_quad_demo_pulses_within_limits(
    const servo_zero_quad_demo_plan_t *plan,
    const uint16_t pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT],
    uint8_t *bad_channel, uint16_t *bad_pulse_us, uint16_t *bad_min_us,
    uint16_t *bad_max_us)
{
    if (plan == NULL || pulse_us == NULL ||
        !valid_pulse_limits(&plan->pulse_limits)) {
        return false;
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        const servo_zero_quad_demo_channel_limit_t *limit =
            &plan->pulse_limits.channel[channel];
        const uint16_t minimum = limit->min_us;
        const uint16_t maximum = limit->max_us;
        const bool below_min = limit->has_min && pulse_us[channel] < minimum;
        const bool above_max = limit->has_max && pulse_us[channel] > maximum;
        if (below_min || above_max) {
            if (bad_channel != NULL) {
                *bad_channel = channel;
            }
            if (bad_pulse_us != NULL) {
                *bad_pulse_us = pulse_us[channel];
            }
            if (bad_min_us != NULL) {
                *bad_min_us = minimum;
            }
            if (bad_max_us != NULL) {
                *bad_max_us = maximum;
            }
            return false;
        }
    }
    return true;
}

static bool pulse_within_channel_limit(
    const servo_zero_quad_demo_channel_limit_t *limit, uint16_t pulse_us)
{
    return limit != NULL &&
           (!limit->has_min || pulse_us >= limit->min_us) &&
           (!limit->has_max || pulse_us <= limit->max_us);
}

/* Map one foot target with the same candidate q->us reference used by the
 * complete pose evaluator, then apply only this leg's independent limits. */
static bool leg_point_is_safe(const servo_zero_quad_demo_plan_t *plan,
                              uint8_t leg, servo_zero_leg_point_t point)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        !valid_point(point)) {
        return false;
    }
    servo_zero_leg_angles_t start_angles;
    servo_zero_leg_angles_t angles;
    if (!servo_zero_leg_ik(plan->start_foot[leg],
                           plan->leg[leg].knee_branch, &start_angles) ||
        !servo_zero_leg_ik(point, plan->leg[leg].knee_branch, &angles)) {
        return false;
    }
    const servo_zero_leg_pulse_pose_t pulses =
        servo_zero_leg_map_angles_to_pulses(
            start_angles, angles, &plan->leg[leg].pulse_map);
    if (!pulses.valid) {
        return false;
    }
    const uint8_t hip_channel = plan->leg[leg].hip_channel;
    const uint8_t knee_channel = plan->leg[leg].knee_channel;
    return pulse_within_channel_limit(
               &plan->pulse_limits.channel[hip_channel], pulses.hip_us) &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[knee_channel], pulses.knee_us);
}

/* Find the closest reachable x at the requested height.  The scan is
 * intentionally bounded by the foot workspace and is used only when the
 * requested point would cross a per-leg soft limit.  It therefore saturates
 * that leg at its own boundary instead of invalidating the four-leg frame. */
static float clamp_leg_x_to_soft_limits(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg, float requested_x,
    float z_mm)
{
    const float minimum = SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM;
    const float maximum = SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM;
    const float bounded_request = clamp_float(requested_x, minimum, maximum);
    if (leg_point_is_safe(
            plan, leg,
            (servo_zero_leg_point_t){.x_mm = bounded_request, .z_mm = z_mm})) {
        return bounded_request;
    }

    float best_x = bounded_request;
    float best_distance = INFINITY;
    const uint32_t sample_count = (uint32_t)((maximum - minimum) /
                                             SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM);
    for (uint32_t sample = 0u; sample <= sample_count; ++sample) {
        const float candidate =
            sample == sample_count
                ? maximum
                : minimum + (float)sample *
                                SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM;
        if (!leg_point_is_safe(
                plan, leg,
                (servo_zero_leg_point_t){.x_mm = candidate, .z_mm = z_mm})) {
            continue;
        }
        const float distance = fabsf(candidate - bounded_request);
        if (distance < best_distance) {
            best_x = candidate;
            best_distance = distance;
        }
    }
    return best_distance < INFINITY ? best_x : bounded_request;
}

static void apply_foot_soft_limits(
    const servo_zero_quad_demo_plan_t *plan,
    servo_zero_leg_point_t foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT],
    bool *soft_limit_clamped, uint8_t *soft_limit_leg_mask)
{
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const servo_zero_leg_point_t before = foot[leg];
        foot[leg] = servo_zero_quad_demo_limit_start_foot(foot[leg]);
        foot[leg].x_mm = clamp_leg_x_to_soft_limits(
            plan, leg, foot[leg].x_mm, foot[leg].z_mm);
        if ((fabsf(before.x_mm - foot[leg].x_mm) >
             SERVO_ZERO_QUAD_DEMO_EPSILON) ||
            (fabsf(before.z_mm - foot[leg].z_mm) >
             SERVO_ZERO_QUAD_DEMO_EPSILON)) {
            if (soft_limit_clamped != NULL) {
                *soft_limit_clamped = true;
            }
            if (soft_limit_leg_mask != NULL) {
                *soft_limit_leg_mask |= (uint8_t)(1u << leg);
            }
        }
    }
}

static void make_foot_targets(uint32_t elapsed_ms,
                              const servo_zero_quad_demo_plan_t *plan,
                              servo_zero_quad_demo_phase_t *phase,
                              servo_zero_leg_point_t foot[],
                              bool *soft_limit_clamped,
                              uint8_t *soft_limit_leg_mask)
{
    const uint32_t lift_start = plan->settle_ms;
    const uint32_t forward_start = lift_start + plan->lift_ms;
    const uint32_t search_start = forward_start + plan->swing_ms;
    const uint32_t motion_ms = plan->total_ms - plan->settle_ms;
    *phase = SERVO_ZERO_QUAD_DEMO_PHASE_SETTLE;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        foot[leg] = plan->start_foot[leg];
        foot[leg] = servo_zero_quad_demo_limit_start_foot(foot[leg]);
    }

    if (elapsed_ms < lift_start) {
        apply_foot_soft_limits(plan, foot, soft_limit_clamped,
                               soft_limit_leg_mask);
        return;
    }

    const uint32_t motion_elapsed = elapsed_ms - lift_start;
    const float stance_progress = phase_progress(
        motion_elapsed, 0u, motion_ms);
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (!leg_is_active(plan, leg)) {
            foot[leg].x_mm = plan->start_foot[leg].x_mm -
                             plan->stance_step_length_mm * stance_progress;
        }
    }

    if (elapsed_ms < forward_start) {
        *phase = SERVO_ZERO_QUAD_DEMO_PHASE_SWING_LIFT;
        const float progress = phase_progress(
            elapsed_ms, lift_start, plan->lift_ms);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (leg_is_active(plan, leg)) {
                foot[leg].x_mm = plan->start_foot[leg].x_mm -
                                 plan->lift_back_mm * progress;
                foot[leg].z_mm = clamp_float(
                    plan->start_foot[leg].z_mm + plan->lift_mm * progress,
                    SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                    SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
            }
        }
        apply_foot_soft_limits(plan, foot, soft_limit_clamped,
                               soft_limit_leg_mask);
        return;
    }
    if (elapsed_ms < search_start) {
        *phase = SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD;
        const float progress = phase_progress(
            elapsed_ms, forward_start, plan->swing_ms);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (leg_is_active(plan, leg)) {
                const float lift_x = plan->start_foot[leg].x_mm -
                                     plan->lift_back_mm;
                foot[leg].x_mm = lift_x +
                                 (plan->leg_step_length_mm[leg] +
                                  plan->lift_back_mm) * progress;
                foot[leg].z_mm = clamp_float(
                    plan->start_foot[leg].z_mm + plan->lift_mm,
                    SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                    SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
            }
        }
        apply_foot_soft_limits(plan, foot, soft_limit_clamped,
                               soft_limit_leg_mask);
        return;
    }
    if (elapsed_ms < plan->total_ms) {
        *phase = SERVO_ZERO_QUAD_DEMO_PHASE_SEARCH;
        const float progress = phase_progress(
            elapsed_ms, search_start, plan->search_ms);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (leg_is_active(plan, leg)) {
                foot[leg].x_mm = plan->start_foot[leg].x_mm +
                                 plan->leg_step_length_mm[leg];
                foot[leg].z_mm = clamp_float(
                    plan->start_foot[leg].z_mm +
                        plan->lift_mm * (1.0f - progress),
                    SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                    SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
            }
        }
        apply_foot_soft_limits(plan, foot, soft_limit_clamped,
                               soft_limit_leg_mask);
        return;
    }

    *phase = SERVO_ZERO_QUAD_DEMO_PHASE_COMPLETE;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (leg_is_active(plan, leg)) {
            foot[leg].x_mm = plan->start_foot[leg].x_mm +
                             plan->leg_step_length_mm[leg];
            foot[leg].z_mm = clamp_float(
                plan->start_foot[leg].z_mm,
                SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
        }
    }
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (!leg_is_active(plan, leg)) {
            foot[leg].x_mm = plan->start_foot[leg].x_mm -
                             plan->stance_step_length_mm;
        }
    }
    apply_foot_soft_limits(plan, foot, soft_limit_clamped,
                           soft_limit_leg_mask);
}

static bool actual_foot_in_workspace(servo_zero_leg_point_t foot)
{
    return foot.x_mm >= SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM -
               SERVO_ZERO_QUAD_DEMO_EPSILON &&
           foot.x_mm <= SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM +
               SERVO_ZERO_QUAD_DEMO_EPSILON &&
           foot.z_mm >= SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM -
               SERVO_ZERO_QUAD_DEMO_EPSILON &&
           foot.z_mm <= SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM +
               SERVO_ZERO_QUAD_DEMO_EPSILON;
}

static bool knee_branch_matches(float knee_rad, int8_t knee_branch)
{
    if (!isfinite(knee_rad) ||
        (knee_branch != SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD &&
         knee_branch != SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD)) {
        return false;
    }
    if (fabsf(knee_rad) <= SERVO_ZERO_QUAD_DEMO_EPSILON) {
        return false;
    }
    return (knee_rad > 0.0f ? SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD
                            : SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD) ==
           knee_branch;
}

/* Convert one requested active-leg target into the joint-space-split pose.
 * The knee is re-solved at the requested z so this is a genuine joint-space
 * split, rather than merely shortening the x step. */
static bool make_scaled_active_leg_pose(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t requested,
    servo_zero_leg_angles_t *angles,
    servo_zero_leg_point_t *actual_foot)
{
    if (plan == NULL || angles == NULL || actual_foot == NULL ||
        leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        plan->active_hip_scale_percent > 100u ||
        !valid_point(requested)) {
        return false;
    }
    servo_zero_leg_angles_t start_angles;
    servo_zero_leg_angles_t target_angles;
    if (!servo_zero_leg_ik(plan->start_foot[leg],
                           plan->leg[leg].knee_branch, &start_angles) ||
        !servo_zero_leg_ik(requested, plan->leg[leg].knee_branch,
                           &target_angles)) {
        return false;
    }
    const float scale = (float)plan->active_hip_scale_percent / 100.0f;
    angles->hip_rad = start_angles.hip_rad +
                      scale * (target_angles.hip_rad - start_angles.hip_rad);
    angles->knee_rad = target_angles.knee_rad;
    if (!servo_zero_leg_knee_at_height(
            angles->hip_rad, requested.z_mm, target_angles.knee_rad,
            plan->leg[leg].knee_branch, &angles->knee_rad) ||
        !servo_zero_leg_fk(*angles, actual_foot)) {
        return false;
    }
    const servo_zero_leg_pulse_pose_t pulses =
        servo_zero_leg_map_angles_to_pulses(
            start_angles, *angles, &plan->leg[leg].pulse_map);
    return actual_foot_in_workspace(*actual_foot) && pulses.valid &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[plan->leg[leg].hip_channel],
               pulses.hip_us) &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[plan->leg[leg].knee_channel],
               pulses.knee_us);
}

/* Reduce only the hip excursion of selected swing legs while preserving the
 * requested foot height. If the rebalanced pose would cross the actual foot
 * workspace, saturate its requested x to the nearest safe point. Support-leg
 * retreat remains unchanged because it is the body propulsion term. */
static bool apply_active_hip_scale(
    const servo_zero_quad_demo_plan_t *plan,
    servo_zero_quad_demo_pose_t *pose)
{
    if (plan == NULL || pose == NULL ||
        plan->active_hip_scale_percent > 100u) {
        return false;
    }
    if (plan->active_hip_scale_percent ==
        SERVO_ZERO_QUAD_DEMO_FULL_HIP_SCALE_PERCENT) {
        return true;
    }
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (!leg_is_active(plan, leg)) {
            continue;
        }
        const servo_zero_leg_point_t requested = pose->foot[leg];
        servo_zero_leg_angles_t selected_angles;
        servo_zero_leg_point_t selected_foot;
        if (make_scaled_active_leg_pose(plan, leg, requested,
                                        &selected_angles, &selected_foot)) {
            pose->angles[leg] = selected_angles;
            pose->foot[leg] = selected_foot;
            continue;
        }

        float best_distance = INFINITY;
        servo_zero_leg_angles_t best_angles = {0};
        servo_zero_leg_point_t best_foot = {0};
        const uint32_t sample_count = (uint32_t)(
            (SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM -
             SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM) /
            SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM);
        for (uint32_t sample = 0u; sample <= sample_count; ++sample) {
            const float candidate_x =
                sample == sample_count
                    ? SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM
                    : SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM +
                          (float)sample *
                              SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM;
            const servo_zero_leg_point_t candidate = {
                .x_mm = candidate_x,
                .z_mm = requested.z_mm,
            };
            if (!make_scaled_active_leg_pose(
                    plan, leg, candidate, &selected_angles, &selected_foot)) {
                continue;
            }
            const float distance = fabsf(candidate_x - requested.x_mm);
            if (distance < best_distance) {
                best_distance = distance;
                best_angles = selected_angles;
                best_foot = selected_foot;
            }
        }
        if (best_distance == INFINITY) {
            return false;
        }
        pose->angles[leg] = best_angles;
        pose->foot[leg] = best_foot;
    }
    return true;
}

static bool make_exact_leg_pose(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t requested, servo_zero_leg_angles_t *angles,
    servo_zero_leg_point_t *actual_foot)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        angles == NULL || actual_foot == NULL ||
        !actual_foot_in_workspace(requested) ||
        !servo_zero_leg_ik(requested, plan->leg[leg].knee_branch, angles) ||
        !knee_branch_matches(angles->knee_rad,
                             plan->leg[leg].knee_branch) ||
        !servo_zero_leg_fk(*angles, actual_foot) ||
        !actual_foot_in_workspace(*actual_foot)) {
        return false;
    }
    servo_zero_leg_angles_t start_angles;
    if (!servo_zero_leg_ik(plan->start_foot[leg],
                           plan->leg[leg].knee_branch, &start_angles)) {
        return false;
    }
    const servo_zero_leg_pulse_pose_t mapped =
        servo_zero_leg_map_angles_to_pulses(
            start_angles, *angles, &plan->leg[leg].pulse_map);
    return mapped.valid &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[plan->leg[leg].hip_channel],
               mapped.hip_us) &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[plan->leg[leg].knee_channel],
               mapped.knee_us);
}

/* Evaluate the knee-first arc at one height.  The dedicated lift solver is
 * essential here: a fixed hip can have two same-branch knee roots at one z,
 * and nearest-root selection can reverse the knee near a vertical shank. */
static bool make_live_knee_arc_pose_at_z(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg, float target_z_mm,
    servo_zero_leg_angles_t *angles, servo_zero_leg_point_t *actual_foot)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        angles == NULL || actual_foot == NULL || !isfinite(target_z_mm)) {
        return false;
    }
    servo_zero_leg_angles_t start_angles;
    if (!servo_zero_leg_ik(plan->start_foot[leg],
                           plan->leg[leg].knee_branch, &start_angles)) {
        return false;
    }
    angles->hip_rad = start_angles.hip_rad;
    if (!servo_zero_leg_knee_lift_at_height(
            start_angles, target_z_mm, plan->leg[leg].knee_branch,
            &angles->knee_rad) ||
        !knee_branch_matches(angles->knee_rad,
                             plan->leg[leg].knee_branch) ||
        !servo_zero_leg_fk(*angles, actual_foot) ||
        !actual_foot_in_workspace(*actual_foot)) {
        return false;
    }
    const servo_zero_leg_pulse_pose_t mapped =
        servo_zero_leg_map_angles_to_pulses(
            start_angles, *angles, &plan->leg[leg].pulse_map);
    return mapped.valid &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[plan->leg[leg].hip_channel],
               mapped.hip_us) &&
           pulse_within_channel_limit(
               &plan->pulse_limits.channel[plan->leg[leg].knee_channel],
               mapped.knee_us);
}

/* The deliberate swing is useful for propulsion, but the full horizontal
 * swing point is not a good touchdown point for this two-link model: it
 * leaves the shank too far from the near-vertical support attitude.  Keep a
 * small, explicit portion of that displacement at touchdown.  This is a
 * software contact candidate, not a claim that a point-foot model proves a
 * physical sole is flat. */
static float live_touchdown_swing_offset_mm(
    const servo_zero_quad_demo_plan_t *plan, float swing_offset_mm)
{
    if (plan == NULL || !isfinite(swing_offset_mm) || swing_offset_mm < 0.0f) {
        return 0.0f;
    }
    const float active_scale =
        (float)plan->active_hip_scale_percent / 100.0f;
    const float touchdown_retention =
        (float)SERVO_ZERO_QUAD_DEMO_TOUCHDOWN_SWING_RETENTION_PERCENT /
        100.0f;
    return swing_offset_mm * active_scale * touchdown_retention;
}

static bool live_touchdown_path_is_safe(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t from_foot, servo_zero_leg_angles_t from_angles,
    float deliberate_swing_mm)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        !actual_foot_in_workspace(from_foot) ||
        !isfinite(deliberate_swing_mm) || deliberate_swing_mm < 0.0f) {
        return false;
    }
    servo_zero_leg_angles_t previous = from_angles;
    for (uint32_t sample = 1u;
         sample <= SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS; ++sample) {
        const float ratio =
            (float)sample /
            (float)SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS;
        const float target_z =
            from_foot.z_mm +
            (plan->start_foot[leg].z_mm - from_foot.z_mm) * ratio;
        servo_zero_leg_angles_t knee_arc_angles;
        servo_zero_leg_point_t knee_arc_foot;
        if (!make_live_knee_arc_pose_at_z(
                plan, leg, target_z, &knee_arc_angles, &knee_arc_foot)) {
            return false;
        }
        servo_zero_leg_angles_t candidate;
        servo_zero_leg_point_t actual;
        const float target_x = knee_arc_foot.x_mm + deliberate_swing_mm;
        const float path_x = from_foot.x_mm +
                             (target_x - from_foot.x_mm) * ratio;
        if (!make_exact_leg_pose(
                plan, leg,
                (servo_zero_leg_point_t){
                    .x_mm = path_x,
                    .z_mm = target_z},
                &candidate, &actual) ||
            fabsf(candidate.knee_rad - previous.knee_rad) >
                SERVO_ZERO_QUAD_DEMO_MAX_KNEE_STEP_RAD) {
            return false;
        }
        previous = candidate;
    }
    return true;
}

static bool make_live_lift_leg_pose(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_angles_t *angles, servo_zero_leg_point_t *actual_foot)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        angles == NULL || actual_foot == NULL) {
        return false;
    }
    const servo_zero_leg_point_t start = plan->start_foot[leg];
    servo_zero_leg_angles_t start_angles;
    if (!servo_zero_leg_ik(start, plan->leg[leg].knee_branch,
                           &start_angles)) {
        return false;
    }
    const float lifted_z = clamp_float(
        start.z_mm + plan->lift_mm,
        SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
        SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
    if (!make_live_knee_arc_pose_at_z(
            plan, leg, lifted_z, angles, actual_foot)) {
        return false;
    }
    float previous_knee = start_angles.knee_rad;
    float previous_z = start.z_mm;
    for (uint32_t sample = 1u;
         sample <= SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS; ++sample) {
        const float progress =
            (float)sample /
            (float)SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS;
        const float target_z = start.z_mm +
                               (lifted_z - start.z_mm) * progress;
        servo_zero_leg_angles_t sample_angles;
        servo_zero_leg_point_t sample_foot;
        if (!make_live_knee_arc_pose_at_z(
                plan, leg, target_z, &sample_angles, &sample_foot) ||
            fabsf(sample_angles.knee_rad - previous_knee) >
                SERVO_ZERO_QUAD_DEMO_MAX_KNEE_STEP_RAD) {
            return false;
        }
        if (sample_foot.z_mm < previous_z -
                                  SERVO_ZERO_QUAD_DEMO_EPSILON) {
            return false;
        }
        previous_knee = sample_angles.knee_rad;
        previous_z = sample_foot.z_mm;
    }
    return actual_foot_in_workspace(*actual_foot);
}

static bool live_lift_pose_from_plan(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_angles_t *angles, servo_zero_leg_point_t *actual_foot)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        angles == NULL || actual_foot == NULL) {
        return false;
    }
    const uint8_t bit = (uint8_t)(1u << leg);
    if ((plan->live_cache_valid_mask & bit) != 0u) {
        *angles = plan->live_lifted_angles[leg];
        *actual_foot = plan->live_lifted_foot[leg];
        return true;
    }
    return make_live_lift_leg_pose(plan, leg, angles, actual_foot);
}

/* The live SWING curve is an explicit horizontal foot-space segment.  Sample
 * the complete segment so an endpoint cannot hide an IK branch jump or pulse
 * violation in the middle. */
static bool live_swing_path_is_continuous(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_angles_t lifted_angles,
    servo_zero_leg_point_t lifted_foot,
    servo_zero_leg_point_t target_foot,
    servo_zero_leg_angles_t target_angles)
{
    servo_zero_leg_angles_t previous = lifted_angles;
    float previous_x = lifted_foot.x_mm;
    for (uint32_t sample = 1u;
         sample <= SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS; ++sample) {
        const float ratio =
            (float)sample /
            (float)SERVO_ZERO_QUAD_DEMO_LIVE_BRANCH_SCAN_STEPS;
        servo_zero_leg_angles_t candidate;
        servo_zero_leg_point_t sample_foot;
        if (!make_exact_leg_pose(
                plan, leg,
                (servo_zero_leg_point_t){
                    .x_mm = lifted_foot.x_mm +
                            (target_foot.x_mm - lifted_foot.x_mm) * ratio,
                    .z_mm = lifted_foot.z_mm},
                &candidate, &sample_foot) ||
            sample_foot.x_mm < previous_x - SERVO_ZERO_QUAD_DEMO_EPSILON ||
            fabsf(candidate.knee_rad - previous.knee_rad) >
                SERVO_ZERO_QUAD_DEMO_MAX_KNEE_STEP_RAD) {
            return false;
        }
        previous_x = sample_foot.x_mm;
        previous = candidate;
    }
    return knee_branch_matches(target_angles.knee_rad,
                               plan->leg[leg].knee_branch) &&
           fabsf(target_angles.knee_rad - previous.knee_rad) <=
               SERVO_ZERO_QUAD_DEMO_MAX_KNEE_STEP_RAD;
}

/* LIFT deliberately remains knee-first and therefore follows a known FK arc.
 * Once lifted, SWING is represented directly in foot space: key-hold time
 * advances x at a scaled speed while z remains fixed.  Full IK converts each
 * point. Touchdown removes the temporary x excursion made by knee lift and
 * retains only a deliberately reduced portion of the horizontal swing
 * distance, so the shank returns toward the support attitude instead of
 * landing at the most-forward swing point. */
static bool make_live_swing_leg_pose_at_offset(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg, float offset_mm,
    servo_zero_leg_angles_t *angles, servo_zero_leg_point_t *actual_foot)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        angles == NULL || actual_foot == NULL || !isfinite(offset_mm) ||
        offset_mm < 0.0f) {
        return false;
    }
    servo_zero_leg_point_t lifted_foot;
    servo_zero_leg_angles_t lifted_angles;
    if (!live_lift_pose_from_plan(plan, leg, &lifted_angles, &lifted_foot)) {
        return false;
    }
    const float scale = (float)plan->active_hip_scale_percent / 100.0f;
    const servo_zero_leg_point_t target = {
        .x_mm = lifted_foot.x_mm + offset_mm * scale,
        .z_mm = lifted_foot.z_mm,
    };
    if (!make_exact_leg_pose(plan, leg, target, angles, actual_foot) ||
        !live_swing_path_is_continuous(
            plan, leg, lifted_angles, lifted_foot, target, *angles) ||
        (offset_mm > SERVO_ZERO_QUAD_DEMO_EPSILON &&
         !live_touchdown_path_is_safe(
            plan, leg, *actual_foot, *angles,
            live_touchdown_swing_offset_mm(plan, offset_mm)))) {
        return false;
    }
    return actual_foot->x_mm >=
           lifted_foot.x_mm - SERVO_ZERO_QUAD_DEMO_EPSILON;
}

static bool make_live_swing_leg_pose_nearest(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    float requested_offset_mm, servo_zero_leg_angles_t *angles,
    servo_zero_leg_point_t *actual_foot, float *selected_offset_mm)
{
    if (angles == NULL || actual_foot == NULL || selected_offset_mm == NULL ||
        !isfinite(requested_offset_mm) || requested_offset_mm < 0.0f) {
        return false;
    }
    if (make_live_swing_leg_pose_at_offset(
            plan, leg, requested_offset_mm, angles, actual_foot)) {
        *selected_offset_mm = requested_offset_mm;
        return true;
    }

    /* The live swing is one-way: once its first pulse boundary is crossed,
     * later safe islands must not be considered because that would make a
     * held key jump.  Exponentially bracket that first boundary, then refine
     * it.  Unlike the old fixed 200 mm scan, this has no gameplay-distance
     * cap; the first configured pulse limit is the only stopping condition. */
    servo_zero_leg_angles_t safe_angles;
    servo_zero_leg_point_t safe_foot;
    if (!make_live_swing_leg_pose_at_offset(
            plan, leg, 0.0f, &safe_angles, &safe_foot)) {
        return false;
    }
    float safe_offset = 0.0f;
    float unsafe_offset = requested_offset_mm;
    float probe_offset = 1.0f;
    while (probe_offset < requested_offset_mm) {
        servo_zero_leg_angles_t probe_angles;
        servo_zero_leg_point_t probe_foot;
        if (!make_live_swing_leg_pose_at_offset(
                plan, leg, probe_offset, &probe_angles, &probe_foot)) {
            unsafe_offset = probe_offset;
            break;
        }
        safe_offset = probe_offset;
        safe_angles = probe_angles;
        safe_foot = probe_foot;
        probe_offset = probe_offset >= requested_offset_mm * 0.5f
                           ? requested_offset_mm
                           : probe_offset * 2.0f;
    }
    for (uint32_t iteration = 0u;
         iteration < SERVO_ZERO_QUAD_DEMO_LIVE_OFFSET_BINARY_ITERATIONS;
         ++iteration) {
        const float candidate = safe_offset +
                                (unsafe_offset - safe_offset) * 0.5f;
        servo_zero_leg_angles_t candidate_angles;
        servo_zero_leg_point_t candidate_foot;
        if (make_live_swing_leg_pose_at_offset(
                plan, leg, candidate, &candidate_angles, &candidate_foot)) {
            safe_offset = candidate;
            safe_angles = candidate_angles;
            safe_foot = candidate_foot;
        } else {
            unsafe_offset = candidate;
        }
    }
    *selected_offset_mm = safe_offset;
    *angles = safe_angles;
    *actual_foot = safe_foot;
    return true;
}

static bool support_target_is_safe(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot, float retreat_mm)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        !valid_point(base_foot) || !isfinite(retreat_mm) ||
        retreat_mm < 0.0f) {
        return false;
    }
    const servo_zero_leg_point_t target = {
        .x_mm = base_foot.x_mm - retreat_mm,
        .z_mm = base_foot.z_mm,
    };
    return actual_foot_in_workspace(target) &&
           leg_point_is_safe(plan, leg, target);
}

/* Find the largest prefix-safe body-frame retreat for one support foot.
 * Each support leg applies this capacity independently; a limit on one leg
 * must not stop the other support legs. */
static float support_retreat_capacity(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot, float requested_retreat_mm)
{
    if (!isfinite(requested_retreat_mm) || requested_retreat_mm <= 0.0f) {
        return 0.0f;
    }
    const float workspace_capacity =
        base_foot.x_mm - SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM;
    const float maximum = requested_retreat_mm < workspace_capacity
                              ? requested_retreat_mm
                              : workspace_capacity;
    if (!support_target_is_safe(plan, leg, base_foot, 0.0f) ||
        maximum <= SERVO_ZERO_QUAD_DEMO_EPSILON) {
        return 0.0f;
    }
    float safe = 0.0f;
    float unsafe = maximum;
    bool found_unsafe = false;
    for (float candidate = SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM;
         candidate < maximum;
         candidate += SERVO_ZERO_QUAD_DEMO_SOFT_LIMIT_SCAN_MM) {
        if (!support_target_is_safe(plan, leg, base_foot, candidate)) {
            unsafe = candidate;
            found_unsafe = true;
            break;
        }
        safe = candidate;
    }
    if (!found_unsafe) {
        if (support_target_is_safe(plan, leg, base_foot, maximum)) {
            return maximum;
        }
        unsafe = maximum;
    }
    for (uint32_t iteration = 0u;
         iteration < SERVO_ZERO_QUAD_DEMO_LIVE_OFFSET_BINARY_ITERATIONS;
         ++iteration) {
        const float candidate = safe + (unsafe - safe) * 0.5f;
        if (support_target_is_safe(plan, leg, base_foot, candidate)) {
            safe = candidate;
        } else {
            unsafe = candidate;
        }
    }
    return safe;
}

float servo_zero_quad_demo_support_capacity_mm(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot)
{
    if (plan == NULL || leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        !valid_point(base_foot)) {
        return 0.0f;
    }
    const float workspace_capacity =
        base_foot.x_mm - SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM;
    if (workspace_capacity <= SERVO_ZERO_QUAD_DEMO_EPSILON) {
        return 0.0f;
    }
    return support_retreat_capacity(plan, leg, base_foot,
                                    workspace_capacity);
}

float servo_zero_quad_demo_support_safe_rear_x_mm(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot)
{
    const float full_capacity =
        servo_zero_quad_demo_support_capacity_mm(plan, leg, base_foot);
    const float usable_capacity = full_capacity *
        ((float)SERVO_ZERO_QUAD_DEMO_SUPPORT_RESERVE_MARGIN_PERCENT / 100.0f);
    return base_foot.x_mm - usable_capacity;
}

bool servo_zero_quad_demo_rebase_written_pose(
    const servo_zero_quad_demo_plan_t *plan,
    servo_zero_quad_demo_live_state_t *state,
    const servo_zero_quad_demo_pose_t *written_pose, uint8_t landed_mask)
{
    if (plan == NULL || !plan->valid || state == NULL ||
        written_pose == NULL || !written_pose->valid ||
        (landed_mask & (uint8_t)~0x0fu) != 0u) {
        return false;
    }
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        state->support_base_foot[leg] = written_pose->foot[leg];
        if ((landed_mask & bit) != 0u) {
            const float safe_rear =
                servo_zero_quad_demo_support_safe_rear_x_mm(
                    plan, leg, written_pose->foot[leg]);
            if (!isfinite(safe_rear)) {
                state->support_safe_rear_valid_mask &= (uint8_t)~bit;
                return false;
            }
            state->support_safe_rear_x_mm[leg] = safe_rear;
            state->support_safe_rear_valid_mask |= bit;
        }
    }
    if (state->moving_leg_mask == 0u) {
        state->support_drive_reference_mm = 0.0f;
        state->body_drive_floor_mm = 0.0f;
    } else {
        state->support_drive_reference_mm = written_pose->body_drive_mm;
        state->body_drive_floor_mm = written_pose->body_drive_mm;
    }
    return true;
}

static float support_retreat_reserve(
    const servo_zero_quad_demo_plan_t *plan, uint8_t leg,
    servo_zero_leg_point_t base_foot)
{
    return servo_zero_quad_demo_support_capacity_mm(plan, leg, base_foot) *
           ((float)SERVO_ZERO_QUAD_DEMO_SUPPORT_RESERVE_MARGIN_PERCENT /
            100.0f);
}

/* The public evaluator uses the start-foot IK angles as the zero of each
 * affine map.  Keeping that calculation here avoids making a candidate map
 * silently depend on a hard-coded canonical foot. */
static bool map_pose_pulses(const servo_zero_quad_demo_plan_t *plan,
                            servo_zero_quad_demo_pose_t *pose)
{
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        servo_zero_leg_angles_t start_angles;
        if (!servo_zero_leg_ik(plan->start_foot[leg],
                               plan->leg[leg].knee_branch, &start_angles)) {
            return false;
        }
        const servo_zero_leg_pulse_pose_t pulses =
            servo_zero_leg_map_angles_to_pulses(
                start_angles, pose->angles[leg], &plan->leg[leg].pulse_map);
        if (!pulses.valid) {
            return false;
        }
        pose->pulse_us[plan->leg[leg].hip_channel] = pulses.hip_us;
        pose->pulse_us[plan->leg[leg].knee_channel] = pulses.knee_us;
    }
    if (servo_zero_quad_demo_pulses_within_limits(
            plan, pose->pulse_us, &pose->limit_channel,
            &pose->limit_pulse_us, &pose->limit_min_us,
            &pose->limit_max_us)) {
        return true;
    }
    pose->pulse_limit_violation = true;
    return false;
}

servo_zero_quad_demo_pose_t servo_zero_quad_demo_pose_at(
    uint32_t elapsed_ms, const servo_zero_quad_demo_plan_t *plan)
{
    servo_zero_quad_demo_pose_t pose = {0};
    if (plan == NULL || !plan->valid) {
        return pose;
    }
    make_foot_targets(elapsed_ms, plan, &pose.phase, pose.foot,
                      &pose.soft_limit_clamped,
                      &pose.soft_limit_leg_mask);
    pose.complete = pose.phase == SERVO_ZERO_QUAD_DEMO_PHASE_COMPLETE;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (!servo_zero_leg_ik(pose.foot[leg], plan->leg[leg].knee_branch,
                               &pose.angles[leg])) {
            return pose;
        }
    }
    if (!apply_active_hip_scale(plan, &pose)) {
        return pose;
    }
    if (!map_pose_pulses(plan, &pose)) {
        return pose;
    }
    pose.valid = true;
    return pose;
}

servo_zero_quad_demo_pose_t servo_zero_quad_demo_live_pose(
    const servo_zero_quad_demo_plan_t *plan,
    const servo_zero_quad_demo_live_state_t *state)
{
    servo_zero_quad_demo_pose_t pose = {0};
    if (plan == NULL || !plan->valid || state == NULL ||
        (state->moving_leg_mask & (uint8_t)~0x0fu) != 0u ||
        (state->held_leg_mask & (uint8_t)~0x0fu) != 0u ||
        (state->search_leg_mask & (uint8_t)~0x0fu) != 0u ||
        (state->held_leg_mask & state->search_leg_mask) != 0u ||
        (state->held_leg_mask | state->search_leg_mask) !=
            state->moving_leg_mask ||
        active_leg_count(state->moving_leg_mask) > 2u ||
        !isfinite(state->support_drive_reference_mm) ||
        state->support_drive_reference_mm < 0.0f ||
        !isfinite(state->body_drive_floor_mm) ||
        state->body_drive_floor_mm < 0.0f ||
        (state->support_safe_rear_valid_mask & (uint8_t)~0x0fu) != 0u) {
        return pose;
    }

    bool has_lift = false;
    bool has_swing = false;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (!valid_point(state->support_base_foot[leg]) ||
            !isfinite(state->swing_offset_mm[leg]) ||
            (((state->support_safe_rear_valid_mask &
               (uint8_t)(1u << leg)) != 0u) &&
             !isfinite(state->support_safe_rear_x_mm[leg]))) {
            return pose;
        }
        if ((state->moving_leg_mask & (uint8_t)(1u << leg)) == 0u) {
            continue;
        }
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((state->held_leg_mask & bit) != 0u) {
            if (state->lift_elapsed_ms[leg] < plan->lift_ms) {
                has_lift = true;
            } else {
                has_swing = true;
            }
        }
        if ((state->search_leg_mask & bit) != 0u &&
            !valid_point(state->release_foot[leg])) {
            return pose;
        }
    }

    pose.phase = has_lift
                     ? SERVO_ZERO_QUAD_DEMO_PHASE_SWING_LIFT
                     : (has_swing
                            ? SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD
                            : (state->search_leg_mask != 0u
                                   ? SERVO_ZERO_QUAD_DEMO_PHASE_SEARCH
                                   : SERVO_ZERO_QUAD_DEMO_PHASE_COMPLETE));
    pose.complete = state->moving_leg_mask == 0u;
    pose.support_leg_mask = (uint8_t)(~state->moving_leg_mask & 0x0fu);

    /* Evaluate all non-support legs first.  ARD-1 treats knee lift and SEARCH
     * as vertical/landing motion, not new body drive. Only accepted horizontal
     * swing of a leg that is still held contributes to the active target. */
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((state->moving_leg_mask & bit) == 0u) {
            continue;
        }

        if ((state->held_leg_mask & bit) != 0u) {
            const servo_zero_leg_point_t start = plan->start_foot[leg];
            const float lifted_z = clamp_float(
                start.z_mm + plan->lift_mm,
                SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
            if (state->lift_elapsed_ms[leg] < plan->lift_ms) {
                servo_zero_leg_angles_t start_angles;
                if (!servo_zero_leg_ik(start, plan->leg[leg].knee_branch,
                                       &start_angles)) {
                    return pose;
                }
                const float progress = smoothstep5(
                    (float)state->lift_elapsed_ms[leg] /
                    (float)plan->lift_ms);
                const float target_z = start.z_mm +
                                       (lifted_z - start.z_mm) * progress;
                if (!make_live_knee_arc_pose_at_z(
                        plan, leg, target_z, &pose.angles[leg],
                        &pose.foot[leg])) {
                    return pose;
                }
            } else {
                const float requested_offset = clamp_float(
                    state->swing_offset_mm[leg], 0.0f,
                    plan->leg_step_length_mm[leg]);
                float selected_offset = requested_offset;
                const uint8_t cache_bit = (uint8_t)(1u << leg);
                const bool cached_limit =
                    (plan->live_cache_valid_mask & cache_bit) != 0u;
                if (cached_limit) {
                    selected_offset = requested_offset <
                                              plan->live_swing_limit_offset_mm[leg]
                                          ? requested_offset
                                          : plan->live_swing_limit_offset_mm[leg];
                    if (!make_live_swing_leg_pose_at_offset(
                            plan, leg, selected_offset, &pose.angles[leg],
                            &pose.foot[leg])) {
                        /* A cache is a performance optimization, not a new
                         * safety authority. Re-run the conservative search
                         * if a copied/rebased plan no longer matches it. */
                        if (!make_live_swing_leg_pose_nearest(
                                plan, leg, requested_offset,
                                &pose.angles[leg], &pose.foot[leg],
                                &selected_offset)) {
                            return pose;
                        }
                    }
                } else if (!make_live_swing_leg_pose_nearest(
                               plan, leg, requested_offset,
                               &pose.angles[leg], &pose.foot[leg],
                               &selected_offset)) {
                    return pose;
                }
                pose.accepted_swing_offset_mm[leg] = selected_offset;
                if (fabsf(selected_offset - requested_offset) >
                    SERVO_ZERO_QUAD_DEMO_EPSILON) {
                    pose.soft_limit_clamped = true;
                    pose.soft_limit_leg_mask |= bit;
                } else if (state->swing_offset_mm[leg] >=
                           plan->leg_step_length_mm[leg] -
                               SERVO_ZERO_QUAD_DEMO_EPSILON) {
                    /* Hold time selects one bounded step. Reaching it is a
                     * normal state; only geometric shortening is a limit. */
                    pose.swing_target_reached_mask |= bit;
                }
            }
            continue;
        }

        /* SEARCH removes the temporary x generated by the knee-only lift and
         * keeps only the deliberate horizontal swing offset. Full IK changes
         * both joints while uniform progress limits the largest 120 ms knee
         * increment. */
        /* With only six 20 ms frames in the default 120 ms touchdown,
         * quintic easing concentrates too much knee travel in the middle.
         * Uniform foot-space progress gives smaller, more even commands. */
        const float progress = clamp_float(
            (float)state->search_elapsed_ms[leg] /
                (float)plan->search_ms,
            0.0f, 1.0f);
        if (progress <= SERVO_ZERO_QUAD_DEMO_EPSILON) {
            /* Preserve exact C0 continuity at key-up. Re-solving the same
             * workspace-boundary point through FK/IK can move it by a few
             * floating-point ulps beyond the configured x envelope. */
            pose.angles[leg] = state->release_angles[leg];
            pose.foot[leg] = state->release_foot[leg];
            continue;
        }
        const float target_z = state->release_foot[leg].z_mm +
                               (plan->start_foot[leg].z_mm -
                                state->release_foot[leg].z_mm) * progress;
        servo_zero_leg_angles_t knee_arc_angles;
        servo_zero_leg_point_t knee_arc_foot;
        if (!make_live_knee_arc_pose_at_z(
                plan, leg, target_z, &knee_arc_angles, &knee_arc_foot)) {
            return pose;
        }
        const float touchdown_offset = live_touchdown_swing_offset_mm(
            plan, clamp_float(state->swing_offset_mm[leg], 0.0f,
                              plan->leg_step_length_mm[leg]));
        const float target_x = knee_arc_foot.x_mm + touchdown_offset;
        servo_zero_leg_angles_t start_angles;
        if (!servo_zero_leg_ik(plan->start_foot[leg],
                               plan->leg[leg].knee_branch,
                               &start_angles)) {
            return pose;
        }
        const bool released_before_swing =
            fabsf(state->release_angles[leg].hip_rad -
                  start_angles.hip_rad) <= SERVO_ZERO_QUAD_DEMO_EPSILON;
        if (released_before_swing) {
            /* A short tap has not started the horizontal segment.  Retrace
             * the knee-first lift arc, which is guaranteed to be the same
             * safe joint path in reverse. */
            if (!make_live_knee_arc_pose_at_z(
                    plan, leg, target_z, &pose.angles[leg],
                    &pose.foot[leg])) {
                return pose;
            }
        } else if (!make_exact_leg_pose(
                       plan, leg,
                       (servo_zero_leg_point_t){
                           .x_mm = state->release_foot[leg].x_mm +
                                   (target_x - state->release_foot[leg].x_mm) *
                                       progress,
                           .z_mm = target_z},
                       &pose.angles[leg], &pose.foot[leg])) {
            return pose;
        }
    }

    float active_swing_distance_mm = 0.0f;
    bool have_active_swing = false;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((state->moving_leg_mask & bit) == 0u ||
            (state->held_leg_mask & bit) == 0u) {
            continue;
        }
        const float scale =
            (float)plan->active_hip_scale_percent / 100.0f;
        const float actual_drive_mm =
            pose.accepted_swing_offset_mm[leg] * scale;
        if (!isfinite(actual_drive_mm) || actual_drive_mm < 0.0f) {
            return pose;
        }
        /* A diagonal pair is one body translation event. The shorter held
         * swing is the conservative common progress; one leading leg must
         * not force both supports to pay for motion the other leg has not
         * reached. */
        if (!have_active_swing || actual_drive_mm < active_swing_distance_mm) {
            active_swing_distance_mm = actual_drive_mm;
            have_active_swing = true;
        }
    }
    pose.effective_swing_drive_mm = active_swing_distance_mm;
    float active_swing_target_mm = 0.0f;
    bool have_active_target = false;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((state->moving_leg_mask & bit) != 0u &&
            (state->held_leg_mask & bit) != 0u) {
            const float target = plan->leg_step_length_mm[leg] *
                (float)plan->active_hip_scale_percent / 100.0f;
            if (!have_active_target || target < active_swing_target_mm) {
                active_swing_target_mm = target;
                have_active_target = true;
            }
        }
    }
    float drive_progress = 0.0f;
    if (active_swing_target_mm > SERVO_ZERO_QUAD_DEMO_EPSILON) {
        const float swing_progress = clamp_float(
            active_swing_distance_mm / active_swing_target_mm, 0.0f, 1.0f);
        drive_progress = smoothstep5(clamp_float(
            (swing_progress - 0.20f) / 0.65f, 0.0f, 1.0f));
    }
    const float body_drive_from_swing_mm = active_swing_target_mm *
        ((float)SERVO_ZERO_QUAD_DEMO_DEFAULT_BODY_DRIVE_PERCENT / 100.0f) *
        drive_progress;
    float body_drive_mm = body_drive_from_swing_mm;
    if (state->moving_leg_mask != 0u) {
        if (state->body_drive_floor_mm > body_drive_mm) {
            body_drive_mm = state->body_drive_floor_mm;
        }
        /* A rebased support origin must never be asked to undo a previously
         * committed body translation.  This also makes a stale release frame
         * fail safe by holding, rather than pulling the supports forward. */
        if (state->support_drive_reference_mm > body_drive_mm) {
            body_drive_mm = state->support_drive_reference_mm;
        }
    } else {
        /* A completed state has no support-drive request.  This makes the
         * pure evaluator robust even if a caller has not yet cleared the
         * release latch after the final SEARCH frame. */
        body_drive_mm = 0.0f;
    }
    pose.body_drive_mm = body_drive_mm;
    const float drive_delta_mm =
        body_drive_mm > state->support_drive_reference_mm
            ? body_drive_mm - state->support_drive_reference_mm
            : 0.0f;
    const float requested_support_retreat_mm = drive_delta_mm;
    /* ARD-1: D is a body translation and therefore every support foot gets
     * the same -D request. Each leg still applies its own capacity clip; a
     * saturated support must not stop the other supports. */
    pose.support_retreat_mm = requested_support_retreat_mm;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((pose.support_leg_mask & bit) == 0u) {
            continue;
        }
        const float capacity =
            (state->support_safe_rear_valid_mask & bit) != 0u
                ? fmaxf(0.0f, state->support_base_foot[leg].x_mm -
                                  state->support_safe_rear_x_mm[leg])
                : support_retreat_reserve(
                      plan, leg, state->support_base_foot[leg]);
        const float support_retreat_mm =
            capacity < requested_support_retreat_mm
                ? capacity
                : requested_support_retreat_mm;
        if (support_retreat_mm + SERVO_ZERO_QUAD_DEMO_EPSILON <
            requested_support_retreat_mm) {
            /* Only this support leg is saturated.  Other support legs keep
             * following their own copy of the nominal retreat request. */
            pose.soft_limit_clamped = true;
            pose.soft_limit_leg_mask |= bit;
        }
        pose.foot[leg] = state->support_base_foot[leg];
        pose.foot[leg].x_mm -= support_retreat_mm;
        pose.support_applied_mm[leg] = support_retreat_mm;
        if (!support_target_is_safe(plan, leg,
                                    state->support_base_foot[leg],
                                    support_retreat_mm) ||
            !servo_zero_leg_ik(pose.foot[leg],
                               plan->leg[leg].knee_branch,
                               &pose.angles[leg])) {
            return pose;
        }
    }

    if (!map_pose_pulses(plan, &pose)) {
        return pose;
    }
    pose.valid = true;
    return pose;
}

static bool servo_zero_quad_demo_make_plan_internal(
    const servo_zero_quad_demo_params_t *params,
    servo_zero_quad_demo_plan_t *plan, bool preflight_fixed_path)
{
    if (plan == NULL) {
        return false;
    }
    *plan = (servo_zero_quad_demo_plan_t){0};
    if (params == NULL || params->active_leg >= SERVO_ZERO_QUAD_DEMO_LEG_COUNT ||
        (params->active_leg_mask & (uint8_t)~0x0fu) != 0u ||
        active_leg_count(params->active_leg_mask) > 2u ||
        !valid_pulse_limits(&params->pulse_limits) ||
        !isfinite(params->step_length_mm) ||
        params->step_length_mm <= 0.0f ||
        params->step_length_mm > SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM ||
        !isfinite(params->stance_step_length_mm) ||
        params->stance_step_length_mm <= 0.0f ||
        params->stance_step_length_mm > SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM ||
        !isfinite(params->lift_back_mm) || params->lift_back_mm < 0.0f ||
        params->lift_back_mm > SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM ||
        !isfinite(params->lift_mm) || params->lift_mm <= 0.0f ||
        params->lift_mm > SERVO_ZERO_QUAD_DEMO_MAX_LIFT_MM ||
        params->active_hip_scale_percent > 100u ||
        !valid_duration(params->settle_ms) ||
        !valid_duration(params->lift_ms) ||
        !valid_duration(params->swing_ms) ||
        !valid_duration(params->search_ms)) {
        return false;
    }

    plan->active_leg = params->active_leg;
    plan->active_leg_mask = params->active_leg_mask;
    plan->pulse_limits = params->pulse_limits;
    plan->step_length_mm = params->step_length_mm;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        plan->leg_step_length_mm[leg] = params->step_length_mm;
    }
    plan->stance_step_length_mm = params->stance_step_length_mm;
    plan->lift_back_mm = params->lift_back_mm;
    plan->lift_mm = params->lift_mm;
    plan->active_hip_scale_percent = params->active_hip_scale_percent;
    plan->settle_ms = params->settle_ms;
    plan->lift_ms = params->lift_ms;
    plan->swing_ms = params->swing_ms;
    plan->search_ms = params->search_ms;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if (!valid_point(params->start_foot[leg]) ||
            !valid_map(&params->leg[leg].pulse_map) ||
            (params->leg[leg].knee_branch !=
                 SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD &&
             params->leg[leg].knee_branch !=
                 SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD)) {
            return false;
        }
        plan->start_foot[leg] = servo_zero_quad_demo_limit_start_foot(
            params->start_foot[leg]);
        plan->leg[leg] = params->leg[leg];
    }
    if (!channel_is_unique(plan)) {
        *plan = (servo_zero_quad_demo_plan_t){0};
        return false;
    }
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t hip_channel = plan->leg[leg].hip_channel;
        const uint8_t knee_channel = plan->leg[leg].knee_channel;
        const servo_zero_quad_demo_channel_limit_t *hip_limit =
            &plan->pulse_limits.channel[hip_channel];
        const servo_zero_quad_demo_channel_limit_t *knee_limit =
            &plan->pulse_limits.channel[knee_channel];
        if ((hip_limit->has_min &&
             plan->leg[leg].pulse_map.hip_rest_us <
                 hip_limit->min_us) ||
            (hip_limit->has_max &&
             plan->leg[leg].pulse_map.hip_rest_us >
                 hip_limit->max_us) ||
            (knee_limit->has_min &&
             plan->leg[leg].pulse_map.knee_rest_us <
                 knee_limit->min_us) ||
            (knee_limit->has_max &&
             plan->leg[leg].pulse_map.knee_rest_us >
                 knee_limit->max_us)) {
            *plan = (servo_zero_quad_demo_plan_t){0};
            return false;
        }
    }
    if (!add_duration(plan->settle_ms, plan->lift_ms, &plan->total_ms) ||
        !add_duration(plan->total_ms, plan->swing_ms, &plan->total_ms) ||
        !add_duration(plan->total_ms, plan->search_ms, &plan->total_ms)) {
        *plan = (servo_zero_quad_demo_plan_t){0};
        return false;
    }

    plan->valid = true;
    if (!preflight_fixed_path) {
        /* Live mode pays the expensive path-boundary search once, before
         * outputs are enabled.  The control task then uses the cached lift
         * endpoint and safe offset for every 20 ms frame. */
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (!make_live_lift_leg_pose(
                    plan, leg, &plan->live_lifted_angles[leg],
                    &plan->live_lifted_foot[leg])) {
                *plan = (servo_zero_quad_demo_plan_t){0};
                return false;
            }
            plan->live_cache_valid_mask |= (uint8_t)(1u << leg);
            servo_zero_leg_angles_t ignored_angles;
            servo_zero_leg_point_t ignored_foot;
            float safe_offset = 0.0f;
            if (!make_live_swing_leg_pose_nearest(
                    plan, leg, plan->leg_step_length_mm[leg],
                    &ignored_angles, &ignored_foot, &safe_offset)) {
                *plan = (servo_zero_quad_demo_plan_t){0};
                return false;
            }
            plan->live_swing_limit_offset_mm[leg] = safe_offset;
        }
        return true;
    }
    bool have_previous = false;
    uint16_t previous_pulse[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT] = {0};
    for (uint32_t elapsed_ms = 0u;;) {
        const servo_zero_quad_demo_pose_t pose =
            servo_zero_quad_demo_pose_at(elapsed_ms, plan);
        if (!pose.valid) {
            *plan = (servo_zero_quad_demo_plan_t){0};
            return false;
        }
        if (have_previous) {
            for (uint8_t channel = 0u;
                 channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
                const uint16_t current = pose.pulse_us[channel];
                const uint16_t previous = previous_pulse[channel];
                const uint16_t delta = current >= previous
                                           ? current - previous
                                           : previous - current;
                if (delta > SERVO_ZERO_QUAD_DEMO_MAX_PULSE_STEP_US) {
                    *plan = (servo_zero_quad_demo_plan_t){0};
                    return false;
                }
            }
        }
        for (uint8_t channel = 0u;
             channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
            previous_pulse[channel] = pose.pulse_us[channel];
        }
        have_previous = true;
        if (elapsed_ms >= plan->total_ms) {
            break;
        }
        if (plan->total_ms - elapsed_ms <=
            SERVO_ZERO_QUAD_DEMO_UPDATE_MS) {
            elapsed_ms = plan->total_ms;
        } else {
            elapsed_ms += SERVO_ZERO_QUAD_DEMO_UPDATE_MS;
        }
    }
    return true;
}

bool servo_zero_quad_demo_make_plan(
    const servo_zero_quad_demo_params_t *params,
    servo_zero_quad_demo_plan_t *plan)
{
    return servo_zero_quad_demo_make_plan_internal(params, plan, true);
}

bool servo_zero_quad_demo_make_live_plan(
    const servo_zero_quad_demo_params_t *params,
    servo_zero_quad_demo_plan_t *plan)
{
    return servo_zero_quad_demo_make_plan_internal(params, plan, false);
}
