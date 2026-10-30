#include <assert.h>
#include <stdint.h>

#include "pca9685_safety.h"
#include "pose_map.h"

static void test_active_leg_uses_carrier_mapping(void)
{
    const leg_command_t lift = {LEG_CMD_LIFT, 1.0f};
    const qw_pose_pulse_t pose = qw_pose_map(&lift);

    assert(qw_pose_active_leg_index() == 0U);
    assert(qw_pose_active_leg_base_channel() == 2U);
    assert(pose.base_channel == 2U);
    assert(pose.pulse_a_us == 1200U);
    assert(pose.pulse_b_us == 1700U);
}

int main(void)
{
    static const uint16_t rest[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        1494U, 1165U, 1672U, 1748U, 1368U, 1279U, 1764U, 1934U,
    };

    assert(qw_pca9685_limits_valid());
    for (uint8_t channel = 0U;
         channel < QW_PCA9685_FORMAL_CHANNEL_COUNT; channel++) {
        assert(qw_pca9685_pulse_within_limit(channel, rest[channel]));
    }

    assert(qw_pca9685_pulse_within_limit(0U, 1400));
    assert(!qw_pca9685_pulse_within_limit(0U, 1399));
    assert(qw_pca9685_pulse_within_limit(2U, 1750));
    assert(!qw_pca9685_pulse_within_limit(2U, 1751));
    assert(qw_pca9685_pulse_within_limit(4U, 1940));
    assert(!qw_pca9685_pulse_within_limit(4U, 1941));
    assert(qw_pca9685_pulse_within_limit(6U, 1120));
    assert(!qw_pca9685_pulse_within_limit(6U, 1119));
    assert(!qw_pca9685_pulse_within_limit(8U, 1500));
    assert(!qw_pca9685_pulse_within_limit(1U, 2501));
    test_active_leg_uses_carrier_mapping();
    return 0;
}
