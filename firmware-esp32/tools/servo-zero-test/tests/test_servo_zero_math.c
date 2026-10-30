#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "../main/servo_zero_math.h"
#include "../main/servo_zero_leg_math.h"

_Static_assert(SERVO_ZERO_REST_GROUP_MOVE_MS == 500u,
               "rest pose leg segment must remain slow");
_Static_assert(SERVO_ZERO_REST_ENTER_MS == 2000u,
               "rest pose entry window must be 2 seconds");
_Static_assert(SERVO_ZERO_REST_TOTAL_MS == 2400u,
               "rest pose total time must include the 400 ms settle");
_Static_assert(SERVO_ZERO_REST_PSEUDO_TOTAL_MS == 2520u,
               "pseudo-sync V2 timing must be bounded");
_Static_assert(SERVO_ZERO_KNEE_SWING_NOMINAL_DELTA_US == 533u,
               "96 degree nominal knee travel must map to 533 us");
_Static_assert(SERVO_ZERO_KNEE_SWING_MOVE_MS >= 500u,
               "knee swing must retain a bounded bench profile");
_Static_assert(SERVO_ZERO_KNEE_SWING_MOVE_MS %
                   SERVO_ZERO_KNEE_SWING_UPDATE_MS == 0u,
               "knee swing duration must align with the ESP32 update cadence");
_Static_assert(SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS %
                   SERVO_ZERO_CH1_COSINE_SWEEP_UPDATE_MS == 0u,
               "CH1 cosine sweep duration must align with the ESP32 update cadence");

int main(void)
{
    uint8_t frame[5];
    uint8_t active_frame[SERVO_ZERO_ACTIVE_FRAME_BYTES];
    uint8_t active_half_frame[SERVO_ZERO_ACTIVE_HALF_FRAME_BYTES];
    uint8_t active_pair_frame[SERVO_ZERO_ACTIVE_PAIR_FRAME_BYTES];
    uint8_t active_pair_only_frame[SERVO_ZERO_FULL_FRAME_BYTES];
    uint8_t rf_only_frame[SERVO_ZERO_FULL_FRAME_BYTES];
    servo_zero_rf_step_pose_t pose;
    servo_zero_rest_pose_t rest;
    const servo_zero_leg_lift_params_t leg_params = {
        .start = {0.0f, -100.0f},
        .lift_mm = 20.0f,
        .forward_mm = 30.0f,
        .lift_ms = 500u,
        .swing_ms = 650u,
        .lower_ms = 800u,
        .knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
    };

    assert(servo_zero_pulse_to_counts(1500, 50) == 307);
    assert(servo_zero_pulse_to_counts(500, 50) == 102);
    assert(servo_zero_pulse_to_counts(2500, 50) == 512);
    assert(servo_zero_pulse_to_counts(2500, 1526) == 4095);
    assert(servo_zero_prescale(25000000u, 50) == 121);

    servo_zero_make_pulse_frame(0, 307, frame);
    assert(frame[0] == 0x06);
    assert(frame[1] == 0x00 && frame[2] == 0x00);
    assert(frame[3] == 0x33 && frame[4] == 0x01);

    servo_zero_make_full_off_frame(1, frame);
    assert(frame[0] == 0x0a && frame[4] == 0x10);
    servo_zero_make_full_off_frame(15, frame);
    assert(frame[0] == 0x42 && frame[4] == 0x10);

    const uint16_t frame_pulses[SERVO_ZERO_REST_CHANNELS] = {
        1746, 1469, 1427, 1415, 1619, 1582, 1449, 1600,
    };
    servo_zero_make_active_frame(frame_pulses, 50, active_frame);
    assert(active_frame[0] == 0x06);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        const uint16_t counts = servo_zero_pulse_to_counts(frame_pulses[channel], 50);
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        assert(active_frame[offset] == 0x00 &&
               active_frame[offset + 1u] == 0x00 &&
               active_frame[offset + 2u] == (uint8_t)(counts & 0xffu) &&
               active_frame[offset + 3u] == (uint8_t)(counts >> 8));
    }

    servo_zero_make_active_half_frame(frame_pulses, 4u, 50,
                                      active_half_frame);
    assert(active_half_frame[0] == 0x16);
    for (uint8_t index = 0; index < SERVO_ZERO_ACTIVE_HALF_CHANNELS;
         ++index) {
        const uint16_t counts = servo_zero_pulse_to_counts(
            frame_pulses[4u + index], 50);
        const uint8_t offset = (uint8_t)(1u + 4u * index);
        assert(active_half_frame[offset] == 0x00 &&
               active_half_frame[offset + 1u] == 0x00 &&
               active_half_frame[offset + 2u] == (uint8_t)(counts & 0xffu) &&
               active_half_frame[offset + 3u] == (uint8_t)(counts >> 8));
    }

    servo_zero_make_active_pair_frame(frame_pulses[0], frame_pulses[1], 50,
                                      active_pair_frame);
    assert(active_pair_frame[0] == 0x06);
    for (uint8_t channel = 0; channel < 2u; ++channel) {
        const uint16_t counts = servo_zero_pulse_to_counts(frame_pulses[channel], 50);
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        assert(active_pair_frame[offset] == 0x00 &&
               active_pair_frame[offset + 1u] == 0x00 &&
               active_pair_frame[offset + 2u] == (uint8_t)(counts & 0xffu) &&
               active_pair_frame[offset + 3u] == (uint8_t)(counts >> 8));
    }

    servo_zero_make_active_pair_only_frame(frame_pulses[0], frame_pulses[1],
                                           50, active_pair_only_frame);
    assert(active_pair_only_frame[0] == 0x06);
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_FULL_FRAME_CHANNELS; ++channel) {
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        if (channel < 2u) {
            const uint16_t counts =
                servo_zero_pulse_to_counts(frame_pulses[channel], 50);
            assert(active_pair_only_frame[offset] == 0x00 &&
                   active_pair_only_frame[offset + 1u] == 0x00 &&
                   active_pair_only_frame[offset + 2u] ==
                       (uint8_t)(counts & 0xffu) &&
                   active_pair_only_frame[offset + 3u] ==
                       (uint8_t)(counts >> 8));
        } else {
            assert(active_pair_only_frame[offset] == 0x00 &&
                   active_pair_only_frame[offset + 1u] == 0x00 &&
                   active_pair_only_frame[offset + 2u] == 0x00 &&
                   active_pair_only_frame[offset + 3u] == 0x10);
        }
    }

    servo_zero_make_active_with_unused_off_frame(frame_pulses, 50, rf_only_frame);
    assert(rf_only_frame[0] == 0x06);
    for (uint8_t channel = 0; channel < SERVO_ZERO_FULL_FRAME_CHANNELS;
         ++channel) {
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        if (channel < SERVO_ZERO_REST_CHANNELS) {
            const uint16_t pulse = frame_pulses[channel];
            const uint16_t counts = servo_zero_pulse_to_counts(pulse, 50);
            assert(rf_only_frame[offset] == 0x00 &&
                   rf_only_frame[offset + 1u] == 0x00 &&
                   rf_only_frame[offset + 2u] == (uint8_t)(counts & 0xffu) &&
                   rf_only_frame[offset + 3u] == (uint8_t)(counts >> 8));
        } else {
            assert(rf_only_frame[offset] == 0x00 &&
                   rf_only_frame[offset + 1u] == 0x00 &&
                   rf_only_frame[offset + 2u] == 0x00 &&
                   rf_only_frame[offset + 3u] == 0x10);
        }
    }

    assert(servo_zero_s_curve_lerp(1746, 1680, 0, 500) == 1746);
    assert(servo_zero_s_curve_lerp(1746, 1680, 250, 500) == 1713);
    assert(servo_zero_s_curve_lerp(1746, 1680, 500, 500) == 1680);
    assert(servo_zero_s_curve_lerp(1746, 1680, 1, 0) == 1680);
    assert(servo_zero_s_curve_lerp(0, 1000, 250, 1000) == 146);
    assert(servo_zero_minimum_jerk_lerp(0, 1000, 0, 1000) == 0);
    assert(servo_zero_minimum_jerk_lerp(0, 1000, 250, 1000) == 104);
    assert(servo_zero_minimum_jerk_lerp(0, 1000, 500, 1000) == 500);
    assert(servo_zero_minimum_jerk_lerp(0, 1000, 1000, 1000) == 1000);
    assert(servo_zero_minimum_jerk_lerp(0, 1000, 1, 0) == 1000);

    servo_zero_ch1_cosine_sweep_pose_t ch1_sweep =
        servo_zero_ch1_cosine_sweep_pose(0u, 1500u);
    assert(ch1_sweep.valid && ch1_sweep.pulse_us == 1500u);
    ch1_sweep = servo_zero_ch1_cosine_sweep_pose(
        SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS, 1500u);
    assert(ch1_sweep.valid &&
           ch1_sweep.pulse_us == SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US);
    ch1_sweep = servo_zero_ch1_cosine_sweep_pose(
        2u * SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS, 1500u);
    assert(ch1_sweep.valid &&
           ch1_sweep.pulse_us == SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US);
    ch1_sweep = servo_zero_ch1_cosine_sweep_pose(
        3u * SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS, 1500u);
    assert(ch1_sweep.valid &&
           ch1_sweep.pulse_us == SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US);
    ch1_sweep = servo_zero_ch1_cosine_sweep_pose(
        4u * SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS, 1500u);
    assert(ch1_sweep.valid &&
           ch1_sweep.pulse_us == SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US);
    assert(!servo_zero_ch1_cosine_sweep_pose(0u, 999u).valid);
    assert(!servo_zero_ch1_cosine_sweep_pose(0u, 2001u).valid);

    uint16_t previous_ch1_us = 1500u;
    for (uint32_t elapsed_ms = SERVO_ZERO_CH1_COSINE_SWEEP_UPDATE_MS;
         elapsed_ms <= 4u * SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS;
         elapsed_ms += SERVO_ZERO_CH1_COSINE_SWEEP_UPDATE_MS) {
        ch1_sweep = servo_zero_ch1_cosine_sweep_pose(elapsed_ms, 1500u);
        assert(ch1_sweep.valid &&
               ch1_sweep.pulse_us >= SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US &&
               ch1_sweep.pulse_us <= SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US);
        const uint16_t delta =
            ch1_sweep.pulse_us >= previous_ch1_us
                ? ch1_sweep.pulse_us - previous_ch1_us
                : previous_ch1_us - ch1_sweep.pulse_us;
        assert(delta <= 17u);
        previous_ch1_us = ch1_sweep.pulse_us;
    }

    assert(servo_zero_scale_pulse(1746, 1998, 0) == 1746);
    assert(servo_zero_scale_pulse(1746, 1998, 50) == 1872);
    assert(servo_zero_scale_pulse(1746, 1998, 100) == 1998);
    assert(servo_zero_scale_pulse(1427, 1182, 50) == 1304);
    assert(servo_zero_scale_pulse(1746, 1998, 101) == 1998);

    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        const uint16_t upright = servo_zero_rest_upright_us(channel);
        const uint16_t target = servo_zero_rest_target_us(channel);
        uint16_t previous = upright;
        for (uint8_t amplitude = 0; amplitude <= 100; ++amplitude) {
            const uint16_t scaled = servo_zero_rest_target_us_at_amplitude(
                channel, amplitude);
            if (target >= upright) {
                assert(scaled >= previous && scaled <= target);
            } else {
                assert(scaled <= previous && scaled >= target);
            }
            previous = scaled;
        }
    }

    pose = servo_zero_rf_step_pose(0, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
                                   SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US &&
           !pose.complete);
    pose = servo_zero_rf_step_pose(350, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
                                   SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_KNEE_PRELIFT_US);
    pose = servo_zero_rf_step_pose(850, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
                                   SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_HIP_TARGET_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_KNEE_TARGET_US);
    pose = servo_zero_rf_step_pose(1700, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
                                   SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US &&
           !pose.complete);
    pose = servo_zero_rf_step_pose(SERVO_ZERO_RF_STEP_TOTAL_MS,
                                   SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
                                   SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US && pose.complete);

    pose = servo_zero_rf_step_pose_at_amplitude(
        850, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
        SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US, 0);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US);
    pose = servo_zero_rf_step_pose_at_amplitude(
        850, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
        SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US, 50);
    assert(pose.hip_us == 1620 && pose.knee_us == 1317);
    pose = servo_zero_rf_step_pose_at_amplitude(
        850, SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US,
        SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US, 100);
    assert(pose.hip_us == SERVO_ZERO_RF_STEP_HIP_TARGET_US &&
           pose.knee_us == SERVO_ZERO_RF_STEP_KNEE_TARGET_US);

    const uint16_t expected_upright[SERVO_ZERO_REST_CHANNELS] = {
        1746, 1469, 1427, 1415, 1619, 1582, 1449, 1600,
    };
    const uint16_t expected_rest[SERVO_ZERO_REST_CHANNELS] = {
        1494, 1165, 1672, 1748, 1368, 1279, 1764, 1934,
    };
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(servo_zero_rest_upright_us(channel) == expected_upright[channel]);
        assert(servo_zero_rest_target_us(channel) == expected_rest[channel]);
        assert(expected_rest[channel] >= 300 && expected_rest[channel] <= 3000);
    }
    assert(servo_zero_rest_upright_us(SERVO_ZERO_REST_CHANNELS) == 0);
    assert(servo_zero_rest_target_us(SERVO_ZERO_REST_CHANNELS) == 0);

    assert(expected_upright[0] - expected_rest[0] == 252u);
    assert(expected_upright[1] - expected_rest[1] == 304u);
    assert(expected_rest[2] - expected_upright[2] == 245u);
    assert(expected_rest[3] - expected_upright[3] == 333u);

    /* Front knees follow the front hip pulse direction. Rear knees use the
     * opposite pulse direction because the assembled rear legs fold the
     * other way. */
    assert(expected_rest[0] < expected_upright[0] &&
           expected_rest[1] < expected_upright[1]);
    assert(expected_rest[2] > expected_upright[2] &&
           expected_rest[3] > expected_upright[3]);
    assert(expected_rest[4] < expected_upright[4] &&
           expected_rest[5] < expected_upright[5]);
    assert(expected_rest[6] > expected_upright[6] &&
           expected_rest[7] > expected_upright[7]);

    rest = servo_zero_rest_pose(0);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(rest.pulse_us[channel] == expected_upright[channel]);
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pose(SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
                                SERVO_ZERO_REST_GROUP_MOVE_MS / 2u);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        if (channel < 2u) {
            assert(rest.pulse_us[channel] == servo_zero_s_curve_lerp(
                expected_upright[channel], expected_rest[channel],
                SERVO_ZERO_REST_GROUP_MOVE_MS / 2u,
                SERVO_ZERO_REST_GROUP_MOVE_MS));
        } else {
            assert(rest.pulse_us[channel] == expected_upright[channel]);
        }
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pose(SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
                                SERVO_ZERO_REST_GROUP_MOVE_MS);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        if (channel < 2u) {
            assert(rest.pulse_us[channel] == expected_rest[channel]);
        } else {
            assert(rest.pulse_us[channel] == expected_upright[channel]);
        }
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pose(SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
                                2u * SERVO_ZERO_REST_GROUP_MOVE_MS);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        if (channel < 4u) {
            assert(rest.pulse_us[channel] == expected_rest[channel]);
        } else {
            assert(rest.pulse_us[channel] == expected_upright[channel]);
        }
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pose(SERVO_ZERO_REST_TOTAL_MS + 500u);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(rest.pulse_us[channel] == expected_rest[channel]);
    }
    assert(rest.complete);

    rest = servo_zero_rest_pose_at_amplitude(
        SERVO_ZERO_REST_UPRIGHT_SETTLE_MS + SERVO_ZERO_REST_GROUP_MOVE_MS / 2u,
        0);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(rest.pulse_us[channel] == expected_upright[channel]);
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pose_at_amplitude(
        SERVO_ZERO_REST_TOTAL_MS, 50);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(rest.pulse_us[channel] ==
               servo_zero_rest_target_us_at_amplitude(channel, 50));
    }
    assert(rest.complete);

    rest = servo_zero_rest_pseudo_pose_at_amplitude(0, 100);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(rest.pulse_us[channel] == expected_upright[channel]);
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pseudo_pose_at_amplitude(
        SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
            SERVO_ZERO_REST_PSEUDO_PHASE_MS / 2u,
        100);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        if ((SERVO_ZERO_REST_PSEUDO_GROUP_A_MASK & (1u << channel)) != 0u) {
            assert(rest.pulse_us[channel] == servo_zero_minimum_jerk_lerp(
                expected_upright[channel], expected_rest[channel],
                SERVO_ZERO_REST_PSEUDO_PHASE_MS / 2u,
                SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS));
        } else {
            assert(rest.pulse_us[channel] == expected_upright[channel]);
        }
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pseudo_pose_at_amplitude(
        SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
            SERVO_ZERO_REST_PSEUDO_PHASE_MS +
            SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS / 2u,
        100);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        const uint32_t group_elapsed =
            (SERVO_ZERO_REST_PSEUDO_GROUP_B_MASK & (1u << channel)) != 0u
                ? SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS / 2u
                : SERVO_ZERO_REST_PSEUDO_PHASE_MS +
                      SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS / 2u;
        assert(rest.pulse_us[channel] == servo_zero_minimum_jerk_lerp(
            expected_upright[channel], expected_rest[channel], group_elapsed,
            SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS));
    }
    assert(!rest.complete);

    rest = servo_zero_rest_pseudo_pose_at_amplitude(
        SERVO_ZERO_REST_PSEUDO_TOTAL_MS, 100);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(rest.pulse_us[channel] == expected_rest[channel]);
    }
    assert(rest.complete);

    assert(servo_zero_knee_swing_delta_us(0) == 0u);
    assert(servo_zero_knee_swing_delta_us(50) == 267u);
    assert(servo_zero_knee_swing_delta_us(100) == 533u);
    assert(servo_zero_knee_swing_delta_us(101) == 533u);

    servo_zero_knee_swing_pose_t knee_swing =
        servo_zero_knee_swing_pose_at_amplitude(0u, 100u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == expected_upright[channel]);
    }

    knee_swing = servo_zero_knee_swing_pose_at_amplitude(
        SERVO_ZERO_KNEE_SWING_SETTLE_MS, 100u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == expected_upright[channel]);
    }

    /* Current direction probe: CH1 and CH3 are reversed relative to the
     * preceding high-speed demo revision. */
    const uint16_t knee_endpoint_a[SERVO_ZERO_REST_CHANNELS] = {
        expected_upright[0], 936u, expected_upright[2], 882u,
        expected_upright[4], 1049u, expected_upright[6], 1067u,
    };
    const uint16_t knee_endpoint_b[SERVO_ZERO_REST_CHANNELS] = {
        expected_upright[0], 2002u, expected_upright[2], 1948u,
        expected_upright[4], 2115u, expected_upright[6], 2133u,
    };
    knee_swing = servo_zero_knee_swing_pose_at_amplitude(
        SERVO_ZERO_KNEE_SWING_SETTLE_MS + SERVO_ZERO_KNEE_SWING_MOVE_MS,
        100u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == knee_endpoint_a[channel]);
    }

    knee_swing = servo_zero_knee_swing_pose_at_amplitude(
        SERVO_ZERO_KNEE_SWING_SETTLE_MS + 2u *
            SERVO_ZERO_KNEE_SWING_MOVE_MS,
        100u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == knee_endpoint_b[channel]);
    }

    knee_swing = servo_zero_knee_swing_pose_at_amplitude(
        SERVO_ZERO_KNEE_SWING_SETTLE_MS + 3u *
            SERVO_ZERO_KNEE_SWING_MOVE_MS,
        100u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == knee_endpoint_a[channel]);
    }

    knee_swing = servo_zero_knee_swing_pose_at_amplitude(
        SERVO_ZERO_KNEE_SWING_SETTLE_MS + SERVO_ZERO_KNEE_SWING_MOVE_MS +
            SERVO_ZERO_KNEE_SWING_MOVE_MS / 2u,
        100u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == expected_upright[channel]);
    }

    for (uint32_t elapsed_ms = 0u;
         elapsed_ms < SERVO_ZERO_KNEE_SWING_SETTLE_MS +
                           8u * SERVO_ZERO_KNEE_SWING_MOVE_MS;
         elapsed_ms += 20u) {
        knee_swing = servo_zero_knee_swing_pose_at_amplitude(elapsed_ms, 100u);
        assert(knee_swing.valid);
        for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS;
             ++channel) {
            assert(knee_swing.pulse_us[channel] >=
                       SERVO_ZERO_KNEE_SWING_MIN_US &&
                   knee_swing.pulse_us[channel] <=
                       SERVO_ZERO_KNEE_SWING_MAX_US);
            if ((channel % 2u) == 0u) {
                assert(knee_swing.pulse_us[channel] ==
                       expected_upright[channel]);
            }
        }
    }

    knee_swing = servo_zero_knee_swing_pose_at_amplitude(
        SERVO_ZERO_KNEE_SWING_SETTLE_MS + 4u *
            SERVO_ZERO_KNEE_SWING_MOVE_MS,
        0u);
    assert(knee_swing.valid);
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        assert(knee_swing.pulse_us[channel] == expected_upright[channel]);
    }

    servo_zero_leg_angles_t leg_angles;
    servo_zero_leg_point_t leg_foot;
    assert(servo_zero_leg_fk((servo_zero_leg_angles_t){
                                 .hip_rad = -1.57079632679f,
                                 .knee_rad = 0.0f,
                             },
                             &leg_foot));
    assert(fabsf(leg_foot.x_mm) < 0.01f);
    assert(fabsf(leg_foot.z_mm + 115.0f) < 0.01f);

    const servo_zero_leg_point_t round_trip_point = {0.0f, -100.0f};
    assert(servo_zero_leg_ik(round_trip_point,
                             SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &leg_angles));
    assert(servo_zero_leg_fk(leg_angles, &leg_foot));
    assert(fabsf(leg_foot.x_mm - round_trip_point.x_mm) < 0.01f);
    assert(fabsf(leg_foot.z_mm - round_trip_point.z_mm) < 0.01f);
    assert(servo_zero_leg_ik(round_trip_point,
                             SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD,
                             &leg_angles));
    assert(servo_zero_leg_fk(leg_angles, &leg_foot));
    assert(fabsf(leg_foot.x_mm - round_trip_point.x_mm) < 0.01f);
    assert(fabsf(leg_foot.z_mm - round_trip_point.z_mm) < 0.01f);
    assert(!servo_zero_leg_ik((servo_zero_leg_point_t){0.0f, -120.0f},
                              SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
                              &leg_angles));
    assert(!servo_zero_leg_ik((servo_zero_leg_point_t){0.0f, -20.0f},
                              SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
                              &leg_angles));

    /* Fixed-hip height solving must honor the requested relative-knee
     * branch, rather than choosing whichever candidate is nearest. */
    float knee_at_height;
    const float branch_test_hip = 1.5f;
    const float branch_test_height =
        SERVO_ZERO_LEG_THIGH_MM * sinf(branch_test_hip) +
        SERVO_ZERO_LEG_SHANK_MM * 0.8f;
    assert(servo_zero_leg_knee_at_height(
        branch_test_hip, branch_test_height, 0.0f,
        SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &knee_at_height));
    assert(knee_at_height > 0.0f);
    const float forward_knee_at_height = knee_at_height;
    assert(servo_zero_leg_knee_at_height(
        branch_test_hip, branch_test_height, 0.0f,
        SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD, &knee_at_height));
    assert(knee_at_height < 0.0f);
    assert(fabsf(forward_knee_at_height - knee_at_height) > 0.1f);

    /* Regression: with a nearly vertical shank there are two positive-knee
     * roots for the same higher z.  A lift must choose the increasing root;
     * nearest-root selection used to fold the knee backward mid-lift. */
    servo_zero_leg_angles_t retreated_rest;
    assert(servo_zero_leg_ik(
        (servo_zero_leg_point_t){.x_mm = -34.0f, .z_mm = -100.0f},
        SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &retreated_rest));
    float previous_lift_knee = retreated_rest.knee_rad;
    for (uint32_t sample = 1u; sample <= 32u; ++sample) {
        const float target_z = -100.0f + 40.0f * (float)sample / 32.0f;
        float lift_knee;
        assert(servo_zero_leg_knee_lift_at_height(
            retreated_rest, target_z,
            SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &lift_knee));
        assert(lift_knee >= previous_lift_knee - 0.00001f);
        previous_lift_knee = lift_knee;
    }
    servo_zero_leg_point_t retreated_lift_foot;
    assert(servo_zero_leg_fk(
        (servo_zero_leg_angles_t){.hip_rad = retreated_rest.hip_rad,
                                  .knee_rad = previous_lift_knee},
        &retreated_lift_foot));
    assert(fabsf(retreated_lift_foot.z_mm + 60.0f) < 0.01f);
    assert(retreated_lift_foot.x_mm > 29.0f);

    servo_zero_leg_pose_t leg_pose = servo_zero_leg_lift_pose(
        0u, 100u, &leg_params);
    assert(leg_pose.valid && !leg_pose.complete);
    assert(fabsf(leg_pose.foot.x_mm) < 0.01f);
    assert(fabsf(leg_pose.foot.z_mm + 100.0f) < 0.01f);

    leg_pose = servo_zero_leg_lift_pose(500u, 100u, &leg_params);
    assert(leg_pose.valid && !leg_pose.complete);
    assert(fabsf(leg_pose.foot.x_mm) < 0.01f);
    assert(fabsf(leg_pose.foot.z_mm + 80.0f) < 0.01f);

    leg_pose = servo_zero_leg_lift_pose(825u, 100u, &leg_params);
    assert(leg_pose.valid && !leg_pose.complete);
    assert(fabsf(leg_pose.foot.x_mm - 15.0f) < 0.01f);
    assert(fabsf(leg_pose.foot.z_mm + 80.0f) < 0.01f);

    leg_pose = servo_zero_leg_lift_pose(1950u, 100u, &leg_params);
    assert(leg_pose.valid && leg_pose.complete);
    assert(fabsf(leg_pose.foot.x_mm - 30.0f) < 0.01f);
    assert(fabsf(leg_pose.foot.z_mm + 100.0f) < 0.01f);

    leg_pose = servo_zero_leg_lift_pose(1950u, 50u, &leg_params);
    assert(leg_pose.valid && leg_pose.complete);
    assert(fabsf(leg_pose.foot.x_mm - 15.0f) < 0.01f);
    assert(fabsf(leg_pose.foot.z_mm + 100.0f) < 0.01f);

    for (uint8_t amplitude = 0u; amplitude <= 100u; ++amplitude) {
        for (uint32_t elapsed_ms = 0u; elapsed_ms <= 1950u;
             elapsed_ms += 20u) {
            leg_pose = servo_zero_leg_lift_pose(elapsed_ms, amplitude,
                                                &leg_params);
            assert(leg_pose.valid);
        }
    }

    servo_zero_leg_lift_params_t backward_leg_params = leg_params;
    backward_leg_params.knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD;
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= 1950u;
         elapsed_ms += 20u) {
        leg_pose = servo_zero_leg_lift_pose(elapsed_ms, 100u,
                                            &backward_leg_params);
        assert(leg_pose.valid);
    }

    servo_zero_leg_pose_t previous_leg_pose = {0};
    bool have_previous_leg_pose = false;
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= 1950u;
         elapsed_ms += 20u) {
        leg_pose = servo_zero_leg_lift_pose(elapsed_ms, 100u, &leg_params);
        assert(leg_pose.valid);
        if (have_previous_leg_pose) {
            assert(fabsf(leg_pose.angles.hip_rad -
                         previous_leg_pose.angles.hip_rad) < 0.05f);
            assert(fabsf(leg_pose.angles.knee_rad -
                         previous_leg_pose.angles.knee_rad) < 0.05f);
        }
        previous_leg_pose = leg_pose;
        have_previous_leg_pose = true;
    }

    /* Joint-ordered single-leg plan: CH1 lifts first, CH0 swings forward,
     * then IK moves both joints while the foot descends vertically. */
    servo_zero_leg_angles_t single_rest_angles;
    assert(servo_zero_leg_ik(round_trip_point,
                             SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
                             &single_rest_angles));
    const servo_zero_leg_single_params_t single_params = {
        .rest_angles = single_rest_angles,
        .lift_clearance_mm = 20.0f,
        .hip_swing_rad = 0.3490658504f, /* 20 degrees */
        .rest_ms = 400u,
        .knee_lift_ms = 500u,
        .hip_swing_ms = 650u,
        .touchdown_ms = 1000u,
        .knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
    };
    servo_zero_leg_single_plan_t single_plan;
    assert(servo_zero_leg_make_single_plan(&single_params, &single_plan));
    assert(single_plan.valid);
    assert(fabsf(single_plan.rest_foot.x_mm) < 0.01f);
    assert(fabsf(single_plan.rest_foot.z_mm + 100.0f) < 0.01f);
    assert(fabsf(single_plan.knee_lift_foot.z_mm + 80.0f) < 0.01f);
    assert(single_plan.swing_foot.z_mm > single_plan.rest_foot.z_mm);
    assert(fabsf(single_plan.touchdown_foot.x_mm -
                 single_plan.swing_foot.x_mm) < 0.01f);
    assert(fabsf(single_plan.touchdown_foot.z_mm -
                 single_plan.rest_foot.z_mm) < 0.01f);
    assert(fabsf(single_plan.touchdown_angles.hip_rad -
                 single_plan.swing_angles.hip_rad) > 0.01f);
    assert(fabsf(single_plan.touchdown_angles.knee_rad -
                 single_plan.swing_angles.knee_rad) > 0.01f);

    servo_zero_leg_single_pose_t single_pose =
        servo_zero_leg_single_pose(0u, &single_plan);
    assert(single_pose.valid &&
           single_pose.phase == SERVO_ZERO_LEG_SINGLE_PHASE_REST);
    single_pose = servo_zero_leg_single_pose(single_params.rest_ms,
                                             &single_plan);
    assert(single_pose.valid &&
           single_pose.phase == SERVO_ZERO_LEG_SINGLE_PHASE_KNEE_LIFT);
    single_pose = servo_zero_leg_single_pose(
        single_params.rest_ms + single_params.knee_lift_ms, &single_plan);
    assert(single_pose.valid &&
           single_pose.phase == SERVO_ZERO_LEG_SINGLE_PHASE_HIP_SWING);
    single_pose = servo_zero_leg_single_pose(
        single_params.rest_ms + single_params.knee_lift_ms +
            single_params.hip_swing_ms,
        &single_plan);
    assert(single_pose.valid &&
           single_pose.phase == SERVO_ZERO_LEG_SINGLE_PHASE_TOUCHDOWN);
    single_pose = servo_zero_leg_single_pose(single_plan.total_ms,
                                             &single_plan);
    assert(single_pose.valid && single_pose.complete &&
           single_pose.phase == SERVO_ZERO_LEG_SINGLE_PHASE_COMPLETE);
    assert(fabsf(single_pose.foot.x_mm - single_plan.touchdown_foot.x_mm) <
           0.01f);
    assert(fabsf(single_pose.foot.z_mm - single_plan.touchdown_foot.z_mm) <
           0.01f);

    servo_zero_leg_single_pose_t previous_single_pose = {0};
    bool have_previous_single_pose = false;
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= single_plan.total_ms;
         elapsed_ms += 20u) {
        single_pose = servo_zero_leg_single_pose(elapsed_ms, &single_plan);
        assert(single_pose.valid);
        assert(single_pose.foot.z_mm >= single_plan.rest_foot.z_mm - 0.01f);
        if (have_previous_single_pose) {
            assert(fabsf(single_pose.angles.hip_rad -
                         previous_single_pose.angles.hip_rad) < 0.05f);
            assert(fabsf(single_pose.angles.knee_rad -
                         previous_single_pose.angles.knee_rad) < 0.05f);
        }
        previous_single_pose = single_pose;
        have_previous_single_pose = true;
    }

    /* The same lift remains valid while CH0 amplitude is varied, until the
     * touchdown point leaves the 45/70 mm reachable annulus. */
    const float test_swing_degrees[] = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f};
    for (size_t index = 0u;
         index < sizeof(test_swing_degrees) / sizeof(test_swing_degrees[0]);
         ++index) {
        servo_zero_leg_single_params_t amplitude_params = single_params;
        amplitude_params.hip_swing_rad =
            test_swing_degrees[index] * 0.017453292519943295f;
        assert(servo_zero_leg_make_single_plan(&amplitude_params,
                                               &single_plan));
    }
    servo_zero_leg_single_params_t unreachable_swing_params = single_params;
    unreachable_swing_params.hip_swing_rad =
        25.0f * 0.017453292519943295f;
    assert(!servo_zero_leg_make_single_plan(&unreachable_swing_params,
                                            &single_plan));

    servo_zero_leg_single_params_t backward_single_params = single_params;
    assert(servo_zero_leg_ik(round_trip_point,
                             SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD,
                             &backward_single_params.rest_angles));
    backward_single_params.knee_branch =
        SERVO_ZERO_LEG_KNEE_BRANCH_BACKWARD;
    assert(servo_zero_leg_make_single_plan(&backward_single_params,
                                           &single_plan));
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= single_plan.total_ms;
         elapsed_ms += 20u) {
        single_pose = servo_zero_leg_single_pose(elapsed_ms, &single_plan);
        assert(single_pose.valid);
    }

    const servo_zero_leg_pulse_map_t rf_local_map = {
        .hip_rest_us = 1494u,
        .knee_rest_us = 1165u,
        .hip_us_per_rad = 14.6f * 57.29577951308232f,
        .knee_us_per_rad = -10.9f * 57.29577951308232f,
        .min_us = 500u,
        .max_us = 2500u,
    };
    servo_zero_leg_pulse_pose_t mapped_pose =
        servo_zero_leg_map_angles_to_pulses(single_plan.rest_angles,
                                            single_plan.rest_angles,
                                            &rf_local_map);
    assert(mapped_pose.valid && mapped_pose.hip_us == 1494u &&
           mapped_pose.knee_us == 1165u);
    for (uint32_t elapsed_ms = 0u; elapsed_ms <= single_plan.total_ms;
         elapsed_ms += 20u) {
        single_pose = servo_zero_leg_single_pose(elapsed_ms, &single_plan);
        mapped_pose = servo_zero_leg_map_angles_to_pulses(
            single_plan.rest_angles, single_pose.angles, &rf_local_map);
        assert(single_pose.valid && mapped_pose.valid);
    }

    /* Exact configuration used by the experimental UART command. */
    const float hardware_swing_degrees[] = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f};
    for (size_t index = 0u;
         index < sizeof(hardware_swing_degrees) /
                     sizeof(hardware_swing_degrees[0]);
         ++index) {
        servo_zero_leg_single_params_t hardware_params = single_params;
        hardware_params.lift_clearance_mm = 5.0f;
        hardware_params.hip_swing_rad =
            hardware_swing_degrees[index] * 0.017453292519943295f;
        assert(servo_zero_leg_make_single_plan(&hardware_params,
                                               &single_plan));
        for (uint32_t elapsed_ms = 0u; elapsed_ms <= single_plan.total_ms;
             elapsed_ms += 20u) {
            single_pose = servo_zero_leg_single_pose(elapsed_ms,
                                                     &single_plan);
            mapped_pose = servo_zero_leg_map_angles_to_pulses(
                single_plan.rest_angles, single_pose.angles, &rf_local_map);
            assert(single_pose.valid && mapped_pose.valid);
        }
    }

    /* Continuous replacement for the stop-start phase chain. It follows one
     * Bezier foot arc, so there is no internal waypoint with zero velocity. */
    const servo_zero_leg_flow_params_t flow_params = {
        .rest_angles = single_rest_angles,
        .lift_clearance_mm = 5.0f,
        .hip_swing_rad = 20.0f * 0.017453292519943295f,
        .rest_ms = 400u,
        .move_ms = 1800u,
        .knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
    };
    servo_zero_leg_flow_plan_t flow_plan;
    assert(servo_zero_leg_make_flow_plan(&flow_params, &flow_plan));
    assert(flow_plan.valid && flow_plan.total_ms == 2200u);
    servo_zero_leg_flow_pose_t flow_pose =
        servo_zero_leg_flow_pose(0u, &flow_plan);
    assert(flow_pose.valid && !flow_pose.complete);
    assert(fabsf(flow_pose.foot.x_mm - flow_plan.rest_foot.x_mm) < 0.01f);
    flow_pose = servo_zero_leg_flow_pose(
        flow_params.rest_ms + flow_params.move_ms / 2u, &flow_plan);
    assert(flow_pose.valid);
    assert(fabsf(flow_pose.foot.z_mm -
                 (flow_plan.rest_foot.z_mm + flow_params.lift_clearance_mm)) <
           0.01f);
    flow_pose = servo_zero_leg_flow_pose(flow_plan.total_ms, &flow_plan);
    assert(flow_pose.valid && flow_pose.complete);
    assert(fabsf(flow_pose.foot.x_mm - flow_plan.touchdown_foot.x_mm) < 0.01f);
    assert(fabsf(flow_pose.foot.z_mm - flow_plan.rest_foot.z_mm) < 0.01f);

    servo_zero_leg_flow_pose_t previous_flow =
        servo_zero_leg_flow_pose(flow_params.rest_ms, &flow_plan);
    servo_zero_leg_pulse_pose_t previous_flow_pulses =
        servo_zero_leg_map_angles_to_pulses(
            flow_plan.rest_angles, previous_flow.angles, &rf_local_map);
    float largest_joint_step = 0.0f;
    uint16_t largest_pulse_step = 0u;
    for (uint32_t elapsed_ms = flow_params.rest_ms + 20u;
         elapsed_ms <= flow_plan.total_ms; elapsed_ms += 20u) {
        flow_pose = servo_zero_leg_flow_pose(elapsed_ms, &flow_plan);
        mapped_pose = servo_zero_leg_map_angles_to_pulses(
            flow_plan.rest_angles, flow_pose.angles, &rf_local_map);
        assert(flow_pose.valid && mapped_pose.valid);
        assert(flow_pose.foot.z_mm >= flow_plan.rest_foot.z_mm - 0.01f);
        const float hip_step = fabsf(flow_pose.angles.hip_rad -
                                     previous_flow.angles.hip_rad);
        const float knee_step = fabsf(flow_pose.angles.knee_rad -
                                      previous_flow.angles.knee_rad);
        largest_joint_step = fmaxf(largest_joint_step,
                                   fmaxf(hip_step, knee_step));
        const uint16_t hip_pulse_step =
            mapped_pose.hip_us >= previous_flow_pulses.hip_us
                ? mapped_pose.hip_us - previous_flow_pulses.hip_us
                : previous_flow_pulses.hip_us - mapped_pose.hip_us;
        const uint16_t knee_pulse_step =
            mapped_pose.knee_us >= previous_flow_pulses.knee_us
                ? mapped_pose.knee_us - previous_flow_pulses.knee_us
                : previous_flow_pulses.knee_us - mapped_pose.knee_us;
        if (hip_pulse_step > largest_pulse_step) {
            largest_pulse_step = hip_pulse_step;
        }
        if (knee_pulse_step > largest_pulse_step) {
            largest_pulse_step = knee_pulse_step;
        }
        previous_flow = flow_pose;
        previous_flow_pulses = mapped_pose;
    }
    assert(largest_joint_step < 0.02f);
    assert(largest_pulse_step <= 20u);

    /* The requested RF motionleg trial uses a 45-degree physical knee lift
     * from the current rest pose. Verify release touchdown remains reachable
     * across the requested swing demos. */
    const float motionleg_knee_lift_rad =
        45.0f * 0.017453292519943295f;
    for (uint32_t swing_degrees = 5u; swing_degrees <= 60u;
         swing_degrees += 5u) {
        servo_zero_motionleg_plan_t motionleg_plan;
        assert(servo_zero_motionleg_make_plan(
            single_rest_angles, motionleg_knee_lift_rad,
            (float)swing_degrees * 0.017453292519943295f,
            SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &motionleg_plan));
        assert(motionleg_plan.valid);
        assert(motionleg_plan.lifted_foot.z_mm >
               motionleg_plan.rest_foot.z_mm);
        const float expected_touchdown_delta =
            -(fabsf(motionleg_knee_lift_rad) +
              (float)swing_degrees * 0.017453292519943295f);
        assert(fabsf((motionleg_plan.touchdown_angles.knee_rad -
                      motionleg_plan.lifted_angles.knee_rad) -
                     expected_touchdown_delta) < 0.0001f);
        const servo_zero_motionleg_pose_t lift_pose =
            servo_zero_motionleg_pose(50u, UINT32_MAX, 100u, 120u, 240u,
                                      120u, &motionleg_plan);
        const servo_zero_motionleg_pose_t wait_pose =
            servo_zero_motionleg_pose(150u, UINT32_MAX, 100u, 120u, 240u,
                                      120u, &motionleg_plan);
        const servo_zero_motionleg_pose_t coordinated_pose =
            servo_zero_motionleg_pose(300u, UINT32_MAX, 100u, 120u, 240u,
                                      120u, &motionleg_plan);
        assert(lift_pose.valid &&
               lift_pose.phase == SERVO_ZERO_MOTIONLEG_LIFT);
        assert(wait_pose.valid &&
               wait_pose.phase == SERVO_ZERO_MOTIONLEG_WAIT &&
               fabsf(wait_pose.angles.hip_rad -
                     motionleg_plan.lifted_angles.hip_rad) < 0.0001f);
        /* A release observed during the wait is clamped to the swing start;
         * it must not underflow the unsigned swing elapsed time. */
        const servo_zero_motionleg_pose_t early_release_pose =
            servo_zero_motionleg_pose(200u, 150u, 100u, 120u, 240u, 120u,
                                      &motionleg_plan);
        assert(early_release_pose.valid &&
               early_release_pose.phase == SERVO_ZERO_MOTIONLEG_WAIT);
        assert(coordinated_pose.valid &&
               coordinated_pose.phase == SERVO_ZERO_MOTIONLEG_SWING);
        assert(fabsf(coordinated_pose.angles.hip_rad -
                     motionleg_plan.lifted_angles.hip_rad) > 0.0001f);
        assert(fabsf(coordinated_pose.angles.knee_rad -
                     motionleg_plan.lifted_angles.knee_rad) < 0.0001f);

        /* The dedicated demo uses a longer cosine swing so a human can
         * interrupt it while it is moving.  Check the quarter-point against
         * the cosine, not the minimum-jerk curve used by older motion tests. */
        const uint32_t demo_swing_start_ms = 100u + 120u;
        const uint32_t demo_swing_ms = 1800u;
        const servo_zero_motionleg_pose_t quarter_cosine_pose =
            servo_zero_motionleg_pose(
                demo_swing_start_ms + demo_swing_ms / 4u, UINT32_MAX,
                100u, 120u, demo_swing_ms, 120u, &motionleg_plan);
        const float quarter_cosine =
            0.5f - 0.5f * cosf(3.14159265358979323846f * 0.25f);
        const float quarter_progress =
            (quarter_cosine_pose.angles.hip_rad -
             motionleg_plan.lifted_angles.hip_rad) /
            (motionleg_plan.swing_angles.hip_rad -
             motionleg_plan.lifted_angles.hip_rad);
        assert(quarter_cosine_pose.valid &&
               fabsf(quarter_progress - quarter_cosine) < 0.0001f);

        /* A release during the swing freezes the hip at that exact cosine
         * sample; the knee transition starts at the release tick and does not
         * wait for the planned swing endpoint. */
        const uint32_t demo_release_ms = demo_swing_start_ms + 600u;
        const servo_zero_motionleg_pose_t release_pose =
            servo_zero_motionleg_pose(
                demo_release_ms, demo_release_ms, 100u, 120u,
                demo_swing_ms, 120u, &motionleg_plan);
        const servo_zero_motionleg_pose_t mid_touchdown_pose =
            servo_zero_motionleg_pose(
                demo_release_ms + 60u, demo_release_ms, 100u, 120u,
                demo_swing_ms, 120u, &motionleg_plan);
        const servo_zero_motionleg_pose_t complete_pose =
            servo_zero_motionleg_pose(
                demo_release_ms + 120u, demo_release_ms, 100u, 120u,
                demo_swing_ms, 120u, &motionleg_plan);
        assert(release_pose.valid &&
               release_pose.phase == SERVO_ZERO_MOTIONLEG_TOUCHDOWN);
        assert(mid_touchdown_pose.valid &&
               mid_touchdown_pose.phase == SERVO_ZERO_MOTIONLEG_TOUCHDOWN);
        assert(fabsf(mid_touchdown_pose.angles.hip_rad -
                     release_pose.angles.hip_rad) < 0.0001f);
        assert(fabsf(mid_touchdown_pose.angles.knee_rad -
                     release_pose.angles.knee_rad) > 0.0001f);
        assert(complete_pose.valid && complete_pose.complete &&
               complete_pose.phase == SERVO_ZERO_MOTIONLEG_COMPLETE);

        /* At the demo duration, the 20 ms commanded hip samples stay below a
         * 20 us pulse jump with the local 14.6 us/deg slope. */
        const servo_zero_leg_pulse_map_t motionleg_map = {
            .hip_rest_us = 1494u,
            .knee_rest_us = 1165u,
            .hip_us_per_rad = 14.6f * 57.29577951308232f,
            .knee_us_per_rad = -10.9f * 57.29577951308232f,
            .min_us = 500u,
            .max_us = 2500u,
        };
        servo_zero_motionleg_pose_t previous_demo_pose =
            servo_zero_motionleg_pose(
                demo_swing_start_ms, UINT32_MAX, 100u, 120u,
                demo_swing_ms, 120u, &motionleg_plan);
        servo_zero_leg_pulse_pose_t previous_demo_pulses =
            servo_zero_leg_map_angles_to_pulses(
                motionleg_plan.rest_angles, previous_demo_pose.angles,
                &motionleg_map);
        uint16_t largest_demo_hip_step = 0u;
        for (uint32_t elapsed_ms = demo_swing_start_ms + 20u;
             elapsed_ms <= demo_swing_start_ms + demo_swing_ms;
             elapsed_ms += 20u) {
            const servo_zero_motionleg_pose_t demo_pose =
                servo_zero_motionleg_pose(
                    elapsed_ms, UINT32_MAX, 100u, 120u, demo_swing_ms,
                    120u, &motionleg_plan);
            const servo_zero_leg_pulse_pose_t demo_pulses =
                servo_zero_leg_map_angles_to_pulses(
                    motionleg_plan.rest_angles, demo_pose.angles,
                    &motionleg_map);
            assert(demo_pose.valid && demo_pulses.valid);
            const uint16_t hip_step =
                demo_pulses.hip_us >= previous_demo_pulses.hip_us
                    ? demo_pulses.hip_us - previous_demo_pulses.hip_us
                    : previous_demo_pulses.hip_us - demo_pulses.hip_us;
            if (hip_step > largest_demo_hip_step) {
                largest_demo_hip_step = hip_step;
            }
            previous_demo_pulses = demo_pulses;
        }
        assert(largest_demo_hip_step <= 20u);
        for (uint32_t elapsed_ms = 500u; elapsed_ms <= 2500u;
             elapsed_ms += 20u) {
            const uint32_t release_ms = 500u + swing_degrees * 20u;
            servo_zero_motionleg_pose_t motionleg_pose =
                servo_zero_motionleg_pose(elapsed_ms, release_ms, 100u,
                                          120u, 240u, 120u,
                                          &motionleg_plan);
            assert(motionleg_pose.valid);
        }
    }

    /* A 60-degree demo is valid from the recorded support frame but not from
     * the persisted upright CH0 target used by the previous UART trial.  The
     * firmware preflight must reject the latter before enabling /OE. */
    servo_zero_motionleg_plan_t max_demo_plan;
    assert(servo_zero_motionleg_make_plan(
        single_rest_angles, motionleg_knee_lift_rad,
        60.0f * 0.017453292519943295f,
        SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &max_demo_plan));
    const servo_zero_leg_pulse_map_t support_map = {
        .hip_rest_us = 1494u,
        .knee_rest_us = 1165u,
        .hip_us_per_rad = 14.6f * 57.29577951308232f,
        .knee_us_per_rad = -10.9f * 57.29577951308232f,
        .min_us = 500u,
        .max_us = 2500u,
    };
    const servo_zero_leg_pulse_map_t upright_map = {
        .hip_rest_us = 1746u,
        .knee_rest_us = 1469u,
        .hip_us_per_rad = 14.6f * 57.29577951308232f,
        .knee_us_per_rad = -10.9f * 57.29577951308232f,
        .min_us = 500u,
        .max_us = 2500u,
    };
    assert(servo_zero_leg_map_angles_to_pulses(
                max_demo_plan.rest_angles, max_demo_plan.lifted_angles,
                &support_map)
               .valid);
    assert(servo_zero_leg_map_angles_to_pulses(
                max_demo_plan.rest_angles, max_demo_plan.swing_angles,
                &support_map)
               .valid);
    assert(servo_zero_leg_map_angles_to_pulses(
                max_demo_plan.rest_angles, max_demo_plan.touchdown_angles,
                &support_map)
               .valid);
    assert(!servo_zero_leg_map_angles_to_pulses(
                max_demo_plan.rest_angles, max_demo_plan.swing_angles,
                &upright_map)
                .valid);

    servo_zero_leg_flow_params_t invalid_flow = flow_params;
    invalid_flow.lift_clearance_mm = 0.0f;
    assert(!servo_zero_leg_make_flow_plan(&invalid_flow, &flow_plan));

    /* A hip sweep can keep the shank vertical by holding its absolute angle:
     * q_shank = q_h + q_k = -90 degrees, so q_k = -90 degrees - q_h. */
    const uint16_t vertical_positive_hip_us = 2000u;
    const uint16_t vertical_negative_hip_us = 1000u;
    const float vertical_shank_rad = -1.5707963267948966f;
    const float vertical_start_hip_rad =
        single_rest_angles.hip_rad +
        ((float)vertical_positive_hip_us - rf_local_map.hip_rest_us) /
            rf_local_map.hip_us_per_rad;
    const float vertical_end_hip_rad =
        single_rest_angles.hip_rad +
        ((float)vertical_negative_hip_us - rf_local_map.hip_rest_us) /
            rf_local_map.hip_us_per_rad;
    const servo_zero_leg_vertical_sweep_params_t vertical_params = {
        .start_hip_rad = vertical_start_hip_rad,
        .end_hip_rad = vertical_end_hip_rad,
        .shank_rad = vertical_shank_rad,
        .move_ms = 4000u,
    };
    servo_zero_leg_vertical_sweep_plan_t vertical_plan;
    assert(servo_zero_leg_make_vertical_sweep_plan(
        &vertical_params, &vertical_plan));
    assert(vertical_plan.valid);
    assert(fabsf(vertical_plan.start_angles.hip_rad +
                 vertical_plan.start_angles.knee_rad -
                 vertical_shank_rad) < 0.00001f);
    assert(fabsf(vertical_plan.end_angles.hip_rad +
                 vertical_plan.end_angles.knee_rad -
                 vertical_shank_rad) < 0.00001f);
    assert(vertical_plan.start_angles.hip_rad >
           vertical_plan.end_angles.hip_rad);
    assert(vertical_plan.end_angles.knee_rad >
           vertical_plan.start_angles.knee_rad);

    servo_zero_leg_vertical_sweep_pose_t vertical_pose =
        servo_zero_leg_vertical_sweep_pose(0u, &vertical_plan);
    assert(vertical_pose.valid && !vertical_pose.complete);
    servo_zero_leg_pulse_pose_t vertical_pulses =
        servo_zero_leg_map_angles_to_pulses(
            single_rest_angles, vertical_pose.angles, &rf_local_map);
    assert(vertical_pulses.valid &&
           vertical_pulses.hip_us == vertical_positive_hip_us);
    uint16_t previous_vertical_hip_us = vertical_pulses.hip_us;
    uint16_t previous_vertical_knee_us = vertical_pulses.knee_us;
    uint16_t largest_vertical_pulse_step = 0u;
    for (uint32_t elapsed_ms = 20u; elapsed_ms <= vertical_plan.move_ms;
         elapsed_ms += 20u) {
        vertical_pose = servo_zero_leg_vertical_sweep_pose(
            elapsed_ms, &vertical_plan);
        vertical_pulses = servo_zero_leg_map_angles_to_pulses(
            single_rest_angles, vertical_pose.angles, &rf_local_map);
        assert(vertical_pose.valid && vertical_pulses.valid);
        assert(fabsf(vertical_pose.shank_rad - vertical_shank_rad) <
               0.00001f);
        assert(fabsf(vertical_pose.angles.hip_rad +
                     vertical_pose.angles.knee_rad -
                     vertical_shank_rad) < 0.00001f);
        assert(vertical_pose.angles.hip_rad <=
               vertical_plan.start_angles.hip_rad);
        assert(vertical_pose.angles.hip_rad >=
               vertical_plan.end_angles.hip_rad);
        const uint16_t hip_step =
            vertical_pulses.hip_us >= previous_vertical_hip_us
                ? vertical_pulses.hip_us - previous_vertical_hip_us
                : previous_vertical_hip_us - vertical_pulses.hip_us;
        const uint16_t knee_step =
            vertical_pulses.knee_us >= previous_vertical_knee_us
                ? vertical_pulses.knee_us - previous_vertical_knee_us
                : previous_vertical_knee_us - vertical_pulses.knee_us;
        if (hip_step > largest_vertical_pulse_step) {
            largest_vertical_pulse_step = hip_step;
        }
        if (knee_step > largest_vertical_pulse_step) {
            largest_vertical_pulse_step = knee_step;
        }
        previous_vertical_hip_us = vertical_pulses.hip_us;
        previous_vertical_knee_us = vertical_pulses.knee_us;
    }
    assert(vertical_pose.complete);
    assert(vertical_pulses.hip_us == vertical_negative_hip_us);
    assert(largest_vertical_pulse_step <= 20u);
    vertical_pose = servo_zero_leg_vertical_sweep_pose(
        vertical_plan.move_ms + 500u, &vertical_plan);
    assert(vertical_pose.valid && vertical_pose.complete);
    assert(fabsf(vertical_pose.angles.hip_rad -
                 vertical_plan.end_angles.hip_rad) < 0.00001f);

    servo_zero_leg_vertical_sweep_params_t invalid_vertical = vertical_params;
    invalid_vertical.start_hip_rad = invalid_vertical.end_hip_rad;
    assert(!servo_zero_leg_make_vertical_sweep_plan(
        &invalid_vertical, &vertical_plan));
    invalid_vertical = vertical_params;
    invalid_vertical.move_ms = 0u;
    assert(!servo_zero_leg_make_vertical_sweep_plan(
        &invalid_vertical, &vertical_plan));

    return 0;
}
