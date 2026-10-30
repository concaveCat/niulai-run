#include "servo_zero_quad_demo.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

static const uint16_t k_rest_pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT] = {
    1494u, 1165u, 1672u, 1748u, 1368u, 1279u, 1764u, 1934u,
};

static servo_zero_quad_demo_params_t make_params(uint8_t active_leg)
{
    static const uint8_t hip_channels[SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        0u, 2u, 4u, 6u,
    };
    static const uint8_t knee_channels[SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        1u, 3u, 5u, 7u,
    };
    static const float hip_slopes[SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        836.2f, -836.2f, 836.2f, -836.2f,
    };
    static const float knee_slopes[SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        -624.0f, 624.0f, -624.0f, 624.0f,
    };
    servo_zero_quad_demo_params_t params = {
        .active_leg = active_leg,
        .pulse_limits = {
            .configured = true,
            .channel = {
                [0] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [1] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [2] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [3] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [4] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [5] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [6] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
                [7] = {.has_min = true, .min_us = 300u,
                       .has_max = true, .max_us = 3000u},
            },
        },
        .step_length_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM,
        .stance_step_length_mm =
            SERVO_ZERO_QUAD_DEMO_DEFAULT_STANCE_STEP_LENGTH_MM,
        .lift_back_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_BACK_MM,
        .lift_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MM,
        .active_hip_scale_percent =
            SERVO_ZERO_QUAD_DEMO_FULL_HIP_SCALE_PERCENT,
        .settle_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_SETTLE_MS,
        .lift_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MS,
        .swing_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_SWING_MS,
        .search_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_SEARCH_MS,
    };
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        params.start_foot[leg] = (servo_zero_leg_point_t){
            .x_mm = 0.0f,
            .z_mm = -100.0f,
        };
        params.leg[leg] = (servo_zero_quad_demo_leg_config_t){
            .hip_channel = hip_channels[leg],
            .knee_channel = knee_channels[leg],
            .knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
            .pulse_map = {
                .hip_rest_us = k_rest_pulse_us[hip_channels[leg]],
                .knee_rest_us = k_rest_pulse_us[knee_channels[leg]],
                .hip_us_per_rad = hip_slopes[leg],
                .knee_us_per_rad = knee_slopes[leg],
                .min_us = 300u,
                .max_us = 3000u,
            },
        };
    }
    return params;
}

static servo_zero_quad_demo_pulse_limits_t make_operator_limits(void)
{
    return (servo_zero_quad_demo_pulse_limits_t){
        .configured = true,
        .channel = {
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP] = {
                .has_min = true, .min_us = 1400u},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_KNEE] = {
                .has_min = false, .has_max = false},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP] = {
                .has_max = true, .max_us = 1750u},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_KNEE] = {
                .has_min = false, .has_max = false},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_HIP] = {
                .has_max = true, .max_us = 1940u},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_KNEE] = {
                .has_min = false, .has_max = false},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_HIP] = {
                .has_min = true, .min_us = 1120u},
            [SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_KNEE] = {
                .has_min = false, .has_max = false},
        },
    };
}

static servo_zero_quad_demo_live_state_t make_live_state(
    const servo_zero_quad_demo_plan_t *plan, uint8_t moving_mask)
{
    servo_zero_quad_demo_live_state_t state = {
        .moving_leg_mask = moving_mask,
        .held_leg_mask = moving_mask,
        .search_leg_mask = 0u,
        .support_drive_reference_mm = 0.0f,
    };
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        state.support_base_foot[leg] = plan->start_foot[leg];
    }
    return state;
}

static void assert_pose_pulses_in_range(
    const servo_zero_quad_demo_pose_t *pose)
{
    assert(pose->valid);
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        assert(pose->pulse_us[channel] >= 300u);
        assert(pose->pulse_us[channel] <= 3000u);
    }
}

static void test_all_keyboard_legs_have_a_valid_plan(void)
{
    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        const servo_zero_quad_demo_params_t params = make_params(active_leg);
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));
        assert(plan.valid);

        const servo_zero_quad_demo_pose_t settle =
            servo_zero_quad_demo_pose_at(0u, &plan);
        assert(settle.valid);
        assert(settle.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SETTLE);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            assert(fabsf(settle.foot[leg].x_mm) < 0.001f);
            assert(fabsf(settle.foot[leg].z_mm + 100.0f) < 0.001f);
        }
        assert_pose_pulses_in_range(&settle);

        const uint32_t forward_ms = params.settle_ms + params.lift_ms +
                                    params.swing_ms / 2u;
        const servo_zero_quad_demo_pose_t forward =
            servo_zero_quad_demo_pose_at(forward_ms, &plan);
        assert(forward.valid);
        assert(forward.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD);
        assert(forward.foot[active_leg].x_mm > 0.0f);
        assert(fabsf(forward.foot[active_leg].z_mm -
                    (-100.0f + SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MM)) <
               0.001f);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (leg != active_leg) {
                assert(forward.foot[leg].x_mm < 0.0f);
                assert(fabsf(forward.foot[leg].z_mm + 100.0f) < 0.001f);
            }
        }
        assert_pose_pulses_in_range(&forward);

        const servo_zero_quad_demo_pose_t lift =
            servo_zero_quad_demo_pose_at(params.settle_ms +
                                             params.lift_ms / 2u,
                                         &plan);
        assert(lift.valid);
        assert(lift.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_LIFT);
        assert(lift.foot[active_leg].z_mm > -100.0f);
        assert_pose_pulses_in_range(&lift);

        const servo_zero_quad_demo_pose_t complete =
            servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
        assert(complete.valid);
        assert(complete.complete);
        assert(complete.phase == SERVO_ZERO_QUAD_DEMO_PHASE_COMPLETE);
        assert(complete.foot[active_leg].x_mm <=
               SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM + 0.001f);
        assert(fabsf(complete.foot[active_leg].z_mm + 100.0f) < 0.001f);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (leg != active_leg) {
                assert(fabsf(complete.foot[leg].x_mm +
                            SERVO_ZERO_QUAD_DEMO_DEFAULT_STANCE_STEP_LENGTH_MM) <
                       0.001f);
                assert(fabsf(complete.foot[leg].z_mm + 100.0f) < 0.001f);
            }
        }
        assert_pose_pulses_in_range(&complete);
    }

    servo_zero_quad_demo_params_t generic_pulse_params = make_params(0u);
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        generic_pulse_params.leg[leg].pulse_map.hip_rest_us = 1500u;
        generic_pulse_params.leg[leg].pulse_map.knee_rest_us = 1500u;
    }
    servo_zero_quad_demo_plan_t generic_pulse_plan;
    assert(servo_zero_quad_demo_make_plan(&generic_pulse_params,
                                          &generic_pulse_plan));
}

static void test_two_leg_mask_moves_selected_legs_together(void)
{
    servo_zero_quad_demo_params_t params = make_params(1u);
    params.active_leg_mask = (uint8_t)((1u << SERVO_ZERO_QUAD_DEMO_LEG_LF) |
                                       (1u << SERVO_ZERO_QUAD_DEMO_LEG_RR));
    params.step_length_mm = 8.0f;
    params.stance_step_length_mm = 4.0f;
    params.lift_mm = 4.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    const servo_zero_quad_demo_pose_t complete =
        servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
    assert(complete.valid && complete.complete);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm - 8.0f) <
           0.001f);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm - 8.0f) <
           0.001f);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_RF].x_mm + 4.0f) <
           0.001f);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_LR].x_mm + 4.0f) <
           0.001f);
}

static void test_selected_legs_keep_independent_step_lengths(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.active_leg_mask = (uint8_t)((1u << SERVO_ZERO_QUAD_DEMO_LEG_RF) |
                                       (1u << SERVO_ZERO_QUAD_DEMO_LEG_LR));
    params.step_length_mm = 12.0f;
    params.stance_step_length_mm = 9.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    plan.leg_step_length_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] = 12.0f;
    plan.leg_step_length_mm[SERVO_ZERO_QUAD_DEMO_LEG_LR] = 6.0f;
    const servo_zero_quad_demo_pose_t complete =
        servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
    assert(complete.valid && complete.complete);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_RF].x_mm - 12.0f) <
           0.001f);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_LR].x_mm - 6.0f) <
           0.001f);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm + 9.0f) <
           0.001f);
    assert(fabsf(complete.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm + 9.0f) <
           0.001f);
}

static void test_three_leg_mask_is_rejected(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.active_leg_mask = 0x07u;
    servo_zero_quad_demo_plan_t plan;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));
}

static void test_default_profile_and_limits(void)
{
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM == 30.0f);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_STANCE_STEP_LENGTH_MM == 9.0f);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MM == 40.0f);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_SETTLE_MS == 180u);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MS == 300u);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_SWING_MS == 360u);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_SEARCH_MS == 120u);
    assert(SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT == 50u);
    assert(SERVO_ZERO_QUAD_DEMO_MAX_LIFT_MM == 50.0f);
    assert(SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM == -50.0f);
}

static void assert_lift_height_path_is_reachable(float lift_mm,
                                                 float expected_z_mm,
                                                 bool use_operator_limits)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    if (use_operator_limits) {
        params.pulse_limits = make_operator_limits();
    }
    params.lift_mm = lift_mm;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));

    const uint32_t forward_ms = params.settle_ms + params.lift_ms +
                                params.swing_ms / 2u;
    const servo_zero_quad_demo_pose_t forward =
        servo_zero_quad_demo_pose_at(forward_ms, &plan);
    assert(forward.valid && !forward.complete);
    assert(forward.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD);
    assert(fabsf(forward.foot[0].z_mm - expected_z_mm) < 0.001f);
    assert(forward.foot[0].z_mm <= SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
    assert_pose_pulses_in_range(&forward);

    const servo_zero_quad_demo_pose_t complete =
        servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
    assert(complete.valid && complete.complete);
    assert(fabsf(complete.foot[0].z_mm + 100.0f) < 0.001f);
    assert_pose_pulses_in_range(&complete);
}

static void test_thirty_mm_lift_is_reachable_and_clamped_safely(void)
{
    assert_lift_height_path_is_reachable(30.0f, -70.0f, true);
}

static void test_forty_mm_lift_candidate_is_reachable(void)
{
    assert_lift_height_path_is_reachable(40.0f, -60.0f, true);
}

static void test_half_hip_forty_mm_lift_preserves_clearance(void)
{
    servo_zero_quad_demo_params_t full_params = make_params(0u);
    full_params.lift_mm = 40.0f;
    full_params.active_hip_scale_percent = 100u;

    servo_zero_quad_demo_params_t half_params = full_params;
    half_params.active_hip_scale_percent = 50u;
    servo_zero_quad_demo_params_t operator_half_params = half_params;
    operator_half_params.pulse_limits = make_operator_limits();

    servo_zero_quad_demo_plan_t full_plan;
    servo_zero_quad_demo_plan_t half_plan;
    servo_zero_quad_demo_plan_t operator_half_plan;
    assert(servo_zero_quad_demo_make_plan(&full_params, &full_plan));
    assert(servo_zero_quad_demo_make_plan(&half_params, &half_plan));
    assert(servo_zero_quad_demo_make_plan(&operator_half_params,
                                          &operator_half_plan));

    const uint32_t forward_ms = full_params.settle_ms +
                                full_params.lift_ms +
                                full_params.swing_ms / 2u;
    const servo_zero_quad_demo_pose_t full_pose =
        servo_zero_quad_demo_pose_at(forward_ms, &full_plan);
    const servo_zero_quad_demo_pose_t half_pose =
        servo_zero_quad_demo_pose_at(forward_ms, &half_plan);
    assert(full_pose.valid && half_pose.valid);
    assert(fabsf(half_pose.foot[0].z_mm + 60.0f) < 0.001f);
    assert(half_pose.foot[0].x_mm <=
           SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM + 0.001f);

    servo_zero_leg_angles_t start_angles;
    assert(servo_zero_leg_ik(half_plan.start_foot[0],
                             half_plan.leg[0].knee_branch, &start_angles));
    const float full_hip_excursion =
        fabsf(full_pose.angles[0].hip_rad - start_angles.hip_rad);
    const float half_hip_excursion =
        fabsf(half_pose.angles[0].hip_rad - start_angles.hip_rad);
    assert(full_hip_excursion > 0.001f);
    assert(fabsf(half_hip_excursion - 0.5f * full_hip_excursion) < 0.01f);
    assert(fabsf(half_pose.angles[0].knee_rad -
                 full_pose.angles[0].knee_rad) > 0.001f);

    const servo_zero_quad_demo_pose_t half_complete =
        servo_zero_quad_demo_pose_at(half_plan.total_ms, &half_plan);
    assert(half_complete.valid && half_complete.complete);
    assert(fabsf(half_complete.foot[0].z_mm + 100.0f) < 0.001f);

    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        servo_zero_quad_demo_params_t per_leg = make_params(active_leg);
        per_leg.pulse_limits = make_operator_limits();
        per_leg.lift_mm = 40.0f;
        per_leg.active_hip_scale_percent = 50u;
        servo_zero_quad_demo_plan_t per_leg_plan;
        assert(servo_zero_quad_demo_make_plan(&per_leg, &per_leg_plan));
        const servo_zero_quad_demo_pose_t per_leg_complete =
            servo_zero_quad_demo_pose_at(per_leg_plan.total_ms,
                                         &per_leg_plan);
        assert(per_leg_complete.valid && per_leg_complete.complete);
    }

    servo_zero_quad_demo_params_t diagonal = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_LF);
    diagonal.active_leg_mask =
        (uint8_t)((1u << SERVO_ZERO_QUAD_DEMO_LEG_LF) |
                  (1u << SERVO_ZERO_QUAD_DEMO_LEG_RR));
    diagonal.pulse_limits = make_operator_limits();
    diagonal.lift_mm = 40.0f;
    diagonal.active_hip_scale_percent = 50u;
    servo_zero_quad_demo_plan_t diagonal_plan;
    assert(servo_zero_quad_demo_make_plan(&diagonal, &diagonal_plan));
}

static void test_fifty_mm_lift_envelope_is_configured(void)
{
    const servo_zero_leg_point_t clamped =
        servo_zero_quad_demo_limit_start_foot(
            (servo_zero_leg_point_t){.x_mm = 0.0f, .z_mm = 0.0f});
    assert(SERVO_ZERO_QUAD_DEMO_MAX_LIFT_MM == 50.0f);
    assert(SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM == -50.0f);
    assert(fabsf(clamped.z_mm + 50.0f) < 0.001f);
}

static void test_live_hold_lifts_knee_before_hip(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x01u);
    state.lift_elapsed_ms[0] = params.lift_ms / 2u;
    const servo_zero_quad_demo_pose_t lift =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(lift.valid);
    assert(lift.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_LIFT);

    servo_zero_leg_angles_t start_angles;
    assert(servo_zero_leg_ik(plan.start_foot[0],
                             plan.leg[0].knee_branch, &start_angles));
    assert(fabsf(lift.angles[0].hip_rad - start_angles.hip_rad) < 0.0001f);
    assert(fabsf(lift.angles[0].knee_rad - start_angles.knee_rad) > 0.001f);
    assert(lift.pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP] ==
           plan.leg[0].pulse_map.hip_rest_us);
    assert(lift.foot[0].z_mm > plan.start_foot[0].z_mm);
}

static void test_live_hold_reaches_and_holds_a_bounded_target(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.active_hip_scale_percent = 100u;
    params.step_length_mm = 20.0f;
    params.lift_mm = 40.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x01u);
    state.lift_elapsed_ms[0] = 500u;
    state.swing_offset_mm[0] =
        (float)(state.lift_elapsed_ms[0] - params.lift_ms) *
        params.step_length_mm / (float)params.swing_ms;
    const servo_zero_quad_demo_pose_t after_nominal_swing =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(after_nominal_swing.valid && !after_nominal_swing.complete);
    assert(after_nominal_swing.phase ==
           SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD);

    state.lift_elapsed_ms[0] = 2000u;
    state.swing_offset_mm[0] =
        (float)(state.lift_elapsed_ms[0] - params.lift_ms) *
        params.step_length_mm / (float)params.swing_ms;
    const servo_zero_quad_demo_pose_t long_hold =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(long_hold.valid && !long_hold.complete);
    assert(long_hold.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD);
    assert(long_hold.foot[0].x_mm > after_nominal_swing.foot[0].x_mm);
    assert(((long_hold.swing_target_reached_mask |
             long_hold.soft_limit_leg_mask) & 0x01u) != 0u);
    assert(long_hold.accepted_swing_offset_mm[0] <=
           params.step_length_mm + 0.001f);

    /* Further hold time does not grow this step or consume more support
     * travel. The pose remains latched until key-up. */
    state.lift_elapsed_ms[0] = 6000u;
    const servo_zero_quad_demo_pose_t held_at_limit =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(held_at_limit.valid && !held_at_limit.complete);
    assert(fabsf(held_at_limit.foot[0].x_mm - long_hold.foot[0].x_mm) <
           0.001f);
    assert(fabsf(held_at_limit.body_drive_mm - long_hold.body_drive_mm) <
           0.001f);
}

static void test_live_default_is_knee_dominant(void)
{
    servo_zero_quad_demo_params_t knee_params = make_params(0u);
    knee_params.active_hip_scale_percent =
        SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
    servo_zero_quad_demo_plan_t knee_plan;
    assert(servo_zero_quad_demo_make_plan(&knee_params, &knee_plan));
    servo_zero_quad_demo_live_state_t knee_state =
        make_live_state(&knee_plan, 0x01u);
    knee_state.lift_elapsed_ms[0] = knee_params.lift_ms + 120u;
    knee_state.swing_offset_mm[0] = 10.0f;
    const servo_zero_quad_demo_pose_t knee_pose =
        servo_zero_quad_demo_live_pose(&knee_plan, &knee_state);
    assert(knee_pose.valid);

    servo_zero_quad_demo_params_t full_params = knee_params;
    full_params.active_hip_scale_percent =
        SERVO_ZERO_QUAD_DEMO_FULL_HIP_SCALE_PERCENT;
    servo_zero_quad_demo_plan_t full_plan;
    assert(servo_zero_quad_demo_make_plan(&full_params, &full_plan));
    servo_zero_quad_demo_live_state_t full_state =
        make_live_state(&full_plan, 0x01u);
    full_state.lift_elapsed_ms[0] = full_params.lift_ms + 120u;
    full_state.swing_offset_mm[0] = 10.0f;
    const servo_zero_quad_demo_pose_t full_pose =
        servo_zero_quad_demo_live_pose(&full_plan, &full_state);
    assert(full_pose.valid);

    servo_zero_leg_angles_t knee_start;
    servo_zero_leg_angles_t full_start;
    assert(servo_zero_leg_ik(knee_plan.start_foot[0],
                             knee_plan.leg[0].knee_branch, &knee_start));
    assert(servo_zero_leg_ik(full_plan.start_foot[0],
                             full_plan.leg[0].knee_branch, &full_start));
    const float knee_hip_delta =
        fabsf(knee_pose.angles[0].hip_rad - knee_start.hip_rad);
    const float full_hip_delta =
        fabsf(full_pose.angles[0].hip_rad - full_start.hip_rad);
    assert(knee_hip_delta > 0.0f);
    assert(full_hip_delta > knee_hip_delta * 1.8f);
}

static void test_live_diagonal_support_retreat_is_shared(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_LF);
    params.active_leg_mask =
        (uint8_t)((1u << SERVO_ZERO_QUAD_DEMO_LEG_LF) |
                  (1u << SERVO_ZERO_QUAD_DEMO_LEG_RR));
    params.pulse_limits = make_operator_limits();
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_live_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x06u);
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_LF] =
        params.lift_ms + 120u;
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RR] =
        params.lift_ms + 240u;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_LF] = 10.0f;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RR] = 20.0f;
    const servo_zero_quad_demo_pose_t pose =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(pose.valid && !pose.complete);
    assert(pose.support_leg_mask ==
           (uint8_t)((1u << SERVO_ZERO_QUAD_DEMO_LEG_RF) |
                     (1u << SERVO_ZERO_QUAD_DEMO_LEG_LR)));
    assert(pose.effective_swing_drive_mm > 0.0f);
    assert(fabsf(pose.effective_swing_drive_mm - 10.0f) < 0.001f);
    assert(fabsf(pose.support_retreat_mm - pose.body_drive_mm) <
           0.001f);
    assert(pose.body_drive_mm > 0.0f);
    assert(pose.body_drive_mm <=
           params.step_length_mm *
               (float)params.active_hip_scale_percent / 100.0f *
               (float)SERVO_ZERO_QUAD_DEMO_DEFAULT_BODY_DRIVE_PERCENT /
               100.0f + 0.001f);
    assert(fabsf(pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RF].x_mm +
                 pose.support_retreat_mm) < 0.001f);
    assert(fabsf(pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_LR].x_mm +
                 pose.support_retreat_mm) < 0.001f);
}

static void test_live_single_leg_uses_bounded_body_drive(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_RF);
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x01u);
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RF] =
        params.lift_ms + 120u;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] = 10.0f;
    const servo_zero_quad_demo_pose_t pose =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(pose.valid && !pose.complete);
    assert(pose.support_leg_mask == 0x0eu);
    assert(pose.effective_swing_drive_mm > 0.0f);
    assert(fabsf(pose.support_retreat_mm - pose.body_drive_mm) <
           0.001f);
    assert(pose.body_drive_mm > 0.0f);
    assert(pose.body_drive_mm <=
           params.step_length_mm *
               (float)params.active_hip_scale_percent / 100.0f *
               (float)SERVO_ZERO_QUAD_DEMO_DEFAULT_BODY_DRIVE_PERCENT /
               100.0f + 0.001f);
    assert(pose.effective_swing_drive_mm > 0.0f);
    for (uint8_t leg = SERVO_ZERO_QUAD_DEMO_LEG_LF;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        assert(fabsf(state.support_base_foot[leg].x_mm -
                     pose.foot[leg].x_mm - pose.support_retreat_mm) <
               0.001f);
    }

    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] = 0.0f;
    const servo_zero_quad_demo_pose_t lift =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(lift.valid);
    assert(fabsf(lift.support_retreat_mm) < 0.001f);
    assert(fabsf(lift.effective_swing_drive_mm) < 0.001f);
}

static void test_live_supports_apply_independent_asymmetric_limits(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_RR);
    params.pulse_limits = make_operator_limits();
    /* Make only RF's support capacity zero for this small ARD-1 body-drive
     * request; the other support legs must still receive the common request. */
    params.pulse_limits.channel[SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP]
        .min_us = 1494u;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x04u);
    state.support_drive_reference_mm = 0.0f;
    state.support_safe_rear_valid_mask = 0x0fu;
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        state.support_safe_rear_x_mm[leg] =
            state.support_base_foot[leg].x_mm - 100.0f;
    }
    state.support_safe_rear_x_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] =
        state.support_base_foot[SERVO_ZERO_QUAD_DEMO_LEG_RF].x_mm;
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RR] =
        params.lift_ms + 120u;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RR] = 10.0f;
    const servo_zero_quad_demo_pose_t pose =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(pose.valid && pose.soft_limit_clamped);
    assert(pose.support_leg_mask == 0x0bu);
    assert((pose.soft_limit_leg_mask & pose.support_leg_mask) != 0u);
    bool saw_limited_support = false;
    bool saw_unlimited_support = false;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((pose.support_leg_mask & (uint8_t)(1u << leg)) != 0u) {
            const uint8_t bit = (uint8_t)(1u << leg);
            const float support_retreat =
                state.support_base_foot[leg].x_mm - pose.foot[leg].x_mm;
            assert(fabsf(pose.support_applied_mm[leg] - support_retreat) <
                   0.001f);
            assert(support_retreat >= -0.001f);
            assert(support_retreat <= pose.support_retreat_mm + 0.001f);
            if ((pose.soft_limit_leg_mask & bit) != 0u) {
                assert(support_retreat <
                       pose.support_retreat_mm - 0.001f);
                saw_limited_support = true;
            } else {
                assert(fabsf(support_retreat -
                             pose.support_retreat_mm) < 0.001f);
                saw_unlimited_support = true;
            }
        }
    }
    assert(saw_limited_support);
    assert(saw_unlimited_support);
}

static void test_live_preflight_is_separate_from_fixed_diagnostic_path(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_LF);
    params.active_leg_mask = (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_LF);
    params.pulse_limits = make_operator_limits();
    /* RF is a support leg here.  Its fixed diagnostic retreat would require
     * a pulse below the current safe boundary, while the live initial/lift
     * snapshot at x=0 is safe and can simply allocate zero RF retreat.  The
     * short fixed swing also deliberately violates the fixed-path frame-step
     * gate; it must not reject construction of the live plan. */
    params.pulse_limits.channel[SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP]
        .min_us = 1494u;
    params.active_hip_scale_percent =
        SERVO_ZERO_QUAD_DEMO_FULL_HIP_SCALE_PERCENT;
    params.step_length_mm = SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM;
    params.lift_ms = SERVO_ZERO_QUAD_DEMO_UPDATE_MS;
    params.swing_ms = 360u;

    servo_zero_quad_demo_plan_t fixed_plan;
    servo_zero_quad_demo_plan_t live_plan;
    assert(!servo_zero_quad_demo_make_plan(&params, &fixed_plan));
    assert(servo_zero_quad_demo_make_live_plan(&params, &live_plan));
}

static void test_live_knee_branch_and_forward_progress_are_continuous(void)
{
    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        servo_zero_quad_demo_params_t params = make_params(active_leg);
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));
        servo_zero_quad_demo_live_state_t state = make_live_state(
            &plan, (uint8_t)(1u << active_leg));
        state.lift_elapsed_ms[active_leg] = params.lift_ms;
        float previous_x = -INFINITY;
        for (uint32_t offset = 0u; offset <= 160u; offset += 4u) {
            state.swing_offset_mm[active_leg] = (float)offset;
            const servo_zero_quad_demo_pose_t pose =
                servo_zero_quad_demo_live_pose(&plan, &state);
            assert(pose.valid);
            assert(pose.angles[active_leg].knee_rad > 0.0f);
            assert(pose.foot[active_leg].x_mm >=
                   previous_x - 0.001f);
            previous_x = pose.foot[active_leg].x_mm;
        }
    }
}

static void test_live_rapid_four_leg_sequence_remains_reachable(void)
{
    servo_zero_leg_point_t foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint16_t pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT] = {
        1494u, 1165u, 1672u, 1748u, 1368u, 1279u, 1764u, 1934u,
    };
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        foot[leg] = (servo_zero_leg_point_t){.x_mm = 0.0f, .z_mm = -100.0f};
    }

    /* Four complete rotations exercise cumulative support reserve.  The
     * bounded body-drive ratio should keep the common request reachable
     * without shortening the regression horizon. */
    for (uint8_t step = 0u; step < 64u; ++step) {
        const uint8_t active_leg =
            (uint8_t)(step % SERVO_ZERO_QUAD_DEMO_LEG_COUNT);
        servo_zero_quad_demo_params_t params = make_params(active_leg);
        params.pulse_limits = make_operator_limits();
        params.active_hip_scale_percent =
            SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            params.start_foot[leg] = foot[leg];
            params.leg[leg].pulse_map.hip_rest_us =
                pulses[params.leg[leg].hip_channel];
            params.leg[leg].pulse_map.knee_rest_us =
                pulses[params.leg[leg].knee_channel];
        }
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_live_plan(&params, &plan));

        servo_zero_quad_demo_live_state_t held = make_live_state(
            &plan, (uint8_t)(1u << active_leg));
        held.lift_elapsed_ms[active_leg] =
            params.lift_ms + params.swing_ms;
        held.swing_offset_mm[active_leg] = params.step_length_mm;
        const servo_zero_quad_demo_pose_t swing =
            servo_zero_quad_demo_live_pose(&plan, &held);
        assert(swing.valid);
        assert(swing.support_leg_mask ==
               (uint8_t)(0x0fu & ~(1u << active_leg)));

        servo_zero_quad_demo_live_state_t search = held;
        search.held_leg_mask = 0u;
        search.search_leg_mask = (uint8_t)(1u << active_leg);
        search.release_foot[active_leg] = swing.foot[active_leg];
        search.release_angles[active_leg] = swing.angles[active_leg];
        search.body_drive_floor_mm = swing.body_drive_mm;
        search.search_elapsed_ms[active_leg] = params.search_ms;
        const servo_zero_quad_demo_pose_t landing =
            servo_zero_quad_demo_live_pose(&plan, &search);
        assert(landing.valid);
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            foot[leg] = landing.foot[leg];
            pulses[params.leg[leg].hip_channel] =
                landing.pulse_us[params.leg[leg].hip_channel];
            pulses[params.leg[leg].knee_channel] =
                landing.pulse_us[params.leg[leg].knee_channel];
        }
    }
}

static void test_live_curve_is_explicit_and_continuous_from_retreated_start(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_RR);
    params.start_foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm = -34.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_live_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t held = make_live_state(&plan, 0x04u);
    float previous_z = plan.start_foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm;
    float previous_knee = -INFINITY;
    float lift_hip = 0.0f;
    for (uint32_t elapsed = 0u; elapsed <= params.lift_ms; elapsed += 10u) {
        held.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RR] = elapsed;
        const servo_zero_quad_demo_pose_t pose =
            servo_zero_quad_demo_live_pose(&plan, &held);
        assert(pose.valid);
        if (elapsed == 0u) {
            lift_hip = pose.angles[SERVO_ZERO_QUAD_DEMO_LEG_RR].hip_rad;
        }
        assert(fabsf(pose.angles[SERVO_ZERO_QUAD_DEMO_LEG_RR].hip_rad -
                     lift_hip) < 0.0001f);
        assert(pose.angles[SERVO_ZERO_QUAD_DEMO_LEG_RR].knee_rad >=
               previous_knee - 0.0001f);
        assert(pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm >=
               previous_z - 0.0001f);
        previous_knee = pose.angles[SERVO_ZERO_QUAD_DEMO_LEG_RR].knee_rad;
        previous_z = pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm;
    }

    held.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RR] = params.lift_ms;
    held.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RR] = 8.0f;
    const servo_zero_quad_demo_pose_t swing =
        servo_zero_quad_demo_live_pose(&plan, &held);
    assert(swing.valid);
    assert(fabsf(swing.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm + 60.0f) <
           0.01f);

    servo_zero_quad_demo_live_state_t search = held;
    search.held_leg_mask = 0u;
    search.search_leg_mask = 0x04u;
    search.release_foot[SERVO_ZERO_QUAD_DEMO_LEG_RR] =
        swing.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR];
    search.release_angles[SERVO_ZERO_QUAD_DEMO_LEG_RR] =
        swing.angles[SERVO_ZERO_QUAD_DEMO_LEG_RR];
    search.body_drive_floor_mm = swing.body_drive_mm;
    previous_z = search.release_foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm;
    float previous_x = search.release_foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm;
    for (uint32_t elapsed = 0u; elapsed <= params.search_ms; elapsed += 10u) {
        search.search_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RR] = elapsed;
        const servo_zero_quad_demo_pose_t pose =
            servo_zero_quad_demo_live_pose(&plan, &search);
        assert(pose.valid);
        assert(pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm <=
               previous_x + 0.0001f);
        assert(pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm <=
               previous_z + 0.0001f);
        previous_z = pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].z_mm;
        previous_x = pose.foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm;
    }
    const float expected_touchdown_offset =
        8.0f * (float)SERVO_ZERO_QUAD_DEMO_TOUCHDOWN_SWING_RETENTION_PERCENT /
        100.0f;
    assert(fabsf(previous_x -
                 (plan.start_foot[SERVO_ZERO_QUAD_DEMO_LEG_RR].x_mm +
                  expected_touchdown_offset)) < 0.002f);
}

static void test_live_hold_clamps_at_target_or_geometric_limit(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.active_hip_scale_percent = 1u;
    params.lift_mm = 30.0f;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        params.leg[leg].pulse_map.hip_us_per_rad = 1.0f;
        params.leg[leg].pulse_map.knee_us_per_rad = 1.0f;
    }
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x01u);
    state.lift_elapsed_ms[0] = params.lift_ms + 1u;
    state.swing_offset_mm[0] = 2001.0f;
    const servo_zero_quad_demo_pose_t pose =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(pose.valid && !pose.soft_limit_clamped);
    assert((pose.swing_target_reached_mask & 0x01u) != 0u);
    assert(pose.accepted_swing_offset_mm[0] < 2001.0f);
    assert(pose.foot[0].x_mm <=
           SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM + 0.001f);
}

static void test_live_knee_dominant_path_fits_operator_hip_limits(void)
{
    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        servo_zero_quad_demo_params_t params = make_params(active_leg);
        params.pulse_limits = make_operator_limits();
        params.active_hip_scale_percent =
            SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));

        servo_zero_quad_demo_live_state_t state =
            make_live_state(&plan, (uint8_t)(1u << active_leg));
        state.lift_elapsed_ms[active_leg] = params.lift_ms;
        const servo_zero_quad_demo_pose_t lift =
            servo_zero_quad_demo_live_pose(&plan, &state);
        assert(lift.valid);
        assert(lift.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD);
        assert(lift.pulse_us[params.leg[active_leg].hip_channel] ==
               params.leg[active_leg].pulse_map.hip_rest_us);
        assert(servo_zero_quad_demo_pulses_within_limits(
            &plan, lift.pulse_us, NULL, NULL, NULL, NULL));

        state.lift_elapsed_ms[active_leg] = params.lift_ms + 240u;
        state.swing_offset_mm[active_leg] = 20.0f;
        const servo_zero_quad_demo_pose_t held =
            servo_zero_quad_demo_live_pose(&plan, &state);
        assert(held.valid);
        assert(held.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SWING_FORWARD);
        assert(servo_zero_quad_demo_pulses_within_limits(
            &plan, held.pulse_us, NULL, NULL, NULL, NULL));
    }
}

static void test_default_large_visible_swing_fits_existing_limits(void)
{
    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        servo_zero_quad_demo_params_t params = make_params(active_leg);
        params.pulse_limits = make_operator_limits();
        params.step_length_mm =
            SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM;
        params.active_hip_scale_percent =
            SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_live_plan(&params, &plan));
        assert(plan.live_cache_valid_mask == 0x0fu);
        assert(plan.live_swing_limit_offset_mm[active_leg] >= 0.0f);
        assert(plan.live_swing_limit_offset_mm[active_leg] <=
               params.step_length_mm + 0.001f);
        servo_zero_quad_demo_live_state_t state = make_live_state(
            &plan, (uint8_t)(1u << active_leg));
        state.lift_elapsed_ms[active_leg] = params.lift_ms;
        state.swing_offset_mm[active_leg] = params.step_length_mm;
        const servo_zero_quad_demo_pose_t pose =
            servo_zero_quad_demo_live_pose(&plan, &state);
        assert(pose.valid);
        assert(pose.foot[active_leg].x_mm >
               plan.start_foot[active_leg].x_mm + 7.5f);
    }
}

static void test_live_lf_single_hold_has_independent_forward_swing(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_LF);
    params.active_leg_mask =
        (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_LF);
    params.pulse_limits = make_operator_limits();
    params.active_hip_scale_percent =
        SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
    params.step_length_mm = SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_live_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t state = make_live_state(
        &plan, (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_LF));
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_LF] = params.lift_ms;
    const servo_zero_quad_demo_pose_t lifted =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(lifted.valid);

    float previous_x = lifted.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm;
    uint16_t previous_hip_us =
        lifted.pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP];
    float previous_accepted = 0.0f;
    bool saturated = false;
    for (uint32_t requested_mm = 2u; requested_mm <= 40u;
         requested_mm += 2u) {
        state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_LF] =
            (float)requested_mm;
        const servo_zero_quad_demo_pose_t swing =
            servo_zero_quad_demo_live_pose(&plan, &state);
        assert(swing.valid);
        assert(swing.support_leg_mask == 0x0du);
        const float accepted = swing.accepted_swing_offset_mm[
            SERVO_ZERO_QUAD_DEMO_LEG_LF];
        assert(accepted + 0.001f >= previous_accepted);
        assert(accepted <= (float)requested_mm + 0.001f);
        if (accepted < (float)requested_mm - 0.001f) {
            saturated = true;
            assert((swing.soft_limit_leg_mask &
                    (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_LF)) != 0u);
        }
        if (!saturated) {
            assert(swing.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm >
                   previous_x + 0.1f);
        } else {
            assert(swing.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm <=
                   SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM + 0.001f);
        }
        /* LF is mirrored: physical/model forward swing lowers CH2 us,
         * whereas LF support retreat raises it.  The active path must not
         * accidentally reuse the support direction or remain stationary. */
        if (!saturated) {
            assert(swing.pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP] <
                   previous_hip_us);
        }
        previous_x = swing.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm;
        previous_hip_us =
            swing.pulse_us[SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP];
        previous_accepted = accepted;
    }
}

static void test_live_path_respects_atomic_frame_step_limit(void)
{
    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        servo_zero_quad_demo_params_t params = make_params(active_leg);
        params.pulse_limits = make_operator_limits();
        params.active_hip_scale_percent =
            SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));

        servo_zero_quad_demo_live_state_t state =
            make_live_state(&plan, (uint8_t)(1u << active_leg));
        servo_zero_quad_demo_pose_t previous =
            servo_zero_quad_demo_live_pose(&plan, &state);
        assert(previous.valid);
        for (uint32_t elapsed_ms = SERVO_ZERO_QUAD_DEMO_UPDATE_MS;
             elapsed_ms <= 6000u;
             elapsed_ms += SERVO_ZERO_QUAD_DEMO_UPDATE_MS) {
            state.lift_elapsed_ms[active_leg] = elapsed_ms;
            state.swing_offset_mm[active_leg] =
                elapsed_ms > params.lift_ms
                    ? (float)(elapsed_ms - params.lift_ms) *
                          params.step_length_mm /
                          (float)params.swing_ms
                    : 0.0f;
            const servo_zero_quad_demo_pose_t current =
                servo_zero_quad_demo_live_pose(&plan, &state);
            assert(current.valid);
            for (uint8_t channel = 0u;
                 channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
                const uint16_t delta =
                    current.pulse_us[channel] >= previous.pulse_us[channel]
                        ? current.pulse_us[channel] -
                              previous.pulse_us[channel]
                        : previous.pulse_us[channel] -
                              current.pulse_us[channel];
                assert(delta <= SERVO_ZERO_QUAD_DEMO_MAX_PULSE_STEP_US);
            }
            previous = current;
        }
    }
}

static void test_live_release_removes_lift_x_and_preserves_swing_stride(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.active_hip_scale_percent = 100u;
    params.lift_mm = 40.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));

    servo_zero_quad_demo_live_state_t held = make_live_state(&plan, 0x01u);
    held.lift_elapsed_ms[0] = 600u;
    held.swing_offset_mm[0] =
        (float)(held.lift_elapsed_ms[0] - params.lift_ms) *
        params.step_length_mm / (float)params.swing_ms;
    const servo_zero_quad_demo_pose_t before_release =
        servo_zero_quad_demo_live_pose(&plan, &held);
    assert(before_release.valid);

    servo_zero_quad_demo_live_state_t search = held;
    search.held_leg_mask = 0u;
    search.search_leg_mask = 0x01u;
    search.release_foot[0] = before_release.foot[0];
    search.release_angles[0] = before_release.angles[0];
    search.swing_offset_mm[0] =
        before_release.accepted_swing_offset_mm[0];
    search.body_drive_floor_mm = before_release.body_drive_mm;
    servo_zero_quad_demo_pose_t previous_search = before_release;
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= params.search_ms;
         elapsed_ms += SERVO_ZERO_QUAD_DEMO_UPDATE_MS) {
        search.search_elapsed_ms[0] = elapsed_ms;
        const servo_zero_quad_demo_pose_t sample =
            servo_zero_quad_demo_live_pose(&plan, &search);
        assert(sample.valid);
        for (uint8_t channel = 0u;
             channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
            const uint16_t current = sample.pulse_us[channel];
            const uint16_t previous = previous_search.pulse_us[channel];
            const uint16_t delta = current >= previous
                                       ? current - previous
                                       : previous - current;
            /* The slower fixed landing should remain comfortably below the
             * 250 us hard frame guard, including the steep middle segment. */
            assert(delta <= 125u);
        }
        for (uint8_t leg = 1u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            assert(fabsf(sample.foot[leg].x_mm -
                         before_release.foot[leg].x_mm) < 0.001f);
        }
        previous_search = sample;
    }
    const float expected_touchdown_offset =
        search.swing_offset_mm[0] *
        (float)params.active_hip_scale_percent / 100.0f *
        (float)SERVO_ZERO_QUAD_DEMO_TOUCHDOWN_SWING_RETENTION_PERCENT /
        100.0f;
    const float expected_touchdown_x =
        plan.start_foot[0].x_mm + expected_touchdown_offset;
    assert(fabsf(previous_search.foot[0].x_mm - expected_touchdown_x) <
           0.001f);
    servo_zero_leg_angles_t old_forward_touchdown;
    assert(servo_zero_leg_ik(
        (servo_zero_leg_point_t){
            .x_mm = before_release.foot[0].x_mm,
            .z_mm = plan.start_foot[0].z_mm},
        plan.leg[0].knee_branch, &old_forward_touchdown));
    assert(previous_search.angles[0].knee_rad >
           old_forward_touchdown.knee_rad + 0.1f);
    search.search_elapsed_ms[0] = params.search_ms / 2u;
    const servo_zero_quad_demo_pose_t landing =
        servo_zero_quad_demo_live_pose(&plan, &search);
    assert(landing.valid && !landing.complete);
    assert(landing.phase == SERVO_ZERO_QUAD_DEMO_PHASE_SEARCH);
    servo_zero_leg_angles_t rest_angles;
    servo_zero_leg_angles_t knee_arc_angles;
    servo_zero_leg_point_t knee_arc_foot;
    assert(servo_zero_leg_ik(plan.start_foot[0],
                             plan.leg[0].knee_branch, &rest_angles));
    knee_arc_angles.hip_rad = rest_angles.hip_rad;
    assert(servo_zero_leg_knee_lift_at_height(
        rest_angles, landing.foot[0].z_mm, plan.leg[0].knee_branch,
        &knee_arc_angles.knee_rad));
    assert(servo_zero_leg_fk(knee_arc_angles, &knee_arc_foot));
    const float expected_landing_target_x =
        knee_arc_foot.x_mm + expected_touchdown_offset;
    const float expected_landing_path_x =
        search.release_foot[0].x_mm +
        (expected_landing_target_x - search.release_foot[0].x_mm) * 0.5f;
    assert(fabsf(landing.foot[0].x_mm - expected_landing_path_x) < 0.001f);
    assert(fabsf(expected_touchdown_offset) <
           fabsf(search.swing_offset_mm[0]));
    assert(fabsf(landing.angles[0].hip_rad -
                 before_release.angles[0].hip_rad) > 0.001f);
    assert(fabsf(landing.angles[0].knee_rad -
                 before_release.angles[0].knee_rad) > 0.001f);
    assert(landing.foot[0].z_mm < before_release.foot[0].z_mm);
    assert(fabsf(landing.support_retreat_mm -
                 before_release.support_retreat_mm) < 0.001f);
    for (uint8_t leg = 1u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        assert(fabsf(landing.foot[leg].x_mm -
                     before_release.foot[leg].x_mm) < 0.001f);
    }

    search.moving_leg_mask = 0u;
    search.search_leg_mask = 0u;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        search.support_base_foot[leg] = landing.foot[leg];
    }
    const servo_zero_quad_demo_pose_t complete =
        servo_zero_quad_demo_live_pose(&plan, &search);
    assert(complete.valid && complete.complete);
    assert(complete.phase == SERVO_ZERO_QUAD_DEMO_PHASE_COMPLETE);
}

static void test_invalid_path_is_rejected_before_execution(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    servo_zero_quad_demo_plan_t plan;

    params.leg[1].hip_channel = params.leg[0].hip_channel;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));

    params = make_params(0u);
    params.step_length_mm = SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM + 1.0f;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));

    params = make_params(0u);
    params.start_foot[2].x_mm = NAN;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));

    params = make_params(0u);
    params.pulse_limits.channel[0].max_us =
        params.pulse_limits.channel[0].min_us;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));

    params = make_params(0u);
    params.pulse_limits.channel[0].max_us = 0u;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));

    params = make_params(0u);
    params.pulse_limits.channel[0].has_min = false;
    params.pulse_limits.channel[0].min_us = 1400u;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));

    params = make_params(0u);
    params.pulse_limits.channel[0].has_min = true;
    params.pulse_limits.channel[0].min_us = 299u;
    assert(!servo_zero_quad_demo_make_plan(&params, &plan));
}

static void test_per_channel_pulse_limit_reports_the_offending_output(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    const servo_zero_quad_demo_pose_t pose =
        servo_zero_quad_demo_pose_at(0u, &plan);
    assert(pose.valid);

    uint16_t pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT];
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        pulses[channel] = pose.pulse_us[channel];
    }
    pulses[0] = 3001u;
    uint8_t bad_channel = 0u;
    uint16_t bad_pulse_us = 0u;
    uint16_t bad_min_us = 0u;
    uint16_t bad_max_us = 0u;
    assert(!servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, &bad_channel, &bad_pulse_us, &bad_min_us,
        &bad_max_us));
    assert(bad_channel == 0u);
    assert(bad_pulse_us == 3001u);
    assert(bad_min_us == 300u && bad_max_us == 3000u);
}

static void test_operator_limits_are_independent_and_one_sided(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.pulse_limits = make_operator_limits();

    /* A large equal-step candidate must remain executable: each leg saturates
     * at its own foot/pulse soft limit instead of rejecting the whole frame. */
    params.step_length_mm = 20.0f;
    params.stance_step_length_mm = 6.0f;
    params.lift_mm = 15.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    const servo_zero_quad_demo_pose_t saturated = servo_zero_quad_demo_pose_at(
        params.settle_ms + params.lift_ms + params.swing_ms / 2u, &plan);
    assert(saturated.valid && !saturated.complete);
    assert(saturated.soft_limit_clamped);

    /* A large active step is clamped at the +35 mm foot workspace boundary;
     * the smaller independent stance step keeps the other three legs safe. */
    params.step_length_mm = 40.0f;
    params.stance_step_length_mm = 8.0f;
    params.lift_mm = 4.0f;
    for (uint8_t active_leg = 0u;
         active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++active_leg) {
        params.active_leg = active_leg;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));
        const servo_zero_quad_demo_pose_t pose =
            servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
        assert(pose.valid && pose.complete);
        assert(pose.foot[active_leg].x_mm <=
               SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM + 0.001f);
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if (leg != active_leg) {
                assert(fabsf(pose.foot[leg].x_mm + 8.0f) < 0.001f);
            }
        }
    }
}

static void test_operator_limit_directions_are_not_mirrored(void)
{
    servo_zero_quad_demo_params_t params = make_params(0u);
    params.pulse_limits = make_operator_limits();
    params.step_length_mm = 8.0f;
    params.lift_mm = 4.0f;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    const servo_zero_quad_demo_pose_t pose =
        servo_zero_quad_demo_pose_at(0u, &plan);
    assert(pose.valid);

    uint16_t pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT];
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        pulses[channel] = pose.pulse_us[channel];
    }

    /* Each enabled side rejects only its own forbidden direction. */
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP] = 1399u;
    assert(!servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP] = 2500u;
    assert(servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));

    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP] = 1751u;
    assert(!servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP] = 500u;
    assert(servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));

    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_HIP] = 1941u;
    assert(!servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_HIP] = 500u;
    assert(servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));

    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_HIP] = 1119u;
    assert(!servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_HIP] = 2500u;
    assert(servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));

    /* Knees have no known mechanical stop and must not inherit hip limits. */
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_KNEE] = 500u;
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_KNEE] = 2500u;
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_KNEE] = 500u;
    pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_KNEE] = 2500u;
    assert(servo_zero_quad_demo_pulses_within_limits(
        &plan, pulses, NULL, NULL, NULL, NULL));
}

static void test_limit_saturation_can_continue(void)
{
    servo_zero_leg_point_t foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint16_t pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT];
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        foot[leg] = (servo_zero_leg_point_t){.x_mm = 0.0f, .z_mm = -100.0f};
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        pulses[channel] = k_rest_pulse_us[channel];
    }

    uint8_t completed_steps = 0u;
    for (uint8_t step = 0u; step < 32u; ++step) {
        servo_zero_quad_demo_params_t params = make_params(
            step % SERVO_ZERO_QUAD_DEMO_LEG_COUNT);
        params.pulse_limits = make_operator_limits();
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            params.start_foot[leg] = foot[leg];
            params.leg[leg].pulse_map.hip_rest_us =
                pulses[params.leg[leg].hip_channel];
            params.leg[leg].pulse_map.knee_rest_us =
                pulses[params.leg[leg].knee_channel];
        }

        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));

        const servo_zero_quad_demo_pose_t complete =
            servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
        assert(complete.valid && complete.complete);
        assert(servo_zero_quad_demo_pulses_within_limits(
            &plan, complete.pulse_us, NULL, NULL, NULL, NULL));
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            foot[leg] = complete.foot[leg];
            pulses[params.leg[leg].hip_channel] =
                complete.pulse_us[params.leg[leg].hip_channel];
            pulses[params.leg[leg].knee_channel] =
                complete.pulse_us[params.leg[leg].knee_channel];
        }
        ++completed_steps;
    }

    assert(completed_steps == 32u);
}

static void test_start_foot_is_clamped_to_demo_workspace(void)
{
    const servo_zero_leg_point_t limited =
        servo_zero_quad_demo_limit_start_foot(
            (servo_zero_leg_point_t){.x_mm = -100.0f, .z_mm = -120.0f});
    assert(fabsf(limited.x_mm - SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM) <
           0.001f);
    assert(fabsf(limited.z_mm - SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM) <
           0.001f);

    servo_zero_quad_demo_params_t params = make_params(0u);
    params.start_foot[0] =
        (servo_zero_leg_point_t){.x_mm = 100.0f, .z_mm = -40.0f};
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_plan(&params, &plan));
    assert(fabsf(plan.start_foot[0].x_mm -
                 SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM) < 0.001f);
    assert(fabsf(plan.start_foot[0].z_mm -
                 SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM) < 0.001f);
}

static void test_next_step_starts_from_the_previous_commanded_pose(void)
{
    servo_zero_quad_demo_plan_t first_plan;
    const servo_zero_quad_demo_params_t first_params = make_params(0u);
    assert(servo_zero_quad_demo_make_plan(&first_params, &first_plan));
    const servo_zero_quad_demo_pose_t first_complete =
        servo_zero_quad_demo_pose_at(first_plan.total_ms, &first_plan);
    assert(first_complete.valid && first_complete.complete);

    servo_zero_quad_demo_params_t next_params = make_params(1u);
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        next_params.start_foot[leg] = first_complete.foot[leg];
        next_params.leg[leg].pulse_map.hip_rest_us =
            first_complete.pulse_us[next_params.leg[leg].hip_channel];
        next_params.leg[leg].pulse_map.knee_rest_us =
            first_complete.pulse_us[next_params.leg[leg].knee_channel];
    }
    servo_zero_quad_demo_plan_t next_plan;
    assert(servo_zero_quad_demo_make_plan(&next_params, &next_plan));
    const servo_zero_quad_demo_pose_t next_start =
        servo_zero_quad_demo_pose_at(0u, &next_plan);
    assert(next_start.valid);
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        assert(next_start.pulse_us[channel] == first_complete.pulse_us[channel]);
    }
}

static void test_repeated_keyboard_steps_respect_workspace_soft_limits(void)
{
    servo_zero_leg_point_t foot[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
    uint16_t pulses[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT] = {
        1494u, 1165u, 1672u, 1748u, 1368u, 1279u, 1764u, 1934u,
    };
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        foot[leg] = (servo_zero_leg_point_t){.x_mm = 0.0f, .z_mm = -100.0f};
    }

    bool saw_rear_limit = false;
    for (uint8_t step = 0u; step < 32u; ++step) {
        servo_zero_quad_demo_params_t params = make_params(
            step % SERVO_ZERO_QUAD_DEMO_LEG_COUNT);
        /* Exercise the workspace clamp independently from the production
         * large-active/small-stance profile. */
        params.step_length_mm = 8.0f;
        params.stance_step_length_mm = 20.0f;
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            params.start_foot[leg] = foot[leg];
            params.leg[leg].pulse_map.hip_rest_us =
                pulses[params.leg[leg].hip_channel];
            params.leg[leg].pulse_map.knee_rest_us =
                pulses[params.leg[leg].knee_channel];
        }
        servo_zero_quad_demo_plan_t plan;
        assert(servo_zero_quad_demo_make_plan(&params, &plan));
        const servo_zero_quad_demo_pose_t complete =
            servo_zero_quad_demo_pose_at(plan.total_ms, &plan);
        assert(complete.valid && complete.complete);
        for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            assert(complete.foot[leg].x_mm >=
                   SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM);
            assert(complete.foot[leg].x_mm <=
                   SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM);
            assert(complete.foot[leg].z_mm >=
                   SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM);
            assert(complete.foot[leg].z_mm <=
                   SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
            if (fabsf(complete.foot[leg].x_mm -
                      SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM) < 0.001f) {
                saw_rear_limit = true;
            }
            foot[leg] = complete.foot[leg];
            pulses[params.leg[leg].hip_channel] =
                complete.pulse_us[params.leg[leg].hip_channel];
            pulses[params.leg[leg].knee_channel] =
                complete.pulse_us[params.leg[leg].knee_channel];
        }
    }

    assert(saw_rear_limit);
}

static void test_fixed_support_boundary_survives_rebase(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_RF);
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_live_plan(&params, &plan));
    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x01u);
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        state.support_safe_rear_x_mm[leg] =
            servo_zero_quad_demo_support_safe_rear_x_mm(
                &plan, leg, state.support_base_foot[leg]);
        state.support_safe_rear_valid_mask |= (uint8_t)(1u << leg);
    }
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RF] =
        params.lift_ms + params.swing_ms;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] =
        params.step_length_mm;
    const servo_zero_quad_demo_pose_t written =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(written.valid);
    const float fixed_rear =
        state.support_safe_rear_x_mm[SERVO_ZERO_QUAD_DEMO_LEG_LF];
    assert(servo_zero_quad_demo_rebase_written_pose(
        &plan, &state, &written, 0u));
    assert(fabsf(state.support_safe_rear_x_mm[
                     SERVO_ZERO_QUAD_DEMO_LEG_LF] - fixed_rear) < 0.0001f);
    assert(fabsf(state.support_base_foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm -
                 written.foot[SERVO_ZERO_QUAD_DEMO_LEG_LF].x_mm) < 0.0001f);
}

static void test_body_drive_uses_middle_s_curve_window(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_RF);
    params.step_length_mm = 8.0f;
    params.active_hip_scale_percent = 100u;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_live_plan(&params, &plan));
    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x01u);
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RF] = params.lift_ms;

    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] = 1.6f;
    const servo_zero_quad_demo_pose_t early =
        servo_zero_quad_demo_live_pose(&plan, &state);
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] = 4.0f;
    const servo_zero_quad_demo_pose_t middle =
        servo_zero_quad_demo_live_pose(&plan, &state);
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RF] = 6.8f;
    const servo_zero_quad_demo_pose_t late =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(early.valid && middle.valid && late.valid);
    assert(fabsf(early.body_drive_mm) < 0.001f);
    assert(middle.body_drive_mm > early.body_drive_mm);
    assert(late.body_drive_mm > middle.body_drive_mm);
    assert(fabsf(late.body_drive_mm -
                 params.step_length_mm) < 0.01f);
}

static void test_partial_landing_rebase_uses_written_pose(void)
{
    servo_zero_quad_demo_params_t params = make_params(
        SERVO_ZERO_QUAD_DEMO_LEG_LF);
    params.active_leg_mask = 0x06u;
    servo_zero_quad_demo_plan_t plan;
    assert(servo_zero_quad_demo_make_live_plan(&params, &plan));
    servo_zero_quad_demo_live_state_t state = make_live_state(&plan, 0x06u);
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_LF] = params.lift_ms + 80u;
    state.lift_elapsed_ms[SERVO_ZERO_QUAD_DEMO_LEG_RR] = params.lift_ms + 80u;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_LF] = 8.0f;
    state.swing_offset_mm[SERVO_ZERO_QUAD_DEMO_LEG_RR] = 8.0f;
    const servo_zero_quad_demo_pose_t written =
        servo_zero_quad_demo_live_pose(&plan, &state);
    assert(written.valid);

    state.moving_leg_mask = (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_RR);
    state.held_leg_mask = state.moving_leg_mask;
    state.search_leg_mask = 0u;
    assert(servo_zero_quad_demo_rebase_written_pose(
        &plan, &state, &written,
        (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_LF)));
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        assert(fabsf(state.support_base_foot[leg].x_mm -
                     written.foot[leg].x_mm) < 0.0001f);
    }
    assert(fabsf(state.support_drive_reference_mm - written.body_drive_mm) <
           0.0001f);
    assert((state.support_safe_rear_valid_mask &
            (uint8_t)(1u << SERVO_ZERO_QUAD_DEMO_LEG_LF)) != 0u);
    assert(servo_zero_quad_demo_live_pose(&plan, &state).valid);
}

int main(void)
{
    test_default_profile_and_limits();
    test_all_keyboard_legs_have_a_valid_plan();
    test_two_leg_mask_moves_selected_legs_together();
    test_selected_legs_keep_independent_step_lengths();
    test_three_leg_mask_is_rejected();
    test_invalid_path_is_rejected_before_execution();
    test_thirty_mm_lift_is_reachable_and_clamped_safely();
    test_forty_mm_lift_candidate_is_reachable();
    test_half_hip_forty_mm_lift_preserves_clearance();
    test_fifty_mm_lift_envelope_is_configured();
    test_live_hold_lifts_knee_before_hip();
    test_live_hold_reaches_and_holds_a_bounded_target();
    test_live_default_is_knee_dominant();
    test_live_diagonal_support_retreat_is_shared();
    test_live_single_leg_uses_bounded_body_drive();
    test_live_supports_apply_independent_asymmetric_limits();
    test_live_preflight_is_separate_from_fixed_diagnostic_path();
    test_live_knee_branch_and_forward_progress_are_continuous();
    test_live_rapid_four_leg_sequence_remains_reachable();
    test_live_curve_is_explicit_and_continuous_from_retreated_start();
    test_live_hold_clamps_at_target_or_geometric_limit();
    test_live_knee_dominant_path_fits_operator_hip_limits();
    test_default_large_visible_swing_fits_existing_limits();
    test_live_lf_single_hold_has_independent_forward_swing();
    test_live_path_respects_atomic_frame_step_limit();
    test_live_release_removes_lift_x_and_preserves_swing_stride();
    test_per_channel_pulse_limit_reports_the_offending_output();
    test_operator_limits_are_independent_and_one_sided();
    test_operator_limit_directions_are_not_mirrored();
    test_limit_saturation_can_continue();
    test_start_foot_is_clamped_to_demo_workspace();
    test_next_step_starts_from_the_previous_commanded_pose();
    test_repeated_keyboard_steps_respect_workspace_soft_limits();
    test_fixed_support_boundary_survives_rebase();
    test_body_drive_uses_middle_s_curve_window();
    test_partial_landing_rebase_uses_written_pose();
    puts("servo_zero_quad_demo tests passed");
    return 0;
}
