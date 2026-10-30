#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "motion_strategy.h"

static uint8_t s_pressed;
static qw_motion_frame_t s_frame;

static const uint16_t s_rest_pulses[8] = {
    1494U, 1165U, 1672U, 1748U, 1368U, 1279U, 1764U, 1934U,
};
static const uint8_t s_channel_for_leg[4] = {2U, 0U, 6U, 4U};
static const float s_hip_sign[4] = {-1.0f, 1.0f, -1.0f, 1.0f};
static const float s_knee_sign[4] = {1.0f, -1.0f, 1.0f, -1.0f};
static const uint16_t s_channel_min[8] = {
    1400U, 500U, 500U, 500U, 500U, 500U, 1120U, 500U,
};
static const uint16_t s_channel_max[8] = {
    2500U, 2500U, 1750U, 2500U, 1940U, 2500U, 2500U, 2500U,
};
static uint16_t s_previous_pulses[8];
static bool s_have_previous_pulses;

static void check_formal_pulse_steps(void)
{
    const float link_a = 45.0f;
    const float link_b = 70.0f;
    const float radius_sq = 10000.0f;
    const float cosine = (radius_sq - link_a * link_a - link_b * link_b) /
                        (2.0f * link_a * link_b);
    for (uint8_t leg = 0U; leg < 4U; ++leg) {
        const float rest_knee = acosf(cosine);
        const float rest_hip = atan2f(-100.0f, 0.0f) -
                               atan2f(link_b * sinf(rest_knee),
                                      link_a + link_b * cosf(rest_knee));
        const uint8_t base = s_channel_for_leg[leg];
        const float hip = (float)s_rest_pulses[base] +
                          s_hip_sign[leg] * 836.5f *
                              (s_frame.joints[leg].hip_rad - rest_hip);
        const float knee = (float)s_rest_pulses[base + 1U] +
                           s_knee_sign[leg] * 624.5f *
                               (s_frame.joints[leg].knee_rad - rest_knee);
        const uint16_t pulses[2] = {
            (uint16_t)lroundf(hip), (uint16_t)lroundf(knee),
        };
        assert(pulses[0] >= s_channel_min[base] &&
               pulses[0] <= s_channel_max[base]);
        assert(pulses[1] >= s_channel_min[base + 1U] &&
               pulses[1] <= s_channel_max[base + 1U]);
        if (s_have_previous_pulses) {
            assert(abs((int)pulses[0] - (int)s_previous_pulses[base]) <= 250);
            assert(abs((int)pulses[1] -
                       (int)s_previous_pulses[base + 1U]) <= 250);
        }
        s_previous_pulses[base] = pulses[0];
        s_previous_pulses[base + 1U] = pulses[1];
    }
    s_have_previous_pulses = true;
}

static bool read_pressed(void *ctx, uint8_t leg)
{
    (void)ctx;
    return leg < 4U && (s_pressed & (uint8_t)(1U << leg)) != 0U;
}

static bool read_contact(void *ctx, uint8_t leg)
{
    (void)ctx;
    (void)leg;
    return false;
}

static void emit(const qw_motion_frame_t *frame, void *ctx)
{
    (void)ctx;
    s_frame = *frame;
}

static void tick_and_commit(qw_motion_strategy_t *strategy)
{
    qw_motion_strategy_tick(strategy, 0.02f);
    assert(s_frame.command_mask == 0x0FU);
    assert(s_frame.joint_target_mask == 0x0FU);
    for (uint8_t leg = 0; leg < 4U; leg++) {
        assert(s_frame.joints[leg].valid);
    }
    check_formal_pulse_steps();
    qw_motion_strategy_commit(strategy, true);
}

int main(void)
{
    qw_motion_strategy_t strategy;
    const qw_ard1_config_t config = {
        .lift_duration_ms = 300U,
        .swing_duration_ms = 360U,
        .search_duration_ms = 120U,
        .step_length_mm = 30.0f,
        .lift_z_mm = -60.0f,
        .touchdown_z_mm = -100.0f,
    };
    const qw_motion_init_args_t args = {
        .strategy_config = &config,
        .leg_mask = 0x0FU,
        .read_pressed = read_pressed,
        .read_contact = read_contact,
        .emit = emit,
        .capability_ctx = NULL,
    };
    assert(qw_motion_ard1_create(&strategy));
    assert(qw_motion_strategy_init(&strategy, &args));

    /* A second press remains admissible after the first leg has completed
     * lift and entered swing, as long as the two-leg active budget is kept. */
    s_pressed = 0x01U;
    tick_and_commit(&strategy);
    qw_motion_snapshot_t snapshot;
    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.stepping_leg_mask == 0x01U);
    for (unsigned tick = 0U; tick < 20U; ++tick) {
        tick_and_commit(&strategy);
    }
    s_pressed = 0x03U;
    tick_and_commit(&strategy);
    assert(s_frame.legs[0].mode == LEG_CMD_SWING);
    assert(s_frame.legs[1].mode == LEG_CMD_LIFT);
    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.stepping_leg_mask == 0x03U);
    qw_motion_strategy_reset(&strategy);
    s_have_previous_pulses = false;

    /* The public mask must preserve the RL semantic bit even though ARD-1
     * evaluates its internal core leg in the mapped order. */
    s_pressed = 0x04U;
    tick_and_commit(&strategy);
    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.stepping_leg_mask == 0x04U);
    qw_motion_strategy_reset(&strategy);
    s_have_previous_pulses = false;

    /* Two simultaneous press edges are accepted; a third edge is rejected
     * atomically while releases remain observable. */
    s_pressed = 0x03U;
    tick_and_commit(&strategy);
    assert(s_frame.legs[0].mode == LEG_CMD_LIFT);
    assert(s_frame.legs[1].mode == LEG_CMD_LIFT);
    /* Canonical computer ARD-1 geometry uses the forward IK branch for the
     * rear legs too; mirroring is represented by pulse signs. */
    assert(s_frame.joints[2].knee_rad > 0.0f);
    assert(s_frame.joints[3].knee_rad > 0.0f);
    s_pressed = 0x07U;
    tick_and_commit(&strategy);
    assert(s_frame.legs[2].mode == LEG_CMD_HOLD_SUPPORT);

    /* A rejected actuator write must not advance elapsed phase time. */
    s_pressed = 0x03U;
    qw_motion_strategy_tick(&strategy, 0.02f);
    const qw_motion_frame_t rejected_frame = s_frame;
    qw_motion_strategy_commit(&strategy, false);
    qw_motion_strategy_tick(&strategy, 0.02f);
    assert(fabsf(s_frame.joints[0].knee_rad -
                 rejected_frame.joints[0].knee_rad) < 0.000001f);
    assert(fabsf(s_frame.joints[1].knee_rad -
                 rejected_frame.joints[1].knee_rad) < 0.000001f);
    check_formal_pulse_steps();
    qw_motion_strategy_commit(&strategy, true);

    s_pressed = 0x00U;
    tick_and_commit(&strategy);
    assert(s_frame.legs[0].mode == LEG_CMD_SEARCH);
    assert(s_frame.legs[1].mode == LEG_CMD_SEARCH);

    for (unsigned tick = 0U; tick < 7U; ++tick) {
        tick_and_commit(&strategy);
    }
    assert(s_frame.legs[0].mode == LEG_CMD_HOLD_SUPPORT);
    assert(s_frame.legs[1].mode == LEG_CMD_HOLD_SUPPORT);

    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.strategy_name != NULL);
    assert(snapshot.transition_total >= 4U);

    /* RR must exercise the scaled active-hip path used by the computer
     * evaluator; its CH4 1940 us limit rejects the unscaled IK target. */
    s_pressed = 0x08U;
    qw_motion_strategy_tick(&strategy, 0.02f);
    qw_motion_strategy_commit(&strategy, true);
    for (unsigned tick = 0U; tick < 19U; ++tick) {
        qw_motion_strategy_tick(&strategy, 0.02f);
        assert(s_frame.command_mask == 0x0FU);
        assert(s_frame.joint_target_mask == 0x0FU);
        qw_motion_strategy_commit(&strategy, true);
    }
    assert(s_frame.legs[3].mode == LEG_CMD_SWING);
    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.stepping_leg_mask == 0x08U);
    s_have_previous_pulses = false;
    check_formal_pulse_steps();
    return 0;
}
