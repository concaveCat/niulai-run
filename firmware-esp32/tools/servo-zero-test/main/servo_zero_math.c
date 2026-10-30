#include "servo_zero_math.h"

#include <math.h>

#define PCA9685_COUNTS_PER_CYCLE 4096u
#define PCA9685_LED0_ON_L 0x06u
#define PCA9685_FULL_OFF_BIT 0x10u

static const uint16_t s_rest_upright_us[SERVO_ZERO_REST_CHANNELS] = {
    1746u, 1469u, 1427u, 1415u, 1619u, 1582u, 1449u, 1600u,
};

static const uint16_t s_rest_target_us[SERVO_ZERO_REST_CHANNELS] = {
    1494u, 1165u, 1672u, 1748u, 1368u, 1279u, 1764u, 1934u,
};

static const uint8_t s_knee_channels[SERVO_ZERO_KNEE_SWING_KNEE_COUNT] = {
    1u, 3u, 5u, 7u,
};

/* CH1 and CH3 were reversed for the current operator-requested direction
 * probe. A positive sign means increasing pulse width is the selected physical
 * direction for that knee; all signs remain hypotheses until the knees are
 * observed on the fixed bench. */
static const int8_t
    s_knee_pulse_direction[SERVO_ZERO_KNEE_SWING_KNEE_COUNT] = {
        1, 1, 1, 1,
    };

uint16_t servo_zero_pulse_to_counts(uint16_t pulse_us, uint16_t frequency_hz)
{
    const uint64_t counts = (uint64_t)pulse_us * PCA9685_COUNTS_PER_CYCLE *
                            frequency_hz / 1000000u;
    return counts >= PCA9685_COUNTS_PER_CYCLE
               ? PCA9685_COUNTS_PER_CYCLE - 1u
               : (uint16_t)counts;
}

uint8_t servo_zero_prescale(uint32_t oscillator_hz, uint16_t frequency_hz)
{
    return (uint8_t)(oscillator_hz /
                     (PCA9685_COUNTS_PER_CYCLE * frequency_hz) - 1u);
}

void servo_zero_make_pulse_frame(uint8_t channel, uint16_t counts,
                                 uint8_t frame[5])
{
    frame[0] = (uint8_t)(PCA9685_LED0_ON_L + 4u * channel);
    frame[1] = 0x00;
    frame[2] = 0x00;
    frame[3] = (uint8_t)(counts & 0xffu);
    frame[4] = (uint8_t)(counts >> 8);
}

void servo_zero_make_full_off_frame(uint8_t channel, uint8_t frame[5])
{
    frame[0] = (uint8_t)(PCA9685_LED0_ON_L + 4u * channel);
    frame[1] = 0x00;
    frame[2] = 0x00;
    frame[3] = 0x00;
    frame[4] = PCA9685_FULL_OFF_BIT;
}

void servo_zero_make_active_frame(
    const uint16_t pulse_us[], uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_ACTIVE_FRAME_BYTES])
{
    frame[0] = PCA9685_LED0_ON_L;
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        const uint16_t counts =
            servo_zero_pulse_to_counts(pulse_us[channel], frequency_hz);
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        frame[offset] = 0x00;
        frame[offset + 1u] = 0x00;
        frame[offset + 2u] = (uint8_t)(counts & 0xffu);
        frame[offset + 3u] = (uint8_t)(counts >> 8);
    }
}

void servo_zero_make_active_half_frame(
    const uint16_t pulse_us[], uint8_t first_channel, uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_ACTIVE_HALF_FRAME_BYTES])
{
    frame[0] = (uint8_t)(PCA9685_LED0_ON_L + 4u * first_channel);
    for (uint8_t index = 0; index < SERVO_ZERO_ACTIVE_HALF_CHANNELS;
         ++index) {
        const uint16_t counts = servo_zero_pulse_to_counts(
            pulse_us[first_channel + index], frequency_hz);
        const uint8_t offset = (uint8_t)(1u + 4u * index);
        frame[offset] = 0x00;
        frame[offset + 1u] = 0x00;
        frame[offset + 2u] = (uint8_t)(counts & 0xffu);
        frame[offset + 3u] = (uint8_t)(counts >> 8);
    }
}

void servo_zero_make_active_pair_frame(
    uint16_t hip_us, uint16_t knee_us, uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_ACTIVE_PAIR_FRAME_BYTES])
{
    const uint16_t pulses[2] = {hip_us, knee_us};
    frame[0] = PCA9685_LED0_ON_L;
    for (uint8_t channel = 0; channel < 2u; ++channel) {
        const uint16_t counts =
            servo_zero_pulse_to_counts(pulses[channel], frequency_hz);
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        frame[offset] = 0x00;
        frame[offset + 1u] = 0x00;
        frame[offset + 2u] = (uint8_t)(counts & 0xffu);
        frame[offset + 3u] = (uint8_t)(counts >> 8);
    }
}

void servo_zero_make_active_pair_only_frame(
    uint16_t hip_us, uint16_t knee_us, uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_FULL_FRAME_BYTES])
{
    const uint16_t pulses[2] = {hip_us, knee_us};
    frame[0] = PCA9685_LED0_ON_L;
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_FULL_FRAME_CHANNELS; ++channel) {
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        if (channel < 2u) {
            const uint16_t counts = servo_zero_pulse_to_counts(
                pulses[channel], frequency_hz);
            frame[offset] = 0x00;
            frame[offset + 1u] = 0x00;
            frame[offset + 2u] = (uint8_t)(counts & 0xffu);
            frame[offset + 3u] = (uint8_t)(counts >> 8);
        } else {
            frame[offset] = 0x00;
            frame[offset + 1u] = 0x00;
            frame[offset + 2u] = 0x00;
            frame[offset + 3u] = PCA9685_FULL_OFF_BIT;
        }
    }
}

void servo_zero_make_active_with_unused_off_frame(
    const uint16_t pulse_us[], uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_FULL_FRAME_BYTES])
{
    frame[0] = PCA9685_LED0_ON_L;
    for (uint8_t channel = 0; channel < SERVO_ZERO_FULL_FRAME_CHANNELS;
         ++channel) {
        const uint8_t offset = (uint8_t)(1u + 4u * channel);
        if (channel < SERVO_ZERO_REST_CHANNELS) {
            const uint16_t counts =
                servo_zero_pulse_to_counts(pulse_us[channel], frequency_hz);
            frame[offset] = 0x00;
            frame[offset + 1u] = 0x00;
            frame[offset + 2u] = (uint8_t)(counts & 0xffu);
            frame[offset + 3u] = (uint8_t)(counts >> 8);
        } else {
            frame[offset] = 0x00;
            frame[offset + 1u] = 0x00;
            frame[offset + 2u] = 0x00;
            frame[offset + 3u] = PCA9685_FULL_OFF_BIT;
        }
    }
}

uint16_t servo_zero_s_curve_lerp(uint16_t start_us, uint16_t end_us,
                                 uint32_t elapsed_ms, uint32_t duration_ms)
{
    if (duration_ms == 0u || elapsed_ms >= duration_ms) {
        return end_us;
    }
    const float t = (float)elapsed_ms / (float)duration_ms;
    /* Cosine half-wave: zero velocity at both ends of the move. */
    const float smooth = 0.5f -
                         0.5f * cosf(3.14159265358979323846f * t);
    const float value = (float)start_us +
                        ((float)end_us - (float)start_us) * smooth;
    return (uint16_t)(value + 0.5f);
}

uint16_t servo_zero_minimum_jerk_lerp(uint16_t start_us, uint16_t end_us,
                                      uint32_t elapsed_ms,
                                      uint32_t duration_ms)
{
    if (duration_ms == 0u || elapsed_ms >= duration_ms) {
        return end_us;
    }
    const float t = (float)elapsed_ms / (float)duration_ms;
    const float t2 = t * t;
    const float smooth = t2 * t * (10.0f + t * (-15.0f + 6.0f * t));
    const float value = (float)start_us +
                        ((float)end_us - (float)start_us) * smooth;
    return (uint16_t)(value + 0.5f);
}

uint16_t servo_zero_scale_pulse(uint16_t start_us, uint16_t target_us,
                                uint8_t amplitude_percent)
{
    if (amplitude_percent > SERVO_ZERO_AMPLITUDE_MAX_PERCENT) {
        amplitude_percent = SERVO_ZERO_AMPLITUDE_MAX_PERCENT;
    }
    const int32_t delta = (int32_t)target_us - (int32_t)start_us;
    const int32_t magnitude = delta >= 0 ? delta : -delta;
    const int32_t scaled =
        (magnitude * amplitude_percent + 50) / 100;
    int32_t value = (int32_t)start_us + (delta >= 0 ? scaled : -scaled);
    if (value < 0) {
        value = 0;
    } else if (value > UINT16_MAX) {
        value = UINT16_MAX;
    }
    return (uint16_t)value;
}

servo_zero_rf_step_pose_t servo_zero_rf_step_pose_at_amplitude(
    uint32_t elapsed_ms, uint16_t upright_hip_us, uint16_t upright_knee_us,
    uint8_t amplitude_percent)
{
    const uint16_t hip_target_us = servo_zero_scale_pulse(
        upright_hip_us, SERVO_ZERO_RF_STEP_HIP_TARGET_US, amplitude_percent);
    const uint16_t knee_prelift_us = servo_zero_scale_pulse(
        upright_knee_us, SERVO_ZERO_RF_STEP_KNEE_PRELIFT_US,
        amplitude_percent);
    const uint16_t knee_target_us = servo_zero_scale_pulse(
        upright_knee_us, SERVO_ZERO_RF_STEP_KNEE_TARGET_US,
        amplitude_percent);
    servo_zero_rf_step_pose_t pose = {
        .hip_us = upright_hip_us,
        .knee_us = upright_knee_us,
        .complete = false,
    };

    if (elapsed_ms < 250u) {
        return pose;
    }
    if (elapsed_ms < 350u) {
        pose.knee_us = servo_zero_s_curve_lerp(
            upright_knee_us, knee_prelift_us,
            elapsed_ms - 250u, 100u);
        return pose;
    }
    if (elapsed_ms < 850u) {
        pose.hip_us = servo_zero_s_curve_lerp(
            upright_hip_us, hip_target_us,
            elapsed_ms - 350u, 500u);
        pose.knee_us = servo_zero_s_curve_lerp(
            knee_prelift_us, knee_target_us,
            elapsed_ms - 350u, 500u);
        return pose;
    }
    if (elapsed_ms < 1000u) {
        pose.hip_us = hip_target_us;
        pose.knee_us = knee_target_us;
        return pose;
    }
    if (elapsed_ms < 1700u) {
        pose.hip_us = servo_zero_s_curve_lerp(
            hip_target_us, upright_hip_us,
            elapsed_ms - 1000u, 700u);
        pose.knee_us = servo_zero_s_curve_lerp(
            knee_target_us, upright_knee_us,
            elapsed_ms - 1000u, 700u);
        return pose;
    }
    if (elapsed_ms >= SERVO_ZERO_RF_STEP_TOTAL_MS) {
        pose.complete = true;
    }
    return pose;
}

servo_zero_rf_step_pose_t servo_zero_rf_step_pose(uint32_t elapsed_ms,
                                                  uint16_t upright_hip_us,
                                                  uint16_t upright_knee_us)
{
    return servo_zero_rf_step_pose_at_amplitude(
        elapsed_ms, upright_hip_us, upright_knee_us,
        SERVO_ZERO_AMPLITUDE_MAX_PERCENT);
}

uint16_t servo_zero_rest_upright_us(uint8_t channel)
{
    return channel < SERVO_ZERO_REST_CHANNELS
               ? s_rest_upright_us[channel]
               : 0u;
}

uint16_t servo_zero_rest_target_us(uint8_t channel)
{
    return servo_zero_rest_target_us_at_amplitude(
        channel, SERVO_ZERO_AMPLITUDE_MAX_PERCENT);
}

uint16_t servo_zero_rest_target_us_at_amplitude(uint8_t channel,
                                                uint8_t amplitude_percent)
{
    return channel < SERVO_ZERO_REST_CHANNELS
               ? servo_zero_scale_pulse(s_rest_upright_us[channel],
                                        s_rest_target_us[channel],
                                        amplitude_percent)
               : 0u;
}

servo_zero_rest_pose_t servo_zero_rest_pose_at_amplitude(
    uint32_t elapsed_ms, uint8_t amplitude_percent)
{
    servo_zero_rest_pose_t pose = {
        .pulse_us = {0u},
        .complete = false,
    };
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        pose.pulse_us[channel] = s_rest_upright_us[channel];
    }

    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        const uint32_t group_index = channel / 2u;
        const uint32_t group_start = SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
                                     group_index *
                                         SERVO_ZERO_REST_GROUP_MOVE_MS;
        const uint16_t target_us = servo_zero_rest_target_us_at_amplitude(
            channel, amplitude_percent);
        if (elapsed_ms < group_start) {
            continue;
        }
        pose.pulse_us[channel] = servo_zero_s_curve_lerp(
            s_rest_upright_us[channel], target_us,
            elapsed_ms - group_start, SERVO_ZERO_REST_GROUP_MOVE_MS);
    }
    pose.complete = elapsed_ms >= SERVO_ZERO_REST_TOTAL_MS;
    return pose;
}

servo_zero_rest_pose_t servo_zero_rest_pose(uint32_t elapsed_ms)
{
    return servo_zero_rest_pose_at_amplitude(
        elapsed_ms, SERVO_ZERO_AMPLITUDE_MAX_PERCENT);
}

servo_zero_rest_pose_t servo_zero_rest_pseudo_pose_at_amplitude(
    uint32_t elapsed_ms, uint8_t amplitude_percent)
{
    servo_zero_rest_pose_t pose = {
        .pulse_us = {0u},
        .complete = false,
    };
    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        pose.pulse_us[channel] = s_rest_upright_us[channel];
    }

    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        const uint8_t channel_bit = (uint8_t)(1u << channel);
        const uint32_t group_start =
            SERVO_ZERO_REST_UPRIGHT_SETTLE_MS +
            ((SERVO_ZERO_REST_PSEUDO_GROUP_B_MASK & channel_bit) != 0u
                 ? SERVO_ZERO_REST_PSEUDO_PHASE_MS
                 : 0u);
        const uint16_t target_us = servo_zero_rest_target_us_at_amplitude(
            channel, amplitude_percent);
        if (elapsed_ms < group_start) {
            continue;
        }
        pose.pulse_us[channel] = servo_zero_minimum_jerk_lerp(
            s_rest_upright_us[channel], target_us,
            elapsed_ms - group_start,
            SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS);
    }
    pose.complete = elapsed_ms >= SERVO_ZERO_REST_PSEUDO_TOTAL_MS;
    return pose;
}

uint16_t servo_zero_knee_swing_delta_us(uint8_t amplitude_percent)
{
    if (amplitude_percent > SERVO_ZERO_AMPLITUDE_MAX_PERCENT) {
        amplitude_percent = SERVO_ZERO_AMPLITUDE_MAX_PERCENT;
    }
    return (uint16_t)((SERVO_ZERO_KNEE_SWING_NOMINAL_DELTA_US *
                       amplitude_percent + 50u) /
                      100u);
}

servo_zero_knee_swing_pose_t servo_zero_knee_swing_pose_at_amplitude(
    uint32_t elapsed_ms, uint8_t amplitude_percent)
{
    servo_zero_knee_swing_pose_t pose = {
        .pulse_us = {0u},
        .valid = true,
    };
    const uint16_t delta_us = servo_zero_knee_swing_delta_us(amplitude_percent);

    for (uint8_t channel = 0; channel < SERVO_ZERO_REST_CHANNELS; ++channel) {
        pose.pulse_us[channel] = servo_zero_rest_upright_us(channel);
    }

    for (uint8_t knee_index = 0;
         knee_index < SERVO_ZERO_KNEE_SWING_KNEE_COUNT; ++knee_index) {
        const uint8_t channel = s_knee_channels[knee_index];
        const int32_t neutral_us = servo_zero_rest_upright_us(channel);
        const int32_t direction = s_knee_pulse_direction[knee_index];
        const int32_t endpoint_a = neutral_us - direction * delta_us;
        const int32_t endpoint_b = neutral_us + direction * delta_us;
        uint16_t knee_us = (uint16_t)neutral_us;

        if (endpoint_a < (int32_t)SERVO_ZERO_KNEE_SWING_MIN_US ||
            endpoint_a > (int32_t)SERVO_ZERO_KNEE_SWING_MAX_US ||
            endpoint_b < (int32_t)SERVO_ZERO_KNEE_SWING_MIN_US ||
            endpoint_b > (int32_t)SERVO_ZERO_KNEE_SWING_MAX_US) {
            pose.valid = false;
            continue;
        }

        if (elapsed_ms >= SERVO_ZERO_KNEE_SWING_SETTLE_MS) {
            const uint32_t motion_ms =
                elapsed_ms - SERVO_ZERO_KNEE_SWING_SETTLE_MS;
            if (motion_ms < SERVO_ZERO_KNEE_SWING_MOVE_MS) {
                knee_us = servo_zero_minimum_jerk_lerp(
                    (uint16_t)neutral_us, (uint16_t)endpoint_a, motion_ms,
                    SERVO_ZERO_KNEE_SWING_MOVE_MS);
            } else {
                const uint32_t phase_ms =
                    (motion_ms - SERVO_ZERO_KNEE_SWING_MOVE_MS) %
                    (2u * SERVO_ZERO_KNEE_SWING_MOVE_MS);
                if (phase_ms < SERVO_ZERO_KNEE_SWING_MOVE_MS) {
                    knee_us = servo_zero_minimum_jerk_lerp(
                        (uint16_t)endpoint_a, (uint16_t)endpoint_b, phase_ms,
                        SERVO_ZERO_KNEE_SWING_MOVE_MS);
                } else {
                    knee_us = servo_zero_minimum_jerk_lerp(
                        (uint16_t)endpoint_b, (uint16_t)endpoint_a,
                        phase_ms - SERVO_ZERO_KNEE_SWING_MOVE_MS,
                        SERVO_ZERO_KNEE_SWING_MOVE_MS);
                }
            }
        }
        pose.pulse_us[channel] = knee_us;
    }
    return pose;
}

servo_zero_ch1_cosine_sweep_pose_t servo_zero_ch1_cosine_sweep_pose(
    uint32_t elapsed_ms, uint16_t initial_us)
{
    servo_zero_ch1_cosine_sweep_pose_t pose = {
        .pulse_us = initial_us,
        .valid = initial_us >= SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US &&
                 initial_us <= SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US,
    };
    if (!pose.valid) {
        return pose;
    }

    if (elapsed_ms < SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS) {
        pose.pulse_us = servo_zero_s_curve_lerp(
            initial_us, SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US, elapsed_ms,
            SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS);
        return pose;
    }

    const uint32_t motion_ms =
        elapsed_ms - SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS;
    const uint32_t half_ms = SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS;
    const uint32_t phase = motion_ms / half_ms;
    const uint32_t phase_elapsed = motion_ms % half_ms;
    if ((phase & 1u) == 0u) {
        pose.pulse_us = servo_zero_s_curve_lerp(
            SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US,
            SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US, phase_elapsed, half_ms);
    } else {
        pose.pulse_us = servo_zero_s_curve_lerp(
            SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US,
            SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US, phase_elapsed, half_ms);
    }
    return pose;
}
