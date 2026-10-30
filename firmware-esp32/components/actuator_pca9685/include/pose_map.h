/* Legacy static intent -> pulse-width mapping for one compatibility leg.
 *
 * Only the compatibility leg's two PCA9685 channels are ever driven:
 *   FR = CH0/CH1, FL = CH2/CH3, RR = CH4/CH5, RL = CH6/CH7.
 * This is the carrier's physical channel order; public leg masks remain
 * semantic FL/FR/RL/RR order.
 *
 * Every value below is an EXP-001 bench-tuning candidate, deliberately NOT
 * a final hardware spec. No IK, no 8-channel choreography by design.
 *
 * Mapping (Kconfig defaults):
 *   hold_support / safe_stop: 1500/1500 us
 *   lift(progress):           linear 1500 -> 1200 / 1700 us
 *   swing(progress):          +/- amplitude about the lift pose, swept by
 *                             progress from -amp to +amp
 *   search:                   fixed 1000/1800 us (progress ignored)
 */
#ifndef POSE_MAP_H
#define POSE_MAP_H

#include "sdkconfig.h"

#include <math.h>
#include <stdint.h>

#include "leg_fsm.h"

typedef struct {
    uint8_t base_channel; /* even channel of the compatibility leg's pair */
    uint16_t pulse_a_us;  /* even channel */
    uint16_t pulse_b_us;  /* odd channel */
} qw_pose_pulse_t;

static inline uint8_t qw_pose_active_leg_index(void)
{
#if defined(CONFIG_NIULAI_ACTIVE_LEG_FL)
    return 0;
#elif defined(CONFIG_NIULAI_ACTIVE_LEG_FR)
    return 1;
#elif defined(CONFIG_NIULAI_ACTIVE_LEG_RL)
    return 2;
#else
    return 3; /* RR */
#endif
}

static inline uint8_t qw_pose_active_leg_base_channel(void)
{
    static const uint8_t base_channel[4] = {2U, 0U, 6U, 4U};
    return base_channel[qw_pose_active_leg_index()];
}

static inline float qw_clamp01(float v)
{
    if (!isfinite(v)) {
        return 0.0f;
    }
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

static inline qw_pose_pulse_t qw_pose_map(const leg_command_t *cmd)
{
    qw_pose_pulse_t pose;
    const int32_t hold = CONFIG_NIULAI_PCA9685_PULSE_HOLD_US;
    const int32_t lift_a = CONFIG_NIULAI_PCA9685_PULSE_LIFT_US;
    const int32_t lift_b = CONFIG_NIULAI_PCA9685_PULSE_LIFT_B_US;
    const int32_t amp = CONFIG_NIULAI_PCA9685_PULSE_SWING_AMPLITUDE_US;

    pose.base_channel = qw_pose_active_leg_base_channel();
    pose.pulse_a_us = (uint16_t)hold;
    pose.pulse_b_us = (uint16_t)hold;

    switch (cmd->mode) {
    case LEG_CMD_LIFT: {
        float p = qw_clamp01(cmd->progress);
        pose.pulse_a_us = (uint16_t)(hold + (int32_t)((lift_a - hold) * p));
        pose.pulse_b_us = (uint16_t)(hold + (int32_t)((lift_b - hold) * p));
        break;
    }
    case LEG_CMD_SWING: {
        /* +/- amplitude about the lift pose; progress 0 -> -amp, 1 -> +amp */
        float p = qw_clamp01(cmd->progress);
        int32_t offset = (int32_t)((2.0f * p - 1.0f) * (float)amp);
        pose.pulse_a_us = (uint16_t)(lift_a + offset);
        pose.pulse_b_us = (uint16_t)(lift_b + offset);
        break;
    }
    case LEG_CMD_SEARCH:
        pose.pulse_a_us = (uint16_t)CONFIG_NIULAI_PCA9685_PULSE_SEARCH_A_US;
        pose.pulse_b_us = (uint16_t)CONFIG_NIULAI_PCA9685_PULSE_SEARCH_B_US;
        break;
    case LEG_CMD_SAFE_STOP:
    default: /* hold_support (and safe_stop) share the neutral pose */
        break;
    }
    return pose;
}

#endif /* POSE_MAP_H */
